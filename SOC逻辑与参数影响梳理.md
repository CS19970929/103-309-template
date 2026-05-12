# SOC 逻辑与参数影响梳理

本文按当前源码记录 SOC 模块的完整运行链路、参数入口、参数范围、用户体验影响和函数职责。依据文件：

- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/SOC.h`
- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/SocEnhance.h`
- `103 + 309/Project/Source/conf/Project_Config.h`
- `103 + 309/Project/Source/conf/Project_BuildGuard.h`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `tools/run_soc_host_c_test.py`
- `tools/soc_replay_test.py`

## 1. 总体结论

当前 SOC 是“安时积分主导 + 少量可信电压锚点 + 显示平滑”的工程模型，不做复杂 FCC 学习，不做运行态连续 OCV 融合。

运行主链路固定为：

```text
采样 -> 积分 -> sag/rebound holdoff -> 满电确认 -> 中低压弱约束/低压表 -> 稳定静置/RTC OCV -> 保存 -> 发布
```

关键边界：

1. 自动校准单次最多 `1%`，满电、低压、静置/RTC OCV 都不能一次跳变。
2. 满电只能通过满电确认逐步到 `100%`；充电积分未确认前最多到 `99%`。
3. 中低压和低压都走 table-driven 上限约束，默认覆盖 `V0+700mV` 到 `V0-50mV`。
4. 大电流 `Idsg > C/2` 触发 sag holdoff，未回弹且未到真实末端时阻断电压校准。
5. 重载后关机再开机时，快照 `u16Flags bit0` 会带来 `5min` 回弹保护，避免未回弹电压把 SOC 校低。
6. 运行态静置 OCV 不再按固定 `30min` 到点校准；电压稳定至少 `5min` 且 `10min` 刷新节拍到达后才记录可信 OCV 目标，后续优先在充电/放电阶段按 `10min/1%` 消化差值，久置低 OCV 才允许静置下修。
7. 无 Flash 快照冷启动时，电压有效按 OCV 表；电压无效才默认 `60%`。
8. 对外只有 `g_stCellInfoReport.SocElement`，RS485、CAN、LedBar 都读这里的平滑 SOC/SOH/容量。

## 2. 数据单位

| 数据 | 内部单位 | 对外/参数单位 | 说明 |
| --- | --- | --- | --- |
| 充放电电流 | `A * 10` | `A * 10` | `10` 表示 `1.0A` |
| 内部容量 | `As * 10` | - | `cap_factory_as10 = u16Soc_Ah * 3600` |
| 对外容量 | - | `Ah * 100` | `soc_cap_to_ah100()` 输出，`2700` 表示 `27.00Ah` |
| SOC | `%` | `%` | 内部 `soc` 与对外 `display_soc` 分离 |
| SOH | `%` | `%` | 当前按循环次数映射，最低 `80%` |
| 循环次数 | `cycle * 100` | `cycle` | `cycle_x100 / 100` 对外显示 |
| 电压 | `mV` | `mV` | 单体电压用于 OCV、满电、中低压弱约束和低压表 |

## 3. 启动与周期运行

### 3.1 启动初始化

`InitData_SOC()` 的初始化顺序：

1. `SOC_LoadConfigData()` 从 `OtherElement` 复制容量、循环、表选择、`V100`、`V0` 和可写 OCV 表。
2. `soc_param_lib_init()` 初始化 `SOC_STATE` 的容量、SOH、循环、电压/电流输入。
3. `soc_load_or_default()` 读取 Flash SOC journal。
4. 快照有效时恢复 `soc/cap_now/cycle/dsg_acc/snapshot_flags`；若存在重载回弹标志，则先进入 `5min` 电压校准 holdoff。快照无效时，电压有效按 OCV 表，电压无效按默认 `60%`。
5. `soc_publish(1)` 强制发布一次，保证通信和显示有确定值。

### 3.2 周期运行

`App_SOC()` 只在 `g_u32AfeCurrentSampleSeq` 变化时执行完整算法，避免同一 AFE 样本被重复积分。没有新样本时只重新发布当前显示数据。

Type-C PA2 电流不经过 AFE 主回路采样，SOC 不再直接扣 9V 输出侧电流，而是先按输出功率换算为电池侧等效放电电流：

```text
Type-C电池侧等效电流 = Type-C输出电流 * 9000mV / 电池包电压 / 0.9
SOC放电输入 = AFE放电 + Type-C电池侧等效放电 - AFE充电
SOC充电输入 = AFE充电 - AFE放电 - Type-C电池侧等效放电
```

这保证边充边外放时不会虚增 SOC。

### 3.3 方向判定

| 条件 | 模式 |
| --- | --- |
| `Ichg >= 0.2A` 且 `Ichg >= Idsg` | `CHG` |
| `Idsg >= 0.2A` | `DSG` |
| 其他 | `RELAX` |

`0.2A` 以下视作静置，用于抑制电流零漂。AFE 输出死区也同步为 `200mA`，避免 SOC 门槛降低后仍被前级 `0.3A` 死区挡掉。

## 4. 核心算法

### 4.1 安时积分

每个 200ms tick 的容量增量：

```text
delta_as10 = (current_A10 * 200 + rem_ms) / 1000
rem_ms = (current_A10 * 200 + rem_ms) % 1000
```

处理规则：

- `CHG`：增加 `cap_now_as10`，不超过 `cap_full_as10`；未满电确认前内部 SOC 到 `100%` 会被压回 `99%`。
- `DSG`：减少 `cap_now_as10`，同时累计 `dsg_acc_as10` 用于循环计数。
- `RELAX`：清空积分余量，不改容量。
- 模式切换会清空 `rem_ms`，避免充放电方向互相继承余量。

### 4.2 SOH 与有效容量

当前 SOH 只按循环次数映射：

```text
cycle = cycle_x100 / 100
SOH = clamp(100 - cycle / 100, 80, 100)
cap_full_as10 = cap_factory_as10 * SOH / 100
```

含义：

- 每 `100` 次完整循环，SOH 下降 `1%`。
- SOH 最低 `80%`，不再下降。
- 每累计放出 `factory_capacity / 100`，`cycle_x100 + 1`。
- SOH 变化时重新计算 `cap_full_as10`，并按当前容量反推内部 SOC。

### 4.3 OCV 表

OCV 表只用于冷启动、稳定静置/RTC，以及骑行中上限约束参考，不替代运行中的安时积分。当前三元锂默认表：

```text
4160/100, 4100/95, 4050/90, 3995/85, 3935/80,
3880/75, 3835/70, 3795/65, 3760/60, 3725/55,
3695/50, 3670/45, 3645/40, 3615/35, 3585/30,
3555/25, 3525/20, 3480/15, 3400/10, 3250/5,
3000/0
```

表选择：

| 枚举 | 值 | 来源 |
| --- | ---: | --- |
| `SOC_TABLE_TEST` | 0 | `SOC_Table_Set`，上位机写入 RAM 表 |
| `SOC_TABLE_LIFEPO` | 1 | 固件内置 LFP 表 |
| `SOC_TABLE_TERNARYLI` | 2 | 固件内置三元锂表 |
| `SOC_TABLE_LIFEPO2` | 3 | 固件内置备用 LFP 表 |

### 4.4 电压校准门控

电压类校准统一经过 `soc_calibration_allowed()`：

| 门控 | 当前规则 |
| --- | --- |
| 单体电压范围 | `2000mV <= VCellMin <= VCellMax <= 5000mV` |
| 单体压差 | `VCellMax - VCellMin <= 1000mV` |
| 保护故障 | 仅当 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT=1` 时，三级保护故障会阻断校准 |
| 系统故障 | 仅当 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT=1` 时，AFE/ADC/CBC/温度异常会阻断校准 |

门控失败只阻断电压校准，不停止安时积分，也不会用隐藏兜底改 SOC。

稳定静置和中低压弱约束在统一门控之外额外要求单体压差 `<=200mV`，避免电芯不一致或采样异常时扩大校准误差。

### 4.5 满电确认

满电确认按 `V100` 和配置余量判断，不依赖 taper 电流。

| 条件 | 当前默认 |
| --- | --- |
| 满电基准 | `V100 = OtherElement.u16Soc_V_100`，为 0 时默认 `4180mV` |
| 基础门槛 | `VCellMax >= V100 - 80mV`，当前 `4100mV` |
| 快速确认 | `VCellMin >= V100 - 30mV` 且压差 `<=120mV`，持续 `5s` |
| 普通确认 | 内部 SOC `>=95%`，`VCellMin >= V100 - 80mV` 且压差 `<=120mV`，持续 `15s` |

确认满足后每次只把内部 SOC 向 `100%` 推进 `1%`。显示 SOC 继续按显示平滑跟随。

### 4.6 中低压弱约束

中低压弱约束以 `V0` 为基准，只处理 `V0+500mV` 到 `V0+700mV` 的明显高估场景。默认 `V0=3000mV` 时表如下：

| `VCellMin` 相对 `V0` | RELAX 目标/周期 | 轻载 `<=C/5` | 中载 `<=C/2` | 重载 `>C/2` |
| --- | --- | --- | --- | --- |
| `V0 + 500mV` | `25% / 90s` | `32% / 90s` | `42% / 120s` | 禁用 |
| `V0 + 600mV` | `35% / 120s` | `42% / 120s` | `50% / 150s` | 禁用 |
| `V0 + 650mV` | `45% / 150s` | `50% / 150s` | `58% / 180s` | 禁用 |
| `V0 + 700mV` | `55% / 180s` | `60% / 180s` | 禁用 | 禁用 |

规则：

- 只向下修正，不向上修正。
- 只在 `VCellMin > V0 + 400mV` 时运行，避免与低压表重叠。
- 单体压差必须 `<=200mV`。
- sag/rebound holdoff 期间禁用。
- 重载档位禁用。
- 条件中断会清空计数器，下次进入需要重新累计完整周期。

### 4.7 低压表

低压表以 `V0 = OtherElement.u16Soc_V_0` 为基准，为 0 时默认 `3000mV`。默认表：

| `VCellMin` 相对 `V0` | RELAX 目标/周期 | 轻载 `<=C/5` | 中载 `<=C/2` | 重载 `>C/2` |
| --- | --- | --- | --- | --- |
| `V0 + 400mV` | `18% / 24s` | `22% / 20s` | `30% / 16s` | `40% / 12s` |
| `V0 + 300mV` | `14% / 18s` | `18% / 15s` | `25% / 12s` | `32% / 9s` |
| `V0 + 200mV` | `12% / 12s` | `14% / 10s` | `20% / 8s` | `25% / 6s` |
| `V0 + 100mV` | `8% / 7s` | `10% / 6s` | `14% / 5s` | `18% / 4s` |
| `V0 + 50mV` | `4% / 4s` | `5% / 3s` | `8% / 2s` | `12% / 1.6s` |
| `V0` | `0% / 2s` | `0% / 1s` | `0% / 1s` | `0% / 1s` |
| `V0 - 25mV` | `0% / 1s` | `0% / 1s` | `0% / 0.2s` | `0% / 0.2s` |
| `V0 - 50mV` | `0% / 0.2s` | `0% / 0.2s` | `0% / 0.2s` | `0% / 0.2s` |

规则：

- 只向下修正，不向上修正。
- SOC 高于目标时按周期每次下修 `1%`。
- SOC 低于目标时不拉高，继续相信安时积分。
- 低压表活跃时阻断静置 OCV，保证同一 tick 只有一条校准路径。

### 4.8 sag/rebound holdoff

`Idsg > C/2` 判定为大电流放电，进入 `30s` sag holdoff，并设置快照 `u16Flags bit0`。holdoff 期间：

- `VCellMin > V0 + 50mV`：阻断低压表和静置 OCV。
- `VCellMin <= V0 + 50mV`：认为进入真实末端，低压表仍可收敛到 `0%`。

如果 holdoff 期间用户关机，下次开机会根据快照标志进入 `5min` rebound holdoff。该窗口结束后标志自动清除并保存，防止长期锁死。

### 4.9 静置/RTC OCV

运行态静置 `RELAX` 按“最小静置时间 + 电压回弹稳定 + OCV 刷新节拍”判断，不再按 `1800s` 到点强制校正。RTC 唤醒路径也使用稳定窗口：第一次 RTC 电压样本只建立参考值，后续 `VCellMin/VCellMax` 相对参考值波动 `<=30mV` 才累计稳定时间；稳定累计至少 `5min` 且 `10min` 节拍到达后，才按 OCV 表计算并刷新 deferred target。

RTC 唤醒周期可能是 CAN active 下 `1s` 或 idle 下 `10s`，但这个周期只影响通信探测频率，不再直接决定 SOC 校准频率。RTC OCV 仍会被电压合法性、故障和 sag/rebound holdoff 阻断。

方向约束：

- `RELAX`：通常只记录 deferred target，不直接向上修正；若稳定静置后已有低于当前 SOC 的 OCV 目标，允许按 `30min/1%` 慢速下修。当前默认首次下修约为 `10min target + 30min step`。
- `CHG`：只允许向上消化 deferred target，节拍 `10min/1%`。
- `DSG`：只允许向下消化 deferred target，节拍 `10min/1%`。
- 每次最多 `1%`。

稳定静置补偿：

- `RELAX` 下电压稳定至少 `5min`，且 `10min` 刷新节拍到达后建立可信 OCV 目标。
- `VCellMin/VCellMax` 相对参考值波动 `<=30mV`。
- 每累计 `10min` 刷新 deferred target；目标与当前方向不匹配时会清空，避免静置阶段直接把 SOC 快速拉到 OCV。
- 长时间不用车且 OCV 目标明显低于当前 SOC 时，`RELAX/RTC` 可以按 `30min/1%` 下修，避免电池久置没电但 SOC 仍长期挂高；默认首次下修约发生在稳定后约 `40min`。
- 重新骑行、电压跳动、低压表活跃或 sag/rebound holdoff 会清空静置可信度。
- 即使 `RELAX` 超过 `30min`，只要电压仍在回弹/跳动，也不会强行使用 OCV。

### 4.10 显示平滑

`soc_publish()` 将内部 SOC 转为对外显示 SOC，并写入 `g_stCellInfoReport.SocElement`。

| 场景 | 当前行为 |
| --- | --- |
| 普通上升/下降 | `5s / 1%` |
| 充电上升 | `5s / 1%` |
| 低压下降 | `VCellMin <= V0 + 50mV` 时 `1s / 1%` |
| 极低压下降 | `VCellMin <= V0 - 50mV` 时 `200ms / 1%` |
| `0x1005` 设置一次 SOC | 内部和显示强制同步 |
| `SOC_Fixed` | 只对外显示 `60%`，不破坏内部 SOC |
| `SOC_Zero` | 只对外显示 `0%`，不破坏内部 SOC |

## 5. 可配置参数

### 5.1 上位机/Flash 可写参数

| 参数 | 通信入口 | 有效范围 | 影响什么 | 建议 |
| --- | --- | --- | --- | --- |
| `OtherElement.u16Soc_Ah` | `0x2318` | `0..65535`，单位 `10*Ah`；`0` 使用默认 `270` | 出厂容量、积分比例、低压电流档位、SOH 容量基准 | 量产应写实测可用容量，不建议依赖 0 兜底 |
| `OtherElement.u16Soc_Cycle_times` | `0x2319` | `0..65535`，单位 `cycle` | 初始 `cycle_x100`、SOH、满充有效容量 | 现场换包/重置时才改；越大 SOH 越低 |
| `OtherElement.u16Soc_V_100` | `0x231A` | `0..5000mV`，`0` 使用默认 `4180mV` | 满电确认门槛 | 调低更容易到 100%，调高更严格 |
| `OtherElement.u16Soc_V_0` | `0x231B` | `0..5000mV`，`0` 使用默认 `3000mV` | 低压表基准、低压显示加速门槛 | 调高更保守，调低更耐看但欠压前可能偏高 |
| `OtherElement.u16Soc_TableSelect` | 当前映射 `0x230C` | `0..3` | 选择启动/静置 OCV 表 | 电芯体系必须匹配，否则静置和启动 SOC 偏差大 |
| `SOC_Table_Set[42]` | `0x2200~0x2229` | 21 组 `(mV, SOC%)`；源码未强校验 | `SOC_TABLE_TEST` 的 OCV 表 | 只更新 RAM，不跨重启保存；表应按电压降序、SOC 0..100 |
| 设置一次 SOC | `0x1005` | 建议 `0..100`，源码最终 clamp 到 `0..100` | 直接设置内部 SOC、容量和显示，并保存快照 | 售后/测试动作，用户会看到立即跳变 |
| `SOC_Fixed` | 系统功能开关 | `0/1` | 对外显示固定 `60%` | 只用于调试/生产功能验证 |
| `SOC_Zero` | 系统功能开关 | `0/1` | 对外显示固定 `0%` | 只用于调试/生产功能验证 |

写 `0x2318~0x231B` 后会触发 `RefreshData_Flag=2`，SOC 模块会重算容量、循环、SOH，并按当前内部 SOC 重新映射容量后保存。

### 5.2 编译配置参数

| 宏 | 默认 | BuildGuard 范围 | 影响什么 |
| --- | ---: | --- | --- |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | `80` | `0..500mV` | 普通满电确认和 `VCellMax` 基础门槛，越大越容易满电 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | `120` | `0..1000mV` | 满电允许的单体压差，越小越保守 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS` | `15` | `1..600s` | 普通满电确认持续时间 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS` | `5` | `1..600s`，且 `<=` 普通时间 | 快速满电确认持续时间 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT` | `95` | `0..100%` | 普通满电确认所需最低内部 SOC |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV` | `30` | `0..500mV` | 快速满电确认电压余量 |
| `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | `2000` | `1000..3500mV`，且小于 max | 电压校准最低有效单体电压 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | `5000` | `3600..6000mV`，且大于 min | 电压校准最高有效单体电压 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV` | `1000` | `0..3000mV` | 电压校准允许最大单体压差 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT` | `0` | `0/1` | 为 1 时三级保护故障阻断电压校准 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT` | `0` | `0/1` | 为 1 时 AFE/ADC/CBC/温度异常阻断电压校准 |
| `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` | `30` | `0..1800s` | 大电流压降后阻断电压校准的时间 |
| `PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV` | `50` | `0..500mV` | holdoff 期间允许末端校准的 `V0 + offset` 边界 |
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | `1800` | `60..43200s` | 静置计数上限/兼容配置；运行态 OCV 触发以稳定窗口为准 |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | `1` | `1..10%` | 自动校准单次最大步长，当前策略要求保持 `1` |
| `PROJECT_CFG_SOC_TEST_MODE_ENABLE` | `0` | `0/1` | MCU 注入式 SOC 测试入口，量产必须关闭 |
| `PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX` | `300` | `1..1000` | 单次测试注入最多模拟多少个 200ms tick |

`PROJECT_CFG_SOC_TEST_MODE_ENABLE=1` 必须配合 `PROJECT_CFG_BUILD_PROFILE=2`，Release 构建会被 build guard 拦截。

### 5.3 源码策略常量

这些常量不属于上位机可写参数；修改需要改源码并重新验证。

| 常量 | 当前值 | 影响什么 |
| --- | ---: | --- |
| `SOC_TICK_MS` | `200ms` | 安时积分时间基准，必须与 AFE 样本周期一致 |
| `SOC_TICKS_PER_SECOND` | `5` | 满电、低压、静置、显示计时换算 |
| `SOC_CURRENT_ACTIVE_A10` | `2` | 充放电模式判定门槛，`0.2A` |
| `SOC_DEFAULT_CAP_A10` | `270` | 参数容量为 0 时的冷启动兜底容量，`27Ah` |
| `SOC_SOH_MIN` | `80` | SOH 最低值 |
| `SOC_SOH_CYCLE_STEP` | `100` | 每 `100` 次循环 SOH 下降 `1%` |
| `SOC_DEFAULT_FULL_MV` | `4180mV` | `V100` 为 0 时的满电默认值 |
| `SOC_EMPTY_MV` | `3000mV` | `V0` 为 0 时的空电默认值 |
| `SOC_EMPTY_CUR_LIGHT_DIVIDER` | `5` | 轻载门槛 `C/5` |
| `SOC_EMPTY_CUR_MID_DIVIDER` | `2` | 中/重载门槛 `C/2` |
| `SOC_MID_MAX_DELTA_MV` | `200mV` | 稳定静置和中低压弱约束允许的最大单体压差 |
| `SOC_REST_STABLE_DELTA_MV` | `30mV` | 静置参考电压允许波动 |
| `SOC_SHORT_REST_MIN_SECONDS` | `300s` | 静置可信度最短稳定时间 |
| `SOC_SHORT_REST_STEP_SECONDS` | `600s` | 稳定静置每次 `1%` OCV 修正节拍 |
| `SOC_REBOUND_BOOT_HOLDOFF_SECONDS` | `300s` | 重载后跨重启回弹保护时间 |
| `SOC_SNAPSHOT_FLAG_REBOUND_HOLD` | `0x0001` | Flash 快照中的回弹保护标志 |
| `SOC_DISPLAY_NORMAL_SECONDS` | `5s` | 普通显示跟随速度 |
| `SOC_DISPLAY_CHG_SECONDS` | `5s` | 充电显示上升速度 |
| `SOC_DISPLAY_LOW_SECONDS` | `1s` | 低压显示下降速度 |

### 5.4 电流前端相关参数

这些参数在 `DataDeal.c`，不是 SOC 模块内部参数，但会直接改变 SOC 积分输入。

| 参数 | 当前值 | 影响什么 |
| --- | ---: | --- |
| `AFE_CURRENT_OUTPUT_DEADBAND_MA` / `AFE_CURRENT_OUTPUT_DEADBAND_A10` | `200mA` / `2` | AFE 输出死区，影响小电流是否进入 SOC |
| `AFE_CURRENT_AUTO_ZERO_LIMIT_MA` | `1000mA` | 自动零点学习允许的电流范围 |
| `AFE_CURRENT_AUTO_ZERO_STABLE_RAW` | `8` | 零点稳定判断窗口 |
| `AFE_CURRENT_AUTO_ZERO_CONFIRM_CNT` | `16` | 自动零点确认次数 |
| `AFE_CURRENT_AUTO_ZERO_FILTER_DIV` | `16` | 零点滤波强度 |

调 SOC 前必须先确认电流零点稳定，否则安时积分会长期偏移。

## 6. 对外输出与存储

### 6.1 对外输出

| 通道 | 输出内容 |
| --- | --- |
| RS485 `0xD000` 偏移 `52..57` | SOC、SOH、当前容量、满充容量、出厂容量、循环次数 |
| RS485 `0xC000` LCD 区 | 汇总总压、电流、温度、SOC |
| RS485 `0xD300` | SOC 测试模式状态，Release 下 `supported=0` 或不支持 |
| CAN 飞道协议 | SOC、SOH、容量、循环次数 |
| LedBar | 平滑后的 SOC 数字、充电图标、休眠前备份显示 |

### 6.2 Flash 快照

SOC 快照使用内部 Flash 双槽 journal，格式保持 V2 兼容：

| 字段 | 用途 |
| --- | --- |
| `u16SocNow` | 内部 SOC |
| `u16DsgSocInt` | 旧格式放电小数百分比兼容字段 |
| `u32CycleTimes` | `cycle_x100` |
| `u32CapNow` | 当前容量 `As*10` |
| `u32CapFull` | 当前有效满容量 `As*10` |
| `u32LearnPassedAs10` | 未满 1% 循环的放电累计 |
| `u16Flags bit0` | 重载后关机/重启回弹保护标志 |

保存触发：

- 内部 SOC 变化。
- 循环次数变化。
- SOH 变化导致 `cap_full_as10` 变化。
- `snapshot_flags` 变化。
- 设置一次 SOC、参数刷新、RTC OCV 修正等主动保存动作。

## 7. SOC 模块函数清单

### 7.1 `SOC.c`

| 函数 | 作用 |
| --- | --- |
| `SOC_TestMode_InputValid()` | 测试模式下校验注入样本的电压、电流和 tick 数范围 |
| `SOC_TestMode_ApplyReportSample()` | 测试模式下把注入样本写入 `g_stCellInfoReport`，模拟 AFE 报告 |
| `SOC_LimitA10()` | 将净电流限制到 `UINT16` 范围 |
| `SOC_GetNetCurrentForCalc()` | 合成 SOC 计算用净充/放电电流，包含 Type-C 电池侧等效电流抵消 |
| `SOC_LoadConfigData()` | 从 `OtherElement` 和 `SOC_Table_Set` 复制 SOC 配置到 `SOC_Enhance_Element` |
| `InitData_SOC()` | SOC 初始化入口，加载配置、初始化增强状态并发布数据 |
| `App_SOC()` | SOC 周期入口；有新 AFE 样本时执行算法，无新样本时只发布 |
| `SOC_TestMode_RunSample()` | MCU 注入式测试入口，按指定 tick 数加速运行 SOC 状态机 |
| `SOC_TestMode_ReadStatus()` | 读取测试模式状态字，供 `0xD300` 上位机探测 |

### 7.2 `SocEnhance.c`

| 函数 | 作用 |
| --- | --- |
| `soc_cell_delta()` | 计算当前最大/最小单体电压差 |
| `soc_abs_diff_u16()` | 计算两个无符号 16 位值的绝对差，用于静置稳定性判断 |
| `soc_step()` | 将 SOC 按指定步长靠近目标，自动避免越界 |
| `soc_factory_cap_as10_from()` | 将 `10*Ah` 参数转换为 `As*10`，参数为 0 时使用默认 27Ah |
| `soc_soh_from_cycle()` | 按 `cycle_x100` 计算 SOH，当前公式最低 80% |
| `soc_refresh_capacity_base()` | 根据 SOH 刷新 `cap_full_as10`，并限制当前容量不超过满容量 |
| `soc_from_cap()` | 由 `cap_now_as10/cap_full_as10` 反算内部 SOC |
| `soc_cap_to_ah100()` | 将内部 `As*10` 容量转换为对外 `Ah*100` |
| `soc_set()` | 设置内部 SOC，并同步当前容量、积分余量和满电锚点 |
| `soc_direction()` | 根据充/放电电流判定 `RELAX/CHG/DSG` |
| `soc_voltage_valid()` | 校验单体电压范围和 `VCellMax >= VCellMin` |
| `soc_voltage_with_margin()` | 计算 `base_mv - margin_mv`，防止下溢 |
| `soc_protection_fault_blocks_calibration()` | 在配置打开时检查三级保护故障是否阻断校准 |
| `soc_system_fault_blocks_calibration()` | 在配置打开时检查 AFE/ADC/CBC/温度异常是否阻断校准 |
| `soc_calibration_allowed()` | 统一电压校准门控：电压、压差、保护故障、系统故障 |
| `soc_ocv_table()` | 根据表选择返回当前 OCV 表和长度 |
| `soc_ocv_percent()` | 用当前最低单体电压查 OCV 表并限制到 `0..100` |
| `soc_empty_mv()` | 返回 `V0`，参数为 0 时使用默认 `3000mV` |
| `soc_empty_threshold_mv()` | 计算 `V0 + offset`，并做上下限保护 |
| `soc_full_mv()` | 返回 `V100`，参数为 0 时使用默认 `4180mV` |
| `soc_current_limit_a10()` | 按容量和 divider 计算 `C/5`、`C/2` 等电流门槛 |
| `SOC_UpdateSampleData()` | 外部写入本次 SOC 计算用电压/电流样本 |
| `SOC_PublishReportData()` | 将 `SOC_Enhance_Element` 的 SOC 数据发布到 `g_stCellInfoReport.SocElement` |
| `soc_save()` | 组装 V2 SOC 快照并写入 Flash journal |
| `soc_save_if_needed()` | SOC、循环或满容量变化时触发保存 |
| `soc_load_or_default()` | 加载有效快照；无效时按 OCV 或默认 60% 初始化 |
| `soc_add_discharge()` | 累计放电量，更新 `cycle_x100`，必要时刷新 SOH/SOC |
| `soc_integrate()` | 按 200ms tick 做充/放电安时积分 |
| `soc_apply_rest_ocv()` | 主动 OCV 刷新小步校正，单次最多 `SOC_CAL_STEP` |
| `soc_apply_rtc_rest_ocv()` | RTC 休眠 OCV 稳定窗口累计与 deferred target 刷新 |
| `soc_apply_ocv_target_step()` | 按当前模式方向约束执行一次 OCV 目标小步修正 |
| `soc_latch_rest_ocv_target()` | 电压稳定窗口满足后锁存可信 OCV 目标 |
| `soc_set_deferred_ocv_target()` | 记录后续充/放电阶段要消化的 OCV 目标 |
| `soc_apply_deferred_ocv_step()` | 在 `CHG/DSG` 中按方向和 `10min/1%` 节拍消化 deferred target |
| `soc_apply_long_rest_down_step()` | 久置且 OCV 目标低于当前 SOC 时，`RELAX/RTC` 按 `30min/1%` 慢速下修 |
| `soc_full_confirm_seconds()` | 判断当前是否满足快速/普通满电确认，并返回所需秒数 |
| `soc_empty_current_band()` | 按 `RELAX/C/5/C/2/>C/2` 选择低压表电流档位 |
| `soc_heavy_discharge_active()` | 判断是否处于 `Idsg > C/2` 大电流放电 |
| `soc_seconds_to_ticks()` | 将秒数换算为 200ms tick |
| `soc_update_sag_hold()` | 根据大电流状态刷新 sag holdoff 计数 |
| `soc_sag_hold_blocks_calibration()` | 判断 sag holdoff 是否阻断当前电压校准 |
| `soc_empty_tail_config()` | 按 `V0` 相对电压和电流档位查低压表目标/周期 |
| `soc_low_tail_active()` | 判断当前 tick 是否有低压表路径可用 |
| `soc_mid_tail_config()` | 按 `V0` 相对中低压区和电流档位查弱约束目标/周期 |
| `soc_mid_tail_active()` | 判断当前 tick 是否有中低压弱约束路径可用 |
| `soc_apply_mid_tail()` | 执行中低压弱约束，只向下小步限制明显高估 SOC |
| `soc_apply_full_empty()` | 执行满电确认或低压表收敛，二者互斥 |
| `soc_reset_rest_confidence()` | 清空静置可信度计数和参考电压 |
| `soc_rest_voltage_stable()` | 判断当前 RELAX 电压是否满足静置稳定条件 |
| `soc_update_rest_timer()` | RELAX 计时，电压稳定窗口满足后锁存 OCV 目标，并处理久置下修例外 |
| `soc_display_target()` | 计算显示目标，处理 `SOC_Fixed/SOC_Zero` 覆盖 |
| `soc_publish()` | 显示平滑、容量/SOH/循环换算、发布到对外数据源 |
| `soc_handle_command()` | 处理 `RefreshData_Flag`：OCV 刷新、参数刷新、设置一次 SOC |
| `soc_param_lib_init()` | 初始化 SOC 内部状态、加载快照并强制发布 |
| `SOC_ResetStoredSnapshotToDefault()` | 将 SOC 快照重置为默认 60%，用于升级策略或维护动作 |
| `SOC_IntEnhance_Ctrl()` | SOC 增强算法主状态机入口 |
| `SOC_ApplyRtcRelaxationCompensation()` | RTC 唤醒后按休眠时长和电压做静置 OCV 补偿 |
| `SOC_SaveSnapshotBeforeSleep()` | 进入 RTC/普通/深度休眠前刷新 SOC Flash 快照 |

## 8. 调参与验证建议

1. 先确认 AFE 电流零点和 Type-C 电流输入，再调 SOC 参数。
2. `u16Soc_Ah` 必须按实测可用容量设置；虚高会让 SOC 掉得慢并在欠压前高估。
3. 用户反馈长期不到 `100%` 时，优先查 `V100`、`VCellMin`、压差和满电确认时间。
4. 用户反馈低压仍显示高电量时，优先查 `V0`、中低压弱约束、低压表区间和 sag/rebound holdoff 是否长时间阻断。
5. OCV 表只适合启动、可信静置和弱约束参考，不要用它直接补偿大电流骑行压降。
6. 修改 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` 虽然 build guard 允许到 10，但当前用户体验原则要求保持 `1`。
7. 量产必须保持 `PROJECT_CFG_SOC_TEST_MODE_ENABLE=0`；测试模式只能在 Factory/Test profile 下打开。

推荐回归：

```bash
python3 tools/run_soc_host_c_test.py
python3 tools/soc_replay_test.py
python3 tools/project_check.py
git diff --check
```

没有板子时，按 [SOC 无板主机验证方案](SOC_HOST_VALIDATION_PLAN.md) 做算法门禁。当前 `tools/run_soc_host_c_test.py` 直接编译真实 `SOC.c`、`SocEnhance.c` 和 `PubFunc.c`，覆盖 `14` 个关键 C 源码路径；`tools/soc_replay_test.py` 覆盖 `43` 个场景，包含启动、积分、SOH、OCV、源码表格一致性、满电、中低压弱约束、低压表、稳定静置、deferred OCV、静置超过 30min 但电压不稳定不校准、长时间不用车、回弹保护、异常输入和随机不变量。

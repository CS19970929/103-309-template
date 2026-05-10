# SOC 逻辑与参数影响梳理

本文按当前源码实现梳理 SOC 估算链路、可调整参数、参数作用和对用户体验的影响。主要依据：

- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/SocEnhance.h`
- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/Can_HDX.c`

## 1. 总体结论

当前 SOC 不是单纯按电压查表，也不是单纯安时积分，而是以下几部分组合：

1. 200ms AFE 电流样本驱动安时积分。
2. Flash 双槽 journal 保存 SOC 快照，掉电后优先恢复快照。
3. 无快照或快照无效时，用 OCV 表估算启动 SOC；电压无效才回退到 60%。
4. 满电、低压表、静置/RTC OCV 都只做小步校准，每次最多 1%。
5. 内部 SOC 与对外显示 SOC 分离，对外输出经过平滑处理。
6. RS485、CAN、LedBar 最终都读取 `g_stCellInfoReport.SocElement`，也就是显示侧 SOC/SOH/容量。

这套策略偏向骑行体验：电量不轻易跳变，临近欠压时优先保守收敛，充满前不直接报 100%。

## 2. 运行链路

### 2.1 启动初始化

启动时 `InitDevice()` 先执行 `InitE2PROM()` 加载默认参数和 Flash 保存的可写参数，再执行 `InitData_SOC()`。

`InitData_SOC()` 做三件事：

1. 从 `OtherElement` 复制 SOC 基础参数到 `SOC_Enhance_Element`：
   - 额定容量 `u16Soc_Ah`
   - 初始循环次数 `u16Soc_Cycle_times`
   - SOC 表选择 `u16Soc_TableSelect`
   - 满电电压 `u16Soc_V_100`
   - 空电电压 `u16Soc_V_0`
   - 可写 OCV 表 `SOC_Table_Set`
2. 初始化内部 `SOC_STATE`。
3. 读取 Flash SOC 快照。

快照有效时恢复 `soc/cap_now/cycle/dsg_acc`。快照无效时，如果当前电压有效，就按 OCV 表估算；否则使用 `SOC_DEFAULT_STARTUP_PERCENT = 60%`。

### 2.2 周期更新

主循环中 `App_AFEGet()` 每 200ms 读取 AFE、更新单体电压/温度/电流，并递增 `g_u32AfeCurrentSampleSeq`。`App_SOC()` 只在该样本序号变化时运行完整 SOC 算法，避免重复积分同一笔电流。

SOC 输入：

- `u16VCellMax`
- `u16VCellMin`
- `u16Ichg`
- `u16IDischg`

电流单位是 `A * 10`。`DataLoad_Current()` 先做 AFE 电流零点补偿和 0.3A 输出死区，然后输出充/放电电流。

Type-C 输出电流由 `ADC_Current_Smooth()` 计算为 `g_u16TypeCOutCurrent_A10`。SOC 积分入口会把它作为额外放电电流参与净电流计算：

```text
SOC放电输入 = AFE放电 + Type-C放电 - AFE充电
SOC充电输入 = AFE充电 - AFE放电 - Type-C放电
```

因此 Type-C 单独放电会让 SOC 正常下降；边充边给 Type-C 输出时，会先抵消充电电流，避免 SOC 虚高。对外上报的 `g_stCellInfoReport.u16Ichg/u16IDischg` 暂不因此改变。

### 2.3 方向判定

方向由 `soc_direction()` 决定：

| 条件 | 模式 |
| --- | --- |
| `Ichg >= 0.4A` 且 `Ichg >= Idsg` | 充电 |
| `Idsg >= 0.4A` | 放电 |
| 其他 | 静置 |

0.4A 以下不参与 SOC 积分，主要用于抑制电流零漂导致的慢性 SOC 漂移。

## 3. 核心估算逻辑

### 3.1 安时积分

内部容量单位是 `As * 10`。额定容量来自 `u16Soc_Ah`：

```text
cap_factory_as10 = u16Soc_Ah * 3600
```

单次 200ms 增量：

```text
delta_as10 = (current_A10 * 200ms + rem_ms) / 1000
```

充电增加 `cap_now_as10`，放电减少 `cap_now_as10`。充电未经过满电确认前，内部 SOC 到 100% 会被压回 99%，防止只靠积分提前显示满电。

### 3.2 SOH 与容量

当前 SOH 不做复杂学习，只按循环次数映射：

```text
SOH = max(70, 100 - cycle / 50)
cap_full = cap_factory * SOH / 100
```

每累计放出 `factory_capacity / 100`，内部 `cycle_x100 + 1`；对外循环次数是 `cycle_x100 / 100`。

这意味着循环次数越高，SOH 越低，满充容量越小，同样剩余电量对应的续航展示也更保守。

### 3.3 OCV 查表

OCV 表按 `(电压mV, SOC%)` 成对存储，通过 `GetEndValue()` 做线性插值。默认三元锂表：

```text
4160/100, 4100/95, 4050/90, 3995/85, 3935/80,
3880/75, 3835/70, 3795/65, 3760/60, 3725/55,
3695/50, 3670/45, 3645/40, 3615/35, 3585/30,
3555/25, 3525/20, 3480/15, 3400/10, 3250/5,
3000/0
```

OCV 表主要影响启动估算和静置校正，不直接替代运行中的安时积分；骑行低压区由低压表单独收敛。

### 3.4 满电确认

满电确认不依赖 taper 电流，按单体电压确认：

| 条件 | 当前默认 |
| --- | --- |
| 满电基准 `V100` | `OtherElement.u16Soc_V_100`，默认 4180mV |
| `VCellMax` 门槛 | `V100 - PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV`，默认 4100mV |
| 快速确认 | `VCellMin >= V100 - 30mV`，且压差 `<=120mV`，持续 5s |
| 普通确认 | 内部 SOC `>=95%`，`VCellMin >= V100 - 80mV`，且压差 `<=120mV`，持续 15s |

确认过程每次只把内部 SOC 往 100% 推进 1%。因此用户看到的是慢慢到 100%，不是电压一到就跳满。

### 3.5 低压末端收敛

低压末端以 `V0 = OtherElement.u16Soc_V_0` 为基准，默认 3000mV。根据最低单体电压和放电倍率，给 SOC 设置一个保守上限，且每次最多下修 1%。

当前表可理解为：越接近 `V0`，SOC 越快往低值收敛；放电电流越大，目标 SOC 越高，避免负载压降把电量过早拉到 0。

| `VCellMin` 相对 `V0` | RELAX 目标 | 轻载 `<=C/5` | 中载 `<=C/2` | 重载 `>C/2` |
| --- | --- | --- | --- | --- |
| `V0 + 400mV` | 18% | 22% | 30% | 40% |
| `V0 + 300mV` | 14% | 18% | 25% | 32% |
| `V0 + 200mV` | 12% | 14% | 20% | 25% |
| `V0 + 100mV` | 8% | 10% | 14% | 18% |
| `V0 + 50mV` | 4% | 5% | 8% | 12% |
| `V0` | 0% | 0% | 0% | 0% |
| `V0 - 25mV` | 0% | 0% | 0% | 0% |
| `V0 - 50mV` | 0% | 0% | 0% | 0% |

达到目标的速度也随电压和电流变化，最低可到每 200ms 下修 1%。这会让用户在临近欠压时看到电量快速变保守，减少“明明快欠压还显示很多电”的体验问题。

### 3.6 大电流压降 holdoff

当放电电流 `> C/2` 时，进入 30s voltage sag holdoff。holdoff 期间如果 `VCellMin > V0 + 50mV`，禁止低压校正和静置 OCV 校正，避免大电流瞬态压降把 SOC 误拉低。

如果电压已经低到 `V0 + 50mV` 附近，则认为进入真实末端，仍允许低压收敛。

### 3.7 低压校准互斥

当前低压电压类校准只保留低压表一条路径。低压表活跃时会阻断静置 OCV，避免同一调度 tick 内出现多条校准路径竞争；大电流 sag holdoff 未解除时，若电压仍高于 `V0 + 50mV`，低压表也不动作。

### 3.8 静置/RTC OCV 校正

运行中静置满 30min，或 RTC 低功耗唤醒后传入休眠时长，会调用 OCV 小步校正。

方向规则：

- 静置模式可以向上或向下靠近 OCV 目标。
- 充电模式只允许向上修正。
- 放电模式只允许向下修正。
- 每次最多 1%。

这能减少长时间放置后 SOC 和开路电压不一致，但不会造成大幅跳电。

### 3.9 显示平滑

`soc_publish()` 会把内部 SOC 转成显示 SOC，再写入 `g_stCellInfoReport.SocElement`。

默认节奏：

| 场景 | 显示变化 |
| --- | --- |
| 普通变化 | 每 5s 变化 1% |
| 充电上升 | 与普通放电一致，每 5s 变化 1% |
| 低压下降 | 每 1s 或更快变化 1% |
| 强制刷新、固定 SOC、SOC 置零 | 立即刷新 |

因此通信、CAN、LedBar 看到的是平滑后的 SOC。优点是观感稳定；代价是内部 SOC 已变化时，对外显示可能滞后。

## 4. 可调整参数

### 4.1 上位机/Flash 可写参数

| 参数 | 通信入口 | 作用 | 用户体验影响 |
| --- | --- | --- | --- |
| `OtherElement.u16Soc_Ah` | `0x2318` | 额定容量，单位 `10 * Ah` | 调大后电量掉得慢、续航更乐观；调小后掉得快、更保守 |
| `OtherElement.u16Soc_Cycle_times` | `0x2319` | 初始循环次数 | 越大 SOH 越低，满充容量越小，显示更保守 |
| `OtherElement.u16Soc_V_100` | `0x231A` | 满电确认电压 | 调低更容易到 100%；调高可能长期 99% |
| `OtherElement.u16Soc_V_0` | `0x231B` | 空电/低压末端基准 | 调高会更早快速掉电并提示低电；调低会更耐看但可能欠压前仍显示偏高 |
| `OtherElement.u16Soc_TableSelect` | `0x230C` | OCV 表选择 | 影响启动和静置校正；电芯类型不匹配会导致放置后电量偏差 |
| `SOC_Table_Set[42]` | `0x2200~0x2229` | 用户自定义 OCV 表，仅表选择为 `SOC_TABLE_TEST` 时使用 | 可按实测曲线优化静置/启动 SOC；表写错会造成 SOC 估算严重偏差 |
| 一次性设置 SOC | `0x1005` | `0..100` 直接设置当前 SOC 并保存 | 适合售后校准；用户会看到电量立即跳变 |
| SOC 低电保护阈值 | `0x213C~0x2140` | 一/二/三级 SOC 低电告警/保护及滤波 | 阈值高则更早告警/限用；阈值低则续航感更足但风险更靠近欠压 |

注意：写 `0x2318~0x231B` 后会触发 `InitData_SOC()` 和 `RefreshData_Flag = 2`，内部容量基准会重算并保存。

### 4.2 编译配置参数

| 宏 | 默认 | 作用 | 用户体验影响 |
| --- | ---: | --- | --- |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | 80mV | 满电确认允许低于 V100 的余量 | 越大越容易确认满电；越小越严格 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | 120mV | 满电确认允许压差 | 越大越容易到 100%，但不一致电芯也可能被确认满；越小更保守 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS` | 15s | 普通满电确认持续时间 | 越大越不容易到 100%，越稳；越小更快满电但容易误判 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS` | 5s | 快速满电确认持续时间 | 电压非常接近满电时使用，必须不大于普通确认时间 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT` | 95% | 普通满电确认所需最低内部 SOC | 防止中低电量靠电压误跳 100%；不建议低于 90 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV` | 30mV | 快速满电确认允许低于 V100 的余量 | 越大越容易快速满电；三元锂建议 20~50mV |
| `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | 2000mV | OCV/电压校准最低有效电压 | 范围过宽会采信异常电压；过窄可能不校正 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | 5000mV | OCV/电压校准最高有效电压 | 同上 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV` | 1000mV | 校准允许的最大压差 | 越大越容易校正；越小越避免异常串影响 SOC |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT` | 0 | 三阶保护故障时是否禁止校准 | 打开后故障时 SOC 更稳定但纠偏变慢 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT` | 0 | AFE/ADC/CBC/温度异常时是否禁止校准 | 打开后异常采样更不易影响 SOC |
| `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` | 30s | 大电流压降后禁止误校准的时间 | 越长越不容易被瞬态压降拉低 SOC，但低电纠偏更慢 |
| `PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV` | 50mV | holdoff 期间低于 `V0+offset` 才允许末端校准 | 越大越保守，越不容易被压降误拉低 |
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | 1800s | 静置 OCV 校正等待时间 | 调小纠偏快但容易采信未稳定电压；三元锂建议 30min 起 |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | 1% | 自动校准单次最大步长 | 不建议改大。改大用户会看到跳电 |
| `PROJECT_CFG_SOC_TEST_MODE_ENABLE` | 0 | 打开 MCU 注入式 SOC 测试 | 量产必须关闭；打开后上位机可写样本加速跑 SOC |
| `PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX` | 300 | 单次注入最大 200ms tick 数 | 越大测试越快，但越偏离真实运行节奏 |

`PROJECT_CFG_BUILD_PROFILE=0` 的 Release 构建会强制禁止 `PROJECT_CFG_SOC_TEST_MODE_ENABLE=1`。

### 4.3 内部常量参数

这些是 `SocEnhance.c` 内部使用的短名。当前满电确认、压降 holdoff、静置 OCV、校准步长已经接到 `Project_Config.h`，可以通过 `PROJECT_CFG_*` 修改；时基、容量兜底、SOH 简化模型、低压表和电流档位枚举仍属于源码策略常量。

#### 4.3.1 时基与模式

| 常量 | 当前值 | 影响什么 | 建议怎么设置 |
| --- | ---: | --- | --- |
| `SOC_TICK_MS` | 200ms | 每次 SOC 积分代表的时间，直接影响安时积分速度 | 必须和 AFE 电流样本周期一致。当前 `App_AFEGet()` 是 200ms，不建议改 |
| `SOC_TICKS_PER_SECOND` | 5 | 1 秒对应多少个 SOC tick，用于满电、低压、静置计时 | 必须等于 `1000 / SOC_TICK_MS`。200ms 就是 5 |
| `SOC_CURRENT_ACTIVE_A10` | 4 | 充/放电模式判定门槛，单位 `A * 10`，即 0.4A | 应略高于电流零漂和输出死区。过低会漂 SOC，过高会漏算小电流 |
| `SOC_MODE_RELAX` | 0 | 内部静置模式枚举 | 不要改值 |
| `SOC_MODE_CHG` | 1 | 内部充电模式枚举 | 不要改值 |
| `SOC_MODE_DSG` | 2 | 内部放电模式枚举 | 不要改值 |

#### 4.3.2 容量与 SOH

| 常量 | 当前值 | 影响什么 | 建议怎么设置 |
| --- | ---: | --- | --- |
| `SOC_DEFAULT_CAP_A10` | 270 | 兜底额定容量，单位 `10 * Ah`，270 表示 27Ah；仅当参数容量为 0 时使用 | 正常应通过 `0x2318 u16Soc_Ah` 设置，不靠这个默认值 |
| `SOC_SOH_MIN` | 70 | SOH 最低显示 70% | 电池寿命策略。想更保守可提高；不建议低于 70 |
| `SOC_SOH_CYCLE_STEP` | 50 | 每 50 次循环 SOH 下降 1% | 调小会让 SOH 掉得更快，调大更乐观。没有实测老化数据前保持 50 |

#### 4.3.3 满电确认

| 常量 | 当前值 | 影响什么 | 建议怎么设置 |
| --- | ---: | --- | --- |
| `SOC_FULL_SECONDS` | 15s | 普通满电确认持续时间 | 越大越不容易到 100%，越稳；越小更快满电但容易误判 |
| `SOC_FULL_FAST_SECONDS` | 5s | 快速满电确认持续时间 | 电压非常接近满电时用。当前 5s 合理 |
| `SOC_FULL_MIN_SOC` | 95% | 普通满电确认要求内部 SOC 至少达到该值 | 防止中低电量靠电压误跳 100%。不建议低于 90 |
| `SOC_DEFAULT_FULL_MV` | 4180mV | 满电默认单体电压 | 三元锂当前 4180mV 合理。优先通过 `0x231A u16Soc_V_100` 调 |
| `SOC_FULL_FAST_MARGIN_MV` | 30mV | 快速满电条件：`VCellMin >= V100 - 30mV` | 越大越容易快速满电。三元锂建议 20~50mV |
| `SOC_FULL_MIN_MARGIN_MV` | 配置项 | 普通满电条件：`VCellMin >= V100 - margin`，当前来自 `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | 用户反馈长期 99%，可适当加大；反馈未充满就 100%，减小 |
| `SOC_FULL_MAX_DELTA_MV` | 配置项 | 满电确认允许的最大单体压差，当前来自 `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | 电芯一致性好可收紧；压差大仍允许满电会偏乐观 |

#### 4.3.4 空电与低压收敛

| 常量 | 当前值 | 影响什么 | 建议怎么设置 |
| --- | ---: | --- | --- |
| `SOC_EMPTY_MV` | 3000mV | 默认空电单体电压 | 三元锂 e-bike 当前 3000mV 合理。优先通过 `0x231B u16Soc_V_0` 调 |
| `SOC_EMPTY_CUR_LIGHT_DIVIDER` | 5 | 轻载分界：`C / 5`，27Ah 时约 5.4A | 当前适合 e-bike。越大，轻载门槛越低；越小，更多场景被当轻载 |
| `SOC_EMPTY_CUR_MID_DIVIDER` | 2 | 中/重载分界：`C / 2`，27Ah 时约 13.5A | 用于识别大电流压降。当前适合电动车场景 |
| `SOC_SAG_HOLDOFF_SECONDS` | 30s | 大电流压降后禁止误校准的时间 | 大负载时电量掉太快可加大；松油门后低电纠正太慢可减小 |
| `SOC_SAG_ALLOW_OFFSET_MV` | 50mV | holdoff 期间若电压仍高于 `V0 + 50mV`，就阻止低压/OCV 校准 | 越大越保守，越不容易被压降误拉低。当前 50mV 合理 |
| `SOC_EMPTY_BAND_RELAX` | 0 | 低压表静置档位枚举 | 不要改值 |
| `SOC_EMPTY_BAND_LIGHT` | 1 | 低压表轻载档位枚举 | 不要改值 |
| `SOC_EMPTY_BAND_MID` | 2 | 低压表中载档位枚举 | 不要改值 |
| `SOC_EMPTY_BAND_HEAVY` | 3 | 低压表重载档位枚举 | 不要改值 |
| `SOC_EMPTY_BAND_COUNT` | 4 | 低压表档位数量 | 不要改值，除非同步重写低压表逻辑 |

#### 4.3.5 静置 OCV 与校准步长

| 常量 | 当前值 | 影响什么 | 建议怎么设置 |
| --- | ---: | --- | --- |
| `SOC_REST_OCV_SECONDS` | 1800s | 静置 30min 后按 OCV 小步校正 | 调小纠偏快但容易采信未稳定电压；三元锂建议 30min 起 |
| `SOC_CAL_STEP` | 1% | 满电、低压表、静置 OCV 每次自动校准的最大步长 | 不建议改大。改大用户会看到跳电 |

#### 4.3.6 校准有效性

| 常量 | 当前值 | 影响什么 | 建议怎么设置 |
| --- | ---: | --- | --- |
| `SOC_VALID_MIN_MV` | 配置项 | OCV/满电/低压校准允许的最低单体电压，来自 `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | 当前默认 2000mV。不要太低，防异常采样 |
| `SOC_VALID_MAX_MV` | 配置项 | 校准允许的最高单体电压，来自 `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | 当前默认 5000mV。三元锂足够 |
| `SOC_VALID_MAX_DELTA_MV` | 配置项 | 校准允许最大单体压差，来自 `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV` | 默认 1000mV，偏宽。若要更保守，可收紧到 300~500mV |

#### 4.3.7 SOC 显示平滑

| 常量 | 当前值 | 影响什么 | 建议怎么设置 |
| --- | ---: | --- | --- |
| `SOC_DISPLAY_NORMAL_SECONDS` | 5s | 普通 SOC 每 5s 变化 1% | 越大越稳但滞后；越小更跟手但跳动明显 |
| `SOC_DISPLAY_CHG_SECONDS` | 5s | 充电上涨与普通放电同速，每 5s 变化 1% | 保持充放电显示节奏一致；若后续需要更稳，可单独加大 |
| `SOC_DISPLAY_LOW_SECONDS` | 1s | 低压下降每 1s 变化 1% | 低电时更快变保守。若用户觉得低电掉太猛，可加大，但会增加欠压前高估风险 |

#### 4.3.8 电流前端相关常量

这些常量在 `DataDeal.c`，不是 `SocEnhance.c`，但会直接影响 SOC 积分输入：

| 常量 | 当前值 | 影响什么 | 建议怎么设置 |
| --- | ---: | --- | --- |
| `AFE_CURRENT_OUTPUT_DEADBAND_A10` | 3 | 0.3A 电流输出死区 | 越大越抗零漂，但小负载不计入 SOC；越小更灵敏但容易漂 |
| `AFE_CURRENT_AUTO_ZERO_LIMIT_MA` | 1000mA | 自动零点允许学习的最大电流范围 | 过大可能带载学习成零点，过小可能零点迟迟不 ready |
| `AFE_CURRENT_AUTO_ZERO_STABLE_RAW` | 8 | 自动零点稳定判断窗口 | 越小越严格，越大越容易学习 |
| `AFE_CURRENT_AUTO_ZERO_CONFIRM_CNT` | 16 | 运行中自动零点确认次数 | 越大越稳但学习慢 |
| `AFE_CURRENT_AUTO_ZERO_FILTER_DIV` | 16 | 零点滤波强度 | 越大变化越慢，越小跟随更快 |

调 SOC 前应先确认电流零点稳定。电流零漂会让安时积分长期偏移，调 OCV 表无法根治。

### 4.4 显示相关参数

| 参数 | 默认 | 作用 | 用户体验影响 |
| --- | ---: | --- | --- |
| `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS` | 500 | 按键唤醒后 SOC 显示窗口，单位 10ms | 默认显示 5s，调大显示更久但耗电更多 |
| `PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_ENABLE` | 0 | 是否启用 LedBar 数字避重影映射 | 打开后部分 SOC 数字可能被微调，观感更清晰但数值不完全精确 |
| `PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_WINDOW` | 2 | 允许微调范围 | 范围越大越可能偏离真实 SOC |
| `PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_MIN_EXTRA` | 4 | 启动避重影的额外段数阈值 | 越低越容易触发微调 |
| `PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_MIN_GAIN` | 1 | 替代数字必须改善的幅度 | 越大越不容易替换数字 |

LedBar 直接取 `g_stCellInfoReport.SocElement.u16Soc`，也就是平滑后的 SOC。

## 5. 对外输出

| 通道 | 输出内容 |
| --- | --- |
| RS485 `0xD000` 偏移 `52..57` | SOC、SOH、当前容量、满充容量、出厂容量、循环次数 |
| RS485 `0xC000` LCD 区 | 汇总总压、电流、温度、SOC |
| RS485 `0xD300` | SOC 测试模式状态，Release 下 `supported=0` |
| CAN 飞道协议 | SOC、SOH、容量、循环次数 |
| LedBar | 平滑后的 SOC 数字、充电图标、休眠前备份显示 |

## 6. 调参建议

1. 先确认电流零点稳定，再调 SOC。电流零漂会让安时积分长期偏移，调 OCV 表无法根治。
2. 容量 `u16Soc_Ah` 按实测可用容量设置，不要按标称虚高设置。虚高会直接导致掉电慢和欠压前高估。
3. `V100` 以充电器实际截止策略为准。若用户反馈长期 99%，优先看 `V100` 和满电压差门槛。
4. `V0` 以整车欠压保护前的最低单体电压为准。用户反馈“还有电就突然没电”，一般需要提高低压收敛保守性；反馈“掉到 0 太早”，再考虑降低 `V0` 或低压表目标。
5. OCV 表只适合修启动和静置，不要用它补偿大电流骑行压降。
6. SOC 低电保护阈值是保护体验参数，不是估算参数。它会影响告警/保护时机，但不会让 SOC 算得更准。
7. 量产配置必须保持 `PROJECT_CFG_SOC_TEST_MODE_ENABLE=0`，测试入口只用于 MCU 注入式验证。

## 7. 当前风险点

1. SOC 表 `0x2200~0x2229` 写入后只更新 RAM 表；是否持久化需结合当前 Flash RW 参数保存策略确认。
2. 对外 SOC 是显示平滑值，不是内部瞬时值；做台架比对时需要考虑显示滞后。
3. 调 `u16Soc_Ah / V100 / V0 / OCV 表` 会直接影响用户可见电量，应通过骑行回放或上位机注入样本验证后再出货。

# SOC 完整运行流程说明

本文按当前源码说明 SOC 从上电、采样、积分、校准、保存到对外显示的完整运行流程。本文只描述当前有效实现，不沿用历史阶段文档里的旧方案。

## 1. 模块边界

当前 SOC 由以下模块协作完成：

| 模块 | 职责 |
| --- | --- |
| `SOC.c` | SOC 应用入口，加载参数，合成净电流，按 AFE 样本序号防重复积分，处理 SOC 测试模式 |
| `SocEnhance.c` | SOC 核心状态机，负责容量积分、SOH、满电确认、低压尾段、静置 OCV、显示平滑和快照保存 |
| `DataDeal.c` | 每 200ms 读取 AFE 电压/电流，更新 `g_stCellInfoReport`，递增 `g_u32AfeCurrentSampleSeq` |
| `ADC.c` | 计算 Type-C 输出侧电流和 ADC 分压总压；Type-C 输出侧电流由 SOC 换算成电池侧等效放电电流 |
| `Flash.c/.h` | SOC V2 双槽 journal 快照读写，并兼容旧 V1 快照 |
| `Sci_Upper.c` | RS485 参数读写、SOC 表写入、设置一次 SOC、SOC 测试模式命令 |
| `Can_HDX.c` / `LedBar.c` | 读取 `g_stCellInfoReport.SocElement` 对外广播或显示 SOC |

整体数据流：

```text
AFE 电压/电流 + Type-C 输出电流 + 参数 + Flash 快照
        -> SOC.c
        -> SocEnhance.c
        -> g_stCellInfoReport.SocElement
        -> RS485 / CAN / LedBar
```

## 2. 输入数据

### 2.1 AFE 电压

AFE 单体电压由 `DataLoad_CellVolt()` 更新到：

```text
g_stCellInfoReport.u16VCell[]
g_stCellInfoReport.u16VCellMax
g_stCellInfoReport.u16VCellMin
g_stCellInfoReport.u16VCellDelta
g_stCellInfoReport.u16VCellTotle
```

SOC 使用：

- `VCellMax`：满电确认。
- `VCellMin`：OCV 查表、中低压弱约束、低压尾段、静置稳定判断。
- `VCellDelta`：压差门控。
- `VCellTotle`：Type-C 输出功率换算为电池侧等效电流时作为电池包电压，单位 `10mV`。

### 2.2 AFE 主回路电流

`DataLoad_Current()` 读取 SH367309 CADC 电流，经过原始码解析、自动零点补偿、mA 换算和 K/B 预留校准后输出：

```text
g_stCellInfoReport.u16Ichg
g_stCellInfoReport.u16IDischg
```

单位是 `A * 10`，例如 `2 = 0.2A`，`270 = 27A`。

AFE 输出死区当前为：

```text
AFE_CURRENT_OUTPUT_DEADBAND_MA = 200mA
AFE_CURRENT_OUTPUT_DEADBAND_A10 = 2
```

低于 `200mA` 的 AFE 电流保持为 0，`200mA` 及以上允许进入 SOC。

### 2.3 Type-C 输出支路电流

Type-C PA2 ADC 采的是 Type-C 输出支路分流器压降，不经过 AFE 主回路采样。它先计算输出侧电流：

```text
g_u16TypeCOutCurrent_mA
g_u16TypeCOutCurrent_A10
```

这两个变量表达的是 Type-C 输出侧电流，不是电池侧电流，不能直接扣 SOC。

SOC 使用以下换算后的变量：

```text
g_u16TypeCBatEquivCurrent_mA
g_u16TypeCBatEquivCurrent_A10
```

换算公式：

```text
Ibat_equiv = I_typec_out * V_typec_out / V_bat / efficiency
```

当前默认：

```text
V_typec_out = 9000mV
efficiency = 90%
```

电池包电压优先使用：

```text
g_stCellInfoReport.u16VCellTotle * 10mV
```

如果该值还没有更新，则回退使用：

```text
g_u32Vbat_mV
```

SOC 净电流合成时只使用 `g_u16TypeCBatEquivCurrent_A10`。

### 2.4 SOC 参数

SOC 参数来自 `OtherElement`：

| 参数 | 含义 |
| --- | --- |
| `u16Soc_Ah` | 出厂容量，单位 `0.1Ah`，例如 `270 = 27Ah` |
| `u16Soc_Cycle_times` | 初始循环次数 |
| `u16Soc_TableSelect` | OCV 表选择 |
| `u16Soc_V_100` | 满电电压 `V100` |
| `u16Soc_V_0` | 空电电压 `V0` |

上位机写 `0x2318~0x231B` 会刷新容量、循环、`V100`、`V0`。写表选择 `0x230C` 会立即重载 SOC 配置。

### 2.5 Flash SOC 快照

SOC V2 快照保存：

```text
SOC
cap_now
cap_full
cycle_x100
dsg_acc_as10
snapshot_flags
```

其中 `snapshot_flags bit0` 是大电流重载后关机的 rebound hold 标志。

## 3. 调度链路

主循环调用链：

```text
main()
  -> Runtime_RunOnce()
     -> App_AFEGet()
        -> SysTime_Take200msTaskPeriod()
        -> DataLoad_CellVolt()
        -> DataLoad_CellVoltMaxMinFind()
        -> DataLoad_Temperature()
        -> DataLoad_Current()
        -> ++g_u32AfeCurrentSampleSeq
        -> App_SOC()
```

`Runtime_RunOnce()` 中单独的 `App_SOC()` 当前已注释，正常 SOC 只在 `App_AFEGet()` 的 200ms AFE 周期内调用。

`g_u32AfeCurrentSampleSeq` 仍然有实际价值：

- 它标记 AFE 电流样本已经更新。
- `App_SOC()` 只有发现序号变化时才运行完整算法。
- 如果未来有人在其他路径误调用 `App_SOC()`，不会重复积分同一份 AFE 样本。
- host 测试也用这个序号模拟真实 AFE 采样节拍。

## 4. 上电初始化

入口：

```text
InitData_SOC()
```

初始化顺序：

1. `SOC_LoadConfigData()` 从 `OtherElement` 复制容量、循环、表选择、`V100`、`V0`。
2. 复制上位机可写 `SOC_Table_Set[]` 到 `SOC_Enhance_Element.SOC_Table_CanSet[]`。
3. `soc_param_lib_init()` 初始化内部状态。
4. 按 `u16Soc_Ah` 计算出厂容量；如果参数为 0，使用默认 `27Ah`。
5. 按循环次数计算 SOH，最低 `80%`。
6. 尝试读取 Flash SOC 快照。
7. 快照有效时恢复 SOC、容量、循环和 rebound hold 标志。
8. 快照无效时，电压有效则按 OCV 表启动；电压无效则默认 `60%`。
9. 强制发布一次 `SocElement`，保证通信和显示有确定值。

内部容量单位：

```text
cap_factory_as10 = u16Soc_Ah * 3600
```

`u16Soc_Ah` 单位是 `0.1Ah`，内部容量单位是 `As * 10`。

SOH 公式：

```text
cycle = cycle_x100 / 100
SOH = clamp(100 - cycle / 100, 80, 100)
cap_full_as10 = cap_factory_as10 * SOH / 100
```

## 5. 周期运行入口

入口：

```text
App_SOC()
```

运行逻辑：

1. 如果 SOC 测试模式启用，只发布当前值，不走真实 AFE 样本。
2. 比较 `g_u32AfeCurrentSampleSeq` 和本地保存的上次序号。
3. 序号未变化：只发布当前显示 SOC，不积分。
4. 序号变化：合成净电流，写入本次电压/电流样本，调用 `SOC_IntEnhance_Ctrl()`。

## 6. 净电流合成

SOC 先把 Type-C 输出侧电流换算为电池侧等效放电电流：

```text
typec_bat_a10 = I_typec_out * 9000mV / Vbat / 0.9
```

然后合成净电流：

```text
dsg_a10 = AFE放电 + Type-C电池侧等效放电
chg_a10 = AFE充电
```

如果 `chg_a10 >= dsg_a10`：

```text
SOC充电电流 = chg_a10 - dsg_a10
SOC放电电流 = 0
```

否则：

```text
SOC充电电流 = 0
SOC放电电流 = dsg_a10 - chg_a10
```

这样边充边 Type-C 输出时，不会把外放功率误当成净充电。

## 7. 运行模式判定

SOC 有三个模式：

| 模式 | 条件 |
| --- | --- |
| `CHG` | `Ichg >= 0.2A` 且 `Ichg >= Idsg` |
| `DSG` | `Idsg >= 0.2A` |
| `RELAX` | 其他情况 |

有效电流阈值：

```text
SOC_CURRENT_ACTIVE_A10 = 2
```

即 `0.2A`。低于该门槛视作静置，不积分。

## 8. 安时积分

每个 tick 是 `200ms`：

```text
SOC_TICK_MS = 200
SOC_TICKS_PER_SECOND = 5
```

积分公式：

```text
acc_ms = current_A10 * 200 + rem_ms
delta_as10 = acc_ms / 1000
rem_ms = acc_ms % 1000
```

### 8.1 CHG

充电时：

```text
cap_now_as10 += delta_as10
cap_now_as10 <= cap_full_as10
soc = cap_now_as10 / cap_full_as10
```

如果还没有满电锚点，即 `full_anchor = 0`，即使积分到 `100%` 也压回 `99%`。必须经过满电确认才能到 `100%`。

### 8.2 DSG

放电时：

```text
cap_now_as10 -= delta_as10
dsg_acc_as10 += delta_as10
full_anchor = 0
```

每累计放出出厂容量的 `1%`：

```text
cycle_x100 += 1
```

循环增加后会重新计算 SOH 和 `cap_full_as10`。

### 8.3 RELAX

静置时：

```text
不改 cap_now_as10
清空 rem_ms
```

## 9. 电压校准门控

所有电压类校准都必须先通过：

```text
soc_calibration_allowed()
```

门控条件：

| 条件 | 当前规则 |
| --- | --- |
| 单体电压范围 | `2000mV <= VCellMin <= VCellMax <= 5000mV` |
| 单体压差 | `VCellMax - VCellMin <= 1000mV` |
| 保护故障 | 配置打开时，三级保护故障阻断校准 |
| 系统故障 | 配置打开时，AFE/ADC/CBC/温度异常阻断校准 |

门控失败只阻断电压校准，不停止安时积分。

## 10. sag/rebound holdoff

大电流定义：

```text
Idsg > C/2
```

触发后：

```text
sag_hold_ticks = 30s
snapshot_flags |= SOC_SNAPSHOT_FLAG_REBOUND_HOLD
```

holdoff 期间：

| 电压 | 行为 |
| --- | --- |
| `VCellMin > V0 + 50mV` | 认为可能是大电流压降，阻断低压表、OCV 等电压校准 |
| `VCellMin <= V0 + 50mV` | 认为接近真实末端，允许低压表向 0 收敛 |

如果 holdoff 期间关机，快照会保留 rebound 标志。下次开机进入 `5min` rebound holdoff，防止未回弹电压把 SOC 校低。

## 11. 满电确认

满电确认不依赖 taper 电流，也不要求必须处于 `CHG`，只要求不是 `DSG`。

基础条件：

```text
mode != DSG
soc_calibration_allowed() 通过
VCellMax > 4180mV
VCellMax >= V100 - 80mV
```

`VCellMax > 4180mV` 是置 `100%` 的硬门槛，`4180mV` 本身不触发满电锚定。

快速确认：

```text
VCellMin >= V100 - 30mV
VCellMax - VCellMin <= 120mV
持续 5s
```

普通确认：

```text
内部 SOC >= 95%
VCellMin >= V100 - 80mV
VCellMax - VCellMin <= 120mV
持续 15s
```

确认满足后：

```text
soc = soc_step(soc, 100, 1)
```

每次最多向 `100%` 推进 `1%`，所以能到 `100%`，但不会大跳。

## 12. 中低压弱约束

中低压弱约束用于处理“电压已经偏低但 SOC 明显偏高”的场景。

条件：

- 非 `CHG`。
- 电压有效。
- 单体压差 `<=200mV`。
- 不在 sag/rebound 阻断中。
- `VCellMin > V0 + 400mV`，避免与低压尾段重叠。
- 只向下修正，不向上拉高。

默认 `V0=3000mV` 时，中低压表覆盖 `3500mV~3700mV`：

| `VCellMin` 相对 `V0` | RELAX | 轻载 `<=C/5` | 中载 `<=C/2` | 重载 `>C/2` |
| --- | --- | --- | --- | --- |
| `V0 + 500mV` | `25% / 90s` | `32% / 90s` | `42% / 120s` | 禁用 |
| `V0 + 600mV` | `35% / 120s` | `42% / 120s` | `50% / 150s` | 禁用 |
| `V0 + 650mV` | `45% / 150s` | `50% / 150s` | `58% / 180s` | 禁用 |
| `V0 + 700mV` | `55% / 180s` | `60% / 180s` | 禁用 | 禁用 |

## 13. 低压尾段表

低压尾段保证低压末端 SOC 能逐步到 `0%`，同时避免过早跳变。

条件：

- 非 `CHG`。
- 电压有效。
- 不被 sag/rebound 阻断，除非已经进入真实末端。
- 只向下修正，不向上拉高。
- 每次最多 `1%`。

默认 `V0=3000mV`：

| `VCellMin` 相对 `V0` | RELAX | 轻载 `<=C/5` | 中载 `<=C/2` | 重载 `>C/2` |
| --- | --- | --- | --- | --- |
| `V0 + 400mV` | `18% / 24s` | `22% / 20s` | `30% / 16s` | `40% / 12s` |
| `V0 + 300mV` | `14% / 18s` | `18% / 15s` | `25% / 12s` | `32% / 9s` |
| `V0 + 200mV` | `12% / 12s` | `14% / 10s` | `20% / 8s` | `25% / 6s` |
| `V0 + 100mV` | `8% / 7s` | `10% / 6s` | `14% / 5s` | `18% / 4s` |
| `V0 + 50mV` | `4% / 4s` | `5% / 3s` | `8% / 2s` | `12% / 1.6s` |
| `V0` | `0% / 2s` | `0% / 1s` | `0% / 1s` | `0% / 1s` |
| `V0 - 25mV` | `0% / 1s` | `0% / 1s` | `0% / 0.2s` | `0% / 0.2s` |
| `V0 - 50mV` | `0% / 0.2s` | `0% / 0.2s` | `0% / 0.2s` | `0% / 0.2s` |

低压表活跃时阻断静置 OCV，保证同一个 tick 只有一条主要电压校准路径。

## 14. 静置 / RTC OCV

当前静置 OCV 不是“静置 30min 到点立即校准”。

运行态 `RELAX` 下必须先满足稳定窗口：

```text
电压有效
压差受控
无 sag/rebound holdoff
VCellMin/VCellMax 相对参考值波动 <= 30mV
稳定累计 >= 5min
10min OCV 刷新节拍到达
```

满足后只记录：

```text
deferred_ocv_target
```

不立即改内部 SOC。

后续消化规则：

| 情况 | 处理 |
| --- | --- |
| target 高于当前 SOC | 只有后续 `CHG` 时按 `10min/1%` 上修 |
| target 低于当前 SOC | 只有后续 `DSG` 时按 `10min/1%` 下修 |
| 长时间不用车且 target 低于当前 SOC | `RELAX/RTC` 可按 `30min/1%` 慢速下修；当前默认首次约为 `10min target + 30min step` |

如果电压继续回弹、跳动、重新骑行、低压表活跃或 sag/rebound holdoff 生效，会清空静置可信度。

RTC 唤醒路径也使用同样的稳定窗口。CAN active 下 1s 唤醒和 idle 下 10s 唤醒只影响探测频率，不直接决定 SOC 校准频率。

## 15. OCV 表

OCV 表用途：

- 无有效快照冷启动。
- 稳定静置/RTC 的 deferred target。
- 主动 OCV 刷新命令。
- 中低压和低压策略的电芯体系参考。

它不替代运行中的安时积分。

表选择：

| 值 | 表 |
| --- | --- |
| `0` | 上位机写入的 `SOC_Table_Set`，当前只在 RAM 生效 |
| `1` | 内置 LFP 表 |
| `2` | 内置三元锂表 |
| `3` | 内置备用 LFP 表 |

## 16. 上位机命令和参数刷新

| 地址 | 作用 | SOC 影响 |
| --- | --- | --- |
| `0x1005` | 设置一次 SOC | 立即设置内部 SOC、容量和显示 SOC，并保存快照 |
| `0x2200` | 写 42 个 halfword 自定义 OCV 表 | 只更新 RAM 表，不跨重启保存 |
| `0x230C` | 写 SOC 表选择 | 立即 `InitData_SOC()` 重载 SOC 表选择 |
| `0x2318` | 写容量 `u16Soc_Ah` | 重算容量、SOH 和当前容量映射 |
| `0x2319` | 写循环次数 | 重算 SOH 和满充容量 |
| `0x231A` | 写 `V100` | 影响满电确认门槛 |
| `0x231B` | 写 `V0` | 影响低压表、中低压弱约束和显示低压加速 |
| `0x2500` | SOC 测试样本注入 | 仅测试固件启用，量产关闭 |
| `0xD300` | SOC 测试状态读取 | 量产读到 `supported=0` 正常 |

## 17. Flash 快照保存

保存触发：

- 内部 SOC 变化。
- 循环次数变化。
- SOH 变化导致 `cap_full_as10` 变化。
- `snapshot_flags` 变化。
- 设置一次 SOC。
- 参数刷新。
- 进入休眠前强制保存。

保存字段：

```text
u16SocNow
u32CycleTimes
u32CapNow
u32CapFull
u32LearnPassedAs10
u16Flags
```

`u16Flags bit0` 表示 rebound holdoff。

## 18. 显示平滑

内部 SOC：

```text
s_soc.soc
```

对外显示 SOC：

```text
s_soc.display_soc
```

显示规则：

| 场景 | 速度 |
| --- | --- |
| 普通上升/下降 | `5s / 1%` |
| 充电上升 | `5s / 1%` |
| `VCellMin <= V0 + 50mV` 低压下降 | `1s / 1%` |
| `VCellMin <= V0 - 50mV` 极低压下降 | `200ms / 1%` |
| `SOC_Fixed` | 只对外显示 `60%`，不破坏内部 SOC |
| `SOC_Zero` | 只对外显示 `0%`，不破坏内部 SOC |

发布字段：

```text
SOC_Enhance_Element.u8_SOC
SOC_Enhance_Element.u8_SOH
SOC_Enhance_Element.u16_CapacityNow
SOC_Enhance_Element.u16_CapacityFull
SOC_Enhance_Element.u16_CapacityFactory
SOC_Enhance_Element.u16_Cycle_times
```

随后写入：

```text
g_stCellInfoReport.SocElement
```

RS485、CAN、LedBar 都读取这里。

## 19. 典型场景

### 19.1 正常骑行

```text
Idsg >= 0.2A
mode = DSG
每 200ms 扣 cap_now
SOC 按容量下降
显示 SOC 按 5s/1% 平滑跟随
```

### 19.2 Type-C 输出

```text
PA2 采到 9V 输出侧电流
SOC 按 9000mV、包电压和 90% 效率换算为电池侧等效电流
等效电流加到 SOC 放电侧
```

如果 AFE 主回路同时有充电电流，则先抵消 Type-C 等效放电，再计算净充电。

### 19.3 大电流爬坡

```text
Idsg > C/2
进入 30s sag holdoff
VCellMin > V0 + 50mV 时阻断电压校准
```

这样不会把大电流压降误判为真实低电量。

### 19.4 真实低压末端

```text
VCellMin <= V0 + 50mV
低压表允许工作
SOC 按表逐步向低目标或 0 收敛
显示下降加速
```

### 19.5 充满

```text
充电积分最多先到 99%
电压满足快速/普通满电确认
每次 1% 向 100 推进
```

### 19.6 停车静置

```text
RELAX
电压稳定 <=30mV
累计至少 5min 且 10min 刷新节拍到达后记录 deferred OCV target
不立即改 SOC
后续 CHG/DSG 方向匹配时按 10min/1% 消化
```

### 19.7 久置自放电

```text
稳定久置
OCV target 低于当前 SOC
RELAX/RTC 下按 30min/1% 慢速下修
当前默认首次下修约为 10min target + 30min step
```

## 20. 当前验证入口

推荐每次改 SOC 后执行：

```bash
python3 tools/run_soc_host_c_test.py
python3 tools/soc_replay_test.py
python3 tools/project_check.py
git diff --check
```

本轮关键验证点：

- Type-C 输出侧电流按电池侧等效电流抵消充电电流。
- `0.2A` 有效电流门槛进入 `CHG/DSG`。
- AFE 输出死区同步为 `200mA`。
- `g_u32AfeCurrentSampleSeq` 仍用于防重复积分。
- 满电、低压、静置 OCV、sag/rebound、显示平滑和快照逻辑保持原有边界。

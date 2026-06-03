# SOC 无放电静置快降分析

文档状态：已按源码验证
源码验证日期：2026-06-03
验证范围：只读分析当前 SOC 源码、低功耗 RTC 补偿链路和配置宏，未修改 `.c/.h`、Keil 工程、协议、SOC 表、阈值和时间参数
未验证范围：未执行 Keil 编译、真板静置、RTC STOP 功耗、CAN/Modbus 在线读取和 Keil watch 实测
主要参考源码：`103 + 309/Project/Source/SocEnhance.c`、`SocEnhance.h`、`rtc_sleep.c`、`rtc_sleep_port.c`、`RTC.c`、`conf/Project_Config.h`

## 1. 结论

用户反馈“没有放电时，感觉静置 SOC 校准很快”。按当前源码判断，普通静置 OCV 本身不是快降主因。

更可能的来源按优先级排序：

| 优先级 | 可能来源 | 判断 |
|---|---|---|
| 1 | `RELAX` 模式下 low tail / mid tail 生效 | 最可疑。当前 low/mid tail 不要求放电，只排除充电和若干阻塞条件；无放电但电压进入 V0 附近区间时，仍可能按尾端表下修 SOC |
| 2 | 显示 SOC 低压快速追赶 | 内部 SOC 已经下修后，`display_soc` 在低压区可以按 `1s/1%` 或 `200ms/1%` 追赶，用户会感觉掉得很快 |
| 3 | RTC STOP 周期内已提前补偿 | HICCUP STOP 中每次 RTC 周期唤醒都会累计休眠秒数并调用 SOC RTC 补偿；最终用户唤醒看到的是补偿后的显示结果 |
| 4 | 板载自耗积分 | 当前默认 `30mA`，对 27Ah 电池约 9 小时才 1%，不是秒级或分钟级快降主因 |
| 5 | 普通静置 OCV 长静置下修 | 连续普通运行 RELAX 下，首次实际下修通常约 60 分钟量级，不应表现为很快 |

因此，“无放电静置快降”排查时不要先把所有现象归因到 `REST_TARGET` 或 `LONG_REST_DOWN`。第一步应看 `u8LastCalibSource` 是否为 `EMPTY_TAIL` / `MID_TAIL`。

## 2. 当前无放电如何进入 RELAX

当前方向判断在 `soc_direction()`：

- `u16_Ichg >= SOC_CURRENT_ACTIVE_A10` 且大于等于放电电流时判为 `SOC_MODE_CHG`。
- `u16_Idsg >= SOC_CURRENT_ACTIVE_A10` 时判为 `SOC_MODE_DSG`。
- 其他情况判为 `SOC_MODE_RELAX`。

源码证据：`SocEnhance.c:315-327`。

这里的 `SOC_CURRENT_ACTIVE_A10` 是 A*10 口径。也就是说充电/放电电流都低于约 `0.2A` 时，会进入 `RELAX`。用户说“没有放电时”，通常就是这条路径。

进入 `RELAX` 后，容量积分仍会按板载自耗扣容量：

- `soc_integrate_current_ma(SOC_MODE_RELAX)` 返回 `-SOC_BOARD_SELF_CONSUMPTION_MA`。
- 默认 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA = 30`。

源码证据：`SocEnhance.c:329-342`、`Project_Config.h:173-174`。

这条链路是慢变量。估算公式：

```text
1% SOC 对应容量 = CapacityAh / 100
30mA 自耗导致 1% 变化时间 = (CapacityAh / 100) / 0.03A
```

示例：

| 额定容量 | 30mA 自耗到 1% 的时间 |
|---|---:|
| 27Ah | 约 9 小时 |
| 10.4Ah | 约 3.5 小时 |

所以如果现场看到几秒、几十秒、几分钟掉 1% 或多格，自耗积分不是主因。

## 3. 普通静置 OCV 为什么不应该很快

当前配置：

| 配置 | 当前值 | 含义 |
|---|---:|---|
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | 1800s | 静置 OCV 基础等待 |
| `PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS` | 300s | 稳定最短时间 |
| `PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS` | 600s | 静置目标锁存周期 |
| `PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS` | 1800s | 长静置下修 1% 周期 |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | 1% | 自动校准单次最大步进 |

源码证据：`Project_Config.h:158-171`。

普通运行链路：

1. `soc_update_rest_timer()` 只在 `SOC_MODE_RELAX` 下累计静置 tick。
2. 电压稳定条件要求 `Vmin/Vmax` 相对静置参考都在 `30mV` 内。
3. 稳定达到 `300s` 且短静置计数达到 `600s` 后，才可能锁存 OCV 下修目标。
4. 锁存目标不等于立刻显示跳变。
5. `soc_apply_long_rest_down_step()` 还要求 `rest_soc_ticks >= 1800s`，并且再累计 `1800s` 才按 `1%` 走一步。

源码证据：`SocEnhance.c:1353-1390`、`SocEnhance.c:1393-1459`。

因此，在连续普通运行、无 RTC 快进的 RELAX 场景下：

- 静置约 10 分钟可能锁存 deferred target。
- 真实下修通常要到约 60 分钟量级才出现首次 `1%` 长静置下修。
- 之后按约 `30min/1%` 消化目标。

如果现场看到远快于这个节奏的下修，优先排查 tail 和显示层。

## 4. low tail / mid tail 是最可疑来源

### 4.1 当前 tail 不要求放电

`soc_low_tail_config()` 的阻塞条件是：

- 充电模式不执行。
- 电压无效不执行。
- sag hold 阻塞时不执行。

它没有要求 `mode == SOC_MODE_DSG`。因此 `SOC_MODE_RELAX` 也可以进入 low tail。

源码证据：`SocEnhance.c:1235-1245`。

`soc_mid_tail_config()` 也只排除充电、电压无效、压差过大、sag hold、Vmin 过低等条件，同样没有要求放电。

源码证据：`SocEnhance.c:1248-1268`。

这意味着：没有放电时，只要单体最低电压落到对应区间，tail 仍可能下修 SOC。

### 4.2 low tail 的速度可能非常快

low tail 表的注释明确：`ticks` 是 200ms SOC tick，每达到一次 tick 就最多下修 `1%`。

源码证据：`SocEnhance.c:179-189`、`SocEnhance.c:1196-1208`。

`RELAX` 档位下典型速度：

| Vmin 相对 V0 | 目标 SOC | tick | 折算时间 |
|---|---:|---:|---:|
| V0 - 50mV | 0% | 1 | 0.2s / 1% |
| V0 + 50mV | 4% | 20 | 4s / 1% |
| V0 + 200mV | 12% | 60 | 12s / 1% |
| V0 + 400mV | 18% | 120 | 24s / 1% |

这条链路足以解释“无放电但静置 SOC 很快往下掉”的感受，尤其是电池本来就接近低端、电压回弹不足或 V0 配置偏高时。

### 4.3 mid tail 也会在 RELAX 下慢速下修

mid tail 表用于限制 V0 上方区间的 SOC 虚高。

源码证据：`SocEnhance.c:191-197`。

`RELAX` 档位下典型速度：

| Vmin 相对 V0 | 目标 SOC | tick | 折算时间 |
|---|---:|---:|---:|
| V0 + 500mV | 25% | 450 | 90s / 1% |
| V0 + 600mV | 35% | 600 | 120s / 1% |
| V0 + 650mV | 45% | 750 | 150s / 1% |
| V0 + 700mV | 55% | 900 | 180s / 1% |

如果现场电压在 V0+500mV 到 V0+700mV 附近，且当前内部 SOC 高于对应目标，也可能在“无放电”情况下缓慢往目标靠拢。

## 5. 显示层会放大用户感知

对外发布的是显示 SOC：

- `SOC_Enhance_Element.u8_SOC = s_soc.display_soc`。
- `SOC_PublishReportData()` 后，CAN、Modbus、LedBar 等看到的是显示 SOC。

源码证据：`SocEnhance.c:1533-1615`。

显示追赶策略：

| 场景 | 当前配置 | 用户感知 |
|---|---:|---|
| 普通下降 | `5s/1%` | 较平滑 |
| 低压下降 | `1s/1%` | 明显快 |
| 低于 V0 - 50mV | `1 tick/1%`，约 `200ms/1%` | 很快 |

源码证据：`SocEnhance.c:1546-1594`、`Project_Config.h:185-198`。

因此，现场看到的“快降”可能是两个过程叠加：

1. tail 先把内部 `s_soc.soc` 按 1% 步进往低目标拉。
2. display layer 又在低压区按 `1s/1%` 或 `200ms/1%` 追赶。

调试时必须同时看：

- 内部真实值：`g_dbg_soc_watch.u8InternalSoc`。
- 对外显示值：`g_dbg_soc_watch.u8DisplaySoc`。

只看上位机/数码管的 `u16Soc`，容易把显示追赶误判成算法一次性大跳。

## 6. RTC 休眠补偿为什么像“唤醒后立刻变了”

HICCUP STOP 休眠时，`rtc_sleep_run_hiccup_cycle()` 每次 RTC 唤醒会：

1. 读取本次 RTC 周期秒数。
2. 累加 `s_u32RtcSleepElapsedSeconds`。
3. 调用 `RtcSleep_PortApplySocRtcRest(s_u32RtcSleepElapsedSeconds)`。

源码证据：`rtc_sleep.c:247-268`。

`RtcSleep_PortApplySocRtcRest()` 再调用：

```c
SOC_ApplyRtcRelaxationCompensation(rest_seconds,
                                   g_stCellInfoReport.u16VCellMin,
                                   g_stCellInfoReport.u16VCellMax);
```

源码证据：`rtc_sleep_port.c:107-112`。

RTC 默认 wake period 是 `10s`，看门狗安全限制下最大也为 `10s`。

源码证据：`RTC.c:369-429`。

所以 RTC 场景下，SOC 补偿不是用户最终唤醒后才开始计算，而是在周期唤醒中已经按休眠秒数推进。用户按键或外部事件唤醒时，看到的可能已经是休眠期间补偿后的结果。

## 7. 现场排查清单

复现“无放电静置快降”时，优先 watch 这些字段：

| 字段 | 重点判断 |
|---|---|
| `g_dbg_soc_watch.u8LastCalibSource` | 如果是 `EMPTY_TAIL` / `MID_TAIL`，说明不是普通静置 OCV；如果是 `LONG_REST_DOWN` / `RTC_REST`，才是静置/RTC 链路 |
| `g_dbg_soc_watch.u8LastBlockReason` | 是否被 `LOW_TAIL`、`SAG_HOLD`、`REST_UNSTABLE` 阻塞或重置 |
| `g_dbg_soc_watch.u8InternalSoc` | 内部真实 SOC 是否正在下降 |
| `g_dbg_soc_watch.u8DisplaySoc` | 对外显示 SOC 是否在追赶内部值 |
| `g_dbg_soc_watch.u32RestTicks` | 普通静置是否已达到 1800s 量级 |
| `g_dbg_soc_watch.u32StableRestTicks` | 电压是否稳定 |
| `g_dbg_soc_watch.u32LongRestDownTicks` | 长静置下修计数是否在推进 |
| `g_stCellInfoReport.u16VCellMin` | 是否接近 V0、V0+400mV、V0+500mV 到 V0+700mV |
| `SOC_Enhance_Element.u16_SOC_0_Vol` | V0 配置是否偏高，导致 tail 过早生效 |
| `SOC_Enhance_Element.u16_Idsg/u16_Ichg` | 是否低于约 0.2A，确认当前为 `RELAX` |

调试判断顺序：

1. 先看 `u8LastCalibSource`。
2. 如果是 `EMPTY_TAIL` / `MID_TAIL`，再看 `VCellMin` 与 V0 的关系。
3. 如果是 `RTC_REST`，确认是否刚经历 HICCUP STOP 周期休眠。
4. 如果是 `LONG_REST_DOWN`，确认 `RestTicks` 是否已经达到约 1800s 且 `LongRestDownTicks` 达到约 1800s。
5. 如果内部 SOC 没变但显示 SOC 在变，问题在显示平滑/追赶体验，不在核心估算。

## 8. 是否需要优化

可以继续优化，但要区分两类目标。

### 8.1 只优化写法，不改功能

这类可以继续做，风险较低：

- 把 tail、rest、display、rtc compensation 的内部状态组织成更清晰的局部结构或小型子状态结构。
- 给 `SOC_IntEnhance_Ctrl()` 增加更直观的阶段划分，减少读者在多个 flag 之间来回跳。
- 把 debug watch 的来源、阻塞原因和显示/内部 SOC 边界写成统一诊断入口。
- 保持函数调用顺序、阈值、表、时间和协议字段完全不变。

### 8.2 优化无放电快降体验

这类会改变功能，需要单独确认：

| 方向 | 行为变化 | 风险 |
|---|---|---|
| RELAX 下禁用 mid tail | 无放电静置不会被中段尾端拉低 | 可能让高估 SOC 在静置时保留更久 |
| RELAX 下 low tail 只保留 V0 附近强安全区 | 低端保护仍保留，但 V0+较高区间不再快降 | 需要确认低端显示 0 的余量 |
| RELAX tail 速度乘慢，例如 5x 或 10x | 不改变目标，只改变用户感知速度 | 低端虚高收敛变慢 |
| tail 只在 `SOC_MODE_DSG` 生效 | 最直观地满足“无放电不快降” | 功能变化最大，需台架验证 |

建议先做无功能变化的可读性优化；等 Keil watch 或上板复现确认 `u8LastCalibSource` 确实是 `EMPTY_TAIL` / `MID_TAIL` 后，再决定是否调整 RELAX tail 策略。

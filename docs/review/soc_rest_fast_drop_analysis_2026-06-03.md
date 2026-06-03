# SOC 无放电静置快降分析

文档状态：已按源码验证
源码验证日期：2026-06-03
当前权威入口：`docs/design/soc_design.md`
主要参考源码：`SocEnhance.c`、`SocEnhance.h`、`rtc_sleep.c`、`rtc_sleep_port.c`、`conf/Project_Config.h`

## 1. 结论

如果现场看到“没有放电但 SOC 很快下降”，当前最可疑来源仍是 tail 和显示追赶，不是普通静置 OCV。

优先级如下：

| 优先级 | 可能来源 | 当前判断 |
|---|---|---|
| 1 | `RELAX` 下 low-tail / mid-tail 生效 | 最可疑。当前两类 tail 都不要求 `DSG`，只排除充电、电压无效、sag hold 等条件 |
| 2 | 当前 tail 测试表速度很快 | 活动表 tick 全部为 `DELAY_SOC_TEST = 5`，约 `1s/1%` |
| 3 | 低压显示追赶 | 内部 SOC 已下降后，`display_soc` 在低压区可按 `1s/1%` 或 `200ms/1%` 追赶 |
| 4 | 正常运行板载自耗 | 当前 `30mA` 已计入普通 RELAX 积分，但对 27Ah 电池约 9 小时才 1%，不是快降主因 |
| 5 | 普通静置 OCV | 连续普通 RELAX 下首次长静置实际下修通常约 60 分钟量级 |
| 6 | RTC STOP 补偿 | HICCUP STOP 周期内会提前推进静置 OCV；当前不额外扣 RTC 自耗 |

## 2. 自耗确认

当前正常运行链路已经计算自耗：

- `soc_integrate_current_ma(SOC_MODE_RELAX)` 返回 `-SOC_BOARD_SELF_CONSUMPTION_MA`。
- `soc_integrate_mode_from_current()` 将负电流转成 `SOC_MODE_DSG` 积分。
- 默认 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA = 30`。

估算：

```text
1% SOC 对应容量 = CapacityAh / 100
30mA 自耗导致 1% 变化时间 = (CapacityAh / 100) / 0.03A
```

| 额定容量 | 30mA 自耗到 1% 的时间 |
|---|---:|
| 27Ah | 约 9 小时 |
| 10.4Ah | 约 3.5 小时 |

因此，秒级、几十秒、几分钟的 SOC 下降，不应先归因于自耗。

## 3. RTC 自耗确认

当前 `SOC_ApplyRtcRelaxationCompensation()` 调用的 `soc_apply_rtc_rest_ocv()` 不再额外扣 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA`。

RTC STOP 补偿当前只做：

1. 按新增休眠秒数推进静置计数。
2. 电压稳定后锁存 OCV 下修目标。
3. 长静置满足后最多按 1% 步进下修。
4. 发生 SOC 变化时保存 snapshot 并发布。

原因：RTC STOP 下 MCU/RTC 自耗很低，本轮按需求去掉秒级自耗扣减，避免和正常运行板载自耗口径混在一起。

## 4. Low-Tail 快降来源

`soc_low_tail_config()` 当前条件：

- 非充电模式。
- 电压有效。
- sag hold 不阻塞。
- `VCellMin <= V0 + PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV`，当前 offset 为 `400mV`。

它没有要求 `SOC_MODE_DSG`，所以 `RELAX` 也可以进入 low-tail。

当前活动 low-tail 表使用 `DELAY_SOC_TEST = 5`，即所有档位约 `1s/1%`。这能解释低端附近“无放电快降”的大部分现象。

## 5. Mid-Tail 快降来源

`soc_mid_tail_config()` 当前也允许 `RELAX` 生效。它排除：

- 充电模式。
- 电压无效。
- 压差超过 `SOC_MID_MAX_DELTA_MV = 200mV`。
- sag hold 阻塞。
- `Vmin <= V0 + 400mV` 的 low-tail 区。

当前活动 mid-tail 表：

| offset mV | RELAX target | light target | mid target | heavy target | tick |
|---:|---:|---:|---:|---:|---:|
| 450 | 25 | 32 | 42 | disabled | 5 |
| 500 | 35 | 42 | 50 | disabled | 5 |
| 550 | 45 | 50 | 58 | disabled | 5 |
| 600 | 50 | 55 | disabled | disabled | 5 |

这表示 V0 上方 `450..600mV` 区间，如果内部 SOC 高于目标，即使无放电也可能按约 `1s/1%` 往目标靠拢。当前用户正在测试 tail，因此本文只记录事实，不建议在本轮改表。

## 6. 显示追赶会放大感知

对外 SOC 是 `display_soc`，不是内部 `s_soc.soc`。

显示追赶速度：

| 场景 | 当前速度 |
|---|---:|
| 普通显示下降 | `5s/1%` |
| 低压下降 | `1s/1%` |
| 低于 `V0 - 50mV` | `200ms/1%` |

如果 tail 先把内部 SOC 拉低，显示层又在低压区快速追赶，用户会感觉“突然掉很多”。

## 7. 普通静置 OCV 为什么不是快降主因

当前配置：

| 配置 | 当前值 |
|---|---:|
| `PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS` | 300s |
| `PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS` | 600s |
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | 1800s |
| `PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS` | 1800s |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | 1% |

普通 RELAX 场景下，稳定约 10 分钟后可能锁存目标，但真实长静置下修通常要到约 60 分钟量级才首次 1%。这个速度明显慢于当前活动 tail 表。

## 8. 排查清单

优先 watch：

| 字段 | 判断 |
|---|---|
| `g_dbg_soc_watch.u8LastCalibSource` | 是否为 `EMPTY_TAIL`、`MID_TAIL`、`LONG_REST_DOWN`、`RTC_REST`、`BOARD_SELF_CONSUMPTION` |
| `g_dbg_soc_watch.u8LowTailActive/u8MidTailActive` | 当前是否 tail 生效 |
| `g_dbg_soc_watch.u16EmptyTailTarget/u16MidTailTarget` | 当前 tail 目标 |
| `g_dbg_soc_watch.u16EmptyTailTicks/u16MidTailTicks` | 当前 tail 速度 |
| `g_dbg_soc_watch.u8InternalSoc/u8DisplaySoc` | 区分内部算法下降和显示追赶 |
| `g_dbg_soc_watch.u32RestTicks/u32LongRestDownTicks` | 是否进入普通静置 OCV 慢路径 |
| `SOC_Enhance_Element.u16_VCellMin` | 与 `u16_SOC_0_Vol` 的差值 |
| `SOC_Enhance_Element.u16_Ichg/u16_Idsg` | 是否确实处于 RELAX |

已删除字段：

- 不再看 `u8LastBlockReason`，该字段和 `SOC_WATCH_BLOCK_REASON` 已删除。

判断顺序：

1. 先看 `u8LastCalibSource` 和 tail active。
2. 如果是 tail，核 `VCellMin - V0` 是否落在 low/mid 表区间。
3. 如果内部 SOC 没变但显示 SOC 变，问题在显示追赶。
4. 如果是 `RTC_REST`，确认是否刚经历 HICCUP STOP。
5. 如果是 `LONG_REST_DOWN`，确认静置计数是否已达到 1800s 以上。

## 9. 后续是否调整

以下都属于功能体验变更，不能混入“只改写法”的重构：

| 方向 | 行为变化 | 风险 |
|---|---|---|
| RELAX 下禁用 mid-tail | 静置时不再被中段表下修 | 可能保留高估 SOC 更久 |
| RELAX 下 low-tail 只保留 V0 附近强安全区 | 减轻 V0+较高区间快降 | 低端虚高收敛变慢 |
| 放慢 tail tick | 目标不变，只降低用户感知速度 | 低端虚高收敛变慢 |
| tail 仅 `DSG` 生效 | 无放电不再 tail 下修 | 行为变化最大，需台架验证 |

当前建议：先保留两个 tail 表和活动测试值，用上板数据确认快降来源，再决定是否调整 RELAX tail 策略。

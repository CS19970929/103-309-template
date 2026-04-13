# RTC 低功耗修复说明

## 目标

修复工程在进入休眠前反复执行 `Init_RTC()` 时，可能卡死或重复重置备份域的问题。

## 本次调整

### 1. `Init_RTC()` 改为幂等初始化

- 先开启备份域访问。
- 先读取备份寄存器判断是否已经初始化。
- 仅在首次初始化时执行 `BKP_DeInit()`、LSE 使能和 RTC 重新分频。
- 已有备份域时只做 RTC 时钟恢复和同步，不再重置整个备份域。

### 2. 增加 LSE/LSI 兜底

- LSE 启动超时后自动切换到 LSI。
- 避免因为外部 32.768 kHz 晶振未起振而无限等待。

### 3. 修正闹钟中断使用方式

- 将 `RTC_FLAG_ALR` 的误用修正为 `RTC_IT_ALR`。
- 修正 `RTCAlarm_IRQHandler()`，确保真正清除闹钟中断标志。
- 休眠重新配置闹钟前，先清 EXTI17 和 RTC 闹钟标志，避免旧事件残留。

### 4. RTC 唤醒周期改为配置驱动

- `RTC_WKTimeConfig()` 不再写死 3 秒。
- 统一读取 `g_tParam.other.u16Sleep_RTC_WakeUpTime`，单位为分钟。
- 当配置值为 0 时，保留一个保守默认值 3 分钟，避免误配置导致极短周期唤醒。

### 5. `rtc_sleep` 流程收敛

- 将 RTC 进入前的准备动作收敛为 `rtc_sleep_prepare_rtc()`。
- 将唤醒后的退出动作收敛为统一的清理路径。
- `sys_time.rtc_sleep_cnt` 仍然表示“RTC 唤醒轮次”，累计总睡眠时长时按“轮次 × 实际唤醒周期”换算，而不是固定 5 秒。
- `before_wakeup()` 由原来的硬编码秒数改为按实际 RTC 周期累计，避免配置变化后统计失真。

### 6. 增加 RTC 睡眠状态日志

- 新增 `rtc_sleep_dump_state()`，在进入、唤醒和退出时打印当前状态。
- 日志会包含 `is_rtc_wakekup`、`sys_time.rtc_sleep_cnt`、`RTC_FLAG_ALR`、`EXTI_Line17` 和备份寄存器值。
- 这可以直接用于判断问题是在“闹钟没挂上”、“闹钟到了但 EXTI 没清掉”，还是“唤醒后上层状态机没有继续往下走”。

### 7. 去掉 `goto` 型回睡控制

- HICCUP 模式的 RTC 回睡流程改成 `rtc_sleep_run_hiccup_cycle()`。
- 外层通过 `while (...)` 控制是否继续回到 RTC 睡眠，不再依赖 `goto rtcsleep`。
- 这样更容易看清“一次 RTC 周期做了什么”，也方便后续插入额外的唤醒后处理逻辑。

## 相关文件

- [`103 + 309/Project/Source/RTC.c`](103%20+%20309/Project/Source/RTC.c)
- [`103 + 309/Project/Source/rtc_sleep.c`](103%20+%20309/Project/Source/rtc_sleep.c)

## 使用建议

- 如果板上没有外部 32.768 kHz 晶振，优先让工程走 LSI 兜底。
- `Init_RTC()` 不要在所有运行路径里无条件重置备份域。
- 进入 Stop / Standby 前重新配置闹钟时，先清 `RTC_FLAG_ALR` 和 `EXTI_Line17`。
- `u16Sleep_RTC_WakeUpTime` 是 RTC 唤醒周期，单位是分钟，不要和“进入 RTC 休眠的等待时长”混用。
- `sys_time.rtc_sleep_cnt` 是唤醒次数，统计累计睡眠时长时要乘实际 RTC 周期。
- 如果怀疑卡在 RTC 流程，优先看 `rtc_sleep_dump_state()` 的三处日志：`enter`、`wake`、`exit`。
- 如果后续要加新的 RTC 唤醒后动作，建议放进 `rtc_sleep_run_hiccup_cycle()`，不要再把控制流拉回到 `goto`。

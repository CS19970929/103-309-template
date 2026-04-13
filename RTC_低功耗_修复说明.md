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

## 相关文件

- [`103 + 309/Project/Source/RTC.c`](103%20+%20309/Project/Source/RTC.c)
- [`103 + 309/Project/Source/rtc_sleep.c`](103%20+%20309/Project/Source/rtc_sleep.c)

## 使用建议

- 如果板上没有外部 32.768 kHz 晶振，优先让工程走 LSI 兜底。
- `Init_RTC()` 不要在所有运行路径里无条件重置备份域。
- 进入 Stop / Standby 前重新配置闹钟时，先清 `RTC_FLAG_ALR` 和 `EXTI_Line17`。

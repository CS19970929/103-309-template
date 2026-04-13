# RTC 低功耗现场调试清单

适用工程：
- `103 + 309`
- STM32F103 + 标准库
- RTC 闹钟唤醒 + Stop 模式

## 先看结论

现场排查顺序建议固定为：
1. 先确认 ST-Link 还连得上
2. 再确认 LSE / LSI 是否正常
3. 再确认备份域是否已经被正确初始化
4. 再确认 RTC 闹钟是否真的挂上了
5. 再确认 EXTI17 是否清干净
6. 最后确认 Stop 唤醒后系统时钟是否恢复

如果顺序反了，常见结果是：
- 误判为 RTC 卡死，实际是时钟源没起
- 误判为闹钟失败，实际是 EXTI17 悬挂位没清
- 误判为唤醒失败，实际是 Stop 唤醒后系统时钟没恢复

## 1. ST-Link 低功耗调试配置

### 工程内配置

当前工程已经在 `[_DEBUG_]` 路径下显式打开了低功耗调试位：
- `DBGMCU_CR_DBG_SLEEP`
- `DBGMCU_CR_DBG_STOP`
- `DBGMCU_CR_DBG_STANDBY`
- `DBGMCU_CR_DBG_IWDG_STOP`
- `DBGMCU_CR_DBG_WWDG_STOP`

对应代码在：
- [`Project/Source/System_Init.c`](E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Source/System_Init.c)

### Keil / 调试器配置

工程的 debug 配置文件已经设置了：
- `DbgMCU_CR = 0x00000007`

对应文件：
- [`Project/Users/DebugConfig/Target_1_STM32F103C8_1.0.0.dbgconf`](E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Users/DebugConfig/Target_1_STM32F103C8_1.0.0.dbgconf)

### 现场注意事项

- 调试时优先用 `Stop`，不要把 `Standby` 当成默认调试模式。
- 如果必须测 `Standby`，要接受“可能无法单步恢复”的现实。
- 不要把 `SWDIO` / `SWCLK` 复用成普通 GPIO。
- 如果板子上有 JTAG 占脚，尽量只保留 SWD。

## 2. LSE / LSI 排查

### 目标

先确认 RTC 的时钟源本身是可用的。  
如果时钟源不稳定，后面的备份域、闹钟、唤醒都没有意义。

### 排查步骤

1. 进入 `Init_RTC()` 后，确认没有长期卡在等待 LSE 的流程里。
2. 看日志或断点，确认 `RTC_ClockConfig()` 是否走到了 LSE ready。
3. 如果 LSE 超时，确认是否已经切到 LSI 兜底。
4. 如果板子根本没有 32.768 kHz 晶振，调试阶段不要强依赖 LSE。

### 观察点

- `BKP_DR1` 是否已经写入初始化标记
- `RCC_FLAG_LSERDY` 是否置位
- `RCC_FLAG_LSIRDY` 是否置位
- `RTC_GetCounter()` 是否持续递增

### 常见结论

- `LSE` 不起振但工程没有兜底：RTC 初始化会卡住
- `LSE` 起振但分频值不对：RTC 时间会跑偏
- `LSI` 作为兜底：能先保证功能跑通，再决定是否继续优化精度

## 3. 备份域排查

### 目标

确认 RTC 是否被重复初始化，或者备份域是否被意外重置。

### 排查步骤

1. 先读 `BKP_DR1`。
2. 如果值不等于工程定义的魔数，说明当前是首次初始化。
3. 如果值已经正确，说明只需要恢复 RTC 同步，不要再 `BKP_DeInit()`。

### 观察点

- `BKP_DR1` 是否保持稳定
- `Init_RTC()` 是否每次都在重置备份域
- `RTC_ClockConfig(need_init)` 是否走到了“已初始化”分支

### 常见结论

- 每次进 `Init_RTC()` 都重置备份域：RTC 状态会反复丢失
- 备份域被复位后又立刻进 Stop：闹钟配置会变得不可预测

## 4. 闹钟排查

### 目标

确认 RTC 闹钟真的被重新设置并打开。

### 排查步骤

1. 配置闹钟前先关 `RTC_IT_ALR`。
2. 清 `RTC_FLAG_ALR`。
3. 清 `EXTI_Line17`。
4. 设置新的 alarm 值。
5. 再打开 `RTC_IT_ALR`。

### 观察点

- `RTC_SetAlarm()` 传入的值是否正确
- `RTC_IT_ALR` 是否已经使能
- `RTC_FLAG_ALR` 是否在旧唤醒后遗留
- `RTCAlarm_IRQHandler()` 是否真的执行

### 常见结论

- 只设置闹钟但没开中断：不会唤醒
- 中断开了但旧标志没清：会出现立即唤醒或重复进入
- 只清 RTC 标志不清 EXTI17：会卡在中断挂起状态

## 5. EXTI17 排查

### 目标

确认 RTC Alarm 对应的 EXTI 线没有残留挂起位。

### 排查步骤

1. 进入 Stop 前，确认 `EXTI_Line17` 已经清掉。
2. 在 `RTCAlarm_IRQHandler()` 里同时清 RTC 闹钟中断和 EXTI17。
3. 唤醒后如果又马上进中断，优先怀疑 EXTI17。

### 观察点

- `EXTI_GetITStatus(EXTI_Line17)` 是否一直为 1
- `EXTI_ClearITPendingBit(EXTI_Line17)` 是否被执行

### 常见结论

- RTC 标志清了但 EXTI17 没清：中断会反复进
- EXTI17 被别的地方误触发：会误判成 RTC 唤醒

## 6. Stop 恢复排查

### 目标

确认 MCU 从 Stop 退出后，系统时钟恢复正常，程序能继续跑。

### 排查步骤

1. 进入 Stop 前先喂狗。
2. 从 `PWR_EnterSTOPMode()` 返回后，检查系统时钟是否重新配置。
3. 检查是否重新初始化了外设时钟、串口、定时器等依赖系统时钟的模块。

### 观察点

- `Sys_StopMode()` 返回后，系统主频是否恢复
- 串口是否还能正常输出
- 定时器节拍是否正常

### 常见结论

- Stop 唤醒后没重配时钟：程序看起来“活着”，实际上外设不工作
- 只恢复了时钟没恢复 IO：可能会表现为“已唤醒但外设无响应”

## 7. 推荐日志点

工程里已经增加了 `rtc_sleep_dump_state()`，建议优先看这三处日志：
- `enter`
- `wake`
- `exit`

日志字段含义：
- `wake`：是否认为自己是 RTC 唤醒
- `cnt`：当前 RTC 唤醒轮次
- `alr`：RTC 闹钟标志
- `ex17`：EXTI17 挂起状态
- `bkp`：备份寄存器值

### 典型解释

- `enter` 有日志，`wake` 没日志：大概率没真正从 Stop 唤醒
- `wake` 有日志但 `ex17` 不对：优先查 EXTI17 清理
- `exit` 频繁出现但没回到正常业务：优先查 `Init()` 和上层状态机

## 8. 快速定位表

- 现象：进 `Init_RTC()` 卡住
  - 优先看：LSE / LSI、备份域
- 现象：RTC 一直不唤醒
  - 优先看：闹钟配置、`RTC_IT_ALR`、EXTI17
- 现象：唤醒后马上又进中断
  - 优先看：`RTC_FLAG_ALR`、`EXTI_Line17`
- 现象：唤醒后串口没了
  - 优先看：Stop 后系统时钟恢复
- 现象：ST-Link 连不上
  - 优先看：是否误进 Standby、是否把 SWD 引脚复用了

## 9. 代码参考

- [`RTC.c`](E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Source/RTC.c)
- [`rtc_sleep.c`](E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Source/rtc_sleep.c)
- [`System_Init.c`](E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Source/System_Init.c)
- [`conf.c`](E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Source/conf/conf.c)


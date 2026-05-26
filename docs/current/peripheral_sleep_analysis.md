# PeripheralAgent 外设休眠现状分析

## 范围

本文件只分析当前工程中 SysTick、TIM、ADC、UART/Modbus、CAN、LED、AFE 在 RTC/Stop 低功耗前后的实际处理。分析依据全部来自当前源码，未修改源码。

当前工程是 STM32F103 标准外设库工程，低功耗主路径不是一个独立 `LP_*` 框架，而是由 `rtc_sleep.c`、`rtc_sleep_port.c`、`conf.c`、`SleepDeal.c` 和各外设模块共同完成。

## 主循环和低功耗调用链

- 运行态任务入口在 `103 + 309/Project/Source/Runtime.c`：
  - `Runtime_RunFrontTasks()` 先执行 `SysTime_LatchTaskFlags()`、`FactoryAging_Task()`、`APP_LedBar()`、`App_AFEGet()`，见 `Runtime.c:14` 到 `Runtime.c:21`。
  - `Runtime_RunIoAndPowerTasks()` 执行 `AppInit_ServiceSci()`、`App_AnlogCal()`、`App_LowPowerProcess()`、`App_Can()`，见 `Runtime.c:23` 到 `Runtime.c:29`。
  - `Runtime_RunBackgroundTasks()` 执行 Flash/日志/产品 ID 任务并喂狗，见 `Runtime.c:32` 到 `Runtime.c:41`。
- RTC 低功耗入口在 `103 + 309/Project/Source/rtc_sleep.c`：
  - `App_LowPowerProcess()` 直接调用 `rtc_sleep()`，见 `rtc_sleep.c:231` 到 `rtc_sleep.c:233`。
  - `rtc_sleep()` 只在 `RtcSleep_PortIsOneSecondTick()` 为真时做睡眠决策，见 `rtc_sleep.c:414` 到 `rtc_sleep.c:420`。
  - `RtcSleep_PortIsOneSecondTick()` 读取 `g_st_SysTimeFlag.bits.b1Sys1000msFlag`，见 `rtc_sleep_port.c:6` 到 `rtc_sleep_port.c:8`。
- HICCUP/RTC 周期 Stop 路径：
  - `rtc_sleep_run_hiccup_cycle()` 先 `rtc_sleep_prepare_rtc()`，再 `RtcSleep_PortEnterStop()`，唤醒后 `RtcSleep_PortRestoreAfterStop()`，见 `rtc_sleep.c:303` 到 `rtc_sleep.c:326`。
  - `RtcSleep_PortPrepareRtcStop()` 保存核心状态、初始化 RTC、设置 IO 和唤醒源，见 `rtc_sleep_port.c:108` 到 `rtc_sleep_port.c:116`。
  - `RtcSleep_PortEnterStop()` 喂狗后调用 `Sys_StopMode()`，见 `rtc_sleep_port.c:118` 到 `rtc_sleep_port.c:123`。
  - `RtcSleep_PortRestoreAfterStop()` 调用 `InitRunAfterStopWakeup()`，见 `rtc_sleep_port.c:131` 到 `rtc_sleep_port.c:134`。

## STOP 前处理

当前 STOP 前处理分散在三个层次：

1. `LowPowerSleep_SaveCoreState()`：
   - 调用 `Can_PrepareSleep()`、`SOC_SaveSnapshotBeforeSleep()`、`FactoryAging_SaveProgressBeforeSleep()`，见 `103 + 309/Project/Source/LowPowerSleep.c:5` 到 `LowPowerSleep.c:10`。

2. `IOstatus_RTCMode()`：
   - 先调用 `Conf_PrepareStopEntry()`，见 `103 + 309/Project/Source/conf/conf.c:297` 到 `conf.c:302`。
   - `Conf_PrepareStopEntry()` 调用 `LedBar_SetSleep(1u)` 和 `ADC_StopForLowPower()`，见 `conf.c:114` 到 `conf.c:118`。
   - 后续把大部分 GPIO 切为模拟输入，并关闭 `GPIO_DC_EN/PIN_DC_EN`，见 `conf.c:305` 到 `conf.c:323`。
   - 最后调用 `LedBar_PrepareForStop()`，见 `conf.c:321` 到 `conf.c:324`。

3. `Sys_StopMode()`：
   - 使能 PWR 时钟。
   - 使能 TIM3 时钟后关闭 TIM3、清 pending，再关闭 TIM3 外设时钟。
   - 清 EXTI/NVIC pending 后调用 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`。
   - Stop 返回后调用 `cpu_frequency_conf()`。
   - 以上见 `conf.c:374` 到 `conf.c:384`。

## STOP 后恢复

当前恢复入口是 `InitRunAfterStopWakeup()`，见 `103 + 309/Project/Source/conf/conf.c:392` 到 `conf.c:421`，实际顺序为：

1. `is_wakeup = true`。
2. `InitDelay()` 恢复 SysTick 延时参数。
3. `RTC_RestoreRunInterrupts()` 恢复运行态 RTC 中断。
4. `InitIO_rtc()` 恢复运行共享 IO。
5. `ADC_StopForLowPower()` 再清一次 ADC 相关外设，然后 `InitADC()`。
6. 可选 LED 逻辑：`APP_LedBar()`、`set_LED_state(...)`。
7. `USART_DeInit(USART1)`、`USART_DeInit(USART2)`，然后 `AppInit_InitSci()`。
8. `InitCan()`。
9. `InitTimer()` 恢复 TIM3 10ms 系统时基。
10. 设置 `sys_time.wakeup_rtc`。
11. `initAFE1_IIC()` 只恢复 AFE IIC 引脚，不做完整 AFE 参数重写。

时钟恢复在 `Sys_StopMode()` 返回后立即执行 `cpu_frequency_conf()`；该函数调用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`，见 `103 + 309/Project/Source/rtc_sleep_port.c:207` 到 `rtc_sleep_port.c:211`。

## SysTick

当前 SysTick 不是主系统任务 tick，只作为阻塞延时计数器使用：

- `InitDelay()` 设置 SysTick 使用 HCLK/8，并按 `SystemCoreClock` 计算 `fac_us/fac_ms`，见 `103 + 309/Project/Source/System_Init.c:132` 到 `System_Init.c:137`。
- `__delay_us()` 和 `__delay_ms()` 都是临时启动 SysTick，延时结束后关闭 `SysTick_CTRL_ENABLE_Msk` 并清 `VAL`，见 `System_Init.c:139` 到 `System_Init.c:172`。
- `__delay_ms()` 循环内喂狗，见 `System_Init.c:163` 到 `System_Init.c:170`。

结论：

- STOP 前不需要专门关闭一个长期运行的 SysTick tick，因为当前 SysTick 不是持续周期中断。
- STOP 唤醒后必须在恢复系统时钟后重新执行 `InitDelay()`，当前 `cpu_frequency_conf()` 和 `InitRunAfterStopWakeup()` 都调用了 `InitDelay()`。
- 后续低功耗框架应保留这个顺序：先恢复 `SystemCoreClock`，再恢复 `InitDelay()`，否则 AFE/I2C/CAN RTC 服务中的延时会失准。

## TIM

### TIM3 系统时基

- `InitTimer()` 配置 TIM3，初始化 10ms 周期中断，并在末尾 `TIM_Cmd(TIM3, ENABLE)`，见 `103 + 309/Project/Source/System_Init.c:100` 到 `System_Init.c:127`。
- `TIM3_IRQHandler()` 清更新中断后调用 `SysTime_Post10msTick()`，见 `System_Init.c:293` 到 `System_Init.c:299`。
- `SysTime_Post10msTick()` 递增 10ms tick，并派生 50ms、100ms、200ms、500ms、1000ms 标志，见 `System_Init.c:258` 到 `System_Init.c:287`。
- STOP 前 `Sys_StopMode()` 明确关闭 TIM3 和 TIM3 时钟，见 `103 + 309/Project/Source/conf/conf.c:376` 到 `conf.c:380`。
- STOP 后 `InitRunAfterStopWakeup()` 调用 `InitTimer()`，见 `conf.c:412` 到 `conf.c:414`。

结论：

- TIM3 属于 STOP 前必须关闭、唤醒后必须重新初始化的外设。
- 当前 `InitTimer()` 会调用 `SysTime_ResetCounters()`，因此唤醒后不会补跑休眠期间的 10ms/200ms 任务；这符合“先稳定唤醒，不做复杂补偿”的第一阶段策略。
- 后续如需要 SOC 静置时间补偿，应继续使用 RTC 休眠秒数，不应让 TIM3 catch-up 大量补跑。

### TIM2 ADC 触发

- ADC 用 TIM2 CC2 触发，`InitADC_TIMER()` 使能 TIM2 并配置输出比较，见 `103 + 309/Project/Source/ADC.c:149` 到 `ADC.c:181`。
- `InitADC_ADC1()` 配置 `ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2`，见 `ADC.c:215` 到 `ADC.c:260`。
- `ADC_StopForLowPower()` 关闭 TIM2、ADC 外部触发、ADC DMA、ADC1、DMA1，然后关闭 ADC1/TIM2 时钟，见 `ADC.c:268` 到 `ADC.c:283`。

结论：

- TIM2 和 ADC/DMA 是一组外设，应统一作为“采样链路”关闭和恢复。
- 当前 STOP 前已经有关闭动作，STOP 后 `InitADC()` 重新配置 GPIO、TIM2、DMA、ADC1，见 `ADC.c:463` 到 `ADC.c:484`。

### TIM4 LedBar 扫描

- `LedBar_StartScanTimer()` 初始化并启动 TIM4，见 `103 + 309/Project/Source/LedBar.c:376` 到 `LedBar.c:390`。
- `LedBar_StopScanTimer()` 关闭 TIM4、TIM4 更新中断、NVIC，并关闭 TIM4 时钟，见 `LedBar.c:392` 到 `LedBar.c:401`。
- `TIM4_IRQHandler()` 调用 `LedBar_Scan1ms()`，见 `LedBar.c:1015` 到 `LedBar.c:1021`。

结论：

- TIM4 属于 LED 显示扫描专用定时器。显示未请求或进入休眠时应关闭，当前 LedBar 模块已经封装关闭路径。

## ADC

当前 ADC 采样链路包括：

- `InitADC_GPIO()` 配置 ADC 输入和 TIM2 CH2 相关 GPIO，见 `103 + 309/Project/Source/ADC.c:127` 到 `ADC.c:147`。
- `InitADC_TIMER()` 配置 TIM2 触发，见 `ADC.c:149` 到 `ADC.c:181`。
- `InitADC_DMA()` 配置 DMA1 Channel1 把 ADC1->DR 读到 `g_u16ADCValFilter[]`，见 `ADC.c:90` 到 `ADC.c:122`。
- `InitADC_ADC1()` 配置 ADC1 扫描、外部触发、DMA、校准，见 `ADC.c:215` 到 `ADC.c:260`。
- `InitADC()` 会清 ADC 滤波缓存、Type-C 电流缓存、Vbc 缓存，并重新初始化 GPIO/TIM2/DMA/ADC1，见 `ADC.c:463` 到 `ADC.c:484`。
- `App_AnlogCal()` 基于 `SysTime_Get10msTickCount()` 处理 ADC 软件滤波，最多补跑 `ADC_ANALOG_CAL_MAX_CATCHUP_TICKS = 10` 个 10ms tick，见 `ADC.c:488` 到 `ADC.c:512`。

结论：

- ADC/TIM2/DMA 在 STOP 前必须关闭；当前 `ADC_StopForLowPower()` 已满足。
- STOP 后必须重新 `InitADC()`，当前 `InitRunAfterStopWakeup()` 已满足。
- 注意：`InitADC()` 会清空采样滤波缓存。第一版可以接受，因为目标是稳定唤醒；后续若追求显示/通信数据连续性，需要给上层增加“唤醒后采样稳定窗口”，避免刚唤醒就上报零值或未稳定值。

## UART / Modbus / RS485

当前串口配置：

- `AppInit_InitSci()` 是 `InitUSART_CommonUpper()` 的宏封装，见 `103 + 309/Project/Source/AppInit.h:6`。
- `AppInit_ServiceSci()` 是 `App_CommonUpper()` 的宏封装，见 `AppInit.h:7`。
- `Project_Config.h` 中 `PROJECT_CFG_SCI1_ROLE = 1`，SCI1 用作通用上位机协议；`PROJECT_CFG_SCI2_ROLE = 0`、`PROJECT_CFG_SCI3_ROLE = 0`，见 `103 + 309/Project/Source/conf/Project_Config.h:171` 到 `Project_Config.h:193`。
- `Sci_InitCommonPort()` 配置 USART 波特率 19200、8N1、RX/TX、RXNE/IDLE 中断，并打开 USART，见 `103 + 309/Project/Source/Sci_Upper.c:1597` 到 `Sci_Upper.c:1677`。
- `InitUSART_CommonUpper()` 根据宏初始化 SCI1/SCI2/SCI3，见 `Sci_Upper.c:2230` 到 `Sci_Upper.c:2243`。
- `App_CommonUpper()` 服务各 SCI 端口协议状态机，见 `Sci_Upper.c:2245` 到 `Sci_Upper.c:2254`。

当前通信活跃检测：

- `Sci_IsAnyPortBusy()` 聚合各串口的 `Sci_PortIsBusy()`，见 `Sci_Upper.c:1679` 到 `Sci_Upper.c:1691`。
- `Sci_PortIsBusy()` 判断 `u8FramePending`、`u16TxLength` 和协议层 busy，见 `Sci_Upper.c:1577` 到 `Sci_Upper.c:1595`。
- USART RXNE 中断里 `RTC_ExtComCnt++`，见 `Sci_Upper.c:1474` 到 `Sci_Upper.c:1480`。
- 低功耗当前只比较 `last_ext_comm_count != RtcSleep_PortGetExternalCommCounter()` 来延后 RTC 休眠，见 `103 + 309/Project/Source/rtc_sleep.c:201` 到 `rtc_sleep.c:207`。
- `RtcSleep_PortGetExternalCommCounter()` 返回 `RTC_ExtComCnt`，见 `103 + 309/Project/Source/rtc_sleep_port.c:56` 到 `rtc_sleep_port.c:58`。

STOP 前后处理：

- STOP 前没有看到统一的 `Sci_PrepareSleep()` 或 `USART_DeInit()`；串口最终会受 `IOstatus_RTCMode()` 把 GPIO 切模拟输入和 STOP 停 APB 时钟影响。
- STOP 后 `InitRunAfterStopWakeup()` 对 `USART1`、`USART2` 调用 `USART_DeInit()`，再 `AppInit_InitSci()` 重建串口，见 `103 + 309/Project/Source/conf/conf.c:409` 到 `conf.c:412`。
- 当前 `PROJECT_CFG_UART1_WAKEUP_ENABLE = 1`，`UART1_WAKEUP_ENABLE` 会让 `InitWakeUp_NormalMode()` 配置 PB7/EXTI7 上升沿唤醒，见 `Project_Config.h:92` 到 `Project_Config.h:98`、`conf.c:220` 到 `conf.c:228`。
- `InitWakeUp_RTCMode()` 调用 `InitWakeUp_NormalMode()` 后再配置 RTC 唤醒，见 `conf.c:268` 到 `conf.c:272`。

结论：

- 通信活跃时应该禁止进入 STOP。依据是当前串口发送/接收状态机依赖 USART 中断和 `App_CommonUpper()` 推进，若在帧处理中或 TX 未完成时进入 STOP，会丢帧或截断回复。
- 当前已有可用 busy 判断 `Sci_IsAnyPortBusy()`，但低功耗准入没有使用它；仅靠 `RTC_ExtComCnt` 只能捕获“最近 1 秒有 RX 字节变化”，不能覆盖正在发送、帧已收完待处理、协议层 busy 等状态。
- 第一版可复用 RTC 周期唤醒，但不建议继续依赖 USART Stop 唤醒作为主策略；当前代码实际上会配置 UART1 EXTI 唤醒，后续应在框架里明确“第一版禁用通信唤醒，只把通信活跃作为禁止睡眠原因”，避免和现有 Modbus/CAN 协议时序纠缠。

## CAN

当前 CAN 配置：

- `InitCan()` 清状态、清队列、初始化 GPIO/NVIC/CAN1/Filter，见 `103 + 309/Project/Source/Can_HDX.c:847` 到 `Can_HDX.c:863`。
- `InitCan_GPIO()` 打开 `GPIO_CMNT_EN/PIN_CMNT_EN` 给 CAN 通信电源或收发器使能，并配置 PA11/PA12 CAN 引脚，见 `Can_HDX.c:774` 到 `Can_HDX.c:796`；`GPIO_CMNT_EN` 定义见 `103 + 309/Project/Source/conf/conf_gpio.h:90` 到 `conf_gpio.h:91`。
- `InitCan_CAN1()` 使能 CAN1、`CAN_DeInit()`、配置正常模式并打开 FIFO0 消息中断，见 `Can_HDX.c:825` 到 `Can_HDX.c:844`。
- `USB_LP_CAN1_RX0_IRQHandler()` 从 FIFO0 取帧并交给 `feidao_can_handle_rx_msg()`，见 `Can_HDX.c:949` 到 `Can_HDX.c:958`。

当前 CAN 活跃/忙判断：

- `Can_IsBusy()` 判断 TX 队列、TX mailbox、读块流和 CAN TSR 空闲位，见 `Can_HDX.c:865` 到 `Can_HDX.c:880`。
- `Can_IsBusActive()` 返回 `s_u8BusActive`，见 `Can_HDX.c:896` 到 `Can_HDX.c:899`。
- `s_u8BusActive` 在收帧时置 1，见 `Can_HDX.c:758` 到 `Can_HDX.c:770`；发送成功时也置 1，见 `Can_HDX.c:289` 到 `Can_HDX.c:296`。
- `InitCan()` 会把 `s_u8BusActive = 0U`，见 `Can_HDX.c:847` 到 `Can_HDX.c:853`。

STOP 前后处理：

- `Can_PrepareSleep()` 取消当前 mailbox、清 TX 队列、停止读块流，并把 `GPIO_CMNT_EN` 写成 `FEIDAO_CAN_POWER_OFF_LEVEL`，见 `Can_HDX.c:882` 到 `Can_HDX.c:893`。
- `LowPowerSleep_SaveCoreState()` 在 STOP 前调用 `Can_PrepareSleep()`，见 `103 + 309/Project/Source/LowPowerSleep.c:5` 到 `LowPowerSleep.c:10`。
- `RtcSleep_PortPrepareRtcStop()` 也会通过 `LowPowerSleep_SaveCoreState()` 间接调用 `Can_PrepareSleep()`，见 `103 + 309/Project/Source/rtc_sleep_port.c:108` 到 `rtc_sleep_port.c:116`。
- STOP 后 `InitRunAfterStopWakeup()` 调用 `InitCan()`，见 `103 + 309/Project/Source/conf/conf.c:412` 到 `conf.c:414`。
- RTC 周期醒来后，如果没有异常，`rtc_sleep_run_hiccup_cycle()` 会调用 `RtcSleep_PortRunCanRtcWakeService()`；该函数最终调用 `Can_RtcWakeService()`，见 `rtc_sleep.c:323` 到 `rtc_sleep.c:326`、`rtc_sleep_port.c:161` 到 `rtc_sleep_port.c:163`。
- `Can_RtcWakeService()` 打开 CAN 供电、排队周期帧，并在最多 `FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS = 150` 个 10ms 窗口内服务 CAN，见 `Can_HDX.c:906` 到 `Can_HDX.c:933`；RTC 周期为 `FEIDAO_CAN_RTC_PERIOD_SECONDS = 1`，见 `Can_HDX.c:20` 到 `Can_HDX.c:24`。

结论：

- CAN 在 STOP 前应关闭收发器/通信电源并清掉待发送队列；当前 `Can_PrepareSleep()` 已做。
- 通信活跃时应该禁止进入 STOP，但当前低功耗准入没有使用 `Can_IsBusy()` 或 CAN 活跃窗口。
- `Can_IsBusActive()` 更像“本轮运行是否曾经检测到 CAN 交互/ACK”的粘性状态，不适合作为“当前是否活跃”的唯一阻塞条件；它在 `InitCan()` 后清零，但一旦收发成功就保持 1。
- 后续应增加 CAN 最近活动时间或静默窗口，准入使用 `Can_IsBusy() == 0` 加“最近 N 秒无 RX/TX 活动”，而不是直接用粘性的 `Can_IsBusActive()` 永久阻止睡眠。

## LED / LedBar

当前 LedBar 配置：

- `PROJECT_CFG_LEDBAR_SLEEP_ENABLE = 1`，见 `103 + 309/Project/Source/conf/Project_Config.h:354` 到 `Project_Config.h:356`。
- `LedBar_SetSleep()` 设置 sleep 状态并刷新输出，见 `103 + 309/Project/Source/LedBar.c:817` 到 `LedBar.c:833`。
- `LedBar_PrepareForStop()` 在 sleep 使能时设置 sleep、blank，刷新输出并调用 `LedBar_GpioPrepareForStop()`，见 `LedBar.c:982` 到 `LedBar.c:990`。
- `LedBar_GpioPrepareForStop()` 使能 GPIOA/GPIOB 后把 LED 相关引脚输出低，见 `LedBar.c:301` 到 `LedBar.c:306`。
- `APP_LedBar()` 在低功耗待睡并且 MCU_WK 不活跃时保存 SOC 并 `LedBar_SetSleep(1u)`，见 `LedBar.c:1039` 到 `LedBar.c:1044`。
- `LedBar_IsDisplayRequested()` 以 SOC 显示窗口和按键状态判断是否需要显示，见 `LedBar.c:682` 到 `LedBar.c:693`。

结论：

- LED/TIM4 是 STOP 前必须关闭或钳到低功耗状态的模块，当前已有 `LedBar_SetSleep(1u)` 与 `LedBar_PrepareForStop()` 双层处理。
- 现有低功耗准入没有显式 `LED_ACTIVE` 阻塞位。如果用户按键触发 SOC 显示窗口，`APP_LedBar()` 会处理显示/睡眠，但低功耗框架后续仍应把显示窗口作为可见行为约束，避免“刚显示就进入 STOP”。

## AFE / SH367309 / IIC

当前 AFE 初始化和恢复：

- 启动阶段 `AppInit_Boot()` 调用 `InitAFE1()`，见 `103 + 309/Project/Source/AppInit.c:29` 到 `AppInit.c:32`。
- `InitAFE1()` 先 `initAFE1_IIC()`，再 `close_ctlc()`、可能做电流零点校准，后续检查 AFE ready 并更新配置，见 `103 + 309/Project/Source/I2C_AFE1.c:688` 到 `I2C_AFE1.c:704`。
- `initAFE1_IIC()` 只配置 PB8/PB9 为输出高，用于软件 IIC 线恢复，见 `I2C_AFE1.c:670` 到 `I2C_AFE1.c:678`。
- `InitRunAfterStopWakeup()` 唤醒后只调用 `initAFE1_IIC()`，没有完整 `InitAFE1()`，见 `103 + 309/Project/Source/conf/conf.c:420` 到 `conf.c:421`。

当前 AFE 休眠和业务保护：

- `AFE_Sleep()` 设置 `SH367309_Reg_Store.REG_MTP_CONF.bits.SLEEP = 1` 并写 `MTP_CONF`，见 `103 + 309/Project/Source/SH367309_Func.c:65` 到 `SH367309_Func.c:69`。
- `InitAFE1_Sleep(0)` 用于睡眠前恢复 PB8/PB9，调用 `AFE_IsReady()`，但注释明确不重新初始化参数，避免复位模拟前端导致 MOS 开关反复，见 `103 + 309/Project/Source/I2C_AFE1.c:648` 到 `I2C_AFE1.c:667`。
- `SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 开启 CADC，并设置充/放 MOS 由 AFE 控制，见 `SH367309_Func.c:136` 到 `SH367309_Func.c:148`。
- 复位式睡眠 `SleepDeal_Continue()` 会 `InitAFE1_Sleep(0)`、`AFE_Sleep()` 后 `MCU_RESET()`，见 `103 + 309/Project/Source/SleepDeal.c:83` 到 `SleepDeal.c:113`。

当前 RTC 低功耗 AFE 判断：

- `RtcSleep_AfePortIsSleepBlocked()` 读 `MTP_BSTATUS1` 起 3 字节，若 `BSTATUS1/BSTATUS2` 非零、`L0V` 或 `PCHG_FET` 有效则阻塞 RTC 休眠，见 `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:11` 到 `rtc_sleep_afe_sh367309.c:28`。
- `RtcSleep_AfePortUpdateRtcData()` 调用 `UpdateVoltageFromBqMaximo()` 后刷新单体、温度、最大最小值，见 `rtc_sleep_afe_sh367309.c:30` 到 `rtc_sleep_afe_sh367309.c:41`。
- `RtcSleep_AfePortHasAfeWake()` 读 `MTP_BALANCEH` 起 5 字节，同步 MOS 状态并调用 `Fault_ChangeToMCU()`，见 `rtc_sleep_afe_sh367309.c:74` 到 `rtc_sleep_afe_sh367309.c:91`。
- `rtc_sleep.c` 把 `RtcSleep_PortIsAfeSleepBlocked()` 映射为 `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE`，见 `rtc_sleep.c:208` 到 `rtc_sleep.c:212`。

结论：

- AFE 不应在普通 Stop 唤醒后无条件完整 `InitAFE1()`，当前只恢复 IIC 引脚是合理的，因为源码注释已指出完整配置可能导致 MOS 开关反复。
- AFE 状态需要在睡前和唤醒后重新同步。当前 RTC 路径已有 `RtcSleep_AfePortIsSleepBlocked()` 和 `RtcSleep_AfePortHasAfeWake()`，但唤醒后常规恢复入口只做 `initAFE1_IIC()`，完整同步依赖 `rtc_sleep_run_hiccup_cycle()` 后续异常判断和运行态 `App_AFEGet()`。
- 后续低功耗框架应把 AFE busy/故障/EEPROM 写状态作为显式阻塞原因，而不是只放在 SH367309 专用文件里。

## GPIO 和电源控制

关键 GPIO 定义：

- `GPIO_CHG_IN/PIN_CHG_IN = PA0`，见 `103 + 309/Project/Source/conf/conf_gpio.h:30` 到 `conf_gpio.h:31`。
- `GPIO_INT_WK_CMNT/PIN_INT_WK_CMNT = PB12`，见 `conf_gpio.h:33` 到 `conf_gpio.h:34`。
- `GPIO_MCU_WK/PIN_MCU_WK = PB13`，见 `conf_gpio.h:39` 到 `conf_gpio.h:40`。
- `GPIO_SW/PIN_SW = PA9`，见 `conf_gpio.h:42` 到 `conf_gpio.h:43`。
- `GPIO_CMNT_EN/PIN_CMNT_EN = PB4`，见 `conf_gpio.h:90` 到 `conf_gpio.h:91`。

唤醒源配置：

- `InitWakeUp_Base()` 配置充电输入 PA0 下降沿、按键 PA9 下降沿，见 `103 + 309/Project/Source/conf/conf.c:192` 到 `conf.c:213`。
- `InitWakeUp_NormalMode()` 在 `UART1_WAKEUP_ENABLE` 下配置 PB7/EXTI7 上升沿，同时配置 PB12 通信唤醒、PB13 MCU_WK 唤醒，见 `conf.c:215` 到 `conf.c:266`。
- `InitWakeUp_RTCMode()` 复用 `InitWakeUp_NormalMode()`，再调用 `RTC_WKTimeConfig()`，见 `conf.c:268` 到 `conf.c:272`。
- `LowPower_DisableWakeupExti()` 会禁用 EXTI0/9/12/13，以及 UART1/2 对应 EXTI，见 `conf.c:151` 到 `conf.c:166`。

结论：

- 当前 RTC Stop 模式并不只是 RTC 周期唤醒，还会带上正常模式的外部唤醒源，包括按键、充电、通信唤醒、MCU_WK。
- 第一版可继续保留充电和按键唤醒，但建议把通信唤醒从“唤醒源”降级为“禁止睡眠原因”，直到 CAN/USART Stop 唤醒策略单独验证。

## 通信活跃时是否应该禁止 Stop

结论：应该禁止。

依据：

- 串口协议处理依赖中断和 `App_CommonUpper()` 状态机，且已有 `Sci_IsAnyPortBusy()` 可判断帧待处理/发送中/协议 busy，见 `Sci_Upper.c:1577` 到 `Sci_Upper.c:1595`、`Sci_Upper.c:1679` 到 `Sci_Upper.c:1691`。
- 当前低功耗只用 `RTC_ExtComCnt` 变化作为通信阻塞，见 `rtc_sleep.c:201` 到 `rtc_sleep.c:207`，无法覆盖 TX 未完成、帧已收完但未处理、协议层 busy。
- CAN 已有 `Can_IsBusy()` 判断队列/mailbox/读块流/CAN TSR，见 `Can_HDX.c:865` 到 `Can_HDX.c:880`，但低功耗准入未使用。
- `Can_PrepareSleep()` 会取消当前发送并清 TX 队列，见 `Can_HDX.c:882` 到 `Can_HDX.c:893`。如果通信活跃时直接进入 Stop，行为上等同于主动丢弃尚未完成的 CAN 发送窗口。

建议判定：

- RS485/Modbus：`Sci_IsAnyPortBusy() != 0` 或最近通信静默时间不足时禁止 Stop。
- CAN：`Can_IsBusy() != 0` 或最近 CAN RX/TX 活动静默时间不足时禁止 Stop。
- 不建议直接用当前 `Can_IsBusActive()` 作为“当前活跃”阻塞，因为它是粘性状态，一旦收发成功会保持 1，直到 `InitCan()` 清零。

## 当前主要缺口

1. 缺少统一外设休眠合约。当前 STOP 前后处理散落在 `IOstatus_RTCMode()`、`Sys_StopMode()`、`InitRunAfterStopWakeup()`、`LowPowerSleep_SaveCoreState()` 和外设模块里。
2. 串口已有 `Sci_IsAnyPortBusy()`，CAN 已有 `Can_IsBusy()`，但低功耗准入没有统一使用，通信活跃时仍可能进入 Stop。
3. UART1 和通信 EXTI 目前会随 `InitWakeUp_NormalMode()` 带入 RTC 模式。第一版目标是稳定 Stop + RTC 周期唤醒，不建议把 CAN/USART Stop 唤醒混在同一阶段验证。
4. LED 显示窗口没有显式低功耗阻塞位，当前依赖 LedBar 内部 sleep/blank 逻辑。
5. AFE sleep blocked 已有 SH367309 专用判断，但尚未抽象成通用 `LP_BLOCK_AFE_BUSY` 合约。
6. ADC 唤醒后清空滤波缓存，通信刚恢复时可能读到短暂未稳定值；第一版可接受，但测试矩阵必须覆盖。

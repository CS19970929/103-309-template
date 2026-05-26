# 当前项目低功耗用法梳理

> 范围：第一阶段只读分析。本文只记录当前 `103 + 309` STM32 BMS App 的已有低功耗、RTC、时基、IWDG、通信、AFE、SOC、Flash、LED 用法，不修改源码。

## 1. MCU 与工程形态

- 当前主工程目标是 `STM32F103C8`，Keil 工程 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:17` 和 `:954` 都指定 `Device=STM32F103C8`。
- 编译宏为 `STM32F10X_MD,USE_STDPERIPH_DRIVER`，见 `CommomSH367309_16series_103RCT6_C.uvprojx:340` 和 `:1277`。
- GPIO 配置固定启用 `__STM32F1__`，未启用 F0，见 `103 + 309/Project/Source/conf/conf_gpio.h:6-14`。
- 当前工程使用 ST 标准外设库 `STM32F10x_StdPeriph_Lib_V3.5.0`，不是 HAL。

结论：本项目当前低功耗实现应按 STM32F1 的 `RTC Alarm + EXTI17 + Stop` 方式分析；F0 的 RTC Wakeup Timer 只能作为后续可移植框架分支设计。

## 2. 启动与主循环

- `main()` 只调用 `AppInit_Boot()` 后进入 `Runtime_RunOnce()` 主循环，见 `103 + 309/Project/Source/main.c:5-12`。
- `AppInit_InitDevice()` 先 `SystemInit()`，再 `InitDelay()`、`IsSleepStartUp()`、`InitNVIC()`、`InitIO()`、串口、EEPROM、AFE、CAN、ADC、SOC、TIM3，最后启用中断和 IWDG，见 `103 + 309/Project/Source/AppInit.c:7-54`。
- `AppInit_Boot()` 在运行态初始化之后调用 `Init_RTC()`，见 `103 + 309/Project/Source/AppInit.c:66-71`。
- 主循环任务分三组：前置任务 `SysTime_LatchTaskFlags()`、老化、LED、AFE；IO/低功耗任务 `App_CommonUpper()`、`App_AnlogCal()`、`App_LowPowerProcess()`、`App_Can()`；后台任务 Flash 测试、Flash 更新、日志、产品 ID、喂狗，见 `103 + 309/Project/Source/Runtime.c:14-48`。

结论：低功耗入口当前不是独立框架，而是主循环中的 `App_LowPowerProcess()`，其实际实现为 `rtc_sleep()`。

## 3. 现有低功耗入口与状态

- `App_LowPowerProcess()` 直接调用 `rtc_sleep()`，见 `103 + 309/Project/Source/rtc_sleep.c:231-234`。
- 当前睡眠模式枚举是 `NORMAL_MODE / HICCUP_MODE / DEEP_MODE / NO_SLEEP`，见 `103 + 309/Project/Source/rtc_sleep.h:27-29`。
- 当前低功耗状态结构 `g_stLowPowerRtcStatus` 包含 `mode`、`readyToSleep`、单字节 `blockReason`、`rtcWake`、延迟计数和累计休眠秒数，见 `rtc_sleep.h:50-60` 与 `rtc_sleep.c:14-22`。
- `LowPower_Request()` 负责设置睡眠模式；`DEEP_MODE` 会额外调用 `RtcSleep_PortOnDeepSleepRequest()`，见 `rtc_sleep.c:91-112`。

结论：当前已有低功耗状态，但不是用户建议的显式状态机，也不是位图式阻塞原因；已有阻塞原因只覆盖电流、MCU_WAKE、老化、外部通信计数变化、AFE 未空闲。

## 4. HICCUP Stop + RTC 周期唤醒路径

当前非复位式 RTC Stop 路径如下：

1. `rtc_sleep()` 每秒运行一次，依据 `RtcSleep_PortIsOneSecondTick()` 判断，见 `rtc_sleep.c:414-423` 和 `rtc_sleep_port.c:6-9`。
2. `low_power_select_sleep_mode()` 判断是否应进入低功耗，空闲计数达到 `sys_time.time_enter_rtc` 后进入 `HICCUP_MODE`，见 `rtc_sleep.c:147-228` 与 `rtc_sleep_port.c:11-14`。
3. `rtc_sleep_run_hiccup_cycle()` 调用 `RtcSleep_PortPrepareRtcStop()`、`RtcSleep_PortEnterStop()`、`RtcSleep_PortDisableStopWakeup()`、`RtcSleep_PortRestoreAfterStop()`，见 `rtc_sleep.c:303-344`。
4. `RtcSleep_PortPrepareRtcStop()` 保存核心状态，重新初始化 RTC，配置 RTC 模式 IO 和唤醒源，见 `rtc_sleep_port.c:108-116`。
5. `RtcSleep_PortEnterStop()` 在进入 Stop 前后喂狗，并调用 `Sys_StopMode()`，见 `rtc_sleep_port.c:118-123`。
6. `Sys_StopMode()` 关闭 TIM3，清唤醒 pending，进入 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`，返回后调用 `cpu_frequency_conf()` 恢复时钟，见 `103 + 309/Project/Source/conf/conf.c:374-385`。
7. 唤醒后 `InitRunAfterStopWakeup()` 恢复延时、RTC 秒中断、IO、ADC、LED、USART 等，见 `conf.c:392-410`。
8. 若仍是 RTC 唤醒且没有异常，执行 `SOC_ApplyRtcRelaxationCompensation()` 和 `Can_RtcWakeService()`，继续下一轮 Stop，见 `rtc_sleep.c:313-328` 和 `rtc_sleep_port.c:161-177`。

结论：当前已有 Stop + RTC 周期唤醒闭环；它更像“连续打盹式 Stop”，而不是一次进入、一次退出的统一低功耗状态机。

## 5. NORMAL / DEEP 复位式睡眠路径

- `LowPowerSleep_SaveResetState()` 会保存 CAN、SOC 快照、老化进度和 LED 睡眠 SOC，见 `103 + 309/Project/Source/LowPowerSleep.c:5-16`。
- `SleepDeal_Continue()` 根据模式写入 BKP 标志，随后调用 `InitAFE1_Sleep(0)`、`AFE_Sleep()` 和 `MCU_RESET()`，见 `103 + 309/Project/Source/SleepDeal.c:83-114`。
- 睡眠标志保存在 `BKP_DR2/BKP_DR3`，带反码校验，见 `SleepDeal.c:123-167`。
- 复位启动早期 `IsSleepStartUp()` 读取 BKP 标志，并在对应模式里先进入 Stop 循环，唤醒条件有效后再 `IORecover_*()`，当前恢复函数最终 `MCU_RESET()`，见 `SleepDeal.c:186-230` 和 `conf.c:359-371`。

结论：当前 `NORMAL_MODE/DEEP_MODE` 是“写 BKP 标志 -> AFE 睡眠 -> MCU 复位 -> 启动早期 Stop -> 有效唤醒后再复位”的复位式策略，不是保 RAM 的 Stop 恢复策略。

## 6. RTC 当前用法

- `PROJECT_CFG_RTC_ENABLE=1`，见 `103 + 309/Project/Source/conf/Project_Config.h:84-86`。
- `Init_RTC()` 使用 `BKP_DR1` 判断是否需要完整初始化，优先 LSE，失败回退 LSI，见 `103 + 309/Project/Source/RTC.c:437-480` 与 `RTC.c:210-278`。
- 项目已经封装了 `RTC_WaitForLastTaskSafe()` 和 `RTC_WaitForSynchroSafe()`，带超时，见 `RTC.c:26-55`。
- Stop 唤醒周期来自 `RTC_GetWakeupPeriodSeconds()`，当前实际取 `Can_GetIdleRtcPeriodSeconds()`，见 `RTC.c:366-400`。
- IWDG 安全裁剪代码存在但被注释，见 `RTC.c:375-397`。
- `RTC_WKTimeConfig()` 进入 Stop 前关闭秒中断、关闭旧 Alarm、读取唤醒秒数并配置 Alarm，见 `RTC.c:407-418`。
- `RTC_DisableStopWakeup()` 和 `RTC_RestoreRunInterrupts()` 分别用于 Stop 后关闭 Alarm、恢复运行态秒中断，见 `RTC.c:420-435`。
- RTC Alarm 中断通过 `RTCAlarm_IRQHandler()` 和 `RTC_IRQHandler()` 共同处理，标记 `is_rtc_wakekup=true`，见 `RTC.c:492-535`。

结论：当前 RTC 已可用，路径符合 STM32F1 的 `RTC Alarm + EXTI17`；需要补强的是 IWDG 安全周期、BKP 分配冲突、safe wait 返回值处理和 `App_RTC()` 调度关系。

## 7. 时钟与时基

- `system_stm32f10x.c` 当前选择 `SYSCLK_FREQ_HSE = HSE_VALUE`，`SYSCLK_FREQ_72MHz` 被注释，见 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c:106-116`。
- `HSE_VALUE` 默认是 8 MHz，见 `stm32f10x.h:115-121`。
- Keil 工程 `<Cpu>` 字段写有 `CLOCK(12000000)`，见 `CommomSH367309_16series_103RCT6_C.uvprojx:21` 和 `:958`；这与源码默认 8 MHz 存在待确认差异。
- `SystemInit()` 会重置 RCC、配置系统时钟、设置 VTOR，见 `system_stm32f10x.c:212-270`。
- `SetSysClockToHSE()` 启动 HSE 并切 SYSCLK 到 HSE，等待 SWS 变为 HSE，见 `system_stm32f10x.c:504-574`。
- TIM3 是主 10 ms 软件时基：`InitTimer()` 将 TIM3 配成 100 kHz 计数、周期 999，即 10 ms 中断，见 `103 + 309/Project/Source/System_Init.c:100-127` 和 `TIM3_IRQHandler()` `System_Init.c:293-299`。
- `SysTime_Post10msTick()` 派生 50 ms、100 ms、200 ms、1000 ms 标志，见 `System_Init.c:258-286`。
- SysTick 只用于阻塞式延时，`InitDelay()` 根据 `SystemCoreClock` 计算 `fac_us/fac_ms`，见 `System_Init.c:132-150`。

结论：Stop 唤醒后必须先恢复系统时钟并重新计算 SysTick 延时参数，再恢复依赖时钟的 TIM/ADC/UART/CAN/LED；当前 `cpu_frequency_conf()` 已做 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`，但还没有独立 clock restore 模块。

## 8. IWDG 当前用法

- `PROJECT_CFG_WDOG_ENABLE=1`，见 `Project_Config.h:80-82`。
- `AppInit_InitDevice()` 在普通初始化完成后调用 `Init_IWDG()`，见 `AppInit.c:49-50`。
- `Init_IWDG()` 在 `__FUNC_RTC__` 分支下使用 `IWDG_Prescaler_256` 和 `Reload=0x0FFF`，见 `System_Init.c:33-48`。
- 主循环后台任务每轮喂狗，见 `Runtime.c:39-41`。
- 进入 Stop 前后也喂狗，见 `rtc_sleep_port.c:118-123`。
- CAN RTC 唤醒服务等待发送完成期间也喂狗，见 `Can_HDX.c:917-925`。

结论：当前 1 秒 RTC 周期下 IWDG 风险较低；若后续把 RTC 周期调大到十几秒以上，必须恢复 `RTC_GetWakeupPeriodSeconds()` 中的安全裁剪或在新框架里设置 `LP_BLOCK_IWDG_UNSAFE`。

## 9. 通信相关

### CAN

- CAN 低功耗供电脚由 `GPIO_CMNT_EN` 控制，开/关电平定义见 `103 + 309/Project/Source/Can_HDX.c:17-18`。
- CAN 普通周期为 1s/5s tick，RTC 周期当前固定 1 秒，见 `Can_HDX.c:20-24`。
- `Can_PrepareSleep()` 会取消正在发送的邮箱、清发送队列、停止读块流、关闭 CAN 收发器供电，见 `Can_HDX.c:882-894`。
- `Can_IsBusy()` 可判断队列、邮箱、读块流和硬件邮箱状态，见 `Can_HDX.c:865-880`。
- `Can_RtcWakeService()` RTC 唤醒后打开 CAN 供电、队列化 1s 报文，并在最长 150 个 10ms tick 内服务发送，见 `Can_HDX.c:906-933`。
- `App_Can()` 依赖 TIM3 10ms tick 运行周期任务、APP 命令、读块流、发送服务和 IAP 延时，见 `Can_HDX.c:935-947`。

### Modbus / RS485 / 上位机串口

- 串口初始化固定 19200、8N1、开启 RXNE/IDLE 中断，见 `103 + 309/Project/Source/Sci_Upper.c:1596-1653`。
- `Sci_IsAnyPortBusy()` 已能汇总端口待处理帧、发送长度和协议 busy，见 `Sci_Upper.c:1576-1594` 与 `Sci_Upper.c:1678-1690`。
- `App_CommonUpper()` 在主循环服务各串口端口，见 `Sci_Upper.c:2243-2256`。
- 串口发送完成后若 `u8FlashUpdateE2PROM` 置位，会转成 `u8FlashUpdateFlag`，见 `Sci_Upper.c:1437-1441`。

结论：CAN 和串口已有 busy 接口，但当前低功耗阻塞没有统一使用 `Can_IsBusy()` 和 `Sci_IsAnyPortBusy()`；后续应把通信活跃/正在回复/正在升级明确纳入阻塞位图。

## 10. ADC / AFE / SOC / Flash / LED 当前用法

### ADC

- ADC 使用 TIM2 CC2 触发、DMA 循环搬运，见 `103 + 309/Project/Source/ADC.c:149-181`、`ADC.c:215-260`。
- `ADC_StopForLowPower()` 已能关闭 TIM2、ADC 外部触发、ADC DMA、ADC、DMA1 并关闭相关时钟，见 `ADC.c:268-285`。
- Stop 唤醒后 `InitRunAfterStopWakeup()` 调用 `ADC_StopForLowPower()` 后再 `InitADC()`，见 `conf.c:401-402`。
- `App_AnlogCal()` 依据 10ms tick 追赶处理 ADC 结果，见 `ADC.c:488-517`。

### AFE

- 初始化入口 `InitAFE1()` 在启动阶段执行，见 `AppInit.c:30`。
- `App_AFEGet()` 每 200ms 调用一次，执行 AFE 监控、电压/温度/电流、保护和 SOC，见 `103 + 309/Project/Source/DataDeal.c:1225-1249`。
- AFE 读写等待路径中已有多处 `Feed_IWatchDog`，例如 `DataDeal.c:548-552`。
- 睡眠前 `SleepDeal_Continue()` 调用 `InitAFE1_Sleep(0)` 和 `AFE_Sleep()`，见 `SleepDeal.c:109-113`。
- HICCUP 模式下 AFE 是否阻塞由 `RtcSleep_PortIsAfeSleepBlocked()` 透传到 `RtcSleep_AfePortIsSleepBlocked()`，见 `rtc_sleep_port.c:61-64`。

### SOC

- SOC 初始化入口 `InitData_SOC()` 在启动阶段执行，见 `AppInit.c:34`。
- SOC 主计算由 `App_AFEGet()` 内部调用 `App_SOC()`，见 `DataDeal.c:1245-1249`。
- 休眠前保存 SOC 快照：`LowPowerSleep_SaveCoreState()` 调用 `SOC_SaveSnapshotBeforeSleep()`，见 `LowPowerSleep.c:5-10`。
- `SOC_SaveSnapshotBeforeSleep()` 在 SOC 初始化完成后调用 `soc_save_if_needed()`，见 `103 + 309/Project/Source/SocEnhance.c:1678-1685`。
- RTC 休眠累计秒数用于 `SOC_ApplyRtcRelaxationCompensation()`，见 `rtc_sleep.c:289-300` 和 `SocEnhance.c:1739-1764`。

### Flash / 参数 / 日志

- 参数保存入口 `EEPROM_SaveRWParametersToFlash()` 组包后调用 `StorageFlash_SaveRwParamData()`，见 `103 + 309/Project/Source/EEPROM.c:161-172`。
- Flash 写槽会 `FLASH_Unlock()`、擦页、写记录、`FLASH_Lock()`，见 `103 + 309/Project/Source/Flash.c:260-278` 和 `Flash.c:280-342`。
- 单半字写入也会擦页后写入，见 `Flash.c:650-670`。
- 日志请求 `LogRecord_RequestSleep()` 设置睡眠日志标志，见 `103 + 309/Project/Source/LogRecord.c:66-74`；`App_LogRecord()` 每秒写事件，见 `LogRecord.c:178-210`。
- 升级/IAP 标志 `u8FlashUpdateFlag` 置位后 `App_FlashUpdate()` 关 MOS、延时、禁 fault IRQ 并复位，见 `Flash.c:1078-1089`。

结论：Flash 擦写没有统一 busy 查询接口，低功耗框架不能仅靠“写 Flash 期间 CPU 忙”规避，应新增 Flash/Storage busy 或写请求 pending 阻塞位。

### LED

- LED/灯条配置启用休眠 SOC 备份，见 `Project_Config.h:354-356`。
- `APP_LedBar()` 在低功耗 pending 且 MCU_WAKE 不活跃时保存 SOC 并置 sleep，见 `103 + 309/Project/Source/LedBar.c:1039-1045`。
- 按键长按路径会保存 SOC 并直接 `entersleep(DEEP_MODE)` + `SleepDeal_Continue(DEEP_MODE)`，见 `LedBar.c:718-734`。
- 灯条显示窗口、按键滤波、充电滤波依赖 10ms/100ms tick，见 `LedBar.c:620-680`。

结论：LED 已参与低功耗，但“LED 正在显示窗口”当前不是通用阻塞位；后续建议将 `soc_display_10ms` 或公开 API 接入 `LP_BLOCK_LED_ACTIVE`。

## 11. 当前项目已有低功耗逻辑总结

已有：
- F1 RTC 初始化、LSE 优先/LSI fallback、RTC Alarm + EXTI17。
- HICCUP 模式 Stop + RTC 周期唤醒闭环。
- NORMAL/DEEP 复位式睡眠路径。
- Stop 前 IO 转模拟输入、CAN 断电、ADC 停止、LED 休眠、SOC 快照、老化进度保存。
- Stop 后时钟恢复、RTC 运行中断恢复、IO/ADC/USART/LED 恢复。
- CAN RTC 唤醒服务和 IWDG 喂狗点。

缺口：
- 没有独立 `bsp_rtc/bsp_power/bsp_clock/app_lowpower` 分层。
- 没有用户建议的 `LP_STATE_*` 显式状态机。
- 阻塞原因不是位图，缺少 `COMM/FLASH_BUSY/UPGRADE/FAULT/LED_ACTIVE/IWDG_UNSAFE` 等关键位。
- IWDG 与 RTC 周期的强约束在 `RTC_GetWakeupPeriodSeconds()` 中被注释。
- `cpu_frequency_conf()` 复用 `SystemInit()`，还没有专用 Stop 时钟恢复函数。
- BKP 寄存器有 RTC 标志、睡眠标志、故障快照、老化数据等多方使用，缺少统一分配表。
- 当前先追求“能稳定睡眠/唤醒”，不建议马上改最低电流或 CAN/USART Stop 唤醒。

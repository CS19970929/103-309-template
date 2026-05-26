# RTC 使用现状分析

本文档是 RtcAgent 第一阶段只读分析结果，仅依据官方资料和当前项目源码，不修改源码。

## 依据

- ST 官方资料：
  - RM0008：STM32F101xx/102xx/103xx/105xx/107xx Reference Manual，ST 在 STM32F103 文档页列为该系列参考手册。
  - AN2629：STM32F101xx/102xx/103xx low-power modes。该应用笔记说明 F10x 低功耗模式、RTC 自动唤醒、Stop 唤醒后的时钟状态、RTC 同步和写寄存器规则。
  - RM0091：STM32F0x1/F0x2/F0x8 Reference Manual，用于和 F0 RTC Wakeup Timer/Alarm 机制做差异说明。
- 当前项目源码：
  - `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
  - `103 + 309/Project/Source/conf/Project_Config.h`
  - `103 + 309/Project/Source/conf/conf.h`
  - `103 + 309/Project/Source/conf/conf_gpio.h`
  - `103 + 309/Project/Source/AppInit.c`
  - `103 + 309/Project/Source/RTC.c`
  - `103 + 309/Project/Source/RTC.h`
  - `103 + 309/Project/Source/conf/conf.c`
  - `103 + 309/Project/Source/rtc_sleep.c`
  - `103 + 309/Project/Source/rtc_sleep_port.c`
  - `103 + 309/Project/Source/SleepDeal.c`
  - `103 + 309/Project/Source/Runtime.c`
  - `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_rtc.c`
  - `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h`

## MCU 判断

当前 BMS 主工程是 STM32F1，不是 STM32F0。

- Keil 工程 `CommomSH367309_16series_103RCT6_C.uvprojx:17-19` 指定 `Device=STM32F103C8`、`PackID=Keil.STM32F1xx_DFP.2.3.0`。
- 同一工程 `CommomSH367309_16series_103RCT6_C.uvprojx:340-342` 的编译宏为 `STM32F10X_MD,USE_STDPERIPH_DRIVER`，包含路径指向 `STM32F10x_StdPeriph_Lib_V3.5.0`。
- `conf_gpio.h:6-14` 注释掉 `__STM32F0__`，定义 `__STM32F1__`，并包含 `stm32f10x.h`。
- `conf.h:7-8` 直接包含 `stm32f10x.h`，`stm32f0xx.h` 被注释。
- `stm32f10x.h:243-268` 的 `STM32F10X_MD` 中断表包含 `RTC_IRQn=3` 和 `RTCAlarm_IRQn=41`，后者注释为 RTC Alarm through EXTI Line Interrupt。

结论：当前代码应按 STM32F103C8/STM32F10x 标准外设库 RTC 模型分析；F0 的 `RTC Wakeup Timer` 不是当前工程的运行机制。

## F1 与 F0 RTC 机制差异

### STM32F1

F1 RTC 是旧式备份域计数器模型：

- `stm32f10x.h:1104-1126` 的 `RTC_TypeDef` 只有 `CRH/CRL/PRLH/PRLL/DIVH/DIVL/CNTH/CNTL/ALRH/ALRL`，体现为 32 位计数器加 20 位预分频和一个 Alarm 比较寄存器。
- `stm32f10x_rtc.h:58-60` 只有 `RTC_IT_OW`、`RTC_IT_ALR`、`RTC_IT_SEC` 三类 RTC 中断；没有 F0 常见的 `WUT`/Wakeup Timer 相关中断宏。
- AN2629 说明 F10x RTC 的时间基准由预分频器生成，秒中断来自 TR_CLK，Alarm 由 RTC counter 和 `RTC_ALR` 比较产生。
- AN2629 说明 F10x Stop 模式 RTC 自动唤醒需要配置 RTC alarm，并将 EXTI Line 17 配置为上升沿；Standby 模式 RTC alarm 唤醒不需要配置 EXTI17。

### STM32F0

F0 RTC 是新版日历 RTC 模型，移植时不能直接复用 F1 寄存器逻辑：

- ST F0 文档页列出 RM0091/RM0360 作为 F0 系列参考手册。
- RM0091 中 RTC Alarm 低功耗唤醒仍涉及 EXTI Line 17；RTC Wakeup Timer 使用 EXTI Line 20。
- F0 代码通常需要处理 RTC 写保护、初始化模式、`WUTR`、`WUTE/WUTIE`、`WUTF`、Alarm A/B、EXTI17/EXTI20 等；当前 F1 工程没有这些寄存器和宏。
- 具体 F0 子系列要查对应 RM：部分 F030 小容量型号没有完整 periodic wakeup timer，不能假设所有 F0 都有 WUT。

## 当前 RTC 是否使用

RTC 已启用并参与低功耗流程。

- `Project_Config.h:80-86` 设置 `PROJECT_CFG_WDOG_ENABLE=1` 和 `PROJECT_CFG_RTC_ENABLE=1`。
- `conf.h:49-55` 根据 `PROJECT_CFG_RTC_ENABLE` 定义 `__FUNC_RTC__`。
- `AppInit.c:66-71` 的 `AppInit_Boot()` 在初始化设备和运行态状态后调用 `Init_RTC()`。
- `Runtime.c:23-29` 在正常运行循环里调用 `App_LowPowerProcess()`，该函数在 `rtc_sleep.c:231-234` 转到 `rtc_sleep()`。
- `rtc_sleep.c:414-465` 是当前低功耗主流程，满足进入条件后在 `HICCUP_MODE` 中循环执行 RTC Stop 周期唤醒。

## 当前 F1 RTC 初始化路径

`Init_RTC()` 是当前 RTC 初始化入口。

- `RTC.c:437-445` 打开备份域访问，读取 `BKP_DR1` 判断是否需要完整初始化，完整初始化标记为 `RTC_BKP_DATA`，定义见 `RTC.h:7`。
- `RTC.c:206-279` 的 `RTC_ClockConfig()` 先尝试 LSE，超时后回退 LSI；`RTC.h:8-9` 定义 `LSE_START_TIMEOUT` 和 `LSE_FREQUENT=32767`。
- `RTC.c:235-265` 完整初始化时 `BKP_DeInit()`、打开 LSE、等待 `RCC_FLAG_LSERDY`、选择 `RCC_RTCCLKSource_LSE`、设置 `RTC_SetPrescaler(LSE_FREQUENT)`。
- `RTC.c:267-275` 在 LSE 超时后调用 `RTC_EnableLsiClock()`，使用 LSI 作为 RTC 时钟。
- `RTC.c:96-124` 的 `RTC_EnableLsiClock()` 打开 LSI，选择 `RCC_RTCCLKSource_LSI`，使能 RTC，设置秒中断和 `RTC_SetPrescaler(40000 - 1)`。
- `RTC.c:458-461` 首次完整初始化时调用 `RTC_TimeConfig()` 写入默认时间，并把 `RTC_BKP_DATA` 写入 `BKP_DR1`。

结论：当前项目已经做了 LSE 优先、LSI 兜底的 RTC 时钟初始化，符合 BMS 稳定性优先的方向。

## RTC Alarm 与 EXTI17

当前 Stop 周期唤醒使用 F1 RTC Alarm，不使用 F0 Wakeup Timer。

- `conf.c:268-272` 的 `InitWakeUp_RTCMode()` 先调用 `InitWakeUp_NormalMode()` 配置普通唤醒源，再调用 `RTC_WKTimeConfig()`。
- `RTC.c:408-418` 的 `RTC_WKTimeConfig()` 会打开备份域访问、关闭秒中断、关闭旧 alarm、读取周期并调用 `RTC_EnableAlarmAfterSeconds()`。
- `RTC.c:366-399` 的 `RTC_GetWakeupPeriodSeconds()` 从 `Can_GetIdleRtcPeriodSeconds()` 取得周期，最小保证 1 秒；其中 IWDG 安全窗口限制代码在 `RTC.c:375-397` 被注释，当前没有生效。
- `RTC.c:305-317` 的 `RTC_EnableAlarmAfterSeconds()` 清 pending，设置 `RTC_SetAlarm(RTC_GetCounter() + wake_seconds)`，使能 `RTC_IT_ALR`，清 `RTCAlarm_IRQn` pending。
- `RTC.c:326-351` 的 `RTC_AlarmConfig()` 配置 `EXTI_Line17` 为 interrupt/rising，并使能 `RTCAlarm_IRQn`，优先级 0/0。
- `RTC.c:281-288` 的 `RTC_ClearAlarmPending()` 同时清 `RTC_IT_ALR`、`RTC_FLAG_ALR`、`EXTI_Line17` 和 `RTCAlarm_IRQn` pending。
- `RTC.c:494-520` 的 `RTC_HandleAlarmWakeup()`/`RTCAlarm_IRQHandler()` 清 `RTC_IT_ALR` 和 `EXTI_Line17`，将 `is_rtc_wakekup=true`，并累加 `sys_time.rtc_alm_cnt`。

结论：当前 F1 RTC Stop 唤醒核心链路完整：Alarm 设置、EXTI17 上升沿、RTCAlarm IRQ、唤醒标志均已存在。

## Backup Domain 使用

当前备份域同时被 RTC、睡眠标志、老化和故障快照使用。

- RTC 初始化标志使用 `BKP_DR1`，见 `RTC.c:444-461`。
- 睡眠启动标志使用 `BKP_DR2/BKP_DR3`，见 `SleepDeal.c:123-130`。
- 故障原因使用 `BKP_DR11/BKP_DR12`，宏在 `FaultSnapshot.h:4-5`。
- 出厂老化进度使用 `BKP_DR6` 到 `BKP_DR10`，见 `FactoryAging.c:16-21`。
- `RTC.c:129` 和 `RTC.c:235` 在 RTC 重新初始化或首次完整初始化时调用 `BKP_DeInit()`。

风险判断：`BKP_DeInit()` 会重置备份域，和睡眠/老化/故障的 BKP 寄存器存在资源冲突。当前首次上电初始化可以接受；但若运行中因为 LSE/同步异常走到 `RTC_ReinitWithLsiClock()`，会清掉其他 BKP 状态。后续低功耗框架应建立 BKP 寄存器分配表，并限制 `BKP_DeInit()` 的触发范围。

## RTC_WaitForSynchro 卡死风险

官方标准库的 `RTC_WaitForSynchro()` 是死等，但当前项目没有直接调用该函数。

- 标准库 `stm32f10x_rtc.c:223-231` 的 `RTC_WaitForSynchro()` 清 `RSF` 后一直等待 `RTC_FLAG_RSF`，没有超时。
- 标准库 `stm32f10x_rtc.c:207-213` 的 `RTC_WaitForLastTask()` 等 `RTOFF`，也没有超时。
- 当前项目 `RTC.c:41-55` 自己实现了 `RTC_WaitForSynchroSafe()`，带 `RTC_WAIT_TIMEOUT` 超时。
- 当前项目 `RTC.c:26-39` 自己实现了 `RTC_WaitForLastTaskSafe()`，带 `RTC_WAIT_TIMEOUT` 超时。
- 全源码搜索只发现 `RTC.c:335` 的 `RTC_WaitForLastTask()`，但该语句位于 `RTC_AlarmConfig()` 的 `#if 0` 块内，不参与当前编译。

结论：当前代码已经规避了 `RTC_WaitForSynchro()` 直接死等的主要风险；但 `RTC_ClearAlarmPending()`、`RTC_DisableSecondInterrupt()`、`RTC_DisableAlarmInterrupt()`、`RTC_EnableAlarmAfterSeconds()`、`RTC_RestoreRunInterrupts()` 中部分 `RTC_WaitForLastTaskSafe()` 返回值没有被检查，超时后仍可能继续配置，属于 P1 级稳定性风险。

## Stop 进入与唤醒恢复

当前低功耗使用 Stop，不是 Standby。

- `rtc_sleep_port.c:108-116` 的 `RtcSleep_PortPrepareRtcStop()` 保存核心状态、调用 `Init_RTC()`、`IOstatus_RTCMode()` 和 `InitWakeUp_RTCMode()`。
- `rtc_sleep_port.c:118-123` 的 `RtcSleep_PortEnterStop()` 在 `Sys_StopMode()` 前后喂狗。
- `conf.c:374-385` 的 `Sys_StopMode()` 关闭 TIM3，清普通唤醒 EXTI pending，调用 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`，返回后调用 `cpu_frequency_conf()`。
- 标准库 `stm32f10x_pwr.c:197-229` 的 `PWR_EnterSTOPMode()` 设置 LPDS/SLEEPDEEP，执行 `__WFI()`，返回后清 SLEEPDEEP。
- `rtc_sleep_port.c:207-212` 的 `cpu_frequency_conf()` 调用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()` 恢复系统时钟相关状态。
- `conf.c:392-421` 的 `InitRunAfterStopWakeup()` 设置 `is_wakeup`，恢复 Delay、RTC 运行中断、IO、ADC、USART、CAN、TIM3 和 AFE IIC，并记录 `sys_time.wakeup_rtc`。

结论：当前 Stop 后时钟恢复路径存在，并且外设恢复集中在 `InitRunAfterStopWakeup()`；这符合 F1 Stop 唤醒后必须恢复系统时钟的要求。

## 秒中断与 RTC 时间变量

当前 RTC 秒中断已启用，但 `RTC_time` 的运行态更新路径不完整。

- `RTC.c:523-536` 的 `RTC_IRQHandler()` 处理 `RTC_IT_SEC` 时累加 `sys_time.rtc_sec_cnt` 并置 `TimeDisplay=1`。
- `RTC.c:482-490` 的 `App_RTC()` 看到 `TimeDisplay=1` 后调用 `Get_RTC_Time()` 更新 `RTC_time`。
- 全源码搜索未发现 `App_RTC()` 被运行调度调用。
- `Sci_Upper.c:780-786` 和 `Sci_Upper.c:934` 读取 `RTC_time` 对外返回 RTC 时间寄存器。

风险判断：RTC counter 本身在走，但上位机读取的 `RTC_time` 结构可能不是实时值。该问题不是 Stop 唤醒核心风险，但会影响“RTC 时间寄存器”对外一致性。

## 当前结论

1. 当前项目是 STM32F103C8/F1 标准外设库工程，RTC 使用 F1 counter/alarm 模型。
2. RTC 功能已开启，启动时调用 `Init_RTC()`，低功耗中调用 `RTC_WKTimeConfig()` 进入 Alarm 周期唤醒。
3. 当前 Stop 周期唤醒依赖 `RTC_ALR + EXTI_Line17 + RTCAlarm_IRQn`，链路完整。
4. 当前没有使用 F0 Wakeup Timer，也没有 F0 RTC 相关代码。
5. 项目已经用 `RTC_WaitForSynchroSafe()` 和 `RTC_WaitForLastTaskSafe()` 避免标准库死等，但部分返回值未检查。
6. `BKP_DeInit()` 和多个业务模块共享 BKP 寄存器存在冲突风险，后续需要统一备份域资源分配。
7. RTC 周期与 IWDG 安全窗口的限制目前被注释，不满足“RTC 唤醒周期必须小于 IWDG 超时时间”的框架目标，需要在设计阶段恢复为强约束。

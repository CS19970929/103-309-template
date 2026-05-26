# RTC 低功耗改造风险清单

阶段：第一阶段只读分析  
范围：`STM32F103C8` BMS App，标准外设库工程  
约束：本文件只汇总风险和依据，不修改源码，不改变 Modbus/CAN/SOC/保护/AFE/Flash/LED 行为。

## 依据来源

### 官方资料

1. ST RM0008 `STM32F101xx/102xx/103xx/105xx/107xx reference manual`  
   链接：https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf  
   和本项目风险直接相关的规则：
   - STM32F1 Stop 模式退出后，系统时钟由硬件选择为 HSI，原 HSE/PLL/SYSCLK 配置不会自动恢复。
   - 进入 Stop 前必须清 EXTI pending、外设中断 pending 和 RTC Alarm flag，否则 Stop 入口可能被忽略，程序继续执行。
   - IWDG 一旦启动，除复位外不能停止；IWDG 在 Stop/Standby 相关低功耗域中仍可运行。
   - ADC/DAC 若未关闭，在 Stop 中仍可能产生额外功耗。

2. ST AN2629 `STM32F101xx, STM32F102xx and STM32F103xx low-power modes`  
   链接：https://www.st.com/resource/en/application_note/an2629-stm32f101xx-stm32f102xx-and-stm32f103xx-lowpower-modes-stmicroelectronics.pdf  
   和本项目风险直接相关的规则：
   - F1 低功耗应区分 Sleep、Stop、Standby：Stop 保持 SRAM/寄存器，Standby 丢失大部分寄存器和 SRAM，仅保留 Backup 域和 Standby 电路。
   - 低功耗模式选择要在功耗、唤醒源、启动时间之间折中，第一阶段不应直接追求最低电流。

3. ST AN2821 `Clock/calendar implementation on STM32F10xxx RTC`  
   链接：https://www.st.com/resource/en/application_note/an2821-clockcalendar-implementation-on-the-stm32f10xxx-microcontroller-rtc-stmicroelectronics.pdf  
   和本项目风险直接相关的规则：
   - F10xxx Stop 中 CPU 和外设时钟关闭，PLL/HSI/HSE 禁用，SRAM/寄存器保持，RTC 和 IWDG 保持运行。
   - RTC Alarm 可用于从低功耗模式自动唤醒 MCU。

4. ST AN4759 `Introduction to using the hardware RTC and TAMP with STM32 MCUs`  
   链接：https://www.st.com/resource/en/application_note/an4759-introduction-to-using-the-hardware-realtime-clock-rtc-and-the-tamper-management-unit-tamp-with-stm32-mcus-stmicroelectronics.pdf  
   和本项目风险直接相关的规则：
   - RTC 可通过周期唤醒单元/Alarm 从低功耗模式唤醒 MCU。
   - 低功耗前应配置下一次 RTC 唤醒并清相关 flag，唤醒中断中设置应用层唤醒原因。

### 当前项目证据

1. MCU 与构建配置：
   - `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:17` 目标器件为 `STM32F103C8`。
   - `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:340` 编译宏为 `STM32F10X_MD,USE_STDPERIPH_DRIVER`。
   - `103 + 309/Project/Source/conf/Project_Config.h:80-86` 量产配置启用 `PROJECT_CFG_WDOG_ENABLE=1` 和 `PROJECT_CFG_RTC_ENABLE=1`。

2. 当前低功耗路径：
   - `103 + 309/Project/Source/rtc_sleep.h:27-48` 已定义 `NORMAL_MODE/HICCUP_MODE/DEEP_MODE/NO_SLEEP` 和低功耗阻塞原因，但阻塞位不完整。
   - `103 + 309/Project/Source/rtc_sleep.c:130-144` 只以充放电电流和 `MCU_WAKE` 判断基础阻塞。
   - `103 + 309/Project/Source/rtc_sleep.c:190-212` 已阻塞工厂老化、外部通信计数变化、AFE 不空闲。
   - `103 + 309/Project/Source/rtc_sleep.c:303-345` `rtc_sleep_run_hiccup_cycle()` 是当前 Stop + RTC 周期唤醒主路径。
   - `103 + 309/Project/Source/rtc_sleep.c:453-456` HICCUP 通过 `while (rtc_sleep_run_hiccup_cycle())` 连续睡眠。

3. RTC/Stop/时钟恢复：
   - `103 + 309/Project/Source/RTC.c:26-55` 已有 `RTC_WaitForLastTaskSafe()` 和 `RTC_WaitForSynchroSafe()` 超时等待。
   - `103 + 309/Project/Source/RTC.c:206-278` RTC 优先 LSE，失败后回退 LSI。
   - `103 + 309/Project/Source/RTC.c:281-317` 已清 RTC Alarm、EXTI17、NVIC pending 并设置下一次 Alarm。
   - `103 + 309/Project/Source/RTC.c:326-350` 使用 F1 `RTCAlarm_IRQn + EXTI_Line17`。
   - `103 + 309/Project/Source/RTC.c:366-399` `RTC_GetWakeupPeriodSeconds()` 中 IWDG 安全裁剪代码已被注释。
   - `103 + 309/Project/Source/conf/conf.c:374-385` `Sys_StopMode()` 进入 Stop，返回后调用 `cpu_frequency_conf()`。
   - `103 + 309/Project/Source/conf/conf.c:392-421` `InitRunAfterStopWakeup()` 恢复 Delay、RTC、IO、ADC、LED、USART、CAN、TIM、AFE IIC。

4. IWDG：
   - `103 + 309/Project/Source/System_Init.c:33-48` `Init_IWDG()` 在 RTC 功能下配置 `IWDG_Prescaler_256 + Reload 0x0FFF` 并启动 IWDG。
   - `103 + 309/Project/Source/rtc_sleep_port.c:118-123` Stop 前后各喂狗一次。
   - `103 + 309/Project/Source/Can_HDX.c:23-24` 当前 CAN RTC 周期为 1 秒，RTC 服务最长 150 个 10ms tick。
   - `103 + 309/Project/Source/Can_HDX.c:917-925` CAN RTC 服务循环内喂狗。

5. 通信、Flash、SOC、AFE、MOS：
   - `103 + 309/Project/Source/Can_HDX.c:865-879` `Can_IsBusy()` 可判断 TX 队列、邮箱、读块流和发送邮箱忙。
   - `103 + 309/Project/Source/Can_HDX.c:882-893` `Can_PrepareSleep()` 会取消正在发送的邮箱、清队列、关闭 CAN 供电。
   - `103 + 309/Project/Source/Can_HDX.c:896-903` 只导出 `Can_IsBusActive()` 和固定 RTC 周期，当前低功耗阻塞尚未统一使用 `Can_IsBusy()`。
   - `103 + 309/Project/Source/Sci_Upper.c:1678-1690` 已有 `Sci_IsAnyPortBusy()`，但当前低功耗阻塞未接入。
   - `103 + 309/Project/Source/Flash.c:259-278`、`:532-548`、`:613-628` 多处 Flash 擦写/编程没有全局 busy 标志。
   - `103 + 309/Project/Source/LogRecord.c:117-142` 睡眠日志保存成功后才清 `LowPower_ClearToSleepFlag()`。
   - `103 + 309/Project/Source/LowPowerSleep.c:5-10` 入睡前保存 CAN、SOC、老化核心状态。
   - `103 + 309/Project/Source/SocEnhance.c:1678-1685` 入睡前保存 SOC 快照。
   - `103 + 309/Project/Source/SocEnhance.c:1739-1763` RTC 唤醒后用 `rest_seconds` 做 SOC 静置/OCV 补偿。
   - `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:11-28` AFE 状态不空闲时阻塞睡眠。
   - `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:81-107` 唤醒后读取 AFE 状态、同步 MOS 状态并触发故障判断。
   - `103 + 309/Project/Source/System_Monitor.c:72-85` MOS 运行态状态由 `SystemRuntime_SetMosStatus()` 缓存。
   - `103 + 309/Project/Source/SH367309_Func.c:189-209` MCU 写 AFE MTP 配置控制 MOS。

## 风险分级定义

| 等级 | 定义 |
| --- | --- |
| P0 | 可能导致板子无法稳定唤醒、IAP/通信严重异常、IWDG 误复位、保护/MOS 状态错误、Flash 数据破坏，必须在最小实现前纳入硬约束。 |
| P1 | 可能导致低功耗状态不稳定、功能体验异常、SOC/日志/老化时间不准，必须在第一版验证矩阵覆盖。 |
| P2 | 主要影响可维护性、诊断能力、跨项目移植性或功耗优化空间，可在稳定框架后迭代。 |

## P0 风险

### P0-01 RTC/EXTI pending 未清导致 Stop 进不去或刚进即醒

风险描述：  
F1 Stop 入口要求清 EXTI pending、外设中断 pending、RTC Alarm flag。若后续新增 `bsp_rtc/app_lowpower` 时绕过当前 `RTC_WKTimeConfig()` 和 `LowPower_ClearWakeupPending()` 顺序，可能出现 Stop 入口被忽略、RTC 连续唤醒、主循环误判为睡眠成功。

官方依据：  
RM0008：进入 Stop 前必须清 EXTI pending、外设中断 pending 和 RTC Alarm flag，否则 Stop 入口流程会被忽略。

项目依据：  
`RTC_ClearAlarmPending()` 清 `RTC_IT_ALR/RTC_FLAG_ALR/EXTI_Line17/RTCAlarm_IRQn`，见 `103 + 309/Project/Source/RTC.c:281-288`。  
`RTC_EnableAlarmAfterSeconds()` 设置 Alarm 前后清 pending，见 `103 + 309/Project/Source/RTC.c:305-317`。  
`Sys_StopMode()` Stop 前调用 `LowPower_ClearWakeupPending()`，见 `103 + 309/Project/Source/conf/conf.c:374-382`。

后续控制要求：  
低功耗框架必须固定入口顺序：配置唤醒 GPIO/RTC，清 RTC/EXTI/NVIC pending，喂狗，最后进入 Stop。禁止业务模块直接调用 `PWR_EnterSTOPMode()`。

### P0-02 Stop 唤醒后系统时钟未恢复导致 CAN/USART/TIM/ADC 时序错误

风险描述：  
Stop 唤醒后硬件选择 HSI 为系统时钟。若恢复顺序遗漏 `cpu_frequency_conf()` 或未来抽象出 `bsp_clock.c` 时恢复不完整，CAN 位时序、USART 波特率、TIM 扫描、ADC 触发和 SysTick 延时都会偏离，表现为通信异常、显示异常、采样异常。

官方依据：  
RM0008 和 AN2821：F10xxx Stop 退出后时钟配置回到 HSI 作为 SYSCLK，PLL/HSE 不会自动恢复。

项目依据：  
当前 `Sys_StopMode()` 在 `PWR_EnterSTOPMode()` 返回后调用 `cpu_frequency_conf()`，见 `103 + 309/Project/Source/conf/conf.c:374-385`。  
当前恢复函数后续调用 `InitDelay()`、`InitADC()`、`AppInit_InitSci()`、`InitCan()`、`InitTimer()`，见 `103 + 309/Project/Source/conf/conf.c:392-421`。  
系统时钟源码当前使用 `SYSCLK_FREQ_HSE = HSE_VALUE`，见 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c:106-116`。

后续控制要求：  
`LP_AfterWakeup()` 的第一步必须是恢复 SYSCLK/AHB/APB 并更新 `SystemCoreClock`，之后才能恢复 Delay、TIM、ADC、USART、CAN、LED、AFE。恢复失败必须置错误状态并禁止再次睡眠。

### P0-03 RTC 周期超过 IWDG 安全窗口导致 Stop 中误复位

风险描述：  
当前 RTC 周期为 1 秒时安全；但 `RTC_GetWakeupPeriodSeconds()` 已注释 IWDG 安全裁剪。如果后续为了降低功耗把周期改成 30 秒或 60 秒，而 IWDG 已启动，Stop 中可能在 RTC 唤醒前被 IWDG 复位，形成现场“偶发重启”。

官方依据：  
RM0008：IWDG 启动后除复位外不能停止，且低功耗中仍由 LSI 运行。AN2821 也说明 Stop 中 RTC 和 IWDG 保持运行。

项目依据：  
量产配置启用 IWDG 和 RTC，见 `103 + 309/Project/Source/conf/Project_Config.h:80-86`。  
`Init_IWDG()` 在 RTC 功能下配置 `256/0x0FFF` 并启动，见 `103 + 309/Project/Source/System_Init.c:33-48`。  
Stop 前后喂狗见 `103 + 309/Project/Source/rtc_sleep_port.c:118-123`。  
IWDG 安全裁剪已注释，见 `103 + 309/Project/Source/RTC.c:366-399`。

后续控制要求：  
必须新增 `LP_BLOCK_IWDG_UNSAFE` 或等价机制。已启用 IWDG 的非复位式 Stop 周期必须按 LSI 最快频率保守计算，第一版建议限制目标周期不超过 10 秒；超限时禁止 Stop，不允许静默裁剪。

### P0-04 通信半包、待 ACK 或升级窗口中进入 Stop 造成协议乱序

风险描述：  
当前低功耗阻塞只看外部通信计数变化和 CAN bus active，没有统一阻塞 CAN TX 忙、读块流、串口半包、升级延迟窗口。若在 Modbus/RS485 半帧、CAN 待读块/ACK、CAN IAP 进入延迟、上位机写寄存器过程中进入 Stop，可能破坏协议时序或误触发升级/复位。

官方依据：  
RM0008：Stop 中外设时钟停止；退出 Stop 后需要重新初始化外设。通信协议不能假设 Stop 中 USART/CAN 仍按运行态继续完成收发。

项目依据：  
`Can_IsBusy()` 已能判断 TX 队列、邮箱、读块流和邮箱忙，见 `103 + 309/Project/Source/Can_HDX.c:865-879`。  
`Can_PrepareSleep()` 会取消发送邮箱并清队列，见 `103 + 309/Project/Source/Can_HDX.c:882-893`；这意味着睡前直接清队列会丢弃未完成通信。  
CAN IAP 延迟窗口最终置 `u8FlashUpdateFlag=1`，见 `103 + 309/Project/Source/Can_HDX.c:744-755`。  
串口已有 `Sci_IsAnyPortBusy()`，见 `103 + 309/Project/Source/Sci_Upper.c:1678-1690`，但当前低功耗阻塞未接入该函数。  
主循环中低功耗任务在 `App_Can()` 前执行，见 `103 + 309/Project/Source/Runtime.c:23-29`，因此低功耗检查可能先于本轮 CAN 处理。

后续控制要求：  
第一版必须固定“通信活跃禁止休眠”：`Can_IsBusy()`、`Can_IsBusActive()`、`Sci_IsAnyPortBusy()`、升级/IAP pending 任一为真时置 `LP_BLOCK_COMM/LP_BLOCK_UPGRADE`。不要在确认可睡前调用会清队列的 `Can_PrepareSleep()`。

### P0-05 Flash 擦写/参数保存/日志/SOC 快照期间进入 Stop 或复位

风险描述：  
Flash 擦写和编程是不可打断的关键区。当前 Flash 写入函数没有全局 busy 标志，低功耗阻塞位也没有 `FLASH_BUSY`。若休眠判断和 Flash 写入交错，可能导致日志/参数/SOC 快照未写完、IWDG 喂狗窗口被拖长、协议响应丢失，甚至在升级/参数保存窗口复位。

官方依据：  
RM0008：若 Flash programming 正在进行，Stop 入口会被 Flash 访问影响；工程层仍应避免在擦写/编程窗口进入低功耗，因为业务状态和协议响应需要一致性。

项目依据：  
`StorageFlash_WriteSlot()` 包含 `FLASH_Unlock()`、擦页、写记录、`FLASH_Lock()`，见 `103 + 309/Project/Source/Flash.c:259-278`。  
`StorageFlash_SaveJournalPair()` 可能擦页后写记录，见 `103 + 309/Project/Source/Flash.c:532-548`。  
`StorageFlash_SaveJournalPage()` 可能擦页后写记录，见 `103 + 309/Project/Source/Flash.c:613-628`。  
睡眠日志保存成功后才清睡眠 pending，见 `103 + 309/Project/Source/LogRecord.c:117-142`。  
低功耗状态只定义到 `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE`，没有 Flash busy 位，见 `103 + 309/Project/Source/rtc_sleep.h:39-48`。

后续控制要求：  
新增 Storage/Flash busy 入口或由 Flash 写包装层维护 busy 计数。Flash busy、日志 pending、SOC 快照保存 pending、参数保存 pending、IAP pending 时必须禁止 Stop。

### P0-06 AFE/MOS 状态不同步导致保护或 MOS 输出错误

风险描述：  
BMS 保护板低功耗不是普通 MCU 休眠。睡前/醒后如果 AFE 状态、故障状态、MOS 实际状态和 MCU 缓存状态不同步，可能导致放电 MOS 被错误认为打开/关闭、保护状态未及时上报，或进入不应进入的低功耗。

官方依据：  
AN2629 的模式选择原则要求在唤醒源、启动时间和可用功能之间折中。对 BMS 而言，保护和 MOS 状态优先级高于功耗优化。

项目依据：  
`RtcSleep_AfePortIsSleepBlocked()` 在 AFE 状态异常时阻塞睡眠，见 `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:11-28`。  
`RtcSleep_AfePortHasAfeWake()` 唤醒后读 AFE 状态、同步 MOS 并触发故障判断，见 `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:81-107`。  
运行态 MOS 状态是软件缓存，见 `103 + 309/Project/Source/System_Monitor.c:72-85`。  
实际 MOS 控制写入 AFE MTP 配置，见 `103 + 309/Project/Source/SH367309_Func.c:189-209`。

后续控制要求：  
`LP_BeforeSleep()` 只能在 AFE 明确可睡时继续；`LP_AfterWakeup()` 必须重新读取 AFE BSTATUS/MOS/Fault，并以 AFE 实际状态覆盖 MCU 缓存。任何 AFE 通信失败应退出低功耗并置故障/禁止再次睡眠。

## P1 风险

### P1-01 RTC/LSE/LSI 初始化失败或等待点返回值未统一处理

风险描述：  
项目已经把 RTC 等待改成 safe wait，方向正确；但后续若新增 RTC BSP 时未保留超时，或忽略 `RTC_CLOCK_INIT_FAILED`，会复现调试器连接时卡死、LSE 不起振卡死或 RTC 失效后仍继续睡眠的问题。

官方依据：  
RM0008：F1 RTC 位于备份域，配置 RTC 前必须开启 PWR/BKP 访问；RTC 依赖 LSE/LSI 等低速时钟。AN2821：RTC Alarm 是低功耗唤醒关键路径。

项目依据：  
`RTC_WaitForLastTaskSafe()` 和 `RTC_WaitForSynchroSafe()` 带超时，见 `103 + 309/Project/Source/RTC.c:26-55`。  
`RTC_ClockConfig()` 优先 LSE、失败回退 LSI，见 `103 + 309/Project/Source/RTC.c:206-278`。  
`Init_RTC()` 在 `RTC_CLOCK_INIT_FAILED` 时直接返回，见 `103 + 309/Project/Source/RTC.c:437-456`。

后续控制要求：  
RTC 初始化失败必须上报 `LP_BLOCK_RTC_UNREADY` 或进入 `LP_STATE_ERROR`，不能继续进入 Stop。所有 RTC wait 返回值要在 BSP 层统一检查。

### P1-02 Backup Domain 复用导致睡眠标志、LED SOC、老化进度、故障快照丢失

风险描述：  
项目使用 BKP_DR1 作为 RTC 初始化标志，BKP_DR2/3 作为睡眠标志，DR4/5 作为 LED 睡眠 SOC，DR6-10 作为老化进度，DR11/12 作为故障快照。`BKP_DeInit()` 只要被误触发，就会清掉这些跨复位状态，造成休眠启动路径、老化剩余时间、故障原因或显示 SOC 丢失。

官方依据：  
AN2629：Standby 只保留 Backup 域和 Standby 电路；Backup 域是低功耗/复位式恢复的关键状态载体。

项目依据：  
`RTC_ClockConfig()` full init 分支调用 `BKP_DeInit()`，见 `103 + 309/Project/Source/RTC.c:235`。  
睡眠标志使用 `BKP_DR2/DR3`，见 `103 + 309/Project/Source/SleepDeal.c:123-130`。  
LED 睡眠 SOC 使用 `BKP_DR4/DR5`，见 `103 + 309/Project/Source/LedBar.c:937-963`。  
老化进度使用 `BKP_DR6-DR10`，见 `103 + 309/Project/Source/FactoryAging.c:16-21`。  
故障快照使用 `BKP_DR11/DR12`，见 `103 + 309/Project/Source/FaultSnapshot.h:4-5`。

后续控制要求：  
建立 BKP 寄存器分配表。RTC 重新初始化不得无条件 `BKP_DeInit()`，应只在明确允许清备份域时执行，并保护睡眠/老化/故障/显示状态。

### P1-03 SOC 休眠时间补偿丢失或不可信

风险描述：  
Stop 中 SysTick 不运行，SOC 静置时间只能来自 RTC。当前 HICCUP 路径会累计 `s_u32RtcSleepElapsedSeconds` 并调用 `SOC_ApplyRtcRelaxationCompensation()`，但如果 RTC 唤醒原因判断失败、RTC 使用 LSI 且未标记精度、或异常唤醒路径未调用运行时间补偿，SOC 静置 OCV 校准和 runtime seconds 会偏差。

官方依据：  
AN2821：Stop 中 CPU/外设时钟关闭，RTC 保持运行。AN2629：Stop 是寄存器保持的低功耗模式，但系统时基不会像运行态一样推进。

项目依据：  
`rtc_sleep_run_hiccup_cycle()` 只在 `RtcSleep_PortIsRtcWake()` 时累计 RTC 休眠秒数，见 `103 + 309/Project/Source/rtc_sleep.c:313-318`。  
SOC RTC 静置补偿在 `RtcSleep_PortApplySocRtcRest()` 中执行，见 `103 + 309/Project/Source/rtc_sleep_port.c:166-178`。  
运行时间累计在退出低功耗时执行，见 `103 + 309/Project/Source/rtc_sleep_port.c:180-189` 和 `103 + 309/Project/Source/rtc_sleep.c:342-344`。  
SOC 休眠补偿会保存快照，见 `103 + 309/Project/Source/SocEnhance.c:1739-1763`。

后续控制要求：  
低功耗框架必须提供 `LP_GetLastSleepSeconds()`，并区分 RTC 时间可信度。LSI fallback 时应降低 SOC 静置校准置信度或记录诊断状态。

### P1-04 唤醒后外设恢复顺序被新框架破坏

风险描述：  
当前恢复顺序已经覆盖 Delay、RTC、IO、ADC、LED、USART、CAN、TIM3、AFE IIC。后续拆分 `bsp_power/bsp_clock/app_lowpower` 时如果恢复顺序改变，可能造成 GPIO 还未恢复就启动通信、ADC DMA 未恢复就采样、LED 扫描提前启动或 AFE IIC 通信失败。

官方依据：  
RM0008/AN2821：Stop 后外设时钟关闭，醒后需要恢复运行态外设时钟和配置。

项目依据：  
`InitRunAfterStopWakeup()` 当前恢复顺序见 `103 + 309/Project/Source/conf/conf.c:392-421`。  
`ADC_StopForLowPower()` 明确停 TIM2、ADC、DMA 并 DeInit，见 `103 + 309/Project/Source/ADC.c:268-285`。  
`IOstatus_RTCMode()` 入睡前改变 GPIO/电源轨状态，见 `103 + 309/Project/Source/conf/conf.c:297-323`。

后续控制要求：  
`LP_AfterWakeup()` 文档和实现必须固化恢复顺序；第三阶段每次改顺序都要更新文档并验证 CAN、Modbus、ADC、AFE、LED。

### P1-05 复位式睡眠和非复位式 Stop 语义混用

风险描述：  
当前 `NORMAL_MODE/DEEP_MODE` 通过写睡眠标志、AFE sleep、`MCU_RESET()`，启动后 `IsSleepStartUp()` 再进入 Stop；而 `HICCUP_MODE` 是运行态连续 Stop。若后续把 Standby、Deep、Stop 概念混在一个接口里，容易造成 RAM 状态误用、IWDG 策略误用、唤醒路径误判。

官方依据：  
AN2629：Stop 保持 SRAM/寄存器；Standby 丢失 SRAM 和大多数寄存器，仅 Backup 域保留。两者恢复模型不同。

项目依据：  
`SleepDeal_Continue()` 写睡眠标志、AFE sleep 后 `MCU_RESET()`，见 `103 + 309/Project/Source/SleepDeal.c:83-114`。  
`IsSleepStartUp()` 根据 BKP 标志进入不同 Stop 循环，见 `103 + 309/Project/Source/SleepDeal.c:186-230`。  
HICCUP 直接在运行态循环 `rtc_sleep_run_hiccup_cycle()`，见 `103 + 309/Project/Source/rtc_sleep.c:453-456`。

后续控制要求：  
第一版只把 `HICCUP_MODE` 等价为 Stop + RTC 周期唤醒。`DEEP_STANDBY` 必须单独设计为复位式策略，不和运行态 Stop 共用同一套 after-wakeup 假设。

### P1-06 LED/按键显示窗口与休眠抢占

风险描述：  
低功耗前会调用 `LedBar_SetSleep()`、`LedBar_PrepareForStop()`，但当前阻塞原因没有 `LED_ACTIVE`。如果用户按键查看 SOC、老化剩余时间显示或充电图标刷新期间进入 Stop，界面会闪断或显示状态丢失。

项目依据：  
Stop 准备中调用 `LedBar_SetSleep(1u)` 和 `ADC_StopForLowPower()`，见 `103 + 309/Project/Source/conf/conf.c:114-118`。  
`IOstatus_Base()` 和 `IOstatus_RTCMode()` 都会准备 LED 低功耗，见 `103 + 309/Project/Source/conf/conf.c:281-294`、`:297-323`。  
睡眠前保存 LED SOC，见 `103 + 309/Project/Source/LowPowerSleep.c:12-16` 和 `103 + 309/Project/Source/LedBar.c:937-963`。

后续控制要求：  
新增 `LP_BLOCK_LED_ACTIVE`。显示窗口、按键窗口、老化剩余时间读取显示窗口内禁止进入 Stop。

## P2 风险

### P2-01 阻塞原因仍是单值枚举，不利于现场诊断

风险描述：  
当前 `g_stLowPowerRtcStatus.blockReason` 是单个 `uint8_t`，只能记录最后一个阻塞原因。BMS 常见情况是通信、LED、AFE、Flash、IWDG 同时阻塞，单值会隐藏根因，影响上位机诊断和量产问题复现。

项目依据：  
单值字段定义见 `103 + 309/Project/Source/rtc_sleep.h:50-58`。  
当前阻塞原因枚举不包含 Flash、IWDG、Upgrade、Fault、LED，见 `103 + 309/Project/Source/rtc_sleep.h:39-48`。

后续控制要求：  
第三阶段建议按用户建议改为位图：`LP_BLOCK_CHARGE/DISCHARGE/COMM/KEY/AFE_BUSY/FLASH_BUSY/UPGRADE/FAULT/LED_ACTIVE/IWDG_UNSAFE`。

### P2-02 LSE 与 HSE 实际硬件频率/起振裕量未纳入诊断

风险描述：  
当前源码默认 HSE 相关配置来自标准库 `HSE_VALUE`，Keil 工程 `<Cpu>` 字段显示 `CLOCK(12000000)`。虽然这不一定直接等于实际晶振，但如果硬件实际 HSE 不是源码假设值，Stop 后恢复时通信和定时都会偏。LSE 起振失败会回退 LSI，SOC 时间精度也会下降。

官方依据：  
RM0008/AN2821：Stop 后要恢复运行态时钟；RTC 低功耗依赖 LSE/LSI。ST AN2867 说明晶振设计、负载电容和板级布局影响 LSE/HSE 起振可靠性。

项目依据：  
Keil 目标器件和 `<Cpu>` 字段见 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:17-21`。  
标准库时钟配置假设外部 8MHz 晶体，见 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c:96-116`。  
RTC LSE 失败会回退 LSI，见 `103 + 309/Project/Source/RTC.c:206-278`。

后续控制要求：  
文档和启动诊断中记录 HSE/LSE/LSI 状态。低功耗测试矩阵要包含 LSE 正常、LSE 失败回退 LSI、时钟恢复后 CAN/USART 实测。

### P2-03 F0/F1 可移植接口容易误用 F1 RTC Alarm 模型

风险描述：  
当前项目是 F1，RTC 是 32-bit counter + Alarm + EXTI17。后续要移植到 F0 时，不能照搬 F1 的 `RTC_SetAlarm(RTC_GetCounter()+seconds)` 模型；F0 新 RTC 通常有 Wakeup Timer/Alarm，不同 F030/F070 型号能力也不同。

官方依据：  
AN4759：新 RTC 支持周期唤醒单元。RM0091/RM0360：F0 系列 RTC Wakeup Timer/Alarm 和 EXTI line 需按型号区分。

项目依据：  
当前工程明确是 `STM32F103C8`，见 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:17`。  
当前 RTC Alarm + EXTI17 配置见 `103 + 309/Project/Source/RTC.c:326-350`。

后续控制要求：  
`bsp_rtc` 需要编译期能力宏：F1 使用 Alarm+EXTI17；F0 支持 Wakeup Timer 时用 Wakeup+EXTI20，不支持时回退 Alarm+EXTI17。

### P2-04 调试模式与量产模式 IWDG/低功耗行为差异未形成统一说明

风险描述：  
Debug 下 `EnableLowPowerDebug()` 可设置 DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP，但量产不应依赖调试冻结。若测试人员在 Debug 下验证低功耗，再到 Release 量产出现 IWDG 复位，容易误判。

项目依据：  
`EnableLowPowerDebug()` 只在 `_DEBUG_` 下配置 DBGMCU，见 `103 + 309/Project/Source/System_Init.c:21-30`。  
量产 `PROJECT_CFG_BUILD_PROFILE` 默认 0，见 `103 + 309/Project/Source/conf/Project_Config.h:10-18`。

后续控制要求：  
测试记录必须标注 Debug/Release、IWDG 是否冻结、是否接调试器。功耗和 IWDG 结论以 Release 不接调试器为准。

## 风险收敛优先级

1. 第三阶段最小实现前必须先落地 P0 硬约束：RTC/EXTI 清 pending、时钟恢复、IWDG 安全窗口、通信 busy、Flash busy、AFE/MOS 同步。
2. 第一版只做 `HICCUP_MODE` 的 Stop + RTC 周期唤醒，不改 CAN/USART Stop 唤醒，不把 `DEEP_MODE` 改成真正 Standby。
3. 新增 `app_lowpower` 时先做阻塞原因位图和状态机诊断，不要大规模重构现有 CAN、Modbus、SOC、AFE、Flash、LED。
4. 每次代码实现必须同步更新本风险清单或对应设计/测试文档，避免低功耗规则只存在于对话中。

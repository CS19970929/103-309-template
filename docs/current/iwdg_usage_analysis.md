# IWDG 当前使用分析

更新时间：2026-05-26  
阶段：第一阶段，只读源码分析  
范围：当前 `103 + 309` STM32 BMS App 工程，重点分析 IWDG 初始化、喂狗路径、Stop/RTC 低功耗期间误复位风险。

## 资料依据

- ST AN2629《STM32F101xx, STM32F102xx and STM32F103xx low-power modes》：STM32F10xxx Stop 模式会停止 1.8 V 域时钟、HSI/HSE/PLL，SRAM 和寄存器保持；Stop 下可保留 IWDG/RTC/LSI/LSE 等低速域功能；一旦 IWDG 启动，除 Reset 外不能停止；Stop 唤醒后系统时钟为 HSI。
- ST 官方 WDG 入门资料：IWDG 使用独立 LSI 时钟、12-bit downcounter 和 prescaler，独立于主时钟，可在 Stop/Standby 中工作；计数到 0 触发系统复位。
- 当前工程源码：
  - `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
  - `103 + 309/Project/Source/conf/Project_Config.h`
  - `103 + 309/Project/Source/conf/Project_BuildGuard.h`
  - `103 + 309/Project/Source/conf/conf.h`
  - `103 + 309/Project/Source/System_Init.c`
  - `103 + 309/Project/Source/System_Init.h`
  - `103 + 309/Project/Source/AppInit.c`
  - `103 + 309/Project/Source/Runtime.c`
  - `103 + 309/Project/Source/RTC.c`
  - `103 + 309/Project/Source/rtc_sleep.c`
  - `103 + 309/Project/Source/rtc_sleep_port.c`
  - `103 + 309/Project/Source/conf/conf.c`
  - `103 + 309/Project/Source/SleepDeal.c`
  - `103 + 309/Project/Source/Can_HDX.c`

## MCU 与构建档位

当前 Keil 工程目标是 STM32F1，不是 F0：

- `CommomSH367309_16series_103RCT6_C.uvprojx:10-17`：`FD_Release` 目标，`Device` 为 `STM32F103C8`。
- `CommomSH367309_16series_103RCT6_C.uvprojx:340`：Release 编译宏为 `STM32F10X_MD,USE_STDPERIPH_DRIVER`。
- `CommomSH367309_16series_103RCT6_C.uvprojx:947-954`：`FD_Debug` 目标同样是 `STM32F103C8`。
- `CommomSH367309_16series_103RCT6_C.uvprojx:1277`：Debug 额外定义 `PROJECT_CFG_BUILD_PROFILE=1,PROJECT_CFG_DEBUG_WATCH_ENABLE=1,_DEBUG_`。

结论：本项目当前 IWDG/RTC/Stop 低功耗行为应按 STM32F103/F1 标准外设库和 RM0008/AN2629 约束分析。

## 编译期开关

`Project_Config.h` 当前显式开启 IWDG 和 RTC：

- `Project_Config.h:16-18`：默认 `PROJECT_CFG_BUILD_PROFILE 0`，即量产 Release。
- `Project_Config.h:80-86`：`PROJECT_CFG_WDOG_ENABLE 1`，`PROJECT_CFG_RTC_ENABLE 1`。
- `conf.h:49-55`：`PROJECT_CFG_WDOG_ENABLE` 派生 `wdog_enable`；`PROJECT_CFG_RTC_ENABLE` 派生 `__FUNC_RTC__`。
- `Project_BuildGuard.h:260-264`：Release 档位下若 `PROJECT_CFG_WDOG_ENABLE` 不是 1，直接编译报错。

结论：当前量产配置下 IWDG 默认启用，且由于 RTC 默认启用，`Init_IWDG()` 走 RTC 兼容的长窗口配置。

## IWDG 初始化方式

初始化入口：

- `main.c:5-12`：`main()` 调用 `AppInit_Boot()` 后进入 `while(1)` 调用 `Runtime_RunOnce()`。
- `AppInit.c:7-54`：`AppInit_InitDevice()` 完成基础外设初始化，`#ifdef wdog_enable` 下调用 `Init_IWDG()`。
- `AppInit.c:66-72`：`AppInit_Boot()` 在 `AppInit_InitDevice()` 和运行态初始化后调用 `Init_RTC()`。

IWDG 具体配置：

- `System_Init.c:33-48`：`Init_IWDG()` 使能 PWR 时钟，打开 IWDG 写权限，配置 prescaler/reload，先 `IWDG_ReloadCounter()` 再 `IWDG_Enable()`。
- `System_Init.c:37-43`：若没有 `__FUNC_RTC__`，配置 `IWDG_Prescaler_64` + `IWDG_SetReload(800)`；若有 `__FUNC_RTC__`，配置 `IWDG_Prescaler_256` + `IWDG_SetReload(0x0FFF)`。
- `System_Init.c:288-291`：`IWDG_Feed()` 只封装 `IWDG_ReloadCounter()`。
- `System_Init.h:77-78`：`Feed_IWatchDog` 是 `IWDG_Feed()` 的兼容宏。

按源码注释使用的 40 kHz LSI 标称值估算：

| 配置路径 | 条件 | Prescaler | Reload | 标称超时 |
|---|---:|---:|---:|---:|
| 非 RTC | 未定义 `__FUNC_RTC__` | 64 | 800 | 约 1.28 s |
| 当前量产路径 | 已定义 `__FUNC_RTC__` | 256 | 4095 | 约 26.2 s |

当前实际路径是 `PROJECT_CFG_RTC_ENABLE=1 -> __FUNC_RTC__ -> 256/0x0FFF`，所以当前标称 IWDG 窗口约 26.2 秒。

注意：LSI 是 RC 时钟，标称 40 kHz 不能等同于最坏值。若按 60 kHz 估算，`4095*256/60000` 约 17.5 秒；若按 30 kHz 估算约 34.9 秒。低功耗安全设计必须按最快 LSI 估算最短 IWDG 超时，不能只按源码注释的 40 kHz。

## 当前喂狗位置

### 主循环后台喂狗

- `Runtime.c:32-42`：`Runtime_RunBackgroundTasks()` 在 `StorageFlash_AppUseTest_Task()`、`App_FlashUpdate()`、`App_LogRecord()`、`App_ProID_Deal()` 后执行 `Feed_IWatchDog`。
- `Runtime.c:44-49`：正常循环顺序是前台任务、IO/电源任务、后台任务。
- `Runtime.c:52-58`：`Runtime_RunOnce()` 每轮调用正常任务。

结论：正常运行态主要喂狗点在后台任务末尾。若前台任务、IO/电源任务或后台任务前半段长时间阻塞，主循环末尾喂狗会被推迟。

### 阻塞延时喂狗

- `System_Init.c:160-173`：`__delay_ms()` 轮询 SysTick 期间持续 `Feed_IWatchDog`。
- `System_Init.c:139-151`：`__delay_us()` 没有喂狗。

结论：ms 级阻塞延时一般不会导致 IWDG 误复位；us 级短延时不喂狗通常可接受。

### Stop 前后喂狗

- `rtc_sleep_port.c:118-123`：`RtcSleep_PortEnterStop()` 在 `Sys_StopMode()` 前后各喂一次狗。
- `conf.c:374-385`：`Sys_StopMode()` 关闭 TIM3，清唤醒 pending，调用 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`，返回后调用 `cpu_frequency_conf()`。
- `rtc_sleep.c:303-329`：`rtc_sleep_run_hiccup_cycle()` 每轮准备 RTC Stop、进入 Stop、醒来后恢复外设；如果没有异常，继续下一轮 RTC 周期睡眠。

结论：当前 HICCUP_MODE 是运行态不复位的 RTC Stop 周期休眠，IWDG 已经启动，必须依赖 RTC 周期小于 IWDG 最短超时。`RtcSleep_PortEnterStop()` 的睡前/醒后喂狗方向正确，但不能覆盖 RTC 未唤醒或周期过长的问题。

### CAN RTC 唤醒服务喂狗

- `Can_HDX.c:23-24`：`FEIDAO_CAN_RTC_PERIOD_SECONDS` 当前为 1 秒；`FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS` 为 150 个 10 ms tick。
- `Can_HDX.c:901-904`：`Can_GetIdleRtcPeriodSeconds()` 返回 `FEIDAO_CAN_RTC_PERIOD_SECONDS`。
- `Can_HDX.c:906-933`：`Can_RtcWakeService()` 打开 CAN 收发器电源、排队周期帧，在 `Can_IsBusy()` 等待循环内每 10 ms 喂狗一次，最多约 1.5 秒。

结论：当前 RTC 周期为 1 秒，CAN 唤醒服务最长约 1.5 秒且循环内喂狗，因此当前配置下 IWDG 误复位风险低。后续若把 RTC 周期改成几十秒，风险会立即上升。

### ADC/AFE/Flash/通信长等待喂狗

已发现的长等待或写入相关喂狗点：

- `ADC.c:183-212`：ADC reset calibration 和 calibration 等待循环中喂狗，且有 `ADC_CALIBRATION_WAIT_LOOP` 超时。
- `I2C_AFE1.c:243-265`：`TwiChkClkRelease()` 等待 I2C 时钟释放时喂狗，最长约 4 ms。
- `I2C_AFE1.c:550-578`：`MTPWrite()` 进入写循环前喂狗。
- `I2C_AFE1.c:584-613`：`MTPWriteROM()` 每个字节写入前喂狗，内部有 40 ms 延时。
- `I2C_AFE1.c:617-635`：`MTPRead()` 读前喂狗。
- `SH367309_Func.c:88-115`：`AFE_IsReady()` 循环内喂狗，最长约 50*20 ms。
- `SH367309_DataDeal.c:217-226`：写 AFE 参数前后喂狗。
- `SH367309_DataDeal.c:251-285`：`Sci_WrRegs_0x10_AFE_Parameters()` 更新参数前后喂狗，并调用 `AFE_SaveCurValuesToFlash()`。
- `SH367309_DataDeal.c:325-342`：AFE 参数恢复默认前后喂狗，并调用 `AFE_SaveCurValuesToFlash()`。
- `DataDeal.c:543-550`：启动电流零点采样前延时和循环内喂狗。
- `Sci_Upper.c:2263-2284`：阻塞式 `fputc()` 等待 USART TC 时喂狗，等待循环有 `SCI_DEBUG_UART_TX_WAIT_LOOP` 上限。

结论：代码在多个已知慢路径中主动喂狗，能降低误复位。但这些喂狗点也会掩盖部分“业务卡住但仍在低层等待循环喂狗”的故障，后续低功耗框架不应继续扩散无条件喂狗，建议集中保留在明确有超时的硬件等待路径。

## RTC 周期与 IWDG 的当前关系

RTC 唤醒周期当前来自 CAN 模块：

- `RTC.c:366-399`：`RTC_GetWakeupPeriodSeconds()` 读取 `Can_GetIdleRtcPeriodSeconds()`，0 则改成 1。
- `RTC.c:375-397`：函数内曾有按 IWDG 安全窗口裁剪 wake_seconds 的代码，但目前整段被注释。
- `RTC.c:408-418`：`RTC_WKTimeConfig()` 取 `RTC_GetWakeupPeriodSeconds()`，保存到 `s_u32RtcLastWakeupPeriodSeconds`，再通过 `RTC_EnableAlarmAfterSeconds(wake_seconds)` 配置 Alarm。
- `Can_HDX.c:23`：当前 `FEIDAO_CAN_RTC_PERIOD_SECONDS` 为 1 秒。

当前结论：

1. 当前配置下，RTC Stop 周期为 1 秒，远小于 IWDG 最坏估算 17.5 秒，安全。
2. `RTC_GetWakeupPeriodSeconds()` 已经没有 IWDG 裁剪保护，后续修改 `FEIDAO_CAN_RTC_PERIOD_SECONDS` 或改成上位机可配周期时，必须补回安全限制。
3. 如果 RTC Alarm 未能唤醒、EXTI17 pending/flag 处理错误、LSE/LSI 失效，HICCUP_MODE 会在 Stop 中等待到 IWDG 复位。这是需要接受并记录的安全兜底，但不应作为正常唤醒路径。

## Stop/Reset 两类休眠路径对 IWDG 的影响

当前项目存在两类低功耗路径：

### HICCUP_MODE：不复位，IWDG 继续运行

- `rtc_sleep.c:453-456`：HICCUP_MODE 下循环调用 `rtc_sleep_run_hiccup_cycle()`。
- `rtc_sleep.c:307-321`：每轮进入 Stop 后，醒来再恢复外设。
- `rtc_sleep_port.c:118-123`：Stop 前后喂狗。

结论：HICCUP_MODE 是当前最需要 IWDG 周期约束的路径。RTC 周期、唤醒后服务时间、时钟恢复时间、AFE/CAN 检查时间之和必须小于 IWDG 最短超时。

### NORMAL_MODE/DEEP_MODE：先复位，再在 IWDG 启动前 Stop

- `rtc_sleep.c:449-460`：NORMAL_MODE/DEEP_MODE 调用 `low_power_log_and_commit_sleep()`。
- `rtc_sleep_port.c:91-99`：`RtcSleep_PortCommitResetSleep()` 保存 CAN/日志/事件后调用 `SleepDeal_Continue()`。
- `SleepDeal.c:83-115`：`SleepDeal_Continue()` 写 BKP 休眠标志，AFE 进入 sleep，然后 `MCU_RESET()`。
- `main.h:42`：`MCU_RESET()` 是 `NVIC_SystemReset()`。
- `AppInit.c:15-17`：复位启动后，在 `Init_IWDG()` 前先调用 `IsSleepStartUp()`。
- `SleepDeal.c:186-230`：`IsSleepStartUp()` 根据休眠标志直接配置 IO/唤醒源并循环 `Sys_StopMode()`，直到按键/充电等唤醒条件有效。

结论：NORMAL_MODE/DEEP_MODE 当前实际是“复位式 Stop 休眠”。在软件启动 IWDG 的前提下，这条路径进入 Stop 前尚未调用 `Init_IWDG()`，所以不会被 App 软件 IWDG 周期限制。该结论依赖“未配置硬件 IWDG option byte”。当前源码搜索未发现 `FLASH_UserOptionByteConfig`、`OB_IWDG_SW/HW` 等修改 option byte 的业务代码；如果产线或外部工具把 IWDG 配成硬件启动，这条假设会失效。

## 调试态 IWDG 注意点

- `System_Init.c:21-30`：`EnableLowPowerDebug()` 在 `_DEBUG_` 下会设置 `DBG_SLEEP/DBG_STOP/DBG_STANDBY/DBG_IWDG_STOP/DBG_WWDG_STOP`。
- 当前源码搜索未发现 `EnableLowPowerDebug()` 调用。
- `AppInit.c:45-47`：Debug 下只调用 `DBGMCU_Config(DBGMCU_STOP, ENABLE)`，未冻结 IWDG。
- Debug 目标 `uvprojx:1277` 定义 `_DEBUG_`，但 `Project_Config.h:80-86` 仍默认开启 IWDG/RTC，除非另行覆盖。

结论：当前 Debug 下单步或断点停留超过 IWDG 窗口，仍可能触发 IWDG 复位。这个问题不影响量产运行，但会影响低功耗调试可重复性。

## 风险判断

### P0 风险

1. `RTC_GetWakeupPeriodSeconds()` 的 IWDG 安全裁剪被注释。当前 1 秒周期安全，但一旦后续把 RTC 周期改大，就可能在 HICCUP_MODE Stop 中被 IWDG 复位。依据：`RTC.c:366-399`、`Can_HDX.c:23`、`rtc_sleep_port.c:118-123`。
2. HICCUP_MODE 依赖 RTC Alarm 正常唤醒；若 RTC/EXTI17 配置错误，IWDG 会复位。依据：`RTC.c:408-418`、`RTC.c:518-537`、`rtc_sleep.c:303-329`。
3. 若硬件 option byte 被外部配置为硬件 IWDG，`SleepDeal.c` 的复位式睡眠路径会在 `IsSleepStartUp()` 早期 Stop 中仍受 IWDG 限制，当前源码没有对此做检测或文档化。依据：`SleepDeal.c:186-230`、`AppInit.c:15-17`、`AppInit.c:49-50`。

### P1 风险

1. 主循环喂狗点在 `Runtime_RunBackgroundTasks()` 末尾，前置任务若新增无超时长阻塞，仍可能误复位。依据：`Runtime.c:14-42`。
2. 多处底层等待循环喂狗会降低误复位概率，但也可能掩盖“底层一直等待、业务不前进”的问题。依据：`ADC.c:183-212`、`I2C_AFE1.c:243-265`、`SH367309_Func.c:88-115`。
3. Debug 下 `EnableLowPowerDebug()` 未调用，断点停留可能被 IWDG 复位。依据：`System_Init.c:21-30`、`AppInit.c:45-47`。

### P2 风险

1. IWDG 超时计算散落在注释代码中，没有统一宏或接口输出当前 IWDG 安全窗口。依据：`System_Init.c:37-43`、`RTC.c:375-397`。
2. 当前没有记录 IWDG 复位原因的专用上报路径，只能依赖 `RTC.c:463-474` 附近对 RCC reset flag 的旧注释逻辑，后续可补充系统监控字段。

## 当前结论

当前量产配置下，IWDG 启用、RTC 启用，IWDG 标称窗口约 26.2 秒；当前 RTC Stop 周期为 1 秒，醒后 CAN RTC 服务最多约 1.5 秒且服务内喂狗，短期不会因为正常 RTC 周期导致误复位。

最大隐患不是当前 1 秒周期，而是 `RTC_GetWakeupPeriodSeconds()` 中 IWDG 安全裁剪已被注释，导致后续周期改大时没有自动保护。第一阶段建议只记录，不改源码；第三阶段最小实现时应把 IWDG 安全窗口纳入 `LP_CanSleep()` 或 `LP_SetWakeupPeriod()` 的统一判断。

# 01 main 启动流程

状态：已按BMS主 uvprojx 目标源码验证主入口和初始化顺序。参考源码：`103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s`、`103 + 309/Project/Source/main.c`、`AppInit.c`、`Runtime.c`。

## 目录
- [真实入口顺序](#真实入口顺序)
- [main函数](#main函数)
- [AppInit_Boot展开](#appinit_boot展开)
- [运行态进入点](#运行态进入点)
- [函数逐项索引](#函数逐项索引)
- [不确定项](#不确定项)

## 真实入口顺序

1. 上电或复位后，向量表 `__Vectors` 的第二项进入 `Reset_Handler`。
2. `Reset_Handler` 先 `BLX SystemInit`，再跳转到 ARM C runtime `__main`。
3. `__main` 完成 C 运行环境初始化后调用业务 `main()`。
4. `main()` 调用 `AppInit_Boot()`。
5. `main()` 进入 `while (1)`，每圈调用 `Runtime_RunOnce()`。

```c
int main(void)
{
    AppInit_Boot();

    while (1)
    {
        Runtime_RunOnce();
    }
}
```

## main函数

| 项目 | 结论 |
| --- | --- |
| 所在文件 | `103 + 309/Project/Source/main.c:5` |
| 输入参数 | 无 |
| 返回值 | 理论上不返回 |
| 直接读全局 | 无 |
| 直接写全局 | 无 |
| 直接下级调用 | `AppInit_Boot()`、`Runtime_RunOnce()` |
| 硬件影响 | 自身无直接寄存器访问；影响均来自下级初始化和主循环任务 |
| 阻塞点 | `AppInit_Boot()` 内可能因 sleep-startup、AFE ready、ADC校准、电流零点校准、Flash/RTC等等待阻塞 |
| 宏依赖 | `main.c` 自身无条件编译；下级依赖大量 `PROJECT_CFG_*` 和 `conf.h` 派生宏 |


## AppInit_Boot展开

实际顺序如下，注意 `AppInit_InitSci()` 是宏，展开为 `InitUSART_CommonUpper()`；`Init_IWDG()` 当前在源码中被注释，虽然 `PROJECT_CFG_WDOG_ENABLE=1`，启动阶段不会调用 `Init_IWDG()`。

```text
AppInit_Boot
├── DebugWatch_BindAll
├── AppInit_InitDevice
│   ├── IrqDebug_SetPhase(BOOT)
│   ├── SystemInit
│   ├── InitDelay
│   ├── IsSleepStartUp
│   ├── jtag_disableAndConfIO
│   ├── InitNVIC
│   ├── InitIO
│   ├── AppInit_InitSci -> InitUSART_CommonUpper
│   ├── InitE2PROM
│   ├── InitAFE1
│   ├── InitCan
│   ├── InitADC
│   ├── InitData_SOC
│   ├── InitTimer
│   ├── __enable_irq
│   ├── IrqDebug_SetPhase(RUN)
│   └── EnableLowPowerDebug
├── AppInit_InitRuntimeState
└── Init_RTC
```

## 运行态进入点

`Runtime_RunOnce()` 不是抢占式调度器，而是 cooperative 主循环。每圈按固定顺序跑三段：前段任务、IO/低功耗任务、后台任务。`TIM3_IRQHandler()` 只负责置 pending 标志；周期任务在主循环里消费这些标志。

## 函数逐项索引

| 函数 | 定义位置 | 主要作用 | 下级调用 | 写全局 | 读全局 |
| --- | --- | --- | --- | --- | --- |
| main | 103 + 309/Project/Source/main.c:4 | BMS主固件入口；启动初始化后进入 cooperative while(1) 调度。 | AppInit_Boot, Runtime_RunOnce |  |  |
| AppInit_Boot | 103 + 309/Project/Source/AppInit.c:49 | 绑定DebugWatch，完成硬件初始化、运行态初始化和RTC初始化。 | DebugWatch_BindAll, AppInit_InitDevice, AppInit_InitRuntimeState, Init_RTC |  |  |
| AppInit_InitDevice | 103 + 309/Project/Source/AppInit.c:7 | 按固定顺序初始化SystemInit/Delay/Sleep/GPIO/NVIC/SCI/参数/AFE/CAN/ADC/SOC/TIM3/IRQ。 | IrqDebug_SetPhase, SystemInit, InitDelay, IsSleepStartUp, jtag_disableAndConfIO, InitNVIC, InitIO, AppInit_InitSci, InitE2PROM, InitAFE1, InitCan, InitADC, InitData_SOC, InitTimer, __enable_irq, EnableLowPowerDebug |  |  |
| DebugWatch_BindAll | 103 + 309/Project/Source/DebugWatch.c:33 | BMS目标内函数；作用见定义文件和下级调用。 | ADC_DebugWatchBind, DataDeal_DebugWatchBind, Can_DebugWatchBind, LedBar_DebugWatchBind, SleepDeal_DebugWatchBind, Flash_DebugWatchBind, LogRecord_DebugWatchBind, FactoryAging_DebugWatchBind, RTC_DebugWatchBind, Runtime_DebugWatchBind, Sci_DebugWatchBind, I2C_AFE1_DebugWatchBind, SH367309Data_DebugWatchBind, SH367309Func_DebugWatchBind, Fault_DebugWatchBind, ProductionID_DebugWatchBind, CanFeidaoFrames_DebugWatchBind, SystemInit_DebugWatchBind, SystemMonitor_DebugWatchBind, SocEnhance_DebugWatchBind, SystemDebug_DebugWatchBind | sleep, soc | OtherElement, g_stCellInfoReport, g_stLowPowerRtcStatus |
| SystemInit | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c:195 | BMS目标内函数；作用见定义文件和下级调用。 | SystemInit_ExtMemCtl, SetSysClock |  |  |
| InitDelay | 103 + 309/Project/Source/System_Init.c:153 | BMS目标内函数；作用见定义文件和下级调用。 |  | fac_ms, fac_us |  |
| IsSleepStartUp | 103 + 309/Project/Source/SleepDeal.c:212 | BMS目标内函数；作用见定义文件和下级调用。 | BootFlag_Read, BootFlag_Clear, Init_RTC, IOstatus_RTCMode, InitWakeUp_RTCMode, IrqDebug_SetPhase, Sys_StopMode, IsSleepWakeupValid, IORecover_RTCMode, IOstatus_NormalMode, InitWakeUp_NormalMode, IORecover_NormalMode, IOstatus_DeepMode, InitWakeUp_DeepMode, IORecover_DeepMode | boot_sleep, chg_wake, s_sleep |  |
| jtag_disableAndConfIO | 103 + 309/Project/Source/PubFunc.c:221 | BMS目标内函数；作用见定义文件和下级调用。 | RCC_APB2PeriphClockCmd, GPIO_PinRemapConfig |  |  |
| InitNVIC | 103 + 309/Project/Source/System_Init.c:74 | BMS目标内函数；作用见定义文件和下级调用。 | NVIC_PriorityGroupConfig |  |  |
| InitIO | 103 + 309/Project/Source/conf/conf.c:164 | BMS目标内函数；作用见定义文件和下级调用。 | RCC_APB2PeriphClockCmd, Conf_InitGpioMode, Conf_InitRunSharedIo |  |  |
| InitUSART_CommonUpper | 103 + 309/Project/Source/Sci_Upper.c:2072 | BMS目标内函数；作用见定义文件和下级调用。 | InitSCI1_CommonUpper, InitSCI2_CommonUpper, InitSCI3_CommonUpper |  |  |
| InitE2PROM | 103 + 309/Project/Source/EEPROM.c:297 | 从Flash storage加载保护/OtherElement/AFE/日志参数并执行升级策略。 | EEPROM_LoadDefaultRuntimeData, EEPROM_LoadRWParametersFromFlash, ReadEEPROM_AFE_Parameters, ReadEEPROM_EventRecord_Parameters, UpgradeParamPolicy_ApplyOnce |  |  |
| InitAFE1 | 103 + 309/Project/Source/I2C_AFE1.c:670 | 初始化SH367309 AFE并执行电流零点/初始MOS策略。 | AfeCurrent_GetSeq, AfeCurrent_IsStartupZeroDone, initAFE1_IIC, close_ctlc, AfeCurrent_SetStartupColdBoot, SleepDeal_IsBootFromSleepStartup, AfeCurrent_PrepareStartupZero, AFE_Reset, AFE_IsReady, SH367309_UpdataAfeConfig, MosStartup_ApplyInitialState, AfeCurrent_StartupZeroCal, open_ctlc |  |  |
| InitCan | 103 + 309/Project/Source/Can_HDX.c:838 | 初始化CAN运行态、GPIO、NVIC、CAN1和滤波器。 | feidao_can_clear_tx_queue, feidao_can_clear_app_cmd_queue, InitCan_GPIO, InitCan_NVIC, InitCan_CAN1, InitCan_Filter, feidao_can_power_on | enter_iap_delay_ticks, mailbox, mailbox_source, read_block_active, s_app, s_runtime, s_tx, schedule_init |  |
| InitADC | 103 + 309/Project/Source/ADC.c:427 | 初始化ADC运行态、GPIO、TIM2、DMA1和ADC1校准。 | ADC_ClearTypeCOutCurrent, ADC_ResetAnlogCalSchedule, InitADC_GPIO, InitADC_TIMER, InitADC_DMA, InitADC_ADC1 | result, s_adc, vbat |  |
| InitData_SOC | 103 + 309/Project/Source/SOC.c:75 | BMS目标内函数；作用见定义文件和下级调用。 | SOC_LoadConfigData, soc_param_lib_init |  |  |
| InitTimer | 103 + 309/Project/Source/System_Init.c:124 | 配置TIM3 10ms tick。 | RCC_APB1PeriphClockCmd, TIM_Cmd, TIM_ITConfig, Timer_GetPrescalerFor100kHz, TIM_TimeBaseInit, TIM_SetCounter, TIM_ClearITPendingBit, NVIC_Init, SysTime_ResetCounters |  |  |
| EnableLowPowerDebug | 103 + 309/Project/Source/System_Init.c:45 | BMS目标内函数；作用见定义文件和下级调用。 |  |  |  |
| AppInit_InitRuntimeState | 103 + 309/Project/Source/AppInit.c:37 | 初始化系统状态、版本、LED、生产信息和启动日志请求。 | InitSystemMonitorData_EEPROM, SystemRuntime_MarkBootReady, SystemRuntime_SetProjectVersion, LedBar_Init, InitProID, LogRecord_RequestStartup | g_u32CS_Res_AFE | OtherElement |
| InitSystemMonitorData_EEPROM | 103 + 309/Project/Source/System_Monitor.c:126 | BMS目标内函数；作用见定义文件和下级调用。 |  | s_system_onoff_func, s_system_status |  |
| SystemRuntime_MarkBootReady | 103 + 309/Project/Source/System_Monitor.c:132 | BMS目标内函数；作用见定义文件和下级调用。 |  | s_system_status |  |
| SystemRuntime_SetProjectVersion | 103 + 309/Project/Source/System_Monitor.c:138 | BMS目标内函数；作用见定义文件和下级调用。 |  | s_system_status |  |
| LedBar_Init | 103 + 309/Project/Source/LedBar.c:1050 | BMS目标内函数；作用见定义文件和下级调用。 | LedBar_FrameClear, LedBar_GpioInitForDisplay, LedBar_OutputOff, LedBar_GpioPrepareForStop | blank, indicator_mask, key_active, key_filter_initialized, key_hold_10ms, key_last_pressed, key_long_handled, key_off_10ms, key_on_10ms, key_press_start_10ms, mcu_wk_active, mcu_wk_filter_initialized, mcu_wk_off_10ms, mcu_wk_on_10ms, number, s_ledbar | frame |
| InitProID | 103 + 309/Project/Source/ProductionID.c:28 | BMS目标内函数；作用见定义文件和下级调用。 | InitProID_DefaultData |  |  |
| LogRecord_RequestStartup | 103 + 309/Project/Source/LogRecord.c:81 | BMS目标内函数；作用见定义文件和下级调用。 |  | flags, s_log_record |  |
| Init_RTC | 103 + 309/Project/Source/RTC.c:472 | BMS目标内函数；作用见定义文件和下级调用。 | RTC_EnableBackupAccess, BKP_ReadBackupRegister, RTC_ClockConfig, RTC_TimeConfig, BKP_WriteBackupRegister, RCC_GetFlagStatus, RCC_ClearFlag, RTC_ClearAlarmPending, RTC_AlarmConfig, RTC_NVIC_Config |  |  |
| Runtime_RunOnce | 103 + 309/Project/Source/Runtime.c:76 | 执行一次主循环调度。 | Runtime_RunNormalOnce |  |  |


## 不确定项

- `SystemInit()` 被启动文件和 `AppInit_InitDevice()` 各调用一次；是否需要业务层再次调用属于设计确认项，本文只记录当前事实。
- `Init_IWDG()` 在源码中被注释，`Runtime_RunBackgroundTasks()` 仍执行 `Feed_IWatchDog` 宏；若硬件IWDG从未启用，喂狗只是对寄存器写入。需结合Keil实际构建/启动观测确认。
- `Project_BuildGuard.h` 当前对 release profile 与 debug 宏有强约束；工作区中该文件有未提交改动，本文以当前文件内容为准。

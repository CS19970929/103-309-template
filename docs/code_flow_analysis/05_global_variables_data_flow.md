# 05 全局变量与数据流

状态：静态变量索引 + 关键变量人工分类。参考源码覆盖 `Source/*.c/*.h` 及主目标中断文件。单位来自源码运算和字段命名，未能从代码确认的单位标记为“需确认”。

## 目录
- [数据流总览](#数据流总览)
- [关键全局变量分组](#关键全局变量分组)
- [全量变量索引](#全量变量索引)
- [高耦合变量](#高耦合变量)

## 数据流总览

```text
TIM3 ISR -> s_st_SysTimePending -> SysTime_LatchTaskFlags -> g_st_SysTimeFlag
AFE MTP/CADC -> SH367309_Read_AFE1 / SH367309_Reg_Store -> DataLoad_* -> g_stCellInfoReport
g_stCellInfoReport + ADC TypeC -> App_SOC -> SOC_Enhance_Element / s_soc -> g_stCellInfoReport.SocElement
SCI/CAN write -> EEPROM/OtherElement/PRT_E2ROMParas/AFE_PARAM_WRITE_Flag/SOC requests/u8FlashUpdateFlag
rtc_sleep -> g_stLowPowerRtcStatus -> SleepDeal/RTC/LowPowerSleep/AFE sleep/reset
Fault/SystemMonitor -> System_ErrFlag / s_system_status -> SCI/CAN report/log/low-power blocker
```

## 关键全局变量分组

### 系统/tick/调度

| 变量 | 声明摘录 | 文件 | 行 | 范围 | volatile | 初始值 | 写入者 | 读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s_st_SysTimePending | static volatile union SYS_TIME s_st_SysTimePending | 103 + 309/Project/Source/System_Init.c | 6 | static | volatile | 见源码/默认0 | SysTime_LatchTaskFlags, SysTime_Post10msTick, SysTime_ResetCounters | SysTime_HasPendingTaskFlags, SystemInit_DebugWatchBind |
| s_u32Sys10msTickCount | static volatile UINT32 s_u32Sys10msTickCount = 0U | 103 + 309/Project/Source/System_Init.c | 7 | static | volatile | 0U | SysTime_Post10msTick, SysTime_ResetCounters | SysTime_Get10msTickCount, SystemInit_DebugWatchBind |
| s_u8Cnt50ms | static UINT8 s_u8Cnt50ms = 0 | 103 + 309/Project/Source/System_Init.c | 8 | static |  | 0 | SysTime_Post10msTick, SysTime_ResetCounters | SystemInit_DebugWatchBind |
| s_u8Cnt100ms | static UINT8 s_u8Cnt100ms = 0 | 103 + 309/Project/Source/System_Init.c | 10 | static |  | 0 | SysTime_Post10msTick, SysTime_ResetCounters | SystemInit_DebugWatchBind |
| s_u8Cnt200ms | static UINT8 s_u8Cnt200ms = 0 | 103 + 309/Project/Source/System_Init.c | 11 | static |  | 0 | SysTime_Post10msTick, SysTime_ResetCounters | SystemInit_DebugWatchBind |
| s_u8Cnt1000ms | static UINT8 s_u8Cnt1000ms = 0 | 103 + 309/Project/Source/System_Init.c | 12 | static |  | 0 | SysTime_Post10msTick, SysTime_ResetCounters | SystemInit_DebugWatchBind |
| sys_time | extern Time_T sys_time | 103 + 309/Project/Source/conf/conf.h | 129 | global |  | 见源码/默认0 |  |  |
| sys_time | extern Time_T sys_time | build/host_tests/conf_board_self_0/conf.h | 129 | global |  | 见源码/默认0 |  |  |
| sys_time | extern Time_T sys_time | build/host_tests/conf_board_self_1000/conf.h | 129 | global |  | 见源码/默认0 |  |  |
| s_system_status | static volatile union System_Status s_system_status | 103 + 309/Project/Source/System_Monitor.c | 5 | static | volatile | 见源码/默认0 | InitSystemMonitorData_EEPROM, SystemRuntime_MarkBootReady, SystemRuntime_SetAfeStatus, SystemRuntime_SetMosStatus, SystemRuntime_SetProjectVersion | SystemMonitor_DebugWatchBind, SystemRuntime_GetStatusSnapshot, SystemRuntime_IsChargeMosOpen, SystemRuntime_IsDischargeMosOpen |
| s_system_onoff_func | static volatile union System_OnOFF_Function s_system_onoff_func | 103 + 309/Project/Source/System_Monitor.c | 4 | static | volatile | 见源码/默认0 | InitSystemMonitorData_EEPROM, SystemFeature_SetById | SystemFeature_GetMask, SystemFeature_IsSocFixed, SystemFeature_IsSocZero, SystemMonitor_DebugWatchBind |
| System_ErrFlag | extern volatile struct SYSTEM_ERROR System_ErrFlag | 103 + 309/Project/Source/System_Monitor.h | 212 | global | volatile | 见源码/默认0 | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi |
| System_ErrFlag | volatile struct SYSTEM_ERROR System_ErrFlag | tools/soc_host_c_test.c | 18 | global | volatile | 见源码/默认0 | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi |
| System_ErrFlag | volatile struct SYSTEM_ERROR System_ErrFlag | tools/soc_host_visual_trace.c | 14 | global | volatile | 见源码/默认0 | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi |
### BMS采样/电芯/温度/电流

| 变量 | 声明摘录 | 文件 | 行 | 范围 | volatile | 初始值 | 写入者 | 读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| g_stCellInfoReport | extern struct stCell_Info g_stCellInfoReport | 103 + 309/Project/Source/Sci_Upper.h | 482 | global |  | 见源码/默认0 |  |  |
| Registers_AFE1 | extern AFEDATA Registers_AFE1 | 103 + 309/Project/Source/I2C_AFE1.h | 93 | global |  | 见源码/默认0 |  |  |
| g_u16CalibCoefK | UINT16 g_u16CalibCoefK[KB_NUM] | 103 + 309/Project/Source/DataDeal.c | 72 | global |  | 见源码/默认0 | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali |
| g_i16CalibCoefB | INT16 g_i16CalibCoefB[KB_NUM] | 103 + 309/Project/Source/DataDeal.c | 74 | global |  | 见源码/默认0 | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali |
| g_i16CalibCoefB | extern INT16 g_i16CalibCoefB[KB_NUM] | 103 + 309/Project/Source/DataDeal.h | 199 | global |  | 见源码/默认0 | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali |
| g_u32CS_Res_AFE | UINT32 g_u32CS_Res_AFE | 103 + 309/Project/Source/DataDeal.c | 75 | global |  | 见源码/默认0 | AppInit_InitRuntimeState, DataLoad_CurrentMilliAmpToRaw, DataLoad_CurrentRawToMilliAmp, EEPROM_UpdateOtherElementRuntime, Refresh_Parameters, Sci_ApplyOtherElementSideEffects | DataDeal_DebugWatchBind, InitShortCur |
| g_u32CS_Res_AFE | extern UINT32 g_u32CS_Res_AFE | 103 + 309/Project/Source/DataDeal.h | 201 | global |  | 见源码/默认0 | AppInit_InitRuntimeState, DataLoad_CurrentMilliAmpToRaw, DataLoad_CurrentRawToMilliAmp, EEPROM_UpdateOtherElementRuntime, Refresh_Parameters, Sci_ApplyOtherElementSideEffects | DataDeal_DebugWatchBind, InitShortCur |
| s_adc | static ADC_RUNTIME s_adc | 103 + 309/Project/Source/ADC.c | 13 | static |  | 见源码/默认0 | ADC_ClearTypeCOutCurrent, ADC_ResetAnlogCalSchedule, ADC_StopForLowPower, ADC_UpdateMosTemp, ADC_UpdateTypeCCurrent, ADC_UpdateVbc, App_AnlogCal, InitADC | ADC_DebugWatchBind, ADC_GetRaw, ADC_GetResult, ADC_GetTypeCOutCurrentMilliAmp, ADC_GetVbatMilliVolt, ADC_IsReady, InitADC_DMA |
### SOC/SOH

| 变量 | 声明摘录 | 文件 | 行 | 范围 | volatile | 初始值 | 写入者 | 读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SOC_Enhance_Element | extern struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element | 103 + 309/Project/Source/SocEnhance.h | 107 | global |  | 见源码/默认0 |  |  |
| s_soc | static SOC_STATE s_soc | 103 + 309/Project/Source/SocEnhance.c | 141 | static |  | 见源码/默认0 | SOC_IntEnhance_Ctrl, soc_add_discharge, soc_apply_discharge_delta, soc_apply_full_empty, soc_apply_long_rest_down_step, soc_apply_rtc_rest_ocv, soc_clear_rest_down_target, soc_from_cap, soc_handle_command, soc_integrate, soc_load_or_default, soc_param_lib_init, soc_refresh_capacity_base, soc_reset_rest_confidence | SOC_ApplyRtcRelaxationCompensation, SOC_GetDebugInternals, SocEnhance_DebugWatchBind, soc_apply_ocv_target_step, soc_apply_tail_step, soc_display_target, soc_export_public_fields, soc_full_confirm_seconds, soc_sag_hold_blocks_calibration, soc_save, soc_save_mark_changed, soc_update_save_mark, soc_watch_refresh |
| s_saved_soc | static SOC_SAVE_MARK s_saved_soc | 103 + 309/Project/Source/SocEnhance.c | 143 | static |  | 见源码/默认0 | soc_update_save_mark | SocEnhance_DebugWatchBind, soc_save_mark_changed |
| s_u32SocRtcRestAppliedSeconds | static UINT32 s_u32SocRtcRestAppliedSeconds | 103 + 309/Project/Source/SocEnhance.c | 144 | static |  | 见源码/默认0 | soc_apply_rtc_rest_ocv, soc_param_lib_init | SocEnhance_DebugWatchBind |
### MOS/AFE保护

| 变量 | 声明摘录 | 文件 | 行 | 范围 | volatile | 初始值 | 写入者 | 读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SH367309_Reg_Store | extern SH367309_REG_STORE SH367309_Reg_Store | 103 + 309/Project/Source/SH367309_Func.h | 239 | global |  | 见源码/默认0 |  |  |
| AFE_ROM_PARAMETERS_Struction | AFE_ROM_PARAMETERS_TypeDef AFE_ROM_PARAMETERS_Struction = {0} | 103 + 309/Project/Source/SH367309_DataDeal.c | 14 | global |  | {0} | InitShortCur, Refresh_Parameters | SH367309Data_DebugWatchBind, SH367309_UpdataAfeConfig, Write_Parameters |
| AFE_Parameters_RS485_Struction | static AFE_Parameters_RS485_Typedef AFE_Parameters_RS485_Struction = AFE_PARAMETERS_RS485_STRUCTION_DEFAULT | 103 + 309/Project/Source/SH367309_DataDeal.c | 16 | static |  | AFE_PARAMETERS_RS485_STRUCTION_DEFAULT |  | AFE_CopyCurValues, AFE_RestoreCurValues, EEPROM_ResetData_AFE_ParametersToDefault, ReadEEPROM_AFE_Parameters, Refresh_Parameters, SH367309Data_DebugWatchBind, Sci_ACK_0x03_RW_AFE_Parameters, Sci_WrRegs_0x10_AFE_Parameters |
| AFE_PARAM_WRITE_Flag | extern int AFE_PARAM_WRITE_Flag | 103 + 309/Project/Source/SH367309_DataDeal.h | 245 | global |  | 见源码/默认0 |  |  |
| Fault_Flag_Fisrt | extern union FAULT_FLAG_FIRST Fault_Flag_Fisrt | 103 + 309/Project/Source/Fault.h | 381 | global |  | 见源码/默认0 |  |  |
| Fault_Flag_Second | extern union FAULT_FLAG_SECOND Fault_Flag_Second | 103 + 309/Project/Source/Fault.h | 382 | global |  | 见源码/默认0 |  |  |
| Fault_Flag_Third | extern union FAULT_FLAG_THIRD Fault_Flag_Third | 103 + 309/Project/Source/Fault.h | 383 | global |  | 见源码/默认0 |  |  |
### 通信/SCI/Modbus

| 变量 | 声明摘录 | 文件 | 行 | 范围 | volatile | 初始值 | 写入者 | 读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| g_stSciPort1 | static struct SCI_PORT_RUNTIME g_stSciPort1 = { USART1, &g_stCurrentMsgPtr_SCI1, &g_stSciModbusProtocolOps, &gu16_CommuErrCnt_SCI1, &gu8_TxEnable_SCI1, &gu8_TxFinishFlag_SCI1, 0, 0, 0, 0} | 103 + 309/Project/Source/Sci_Upper.c | 106 | static |  | { USART1, &g_stCurrentMsgPtr_SCI1, &g_stSciModbusProtocolOps, &gu16_CommuErrCnt_SCI1, &gu8_TxEnable_SCI1, &gu8_Tx |  | App_CommonUpper, InitSCI1_CommonUpper, Sci1_CommonUpper_IRQHandler, Sci_DebugWatchBind, Sci_IsAnyPortBusy |
| gu16_CommuErrCnt_SCI1 | static UINT16 gu16_CommuErrCnt_SCI1 = 0 | 103 + 309/Project/Source/Sci_Upper.c | 5 | static |  | 0 |  | Sci_DebugWatchBind |
| gu8_TxEnable_SCI1 | static UINT8 gu8_TxEnable_SCI1 = 0 | 103 + 309/Project/Source/Sci_Upper.c | 6 | static |  | 0 |  | Sci_DebugWatchBind |
| u8FlashUpdateFlag | UINT8 u8FlashUpdateFlag = 0 | 103 + 309/Project/Source/Sci_Upper.c | 26 | global |  | 0 | App_FlashUpdate, Sci_PortFinishTx, feidao_can_service_enter_iap_delay | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot |
| u8FlashUpdateFlag | extern UINT8 u8FlashUpdateFlag | 103 + 309/Project/Source/Sci_Upper.h | 478 | global |  | 见源码/默认0 | App_FlashUpdate, Sci_PortFinishTx, feidao_can_service_enter_iap_delay | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot |
| u8FlashUpdateE2PROM | UINT8 u8FlashUpdateE2PROM = 0 | 103 + 309/Project/Source/Sci_Upper.c | 27 | global |  | 0 | Sci_PortFinishTx, Sci_WrRegs_0x10_FlashConnect | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot |
| u8FlashUpdateE2PROM | extern UINT8 u8FlashUpdateE2PROM | 103 + 309/Project/Source/Sci_Upper.h | 481 | global |  | 见源码/默认0 | Sci_PortFinishTx, Sci_WrRegs_0x10_FlashConnect | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot |
### CAN

| 变量 | 声明摘录 | 文件 | 行 | 范围 | volatile | 初始值 | 写入者 | 读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s_tx | static FeidaoCanTxRuntime s_tx = { {0}, 0U, 0U, 0U, CAN_TxStatus_NoMailBox, FEIDAO_CAN_TX_SOURCE_NONE, 0U } | 103 + 309/Project/Source/Can_HDX.c | 92 | static |  | { {0}, 0U, 0U, 0U, CAN_TxStatus_NoMailBox, FEIDAO_CAN_TX_SOURCE_NONE, 0U } | InitCan, can_has_sleep_blocking_work, feidao_can_abort_tx, feidao_can_clear_tx_queue, feidao_can_dequeue_tx, feidao_can_enqueue_tx, feidao_can_service_tx | Can_DebugWatchBind, Can_GetDebugSnapshot, can_has_pending_work, feidao_can_queue_has_request, feidao_can_service_read_block_stream, respond |
| s_runtime | static FeidaoCanRuntime s_runtime | 103 + 309/Project/Source/Can_HDX.c | 102 | static |  | 见源码/默认0 | App_Can, InitCan, feidao_can_schedule_periodic | Can_DebugWatchBind, feidao_can_start_read_block_stream |
| s_app | static FeidaoCanAppRuntime s_app | 103 + 309/Project/Source/Can_HDX.c | 103 | static |  | 见源码/默认0 | InitCan, feidao_can_clear_app_cmd_queue, feidao_can_handle_app_cmd_data, feidao_can_queue_app_cmd, feidao_can_service_enter_iap_delay, feidao_can_service_read_block_stream, feidao_can_start_read_block_stream, feidao_can_stop_read_block_stream, feidao_can_take_app_cmd | Can_DebugWatchBind, can_has_pending_work, can_has_sleep_blocking_work |
### Flash/日志/老化/生产信息

| 变量 | 声明摘录 | 文件 | 行 | 范围 | volatile | 初始值 | 写入者 | 读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s_log_record | static LogRecordRuntime s_log_record | 103 + 309/Project/Source/LogRecord.c | 18 | static |  | 见源码/默认0 | App_LogRecord, EEPROM_ResetData_EventRecord_ToDefault, LogEvent_EEPROM, LogEvent_Record, LogRecord_MarkEventSaved, LogRecord_RequestSleep, LogRecord_RequestStartup, ReadEEPROM_EventRecord_Parameters | LogRecord_CanSaveEvent, LogRecord_DebugWatchBind, Sci_ACK_0x03_ReadRegs_EventRecord |
| su32_Interval_S_Tcnt | UINT32 su32_Interval_S_Tcnt = 0 | 103 + 309/Project/Source/LogRecord.c | 16 | global |  | 0 | App_LogRecord, RtcSleep_PortAddRuntimeSeconds | LogRecord_DebugWatchBind, RtcSleep_PortCommitResetSleep |
| su32_Interval_S_Tcnt | extern UINT32 su32_Interval_S_Tcnt | 103 + 309/Project/Source/LogRecord.h | 44 | global |  | 见源码/默认0 | App_LogRecord, RtcSleep_PortAddRuntimeSeconds | LogRecord_DebugWatchBind, RtcSleep_PortCommitResetSleep |
| s_factory_aging | static FactoryAgingRuntime s_factory_aging = { FACTORY_AGING_STATE_UNINIT, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, FACTORY_AGING_MOS_MODE_UNKNOWN } | 103 + 309/Project/Source/FactoryAging.c | 41 | static |  | { FACTORY_AGING_STATE_UNINIT, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, FACTORY_AGING_MOS_MODE_UNKNOWN } | FactoryAging_AddRunningTicks, FactoryAging_ApplyRunningMos, FactoryAging_EnterRunningFromHost, FactoryAging_Finish, FactoryAging_GetRemainingSeconds, FactoryAging_IsActive, FactoryAging_LoadDurationFromData, FactoryAging_LoadRuntimeStateForHost, FactoryAging_LoadStoredProgress, FactoryAging_MarkDone, FactoryAging_ResetMosCache, FactoryAging_ResetTimeByHost, FactoryAging_SaveBkp, FactoryAging_SaveStoredProgress | FactoryAging_DebugWatchBind, FactoryAging_GetDuration10ms, FactoryAging_GetState, FactoryAging_SaveProgressBeforeSleep |
| ProductionInfor | extern PRODUCTION_ID_INFO ProductionInfor | 103 + 309/Project/Source/ProductionID.h | 19 | global |  | 见源码/默认0 |  |  |
### LED/低功耗/RTC

| 变量 | 声明摘录 | 文件 | 行 | 范围 | volatile | 初始值 | 写入者 | 读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s_ledbar | static LedBarRuntime s_ledbar = { 0u, 0u, 1u, 0u, LEDBAR_ICON_PERCENT_MASK, } | 103 + 309/Project/Source/LedBar.c | 159 | static |  | { 0u, 0u, 1u, 0u, LEDBAR_ICON_PERCENT_MASK, } | APP_LedBar, LedBar_ApplyFrame, LedBar_Clear, LedBar_EnsureInit, LedBar_Init, LedBar_IsActiveForLowPower, LedBar_PrepareForStop, LedBar_RequestSocDisplayWindow, LedBar_RequestStartupDisplayWindow, LedBar_Scan1ms, LedBar_ScanTimerInit, LedBar_ServiceMcuWakeFilter, LedBar_ServiceStartupDisplayWindow, LedBar_ServiceSwitch | LedBar_BuildCurrentFrame, LedBar_DebugWatchBind, LedBar_GetDebugSnapshot, LedBar_IsDisplayRequested, LedBar_IsMcuWakeActive, LedBar_SetIndicatorState |
| g_stLowPowerRtcStatus | volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus = { NO_SLEEP, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U} | 103 + 309/Project/Source/rtc_sleep.c | 14 | global | volatile | { NO_SLEEP, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U} | LP_GetBlockReason, LP_RecordLastSleepSeconds, LowPower_Request, lp_deep, lp_idle, lp_select, lp_sync, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | DbgPrint_Wakeup, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, DebugWatch_BindAll, LP_GetLastSleepSeconds, SystemDebug_Snapshot, rtc_sleep |
| g_stLowPowerRtcStatus | extern volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus | 103 + 309/Project/Source/rtc_sleep.h | 54 | global | volatile | 见源码/默认0 | LP_GetBlockReason, LP_RecordLastSleepSeconds, LowPower_Request, lp_deep, lp_idle, lp_select, lp_sync, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | DbgPrint_Wakeup, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, DebugWatch_BindAll, LP_GetLastSleepSeconds, SystemDebug_Snapshot, rtc_sleep |
| g_irq_t | extern enum irqWakeup g_irq_t | 103 + 309/Project/Source/rtc_sleep.h | 56 | global |  | 见源码/默认0 |  |  |
| s_sleep | static SLEEP_RUNTIME s_sleep | 103 + 309/Project/Source/SleepDeal.c | 12 | static |  | 见源码/默认0 | IsSleepStartUp, SleepDeal_IsBootFromSleepChargerWakeup, SleepDeal_MarkBootFromSleepChargerWakeup | SleepDeal_DebugWatchBind, SleepDeal_GetExternalCommCounter, SleepDeal_IsBootFromSleepStartup, SleepDeal_RecordExternalComm |
| s_rtc | static RTC_RUNTIME s_rtc = { 0U, false, {0}, 1U, 0U } | 103 + 309/Project/Source/RTC.c | 12 | static |  | { 0U, false, {0}, 1U, 0U } | App_RTC, RTC_ClearStopWakeup, RTC_HandleAlarmWakeup, RTC_IRQHandler, RTC_SetWakeupPeriodSeconds, RTC_WKTimeConfig | Get_RTC_Time, RTC_DebugWatchBind, RTC_GetLastWakeupPeriodSeconds, RTC_GetWakeupPeriodSeconds, RTC_IsStopWakeup |


## 全量变量索引

| 变量 | 文件 | 行 | static | extern | volatile | 声明摘录 | 静态写入者 | 静态读取者 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 134 |  |  |  | uint32_t RESERVED0[24] |  |  |
| ICER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 135 |  |  | volatile | __IO uint32_t ICER[8] | NVIC_DisableIRQ, NVIC_Init |  |
| RSERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 136 |  |  |  | uint32_t RSERVED1[24] |  |  |
| ISPR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 137 |  |  | volatile | __IO uint32_t ISPR[8] | NVIC_SetPendingIRQ | IrqDebug_CountFast, NVIC_GetPendingIRQ, SystemDebug_SnapshotMcuResources |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 138 |  |  |  | uint32_t RESERVED2[24] |  |  |
| ICPR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 139 |  |  | volatile | __IO uint32_t ICPR[8] | NVIC_ClearPendingIRQ |  |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 140 |  |  |  | uint32_t RESERVED3[24] |  |  |
| IABR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 141 |  |  | volatile | __IO uint32_t IABR[8] |  | IrqDebug_CountFast, NVIC_GetActive, SystemDebug_SnapshotMcuResources |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 142 |  |  |  | uint32_t RESERVED4[56] |  |  |
| IP | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 143 |  |  | volatile | __IO uint8_t IP[240] | NVIC_Init, NVIC_SetPriority | NVIC_GetPriority |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 144 |  |  |  | uint32_t RESERVED5[644] |  |  |
| STIR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 145 |  |  |  | __O uint32_t STIR |  |  |
| ICSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 157 |  |  | volatile | __IO uint32_t ICSR |  | IrqDebug_CountFast, SystemDebug_SnapshotMcuResources |
| VTOR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 158 |  |  | volatile | __IO uint32_t VTOR | Board_Init, NVIC_SetVectorTable, SystemInit, jump_to_app |  |
| AIRCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 159 |  |  | volatile | __IO uint32_t AIRCR | NVIC_PriorityGroupConfig, NVIC_SetPriorityGrouping, NVIC_SystemReset | NVIC_GetPriorityGrouping, NVIC_Init |
| SCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 160 |  |  | volatile | __IO uint32_t SCR | NVIC_SystemLPConfig, PWR_EnterSTANDBYMode, PWR_EnterSTOPMode |  |
| CCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 161 |  |  | volatile | __IO uint32_t CCR | DMA_Cmd, DMA_DeInit, DMA_ITConfig, DMA_Init, I2C_FastModeDutyCycleConfig, I2C_Init |  |
| SHP | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 162 |  |  | volatile | __IO uint8_t SHP[12] | NVIC_SetPriority | NVIC_GetPriority |
| SHCSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 163 |  |  | volatile | __IO uint32_t SHCSR |  | SystemDebug_SnapshotMcuResources |
| CFSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 164 |  |  | volatile | __IO uint32_t CFSR |  |  |
| HFSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 165 |  |  | volatile | __IO uint32_t HFSR |  |  |
| DFSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 166 |  |  | volatile | __IO uint32_t DFSR |  |  |
| MMFAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 167 |  |  | volatile | __IO uint32_t MMFAR |  |  |
| BFAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 168 |  |  | volatile | __IO uint32_t BFAR |  |  |
| AFSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 169 |  |  | volatile | __IO uint32_t AFSR |  |  |
| PFR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 170 |  |  |  | __I uint32_t PFR[2] |  |  |
| DFR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 171 |  |  |  | __I uint32_t DFR |  |  |
| ADR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 172 |  |  |  | __I uint32_t ADR |  |  |
| MMFR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 173 |  |  |  | __I uint32_t MMFR[4] |  |  |
| ISAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 174 |  |  |  | __I uint32_t ISAR[5] |  |  |
| LOAD | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 367 |  |  | volatile | __IO uint32_t LOAD | SysTick_Config, __delay_ms, __delay_us |  |
| VAL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 368 |  |  | volatile | __IO uint32_t VAL | SysTick_Config, __delay_ms, __delay_us | SystemDebug_SnapshotMcuResources |
| CALIB | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 369 |  |  |  | __I uint32_t CALIB |  |  |
| u16 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 414 |  |  |  | __O uint16_t u16 |  |  |
| u32 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 415 |  |  |  | __O uint32_t u32 | ITM_SendChar |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 417 |  |  |  | uint32_t RESERVED0[864] |  |  |
| TER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 418 |  |  | volatile | __IO uint32_t TER |  | ITM_SendChar |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 419 |  |  |  | uint32_t RESERVED1[15] |  |  |
| TPR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 420 |  |  | volatile | __IO uint32_t TPR |  |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 421 |  |  |  | uint32_t RESERVED2[15] |  |  |
| TCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 422 |  |  | volatile | __IO uint32_t TCR |  | ITM_SendChar |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 423 |  |  |  | uint32_t RESERVED3[29] |  |  |
| IWR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 424 |  |  | volatile | __IO uint32_t IWR |  |  |
| IRR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 425 |  |  | volatile | __IO uint32_t IRR |  |  |
| IMCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 426 |  |  | volatile | __IO uint32_t IMCR |  |  |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 427 |  |  |  | uint32_t RESERVED4[43] |  |  |
| LAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 428 |  |  | volatile | __IO uint32_t LAR |  |  |
| LSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 429 |  |  | volatile | __IO uint32_t LSR |  |  |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 430 |  |  |  | uint32_t RESERVED5[6] |  |  |
| PID4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 431 |  |  |  | __I uint32_t PID4 |  |  |
| PID5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 432 |  |  |  | __I uint32_t PID5 |  |  |
| PID6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 433 |  |  |  | __I uint32_t PID6 |  |  |
| PID7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 434 |  |  |  | __I uint32_t PID7 |  |  |
| PID0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 435 |  |  |  | __I uint32_t PID0 |  |  |
| PID1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 436 |  |  |  | __I uint32_t PID1 |  |  |
| PID2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 437 |  |  |  | __I uint32_t PID2 |  |  |
| PID3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 438 |  |  |  | __I uint32_t PID3 |  |  |
| CID0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 439 |  |  |  | __I uint32_t CID0 |  |  |
| CID1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 440 |  |  |  | __I uint32_t CID1 |  |  |
| CID2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 441 |  |  |  | __I uint32_t CID2 |  |  |
| CID3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 442 |  |  |  | __I uint32_t CID3 |  |  |
| ICTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 505 |  |  |  | __I uint32_t ICTR |  |  |
| CTRL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 537 |  |  | volatile | __IO uint32_t CTRL | InitDelay, SysTick_CLKSourceConfig, SysTick_Config, __delay_ms, __delay_us, jump_to_app | SystemDebug_SnapshotMcuResources |
| RNR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 538 |  |  | volatile | __IO uint32_t RNR |  |  |
| RBAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 539 |  |  | volatile | __IO uint32_t RBAR |  |  |
| RASR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 540 |  |  | volatile | __IO uint32_t RASR |  |  |
| RBAR_A1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 541 |  |  | volatile | __IO uint32_t RBAR_A1 |  |  |
| RASR_A1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 542 |  |  | volatile | __IO uint32_t RASR_A1 |  |  |
| RBAR_A2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 543 |  |  | volatile | __IO uint32_t RBAR_A2 |  |  |
| RASR_A2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 544 |  |  | volatile | __IO uint32_t RASR_A2 |  |  |
| RBAR_A3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 545 |  |  | volatile | __IO uint32_t RBAR_A3 |  |  |
| RASR_A3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 546 |  |  | volatile | __IO uint32_t RASR_A3 |  |  |
| DCRSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 622 |  |  |  | __O uint32_t DCRSR |  |  |
| DCRDR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 623 |  |  | volatile | __IO uint32_t DCRDR |  |  |
| DEMCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 624 |  |  | volatile | __IO uint32_t DEMCR | SystemDebug_InitCycCnt | ITM_SendChar |
| CR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 544 |  |  | volatile | __IO uint32_t CR1 | ADC_AnalogWatchdogCmd, ADC_AnalogWatchdogSingleChannelConfig, ADC_AutoInjectedConvCmd, ADC_DiscModeChannelCountConfig, ADC_DiscModeCmd, ADC_ITConfig, ADC_Init, ADC_InjectedDiscModeCmd, I2C_ARPCmd, I2C_AcknowledgeConfig | ADC_GetITStatus, USART_GetITStatus |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 545 |  |  | volatile | __IO uint32_t CR2 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus |
| SMPR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 546 |  |  | volatile | __IO uint32_t SMPR1 | ADC_InjectedChannelConfig, ADC_RegularChannelConfig |  |
| SMPR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 547 |  |  | volatile | __IO uint32_t SMPR2 | ADC_InjectedChannelConfig, ADC_RegularChannelConfig |  |
| JOFR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 548 |  |  | volatile | __IO uint32_t JOFR1 |  |  |
| JOFR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 549 |  |  | volatile | __IO uint32_t JOFR2 |  |  |
| JOFR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 550 |  |  | volatile | __IO uint32_t JOFR3 |  |  |
| JOFR4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 551 |  |  | volatile | __IO uint32_t JOFR4 |  |  |
| HTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 552 |  |  | volatile | __IO uint32_t HTR | ADC_AnalogWatchdogThresholdsConfig |  |
| LTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 553 |  |  | volatile | __IO uint32_t LTR | ADC_AnalogWatchdogThresholdsConfig |  |
| SQR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 554 |  |  | volatile | __IO uint32_t SQR1 | ADC_Init, ADC_RegularChannelConfig |  |
| SQR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 555 |  |  | volatile | __IO uint32_t SQR2 | ADC_RegularChannelConfig |  |
| SQR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 556 |  |  | volatile | __IO uint32_t SQR3 | ADC_RegularChannelConfig |  |
| JSQR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 557 |  |  | volatile | __IO uint32_t JSQR | ADC_InjectedChannelConfig, ADC_InjectedSequencerLengthConfig |  |
| JDR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 558 |  |  | volatile | __IO uint32_t JDR1 |  |  |
| JDR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 559 |  |  | volatile | __IO uint32_t JDR2 |  |  |
| JDR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 560 |  |  | volatile | __IO uint32_t JDR3 |  |  |
| JDR4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 561 |  |  | volatile | __IO uint32_t JDR4 |  |  |
| DR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 562 |  |  | volatile | __IO uint32_t DR | CRC_CalcBlockCRC, CRC_CalcCRC, I2C_Send7bitAddress, I2C_SendData, SPI_I2S_SendData, Sci_PortIRQHandler, USART_SendData, dbg_uart_putc | ADC_GetConversionValue, BOARD_UART_IRQHandler, CRC_GetCRC, I2C_ReceiveData, InitADC_DMA, SPI_I2S_ReceiveData, Sci_PortArmReceiver, Sci_PortHandleError, USART_ReceiveData, serial_clear_overrun |
| DR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 572 |  |  | volatile | __IO uint16_t DR1 |  |  |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 573 |  |  |  | uint16_t RESERVED1 |  |  |
| DR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 574 |  |  | volatile | __IO uint16_t DR2 |  |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 575 |  |  |  | uint16_t RESERVED2 |  |  |
| DR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 576 |  |  | volatile | __IO uint16_t DR3 |  |  |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 577 |  |  |  | uint16_t RESERVED3 |  |  |
| DR4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 578 |  |  | volatile | __IO uint16_t DR4 |  |  |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 579 |  |  |  | uint16_t RESERVED4 |  |  |
| DR5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 580 |  |  | volatile | __IO uint16_t DR5 |  |  |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 581 |  |  |  | uint16_t RESERVED5 |  |  |
| DR6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 582 |  |  | volatile | __IO uint16_t DR6 |  |  |
| RESERVED6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 583 |  |  |  | uint16_t RESERVED6 |  |  |
| DR7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 584 |  |  | volatile | __IO uint16_t DR7 |  |  |
| RESERVED7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 585 |  |  |  | uint16_t RESERVED7 |  |  |
| DR8 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 586 |  |  | volatile | __IO uint16_t DR8 |  |  |
| RESERVED8 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 587 |  |  |  | uint16_t RESERVED8 |  |  |
| DR9 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 588 |  |  | volatile | __IO uint16_t DR9 |  |  |
| RESERVED9 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 589 |  |  |  | uint16_t RESERVED9 |  |  |
| DR10 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 590 |  |  | volatile | __IO uint16_t DR10 |  |  |
| RESERVED10 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 591 |  |  |  | uint16_t RESERVED10 |  |  |
| RTCCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 592 |  |  | volatile | __IO uint16_t RTCCR | BKP_RTCOutputConfig, BKP_SetRTCCalibrationValue |  |
| RESERVED11 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 593 |  |  |  | uint16_t RESERVED11 |  |  |
| CR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 594 |  |  | volatile | __IO uint16_t CR | CRC_ResetDR, DAC_Cmd, DAC_DMACmd, DAC_ITConfig, DAC_Init, DAC_WaveGenerationCmd, DBGMCU_Config, EnableLowPowerDebug, FLASH_BootConfig, FLASH_EnableWriteProtection | DAC_GetITStatus, RCC_GetFlagStatus, SystemDebug_SnapshotMcuResources |
| RESERVED12 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 595 |  |  |  | uint16_t RESERVED12 |  |  |
| CSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 596 |  |  | volatile | __IO uint16_t CSR | BKP_ClearFlag, BKP_ClearITPendingBit, CEC_ClearFlag, CEC_ClearITPendingBit, RCC_ClearFlag | CEC_GetITStatus, PWR_GetFlagStatus, RCC_GetFlagStatus, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |
| RESERVED13 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 597 |  |  |  | uint16_t RESERVED13[5] |  |  |
| DR11 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 598 |  |  | volatile | __IO uint16_t DR11 |  |  |
| RESERVED14 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 599 |  |  |  | uint16_t RESERVED14 |  |  |
| DR12 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 600 |  |  | volatile | __IO uint16_t DR12 |  |  |
| RESERVED15 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 601 |  |  |  | uint16_t RESERVED15 |  |  |
| DR13 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 602 |  |  | volatile | __IO uint16_t DR13 |  |  |
| RESERVED16 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 603 |  |  |  | uint16_t RESERVED16 |  |  |
| DR14 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 604 |  |  | volatile | __IO uint16_t DR14 |  |  |
| RESERVED17 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 605 |  |  |  | uint16_t RESERVED17 |  |  |
| DR15 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 606 |  |  | volatile | __IO uint16_t DR15 |  |  |
| RESERVED18 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 607 |  |  |  | uint16_t RESERVED18 |  |  |
| DR16 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 608 |  |  | volatile | __IO uint16_t DR16 |  |  |
| RESERVED19 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 609 |  |  |  | uint16_t RESERVED19 |  |  |
| DR17 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 610 |  |  | volatile | __IO uint16_t DR17 |  |  |
| RESERVED20 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 611 |  |  |  | uint16_t RESERVED20 |  |  |
| DR18 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 612 |  |  | volatile | __IO uint16_t DR18 |  |  |
| RESERVED21 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 613 |  |  |  | uint16_t RESERVED21 |  |  |
| DR19 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 614 |  |  | volatile | __IO uint16_t DR19 |  |  |
| RESERVED22 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 615 |  |  |  | uint16_t RESERVED22 |  |  |
| DR20 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 616 |  |  | volatile | __IO uint16_t DR20 |  |  |
| RESERVED23 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 617 |  |  |  | uint16_t RESERVED23 |  |  |
| DR21 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 618 |  |  | volatile | __IO uint16_t DR21 |  |  |
| RESERVED24 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 619 |  |  |  | uint16_t RESERVED24 |  |  |
| DR22 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 620 |  |  | volatile | __IO uint16_t DR22 |  |  |
| RESERVED25 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 621 |  |  |  | uint16_t RESERVED25 |  |  |
| DR23 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 622 |  |  | volatile | __IO uint16_t DR23 |  |  |
| RESERVED26 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 623 |  |  |  | uint16_t RESERVED26 |  |  |
| DR24 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 624 |  |  | volatile | __IO uint16_t DR24 |  |  |
| RESERVED27 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 625 |  |  |  | uint16_t RESERVED27 |  |  |
| DR25 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 626 |  |  | volatile | __IO uint16_t DR25 |  |  |
| RESERVED28 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 627 |  |  |  | uint16_t RESERVED28 |  |  |
| DR26 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 628 |  |  | volatile | __IO uint16_t DR26 |  |  |
| RESERVED29 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 629 |  |  |  | uint16_t RESERVED29 |  |  |
| DR27 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 630 |  |  | volatile | __IO uint16_t DR27 |  |  |
| RESERVED30 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 631 |  |  |  | uint16_t RESERVED30 |  |  |
| DR28 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 632 |  |  | volatile | __IO uint16_t DR28 |  |  |
| RESERVED31 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 633 |  |  |  | uint16_t RESERVED31 |  |  |
| DR29 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 634 |  |  | volatile | __IO uint16_t DR29 |  |  |
| RESERVED32 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 635 |  |  |  | uint16_t RESERVED32 |  |  |
| DR30 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 636 |  |  | volatile | __IO uint16_t DR30 |  |  |
| RESERVED33 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 637 |  |  |  | uint16_t RESERVED33 |  |  |
| DR31 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 638 |  |  | volatile | __IO uint16_t DR31 |  |  |
| RESERVED34 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 639 |  |  |  | uint16_t RESERVED34 |  |  |
| DR32 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 640 |  |  | volatile | __IO uint16_t DR32 |  |  |
| RESERVED35 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 641 |  |  |  | uint16_t RESERVED35 |  |  |
| DR33 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 642 |  |  | volatile | __IO uint16_t DR33 |  |  |
| RESERVED36 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 643 |  |  |  | uint16_t RESERVED36 |  |  |
| DR34 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 644 |  |  | volatile | __IO uint16_t DR34 |  |  |
| RESERVED37 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 645 |  |  |  | uint16_t RESERVED37 |  |  |
| DR35 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 646 |  |  | volatile | __IO uint16_t DR35 |  |  |
| RESERVED38 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 647 |  |  |  | uint16_t RESERVED38 |  |  |
| DR36 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 648 |  |  | volatile | __IO uint16_t DR36 |  |  |
| RESERVED39 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 649 |  |  |  | uint16_t RESERVED39 |  |  |
| DR37 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 650 |  |  | volatile | __IO uint16_t DR37 |  |  |
| RESERVED40 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 651 |  |  |  | uint16_t RESERVED40 |  |  |
| DR38 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 652 |  |  | volatile | __IO uint16_t DR38 |  |  |
| RESERVED41 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 653 |  |  |  | uint16_t RESERVED41 |  |  |
| DR39 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 654 |  |  | volatile | __IO uint16_t DR39 |  |  |
| RESERVED42 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 655 |  |  |  | uint16_t RESERVED42 |  |  |
| DR40 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 656 |  |  | volatile | __IO uint16_t DR40 |  |  |
| RESERVED43 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 657 |  |  |  | uint16_t RESERVED43 |  |  |
| DR41 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 658 |  |  | volatile | __IO uint16_t DR41 |  |  |
| RESERVED44 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 659 |  |  |  | uint16_t RESERVED44 |  |  |
| DR42 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 660 |  |  | volatile | __IO uint16_t DR42 |  |  |
| RESERVED45 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 661 |  |  |  | uint16_t RESERVED45 |  |  |
| TDTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 671 |  |  | volatile | __IO uint32_t TDTR | CAN_TTComModeCmd, CAN_Transmit |  |
| TDLR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 672 |  |  | volatile | __IO uint32_t TDLR | CAN_Transmit |  |
| TDHR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 673 |  |  | volatile | __IO uint32_t TDHR | CAN_Transmit |  |
| RDTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 683 |  |  | volatile | __IO uint32_t RDTR |  | CAN_Receive |
| RDLR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 684 |  |  | volatile | __IO uint32_t RDLR |  | CAN_Receive |
| RDHR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 685 |  |  | volatile | __IO uint32_t RDHR |  | CAN_Receive |
| FR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 695 |  |  | volatile | __IO uint32_t FR2 | CAN_FilterInit |  |
| MSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 705 |  |  | volatile | __IO uint32_t MSR | CAN_ClearFlag, CAN_ClearITPendingBit | CAN_GetFlagStatus, CAN_GetITStatus, CAN_Init, CAN_OperatingModeRequest, CAN_Sleep, CAN_WakeUp, SystemDebug_SnapshotMcuResources, can_diag_latch_regs |
| TSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 706 |  |  | volatile | __IO uint32_t TSR | CAN_CancelTransmit, CAN_ClearFlag, CAN_ClearITPendingBit | CAN_GetFlagStatus, CAN_GetITStatus, CAN_Transmit, CAN_TransmitStatus, SystemDebug_SnapshotMcuResources, can_diag_latch_regs, can_has_pending_work, can_has_sleep_blocking_work, feidao_can_cancel_tx |
| RF0R | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 707 |  |  | volatile | __IO uint32_t RF0R | CAN_ClearFlag, CAN_ClearITPendingBit, CAN_FIFORelease, CAN_Receive | CAN_GetFlagStatus, CAN_GetITStatus, CAN_MessagePending, SystemDebug_SnapshotMcuResources, can_diag_latch_regs |
| RF1R | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 708 |  |  | volatile | __IO uint32_t RF1R | CAN_ClearFlag, CAN_ClearITPendingBit, CAN_FIFORelease, CAN_Receive | CAN_GetFlagStatus, CAN_GetITStatus, CAN_MessagePending |
| IER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 709 |  |  | volatile | __IO uint32_t IER | CAN_ITConfig | CAN_GetITStatus |
| ESR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 710 |  |  | volatile | __IO uint32_t ESR | CAN_ClearFlag, CAN_ClearITPendingBit | CAN_GetFlagStatus, CAN_GetITStatus, CAN_GetLSBTransmitErrorCounter, CAN_GetLastErrorCode, CAN_GetReceiveErrorCounter, Can_GetDebugSnapshot, SystemDebug_SnapshotMcuResources, can_diag_latch_regs |
| BTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 711 |  |  | volatile | __IO uint32_t BTR | CAN_Init |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 712 |  |  |  | uint32_t RESERVED0[88] |  |  |
| sTxMailBox | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 713 |  |  |  | CAN_TxMailBox_TypeDef sTxMailBox[3] | CAN_TTComModeCmd, CAN_Transmit |  |
| sFIFOMailBox | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 714 |  |  |  | CAN_FIFOMailBox_TypeDef sFIFOMailBox[2] |  | CAN_Receive |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 715 |  |  |  | uint32_t RESERVED1[12] |  |  |
| FMR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 716 |  |  | volatile | __IO uint32_t FMR | CAN_FilterInit, CAN_SlaveStartBank |  |
| FM1R | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 717 |  |  | volatile | __IO uint32_t FM1R | CAN_FilterInit |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 718 |  |  |  | uint32_t RESERVED2 |  |  |
| FS1R | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 719 |  |  | volatile | __IO uint32_t FS1R | CAN_FilterInit |  |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 720 |  |  |  | uint32_t RESERVED3 |  |  |
| FFA1R | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 721 |  |  | volatile | __IO uint32_t FFA1R | CAN_FilterInit |  |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 722 |  |  |  | uint32_t RESERVED4 |  |  |
| FA1R | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 723 |  |  | volatile | __IO uint32_t FA1R | CAN_FilterInit |  |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 724 |  |  |  | uint32_t RESERVED5[8] |  |  |
| OAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 738 |  |  | volatile | __IO uint32_t OAR | CEC_OwnAddressConfig |  |
| PRES | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 739 |  |  | volatile | __IO uint32_t PRES | CEC_SetPrescaler |  |
| ESR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 740 |  |  | volatile | __IO uint32_t ESR | CAN_ClearFlag, CAN_ClearITPendingBit | CAN_GetFlagStatus, CAN_GetITStatus, CAN_GetLSBTransmitErrorCounter, CAN_GetLastErrorCode, CAN_GetReceiveErrorCounter, Can_GetDebugSnapshot, SystemDebug_SnapshotMcuResources, can_diag_latch_regs |
| CSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 741 |  |  | volatile | __IO uint32_t CSR | BKP_ClearFlag, BKP_ClearITPendingBit, CEC_ClearFlag, CEC_ClearITPendingBit, RCC_ClearFlag | CEC_GetITStatus, PWR_GetFlagStatus, RCC_GetFlagStatus, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |
| TXD | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 742 |  |  | volatile | __IO uint32_t TXD | CEC_SendDataByte |  |
| RXD | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 743 |  |  | volatile | __IO uint32_t RXD |  | CEC_ReceiveDataByte |
| IDR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 753 |  |  | volatile | __IO uint8_t IDR | CRC_SetIDRegister | CRC_GetIDRegister, GPIO_ReadInputData, GPIO_ReadInputDataBit |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 754 |  |  |  | uint8_t RESERVED0 |  |  |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 755 |  |  |  | uint16_t RESERVED1 |  |  |
| CR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 756 |  |  | volatile | __IO uint32_t CR | CRC_ResetDR, DAC_Cmd, DAC_DMACmd, DAC_ITConfig, DAC_Init, DAC_WaveGenerationCmd, DBGMCU_Config, EnableLowPowerDebug, FLASH_BootConfig, FLASH_EnableWriteProtection | DAC_GetITStatus, RCC_GetFlagStatus, SystemDebug_SnapshotMcuResources |
| SWTRIGR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 766 |  |  | volatile | __IO uint32_t SWTRIGR | DAC_DualSoftwareTriggerCmd, DAC_SoftwareTriggerCmd |  |
| DHR12R1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 767 |  |  | volatile | __IO uint32_t DHR12R1 |  |  |
| DHR12L1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 768 |  |  | volatile | __IO uint32_t DHR12L1 |  |  |
| DHR8R1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 769 |  |  | volatile | __IO uint32_t DHR8R1 |  |  |
| DHR12R2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 770 |  |  | volatile | __IO uint32_t DHR12R2 |  |  |
| DHR12L2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 771 |  |  | volatile | __IO uint32_t DHR12L2 |  |  |
| DHR8R2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 772 |  |  | volatile | __IO uint32_t DHR8R2 |  |  |
| DHR12RD | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 773 |  |  | volatile | __IO uint32_t DHR12RD |  |  |
| DHR12LD | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 774 |  |  | volatile | __IO uint32_t DHR12LD |  |  |
| DHR8RD | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 775 |  |  | volatile | __IO uint32_t DHR8RD |  |  |
| DOR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 776 |  |  | volatile | __IO uint32_t DOR1 |  |  |
| DOR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 777 |  |  | volatile | __IO uint32_t DOR2 |  |  |
| CR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 790 |  |  | volatile | __IO uint32_t CR | CRC_ResetDR, DAC_Cmd, DAC_DMACmd, DAC_ITConfig, DAC_Init, DAC_WaveGenerationCmd, DBGMCU_Config, EnableLowPowerDebug, FLASH_BootConfig, FLASH_EnableWriteProtection | DAC_GetITStatus, RCC_GetFlagStatus, SystemDebug_SnapshotMcuResources |
| CNDTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 800 |  |  | volatile | __IO uint32_t CNDTR | DMA_DeInit, DMA_Init, DMA_SetCurrDataCounter | DMA_GetCurrDataCounter |
| CPAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 801 |  |  | volatile | __IO uint32_t CPAR | DMA_DeInit, DMA_Init |  |
| CMAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 802 |  |  | volatile | __IO uint32_t CMAR | DMA_DeInit, DMA_Init |  |
| IFCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 808 |  |  | volatile | __IO uint32_t IFCR | DMA_ClearFlag, DMA_ClearITPendingBit, DMA_DeInit |  |
| MACFFR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 818 |  |  | volatile | __IO uint32_t MACFFR |  |  |
| MACHTHR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 819 |  |  | volatile | __IO uint32_t MACHTHR |  |  |
| MACHTLR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 820 |  |  | volatile | __IO uint32_t MACHTLR |  |  |
| MACMIIAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 821 |  |  | volatile | __IO uint32_t MACMIIAR |  |  |
| MACMIIDR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 822 |  |  | volatile | __IO uint32_t MACMIIDR |  |  |
| MACFCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 823 |  |  | volatile | __IO uint32_t MACFCR |  |  |
| MACVLANTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 824 |  |  | volatile | __IO uint32_t MACVLANTR |  |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 825 |  |  |  | uint32_t RESERVED0[2] |  |  |
| MACRWUFFR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 826 |  |  | volatile | __IO uint32_t MACRWUFFR |  |  |
| MACPMTCSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 827 |  |  | volatile | __IO uint32_t MACPMTCSR |  |  |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 828 |  |  |  | uint32_t RESERVED1[2] |  |  |
| MACSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 829 |  |  | volatile | __IO uint32_t MACSR |  |  |
| MACIMR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 830 |  |  | volatile | __IO uint32_t MACIMR |  |  |
| MACA0HR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 831 |  |  | volatile | __IO uint32_t MACA0HR |  |  |
| MACA0LR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 832 |  |  | volatile | __IO uint32_t MACA0LR |  |  |
| MACA1HR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 833 |  |  | volatile | __IO uint32_t MACA1HR |  |  |
| MACA1LR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 834 |  |  | volatile | __IO uint32_t MACA1LR |  |  |
| MACA2HR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 835 |  |  | volatile | __IO uint32_t MACA2HR |  |  |
| MACA2LR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 836 |  |  | volatile | __IO uint32_t MACA2LR |  |  |
| MACA3HR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 837 |  |  | volatile | __IO uint32_t MACA3HR |  |  |
| MACA3LR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 838 |  |  | volatile | __IO uint32_t MACA3LR |  |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 839 |  |  |  | uint32_t RESERVED2[40] |  |  |
| MMCCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 840 |  |  | volatile | __IO uint32_t MMCCR |  |  |
| MMCRIR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 841 |  |  | volatile | __IO uint32_t MMCRIR |  |  |
| MMCTIR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 842 |  |  | volatile | __IO uint32_t MMCTIR |  |  |
| MMCRIMR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 843 |  |  | volatile | __IO uint32_t MMCRIMR |  |  |
| MMCTIMR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 844 |  |  | volatile | __IO uint32_t MMCTIMR |  |  |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 845 |  |  |  | uint32_t RESERVED3[14] |  |  |
| MMCTGFSCCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 846 |  |  | volatile | __IO uint32_t MMCTGFSCCR |  |  |
| MMCTGFMSCCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 847 |  |  | volatile | __IO uint32_t MMCTGFMSCCR |  |  |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 848 |  |  |  | uint32_t RESERVED4[5] |  |  |
| MMCTGFCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 849 |  |  | volatile | __IO uint32_t MMCTGFCR |  |  |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 850 |  |  |  | uint32_t RESERVED5[10] |  |  |
| MMCRFCECR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 851 |  |  | volatile | __IO uint32_t MMCRFCECR |  |  |
| MMCRFAECR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 852 |  |  | volatile | __IO uint32_t MMCRFAECR |  |  |
| RESERVED6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 853 |  |  |  | uint32_t RESERVED6[10] |  |  |
| MMCRGUFCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 854 |  |  | volatile | __IO uint32_t MMCRGUFCR |  |  |
| RESERVED7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 855 |  |  |  | uint32_t RESERVED7[334] |  |  |
| PTPTSCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 856 |  |  | volatile | __IO uint32_t PTPTSCR |  |  |
| PTPSSIR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 857 |  |  | volatile | __IO uint32_t PTPSSIR |  |  |
| PTPTSHR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 858 |  |  | volatile | __IO uint32_t PTPTSHR |  |  |
| PTPTSLR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 859 |  |  | volatile | __IO uint32_t PTPTSLR |  |  |
| PTPTSHUR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 860 |  |  | volatile | __IO uint32_t PTPTSHUR |  |  |
| PTPTSLUR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 861 |  |  | volatile | __IO uint32_t PTPTSLUR |  |  |
| PTPTSAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 862 |  |  | volatile | __IO uint32_t PTPTSAR |  |  |
| PTPTTHR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 863 |  |  | volatile | __IO uint32_t PTPTTHR |  |  |
| PTPTTLR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 864 |  |  | volatile | __IO uint32_t PTPTTLR |  |  |
| RESERVED8 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 865 |  |  |  | uint32_t RESERVED8[567] |  |  |
| DMABMR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 866 |  |  | volatile | __IO uint32_t DMABMR |  |  |
| DMATPDR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 867 |  |  | volatile | __IO uint32_t DMATPDR |  |  |
| DMARPDR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 868 |  |  | volatile | __IO uint32_t DMARPDR |  |  |
| DMARDLAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 869 |  |  | volatile | __IO uint32_t DMARDLAR |  |  |
| DMATDLAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 870 |  |  | volatile | __IO uint32_t DMATDLAR |  |  |
| DMASR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 871 |  |  | volatile | __IO uint32_t DMASR |  |  |
| DMAOMR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 872 |  |  | volatile | __IO uint32_t DMAOMR |  |  |
| DMAIER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 873 |  |  | volatile | __IO uint32_t DMAIER |  |  |
| DMAMFBOCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 874 |  |  | volatile | __IO uint32_t DMAMFBOCR |  |  |
| RESERVED9 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 875 |  |  |  | uint32_t RESERVED9[9] |  |  |
| DMACHTDR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 876 |  |  | volatile | __IO uint32_t DMACHTDR |  |  |
| DMACHRDR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 877 |  |  | volatile | __IO uint32_t DMACHRDR |  |  |
| DMACHTBAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 878 |  |  | volatile | __IO uint32_t DMACHTBAR |  |  |
| DMACHRBAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 879 |  |  | volatile | __IO uint32_t DMACHRBAR |  |  |
| EMR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 889 |  |  | volatile | __IO uint32_t EMR | EXTI_DeInit, EXTI_Init |  |
| RTSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 890 |  |  | volatile | __IO uint32_t RTSR | EXTI_DeInit, EXTI_Init |  |
| FTSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 891 |  |  | volatile | __IO uint32_t FTSR | EXTI_DeInit, EXTI_Init |  |
| SWIER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 892 |  |  | volatile | __IO uint32_t SWIER | EXTI_GenerateSWInterrupt |  |
| PR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 893 |  |  | volatile | __IO uint32_t PR | EXTI_ClearFlag, EXTI_ClearITPendingBit, EXTI_DeInit, IWDG_SetPrescaler | EXTI_GetFlagStatus, EXTI_GetITStatus, IrqDebug_CountFast, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |
| KEYR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 903 |  |  | volatile | __IO uint32_t KEYR | FLASH_Unlock, FLASH_UnlockBank1 |  |
| OPTKEYR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 904 |  |  | volatile | __IO uint32_t OPTKEYR | FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseOptionBytes, FLASH_ProgramOptionByteData, FLASH_ReadOutProtection, FLASH_UserOptionByteConfig |  |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 905 |  |  | volatile | __IO uint32_t SR | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus |
| CR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 906 |  |  | volatile | __IO uint32_t CR | CRC_ResetDR, DAC_Cmd, DAC_DMACmd, DAC_ITConfig, DAC_Init, DAC_WaveGenerationCmd, DBGMCU_Config, EnableLowPowerDebug, FLASH_BootConfig, FLASH_EnableWriteProtection | DAC_GetITStatus, RCC_GetFlagStatus, SystemDebug_SnapshotMcuResources |
| AR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 907 |  |  | volatile | __IO uint32_t AR | FLASH_ErasePage |  |
| RESERVED | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 908 |  |  | volatile | __IO uint32_t RESERVED |  |  |
| OBR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 909 |  |  | volatile | __IO uint32_t OBR |  | FLASH_GetFlagStatus, FLASH_GetReadOutProtectionStatus, FLASH_GetUserOptionByte |
| WRPR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 910 |  |  | volatile | __IO uint32_t WRPR |  | FLASH_GetWriteProtectionOptionByte |
| KEYR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 913 |  |  | volatile | __IO uint32_t KEYR2 | FLASH_Unlock, FLASH_UnlockBank2 |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 914 |  |  |  | uint32_t RESERVED2 |  |  |
| SR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 915 |  |  | volatile | __IO uint32_t SR2 | FLASH_ClearFlag, FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_NANDDeInit | FLASH_GetBank2Status, FLASH_GetFlagStatus, FSMC_GetFlagStatus, FSMC_GetITStatus, I2C_CheckEvent, I2C_GetLastEvent, I2C_GetPEC |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 916 |  |  | volatile | __IO uint32_t CR2 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus |
| AR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 917 |  |  | volatile | __IO uint32_t AR2 | FLASH_ErasePage |  |
| USER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 928 |  |  | volatile | __IO uint16_t USER | FLASH_BootConfig, FLASH_UserOptionByteConfig |  |
| Data0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 929 |  |  | volatile | __IO uint16_t Data0 |  |  |
| Data1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 930 |  |  | volatile | __IO uint16_t Data1 |  | DAC_SetDualChannelData |
| WRP0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 931 |  |  | volatile | __IO uint16_t WRP0 | FLASH_EnableWriteProtection |  |
| WRP1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 932 |  |  | volatile | __IO uint16_t WRP1 | FLASH_EnableWriteProtection |  |
| WRP2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 933 |  |  | volatile | __IO uint16_t WRP2 | FLASH_EnableWriteProtection |  |
| WRP3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 934 |  |  | volatile | __IO uint16_t WRP3 | FLASH_EnableWriteProtection |  |
| SR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 962 |  |  | volatile | __IO uint32_t SR2 | FLASH_ClearFlag, FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_NANDDeInit | FLASH_GetBank2Status, FLASH_GetFlagStatus, FSMC_GetFlagStatus, FSMC_GetITStatus, I2C_CheckEvent, I2C_GetLastEvent, I2C_GetPEC |
| PMEM2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 963 |  |  | volatile | __IO uint32_t PMEM2 | FSMC_NANDDeInit, FSMC_NANDInit |  |
| PATT2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 964 |  |  | volatile | __IO uint32_t PATT2 | FSMC_NANDDeInit, FSMC_NANDInit |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 965 |  |  |  | uint32_t RESERVED0 |  |  |
| ECCR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 966 |  |  | volatile | __IO uint32_t ECCR2 |  | FSMC_GetECC |
| SR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 976 |  |  | volatile | __IO uint32_t SR3 | FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_NANDDeInit | FSMC_GetFlagStatus, FSMC_GetITStatus |
| PMEM3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 977 |  |  | volatile | __IO uint32_t PMEM3 | FSMC_NANDDeInit, FSMC_NANDInit |  |
| PATT3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 978 |  |  | volatile | __IO uint32_t PATT3 | FSMC_NANDDeInit, FSMC_NANDInit |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 979 |  |  |  | uint32_t RESERVED0 |  |  |
| ECCR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 980 |  |  | volatile | __IO uint32_t ECCR3 |  | FSMC_GetECC |
| SR4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 990 |  |  | volatile | __IO uint32_t SR4 | FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_PCCARDDeInit | FSMC_GetFlagStatus, FSMC_GetITStatus |
| PMEM4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 991 |  |  | volatile | __IO uint32_t PMEM4 | FSMC_PCCARDDeInit, FSMC_PCCARDInit |  |
| PATT4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 992 |  |  | volatile | __IO uint32_t PATT4 | FSMC_PCCARDDeInit, FSMC_PCCARDInit |  |
| PIO4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 993 |  |  | volatile | __IO uint32_t PIO4 | FSMC_PCCARDDeInit, FSMC_PCCARDInit |  |
| CRH | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1003 |  |  | volatile | __IO uint32_t CRH | GPIO_Init, LedBar_AllPinsHiZ, RTC_ITConfig, SystemInit_ExtMemCtl | LedBar_PinModeF1, RTC_GetITStatus |
| IDR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1004 |  |  | volatile | __IO uint32_t IDR | CRC_SetIDRegister | CRC_GetIDRegister, GPIO_ReadInputData, GPIO_ReadInputDataBit |
| ODR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1005 |  |  | volatile | __IO uint32_t ODR | GPIO_Write | GPIO_ReadOutputData, GPIO_ReadOutputDataBit |
| BSRR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1006 |  |  | volatile | __IO uint32_t BSRR | GPIO_Init, GPIO_SetBits, GPIO_WriteBit, LedBar_PinWrite |  |
| BRR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1007 |  |  | volatile | __IO uint32_t BRR | GPIO_Init, GPIO_ResetBits, GPIO_WriteBit, LedBar_PinWrite, USART_Init |  |
| LCKR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1008 |  |  | volatile | __IO uint32_t LCKR | GPIO_PinLockConfig |  |
| MAPR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1018 |  |  | volatile | __IO uint32_t MAPR | GPIO_PinRemapConfig |  |
| EXTICR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1019 |  |  | volatile | __IO uint32_t EXTICR[4] | GPIO_EXTILineConfig |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1020 |  |  |  | uint32_t RESERVED0 |  |  |
| MAPR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1021 |  |  | volatile | __IO uint32_t MAPR2 | GPIO_PinRemapConfig |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1030 |  |  |  | uint16_t RESERVED0 |  |  |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1031 |  |  | volatile | __IO uint16_t CR2 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1032 |  |  |  | uint16_t RESERVED1 |  |  |
| OAR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1033 |  |  | volatile | __IO uint16_t OAR1 | I2C_Init |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1034 |  |  |  | uint16_t RESERVED2 |  |  |
| OAR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1035 |  |  | volatile | __IO uint16_t OAR2 | I2C_DualAddressCmd, I2C_OwnAddress2Config |  |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1036 |  |  |  | uint16_t RESERVED3 |  |  |
| DR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1037 |  |  | volatile | __IO uint16_t DR | CRC_CalcBlockCRC, CRC_CalcCRC, I2C_Send7bitAddress, I2C_SendData, SPI_I2S_SendData, Sci_PortIRQHandler, USART_SendData, dbg_uart_putc | ADC_GetConversionValue, BOARD_UART_IRQHandler, CRC_GetCRC, I2C_ReceiveData, InitADC_DMA, SPI_I2S_ReceiveData, Sci_PortArmReceiver, Sci_PortHandleError, USART_ReceiveData, serial_clear_overrun |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1038 |  |  |  | uint16_t RESERVED4 |  |  |
| SR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1039 |  |  | volatile | __IO uint16_t SR1 | I2C_ClearFlag, I2C_ClearITPendingBit | I2C_CheckEvent, I2C_GetITStatus, I2C_GetLastEvent |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1040 |  |  |  | uint16_t RESERVED5 |  |  |
| SR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1041 |  |  | volatile | __IO uint16_t SR2 | FLASH_ClearFlag, FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_NANDDeInit | FLASH_GetBank2Status, FLASH_GetFlagStatus, FSMC_GetFlagStatus, FSMC_GetITStatus, I2C_CheckEvent, I2C_GetLastEvent, I2C_GetPEC |
| RESERVED6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1042 |  |  |  | uint16_t RESERVED6 |  |  |
| CCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1043 |  |  | volatile | __IO uint16_t CCR | DMA_Cmd, DMA_DeInit, DMA_ITConfig, DMA_Init, I2C_FastModeDutyCycleConfig, I2C_Init |  |
| RESERVED7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1044 |  |  |  | uint16_t RESERVED7 |  |  |
| TRISE | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1045 |  |  | volatile | __IO uint16_t TRISE | I2C_Init |  |
| RESERVED8 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1046 |  |  |  | uint16_t RESERVED8 |  |  |
| PR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1056 |  |  | volatile | __IO uint32_t PR | EXTI_ClearFlag, EXTI_ClearITPendingBit, EXTI_DeInit, IWDG_SetPrescaler | EXTI_GetFlagStatus, EXTI_GetITStatus, IrqDebug_CountFast, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |
| RLR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1057 |  |  | volatile | __IO uint32_t RLR | IWDG_SetReload | SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1058 |  |  | volatile | __IO uint32_t SR | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus |
| CSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1068 |  |  | volatile | __IO uint32_t CSR | BKP_ClearFlag, BKP_ClearITPendingBit, CEC_ClearFlag, CEC_ClearITPendingBit, RCC_ClearFlag | CEC_GetITStatus, PWR_GetFlagStatus, RCC_GetFlagStatus, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |
| CFGR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1078 |  |  | volatile | __IO uint32_t CFGR | CEC_Init, RCC_ADCCLKConfig, RCC_DeInit, RCC_HCLKConfig, RCC_PCLK1Config, RCC_PCLK2Config, RCC_PLLConfig, RCC_SYSCLKConfig, SetSysClockTo24, SetSysClockTo36 | CEC_Cmd, CEC_GetITStatus, RCC_GetClocksFreq, RCC_GetSYSCLKSource, SystemCoreClockUpdate, SystemDebug_SnapshotMcuResources |
| CIR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1079 |  |  | volatile | __IO uint32_t CIR | RCC_DeInit, SystemInit | RCC_GetITStatus |
| APB2RSTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1080 |  |  | volatile | __IO uint32_t APB2RSTR | RCC_APB2PeriphResetCmd |  |
| APB1RSTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1081 |  |  | volatile | __IO uint32_t APB1RSTR | RCC_APB1PeriphResetCmd |  |
| AHBENR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1082 |  |  | volatile | __IO uint32_t AHBENR | RCC_AHBPeriphClockCmd, SystemInit_ExtMemCtl | SystemDebug_SnapshotMcuResources |
| APB2ENR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1083 |  |  | volatile | __IO uint32_t APB2ENR | RCC_APB2PeriphClockCmd, SystemInit_ExtMemCtl | SystemDebug_ReadUartSr, SystemDebug_SnapshotMcuResources |
| APB1ENR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1084 |  |  | volatile | __IO uint32_t APB1ENR | RCC_APB1PeriphClockCmd | SystemDebug_ReadUartSr, SystemDebug_SnapshotMcuResources |
| BDCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1085 |  |  | volatile | __IO uint32_t BDCR | RCC_RTCCLKConfig | RCC_GetFlagStatus, RTC_PrepareExistingClock, SystemDebug_SnapshotMcuResources |
| CSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1086 |  |  | volatile | __IO uint32_t CSR | BKP_ClearFlag, BKP_ClearITPendingBit, CEC_ClearFlag, CEC_ClearITPendingBit, RCC_ClearFlag | CEC_GetITStatus, PWR_GetFlagStatus, RCC_GetFlagStatus, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |
| CFGR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1090 |  |  | volatile | __IO uint32_t CFGR2 | RCC_DeInit, RCC_PLL2Config, RCC_PLL3Config, RCC_PREDIV1Config, RCC_PREDIV2Config, SetSysClockTo24, SetSysClockTo36, SetSysClockTo48, SetSysClockTo56, SetSysClockTo72 | I2S_Init, RCC_GetClocksFreq, SystemCoreClockUpdate |
| CFGR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1095 |  |  | volatile | __IO uint32_t CFGR2 | RCC_DeInit, RCC_PLL2Config, RCC_PLL3Config, RCC_PREDIV1Config, RCC_PREDIV2Config, SetSysClockTo24, SetSysClockTo36, SetSysClockTo48, SetSysClockTo56, SetSysClockTo72 | I2S_Init, RCC_GetClocksFreq, SystemCoreClockUpdate |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1106 |  |  |  | uint16_t RESERVED0 |  |  |
| CRL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1107 |  |  | volatile | __IO uint16_t CRL | GPIO_Init, LedBar_AllPinsHiZ, RTC_ClearFlag, RTC_ClearITPendingBit, RTC_EnterConfigMode, RTC_ExitConfigMode, RTC_WaitForSynchro, RTC_WaitForSynchroSafe, SystemInit_ExtMemCtl | LedBar_PinModeF1, RTC_GetFlagStatus, RTC_GetITStatus, RTC_WaitForLastTask, RTC_WaitForLastTaskSafe |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1108 |  |  |  | uint16_t RESERVED1 |  |  |
| PRLH | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1109 |  |  | volatile | __IO uint16_t PRLH | RTC_SetPrescaler |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1110 |  |  |  | uint16_t RESERVED2 |  |  |
| PRLL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1111 |  |  | volatile | __IO uint16_t PRLL | RTC_SetPrescaler |  |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1112 |  |  |  | uint16_t RESERVED3 |  |  |
| DIVH | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1113 |  |  | volatile | __IO uint16_t DIVH |  | RTC_GetDivider |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1114 |  |  |  | uint16_t RESERVED4 |  |  |
| DIVL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1115 |  |  | volatile | __IO uint16_t DIVL |  | RTC_GetDivider |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1116 |  |  |  | uint16_t RESERVED5 |  |  |
| CNTH | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1117 |  |  | volatile | __IO uint16_t CNTH | RTC_SetCounter | RTC_GetCounter |
| RESERVED6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1118 |  |  |  | uint16_t RESERVED6 |  |  |
| CNTL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1119 |  |  | volatile | __IO uint16_t CNTL | RTC_SetCounter | RTC_GetCounter |
| RESERVED7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1120 |  |  |  | uint16_t RESERVED7 |  |  |
| ALRH | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1121 |  |  | volatile | __IO uint16_t ALRH | RTC_SetAlarm |  |
| RESERVED8 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1122 |  |  |  | uint16_t RESERVED8 |  |  |
| ALRL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1123 |  |  | volatile | __IO uint16_t ALRL | RTC_SetAlarm |  |
| RESERVED9 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1124 |  |  |  | uint16_t RESERVED9 |  |  |
| CLKCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1134 |  |  | volatile | __IO uint32_t CLKCR | SDIO_DeInit, SDIO_Init |  |
| ARG | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1135 |  |  | volatile | __IO uint32_t ARG | SDIO_DeInit, SDIO_SendCommand |  |
| CMD | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1136 |  |  | volatile | __IO uint32_t CMD | SDIO_DeInit, SDIO_SendCommand |  |
| RESPCMD | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1137 |  |  |  | __I uint32_t RESPCMD |  | SDIO_GetCommandResponse |
| RESP1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1138 |  |  |  | __I uint32_t RESP1 |  |  |
| RESP2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1139 |  |  |  | __I uint32_t RESP2 |  |  |
| RESP3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1140 |  |  |  | __I uint32_t RESP3 |  |  |
| RESP4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1141 |  |  |  | __I uint32_t RESP4 |  |  |
| DTIMER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1142 |  |  | volatile | __IO uint32_t DTIMER | SDIO_DataConfig, SDIO_DeInit |  |
| DLEN | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1143 |  |  | volatile | __IO uint32_t DLEN | SDIO_DataConfig, SDIO_DeInit |  |
| DCTRL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1144 |  |  | volatile | __IO uint32_t DCTRL | SDIO_DataConfig, SDIO_DeInit |  |
| DCOUNT | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1145 |  |  |  | __I uint32_t DCOUNT |  | SDIO_GetDataCounter |
| STA | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1146 |  |  |  | __I uint32_t STA |  | SDIO_GetFlagStatus, SDIO_GetITStatus |
| ICR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1147 |  |  | volatile | __IO uint32_t ICR | SDIO_ClearFlag, SDIO_ClearITPendingBit, SDIO_DeInit |  |
| MASK | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1148 |  |  | volatile | __IO uint32_t MASK | SDIO_DeInit, SDIO_ITConfig |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1149 |  |  |  | uint32_t RESERVED0[2] |  |  |
| FIFOCNT | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1150 |  |  |  | __I uint32_t FIFOCNT |  | SDIO_GetFIFOCount |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1151 |  |  |  | uint32_t RESERVED1[13] |  |  |
| FIFO | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1152 |  |  | volatile | __IO uint32_t FIFO | SDIO_WriteData | SDIO_ReadData |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1162 |  |  |  | uint16_t RESERVED0 |  |  |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1163 |  |  | volatile | __IO uint16_t CR2 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1164 |  |  |  | uint16_t RESERVED1 |  |  |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1165 |  |  | volatile | __IO uint16_t SR | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1166 |  |  |  | uint16_t RESERVED2 |  |  |
| DR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1167 |  |  | volatile | __IO uint16_t DR | CRC_CalcBlockCRC, CRC_CalcCRC, I2C_Send7bitAddress, I2C_SendData, SPI_I2S_SendData, Sci_PortIRQHandler, USART_SendData, dbg_uart_putc | ADC_GetConversionValue, BOARD_UART_IRQHandler, CRC_GetCRC, I2C_ReceiveData, InitADC_DMA, SPI_I2S_ReceiveData, Sci_PortArmReceiver, Sci_PortHandleError, USART_ReceiveData, serial_clear_overrun |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1168 |  |  |  | uint16_t RESERVED3 |  |  |
| CRCPR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1169 |  |  | volatile | __IO uint16_t CRCPR | SPI_Init | SPI_GetCRCPolynomial |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1170 |  |  |  | uint16_t RESERVED4 |  |  |
| RXCRCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1171 |  |  | volatile | __IO uint16_t RXCRCR |  | SPI_GetCRC |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1172 |  |  |  | uint16_t RESERVED5 |  |  |
| TXCRCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1173 |  |  | volatile | __IO uint16_t TXCRCR |  | SPI_GetCRC |
| RESERVED6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1174 |  |  |  | uint16_t RESERVED6 |  |  |
| I2SCFGR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1175 |  |  | volatile | __IO uint16_t I2SCFGR | I2S_Cmd, I2S_Init, SPI_Init |  |
| RESERVED7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1176 |  |  |  | uint16_t RESERVED7 |  |  |
| I2SPR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1177 |  |  | volatile | __IO uint16_t I2SPR | I2S_Init |  |
| RESERVED8 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1178 |  |  |  | uint16_t RESERVED8 |  |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1188 |  |  |  | uint16_t RESERVED0 |  |  |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1189 |  |  | volatile | __IO uint16_t CR2 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1190 |  |  |  | uint16_t RESERVED1 |  |  |
| SMCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1191 |  |  | volatile | __IO uint16_t SMCR | TIM_ETRClockMode1Config, TIM_ETRClockMode2Config, TIM_ETRConfig, TIM_EncoderInterfaceConfig, TIM_ITRxExternalClockConfig, TIM_InternalClockConfig, TIM_SelectInputTrigger, TIM_SelectMasterSlaveMode, TIM_SelectSlaveMode, TIM_TIxExternalClockConfig |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1192 |  |  |  | uint16_t RESERVED2 |  |  |
| DIER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1193 |  |  | volatile | __IO uint16_t DIER | TIM_DMACmd, TIM_ITConfig | TIM_GetITStatus |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1194 |  |  |  | uint16_t RESERVED3 |  |  |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1195 |  |  | volatile | __IO uint16_t SR | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1196 |  |  |  | uint16_t RESERVED4 |  |  |
| EGR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1197 |  |  | volatile | __IO uint16_t EGR | TIM_GenerateEvent, TIM_PrescalerConfig, TIM_TimeBaseInit |  |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1198 |  |  |  | uint16_t RESERVED5 |  |  |
| CCMR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1199 |  |  | volatile | __IO uint16_t CCMR1 | TI1_Config, TI2_Config, TIM_ClearOC1Ref, TIM_ClearOC2Ref, TIM_EncoderInterfaceConfig, TIM_ForcedOC1Config, TIM_ForcedOC2Config, TIM_OC1FastConfig, TIM_OC1Init, TIM_OC1PreloadConfig |  |
| RESERVED6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1200 |  |  |  | uint16_t RESERVED6 |  |  |
| CCMR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1201 |  |  | volatile | __IO uint16_t CCMR2 | TI3_Config, TI4_Config, TIM_ClearOC3Ref, TIM_ClearOC4Ref, TIM_ForcedOC3Config, TIM_ForcedOC4Config, TIM_OC3FastConfig, TIM_OC3Init, TIM_OC3PreloadConfig, TIM_OC4FastConfig |  |
| RESERVED7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1202 |  |  |  | uint16_t RESERVED7 |  |  |
| CCER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1203 |  |  | volatile | __IO uint16_t CCER | TI1_Config, TI2_Config, TI3_Config, TI4_Config, TIM_CCxCmd, TIM_CCxNCmd, TIM_EncoderInterfaceConfig, TIM_OC1Init, TIM_OC1NPolarityConfig, TIM_OC1PolarityConfig |  |
| RESERVED8 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1204 |  |  |  | uint16_t RESERVED8 |  |  |
| CNT | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1205 |  |  | volatile | __IO uint16_t CNT | TIM_SetCounter | TIM_GetCounter |
| RESERVED9 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1206 |  |  |  | uint16_t RESERVED9 |  |  |
| PSC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1207 |  |  | volatile | __IO uint16_t PSC | TIM_PrescalerConfig, TIM_TimeBaseInit | TIM_GetPrescaler |
| RESERVED10 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1208 |  |  |  | uint16_t RESERVED10 |  |  |
| ARR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1209 |  |  | volatile | __IO uint16_t ARR | TIM_SetAutoreload, TIM_TimeBaseInit |  |
| RESERVED11 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1210 |  |  |  | uint16_t RESERVED11 |  |  |
| RCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1211 |  |  | volatile | __IO uint16_t RCR | TIM_TimeBaseInit |  |
| RESERVED12 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1212 |  |  |  | uint16_t RESERVED12 |  |  |
| CCR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1213 |  |  | volatile | __IO uint16_t CCR1 | TIM_OC1Init, TIM_SetCompare1 | TIM_GetCapture1 |
| RESERVED13 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1214 |  |  |  | uint16_t RESERVED13 |  |  |
| CCR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1215 |  |  | volatile | __IO uint16_t CCR2 | TIM_OC2Init, TIM_SetCompare2 | TIM_GetCapture2 |
| RESERVED14 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1216 |  |  |  | uint16_t RESERVED14 |  |  |
| CCR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1217 |  |  | volatile | __IO uint16_t CCR3 | TIM_OC3Init, TIM_SetCompare3 | TIM_GetCapture3 |
| RESERVED15 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1218 |  |  |  | uint16_t RESERVED15 |  |  |
| CCR4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1219 |  |  | volatile | __IO uint16_t CCR4 | TIM_OC4Init, TIM_SetCompare4 | TIM_GetCapture4 |
| RESERVED16 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1220 |  |  |  | uint16_t RESERVED16 |  |  |
| BDTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1221 |  |  | volatile | __IO uint16_t BDTR | TIM_BDTRConfig, TIM_CtrlPWMOutputs |  |
| RESERVED17 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1222 |  |  |  | uint16_t RESERVED17 |  |  |
| DCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1223 |  |  | volatile | __IO uint16_t DCR | TIM_DMAConfig |  |
| RESERVED18 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1224 |  |  |  | uint16_t RESERVED18 |  |  |
| DMAR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1225 |  |  | volatile | __IO uint16_t DMAR |  |  |
| RESERVED19 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1226 |  |  |  | uint16_t RESERVED19 |  |  |
| RESERVED0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1236 |  |  |  | uint16_t RESERVED0 |  |  |
| DR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1237 |  |  | volatile | __IO uint16_t DR | CRC_CalcBlockCRC, CRC_CalcCRC, I2C_Send7bitAddress, I2C_SendData, SPI_I2S_SendData, Sci_PortIRQHandler, USART_SendData, dbg_uart_putc | ADC_GetConversionValue, BOARD_UART_IRQHandler, CRC_GetCRC, I2C_ReceiveData, InitADC_DMA, SPI_I2S_ReceiveData, Sci_PortArmReceiver, Sci_PortHandleError, USART_ReceiveData, serial_clear_overrun |
| RESERVED1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1238 |  |  |  | uint16_t RESERVED1 |  |  |
| BRR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1239 |  |  | volatile | __IO uint16_t BRR | GPIO_Init, GPIO_ResetBits, GPIO_WriteBit, LedBar_PinWrite, USART_Init |  |
| RESERVED2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1240 |  |  |  | uint16_t RESERVED2 |  |  |
| CR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1241 |  |  | volatile | __IO uint16_t CR1 | ADC_AnalogWatchdogCmd, ADC_AnalogWatchdogSingleChannelConfig, ADC_AutoInjectedConvCmd, ADC_DiscModeChannelCountConfig, ADC_DiscModeCmd, ADC_ITConfig, ADC_Init, ADC_InjectedDiscModeCmd, I2C_ARPCmd, I2C_AcknowledgeConfig | ADC_GetITStatus, USART_GetITStatus |
| RESERVED3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1242 |  |  |  | uint16_t RESERVED3 |  |  |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1243 |  |  | volatile | __IO uint16_t CR2 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus |
| RESERVED4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1244 |  |  |  | uint16_t RESERVED4 |  |  |
| CR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1245 |  |  | volatile | __IO uint16_t CR3 | Sci_InitCommonPort, USART_DMACmd, USART_HalfDuplexCmd, USART_Init, USART_IrDACmd, USART_IrDAConfig, USART_OneBitMethodCmd, USART_SmartCardCmd, USART_SmartCardNACKCmd | USART_GetITStatus |
| RESERVED5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1246 |  |  |  | uint16_t RESERVED5 |  |  |
| GTPR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1247 |  |  | volatile | __IO uint16_t GTPR | USART_SetGuardTime, USART_SetPrescaler |  |
| RESERVED6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1248 |  |  |  | uint16_t RESERVED6 |  |  |
| CFR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1258 |  |  | volatile | __IO uint32_t CFR | WWDG_SetPrescaler, WWDG_SetWindowValue |  |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1259 |  |  | volatile | __IO uint32_t SR | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus |
| NVIC_IRQChannelPreemptionPriority | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/misc.h | 52 |  |  |  | uint8_t NVIC_IRQChannelPreemptionPriority | BoardUart_Init, Conf_InitWakeupInputExti, InitCan_NVIC, InitTimer, LedBar_ScanTimerInit, RTC_AlarmConfig, RTC_NVIC_Config, Sci_InitCommonPort, can_hw_init, iap_uart_init | NVIC_Init |
| NVIC_IRQChannelSubPriority | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/misc.h | 57 |  |  |  | uint8_t NVIC_IRQChannelSubPriority | BoardUart_Init, Conf_InitWakeupInputExti, InitCan_NVIC, InitTimer, LedBar_ScanTimerInit, RTC_AlarmConfig, RTC_NVIC_Config, Sci_InitCommonPort, can_hw_init, iap_uart_init | NVIC_Init |
| NVIC_IRQChannelCmd | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/misc.h | 61 |  |  |  | FunctionalState NVIC_IRQChannelCmd | BoardUart_Init, Conf_InitWakeupInputExti, InitCan_NVIC, InitTimer, LedBar_ScanTimerInit, RTC_AlarmConfig, RTC_NVIC_Config, Sci_InitCommonPort, can_hw_init, iap_uart_init | NVIC_Init |
| ADC_ScanConvMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_adc.h | 52 |  |  |  | FunctionalState ADC_ScanConvMode | ADC_StructInit, InitADC_ADC1 | ADC_Init |
| ADC_ContinuousConvMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_adc.h | 56 |  |  |  | FunctionalState ADC_ContinuousConvMode | ADC_StructInit, InitADC_ADC1 | ADC_Init |
| ADC_ExternalTrigConv | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_adc.h | 60 |  |  |  | uint32_t ADC_ExternalTrigConv | ADC_StructInit, InitADC_ADC1 | ADC_Init |
| ADC_DataAlign | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_adc.h | 64 |  |  |  | uint32_t ADC_DataAlign | ADC_StructInit, InitADC_ADC1 | ADC_Init |
| ADC_NbrOfChannel | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_adc.h | 68 |  |  |  | uint8_t ADC_NbrOfChannel | ADC_StructInit, InitADC_ADC1 | ADC_Init |
| CAN_Mode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 55 |  |  |  | uint8_t CAN_Mode | CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init | CAN_Init |
| CAN_SJW | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 58 |  |  |  | uint8_t CAN_SJW | CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init | CAN_Init |
| CAN_BS1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 62 |  |  |  | uint8_t CAN_BS1 | CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init | CAN_Init |
| CAN_BS2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 68 |  |  |  | uint8_t CAN_BS2 | CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init | CAN_Init |
| CAN_TTCM | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 72 |  |  |  | FunctionalState CAN_TTCM | CAN_Init, CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init |  |
| CAN_ABOM | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 77 |  |  |  | FunctionalState CAN_ABOM | CAN_Init, CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init |  |
| CAN_AWUM | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 81 |  |  |  | FunctionalState CAN_AWUM | CAN_Init, CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init |  |
| CAN_NART | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 85 |  |  |  | FunctionalState CAN_NART | CAN_Init, CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init |  |
| CAN_RFLM | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 89 |  |  |  | FunctionalState CAN_RFLM | CAN_Init, CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init |  |
| CAN_TXFP | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 93 |  |  |  | FunctionalState CAN_TXFP | CAN_Init, CAN_StructInit, InitCan_CAN1, can_hw_init, iap_can_init |  |
| CAN_FilterIdLow | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 108 |  |  |  | uint16_t CAN_FilterIdLow | InitCan_Filter, can_hw_init, iap_can_init | CAN_FilterInit |
| CAN_FilterMaskIdHigh | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 112 |  |  |  | uint16_t CAN_FilterMaskIdHigh | InitCan_Filter, can_hw_init, iap_can_init | CAN_FilterInit |
| CAN_FilterMaskIdLow | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 116 |  |  |  | uint16_t CAN_FilterMaskIdLow | InitCan_Filter, can_hw_init, iap_can_init | CAN_FilterInit |
| CAN_FilterFIFOAssignment | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 121 |  |  |  | uint16_t CAN_FilterFIFOAssignment | CAN_FilterInit, InitCan_Filter, can_hw_init, iap_can_init |  |
| CAN_FilterNumber | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 126 |  |  |  | uint8_t CAN_FilterNumber | InitCan_Filter, can_hw_init, iap_can_init | CAN_FilterInit |
| CAN_FilterMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 129 |  |  |  | uint8_t CAN_FilterMode | CAN_FilterInit, InitCan_Filter, can_hw_init, iap_can_init |  |
| CAN_FilterScale | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 131 |  |  |  | uint8_t CAN_FilterScale | CAN_FilterInit, InitCan_Filter, can_hw_init, iap_can_init |  |
| CAN_FilterActivation | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 134 |  |  |  | FunctionalState CAN_FilterActivation | CAN_FilterInit, InitCan_Filter, can_hw_init, iap_can_init |  |
| ExtId | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 147 |  |  |  | uint32_t ExtId | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, can_handle_rx, can_send_ack, can_send_nack | CAN_Transmit, can_handle_data, can_handle_start, can_rx_push |
| IDE | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 150 |  |  |  | uint8_t IDE | CAN_Receive, CAN_Transmit, CanFeidao_SendFrame, CtBoard_CanSend, can_rx_push, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame, feidao_can_transmit | can_handle_rx, feidao_can_handle_rx_msg |
| RTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 153 |  |  |  | uint8_t RTR | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame | CAN_Transmit, can_handle_rx |
| DLC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 157 |  |  |  | uint8_t DLC | CAN_Receive, CAN_Transmit, CanFeidao_SendFrame, CtBoard_CanSend, can_handle_ctrl, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame, feidao_can_handle_rx_msg | can_handle_commit, can_handle_data, can_handle_end, can_handle_hello, can_handle_start, can_rx_push, feidao_can_enqueue_tx |
| Data | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 161 |  |  |  | uint8_t Data[8] | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, TwiSendData, feidao_can_app_send_ack, feidao_can_app_send_word_frame | BKP_WriteBackupRegister, CAN_Transmit, CEC_SendDataByte, CRC_CalcCRC, DAC_SetChannel1Data, DAC_SetChannel2Data, FLASH_ProgramHalfWord, FLASH_ProgramOptionByteData, FLASH_ProgramWord, I2C_SendData |
| ExtId | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 175 |  |  |  | uint32_t ExtId | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, can_handle_rx, can_send_ack, can_send_nack | CAN_Transmit, can_handle_data, can_handle_start, can_rx_push |
| IDE | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 178 |  |  |  | uint8_t IDE | CAN_Receive, CAN_Transmit, CanFeidao_SendFrame, CtBoard_CanSend, can_rx_push, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame, feidao_can_transmit | can_handle_rx, feidao_can_handle_rx_msg |
| RTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 181 |  |  |  | uint8_t RTR | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame | CAN_Transmit, can_handle_rx |
| DLC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 185 |  |  |  | uint8_t DLC | CAN_Receive, CAN_Transmit, CanFeidao_SendFrame, CtBoard_CanSend, can_handle_ctrl, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame, feidao_can_handle_rx_msg | can_handle_commit, can_handle_data, can_handle_end, can_handle_hello, can_handle_start, can_rx_push, feidao_can_enqueue_tx |
| Data | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 189 |  |  |  | uint8_t Data[8] | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, TwiSendData, feidao_can_app_send_ack, feidao_can_app_send_word_frame | BKP_WriteBackupRegister, CAN_Transmit, CEC_SendDataByte, CRC_CalcCRC, DAC_SetChannel1Data, DAC_SetChannel2Data, FLASH_ProgramHalfWord, FLASH_ProgramOptionByteData, FLASH_ProgramWord, I2C_SendData |
| FMI | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 192 |  |  |  | uint8_t FMI | CAN_Receive |  |
| CEC_BitPeriodMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_cec.h | 52 |  |  |  | uint16_t CEC_BitPeriodMode |  | CEC_Init |
| DAC_WaveGeneration | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dac.h | 52 |  |  |  | uint32_t DAC_WaveGeneration | DAC_StructInit | DAC_Init |
| DAC_LFSRUnmask_TriangleAmplitude | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dac.h | 55 |  |  |  | uint32_t DAC_LFSRUnmask_TriangleAmplitude | DAC_StructInit | DAC_Init |
| DAC_OutputBuffer | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dac.h | 59 |  |  |  | uint32_t DAC_OutputBuffer | DAC_StructInit | DAC_Init |
| DMA_MemoryBaseAddr | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 52 |  |  |  | uint32_t DMA_MemoryBaseAddr | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_DIR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 54 |  |  |  | uint32_t DMA_DIR | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_BufferSize | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 56 |  |  |  | uint32_t DMA_BufferSize | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_PeripheralInc | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 59 |  |  |  | uint32_t DMA_PeripheralInc | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_MemoryInc | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 63 |  |  |  | uint32_t DMA_MemoryInc | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_PeripheralDataSize | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 66 |  |  |  | uint32_t DMA_PeripheralDataSize | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_MemoryDataSize | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 69 |  |  |  | uint32_t DMA_MemoryDataSize | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_Mode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 72 |  |  |  | uint32_t DMA_Mode | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_Priority | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 75 |  |  |  | uint32_t DMA_Priority | DMA_StructInit, InitADC_DMA | DMA_Init |
| DMA_M2M | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_dma.h | 80 |  |  |  | uint32_t DMA_M2M | DMA_StructInit, InitADC_DMA | DMA_Init |
| EXTI_Mode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_exti.h | 78 |  |  |  | EXTIMode_TypeDef EXTI_Mode | Conf_InitWakeupInputExti, EXTI_StructInit, LowPower_ConfigWakeupExti, RTC_AlarmConfig | EXTI_Init |
| EXTI_Trigger | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_exti.h | 81 |  |  |  | EXTITrigger_TypeDef EXTI_Trigger | Conf_InitWakeupInputExti, EXTI_Init, EXTI_StructInit, LowPower_ConfigWakeupExti, RTC_AlarmConfig |  |
| EXTI_LineCmd | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_exti.h | 84 |  |  |  | FunctionalState EXTI_LineCmd | Conf_InitWakeupInputExti, EXTI_StructInit, LowPower_ConfigWakeupExti, RTC_AlarmConfig | EXTI_Init |
| FSMC_AddressHoldTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 52 |  |  |  | uint32_t FSMC_AddressHoldTime | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_DataSetupTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 57 |  |  |  | uint32_t FSMC_DataSetupTime | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_BusTurnAroundDuration | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 62 |  |  |  | uint32_t FSMC_BusTurnAroundDuration | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_CLKDivision | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 67 |  |  |  | uint32_t FSMC_CLKDivision | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_DataLatency | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 72 |  |  |  | uint32_t FSMC_DataLatency | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_AccessMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 76 |  |  |  | uint32_t FSMC_AccessMode | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_DataAddressMux | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 94 |  |  |  | uint32_t FSMC_DataAddressMux | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_MemoryType | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 97 |  |  |  | uint32_t FSMC_MemoryType | FSMC_NORSRAMInit, FSMC_NORSRAMStructInit |  |
| FSMC_MemoryDataWidth | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 101 |  |  |  | uint32_t FSMC_MemoryDataWidth | FSMC_NANDStructInit, FSMC_NORSRAMStructInit | FSMC_NANDInit, FSMC_NORSRAMInit |
| FSMC_BurstAccessMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 105 |  |  |  | uint32_t FSMC_BurstAccessMode | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_AsynchronousWait | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 108 |  |  |  | uint32_t FSMC_AsynchronousWait | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_WaitSignalPolarity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 112 |  |  |  | uint32_t FSMC_WaitSignalPolarity | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_WrapMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 116 |  |  |  | uint32_t FSMC_WrapMode | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_WaitSignalActive | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 120 |  |  |  | uint32_t FSMC_WaitSignalActive | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_WriteOperation | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 124 |  |  |  | uint32_t FSMC_WriteOperation | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_WaitSignal | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 129 |  |  |  | uint32_t FSMC_WaitSignal | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_ExtendedMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 132 |  |  |  | uint32_t FSMC_ExtendedMode | FSMC_NORSRAMInit, FSMC_NORSRAMStructInit |  |
| FSMC_WriteBurst | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 136 |  |  |  | uint32_t FSMC_WriteBurst | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_ReadWriteTimingStruct | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 139 |  |  |  | FSMC_NORSRAMTimingInitTypeDef* FSMC_ReadWriteTimingStruct | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_WriteTimingStruct | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 142 |  |  |  | FSMC_NORSRAMTimingInitTypeDef* FSMC_WriteTimingStruct | FSMC_NORSRAMStructInit | FSMC_NORSRAMInit |
| FSMC_WaitSetupTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 153 |  |  |  | uint32_t FSMC_WaitSetupTime | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_HoldSetupTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 159 |  |  |  | uint32_t FSMC_HoldSetupTime | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_HiZSetupTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 165 |  |  |  | uint32_t FSMC_HiZSetupTime | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_Waitfeature | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 185 |  |  |  | uint32_t FSMC_Waitfeature | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_MemoryDataWidth | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 188 |  |  |  | uint32_t FSMC_MemoryDataWidth | FSMC_NANDStructInit, FSMC_NORSRAMStructInit | FSMC_NANDInit, FSMC_NORSRAMInit |
| FSMC_ECC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 191 |  |  |  | uint32_t FSMC_ECC | FSMC_NANDStructInit | FSMC_NANDInit |
| FSMC_ECCPageSize | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 194 |  |  |  | uint32_t FSMC_ECCPageSize | FSMC_NANDStructInit | FSMC_NANDInit |
| FSMC_TCLRSetupTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 197 |  |  |  | uint32_t FSMC_TCLRSetupTime | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_TARSetupTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 200 |  |  |  | uint32_t FSMC_TARSetupTime | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_CommonSpaceTimingStruct | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 204 |  |  |  | FSMC_NAND_PCCARDTimingInitTypeDef* FSMC_CommonSpaceTimingStruct | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_AttributeSpaceTimingStruct | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 208 |  |  |  | FSMC_NAND_PCCARDTimingInitTypeDef* FSMC_AttributeSpaceTimingStruct | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_TCLRSetupTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 219 |  |  |  | uint32_t FSMC_TCLRSetupTime | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_TARSetupTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 222 |  |  |  | uint32_t FSMC_TARSetupTime | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_CommonSpaceTimingStruct | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 226 |  |  |  | FSMC_NAND_PCCARDTimingInitTypeDef* FSMC_CommonSpaceTimingStruct | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_AttributeSpaceTimingStruct | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 231 |  |  |  | FSMC_NAND_PCCARDTimingInitTypeDef* FSMC_AttributeSpaceTimingStruct | FSMC_NANDStructInit, FSMC_PCCARDStructInit | FSMC_NANDInit, FSMC_PCCARDInit |
| FSMC_IOSpaceTimingStruct | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_fsmc.h | 233 |  |  |  | FSMC_NAND_PCCARDTimingInitTypeDef* FSMC_IOSpaceTimingStruct | FSMC_PCCARDStructInit | FSMC_PCCARDInit |
| GPIO_Speed | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_gpio.h | 93 |  |  |  | GPIOSpeed_TypeDef GPIO_Speed | Conf_InitGpioMode, GPIO_StructInit, InitAFE1_Sleep, InitCan_GPIO, Sci_InitCommonPort, board_gpio_init, board_uart_gpio_init, can_gpio_init, iap_can_init, iap_gpio_init | GPIO_Init |
| GPIO_Mode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_gpio.h | 96 |  |  |  | GPIOMode_TypeDef GPIO_Mode | Conf_InitGpioMode, GPIO_Init, GPIO_StructInit, InitADC_GPIO, InitAFE1_Sleep, InitCan_GPIO, Sci_InitCommonPort, board_gpio_init, board_uart_gpio_init, can_gpio_init |  |
| I2C_Mode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_i2c.h | 52 |  |  |  | uint16_t I2C_Mode | I2C_StructInit | I2C_Init |
| I2C_DutyCycle | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_i2c.h | 55 |  |  |  | uint16_t I2C_DutyCycle | I2C_Init, I2C_StructInit | I2C_FastModeDutyCycleConfig |
| I2C_OwnAddress1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_i2c.h | 58 |  |  |  | uint16_t I2C_OwnAddress1 | I2C_StructInit | I2C_Init |
| I2C_Ack | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_i2c.h | 61 |  |  |  | uint16_t I2C_Ack | I2C_StructInit | I2C_Init |
| I2C_AcknowledgedAddress | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_i2c.h | 64 |  |  |  | uint16_t I2C_AcknowledgedAddress | I2C_StructInit | I2C_Init |
| HCLK_Frequency | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_rcc.h | 48 |  |  |  | uint32_t HCLK_Frequency | RCC_GetClocksFreq |  |
| PCLK1_Frequency | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_rcc.h | 49 |  |  |  | uint32_t PCLK1_Frequency | RCC_GetClocksFreq | I2C_Init, USART_Init |
| PCLK2_Frequency | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_rcc.h | 50 |  |  |  | uint32_t PCLK2_Frequency | RCC_GetClocksFreq | USART_Init |
| ADCCLK_Frequency | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_rcc.h | 51 |  |  |  | uint32_t ADCCLK_Frequency | RCC_GetClocksFreq |  |
| SDIO_ClockBypass | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 48 |  |  |  | uint32_t SDIO_ClockBypass | SDIO_StructInit | SDIO_Init |
| SDIO_ClockPowerSave | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 51 |  |  |  | uint32_t SDIO_ClockPowerSave | SDIO_StructInit | SDIO_Init |
| SDIO_BusWide | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 55 |  |  |  | uint32_t SDIO_BusWide | SDIO_StructInit | SDIO_Init |
| SDIO_HardwareFlowControl | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 59 |  |  |  | uint32_t SDIO_HardwareFlowControl | SDIO_StructInit | SDIO_Init |
| SDIO_ClockDiv | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 62 |  |  |  | uint8_t SDIO_ClockDiv | SDIO_StructInit | SDIO_Init |
| SDIO_CmdIndex | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 72 |  |  |  | uint32_t SDIO_CmdIndex | SDIO_CmdStructInit | SDIO_SendCommand |
| SDIO_Response | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 77 |  |  |  | uint32_t SDIO_Response | SDIO_CmdStructInit | SDIO_SendCommand |
| SDIO_Wait | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 79 |  |  |  | uint32_t SDIO_Wait | SDIO_CmdStructInit | SDIO_SendCommand |
| SDIO_CPSM | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 82 |  |  |  | uint32_t SDIO_CPSM | SDIO_CmdStructInit | SDIO_SendCommand |
| SDIO_DataLength | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 92 |  |  |  | uint32_t SDIO_DataLength | SDIO_DataStructInit | SDIO_DataConfig |
| SDIO_DataBlockSize | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 94 |  |  |  | uint32_t SDIO_DataBlockSize | SDIO_DataStructInit | SDIO_DataConfig |
| SDIO_TransferDir | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 96 |  |  |  | uint32_t SDIO_TransferDir | SDIO_DataStructInit | SDIO_DataConfig |
| SDIO_TransferMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 99 |  |  |  | uint32_t SDIO_TransferMode | SDIO_DataStructInit | SDIO_DataConfig |
| SDIO_DPSM | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_sdio.h | 103 |  |  |  | uint32_t SDIO_DPSM | SDIO_DataStructInit | SDIO_DataConfig |
| SPI_Mode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 52 |  |  |  | uint16_t SPI_Mode | SPI_StructInit | SPI_Init |
| SPI_DataSize | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 55 |  |  |  | uint16_t SPI_DataSize | SPI_StructInit | SPI_DataSizeConfig, SPI_Init |
| SPI_CPOL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 58 |  |  |  | uint16_t SPI_CPOL | SPI_StructInit | SPI_Init |
| SPI_CPHA | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 61 |  |  |  | uint16_t SPI_CPHA | SPI_StructInit | SPI_Init |
| SPI_NSS | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 64 |  |  |  | uint16_t SPI_NSS | SPI_StructInit | SPI_Init |
| SPI_BaudRatePrescaler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 67 |  |  |  | uint16_t SPI_BaudRatePrescaler | SPI_StructInit | SPI_Init |
| SPI_FirstBit | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 71 |  |  |  | uint16_t SPI_FirstBit | SPI_StructInit | SPI_Init |
| SPI_CRCPolynomial | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 77 |  |  |  | uint16_t SPI_CRCPolynomial | SPI_StructInit | SPI_Init |
| I2S_Standard | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 90 |  |  |  | uint16_t I2S_Standard | I2S_StructInit | I2S_Init |
| I2S_DataFormat | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 93 |  |  |  | uint16_t I2S_DataFormat | I2S_Init, I2S_StructInit |  |
| I2S_MCLKOutput | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 96 |  |  |  | uint16_t I2S_MCLKOutput | I2S_Init, I2S_StructInit |  |
| I2S_AudioFreq | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 99 |  |  |  | uint32_t I2S_AudioFreq | I2S_Init, I2S_StructInit |  |
| I2S_CPOL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_spi.h | 102 |  |  |  | uint16_t I2S_CPOL | I2S_StructInit | I2S_Init |
| TIM_CounterMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 53 |  |  |  | uint16_t TIM_CounterMode | InitADC_TIMER, InitTimer, LedBar_ScanTimerInit, TIM_TimeBaseStructInit | TIM_CounterModeConfig, TIM_TimeBaseInit |
| TIM_Period | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 56 |  |  |  | uint16_t TIM_Period | InitADC_TIMER, InitTimer, LedBar_ScanTimerInit, TIM_TimeBaseStructInit | TIM_TimeBaseInit |
| TIM_ClockDivision | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 59 |  |  |  | uint16_t TIM_ClockDivision | InitADC_TIMER, InitTimer, LedBar_ScanTimerInit, TIM_TimeBaseStructInit | TIM_TimeBaseInit |
| TIM_RepetitionCounter | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 63 |  |  |  | uint8_t TIM_RepetitionCounter | InitADC_TIMER, InitTimer, LedBar_ScanTimerInit, TIM_TimeBaseStructInit | TIM_TimeBaseInit |
| TIM_OutputState | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 82 |  |  |  | uint16_t TIM_OutputState | InitADC_TIMER, TIM_OCStructInit | TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init |
| TIM_OutputNState | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 85 |  |  |  | uint16_t TIM_OutputNState | TIM_OCStructInit | TIM_OC1Init, TIM_OC2Init, TIM_OC3Init |
| TIM_Pulse | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 88 |  |  |  | uint16_t TIM_Pulse | InitADC_TIMER, TIM_OCStructInit | TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init |
| TIM_OCPolarity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 92 |  |  |  | uint16_t TIM_OCPolarity | InitADC_TIMER, TIM_OCStructInit | TIM_OC1Init, TIM_OC1PolarityConfig, TIM_OC2Init, TIM_OC2PolarityConfig, TIM_OC3Init, TIM_OC3PolarityConfig, TIM_OC4Init, TIM_OC4PolarityConfig |
| TIM_OCNPolarity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 95 |  |  |  | uint16_t TIM_OCNPolarity | TIM_OCStructInit | TIM_OC1Init, TIM_OC1NPolarityConfig, TIM_OC2Init, TIM_OC2NPolarityConfig, TIM_OC3Init, TIM_OC3NPolarityConfig |
| TIM_OCIdleState | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 98 |  |  |  | uint16_t TIM_OCIdleState | TIM_OCStructInit | TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init |
| TIM_OCNIdleState | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 102 |  |  |  | uint16_t TIM_OCNIdleState | TIM_OCStructInit | TIM_OC1Init, TIM_OC2Init, TIM_OC3Init |
| TIM_ICPolarity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 118 |  |  |  | uint16_t TIM_ICPolarity | TIM_ICStructInit, TIM_PWMIConfig | TI1_Config, TI2_Config, TI3_Config, TI4_Config, TIM_ICInit, TIM_TIxExternalClockConfig |
| TIM_ICSelection | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 121 |  |  |  | uint16_t TIM_ICSelection | TIM_ICStructInit, TIM_PWMIConfig | TI1_Config, TI2_Config, TI3_Config, TI4_Config, TIM_ICInit |
| TIM_ICPrescaler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 124 |  |  |  | uint16_t TIM_ICPrescaler | TIM_ICStructInit | TIM_ICInit, TIM_PWMIConfig |
| TIM_ICFilter | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 127 |  |  |  | uint16_t TIM_ICFilter | TIM_ICStructInit | TI1_Config, TI2_Config, TI3_Config, TI4_Config, TIM_ICInit, TIM_PWMIConfig |
| TIM_OSSIState | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 142 |  |  |  | uint16_t TIM_OSSIState | TIM_BDTRStructInit | TIM_BDTRConfig |
| TIM_LOCKLevel | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 145 |  |  |  | uint16_t TIM_LOCKLevel | TIM_BDTRStructInit | TIM_BDTRConfig |
| TIM_DeadTime | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 148 |  |  |  | uint16_t TIM_DeadTime | TIM_BDTRStructInit | TIM_BDTRConfig |
| TIM_Break | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 151 |  |  |  | uint16_t TIM_Break | TIM_BDTRStructInit | TIM_BDTRConfig |
| TIM_BreakPolarity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 155 |  |  |  | uint16_t TIM_BreakPolarity | TIM_BDTRStructInit | TIM_BDTRConfig |
| TIM_AutomaticOutput | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 158 |  |  |  | uint16_t TIM_AutomaticOutput | TIM_BDTRStructInit | TIM_BDTRConfig |
| USART_WordLength | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_usart.h | 52 |  |  |  | uint16_t USART_WordLength | BoardUart_Init, Sci_InitCommonPort, USART_StructInit, iap_uart_init | USART_Init |
| USART_StopBits | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_usart.h | 57 |  |  |  | uint16_t USART_StopBits | BoardUart_Init, Sci_InitCommonPort, USART_StructInit, iap_uart_init | USART_Init |
| USART_Parity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_usart.h | 60 |  |  |  | uint16_t USART_Parity | BoardUart_Init, Sci_InitCommonPort, USART_StructInit, iap_uart_init | USART_Init |
| USART_Mode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_usart.h | 63 |  |  |  | uint16_t USART_Mode | BoardUart_Init, Sci_InitCommonPort, USART_StructInit, iap_uart_init | USART_Init |
| USART_HardwareFlowControl | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_usart.h | 70 |  |  |  | uint16_t USART_HardwareFlowControl | BoardUart_Init, Sci_InitCommonPort, USART_StructInit, iap_uart_init | USART_Init |
| USART_CPOL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_usart.h | 85 |  |  |  | uint16_t USART_CPOL | USART_ClockStructInit | USART_ClockInit |
| USART_CPHA | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_usart.h | 88 |  |  |  | uint16_t USART_CPHA | USART_ClockStructInit | USART_ClockInit |
| USART_LastBit | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_usart.h | 91 |  |  |  | uint16_t USART_LastBit | USART_ClockStructInit | USART_ClockInit |
| ADCPrescTable | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_rcc.c | 193 | static |  |  | static __I uint8_t ADCPrescTable[4] = {2, 4, 6, 8} |  | RCC_GetClocksFreq |
| result | 103 + 309/Project/Source/ADC.c | 6 |  |  |  | INT32 result[ADC_NUM] | ADC_ClearTypeCOutCurrent, ADC_UpdateMosTemp, ADC_UpdateTypeCCurrent, ADC_UpdateVbc, AFE_CheckStatus, AFE_IsReady, CtBoard_SetCanBitrate, FlashErasePageVerified, FlashProgramBytesVerified, FlashProgramHalfWordVerified | ADC_GetResult, MonitorAFE_UpdateChannel |
| last | 103 + 309/Project/Source/ADC.c | 7 |  |  |  | UINT32 last | ADC_ResetAnlogCalSchedule, App_AnlogCal, LP_RecordLastSleepSeconds, RTC_WKTimeConfig, soc_table_percent | LP_GetLastSleepSeconds, RTC_GetLastWakeupPeriodSeconds, SystemDebug_Snapshot |
| vbat | 103 + 309/Project/Source/ADC.c | 8 |  |  |  | UINT32 vbat | ADC_UpdateVbc, InitADC | ADC_GetVbatMilliVolt |
| typec | 103 + 309/Project/Source/ADC.c | 9 |  |  |  | UINT16 typec | ADC_ClearTypeCOutCurrent, ADC_UpdateTypeCCurrent | ADC_GetTypeCOutCurrentMilliAmp |
| discard | 103 + 309/Project/Source/ADC.c | 10 |  |  |  | UINT8 discard | ADC_ResetAnlogCalSchedule | App_AnlogCal |
| ready | 103 + 309/Project/Source/ADC.c | 11 |  |  |  | UINT8 ready | ADC_ResetAnlogCalSchedule, ADC_StopForLowPower, App_AnlogCal, SystemDebug_Snapshot | ADC_IsReady, DbgPrint_LP, SystemDebug_RefreshModuleStates |
| s_adc | 103 + 309/Project/Source/ADC.c | 13 | static |  |  | static ADC_RUNTIME s_adc | ADC_ClearTypeCOutCurrent, ADC_ResetAnlogCalSchedule, ADC_StopForLowPower, ADC_UpdateMosTemp, ADC_UpdateTypeCCurrent, ADC_UpdateVbc, App_AnlogCal, InitADC | ADC_DebugWatchBind, ADC_GetRaw, ADC_GetResult, ADC_GetTypeCOutCurrentMilliAmp, ADC_GetVbatMilliVolt, ADC_IsReady, InitADC_DMA |
| handler | 103 + 309/Project/Source/CanFeidaoFrames.c | 24 |  |  |  | CanFeidao_SendHandler handler |  | CanFeidao_SendNextPending |
| s_can_feidao_dispatch | 103 + 309/Project/Source/CanFeidaoFrames.c | 26 | static |  |  | static const CanFeidao_FrameDispatch s_can_feidao_dispatch[] = { {CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS, CanFeidao_SendVoltageCurrent1000ms}, {CAN_FEIDAO_MSG_SOC_1000MS, CanFeidao_SendSoc1000ms}, {CAN_FEIDAO_MSG_CAP_5... |  | CanFeidaoFrames_DebugWatchBind, CanFeidao_SendNextPending |
| source | 103 + 309/Project/Source/Can_HDX.c | 54 |  |  |  | UINT8 source | RtcSleep_AfePortHasAfeWake, RtcSleep_AfePortHasCurrentWake, RtcSleep_PortOnWakeupSource, feidao_can_enqueue_tx, isException | RtcSleep_PortHasAfeWake, RtcSleep_PortHasCurrentWake, SystemDebug_RecordWatchdogFeed, feidao_can_queue_has_request, feidao_can_service_tx, feidao_can_transmit, soc_watch_set_calib_source |
| head | 103 + 309/Project/Source/Can_HDX.c | 60 |  |  |  | UINT8 head | SystemDebug_Event, feidao_can_clear_tx_queue, feidao_can_dequeue_tx | SystemDebug_ReadEventRing, feidao_can_queue_has_request |
| tail | 103 + 309/Project/Source/Can_HDX.c | 61 |  |  |  | UINT8 tail | feidao_can_clear_tx_queue, feidao_can_enqueue_tx |  |
| count | 103 + 309/Project/Source/Can_HDX.c | 62 |  |  |  | UINT8 count | CtCan_AppReadRegs, CtCan_AppWriteRegs, CtDebugLog_EncodeLatest, Sci_PutZeroWordsBE, Sci_RangeFits, SystemDebug_Event, feidao_can_clear_tx_queue, feidao_can_dequeue_tx, feidao_can_enqueue_tx, handle_bms_read | Can_GetDebugSnapshot, DbgPrint_EventRing, EEPROM_WordBlockInRange, Sci_ApplyOtherElementSideEffects, Sci_ApplyProtectSideEffects, Sci_CopyWords, Sci_PutBytes, Sci_RangeOverlaps, Sci_WrValuesInRange, Sci_WriteWordsFromRequest |
| mailbox | 103 + 309/Project/Source/Can_HDX.c | 63 |  |  |  | UINT8 mailbox | AppUpgrade_IsIapRequested, AppUpgrade_RequestIap, CtBoard_CanSend, CtBoot_ClearRequest, CtBoot_RequestIap, InitCan, boot_consume_iap_request, boot_request_valid, can_has_sleep_blocking_work, can_transmit | can_has_pending_work, feidao_can_cancel_tx, feidao_can_clear_tx_done |
| mailbox_source | 103 + 309/Project/Source/Can_HDX.c | 64 |  |  |  | UINT8 mailbox_source | InitCan, feidao_can_abort_tx, feidao_can_service_tx | can_has_sleep_blocking_work |
| start_tick | 103 + 309/Project/Source/Can_HDX.c | 65 |  |  |  | UINT32 start_tick | feidao_can_service_tx | feidao_can_tick_elapsed |
| last_1000ms_tick | 103 + 309/Project/Source/Can_HDX.c | 71 |  |  |  | UINT32 last_1000ms_tick | feidao_can_schedule_periodic |  |
| last_5000ms_tick | 103 + 309/Project/Source/Can_HDX.c | 72 |  |  |  | UINT32 last_5000ms_tick | feidao_can_schedule_periodic |  |
| schedule_init | 103 + 309/Project/Source/Can_HDX.c | 73 |  |  |  | UINT8 schedule_init | InitCan, feidao_can_schedule_periodic |  |
| cmd_tail | 103 + 309/Project/Source/Can_HDX.c | 79 |  |  | volatile | volatile UINT8 cmd_tail | feidao_can_clear_app_cmd_queue, feidao_can_queue_app_cmd |  |
| cmd_count | 103 + 309/Project/Source/Can_HDX.c | 80 |  |  | volatile | volatile UINT8 cmd_count | feidao_can_clear_app_cmd_queue, feidao_can_queue_app_cmd, feidao_can_take_app_cmd | can_has_pending_work, can_has_sleep_blocking_work |
| cmd_queue | 103 + 309/Project/Source/Can_HDX.c | 81 |  |  |  | UINT8 cmd_queue[FEIDAO_CAN_APP_CMD_QUEUE_SIZE][8] |  | feidao_can_queue_app_cmd, feidao_can_take_app_cmd |
| write_pending | 103 + 309/Project/Source/Can_HDX.c | 82 |  |  |  | UINT8 write_pending | feidao_can_handle_app_cmd_data |  |
| write_addr | 103 + 309/Project/Source/Can_HDX.c | 83 |  |  |  | UINT16 write_addr | feidao_can_handle_app_cmd_data |  |
| write_value_hi | 103 + 309/Project/Source/Can_HDX.c | 84 |  |  |  | UINT8 write_value_hi | feidao_can_handle_app_cmd_data |  |
| enter_iap_delay_ticks | 103 + 309/Project/Source/Can_HDX.c | 85 |  |  |  | UINT8 enter_iap_delay_ticks | InitCan, feidao_can_handle_app_cmd_data, feidao_can_service_enter_iap_delay |  |
| read_block_words | 103 + 309/Project/Source/Can_HDX.c | 86 |  |  |  | UINT16 read_block_words[FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS] |  | feidao_can_handle_app_cmd_data, feidao_can_service_read_block_stream |
| read_block_count | 103 + 309/Project/Source/Can_HDX.c | 87 |  |  |  | UINT8 read_block_count | feidao_can_start_read_block_stream, feidao_can_stop_read_block_stream | feidao_can_service_read_block_stream |
| read_block_index | 103 + 309/Project/Source/Can_HDX.c | 88 |  |  |  | UINT8 read_block_index | feidao_can_service_read_block_stream, feidao_can_start_read_block_stream, feidao_can_stop_read_block_stream |  |
| read_block_active | 103 + 309/Project/Source/Can_HDX.c | 89 |  |  |  | UINT8 read_block_active | InitCan, feidao_can_service_read_block_stream, feidao_can_start_read_block_stream, feidao_can_stop_read_block_stream | can_has_pending_work, can_has_sleep_blocking_work |
| read_block_last_tick | 103 + 309/Project/Source/Can_HDX.c | 90 |  |  |  | UINT32 read_block_last_tick | feidao_can_service_read_block_stream, feidao_can_start_read_block_stream |  |
| s_tx | 103 + 309/Project/Source/Can_HDX.c | 92 | static |  |  | static FeidaoCanTxRuntime s_tx = { {0}, 0U, 0U, 0U, CAN_TxStatus_NoMailBox, FEIDAO_CAN_TX_SOURCE_NONE, 0U } | InitCan, can_has_sleep_blocking_work, feidao_can_abort_tx, feidao_can_clear_tx_queue, feidao_can_dequeue_tx, feidao_can_enqueue_tx, feidao_can_service_tx | Can_DebugWatchBind, Can_GetDebugSnapshot, can_has_pending_work, feidao_can_queue_has_request, feidao_can_service_read_block_stream, respond |
| s_runtime | 103 + 309/Project/Source/Can_HDX.c | 102 | static |  |  | static FeidaoCanRuntime s_runtime | App_Can, InitCan, feidao_can_schedule_periodic | Can_DebugWatchBind, feidao_can_start_read_block_stream |
| s_app | 103 + 309/Project/Source/Can_HDX.c | 103 | static |  |  | static FeidaoCanAppRuntime s_app | InitCan, feidao_can_clear_app_cmd_queue, feidao_can_handle_app_cmd_data, feidao_can_queue_app_cmd, feidao_can_service_enter_iap_delay, feidao_can_service_read_block_stream, feidao_can_start_read_block_stream, feidao_can_stop_read_block_stream, feidao_can_take_app_cmd | Can_DebugWatchBind, can_has_pending_work, can_has_sleep_blocking_work |
| u8MaxCnt | 103 + 309/Project/Source/DataDeal.c | 22 |  |  |  | UINT8 u8MaxCnt |  | AfeCurrent_StartupZeroCal |
| u8DiscardCnt | 103 + 309/Project/Source/DataDeal.c | 23 |  |  |  | UINT8 u8DiscardCnt |  | AfeCurrent_StartupZeroCal |
| u16SettleMs | 103 + 309/Project/Source/DataDeal.c | 24 |  |  |  | UINT16 u16SettleMs |  | AfeCurrent_StartupZeroCal |
| u16IntervalMs | 103 + 309/Project/Source/DataDeal.c | 25 |  |  |  | UINT16 u16IntervalMs |  | AfeCurrent_StartupZeroCal |
| s_stAfeCurrentColdStartupZeroParam | 103 + 309/Project/Source/DataDeal.c | 27 | static |  |  | static const AFE_CURRENT_STARTUP_ZERO_PARAM s_stAfeCurrentColdStartupZeroParam = {4U, 32U, 6U, 800U, 25U} |  | AfeCurrent_GetStartupZeroParam |
| s_stAfeCurrentWarmStartupZeroParam | 103 + 309/Project/Source/DataDeal.c | 29 | static |  |  | static const AFE_CURRENT_STARTUP_ZERO_PARAM s_stAfeCurrentWarmStartupZeroParam = {4U, 16U, 2U, 120U, 20U} |  | AfeCurrent_GetStartupZeroParam |
| zeroOffsetRawQ4 | 103 + 309/Project/Source/DataDeal.c | 34 |  |  |  | INT32 zeroOffsetRawQ4 | AfeCurrent_PrepareStartupZero, DataLoad_CurrentApplyAutoZero, DataLoad_CurrentMarkZeroPending, DataLoad_CurrentSetZeroOffset |  |
| lastRawSigned | 103 + 309/Project/Source/DataDeal.c | 35 |  |  |  | INT32 lastRawSigned | AfeCurrent_PrepareStartupZero, DataLoad_CurrentApplyAutoZero, DataLoad_CurrentMarkZeroPending, DataLoad_CurrentSetZeroOffset |  |
| zeroStableCnt | 103 + 309/Project/Source/DataDeal.c | 36 |  |  |  | UINT8 zeroStableCnt | AfeCurrent_PrepareStartupZero, DataLoad_CurrentApplyAutoZero, DataLoad_CurrentMarkZeroPending, DataLoad_CurrentSetZeroOffset |  |
| zeroReady | 103 + 309/Project/Source/DataDeal.c | 37 |  |  |  | UINT8 zeroReady | AfeCurrent_PrepareStartupZero, DataLoad_CurrentApplyAutoZero, DataLoad_CurrentMarkZeroPending, DataLoad_CurrentSetZeroOffset |  |
| zeroState | 103 + 309/Project/Source/DataDeal.c | 38 |  |  |  | UINT8 zeroState | AfeCurrent_IsStartupZeroDone, AfeCurrent_PrepareStartupZero, AfeCurrent_StartupZeroCal, DataLoad_CurrentApplyAutoZero, DataLoad_CurrentMarkZeroPending, DataLoad_CurrentSetZeroOffset |  |
| wakeCnt | 103 + 309/Project/Source/DataDeal.c | 44 |  |  |  | UINT8 wakeCnt |  | MonitorAFE |
| sleepDelay | 103 + 309/Project/Source/DataDeal.c | 50 |  |  |  | UINT16 sleepDelay[3] |  | MonitorAFE |
| mon | 103 + 309/Project/Source/DataDeal.c | 56 |  |  |  | AFE_MONITOR_RUNTIME mon |  | MonitorAFE, MonitorAFE_GetChannel |
| afeSeq | 103 + 309/Project/Source/DataDeal.c | 57 |  |  |  | UINT32 afeSeq | AfeCurrent_NextSeq | AfeCurrent_GetSeq |
| g_u16CalibCoefK | 103 + 309/Project/Source/DataDeal.c | 72 |  |  |  | UINT16 g_u16CalibCoefK[KB_NUM] | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali |
| g_i16CalibCoefB | 103 + 309/Project/Source/DataDeal.c | 74 |  |  |  | INT16 g_i16CalibCoefB[KB_NUM] | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali |
| g_u32CS_Res_AFE | 103 + 309/Project/Source/DataDeal.c | 75 |  |  |  | UINT32 g_u32CS_Res_AFE | AppInit_InitRuntimeState, DataLoad_CurrentMilliAmpToRaw, DataLoad_CurrentRawToMilliAmp, EEPROM_UpdateOtherElementRuntime, Refresh_Parameters, Sci_ApplyOtherElementSideEffects | DataDeal_DebugWatchBind, InitShortCur |
| u16Balance_OpenWindow | 103 + 309/Project/Source/DataDeal.h | 97 |  |  |  | UINT16 u16Balance_OpenWindow |  |  |
| u16Balance_CloseWindow | 103 + 309/Project/Source/DataDeal.h | 98 |  |  |  | UINT16 u16Balance_CloseWindow |  |  |
| u16Balance_Res1 | 103 + 309/Project/Source/DataDeal.h | 99 |  |  |  | UINT16 u16Balance_Res1 |  |  |
| u16Balance_Res2 | 103 + 309/Project/Source/DataDeal.h | 100 |  |  |  | UINT16 u16Balance_Res2 |  |  |
| u16Balance_Res3 | 103 + 309/Project/Source/DataDeal.h | 101 |  |  |  | UINT16 u16Balance_Res3 |  |  |
| u16Balance_Res4 | 103 + 309/Project/Source/DataDeal.h | 102 |  |  |  | UINT16 u16Balance_Res4 |  |  |
| u16Balance_Res5 | 103 + 309/Project/Source/DataDeal.h | 103 |  |  |  | UINT16 u16Balance_Res5 |  |  |
| u16CS_Cur_CHGmax | 103 + 309/Project/Source/DataDeal.h | 104 |  |  |  | UINT16 u16CS_Cur_CHGmax | InitShortCur |  |
| u16CS_Cur_DSGmax | 103 + 309/Project/Source/DataDeal.h | 106 |  |  |  | UINT16 u16CS_Cur_DSGmax | InitShortCur |  |
| u16CBC_DelayT | 103 + 309/Project/Source/DataDeal.h | 107 |  |  |  | UINT16 u16CBC_DelayT | InitShortCur, SH367309_SC_DelayT_Set | Refresh_Parameters |
| u16CBC_Cur_DSG | 103 + 309/Project/Source/DataDeal.h | 108 |  |  |  | UINT16 u16CBC_Cur_DSG | InitShortCur | Refresh_Parameters |
| u16Soc_TableSelect | 103 + 309/Project/Source/DataDeal.h | 109 |  |  |  | UINT16 u16Soc_TableSelect | EEPROM_LoadDefaultSocConfig, host_apply_default_config |  |
| u16Password_Always | 103 + 309/Project/Source/DataDeal.h | 111 |  |  |  | UINT16 u16Password_Always |  |  |
| u16CurLimit_Vdelta | 103 + 309/Project/Source/DataDeal.h | 112 |  |  |  | UINT16 u16CurLimit_Vdelta |  |  |
| u16CurLimit_Cur | 103 + 309/Project/Source/DataDeal.h | 113 |  |  |  | UINT16 u16CurLimit_Cur |  |  |
| u16Sleep_VNormal | 103 + 309/Project/Source/DataDeal.h | 114 |  |  |  | UINT16 u16Sleep_VNormal |  |  |
| u16Sleep_TimeNormal | 103 + 309/Project/Source/DataDeal.h | 116 |  |  |  | UINT16 u16Sleep_TimeNormal |  |  |
| u16Sleep_Vlow | 103 + 309/Project/Source/DataDeal.h | 117 |  |  |  | UINT16 u16Sleep_Vlow |  | RtcSleep_PortGetLowVoltageSleepMv |
| u16Sleep_TimeVlow | 103 + 309/Project/Source/DataDeal.h | 118 |  |  |  | UINT16 u16Sleep_TimeVlow |  | lp_deep |
| u16Sleep_VirCur_Chg | 103 + 309/Project/Source/DataDeal.h | 119 |  |  |  | UINT16 u16Sleep_VirCur_Chg |  |  |
| u16Sleep_VirCur_Dsg | 103 + 309/Project/Source/DataDeal.h | 120 |  |  |  | UINT16 u16Sleep_VirCur_Dsg |  |  |
| u16Sleep_RTC_WakeUpTime | 103 + 309/Project/Source/DataDeal.h | 121 |  |  |  | UINT16 u16Sleep_RTC_WakeUpTime |  |  |
| u16Sleep_TimeRTC | 103 + 309/Project/Source/DataDeal.h | 122 |  |  |  | UINT16 u16Sleep_TimeRTC |  |  |
| u16Soc_Ah | 103 + 309/Project/Source/DataDeal.h | 123 |  |  |  | UINT16 u16Soc_Ah | EEPROM_LoadDefaultSocConfig, host_apply_default_config | SOC_LoadConfigData, SOC_ResetStoredSnapshotToDefault |
| u16Soc_Cycle_times | 103 + 309/Project/Source/DataDeal.h | 125 |  |  |  | UINT16 u16Soc_Cycle_times | EEPROM_LoadDefaultSocConfig, host_apply_default_config | SOC_LoadConfigData, SOC_ResetStoredSnapshotToDefault |
| u16Soc_V_100 | 103 + 309/Project/Source/DataDeal.h | 126 |  |  |  | UINT16 u16Soc_V_100 | EEPROM_LoadDefaultSocConfig, host_apply_default_config | SOC_LoadConfigData |
| u16Soc_V_0 | 103 + 309/Project/Source/DataDeal.h | 127 |  |  |  | UINT16 u16Soc_V_0 | EEPROM_LoadDefaultSocConfig, host_apply_default_config | SOC_LoadConfigData |
| u16Sys_SeriesNum | 103 + 309/Project/Source/DataDeal.h | 128 |  |  |  | UINT16 u16Sys_SeriesNum | host_apply_default_config | EEPROM_UpdateOtherElementRuntime, Sci_ApplyOtherElementSideEffects |
| u16Sys_CS_Res | 103 + 309/Project/Source/DataDeal.h | 130 |  |  |  | UINT16 u16Sys_CS_Res |  | AppInit_InitRuntimeState, EEPROM_UpdateOtherElementRuntime, InitShortCur, Refresh_Parameters, Sci_ApplyOtherElementSideEffects |
| u16Sys_CS_Res_Num | 103 + 309/Project/Source/DataDeal.h | 131 |  |  |  | UINT16 u16Sys_CS_Res_Num |  | AppInit_InitRuntimeState, EEPROM_UpdateOtherElementRuntime, InitShortCur, Refresh_Parameters, Sci_ApplyOtherElementSideEffects |
| u16Sys_PreChg_Time | 103 + 309/Project/Source/DataDeal.h | 132 |  |  |  | UINT16 u16Sys_PreChg_Time |  |  |
| g_i16CalibCoefB | 103 + 309/Project/Source/DataDeal.h | 199 |  | extern |  | extern INT16 g_i16CalibCoefB[KB_NUM] | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali |
| OtherElement | 103 + 309/Project/Source/DataDeal.h | 200 |  | extern |  | extern struct OTHER_ELEMENT OtherElement |  |  |
| g_u32CS_Res_AFE | 103 + 309/Project/Source/DataDeal.h | 201 |  | extern |  | extern UINT32 g_u32CS_Res_AFE | AppInit_InitRuntimeState, DataLoad_CurrentMilliAmpToRaw, DataLoad_CurrentRawToMilliAmp, EEPROM_UpdateOtherElementRuntime, Refresh_Parameters, Sci_ApplyOtherElementSideEffects | DataDeal_DebugWatchBind, InitShortCur |
| fault | 103 + 309/Project/Source/DebugHooks.c | 16 |  |  |  | uint16_t fault | DebugHooks_RuntimeRecordEvents, Fault_DebugWatchBind, SystemDebug_Event, SystemDebug_Snapshot | SH367309_RecordFaultOnActive, SystemDebug_RefreshModuleStates |
| lp | 103 + 309/Project/Source/DebugHooks.c | 17 |  |  |  | uint8_t lp | DebugHooks_RuntimeRecordEvents, SystemDebug_Snapshot | DbgPrint_LP, DbgPrint_Summary, DbgPrint_Wakeup, SystemDebug_RefreshModuleStates |
| reserved | 103 + 309/Project/Source/DebugHooks.c | 18 |  |  |  | uint8_t reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| s_rt | 103 + 309/Project/Source/DebugHooks.c | 20 | static |  |  | static APP_RUNTIME s_rt = { 0U, 0U, 3U, 0U } | DebugHooks_RuntimeDebugPrint, DebugHooks_RuntimeRecordEvents | Runtime_DebugWatchBind |
| soc_rtc_rest_applied_seconds | 103 + 309/Project/Source/DebugWatch.h | 74 |  |  |  | uint32_t *soc_rtc_rest_applied_seconds | SocEnhance_DebugWatchBind |  |
| soc_rest_voltage_stable | 103 + 309/Project/Source/DebugWatch.h | 75 |  |  |  | uint8_t *soc_rest_voltage_stable | SocEnhance_DebugWatchBind | soc_apply_rtc_rest_ocv, soc_update_rest_timer |
| sci_tx_buffer | 103 + 309/Project/Source/DebugWatch.h | 87 |  |  |  | uint8_t *sci_tx_buffer | Sci_DebugWatchBind |  |
| sci_err1 | 103 + 309/Project/Source/DebugWatch.h | 88 |  |  | volatile | volatile uint16_t *sci_err1 | Sci_DebugWatchBind |  |
| sci_err2 | 103 + 309/Project/Source/DebugWatch.h | 89 |  |  | volatile | volatile uint16_t *sci_err2 | Sci_DebugWatchBind |  |
| sci_err3 | 103 + 309/Project/Source/DebugWatch.h | 90 |  |  | volatile | volatile uint16_t *sci_err3 | Sci_DebugWatchBind |  |
| sci_tx_enable1 | 103 + 309/Project/Source/DebugWatch.h | 91 |  |  | volatile | volatile uint8_t *sci_tx_enable1 | Sci_DebugWatchBind |  |
| sci_tx_enable2 | 103 + 309/Project/Source/DebugWatch.h | 92 |  |  | volatile | volatile uint8_t *sci_tx_enable2 | Sci_DebugWatchBind |  |
| sci_tx_enable3 | 103 + 309/Project/Source/DebugWatch.h | 93 |  |  | volatile | volatile uint8_t *sci_tx_enable3 | Sci_DebugWatchBind |  |
| sci_tx_finish1 | 103 + 309/Project/Source/DebugWatch.h | 94 |  |  | volatile | volatile uint8_t *sci_tx_finish1 | Sci_DebugWatchBind |  |
| sci_tx_finish2 | 103 + 309/Project/Source/DebugWatch.h | 95 |  |  | volatile | volatile uint8_t *sci_tx_finish2 | Sci_DebugWatchBind |  |
| sci_tx_finish3 | 103 + 309/Project/Source/DebugWatch.h | 96 |  |  | volatile | volatile uint8_t *sci_tx_finish3 | Sci_DebugWatchBind |  |
| flash_update_flag | 103 + 309/Project/Source/DebugWatch.h | 97 |  |  |  | uint8_t *flash_update_flag | Sci_DebugWatchBind |  |
| flash_update_e2prom | 103 + 309/Project/Source/DebugWatch.h | 98 |  |  |  | uint8_t *flash_update_e2prom | Sci_DebugWatchBind |  |
| irq | 103 + 309/Project/Source/DebugWatch.h | 104 |  |  | volatile | volatile struct IRQ_DEBUG_STATE *irq | DebugWatch_BindAll, SystemDebug_SnapshotMcuResources |  |
| time_latched | 103 + 309/Project/Source/DebugWatch.h | 105 |  |  | volatile | volatile union SYS_TIME *time_latched | DebugWatch_BindAll |  |
| time_pending | 103 + 309/Project/Source/DebugWatch.h | 106 |  |  | volatile | volatile union SYS_TIME *time_pending | DebugWatch_BindAll |  |
| tick_10ms | 103 + 309/Project/Source/DebugWatch.h | 107 |  |  | volatile | volatile uint32_t *tick_10ms | DebugWatch_BindAll, IrqDebug_RecordEvent, SystemDebug_Snapshot | DbgPrint_Summary |
| cnt50ms | 103 + 309/Project/Source/DebugWatch.h | 108 |  |  |  | uint8_t *cnt50ms | DebugWatch_BindAll |  |
| cnt100ms | 103 + 309/Project/Source/DebugWatch.h | 109 |  |  |  | uint8_t *cnt100ms | DebugWatch_BindAll |  |
| cnt200ms | 103 + 309/Project/Source/DebugWatch.h | 110 |  |  |  | uint8_t *cnt200ms | DebugWatch_BindAll |  |
| cnt1000ms | 103 + 309/Project/Source/DebugWatch.h | 111 |  |  |  | uint8_t *cnt1000ms | DebugWatch_BindAll |  |
| pending_200ms | 103 + 309/Project/Source/DebugWatch.h | 112 |  |  | volatile | volatile uint8_t *pending_200ms | DebugWatch_BindAll |  |
| overflow_200ms | 103 + 309/Project/Source/DebugWatch.h | 113 |  |  | volatile | volatile uint16_t *overflow_200ms | DebugWatch_BindAll |  |
| feature | 103 + 309/Project/Source/DebugWatch.h | 114 |  |  | volatile | volatile union System_OnOFF_Function *feature | DebugWatch_BindAll, SystemDebug_Snapshot |  |
| status | 103 + 309/Project/Source/DebugWatch.h | 115 |  |  | volatile | volatile union System_Status *status | CAN_OperatingModeRequest, CtApp_Poll, CtProtocol_Feed, DebugWatch_BindAll, FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseAllBank1Pages, FLASH_EraseAllBank2Pages, FLASH_EraseAllPages, FLASH_EraseOptionBytes | CtProtocol_Encode, can_build_ack, can_send_ack, feidao_can_app_send_ack, respond |
| error | 103 + 309/Project/Source/DebugWatch.h | 116 |  |  | volatile | volatile struct SYSTEM_ERROR *error | DebugWatch_BindAll | InitShortCur, Sci_ApplyOtherElementSideEffects, Sci_SetWrError, feidao_can_app_status_from_host_error, serial_send_error |
| low_power | 103 + 309/Project/Source/DebugWatch.h | 117 |  |  | volatile | volatile struct LOW_POWER_RTC_STATUS *low_power | DebugWatch_BindAll, SystemDebug_SnapshotMcuResources | SystemDebug_ModuleItem |
| delay_fac_us | 103 + 309/Project/Source/DebugWatch.h | 119 |  |  |  | uint8_t *delay_fac_us | SystemInit_DebugWatchBind |  |
| delay_fac_ms | 103 + 309/Project/Source/DebugWatch.h | 120 |  |  |  | uint16_t *delay_fac_ms | SystemInit_DebugWatchBind |  |
| reg_store | 103 + 309/Project/Source/DebugWatch.h | 127 |  |  |  | SH367309_REG_STORE *reg_store | SH367309Func_DebugWatchBind |  |
| rom_params | 103 + 309/Project/Source/DebugWatch.h | 128 |  |  |  | AFE_ROM_PARAMETERS_TypeDef *rom_params | SH367309Data_DebugWatchBind |  |
| rs485_params | 103 + 309/Project/Source/DebugWatch.h | 129 |  |  |  | AFE_Parameters_RS485_Typedef *rs485_params | SH367309Data_DebugWatchBind |  |
| param_write_flag | 103 + 309/Project/Source/DebugWatch.h | 130 |  |  |  | int *param_write_flag | SH367309Data_DebugWatchBind |  |
| mtp_buffer | 103 + 309/Project/Source/DebugWatch.h | 131 |  |  |  | uint8_t *mtp_buffer | SH367309Func_DebugWatchBind |  |
| record_third | 103 + 309/Project/Source/DebugWatch.h | 140 |  |  |  | uint16_t *record_third | Fault_DebugWatchBind |  |
| record_third2 | 103 + 309/Project/Source/DebugWatch.h | 141 |  |  |  | uint16_t *record_third2 | Fault_DebugWatchBind |  |
| point_third | 103 + 309/Project/Source/DebugWatch.h | 142 |  |  |  | uint8_t *point_third | Fault_DebugWatchBind |  |
| point_third2 | 103 + 309/Project/Source/DebugWatch.h | 143 |  |  |  | uint8_t *point_third2 | Fault_DebugWatchBind |  |
| production | 103 + 309/Project/Source/DebugWatch.h | 151 |  |  |  | PRODUCTION_ID_INFO *production | ProductionID_DebugWatchBind |  |
| log_interval_s_tcnt | 103 + 309/Project/Source/DebugWatch.h | 159 |  |  |  | uint32_t *log_interval_s_tcnt | LogRecord_DebugWatchBind |  |
| coef_b | 103 + 309/Project/Source/DebugWatch.h | 165 |  |  |  | int16_t *coef_b | DataDeal_DebugWatchBind |  |
| cs_res_afe | 103 + 309/Project/Source/DebugWatch.h | 166 |  |  |  | uint32_t *cs_res_afe | DataDeal_DebugWatchBind |  |
| afe_ntc_10k | 103 + 309/Project/Source/DebugWatch.h | 172 |  |  |  | const uint16_t *afe_ntc_10k | I2C_AFE1_DebugWatchBind |  |
| afe_crc8 | 103 + 309/Project/Source/DebugWatch.h | 173 |  |  |  | const uint8_t *afe_crc8 | I2C_AFE1_DebugWatchBind |  |
| sh_ntc_10k | 103 + 309/Project/Source/DebugWatch.h | 174 |  |  |  | const uint16_t *sh_ntc_10k | SH367309Func_DebugWatchBind |  |
| sh_afe_scv | 103 + 309/Project/Source/DebugWatch.h | 175 |  |  |  | const uint16_t *sh_afe_scv | SH367309Data_DebugWatchBind |  |
| sh_afe_sct | 103 + 309/Project/Source/DebugWatch.h | 176 |  |  |  | const uint16_t *sh_afe_sct | SH367309Data_DebugWatchBind |  |
| sh_afe_ocd1v_occv | 103 + 309/Project/Source/DebugWatch.h | 177 |  |  |  | const uint16_t *sh_afe_ocd1v_occv | SH367309Data_DebugWatchBind |  |
| sh_afe_ocd2v | 103 + 309/Project/Source/DebugWatch.h | 178 |  |  |  | const uint16_t *sh_afe_ocd2v | SH367309Data_DebugWatchBind |  |
| sh_afe_ovt_uvt | 103 + 309/Project/Source/DebugWatch.h | 179 |  |  |  | const uint16_t *sh_afe_ovt_uvt | SH367309Data_DebugWatchBind |  |
| sh_afe_ocd1t | 103 + 309/Project/Source/DebugWatch.h | 180 |  |  |  | const uint16_t *sh_afe_ocd1t | SH367309Data_DebugWatchBind |  |
| sh_afe_occt_ocd2t | 103 + 309/Project/Source/DebugWatch.h | 181 |  |  |  | const uint16_t *sh_afe_occt_ocd2t | SH367309Data_DebugWatchBind |  |
| soc_lifepo | 103 + 309/Project/Source/DebugWatch.h | 182 |  |  |  | const uint16_t *soc_lifepo | SocEnhance_DebugWatchBindTables |  |
| soc_ternary | 103 + 309/Project/Source/DebugWatch.h | 183 |  |  |  | const uint16_t *soc_ternary | SocEnhance_DebugWatchBindTables |  |
| soc_empty_tail | 103 + 309/Project/Source/DebugWatch.h | 184 |  |  |  | const struct SOC_EMPTY_TAIL_RULE_TAG *soc_empty_tail | SocEnhance_DebugWatchBindTables |  |
| soc_empty_tail_count | 103 + 309/Project/Source/DebugWatch.h | 185 |  |  |  | uint16_t soc_empty_tail_count | SocEnhance_DebugWatchBindTables |  |
| ledbar_digit_map | 103 + 309/Project/Source/DebugWatch.h | 186 |  |  |  | const uint8_t *ledbar_digit_map | LedBar_DebugWatchBind |  |
| ledbar_digit_map_count | 103 + 309/Project/Source/DebugWatch.h | 187 |  |  |  | uint16_t ledbar_digit_map_count | LedBar_DebugWatchBind |  |
| ledbar_routes | 103 + 309/Project/Source/DebugWatch.h | 188 |  |  |  | const void *ledbar_routes | LedBar_DebugWatchBind |  |
| ledbar_routes_count | 103 + 309/Project/Source/DebugWatch.h | 189 |  |  |  | uint16_t ledbar_routes_count | LedBar_DebugWatchBind |  |
| ledbar_pins | 103 + 309/Project/Source/DebugWatch.h | 190 |  |  |  | const void *ledbar_pins | LedBar_DebugWatchBind |  |
| ledbar_pins_count | 103 + 309/Project/Source/DebugWatch.h | 191 |  |  |  | uint16_t ledbar_pins_count | LedBar_DebugWatchBind |  |
| rtc_month_days | 103 + 309/Project/Source/DebugWatch.h | 192 |  |  |  | const uint8_t *rtc_month_days | RTC_DebugWatchBind |  |
| rtc_month_days_count | 103 + 309/Project/Source/DebugWatch.h | 193 |  |  |  | uint16_t rtc_month_days_count | RTC_DebugWatchBind |  |
| system_error_field_offset | 103 + 309/Project/Source/DebugWatch.h | 194 |  |  |  | const uint8_t *system_error_field_offset | SystemMonitor_DebugWatchBind |  |
| system_error_field_offset_count | 103 + 309/Project/Source/DebugWatch.h | 195 |  |  |  | uint16_t system_error_field_offset_count | SystemMonitor_DebugWatchBind |  |
| bq_afe_scv | 103 + 309/Project/Source/DebugWatch.h | 196 |  |  |  | const uint16_t *bq_afe_scv |  |  |
| bq_afe_sct | 103 + 309/Project/Source/DebugWatch.h | 197 |  |  |  | const uint16_t *bq_afe_sct |  |  |
| can_feidao_dispatch | 103 + 309/Project/Source/DebugWatch.h | 198 |  |  |  | const struct CAN_FEIDAO_FRAME_DISPATCH_TAG *can_feidao_dispatch | CanFeidaoFrames_DebugWatchBind |  |
| can_feidao_dispatch_count | 103 + 309/Project/Source/DebugWatch.h | 199 |  |  |  | uint16_t can_feidao_dispatch_count | CanFeidaoFrames_DebugWatchBind |  |
| comm | 103 + 309/Project/Source/DebugWatch.h | 205 |  |  |  | DEBUG_WATCH_COMM_DIR comm | LP_GetBlockReason, Sci_DebugWatchBind |  |
| system | 103 + 309/Project/Source/DebugWatch.h | 206 |  |  |  | DEBUG_WATCH_SYSTEM_DIR system | DebugWatch_BindAll, SystemDebug_DebugWatchBind, SystemInit_DebugWatchBind |  |
| afe | 103 + 309/Project/Source/DebugWatch.h | 207 |  |  |  | DEBUG_WATCH_AFE_DIR afe | I2C_AFE1_DebugWatchBind, SH367309Data_DebugWatchBind, SH367309Func_DebugWatchBind, SystemDebug_Snapshot | SystemDebug_ModuleItem |
| fault | 103 + 309/Project/Source/DebugWatch.h | 208 |  |  |  | DEBUG_WATCH_FAULT_DIR fault | DebugHooks_RuntimeRecordEvents, Fault_DebugWatchBind, SystemDebug_Event, SystemDebug_Snapshot | SH367309_RecordFaultOnActive, SystemDebug_RefreshModuleStates |
| public_data | 103 + 309/Project/Source/DebugWatch.h | 209 |  |  |  | DEBUG_WATCH_PUBLIC_DIR public_data | DebugWatch_BindAll, ProductionID_DebugWatchBind, RTC_DebugWatchBind, Sci_DebugWatchBind |  |
| app | 103 + 309/Project/Source/DebugWatch.h | 210 |  |  |  | DEBUG_WATCH_APP_DIR app | DebugWatch_BindAll, LogRecord_DebugWatchBind, Runtime_DebugWatchBind |  |
| calib | 103 + 309/Project/Source/DebugWatch.h | 211 |  |  |  | DEBUG_WATCH_CALIB_DIR calib | DataDeal_DebugWatchBind |  |
| tables | 103 + 309/Project/Source/DebugWatch.h | 212 |  |  |  | DEBUG_WATCH_TABLE_DIR tables | ADC_DebugWatchBind, CanFeidaoFrames_DebugWatchBind, I2C_AFE1_DebugWatchBind, LedBar_DebugWatchBind, RTC_DebugWatchBind, SH367309Data_DebugWatchBind, SH367309Func_DebugWatchBind, SocEnhance_DebugWatchBindTables, SystemMonitor_DebugWatchBind |  |
| sys_time_latched | 103 + 309/Project/Source/DebugWatch.h | 225 |  |  | volatile | volatile union SYS_TIME *sys_time_latched | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| sys_time_pending | 103 + 309/Project/Source/DebugWatch.h | 227 |  |  | volatile | volatile union SYS_TIME *sys_time_pending | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| sys_10ms_tick_count | 103 + 309/Project/Source/DebugWatch.h | 228 |  |  | volatile | volatile uint32_t *sys_10ms_tick_count | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| sys_cnt50ms | 103 + 309/Project/Source/DebugWatch.h | 229 |  |  |  | uint8_t *sys_cnt50ms | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| sys_cnt100ms | 103 + 309/Project/Source/DebugWatch.h | 230 |  |  |  | uint8_t *sys_cnt100ms | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| sys_cnt200ms | 103 + 309/Project/Source/DebugWatch.h | 231 |  |  |  | uint8_t *sys_cnt200ms | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| sys_cnt1000ms | 103 + 309/Project/Source/DebugWatch.h | 232 |  |  |  | uint8_t *sys_cnt1000ms | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| sys_200ms_pending_periods | 103 + 309/Project/Source/DebugWatch.h | 233 |  |  | volatile | volatile uint8_t *sys_200ms_pending_periods | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| sys_200ms_overflow_count | 103 + 309/Project/Source/DebugWatch.h | 234 |  |  | volatile | volatile uint16_t *sys_200ms_overflow_count | SystemInit_DebugWatchBind | DebugWatch_BindAll |
| system_feature | 103 + 309/Project/Source/DebugWatch.h | 235 |  |  | volatile | volatile union System_OnOFF_Function *system_feature | SystemMonitor_DebugWatchBind | DebugWatch_BindAll |
| system_status | 103 + 309/Project/Source/DebugWatch.h | 237 |  |  | volatile | volatile union System_Status *system_status | SystemMonitor_DebugWatchBind | DebugWatch_BindAll |
| system_error | 103 + 309/Project/Source/DebugWatch.h | 238 |  |  | volatile | volatile struct SYSTEM_ERROR *system_error | DebugWatch_BindAll |  |
| low_power | 103 + 309/Project/Source/DebugWatch.h | 239 |  |  | volatile | volatile struct LOW_POWER_RTC_STATUS *low_power | DebugWatch_BindAll, SystemDebug_SnapshotMcuResources | SystemDebug_ModuleItem |
| g_dbg_watch | 103 + 309/Project/Source/DebugWatch.h | 246 |  | extern |  | extern DEBUG_WATCH_ROOT g_dbg_watch |  |  |
| elapsed10ms | 103 + 309/Project/Source/FactoryAging.c | 31 |  |  |  | UINT32 elapsed10ms | FactoryAging_AddRunningTicks, FactoryAging_GetRemainingSeconds, FactoryAging_LoadBkp, FactoryAging_LoadRuntimeStateForHost, FactoryAging_LoadStoredProgress, FactoryAging_MarkDone, FactoryAging_ResetTimeByHost, FactoryAging_SaveBkp, FactoryAging_SetDurationHoursByHost, FactoryAging_Start | FactoryAging_BkpCrc, FactoryAging_ClampElapsed, FactoryAging_SaveProgressBeforeSleep, FactoryAging_SaveStoredProgress, FactoryAging_Task |
| lastTick | 103 + 309/Project/Source/FactoryAging.c | 32 |  |  |  | UINT32 lastTick | FactoryAging_AddRunningTicks, FactoryAging_EnterRunningFromHost, FactoryAging_LoadRuntimeStateForHost, FactoryAging_Start |  |
| lastBkpSave10ms | 103 + 309/Project/Source/FactoryAging.c | 33 |  |  |  | UINT32 lastBkpSave10ms | FactoryAging_SaveBkp | FactoryAging_SaveStoredProgress |
| lastFlashSave10ms | 103 + 309/Project/Source/FactoryAging.c | 34 |  |  |  | UINT32 lastFlashSave10ms | FactoryAging_LoadStoredProgress, FactoryAging_SaveStoredProgress |  |
| nextFinishRetry10ms | 103 + 309/Project/Source/FactoryAging.c | 35 |  |  |  | UINT32 nextFinishRetry10ms | FactoryAging_EnterRunningFromHost, FactoryAging_Finish, FactoryAging_LoadRuntimeStateForHost, FactoryAging_Start, FactoryAging_Task |  |
| durationHours | 103 + 309/Project/Source/FactoryAging.c | 36 |  |  |  | UINT16 durationHours | FactoryAging_LoadDurationFromData, FactoryAging_SetDurationHoursByHost | FactoryAging_GetDuration10ms, FactoryAging_SaveStoredProgress |
| bkpSaveValid | 103 + 309/Project/Source/FactoryAging.c | 37 |  |  |  | UINT8 bkpSaveValid | FactoryAging_SaveBkp, FactoryAging_SaveStoredProgress |  |
| flashSaveValid | 103 + 309/Project/Source/FactoryAging.c | 38 |  |  |  | UINT8 flashSaveValid | FactoryAging_LoadStoredProgress, FactoryAging_SaveStoredProgress |  |
| mosMode | 103 + 309/Project/Source/FactoryAging.c | 39 |  |  |  | UINT8 mosMode | FactoryAging_ApplyRunningMos, FactoryAging_ResetMosCache |  |
| s_factory_aging | 103 + 309/Project/Source/FactoryAging.c | 41 | static |  |  | static FactoryAgingRuntime s_factory_aging = { FACTORY_AGING_STATE_UNINIT, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, FACTORY_AGING_MOS_MODE_UNKNOWN } | FactoryAging_AddRunningTicks, FactoryAging_ApplyRunningMos, FactoryAging_EnterRunningFromHost, FactoryAging_Finish, FactoryAging_GetRemainingSeconds, FactoryAging_IsActive, FactoryAging_LoadDurationFromData, FactoryAging_LoadRuntimeStateForHost, FactoryAging_LoadStoredProgress, FactoryAging_MarkDone | FactoryAging_DebugWatchBind, FactoryAging_GetDuration10ms, FactoryAging_GetState, FactoryAging_SaveProgressBeforeSleep |
| Fault_record_Third | 103 + 309/Project/Source/Fault.c | 8 |  |  |  | UINT16 Fault_record_Third[Record_len] |  | Fault_DebugWatchBind, Sci_ACK_0x03_ReadRegs_LCD |
| Fault_record_Third2 | 103 + 309/Project/Source/Fault.c | 10 |  |  |  | UINT16 Fault_record_Third2[Record_len] | FaultWarnRecord2, Sci_WrReg_0x06_Reset_ProtectRecord | Fault_DebugWatchBind, Sci_ACK_0x03_ReadRegs_Data |
| FaultPoint_Third | 103 + 309/Project/Source/Fault.c | 12 |  |  |  | UINT8 FaultPoint_Third |  | Fault_DebugWatchBind, Sci_ACK_0x03_ReadRegs_LCD |
| FaultPoint_Third2 | 103 + 309/Project/Source/Fault.c | 14 |  |  |  | UINT8 FaultPoint_Third2 | FaultWarnRecord2, Sci_WrReg_0x06_Reset_ProtectRecord | Fault_DebugWatchBind, Sci_ACK_0x03_ReadRegs_Data |
| CellUvp_First | 103 + 309/Project/Source/Fault.h | 52 |  |  |  | UINT8 CellUvp_First :1 |  |  |
| BatOvp_First | 103 + 309/Project/Source/Fault.h | 53 |  |  |  | UINT8 BatOvp_First :1 |  |  |
| BatUvp_First | 103 + 309/Project/Source/Fault.h | 54 |  |  |  | UINT8 BatUvp_First :1 |  |  |
| IchgOcp_First | 103 + 309/Project/Source/Fault.h | 55 |  |  |  | UINT8 IchgOcp_First :1 |  |  |
| IdischgOcp_First | 103 + 309/Project/Source/Fault.h | 57 |  |  |  | UINT8 IdischgOcp_First :1 |  |  |
| CellChgOTp_First | 103 + 309/Project/Source/Fault.h | 58 |  |  |  | UINT8 CellChgOTp_First :1 |  |  |
| CellChgUTp_First | 103 + 309/Project/Source/Fault.h | 59 |  |  |  | UINT8 CellChgUTp_First :1 |  |  |
| CellDsgOTp_First | 103 + 309/Project/Source/Fault.h | 60 |  |  |  | UINT8 CellDsgOTp_First :1 |  |  |
| CellDsgUTp_First | 103 + 309/Project/Source/Fault.h | 62 |  |  |  | UINT8 CellDsgUTp_First :1 |  |  |
| MosOTp_First | 103 + 309/Project/Source/Fault.h | 63 |  |  |  | UINT8 MosOTp_First :1 |  |  |
| VdeltaOvp_First | 103 + 309/Project/Source/Fault.h | 64 |  |  |  | UINT8 VdeltaOvp_First :1 |  |  |
| CellSocUp_First | 103 + 309/Project/Source/Fault.h | 65 |  |  |  | UINT8 CellSocUp_First :1 |  |  |
| Rcv | 103 + 309/Project/Source/Fault.h | 67 |  |  |  | UINT8 Rcv :3 |  |  |
| CellUvp_Second | 103 + 309/Project/Source/Fault.h | 75 |  |  |  | UINT8 CellUvp_Second :1 |  |  |
| BatOvp_Second | 103 + 309/Project/Source/Fault.h | 76 |  |  |  | UINT8 BatOvp_Second :1 |  |  |
| BatUvp_Second | 103 + 309/Project/Source/Fault.h | 77 |  |  |  | UINT8 BatUvp_Second :1 |  |  |
| IchgOcp_Second | 103 + 309/Project/Source/Fault.h | 78 |  |  |  | UINT8 IchgOcp_Second :1 |  |  |
| IdischgOcp_Second | 103 + 309/Project/Source/Fault.h | 80 |  |  |  | UINT8 IdischgOcp_Second :1 |  |  |
| CellChgOTp_Second | 103 + 309/Project/Source/Fault.h | 81 |  |  |  | UINT8 CellChgOTp_Second :1 |  |  |
| CellDsgOTp_Second | 103 + 309/Project/Source/Fault.h | 82 |  |  |  | UINT8 CellDsgOTp_Second :1 |  |  |
| CellChgUTp_Second | 103 + 309/Project/Source/Fault.h | 83 |  |  |  | UINT8 CellChgUTp_Second :1 |  |  |
| CellDsgUTp_Second | 103 + 309/Project/Source/Fault.h | 85 |  |  |  | UINT8 CellDsgUTp_Second :1 |  |  |
| MosOTp_Second | 103 + 309/Project/Source/Fault.h | 86 |  |  |  | UINT8 MosOTp_Second :1 |  |  |
| VdeltaOvp_Second | 103 + 309/Project/Source/Fault.h | 87 |  |  |  | UINT8 VdeltaOvp_Second :1 |  |  |
| CellSocUp_Second | 103 + 309/Project/Source/Fault.h | 88 |  |  |  | UINT8 CellSocUp_Second :1 |  |  |
| Rcv | 103 + 309/Project/Source/Fault.h | 90 |  |  |  | UINT8 Rcv :3 |  |  |
| CellUvp_Third | 103 + 309/Project/Source/Fault.h | 98 |  |  |  | UINT8 CellUvp_Third :1 |  | Fault_ChangeToMCU |
| BatOvp_Third | 103 + 309/Project/Source/Fault.h | 99 |  |  |  | UINT8 BatOvp_Third :1 |  | new_todo_logi |
| BatUvp_Third | 103 + 309/Project/Source/Fault.h | 100 |  |  |  | UINT8 BatUvp_Third :1 |  |  |
| IchgOcp_Third | 103 + 309/Project/Source/Fault.h | 101 |  |  |  | UINT8 IchgOcp_Third :1 |  | Fault_ChangeToMCU |
| IdischgOcp_Third | 103 + 309/Project/Source/Fault.h | 103 |  |  |  | UINT8 IdischgOcp_Third :1 |  | Fault_ChangeToMCU |
| CellChgOTp_Third | 103 + 309/Project/Source/Fault.h | 104 |  |  |  | UINT8 CellChgOTp_Third :1 |  | Fault_ChangeToMCU, new_todo_logi |
| CellChgUTp_Third | 103 + 309/Project/Source/Fault.h | 105 |  |  |  | UINT8 CellChgUTp_Third :1 |  | Fault_ChangeToMCU |
| CellDsgOTp_Third | 103 + 309/Project/Source/Fault.h | 106 |  |  |  | UINT8 CellDsgOTp_Third :1 |  | Fault_ChangeToMCU, new_todo_logi |
| CellDsgUTp_Third | 103 + 309/Project/Source/Fault.h | 108 |  |  |  | UINT8 CellDsgUTp_Third :1 |  | Fault_ChangeToMCU |
| MosOTp_Third | 103 + 309/Project/Source/Fault.h | 109 |  |  |  | UINT8 MosOTp_Third :1 |  | new_todo_logi |
| VdeltaOvp_Third | 103 + 309/Project/Source/Fault.h | 110 |  |  |  | UINT8 VdeltaOvp_Third :1 |  |  |
| CellSocUp_Third | 103 + 309/Project/Source/Fault.h | 111 |  |  |  | UINT8 CellSocUp_Third :1 |  |  |
| Rcv | 103 + 309/Project/Source/Fault.h | 113 |  |  |  | UINT8 Rcv :3 |  |  |
| u16VcellOvp_Second | 103 + 309/Project/Source/Fault.h | 121 |  |  |  | UINT16 u16VcellOvp_Second |  |  |
| u16VcellOvp_Third | 103 + 309/Project/Source/Fault.h | 122 |  |  |  | UINT16 u16VcellOvp_Third |  |  |
| u16VcellOvp_Rcv | 103 + 309/Project/Source/Fault.h | 123 |  |  |  | UINT16 u16VcellOvp_Rcv |  | Refresh_Parameters |
| u16VcellOvp_Filter | 103 + 309/Project/Source/Fault.h | 124 |  |  |  | UINT16 u16VcellOvp_Filter |  | Refresh_Parameters |
| u16VcellUvp_First | 103 + 309/Project/Source/Fault.h | 125 |  |  |  | UINT16 u16VcellUvp_First |  |  |
| u16VcellUvp_Second | 103 + 309/Project/Source/Fault.h | 127 |  |  |  | UINT16 u16VcellUvp_Second |  |  |
| u16VcellUvp_Third | 103 + 309/Project/Source/Fault.h | 128 |  |  |  | UINT16 u16VcellUvp_Third |  |  |
| u16VcellUvp_Rcv | 103 + 309/Project/Source/Fault.h | 129 |  |  |  | UINT16 u16VcellUvp_Rcv |  | Refresh_Parameters |
| u16VcellUvp_Filter | 103 + 309/Project/Source/Fault.h | 130 |  |  |  | UINT16 u16VcellUvp_Filter |  | Refresh_Parameters |
| u16VbusOvp_First | 103 + 309/Project/Source/Fault.h | 131 |  |  |  | UINT16 u16VbusOvp_First |  |  |
| u16VbusOvp_Second | 103 + 309/Project/Source/Fault.h | 133 |  |  |  | UINT16 u16VbusOvp_Second |  |  |
| u16VbusOvp_Third | 103 + 309/Project/Source/Fault.h | 134 |  |  |  | UINT16 u16VbusOvp_Third |  |  |
| u16VbusOvp_Rcv | 103 + 309/Project/Source/Fault.h | 135 |  |  |  | UINT16 u16VbusOvp_Rcv |  |  |
| u16VbusOvp_Filter | 103 + 309/Project/Source/Fault.h | 136 |  |  |  | UINT16 u16VbusOvp_Filter |  |  |
| u16VbusUvp_First | 103 + 309/Project/Source/Fault.h | 137 |  |  |  | UINT16 u16VbusUvp_First |  |  |
| u16VbusUvp_Second | 103 + 309/Project/Source/Fault.h | 139 |  |  |  | UINT16 u16VbusUvp_Second |  |  |
| u16VbusUvp_Third | 103 + 309/Project/Source/Fault.h | 140 |  |  |  | UINT16 u16VbusUvp_Third |  |  |
| u16VbusUvp_Rcv | 103 + 309/Project/Source/Fault.h | 141 |  |  |  | UINT16 u16VbusUvp_Rcv |  |  |
| u16VbusUvp_Filter | 103 + 309/Project/Source/Fault.h | 142 |  |  |  | UINT16 u16VbusUvp_Filter |  |  |
| u16IchgOcp_First | 103 + 309/Project/Source/Fault.h | 143 |  |  |  | UINT16 u16IchgOcp_First |  |  |
| u16IchgOcp_Second | 103 + 309/Project/Source/Fault.h | 145 |  |  |  | UINT16 u16IchgOcp_Second |  | Refresh_Parameters |
| u16IchgOcp_Third | 103 + 309/Project/Source/Fault.h | 146 |  |  |  | UINT16 u16IchgOcp_Third |  |  |
| u16IchgOcp_Rcv | 103 + 309/Project/Source/Fault.h | 147 |  |  |  | UINT16 u16IchgOcp_Rcv |  |  |
| u16IchgOcp_Filter | 103 + 309/Project/Source/Fault.h | 148 |  |  |  | UINT16 u16IchgOcp_Filter |  |  |
| u16IdsgOcp_First | 103 + 309/Project/Source/Fault.h | 149 |  |  |  | UINT16 u16IdsgOcp_First |  | Refresh_Parameters |
| u16IdsgOcp_Second | 103 + 309/Project/Source/Fault.h | 151 |  |  |  | UINT16 u16IdsgOcp_Second |  | Refresh_Parameters |
| u16IdsgOcp_Third | 103 + 309/Project/Source/Fault.h | 152 |  |  |  | UINT16 u16IdsgOcp_Third |  |  |
| u16IdsgOcp_Rcv | 103 + 309/Project/Source/Fault.h | 153 |  |  |  | UINT16 u16IdsgOcp_Rcv |  |  |
| u16IdsgOcp_Filter | 103 + 309/Project/Source/Fault.h | 154 |  |  |  | UINT16 u16IdsgOcp_Filter |  |  |
| u16TChgOTp_First | 103 + 309/Project/Source/Fault.h | 155 |  |  |  | UINT16 u16TChgOTp_First |  |  |
| u16TChgOTp_Second | 103 + 309/Project/Source/Fault.h | 157 |  |  |  | UINT16 u16TChgOTp_Second |  |  |
| u16TChgOTp_Third | 103 + 309/Project/Source/Fault.h | 158 |  |  |  | UINT16 u16TChgOTp_Third |  |  |
| u16TChgOTp_Rcv | 103 + 309/Project/Source/Fault.h | 159 |  |  |  | UINT16 u16TChgOTp_Rcv |  | Refresh_Parameters |
| u16TChgOTp_Filter | 103 + 309/Project/Source/Fault.h | 160 |  |  |  | UINT16 u16TChgOTp_Filter |  |  |
| u16TchgUTp_First | 103 + 309/Project/Source/Fault.h | 161 |  |  |  | UINT16 u16TchgUTp_First |  |  |
| u16TchgUTp_Second | 103 + 309/Project/Source/Fault.h | 163 |  |  |  | UINT16 u16TchgUTp_Second |  |  |
| u16TchgUTp_Third | 103 + 309/Project/Source/Fault.h | 164 |  |  |  | UINT16 u16TchgUTp_Third |  |  |
| u16TchgUTp_Rcv | 103 + 309/Project/Source/Fault.h | 165 |  |  |  | UINT16 u16TchgUTp_Rcv |  | Refresh_Parameters |
| u16TchgUTp_Filter | 103 + 309/Project/Source/Fault.h | 166 |  |  |  | UINT16 u16TchgUTp_Filter |  |  |
| u16TdischgOTp_First | 103 + 309/Project/Source/Fault.h | 167 |  |  |  | UINT16 u16TdischgOTp_First |  |  |
| u16TdischgOTp_Second | 103 + 309/Project/Source/Fault.h | 169 |  |  |  | UINT16 u16TdischgOTp_Second |  |  |
| u16TdischgOTp_Third | 103 + 309/Project/Source/Fault.h | 170 |  |  |  | UINT16 u16TdischgOTp_Third |  |  |
| u16TdischgOTp_Rcv | 103 + 309/Project/Source/Fault.h | 171 |  |  |  | UINT16 u16TdischgOTp_Rcv |  | Refresh_Parameters |
| u16TdischgOTp_Filter | 103 + 309/Project/Source/Fault.h | 172 |  |  |  | UINT16 u16TdischgOTp_Filter |  |  |
| u16TdischgUTp_First | 103 + 309/Project/Source/Fault.h | 173 |  |  |  | UINT16 u16TdischgUTp_First |  |  |
| u16TdischgUTp_Second | 103 + 309/Project/Source/Fault.h | 175 |  |  |  | UINT16 u16TdischgUTp_Second |  |  |
| u16TdischgUTp_Third | 103 + 309/Project/Source/Fault.h | 176 |  |  |  | UINT16 u16TdischgUTp_Third |  |  |
| u16TdischgUTp_Rcv | 103 + 309/Project/Source/Fault.h | 177 |  |  |  | UINT16 u16TdischgUTp_Rcv |  | Refresh_Parameters |
| u16TdischgUTp_Filter | 103 + 309/Project/Source/Fault.h | 178 |  |  |  | UINT16 u16TdischgUTp_Filter |  |  |
| u16TmosOTp_First | 103 + 309/Project/Source/Fault.h | 179 |  |  |  | UINT16 u16TmosOTp_First |  |  |
| u16TmosOTp_Second | 103 + 309/Project/Source/Fault.h | 181 |  |  |  | UINT16 u16TmosOTp_Second |  |  |
| u16TmosOTp_Third | 103 + 309/Project/Source/Fault.h | 182 |  |  |  | UINT16 u16TmosOTp_Third |  |  |
| u16TmosOTp_Rcv | 103 + 309/Project/Source/Fault.h | 183 |  |  |  | UINT16 u16TmosOTp_Rcv |  |  |
| u16TmosOTp_Filter | 103 + 309/Project/Source/Fault.h | 184 |  |  |  | UINT16 u16TmosOTp_Filter |  |  |
| u16VdeltaOvp_First | 103 + 309/Project/Source/Fault.h | 185 |  |  |  | UINT16 u16VdeltaOvp_First |  |  |
| u16VdeltaOvp_Second | 103 + 309/Project/Source/Fault.h | 187 |  |  |  | UINT16 u16VdeltaOvp_Second |  |  |
| u16VdeltaOvp_Third | 103 + 309/Project/Source/Fault.h | 188 |  |  |  | UINT16 u16VdeltaOvp_Third |  |  |
| u16VdeltaOvp_Rcv | 103 + 309/Project/Source/Fault.h | 189 |  |  |  | UINT16 u16VdeltaOvp_Rcv |  |  |
| u16VdeltaOvp_Filter | 103 + 309/Project/Source/Fault.h | 190 |  |  |  | UINT16 u16VdeltaOvp_Filter |  |  |
| u16SocUp_First | 103 + 309/Project/Source/Fault.h | 191 |  |  |  | UINT16 u16SocUp_First |  |  |
| u16SocUp_Second | 103 + 309/Project/Source/Fault.h | 193 |  |  |  | UINT16 u16SocUp_Second |  |  |
| u16SocUp_Third | 103 + 309/Project/Source/Fault.h | 194 |  |  |  | UINT16 u16SocUp_Third |  |  |
| u16SocUp_Rcv | 103 + 309/Project/Source/Fault.h | 195 |  |  |  | UINT16 u16SocUp_Rcv |  |  |
| u16SocUp_Filter | 103 + 309/Project/Source/Fault.h | 196 |  |  |  | UINT16 u16SocUp_Filter |  |  |
| Fault_Flag_Fisrt | 103 + 309/Project/Source/Fault.h | 381 |  | extern |  | extern union FAULT_FLAG_FIRST Fault_Flag_Fisrt |  |  |
| Fault_Flag_Second | 103 + 309/Project/Source/Fault.h | 382 |  | extern |  | extern union FAULT_FLAG_SECOND Fault_Flag_Second |  |  |
| Fault_Flag_Third | 103 + 309/Project/Source/Fault.h | 383 |  | extern |  | extern union FAULT_FLAG_THIRD Fault_Flag_Third |  |  |
| Fault_record_Third | 103 + 309/Project/Source/Fault.h | 384 |  | extern |  | extern UINT16 Fault_record_Third[Record_len] |  | Fault_DebugWatchBind, Sci_ACK_0x03_ReadRegs_LCD |
| Fault_record_Third2 | 103 + 309/Project/Source/Fault.h | 386 |  | extern |  | extern UINT16 Fault_record_Third2[Record_len] | FaultWarnRecord2, Sci_WrReg_0x06_Reset_ProtectRecord | Fault_DebugWatchBind, Sci_ACK_0x03_ReadRegs_Data |
| FaultPoint_Third | 103 + 309/Project/Source/Fault.h | 388 |  | extern |  | extern UINT8 FaultPoint_Third |  | Fault_DebugWatchBind, Sci_ACK_0x03_ReadRegs_LCD |
| FaultPoint_Third2 | 103 + 309/Project/Source/Fault.h | 390 |  | extern |  | extern UINT8 FaultPoint_Third2 | FaultWarnRecord2, Sci_WrReg_0x06_Reset_ProtectRecord | Fault_DebugWatchBind, Sci_ACK_0x03_ReadRegs_Data |
| u16DsgSocInt | 103 + 309/Project/Source/Flash.c | 18 |  |  |  | UINT16 u16DsgSocInt | StorageFlash_LoadSocData, host_set_snapshot, soc_save | soc_load_or_default |
| u32CycleTimes | 103 + 309/Project/Source/Flash.c | 19 |  |  |  | UINT32 u32CycleTimes | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_save | soc_load_or_default |
| version | 103 + 309/Project/Source/Flash.c | 25 |  |  |  | UINT16 version | CtProtocol_Feed, StorageFlash_ProgramRecord | StorageFlash_ReadSlot |
| length | 103 + 309/Project/Source/Flash.c | 26 |  |  |  | UINT16 length | CtBoard_UartWrite, CtDebugLog_EncodeLatest, CtFlash_Read, CtFlash_Write, CtProtocol_Feed, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear, LedBar_Scan1ms | APP_LedBar, CanFeidao_SendFrame, CtApp_HandleFrame, CtCrc16_Calc, CtCrc16_Update, FlashProgramBytesVerified, LedBar_BuildGreedyFrameFromStart, LedBar_FrameEquals, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot |
| sequence | 103 + 309/Project/Source/Flash.c | 27 |  |  |  | UINT32 sequence | StorageFlash_LoadJournalPage, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, StorageFlash_SaveJournalPage | StorageFlash_WriteSlot |
| crc | 103 + 309/Project/Source/Flash.c | 28 |  |  |  | UINT16 crc | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, CtCrc16_Update, CtProtocol_Encode, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, boot_consume_iap_request, can_handle_end, can_handle_start | AppUpgrade_IsIapRequested, boot_request_valid |
| reserved | 103 + 309/Project/Source/Flash.c | 29 |  |  |  | UINT16 reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| reserved | 103 + 309/Project/Source/Flash.c | 35 |  |  |  | UINT8 reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| records | 103 + 309/Project/Source/Flash.c | 36 |  |  |  | UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2] | EEPROM_ResetData_EventRecord_ToDefault, LogEvent_EEPROM, StorageFlash_LoadLogData, StorageFlash_SaveLogData | ReadEEPROM_EventRecord_Parameters, Sci_ACK_0x03_ReadRegs_EventRecord |
| magic_inv | 103 + 309/Project/Source/Flash.c | 42 |  |  |  | UINT32 magic_inv | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid |
| request | 103 + 309/Project/Source/Flash.c | 43 |  |  |  | UINT32 request | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, AppUpgrade_MailboxCrc, boot_crc, boot_request_valid, legacy_send_read_ack, legacy_send_write_ack, serial_send_ack, serial_send_error, serial_send_read_regs |
| request_inv | 103 + 309/Project/Source/Flash.c | 44 |  |  |  | UINT32 request_inv | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid |
| crc | 103 + 309/Project/Source/Flash.c | 45 |  |  |  | UINT32 crc | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, CtCrc16_Update, CtProtocol_Encode, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, boot_consume_iap_request, can_handle_end, can_handle_start | AppUpgrade_IsIapRequested, boot_request_valid |
| s_flash | 103 + 309/Project/Source/Flash.c | 52 | static |  |  | static FLASH_RUNTIME s_flash | StorageFlash_BeginWrite, StorageFlash_EndWrite, iap_flash_abort, iap_flash_begin, iap_flash_ensure_page_erased, iap_flash_finish, iap_flash_write, serial_write_block | Flash_DebugWatchBind, StorageFlash_IsBusy, serial_status_word, vector_valid_in_buffer |
| u16SocNow | 103 + 309/Project/Source/Flash.h | 63 |  |  |  | UINT16 u16SocNow | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_load_or_default, soc_save | host_internal_soc |
| u16DsgSocInt | 103 + 309/Project/Source/Flash.h | 64 |  |  |  | UINT16 u16DsgSocInt | StorageFlash_LoadSocData, host_set_snapshot, soc_save | soc_load_or_default |
| u16MaxErrorPercent | 103 + 309/Project/Source/Flash.h | 65 |  |  |  | UINT16 u16MaxErrorPercent | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_save |  |
| u32CycleTimes | 103 + 309/Project/Source/Flash.h | 66 |  |  |  | UINT32 u32CycleTimes | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_save | soc_load_or_default |
| u32CapNow | 103 + 309/Project/Source/Flash.h | 67 |  |  |  | UINT32 u32CapNow | SOC_ResetStoredSnapshotToDefault, host_set_snapshot, soc_save | soc_load_or_default |
| u32CapFull | 103 + 309/Project/Source/Flash.h | 68 |  |  |  | UINT32 u32CapFull | SOC_ResetStoredSnapshotToDefault, host_set_snapshot, soc_save |  |
| u32LearnPassedAs10 | 103 + 309/Project/Source/Flash.h | 69 |  |  |  | UINT32 u32LearnPassedAs10 | host_set_snapshot, soc_save | soc_load_or_default |
| u16LearnAnchorSoc | 103 + 309/Project/Source/Flash.h | 70 |  |  |  | UINT16 u16LearnAnchorSoc |  |  |
| u16LearnState | 103 + 309/Project/Source/Flash.h | 71 |  |  |  | UINT16 u16LearnState |  |  |
| u16Flags | 103 + 309/Project/Source/Flash.h | 72 |  |  |  | UINT16 u16Flags | host_set_snapshot, soc_save | soc_load_or_default, test_rebound_flag_clears_when_holdoff_expires |
| u16Reserved | 103 + 309/Project/Source/Flash.h | 73 |  |  |  | UINT16 u16Reserved[4] |  |  |
| other | 103 + 309/Project/Source/Flash.h | 79 |  |  |  | UINT16 other[FLASH_STORAGE_RW_PARAM_OTHER_WORD_COUNT] | DebugWatch_BindAll, EEPROM_BuildRWParamData | EEPROM_ApplyRWParamData, EEPROM_RWParamDataIsValid |
| reserved | 103 + 309/Project/Source/Flash.h | 80 |  |  |  | UINT16 reserved[FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT] | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| u16State | 103 + 309/Project/Source/Flash.h | 86 |  |  |  | UINT16 u16State | FactoryAging_LoadStoredProgress, FactoryAging_MarkDone, FactoryAging_SaveStoredProgress, StorageFlash_LoadFactoryAgingData | StorageFlash_PrintBootCheck |
| u16DurationHours | 103 + 309/Project/Source/Flash.h | 87 |  |  |  | UINT16 u16DurationHours | FactoryAging_SaveStoredProgress | FactoryAging_LoadDurationFromData |
| CRC8Table | 103 + 309/Project/Source/I2C_AFE1.c | 66 | static |  |  | static const UINT8 CRC8Table[] = { 0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x... |  | CRC8cal, I2C_AFE1_DebugWatchBind |
| Temp2 | 103 + 309/Project/Source/I2C_AFE1.h | 52 |  |  |  | UINT16 Temp2 |  | UpdateVoltageFromBqMaximo |
| Temp3 | 103 + 309/Project/Source/I2C_AFE1.h | 53 |  |  |  | UINT16 Temp3 |  | UpdateVoltageFromBqMaximo |
| Cur1 | 103 + 309/Project/Source/I2C_AFE1.h | 54 |  |  |  | INT16 Cur1 |  |  |
| Cell | 103 + 309/Project/Source/I2C_AFE1.h | 55 |  |  |  | UINT16 Cell[16] |  | UpdateVoltageFromBqMaximo |
| Cadc | 103 + 309/Project/Source/I2C_AFE1.h | 56 |  |  |  | INT16 Cadc |  | UpdateVoltageFromBqMaximo |
| u16TempBat | 103 + 309/Project/Source/I2C_AFE1.h | 62 |  |  |  | UINT16 u16TempBat[3] | UpdateVoltageFromBqMaximo | DataLoad_Temperature |
| u32VBat | 103 + 309/Project/Source/I2C_AFE1.h | 63 |  |  |  | UINT32 u32VBat |  |  |
| u16Current | 103 + 309/Project/Source/I2C_AFE1.h | 64 |  |  |  | UINT16 u16Current | DataLoad_CurrentReadCadcRaw, UpdateVoltageFromBqMaximo | DataLoad_Current, SystemDebug_Snapshot |
| Registers_AFE1 | 103 + 309/Project/Source/I2C_AFE1.h | 93 |  | extern |  | extern AFEDATA Registers_AFE1 |  |  |
| tick_10ms | 103 + 309/Project/Source/IrqDebug.h | 56 |  |  |  | uint32_t tick_10ms | DebugWatch_BindAll, IrqDebug_RecordEvent, SystemDebug_Snapshot | DbgPrint_Summary |
| exti_pr | 103 + 309/Project/Source/IrqDebug.h | 57 |  |  |  | uint32_t exti_pr | IrqDebug_RecordEvent, SystemDebug_SnapshotMcuResources |  |
| nvic_ispr0 | 103 + 309/Project/Source/IrqDebug.h | 58 |  |  |  | uint32_t nvic_ispr0 | IrqDebug_RecordEvent |  |
| nvic_iabr0 | 103 + 309/Project/Source/IrqDebug.h | 59 |  |  |  | uint32_t nvic_iabr0 | IrqDebug_RecordEvent |  |
| id | 103 + 309/Project/Source/IrqDebug.h | 60 |  |  |  | uint16_t id | CtCan_IapSendData, CtCan_ReadFactoryAgingBroadcast, IrqDebug_RecordEvent, can_rx_push, can_send_ack, send_app_cmd, send_iap_ctrl | CtBoard_CanSend, CtCan_IapPollAck, CtCan_IapWaitAck, IrqDebug_Count, IrqDebug_CountFast, Sci_BmsFunctionIdIsSupported, can_decode_request, decode_app_ack, decode_app_word_frame |
| vectactive | 103 + 309/Project/Source/IrqDebug.h | 61 |  |  |  | uint16_t vectactive | IrqDebug_RecordEvent |  |
| phase | 103 + 309/Project/Source/IrqDebug.h | 62 |  |  |  | uint8_t phase | CtUpgrade_Abort, CtUpgrade_Task, IrqDebug_CountFast, IrqDebug_RecordEvent, reset_context, set_error, set_phase | IrqDebug_SetPhase |
| reserved | 103 + 309/Project/Source/IrqDebug.h | 63 |  |  |  | uint8_t reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| phase | 103 + 309/Project/Source/IrqDebug.h | 68 |  |  | volatile | volatile uint32_t phase[IRQDBG_PHASE_COUNT][IRQDBG_COUNT] | CtUpgrade_Abort, CtUpgrade_Task, IrqDebug_CountFast, IrqDebug_RecordEvent, reset_context, set_error, set_phase | IrqDebug_SetPhase |
| phase_enter_count | 103 + 309/Project/Source/IrqDebug.h | 69 |  |  | volatile | volatile uint32_t phase_enter_count[IRQDBG_PHASE_COUNT] |  | IrqDebug_SetPhase |
| event_seq | 103 + 309/Project/Source/IrqDebug.h | 70 |  |  | volatile | volatile uint32_t event_seq |  | IrqDebug_RecordEvent |
| last_id | 103 + 309/Project/Source/IrqDebug.h | 71 |  |  | volatile | volatile uint16_t last_id | IrqDebug_CountFast, SystemDebug_ModuleHeartbeat, SystemDebug_SnapshotMcuResources |  |
| last_vectactive | 103 + 309/Project/Source/IrqDebug.h | 72 |  |  | volatile | volatile uint16_t last_vectactive | IrqDebug_CountFast, SystemDebug_SnapshotMcuResources | IrqDebug_RecordEvent |
| current_phase | 103 + 309/Project/Source/IrqDebug.h | 73 |  |  | volatile | volatile uint8_t current_phase | IrqDebug_SetPhase, SystemDebug_SnapshotMcuResources | IrqDebug_CountFast |
| last_phase | 103 + 309/Project/Source/IrqDebug.h | 74 |  |  | volatile | volatile uint8_t last_phase | IrqDebug_CountFast, IrqDebug_SetPhase | IrqDebug_RecordEvent |
| last_tick_10ms | 103 + 309/Project/Source/IrqDebug.h | 75 |  |  | volatile | volatile uint32_t last_tick_10ms | IrqDebug_SetPhase |  |
| last_exti_pr | 103 + 309/Project/Source/IrqDebug.h | 76 |  |  | volatile | volatile uint32_t last_exti_pr | IrqDebug_CountFast | IrqDebug_RecordEvent |
| last_nvic_ispr0 | 103 + 309/Project/Source/IrqDebug.h | 77 |  |  | volatile | volatile uint32_t last_nvic_ispr0 | IrqDebug_CountFast | IrqDebug_RecordEvent |
| last_nvic_iabr0 | 103 + 309/Project/Source/IrqDebug.h | 78 |  |  | volatile | volatile uint32_t last_nvic_iabr0 | IrqDebug_CountFast | IrqDebug_RecordEvent |
| event_head | 103 + 309/Project/Source/IrqDebug.h | 79 |  |  | volatile | volatile uint8_t event_head | IrqDebug_RecordEvent |  |
| event_count | 103 + 309/Project/Source/IrqDebug.h | 80 |  |  | volatile | volatile uint8_t event_count | IrqDebug_RecordEvent, SystemDebug_SnapshotMcuResources |  |
| events | 103 + 309/Project/Source/IrqDebug.h | 81 |  |  | volatile | volatile struct IRQ_DEBUG_EVENT events[IRQ_DEBUG_EVENT_RING_SIZE] | IrqDebug_RecordEvent |  |
| g_stIrqDebug | 103 + 309/Project/Source/IrqDebug.h | 83 |  | extern | volatile | extern volatile struct IRQ_DEBUG_STATE g_stIrqDebug |  |  |
| high_pin | 103 + 309/Project/Source/LedBar.c | 63 |  |  |  | uint8_t high_pin | LedBar_FindRouteByPins | LedBar_OutputRoute, LedBar_TransitionCost |
| pin | 103 + 309/Project/Source/LedBar.c | 69 |  |  |  | uint16_t pin | LedBar_GetPinIndex, LedBar_PinToOutput, LedBar_PinWrite, SystemDebug_SnapshotMcuResources | Conf_InitGpioMode, Conf_InitWakeupInputExti, LedBar_PinToOutputMode |
| length | 103 + 309/Project/Source/LedBar.c | 75 |  |  |  | uint8_t length | CtBoard_UartWrite, CtDebugLog_EncodeLatest, CtFlash_Read, CtFlash_Write, CtProtocol_Feed, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear, LedBar_Scan1ms | APP_LedBar, CanFeidao_SendFrame, CtApp_HandleFrame, CtCrc16_Calc, CtCrc16_Update, FlashProgramBytesVerified, LedBar_BuildGreedyFrameFromStart, LedBar_FrameEquals, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot |
| sleep | 103 + 309/Project/Source/LedBar.c | 81 |  |  |  | uint8_t sleep | DebugWatch_BindAll, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_PrepareForStop, LedBar_SetSleep, LedBar_ShowSleepSocPreview, SleepDeal_DebugWatchBind, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | APP_LedBar, DbgPrint_All, LedBar_BuildCurrentFrame, LedBar_IsActiveForLowPower, LedBar_Scan1ms, SystemDebug_Snapshot |
| blank | 103 + 309/Project/Source/LedBar.c | 82 |  |  |  | uint8_t blank | APP_LedBar, LedBar_Clear, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_PrepareForStop, LedBar_SetIndicators, LedBar_SetNumber, LedBar_ShowSleepSocPreview, SystemDebug_RefreshModuleStates | LedBar_BuildCurrentFrame, SystemDebug_Snapshot |
| number | 103 + 309/Project/Source/LedBar.c | 83 |  |  |  | uint8_t number | APP_LedBar, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_SetNumber, LedBar_ShowSleepSocPreview | DbgPrint_All, LedBar_BuildCurrentFrame, SystemDebug_Snapshot |
| indicator_mask | 103 + 309/Project/Source/LedBar.c | 84 |  |  |  | uint8_t indicator_mask | APP_LedBar, LedBar_Init, LedBar_SetIndicatorState, LedBar_SetIndicators, LedBar_ShowSleepSocPreview | LedBar_BuildCurrentFrame, LedBar_BuildTargetMask, LedBar_GetDebugSnapshot |
| test_single_segment_id | 103 + 309/Project/Source/LedBar.c | 85 |  |  |  | uint8_t test_single_segment_id | LedBar_Init, LedBar_SetSingleSegmentIndex |  |
| frame | 103 + 309/Project/Source/LedBar.c | 86 |  |  |  | LedBarFrame frame | CtApp_HandleFrame, CtBoard_CanSend, CtCan_IapPollAck, CtCan_IapSendData, CtCan_IapWaitAck, CtCan_ReadFactoryAgingBroadcast, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear | APP_LedBar, CtBoard_CanRecv, CtCan_AppReadRegs, CtSelfIap_PollCan, LedBar_BuildCurrentFrame, LedBar_BuildGreedyFrameFromStart, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot, LedBar_ImproveFrameOrder, LedBar_Init |
| scan_index | 103 + 309/Project/Source/LedBar.c | 87 |  |  |  | uint8_t scan_index | LedBar_ApplyFrame, LedBar_Init, LedBar_Scan1ms | LedBar_GetDebugSnapshot |
| scan_timer_initialized | 103 + 309/Project/Source/LedBar.c | 88 |  |  |  | uint8_t scan_timer_initialized | LedBar_Init, LedBar_ScanTimerInit, LedBar_StopScanTimer |  |
| scan_timer_enabled | 103 + 309/Project/Source/LedBar.c | 89 |  |  |  | uint8_t scan_timer_enabled | LedBar_Init, LedBar_StartScanTimer, LedBar_StopScanTimer | APP_LedBar, LedBar_ApplyFrame, LedBar_IsActiveForLowPower |
| soc_display_10ms | 103 + 309/Project/Source/LedBar.c | 90 |  |  |  | uint16_t soc_display_10ms | LedBar_Init, LedBar_RequestSocDisplayWindow, LedBar_RequestStartupDisplayWindow, LedBar_ServiceSwitch | LedBar_GetDebugSnapshot, LedBar_IsActiveForLowPower, LedBar_IsDisplayRequested |
| startup_display_armed | 103 + 309/Project/Source/LedBar.c | 91 |  |  |  | uint8_t startup_display_armed | LedBar_Init, LedBar_ServiceStartupDisplayWindow |  |
| key_hold_10ms | 103 + 309/Project/Source/LedBar.c | 92 |  |  |  | uint32_t key_hold_10ms | LedBar_Init, LedBar_ServiceSwitch |  |
| key_press_start_10ms | 103 + 309/Project/Source/LedBar.c | 93 |  |  |  | uint32_t key_press_start_10ms | LedBar_Init, LedBar_ServiceSwitch |  |
| key_last_pressed | 103 + 309/Project/Source/LedBar.c | 94 |  |  |  | uint8_t key_last_pressed | LedBar_Init, LedBar_ServiceSwitch |  |
| key_long_handled | 103 + 309/Project/Source/LedBar.c | 95 |  |  |  | uint8_t key_long_handled | LedBar_Init, LedBar_ServiceSwitch |  |
| key_filter_initialized | 103 + 309/Project/Source/LedBar.c | 96 |  |  |  | uint8_t key_filter_initialized | LedBar_Init, LedBar_ServiceSwitch |  |
| key_active | 103 + 309/Project/Source/LedBar.c | 97 |  |  |  | uint8_t key_active | LedBar_GetDebugSnapshot, LedBar_Init | LedBar_ServiceSwitch, SystemDebug_RefreshModuleStates, SystemDebug_Snapshot |
| key_on_10ms | 103 + 309/Project/Source/LedBar.c | 98 |  |  |  | uint8_t key_on_10ms | LedBar_Init | LedBar_ServiceSwitch |
| key_off_10ms | 103 + 309/Project/Source/LedBar.c | 99 |  |  |  | uint8_t key_off_10ms | LedBar_Init | LedBar_ServiceSwitch |
| mcu_wk_filter_initialized | 103 + 309/Project/Source/LedBar.c | 100 |  |  |  | uint8_t mcu_wk_filter_initialized | LedBar_Init, LedBar_ServiceMcuWakeFilter |  |
| mcu_wk_active | 103 + 309/Project/Source/LedBar.c | 101 |  |  |  | uint8_t mcu_wk_active | APP_LedBar, LedBar_Init | LedBar_IsMcuWakeActive, LedBar_ServiceMcuWakeFilter |
| mcu_wk_on_10ms | 103 + 309/Project/Source/LedBar.c | 102 |  |  |  | uint8_t mcu_wk_on_10ms | LedBar_Init | LedBar_ServiceMcuWakeFilter |
| mcu_wk_off_10ms | 103 + 309/Project/Source/LedBar.c | 103 |  |  |  | uint8_t mcu_wk_off_10ms | LedBar_Init | LedBar_ServiceMcuWakeFilter |
| s_ledbar_routes | 103 + 309/Project/Source/LedBar.c | 105 | static |  |  | static const LedBarRoute s_ledbar_routes[LEDBAR_ROUTE_COUNT] = { {3u, 2u}, {3u, 1u}, {2u, 1u}, {1u, 2u}, {2u, 3u}, {1u, 3u}, {1u, 4u}, {2u, 4u}, {3u, 4u}, {1u, 0u}, {0u, 1u}, ... | LedBar_FindRouteByPins | LedBar_DebugWatchBind, LedBar_OutputRoute, LedBar_TransitionCost |
| s_ledbar_pins | 103 + 309/Project/Source/LedBar.c | 127 | static |  |  | static const LedBarPinDef s_ledbar_pins[LEDBAR_PIN_COUNT] = { {LEDBAR_GPIO_P1, LEDBAR_PIN_P1}, {LEDBAR_GPIO_P3, LEDBAR_PIN_P3}, {LEDBAR_GPIO_P2, LEDBAR_PIN_P2}, {LEDBAR_GPIO_P4, LEDBAR_PIN_P4}, {LEDBA... |  | LedBar_DebugWatchBind, LedBar_PinToOutput, LedBar_PinToOutputMode, LedBar_PinWrite |
| s_ledbar_digit_map | 103 + 309/Project/Source/LedBar.c | 136 | static |  |  | static const uint8_t s_ledbar_digit_map[10] = { LEDBAR_DIGIT_BIT_A \\| LEDBAR_DIGIT_BIT_B \\| LEDBAR_DIGIT_BIT_C \\| LEDBAR_DIGIT_BIT_D \\| LEDBAR_DIGIT_BIT_E \\| LEDBAR_DIGIT_BIT_F, LEDBAR_DIGIT_BIT_B \\| LEDBAR_DIG... |  | LedBar_AddDigitRoutes, LedBar_DebugWatchBind |
| s_ledbar | 103 + 309/Project/Source/LedBar.c | 159 | static |  |  | static LedBarRuntime s_ledbar = { 0u, 0u, 1u, 0u, LEDBAR_ICON_PERCENT_MASK, } | APP_LedBar, LedBar_ApplyFrame, LedBar_Clear, LedBar_EnsureInit, LedBar_Init, LedBar_IsActiveForLowPower, LedBar_PrepareForStop, LedBar_RequestSocDisplayWindow, LedBar_RequestStartupDisplayWindow, LedBar_Scan1ms | LedBar_BuildCurrentFrame, LedBar_DebugWatchBind, LedBar_GetDebugSnapshot, LedBar_IsDisplayRequested, LedBar_IsMcuWakeActive, LedBar_SetIndicatorState |
| records | 103 + 309/Project/Source/LogRecord.c | 8 |  |  |  | UINT8 records[EVENT_RECORD_LENGTH][2] | EEPROM_ResetData_EventRecord_ToDefault, LogEvent_EEPROM, StorageFlash_LoadLogData, StorageFlash_SaveLogData | ReadEEPROM_EventRecord_Parameters, Sci_ACK_0x03_ReadRegs_EventRecord |
| flags | 103 + 309/Project/Source/LogRecord.c | 9 |  |  |  | LOG_RECORD_FLAG flags | CtProtocol_Feed, LogEvent_Record, LogRecord_RequestSleep, LogRecord_RequestStartup | App_LogRecord, CtProtocol_Encode, host_set_snapshot |
| uptimeSeconds | 103 + 309/Project/Source/LogRecord.c | 10 |  |  |  | UINT32 uptimeSeconds |  | App_LogRecord, LogRecord_CanSaveEvent, LogRecord_MarkEventSaved |
| lastSaveSeconds | 103 + 309/Project/Source/LogRecord.c | 11 |  |  |  | UINT32 lastSaveSeconds[EVENT_NUM] | LogRecord_MarkEventSaved | LogRecord_CanSaveEvent |
| lastSaveValid | 103 + 309/Project/Source/LogRecord.c | 12 |  |  |  | UINT8 lastSaveValid[EVENT_NUM] | LogRecord_MarkEventSaved | LogRecord_CanSaveEvent |
| eventLatch | 103 + 309/Project/Source/LogRecord.c | 13 |  |  |  | UINT8 eventLatch[EVENT_NUM] | LogEvent_Record |  |
| cbcTemp | 103 + 309/Project/Source/LogRecord.c | 14 |  |  |  | UINT8 cbcTemp | LogEvent_Record |  |
| su32_Interval_S_Tcnt | 103 + 309/Project/Source/LogRecord.c | 16 |  |  |  | UINT32 su32_Interval_S_Tcnt = 0 | App_LogRecord, RtcSleep_PortAddRuntimeSeconds | LogRecord_DebugWatchBind, RtcSleep_PortCommitResetSleep |
| s_log_record | 103 + 309/Project/Source/LogRecord.c | 18 | static |  |  | static LogRecordRuntime s_log_record | App_LogRecord, EEPROM_ResetData_EventRecord_ToDefault, LogEvent_EEPROM, LogEvent_Record, LogRecord_MarkEventSaved, LogRecord_RequestSleep, LogRecord_RequestStartup, ReadEEPROM_EventRecord_Parameters | LogRecord_CanSaveEvent, LogRecord_DebugWatchBind, Sci_ACK_0x03_ReadRegs_EventRecord |
| Log_Sleep | 103 + 309/Project/Source/LogRecord.h | 37 |  |  |  | UINT8 Log_Sleep :1 | LogEvent_Record, LogRecord_RequestSleep |  |
| BatOvp_Third | 103 + 309/Project/Source/LogRecord.h | 38 |  |  |  | UINT8 BatOvp_Third :1 |  | new_todo_logi |
| BatUvp_Third | 103 + 309/Project/Source/LogRecord.h | 39 |  |  |  | UINT8 BatUvp_Third :1 |  |  |
| Rcv | 103 + 309/Project/Source/LogRecord.h | 40 |  |  |  | UINT8 Rcv :4 |  |  |
| su32_Interval_S_Tcnt | 103 + 309/Project/Source/LogRecord.h | 44 |  | extern |  | extern UINT32 su32_Interval_S_Tcnt | App_LogRecord, RtcSleep_PortAddRuntimeSeconds | LogRecord_DebugWatchBind, RtcSleep_PortCommitResetSleep |
| BMS_HardWareVersion | 103 + 309/Project/Source/ProductionID.h | 8 |  |  |  | UINT8 BMS_HardWareVersion[PRODUCT_ID_LENGTH_MAX] |  | InitProID_DefaultData, Sci_ACK_0x03_ReadRegs_LCD, Sci_WrRegs_0x10_SN_Version |
| BMS_SoftWareVersion | 103 + 309/Project/Source/ProductionID.h | 9 |  |  |  | UINT8 BMS_SoftWareVersion[PRODUCT_ID_LENGTH_MAX] |  | InitProID_DefaultData, Sci_ACK_0x03_ReadRegs_LCD, Sci_WrRegs_0x10_SN_Version |
| BMS_SerialNumberLength | 103 + 309/Project/Source/ProductionID.h | 10 |  |  |  | UINT16 BMS_SerialNumberLength | InitProID_DefaultData | Sci_WrRegs_0x10_SN_Version |
| BMS_HardWareVersionLength | 103 + 309/Project/Source/ProductionID.h | 12 |  |  |  | UINT16 BMS_HardWareVersionLength | InitProID_DefaultData | Sci_WrRegs_0x10_SN_Version |
| BMS_SoftWareVersionLength | 103 + 309/Project/Source/ProductionID.h | 13 |  |  |  | UINT16 BMS_SoftWareVersionLength | InitProID_DefaultData | Sci_WrRegs_0x10_SN_Version |
| BMS_SerialNumberHeadAdress | 103 + 309/Project/Source/ProductionID.h | 14 |  |  |  | UINT16 BMS_SerialNumberHeadAdress |  |  |
| BMS_HardWareVersionHeadAdress | 103 + 309/Project/Source/ProductionID.h | 16 |  |  |  | UINT16 BMS_HardWareVersionHeadAdress |  |  |
| BMS_SoftWareVersionHeadAdress | 103 + 309/Project/Source/ProductionID.h | 17 |  |  |  | UINT16 BMS_SoftWareVersionHeadAdress |  |  |
| ProductionInfor | 103 + 309/Project/Source/ProductionID.h | 19 |  | extern |  | extern PRODUCTION_ID_INFO ProductionInfor |  |  |
| wake | 103 + 309/Project/Source/RTC.c | 7 |  |  | volatile | volatile bool wake | RTC_ClearStopWakeup, RTC_HandleAlarmWakeup | RTC_IsStopWakeup |
| last | 103 + 309/Project/Source/RTC.c | 9 |  |  |  | UINT32 last | ADC_ResetAnlogCalSchedule, App_AnlogCal, LP_RecordLastSleepSeconds, RTC_WKTimeConfig, soc_table_percent | LP_GetLastSleepSeconds, RTC_GetLastWakeupPeriodSeconds, SystemDebug_Snapshot |
| wake_override | 103 + 309/Project/Source/RTC.c | 10 |  |  |  | UINT32 wake_override | RTC_SetWakeupPeriodSeconds | RTC_GetWakeupPeriodSeconds |
| s_rtc | 103 + 309/Project/Source/RTC.c | 12 | static |  |  | static RTC_RUNTIME s_rtc = { 0U, false, {0}, 1U, 0U } | App_RTC, RTC_ClearStopWakeup, RTC_HandleAlarmWakeup, RTC_IRQHandler, RTC_SetWakeupPeriodSeconds, RTC_WKTimeConfig | Get_RTC_Time, RTC_DebugWatchBind, RTC_GetLastWakeupPeriodSeconds, RTC_GetWakeupPeriodSeconds, RTC_IsStopWakeup |
| month_days | 103 + 309/Project/Source/RTC.c | 21 | static |  |  | static const UINT8 month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31} |  | RTC_DebugWatchBind, RTC_GetMonthDays |
| RTC_Time_Month | 103 + 309/Project/Source/RTC.h | 31 |  |  |  | UINT16 RTC_Time_Month | Seccond_Cal, Second_To_RTCtime | Sci_ACK_0x03_ReadRegs_Data |
| RTC_Time_Day | 103 + 309/Project/Source/RTC.h | 32 |  |  |  | UINT16 RTC_Time_Day | Second_To_RTCtime | Sci_ACK_0x03_ReadRegs_Data, Seccond_Cal |
| RTC_Time_Hour | 103 + 309/Project/Source/RTC.h | 33 |  |  |  | UINT16 RTC_Time_Hour | Second_To_RTCtime | Sci_ACK_0x03_ReadRegs_Data, Seccond_Cal |
| RTC_Time_Minute | 103 + 309/Project/Source/RTC.h | 34 |  |  |  | UINT16 RTC_Time_Minute | Second_To_RTCtime | Sci_ACK_0x03_ReadRegs_Data, Seccond_Cal |
| RTC_Time_Second | 103 + 309/Project/Source/RTC.h | 36 |  |  |  | UINT16 RTC_Time_Second | Second_To_RTCtime | Sci_ACK_0x03_ReadRegs_Data, Seccond_Cal |
| RTC_Alarm_Year | 103 + 309/Project/Source/RTC.h | 37 |  |  |  | UINT16 RTC_Alarm_Year |  |  |
| RTC_Alarm_Month | 103 + 309/Project/Source/RTC.h | 38 |  |  |  | UINT16 RTC_Alarm_Month |  |  |
| RTC_Alarm_Day | 103 + 309/Project/Source/RTC.h | 39 |  |  |  | UINT16 RTC_Alarm_Day |  |  |
| RTC_Alarm_Hour | 103 + 309/Project/Source/RTC.h | 41 |  |  |  | UINT16 RTC_Alarm_Hour |  |  |
| RTC_Alarm_Minute | 103 + 309/Project/Source/RTC.h | 42 |  |  |  | UINT16 RTC_Alarm_Minute |  |  |
| RTC_Alarm_Second | 103 + 309/Project/Source/RTC.h | 43 |  |  |  | UINT16 RTC_Alarm_Second |  |  |
| s_sh_afe_ocd1v_occv | 103 + 309/Project/Source/SH367309_DataDeal.c | 6 | static |  |  | static const UINT16 s_sh_afe_ocd1v_occv[16] = {20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 160, 180, 200} |  | Refresh_Parameters, SH367309Data_DebugWatchBind |
| s_sh_afe_ocd2v | 103 + 309/Project/Source/SH367309_DataDeal.c | 8 | static |  |  | static const UINT16 s_sh_afe_ocd2v[16] = {30, 40, 50, 60, 70, 80, 90, 100, 120, 140, 160, 180, 200, 300, 400, 500} |  | Refresh_Parameters, SH367309Data_DebugWatchBind |
| g_u16ShAfeScvTable | 103 + 309/Project/Source/SH367309_DataDeal.c | 9 |  |  |  | const UINT16 g_u16ShAfeScvTable[16] = {50, 80, 110, 140, 170, 200, 230, 260, 290, 320, 350, 400, 500, 600, 800, 1000} |  | InitShortCur, Refresh_Parameters, SH367309Data_DebugWatchBind |
| s_sh_afe_ovt_uvt | 103 + 309/Project/Source/SH367309_DataDeal.c | 10 | static |  |  | static const UINT16 s_sh_afe_ovt_uvt[16] = {100, 200, 300, 400, 600, 800, 1000, 2000, 3000, 4000, 6000, 8000, 10000, 20000, 30000, 40000} |  | Refresh_Parameters, SH367309Data_DebugWatchBind |
| g_u16ShAfeSctTable | 103 + 309/Project/Source/SH367309_DataDeal.c | 11 |  |  |  | const UINT16 g_u16ShAfeSctTable[16] = {0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960} |  | InitShortCur, Refresh_Parameters, SH367309Data_DebugWatchBind |
| s_sh_afe_ocd1t | 103 + 309/Project/Source/SH367309_DataDeal.c | 12 | static |  |  | static const UINT16 s_sh_afe_ocd1t[16] = {50, 100, 200, 400, 600, 800, 1000, 2000, 4000, 6000, 8000, 10000, 15000, 20000, 30000, 40000} |  | Refresh_Parameters, SH367309Data_DebugWatchBind |
| s_sh_afe_occt_ocd2t | 103 + 309/Project/Source/SH367309_DataDeal.c | 13 | static |  |  | static const UINT16 s_sh_afe_occt_ocd2t[16] = {10, 20, 40, 60, 80, 100, 200, 400, 600, 800, 1000, 2000, 4000, 8000, 10000, 20000} |  | Refresh_Parameters, SH367309Data_DebugWatchBind |
| AFE_ROM_PARAMETERS_Struction | 103 + 309/Project/Source/SH367309_DataDeal.c | 14 |  |  |  | AFE_ROM_PARAMETERS_TypeDef AFE_ROM_PARAMETERS_Struction = {0} | InitShortCur, Refresh_Parameters | SH367309Data_DebugWatchBind, SH367309_UpdataAfeConfig, Write_Parameters |
| AFE_Parameters_RS485_Struction | 103 + 309/Project/Source/SH367309_DataDeal.c | 16 | static |  |  | static AFE_Parameters_RS485_Typedef AFE_Parameters_RS485_Struction = AFE_PARAMETERS_RS485_STRUCTION_DEFAULT |  | AFE_CopyCurValues, AFE_RestoreCurValues, EEPROM_ResetData_AFE_ParametersToDefault, ReadEEPROM_AFE_Parameters, Refresh_Parameters, SH367309Data_DebugWatchBind, Sci_ACK_0x03_RW_AFE_Parameters, Sci_WrRegs_0x10_AFE_Parameters |
| ucMTPBuffer | 103 + 309/Project/Source/SH367309_DataDeal.c | 17 |  | extern |  | extern UINT8 ucMTPBuffer[26] | Refresh_Parameters | SH367309Func_DebugWatchBind |
| iSheldTemp_10K_NTC | 103 + 309/Project/Source/SH367309_DataDeal.c | 19 |  | extern |  | extern const UINT16 iSheldTemp_10K_NTC[141] |  | Refresh_Parameters, SH367309Func_DebugWatchBind |
| BAL | 103 + 309/Project/Source/SH367309_DataDeal.h | 95 |  |  |  | UINT8 BAL :1 | Refresh_Parameters |  |
| OCPM | 103 + 309/Project/Source/SH367309_DataDeal.h | 96 |  |  |  | UINT8 OCPM :1 |  |  |
| ENMOS | 103 + 309/Project/Source/SH367309_DataDeal.h | 97 |  |  |  | UINT8 ENMOS :1 |  |  |
| ENPCH | 103 + 309/Project/Source/SH367309_DataDeal.h | 98 |  |  |  | UINT8 ENPCH :1 |  |  |
| EUVR | 103 + 309/Project/Source/SH367309_DataDeal.h | 100 |  |  |  | UINT8 EUVR :1 |  |  |
| OCRA | 103 + 309/Project/Source/SH367309_DataDeal.h | 102 |  |  |  | UINT8 OCRA :1 |  |  |
| CTLC | 103 + 309/Project/Source/SH367309_DataDeal.h | 103 |  |  |  | UINT8 CTLC :2 | Refresh_Parameters |  |
| DIS_PF | 103 + 309/Project/Source/SH367309_DataDeal.h | 105 |  |  |  | UINT8 DIS_PF :1 |  |  |
| UV_OP | 103 + 309/Project/Source/SH367309_DataDeal.h | 106 |  |  |  | UINT8 UV_OP :1 |  |  |
| Reserve | 103 + 309/Project/Source/SH367309_DataDeal.h | 107 |  |  |  | UINT8 Reserve :1 |  |  |
| E0VB | 103 + 309/Project/Source/SH367309_DataDeal.h | 108 |  |  |  | UINT8 E0VB :1 |  |  |
| LDRT | 103 + 309/Project/Source/SH367309_DataDeal.h | 115 |  |  |  | UINT8 LDRT :2 |  |  |
| OVT | 103 + 309/Project/Source/SH367309_DataDeal.h | 116 |  |  |  | UINT8 OVT :4 | Refresh_Parameters |  |
| OVL | 103 + 309/Project/Source/SH367309_DataDeal.h | 117 |  |  |  | UINT8 OVL | Refresh_Parameters |  |
| Reserve | 103 + 309/Project/Source/SH367309_DataDeal.h | 123 |  |  |  | UINT8 Reserve :2 |  |  |
| UVT | 103 + 309/Project/Source/SH367309_DataDeal.h | 124 |  |  |  | UINT8 UVT :4 | Refresh_Parameters |  |
| OVRL | 103 + 309/Project/Source/SH367309_DataDeal.h | 125 |  |  |  | UINT8 OVRL | Refresh_Parameters |  |
| UVR | 103 + 309/Project/Source/SH367309_DataDeal.h | 130 |  |  |  | UINT8 UVR | Refresh_Parameters |  |
| PREV | 103 + 309/Project/Source/SH367309_DataDeal.h | 136 |  |  |  | UINT8 PREV |  |  |
| PFV | 103 + 309/Project/Source/SH367309_DataDeal.h | 141 |  |  |  | UINT8 PFV |  |  |
| OCD1V | 103 + 309/Project/Source/SH367309_DataDeal.h | 147 |  |  |  | UINT8 OCD1V :4 | Refresh_Parameters |  |
| OCD2T | 103 + 309/Project/Source/SH367309_DataDeal.h | 148 |  |  |  | UINT8 OCD2T :4 | Refresh_Parameters |  |
| OCD2V | 103 + 309/Project/Source/SH367309_DataDeal.h | 149 |  |  |  | UINT8 OCD2V :4 | Refresh_Parameters |  |
| SCV | 103 + 309/Project/Source/SH367309_DataDeal.h | 154 |  |  |  | UINT8 SCV :4 | InitShortCur, Refresh_Parameters |  |
| OCCT | 103 + 309/Project/Source/SH367309_DataDeal.h | 155 |  |  |  | UINT8 OCCT :4 | Refresh_Parameters |  |
| OCCV | 103 + 309/Project/Source/SH367309_DataDeal.h | 156 |  |  |  | UINT8 OCCV :4 | Refresh_Parameters |  |
| OCRT | 103 + 309/Project/Source/SH367309_DataDeal.h | 161 |  |  |  | UINT8 OCRT :2 |  |  |
| MOST | 103 + 309/Project/Source/SH367309_DataDeal.h | 162 |  |  |  | UINT8 MOST :2 |  |  |
| CHS | 103 + 309/Project/Source/SH367309_DataDeal.h | 163 |  |  |  | UINT8 CHS :2 |  |  |
| OTCR | 103 + 309/Project/Source/SH367309_DataDeal.h | 168 |  |  |  | UINT8 OTCR |  |  |
| UTC | 103 + 309/Project/Source/SH367309_DataDeal.h | 169 |  |  |  | UINT8 UTC |  | Fault_ChangeToMCU |
| UTCR | 103 + 309/Project/Source/SH367309_DataDeal.h | 170 |  |  |  | UINT8 UTCR |  |  |
| OTD | 103 + 309/Project/Source/SH367309_DataDeal.h | 171 |  |  |  | UINT8 OTD |  | Fault_ChangeToMCU |
| OTDR | 103 + 309/Project/Source/SH367309_DataDeal.h | 172 |  |  |  | UINT8 OTDR |  |  |
| UTD | 103 + 309/Project/Source/SH367309_DataDeal.h | 173 |  |  |  | UINT8 UTD |  | Fault_ChangeToMCU |
| UTDR | 103 + 309/Project/Source/SH367309_DataDeal.h | 174 |  |  |  | UINT8 UTDR |  |  |
| TR | 103 + 309/Project/Source/SH367309_DataDeal.h | 175 |  |  |  | UINT8 TR | Refresh_Parameters |  |
| m02H_03H | 103 + 309/Project/Source/SH367309_DataDeal.h | 181 |  |  |  | BYTE_02H_03H_TypeDef m02H_03H | Refresh_Parameters |  |
| m04H_05H | 103 + 309/Project/Source/SH367309_DataDeal.h | 182 |  |  |  | BYTE_04H_05H_TypeDef m04H_05H | Refresh_Parameters |  |
| m06H_07H | 103 + 309/Project/Source/SH367309_DataDeal.h | 183 |  |  |  | BYTE_06H_07H_TypeDef m06H_07H | Refresh_Parameters |  |
| m08H_09H | 103 + 309/Project/Source/SH367309_DataDeal.h | 184 |  |  |  | BYTE_08H_09H_TypeDef m08H_09H | Refresh_Parameters |  |
| m0AH_0BH | 103 + 309/Project/Source/SH367309_DataDeal.h | 185 |  |  |  | BYTE_0AH_0BH_TypeDef m0AH_0BH |  |  |
| m0CH_0DH | 103 + 309/Project/Source/SH367309_DataDeal.h | 186 |  |  |  | BYTE_0CH_0DH_TypeDef m0CH_0DH | Refresh_Parameters |  |
| m0EH_0FH | 103 + 309/Project/Source/SH367309_DataDeal.h | 187 |  |  |  | BYTE_0EH_0FH_TypeDef m0EH_0FH | InitShortCur, Refresh_Parameters |  |
| m10H | 103 + 309/Project/Source/SH367309_DataDeal.h | 188 |  |  |  | BYTE_10H_TypeDef m10H |  |  |
| m11H_19H | 103 + 309/Project/Source/SH367309_DataDeal.h | 189 |  |  |  | BYTE_11H_19H_TypeDef m11H_19H |  | Refresh_Parameters |
| defaultValue | 103 + 309/Project/Source/SH367309_DataDeal.h | 197 |  |  |  | UINT16 defaultValue |  | EEPROM_ResetData_AFE_ParametersToDefault |
| maxValue | 103 + 309/Project/Source/SH367309_DataDeal.h | 198 |  |  |  | UINT16 maxValue |  | ReadEEPROM_AFE_Parameters |
| minValue | 103 + 309/Project/Source/SH367309_DataDeal.h | 199 |  |  |  | UINT16 minValue |  | ReadEEPROM_AFE_Parameters |
| u16VcellOvp_Rcv | 103 + 309/Project/Source/SH367309_DataDeal.h | 204 |  |  |  | AFE_Value_Typedef u16VcellOvp_Rcv |  | Refresh_Parameters |
| u16VcellOvp_Filter | 103 + 309/Project/Source/SH367309_DataDeal.h | 205 |  |  |  | AFE_Value_Typedef u16VcellOvp_Filter |  | Refresh_Parameters |
| u16VcellUvp | 103 + 309/Project/Source/SH367309_DataDeal.h | 206 |  |  |  | AFE_Value_Typedef u16VcellUvp |  | Refresh_Parameters |
| u16VcellUvp_Rcv | 103 + 309/Project/Source/SH367309_DataDeal.h | 208 |  |  |  | AFE_Value_Typedef u16VcellUvp_Rcv |  | Refresh_Parameters |
| u16VcellUvp_Filter | 103 + 309/Project/Source/SH367309_DataDeal.h | 209 |  |  |  | AFE_Value_Typedef u16VcellUvp_Filter |  | Refresh_Parameters |
| u16IchgOcp_First | 103 + 309/Project/Source/SH367309_DataDeal.h | 210 |  |  |  | AFE_Value_Typedef u16IchgOcp_First |  |  |
| u16IchgOcp_Filter_First | 103 + 309/Project/Source/SH367309_DataDeal.h | 212 |  |  |  | AFE_Value_Typedef u16IchgOcp_Filter_First |  |  |
| u16IchgOcp_Second | 103 + 309/Project/Source/SH367309_DataDeal.h | 213 |  |  |  | AFE_Value_Typedef u16IchgOcp_Second |  | Refresh_Parameters |
| u16IchgOcp_Filter_Second | 103 + 309/Project/Source/SH367309_DataDeal.h | 215 |  |  |  | AFE_Value_Typedef u16IchgOcp_Filter_Second |  | Refresh_Parameters |
| u16IdsgOcp_First | 103 + 309/Project/Source/SH367309_DataDeal.h | 216 |  |  |  | AFE_Value_Typedef u16IdsgOcp_First |  | Refresh_Parameters |
| u16IdsgOcp_Filter_First | 103 + 309/Project/Source/SH367309_DataDeal.h | 218 |  |  |  | AFE_Value_Typedef u16IdsgOcp_Filter_First |  | Refresh_Parameters |
| u16IdsgOcp_Second | 103 + 309/Project/Source/SH367309_DataDeal.h | 219 |  |  |  | AFE_Value_Typedef u16IdsgOcp_Second |  | Refresh_Parameters |
| u16IdsgOcp_Filter_Second | 103 + 309/Project/Source/SH367309_DataDeal.h | 221 |  |  |  | AFE_Value_Typedef u16IdsgOcp_Filter_Second |  | Refresh_Parameters |
| u16TChgOTp | 103 + 309/Project/Source/SH367309_DataDeal.h | 222 |  |  |  | AFE_Value_Typedef u16TChgOTp |  | Refresh_Parameters |
| u16TChgOTp_Rcv | 103 + 309/Project/Source/SH367309_DataDeal.h | 224 |  |  |  | AFE_Value_Typedef u16TChgOTp_Rcv |  | Refresh_Parameters |
| u16TchgUTp | 103 + 309/Project/Source/SH367309_DataDeal.h | 225 |  |  |  | AFE_Value_Typedef u16TchgUTp |  | Refresh_Parameters |
| u16TchgUTp_Rcv | 103 + 309/Project/Source/SH367309_DataDeal.h | 226 |  |  |  | AFE_Value_Typedef u16TchgUTp_Rcv |  | Refresh_Parameters |
| u16TdischgOTp | 103 + 309/Project/Source/SH367309_DataDeal.h | 227 |  |  |  | AFE_Value_Typedef u16TdischgOTp |  | Refresh_Parameters |
| u16TdischgOTp_Rcv | 103 + 309/Project/Source/SH367309_DataDeal.h | 228 |  |  |  | AFE_Value_Typedef u16TdischgOTp_Rcv |  | Refresh_Parameters |
| u16TdischgUTp | 103 + 309/Project/Source/SH367309_DataDeal.h | 229 |  |  |  | AFE_Value_Typedef u16TdischgUTp |  | Refresh_Parameters |
| u16TdischgUTp_Rcv | 103 + 309/Project/Source/SH367309_DataDeal.h | 230 |  |  |  | AFE_Value_Typedef u16TdischgUTp_Rcv |  | Refresh_Parameters |
| u16CBC_Cur_DSG | 103 + 309/Project/Source/SH367309_DataDeal.h | 231 |  |  |  | AFE_Value_Typedef u16CBC_Cur_DSG | InitShortCur | Refresh_Parameters |
| u16CBC_DelayT | 103 + 309/Project/Source/SH367309_DataDeal.h | 232 |  |  |  | AFE_Value_Typedef u16CBC_DelayT | InitShortCur, SH367309_SC_DelayT_Set | Refresh_Parameters |
| AFE_PARAM_WRITE_Flag | 103 + 309/Project/Source/SH367309_DataDeal.h | 245 |  | extern |  | extern int AFE_PARAM_WRITE_Flag |  |  |
| iSheldTemp_10K_NTC | 103 + 309/Project/Source/SH367309_Func.c | 4 |  |  |  | const UINT16 iSheldTemp_10K_NTC[141] = {20375, 19204, 18115, 17100, 16152, 15266, 14437, 13661, 12934, 12251, 11611, 11008, 10442, 9909, 9407, 8935, 8489, 8068, 7672, 7297, 6943, 6608, 6292, 5993, 571... |  | Refresh_Parameters, SH367309Func_DebugWatchBind |
| ucMTPBuffer | 103 + 309/Project/Source/SH367309_Func.c | 22 |  |  |  | UINT8 ucMTPBuffer[26] = { BYTE_00H_SCONF1, BYTE_01H_SCONF2, BYTE_02H_OVT_LDRT_OVH, BYTE_03H_OVL, BYTE_04H_UVT_OVRH, BYTE_05H_OVRL, BYTE_06H_UV, BYTE_07H_UVR, BYTE_08H_BALV, BYTE_09H_PREV, BYTE_0AH_L0V, BYTE_0BH_PFV, B... | Refresh_Parameters | SH367309Func_DebugWatchBind |
| UV | 103 + 309/Project/Source/SH367309_Func.h | 155 |  |  |  | UINT8 UV :1 | Refresh_Parameters | Fault_ChangeToMCU |
| OCD1 | 103 + 309/Project/Source/SH367309_Func.h | 156 |  |  |  | UINT8 OCD1 :1 |  | Fault_ChangeToMCU |
| OCD2 | 103 + 309/Project/Source/SH367309_Func.h | 157 |  |  |  | UINT8 OCD2 :1 |  | Fault_ChangeToMCU |
| OCC | 103 + 309/Project/Source/SH367309_Func.h | 158 |  |  |  | UINT8 OCC :1 |  | Fault_ChangeToMCU |
| SC | 103 + 309/Project/Source/SH367309_Func.h | 160 |  |  |  | UINT8 SC :1 |  | Fault_ChangeToMCU |
| PF | 103 + 309/Project/Source/SH367309_Func.h | 161 |  |  |  | UINT8 PF :1 |  | App_SH367309_Monitor |
| WDT | 103 + 309/Project/Source/SH367309_Func.h | 162 |  |  |  | UINT8 WDT :1 |  | App_SH367309_Monitor |
| OTC | 103 + 309/Project/Source/SH367309_Func.h | 171 |  |  |  | UINT8 OTC :1 |  | Fault_ChangeToMCU |
| UTD | 103 + 309/Project/Source/SH367309_Func.h | 172 |  |  |  | UINT8 UTD :1 |  | Fault_ChangeToMCU |
| OTD | 103 + 309/Project/Source/SH367309_Func.h | 173 |  |  |  | UINT8 OTD :1 |  | Fault_ChangeToMCU |
| Rcv | 103 + 309/Project/Source/SH367309_Func.h | 174 |  |  |  | UINT8 Rcv :4 |  |  |
| CHG_FET | 103 + 309/Project/Source/SH367309_Func.h | 185 |  |  |  | UINT8 CHG_FET :1 |  | App_SH367309_Monitor, RtcSleep_AfePortHasAfeWake, SystemDebug_Snapshot |
| PCHG_FET | 103 + 309/Project/Source/SH367309_Func.h | 186 |  |  |  | UINT8 PCHG_FET :1 |  |  |
| L0V | 103 + 309/Project/Source/SH367309_Func.h | 187 |  |  |  | UINT8 L0V :1 |  | App_SH367309_Monitor |
| EEPR_WR | 103 + 309/Project/Source/SH367309_Func.h | 188 |  |  |  | UINT8 EEPR_WR :1 |  | App_SH367309_Monitor |
| RCV | 103 + 309/Project/Source/SH367309_Func.h | 190 |  |  |  | UINT8 RCV :1 |  |  |
| DSGING | 103 + 309/Project/Source/SH367309_Func.h | 191 |  |  |  | UINT8 DSGING :1 |  |  |
| CHGING | 103 + 309/Project/Source/SH367309_Func.h | 192 |  |  |  | UINT8 CHGING :1 |  |  |
| SLEEP | 103 + 309/Project/Source/SH367309_Func.h | 201 |  |  |  | UINT8 SLEEP :1 | AFE_Sleep |  |
| ENWDT | 103 + 309/Project/Source/SH367309_Func.h | 202 |  |  |  | UINT8 ENWDT :1 |  |  |
| CADCON | 103 + 309/Project/Source/SH367309_Func.h | 203 |  |  |  | UINT8 CADCON :1 | MosStartup_WriteMosState, SH367309_Enable_AFE_Wdt_Cadc_Drivers |  |
| CHGMOS | 103 + 309/Project/Source/SH367309_Func.h | 204 |  |  |  | UINT8 CHGMOS :1 | MosStartup_WriteMosState, SH367309_DriverMos_Ctrl, SH367309_Enable_AFE_Wdt_Cadc_Drivers |  |
| DSGMOS | 103 + 309/Project/Source/SH367309_Func.h | 206 |  |  |  | UINT8 DSGMOS :1 | MosStartup_WriteMosState, SH367309_DriverMos_Ctrl, SH367309_Enable_AFE_Wdt_Cadc_Drivers |  |
| PCHMOS | 103 + 309/Project/Source/SH367309_Func.h | 207 |  |  |  | UINT8 PCHMOS :1 | SH367309_DriverMos_Ctrl |  |
| OCRC | 103 + 309/Project/Source/SH367309_Func.h | 208 |  |  |  | UINT8 OCRC :1 |  |  |
| u8_MTP_SCV_SCT | 103 + 309/Project/Source/SH367309_Func.h | 215 |  |  |  | UINT8 u8_MTP_SCV_SCT | SH367309_SC_DelayT_Set |  |
| REG_MTP_CONF | 103 + 309/Project/Source/SH367309_Func.h | 216 |  |  |  | MTP_REG_CONF REG_MTP_CONF | AFE_IDLE, AFE_Sleep, MosStartup_WriteMosState, SH367309_DriverMos_Ctrl, SH367309_Enable_AFE_Wdt_Cadc_Drivers |  |
| u8_MTP_BALANCEH | 103 + 309/Project/Source/SH367309_Func.h | 218 |  |  |  | UINT8 u8_MTP_BALANCEH |  | App_SH367309_Monitor, RtcSleep_AfePortHasAfeWake |
| u8_MTP_BALANCEL | 103 + 309/Project/Source/SH367309_Func.h | 219 |  |  |  | UINT8 u8_MTP_BALANCEL |  |  |
| REG_BSTATUS1 | 103 + 309/Project/Source/SH367309_Func.h | 220 |  |  |  | MTP_REG_BSTATUS1 REG_BSTATUS1 |  | AFE_CheckStatus, App_SH367309_Monitor, Fault_ChangeToMCU, SystemDebug_Snapshot |
| REG_BSTATUS2 | 103 + 309/Project/Source/SH367309_Func.h | 221 |  |  |  | MTP_REG_BSTATUS2 REG_BSTATUS2 |  | Fault_ChangeToMCU |
| REG_BSTATUS3 | 103 + 309/Project/Source/SH367309_Func.h | 222 |  |  |  | MTP_REG_BSTATUS3 REG_BSTATUS3 |  | App_SH367309_Monitor, LedBar_IsDischargeMosOpen, RtcSleep_AfePortHasAfeWake, SystemDebug_Snapshot |
| TR | 103 + 309/Project/Source/SH367309_Func.h | 223 |  |  |  | UINT8 TR | Refresh_Parameters |  |
| TR_ResRef | 103 + 309/Project/Source/SH367309_Func.h | 224 |  |  |  | UINT16 TR_ResRef | Refresh_Parameters | UpdateVoltageFromBqMaximo |
| SH367309_Reg_Store | 103 + 309/Project/Source/SH367309_Func.h | 239 |  | extern |  | extern SH367309_REG_STORE SH367309_Reg_Store |  |  |
| gu16_CommuErrCnt_SCI1 | 103 + 309/Project/Source/Sci_Upper.c | 5 | static |  |  | static UINT16 gu16_CommuErrCnt_SCI1 = 0 |  | Sci_DebugWatchBind |
| gu8_TxEnable_SCI1 | 103 + 309/Project/Source/Sci_Upper.c | 6 | static |  |  | static UINT8 gu8_TxEnable_SCI1 = 0 |  | Sci_DebugWatchBind |
| gu8_TxFinishFlag_SCI1 | 103 + 309/Project/Source/Sci_Upper.c | 7 | static |  |  | static UINT8 gu8_TxFinishFlag_SCI1 = 0 |  | Sci_DebugWatchBind |
| gu16_CommuErrCnt_SCI2 | 103 + 309/Project/Source/Sci_Upper.c | 11 | static |  |  | static UINT16 gu16_CommuErrCnt_SCI2 = 0 |  | Sci_DebugWatchBind |
| gu8_TxEnable_SCI2 | 103 + 309/Project/Source/Sci_Upper.c | 12 | static |  |  | static UINT8 gu8_TxEnable_SCI2 = 0 |  | Sci_DebugWatchBind |
| gu8_TxFinishFlag_SCI2 | 103 + 309/Project/Source/Sci_Upper.c | 13 | static |  |  | static UINT8 gu8_TxFinishFlag_SCI2 = 0 |  | Sci_DebugWatchBind |
| gu16_CommuErrCnt_SCI3 | 103 + 309/Project/Source/Sci_Upper.c | 18 | static |  |  | static UINT16 gu16_CommuErrCnt_SCI3 = 0 |  | Sci_DebugWatchBind |
| gu8_TxEnable_SCI3 | 103 + 309/Project/Source/Sci_Upper.c | 19 | static |  |  | static UINT8 gu8_TxEnable_SCI3 = 0 |  | Sci_DebugWatchBind |
| gu8_TxFinishFlag_SCI3 | 103 + 309/Project/Source/Sci_Upper.c | 20 | static |  |  | static UINT8 gu8_TxFinishFlag_SCI3 = 0 |  | Sci_DebugWatchBind |
| u8FlashUpdateFlag | 103 + 309/Project/Source/Sci_Upper.c | 26 |  |  |  | UINT8 u8FlashUpdateFlag = 0 | App_FlashUpdate, Sci_PortFinishTx, feidao_can_service_enter_iap_delay | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot |
| u8FlashUpdateE2PROM | 103 + 309/Project/Source/Sci_Upper.c | 27 |  |  |  | UINT8 u8FlashUpdateE2PROM = 0 | Sci_PortFinishTx, Sci_WrRegs_0x10_FlashConnect | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot |
| pfRxFeed | 103 + 309/Project/Source/Sci_Upper.c | 43 |  |  |  | SCI_PROTOCOL_RX_FEED_FN pfRxFeed |  | Sci_PortIRQHandler |
| pfProcessFrame | 103 + 309/Project/Source/Sci_Upper.c | 44 |  |  |  | SCI_PROTOCOL_PROCESS_FN pfProcessFrame |  | Sci_PortService |
| pfGetTxBuffer | 103 + 309/Project/Source/Sci_Upper.c | 45 |  |  |  | SCI_PROTOCOL_TX_BUFFER_FN pfGetTxBuffer |  | Sci_PortService |
| pfGetTxLength | 103 + 309/Project/Source/Sci_Upper.c | 46 |  |  |  | SCI_PROTOCOL_TX_LENGTH_FN pfGetTxLength |  | Sci_PortService |
| pfIsBusy | 103 + 309/Project/Source/Sci_Upper.c | 47 |  |  |  | SCI_PROTOCOL_IS_BUSY_FN pfIsBusy |  | Sci_PortIsBusy |
| pfOnRxIdle | 103 + 309/Project/Source/Sci_Upper.c | 48 |  |  |  | SCI_PROTOCOL_RX_IDLE_FN pfOnRxIdle |  | Sci_PortIRQHandler |
| pfOnTxComplete | 103 + 309/Project/Source/Sci_Upper.c | 49 |  |  |  | SCI_PROTOCOL_TX_COMPLETE_FN pfOnTxComplete |  | Sci_PortFinishTx |
| pvProtocolCtx | 103 + 309/Project/Source/Sci_Upper.c | 55 |  |  |  | void *pvProtocolCtx |  | Sci_InitCommonPort, Sci_ModbusGetTxBuffer, Sci_ModbusGetTxLength, Sci_ModbusIsBusy, Sci_ModbusOnRxIdle, Sci_ModbusOnTxComplete, Sci_ModbusProcessFrame, Sci_ModbusProtocolFeed, Sci_ModbusResetProtocol, Sci_PortAbortTransfer |
| pstProtocolOps | 103 + 309/Project/Source/Sci_Upper.c | 56 |  |  |  | const struct SCI_PROTOCOL_OPS *pstProtocolOps |  | Sci_InitCommonPort, Sci_PortAbortTransfer, Sci_PortFinishTx, Sci_PortIRQHandler, Sci_PortIsBusy, Sci_PortService |
| pu16ErrorCounter | 103 + 309/Project/Source/Sci_Upper.c | 57 |  |  | volatile | volatile UINT16 *pu16ErrorCounter | Sci_InitCommonPort | Sci_PortHandleError |
| pu8TxEnableFlag | 103 + 309/Project/Source/Sci_Upper.c | 58 |  |  | volatile | volatile UINT8 *pu8TxEnableFlag | Sci_InitCommonPort, Sci_PortAbortTransfer, Sci_PortFinishTx, Sci_PortStartTx |  |
| pu8TxFinishFlag | 103 + 309/Project/Source/Sci_Upper.c | 59 |  |  | volatile | volatile UINT8 *pu8TxFinishFlag | Sci_InitCommonPort, Sci_PortAbortTransfer, Sci_PortFinishTx, Sci_PortStartTx |  |
| pu8TxBuffer | 103 + 309/Project/Source/Sci_Upper.c | 60 |  |  |  | UINT8 *pu8TxBuffer | Sci_PortAbortTransfer, Sci_PortArmReceiver, Sci_PortIRQHandler, Sci_PortService |  |
| u16TxIndex | 103 + 309/Project/Source/Sci_Upper.c | 61 |  |  |  | UINT16 u16TxIndex | Sci_PortAbortTransfer, Sci_PortArmReceiver, Sci_PortIRQHandler, Sci_PortStartTx |  |
| u16TxLength | 103 + 309/Project/Source/Sci_Upper.c | 62 |  |  |  | UINT16 u16TxLength | Sci_PortAbortTransfer, Sci_PortArmReceiver, Sci_PortIRQHandler, Sci_PortService | Sci_PortIsBusy |
| u8FramePending | 103 + 309/Project/Source/Sci_Upper.c | 63 |  |  |  | UINT8 u8FramePending | Sci_PortAbortTransfer, Sci_PortArmReceiver, Sci_PortIRQHandler, Sci_PortService | Sci_PortIsBusy |
| g_stSciModbusProtocolOps | 103 + 309/Project/Source/Sci_Upper.c | 96 | static |  |  | static const struct SCI_PROTOCOL_OPS g_stSciModbusProtocolOps = { Sci_ModbusResetProtocol, Sci_ModbusProtocolFeed, Sci_ModbusProcessFrame, Sci_ModbusGetTxBuffer, Sci_ModbusGetTxLength, Sci_ModbusIsBusy, Sci_Modbus... |  |  |
| g_stSciPort1 | 103 + 309/Project/Source/Sci_Upper.c | 106 | static |  |  | static struct SCI_PORT_RUNTIME g_stSciPort1 = { USART1, &g_stCurrentMsgPtr_SCI1, &g_stSciModbusProtocolOps, &gu16_CommuErrCnt_SCI1, &gu8_TxEnable_SCI1, &gu8_TxFinishFlag_SCI1, 0, 0, 0, 0} |  | App_CommonUpper, InitSCI1_CommonUpper, Sci1_CommonUpper_IRQHandler, Sci_DebugWatchBind, Sci_IsAnyPortBusy |
| u16Soh | 103 + 309/Project/Source/Sci_Upper.h | 24 |  |  |  | UINT16 u16Soh | SOC_PublishReportData | CanFeidao_SendSoh5000ms, feidao_can_handle_app_cmd_data, host_emit_row |
| u16CapacityNow | 103 + 309/Project/Source/Sci_Upper.h | 25 |  |  |  | UINT16 u16CapacityNow | SOC_PublishReportData | CanFeidao_SendCap5000ms, CanFeidao_SendStatus5000ms, host_emit_row |
| u16CapacityFull | 103 + 309/Project/Source/Sci_Upper.h | 26 |  |  |  | UINT16 u16CapacityFull | SOC_PublishReportData |  |
| u16CapacityFactory | 103 + 309/Project/Source/Sci_Upper.h | 27 |  |  |  | UINT16 u16CapacityFactory | SOC_PublishReportData | CanFeidao_SendCap5000ms, CanFeidao_SendFactoryTime5000ms, CanFeidao_SendStatus5000ms |
| u16Cycle_times | 103 + 309/Project/Source/Sci_Upper.h | 28 |  |  |  | UINT16 u16Cycle_times | SOC_PublishReportData | CanFeidao_SendSoh5000ms |
| b1CellUvp | 103 + 309/Project/Source/Sci_Upper.h | 34 |  |  |  | UINT8 b1CellUvp :1 | Fault_ChangeToMCU | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1BatOvp | 103 + 309/Project/Source/Sci_Upper.h | 35 |  |  |  | UINT8 b1BatOvp :1 |  | App_LogRecord, CanFeidao_SendSoc1000ms, CanFeidao_SendStatus5000ms |
| b1BatUvp | 103 + 309/Project/Source/Sci_Upper.h | 36 |  |  |  | UINT8 b1BatUvp :1 |  | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1IchgOcp | 103 + 309/Project/Source/Sci_Upper.h | 37 |  |  |  | UINT8 b1IchgOcp :1 | Fault_ChangeToMCU | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1IdischgOcp | 103 + 309/Project/Source/Sci_Upper.h | 39 |  |  |  | UINT8 b1IdischgOcp :1 | Fault_ChangeToMCU | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1CellChgOtp | 103 + 309/Project/Source/Sci_Upper.h | 40 |  |  |  | UINT8 b1CellChgOtp :1 | Fault_ChangeToMCU | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1CellDischgOtp | 103 + 309/Project/Source/Sci_Upper.h | 41 |  |  |  | UINT8 b1CellDischgOtp :1 | Fault_ChangeToMCU | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1CellChgUtp | 103 + 309/Project/Source/Sci_Upper.h | 42 |  |  |  | UINT8 b1CellChgUtp :1 | Fault_ChangeToMCU | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1CellDischgUtp | 103 + 309/Project/Source/Sci_Upper.h | 44 |  |  |  | UINT8 b1CellDischgUtp :1 | Fault_ChangeToMCU | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1VcellDeltaBig | 103 + 309/Project/Source/Sci_Upper.h | 45 |  |  |  | UINT8 b1VcellDeltaBig :1 |  | App_LogRecord, CanFeidao_SendStatus5000ms |
| b1TempDeltaBig | 103 + 309/Project/Source/Sci_Upper.h | 46 |  |  |  | UINT8 b1TempDeltaBig :1 |  |  |
| b1SocLow | 103 + 309/Project/Source/Sci_Upper.h | 47 |  |  |  | UINT8 b1SocLow :1 |  |  |
| b1TmosOtp | 103 + 309/Project/Source/Sci_Upper.h | 49 |  |  |  | UINT8 b1TmosOtp :1 |  |  |
| b1Rcved1 | 103 + 309/Project/Source/Sci_Upper.h | 50 |  |  |  | UINT8 b1Rcved1 :1 |  |  |
| b1Rcved2 | 103 + 309/Project/Source/Sci_Upper.h | 51 |  |  |  | UINT8 b1Rcved2 :1 |  |  |
| u16VCellMax | 103 + 309/Project/Source/Sci_Upper.h | 64 |  |  |  | UINT16 u16VCellMax | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, RtcSleep_PortApplySocRtcRest, SystemDebug_Snapshot, new_todo_logi, soc_param_lib_init |
| u16VCellMin | 103 + 309/Project/Source/Sci_Upper.h | 65 |  |  |  | UINT16 u16VCellMin | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, RtcSleep_PortApplySocRtcRest, RtcSleep_PortGetCellMinMv, RtcSleep_PortIsEmergencyWakeVoltage, SystemDebug_Snapshot, new_todo_logi, soc_param_lib_init |
| u16VCellMaxPosition | 103 + 309/Project/Source/Sci_Upper.h | 66 |  |  |  | UINT16 u16VCellMaxPosition | DataLoad_CellVoltMaxMinFind |  |
| u16VCellMinPosition | 103 + 309/Project/Source/Sci_Upper.h | 67 |  |  |  | UINT16 u16VCellMinPosition | DataLoad_CellVoltMaxMinFind |  |
| u16VCellDelta | 103 + 309/Project/Source/Sci_Upper.h | 68 |  |  |  | UINT16 u16VCellDelta | DataLoad_CellVoltMaxMinFind, host_tick |  |
| u16VCellTotle | 103 + 309/Project/Source/Sci_Upper.h | 69 |  |  |  | UINT16 u16VCellTotle | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick | CanFeidao_SendCap5000ms, CanFeidao_SendVoltageCurrent1000ms, SOC_GetPackVoltageForTypeCMv, SystemDebug_Snapshot |
| u16Temperature | 103 + 309/Project/Source/Sci_Upper.h | 70 |  |  |  | UINT16 u16Temperature[TEMP_NUM] | DataLoad_Temperature, DataLoad_TemperatureMaxMinFind | new_todo_logi |
| u16TempMax | 103 + 309/Project/Source/Sci_Upper.h | 71 |  |  |  | UINT16 u16TempMax | DataLoad_TemperatureMaxMinFind | CanFeidao_SendSoc1000ms |
| u16TempMin | 103 + 309/Project/Source/Sci_Upper.h | 72 |  |  |  | UINT16 u16TempMin | DataLoad_TemperatureMaxMinFind |  |
| u16Ichg | 103 + 309/Project/Source/Sci_Upper.h | 73 |  |  |  | UINT16 u16Ichg | DataLoad_Current, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, CanFeidao_SendSoc1000ms, CanFeidao_SendStatus5000ms, CanFeidao_SendVoltageCurrent1000ms, RtcSleep_AfePortHasCurrentWake, RtcSleep_PortGetChargeCurrentMa, new_todo_logi, soc_param_lib_init |
| u16IDischg | 103 + 309/Project/Source/Sci_Upper.h | 74 |  |  |  | UINT16 u16IDischg | DataLoad_Current, host_init_with_voltage, host_tick | App_SOC, CanFeidao_SendStatus5000ms, CanFeidao_SendVoltageCurrent1000ms, RtcSleep_AfePortHasCurrentWake, RtcSleep_PortGetDischargeCurrentMa, soc_param_lib_init |
| u16BalanceFlag1 | 103 + 309/Project/Source/Sci_Upper.h | 80 |  |  |  | UINT16 u16BalanceFlag1 |  |  |
| u16BalanceFlag2 | 103 + 309/Project/Source/Sci_Upper.h | 81 |  |  |  | UINT16 u16BalanceFlag2 |  |  |
| csr | 103 + 309/Project/Source/Sci_Upper.h | 108 |  |  |  | UINT8 csr | Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_DataInit, Sci_ModbusOnRxIdle, Sci_ModbusProtocolFeed, Sci_ModbusResetMessage, SystemDebug_SnapshotMcuResources | Sci_ModbusIsBusy |
| u16RdRegStartAddr | 103 + 309/Project/Source/Sci_Upper.h | 109 |  |  |  | UINT16 u16RdRegStartAddr | Sci_ACK_0x03_ReadRegs_LCD, Sci_Deal_ReadRegs_0x03, Sci_ModbusResetMessage | Sci_ACK_0x03 |
| u16RdRegStartAddrActure | 103 + 309/Project/Source/Sci_Upper.h | 110 |  |  |  | UINT16 u16RdRegStartAddrActure | Sci_Deal_ReadRegs_0x03, Sci_ModbusResetMessage | Sci_ACK_0x03 |
| u16RdRegByteNum | 103 + 309/Project/Source/Sci_Upper.h | 111 |  |  |  | UINT8 u16RdRegByteNum | CRC_verify, Sci_Deal_ReadRegs_0x03, Sci_ModbusProcessFrame, Sci_ModbusResetMessage | Sci_ACK_0x03 |
| AckLenth | 103 + 309/Project/Source/Sci_Upper.h | 112 |  |  |  | UINT8 AckLenth | Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_ModbusResetMessage | Sci_HostReadWords, Sci_ModbusGetTxLength |
| AckType | 103 + 309/Project/Source/Sci_Upper.h | 113 |  |  |  | UINT8 AckType | CRC_verify, Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_Deal_ReadRegs_0x03, Sci_Deal_WrReg_0x06, Sci_Deal_WrRegs_0x10, Sci_HostReadWords, Sci_HostWriteWords, Sci_ModbusProcessFrame, Sci_ModbusResetMessage |  |
| ErrorType | 103 + 309/Project/Source/Sci_Upper.h | 114 |  |  |  | UINT8 ErrorType | CRC_verify, Sci_Deal_ReadRegs_0x03, Sci_Deal_WrReg_0x06, Sci_Deal_WrRegs_0x10, Sci_HostReadWords, Sci_HostWriteWords, Sci_ModbusProcessFrame, Sci_ModbusResetMessage, Sci_SetWrError, Sci_WrReg_0x06_BMS_FunctionOFF | Sci_ACK_0x03, Sci_ACK_0x06_0x10 |
| u16Buffer | 103 + 309/Project/Source/Sci_Upper.h | 115 |  |  |  | UINT8 u16Buffer[RS485_MAX_BUFFER_SIZE] | Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_DataInit, Sci_HostReadWords, Sci_HostWriteWords, Sci_ModbusProtocolFeed, Sci_ModbusResetMessage, Sci_WrRegsByteCountValid | CRC_verify, Sci_CopyProductIdBytes, Sci_Deal_ReadRegs_0x03, Sci_Deal_WrReg_0x06, Sci_Deal_WrRegs_0x10, Sci_GetWrRegNum, Sci_GetWrValue, Sci_ModbusGetTxBuffer, Sci_WrReg_0x06_BMS_FunctionOFF, Sci_WrReg_0x06_BMS_FunctionON |
| u8FlashUpdateFlag | 103 + 309/Project/Source/Sci_Upper.h | 478 |  | extern |  | extern UINT8 u8FlashUpdateFlag | App_FlashUpdate, Sci_PortFinishTx, feidao_can_service_enter_iap_delay | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot |
| u8FlashUpdateE2PROM | 103 + 309/Project/Source/Sci_Upper.h | 481 |  | extern |  | extern UINT8 u8FlashUpdateE2PROM | Sci_PortFinishTx, Sci_WrRegs_0x10_FlashConnect | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot |
| g_stCellInfoReport | 103 + 309/Project/Source/Sci_Upper.h | 482 |  | extern |  | extern struct stCell_Info g_stCellInfoReport |  |  |
| s_bq_afe_sct | 103 + 309/Project/Source/ShortFunc.c | 4 | static |  |  | static const UINT16 s_bq_afe_sct[10] = {50, 100, 200, 400, 400, 400, 400, 400, 400, 400} |  | InitShortCur |
| g_u16ShAfeSctTable | 103 + 309/Project/Source/ShortFunc.c | 23 |  | extern |  | extern const UINT16 g_u16ShAfeSctTable[16] |  | InitShortCur, Refresh_Parameters, SH367309Data_DebugWatchBind |
| boot_sleep | 103 + 309/Project/Source/SleepDeal.c | 8 |  |  |  | UINT8 boot_sleep | IsSleepStartUp | SleepDeal_IsBootFromSleepStartup |
| chg_wake | 103 + 309/Project/Source/SleepDeal.c | 9 |  |  |  | UINT8 chg_wake | IsSleepStartUp, SleepDeal_IsBootFromSleepChargerWakeup, SleepDeal_MarkBootFromSleepChargerWakeup |  |
| reserved | 103 + 309/Project/Source/SleepDeal.c | 10 |  |  |  | UINT8 reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| s_sleep | 103 + 309/Project/Source/SleepDeal.c | 12 | static |  |  | static SLEEP_RUNTIME s_sleep | IsSleepStartUp, SleepDeal_IsBootFromSleepChargerWakeup, SleepDeal_MarkBootFromSleepChargerWakeup | SleepDeal_DebugWatchBind, SleepDeal_GetExternalCommCounter, SleepDeal_IsBootFromSleepStartup, SleepDeal_RecordExternalComm |
| u16SocNow | 103 + 309/Project/Source/SocEnhance.c | 15 |  |  |  | UINT16 u16SocNow | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_load_or_default, soc_save | host_internal_soc |
| u16DsgSocInt | 103 + 309/Project/Source/SocEnhance.c | 16 |  |  |  | UINT16 u16DsgSocInt | StorageFlash_LoadSocData, host_set_snapshot, soc_save | soc_load_or_default |
| u16MaxErrorPercent | 103 + 309/Project/Source/SocEnhance.c | 17 |  |  |  | UINT16 u16MaxErrorPercent | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_save |  |
| u32CycleTimes | 103 + 309/Project/Source/SocEnhance.c | 18 |  |  |  | UINT32 u32CycleTimes | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_save | soc_load_or_default |
| u32CapNow | 103 + 309/Project/Source/SocEnhance.c | 19 |  |  |  | UINT32 u32CapNow | SOC_ResetStoredSnapshotToDefault, host_set_snapshot, soc_save | soc_load_or_default |
| u32CapFull | 103 + 309/Project/Source/SocEnhance.c | 20 |  |  |  | UINT32 u32CapFull | SOC_ResetStoredSnapshotToDefault, host_set_snapshot, soc_save |  |
| u32LearnPassedAs10 | 103 + 309/Project/Source/SocEnhance.c | 21 |  |  |  | UINT32 u32LearnPassedAs10 | host_set_snapshot, soc_save | soc_load_or_default |
| u16LearnAnchorSoc | 103 + 309/Project/Source/SocEnhance.c | 22 |  |  |  | UINT16 u16LearnAnchorSoc |  |  |
| u16LearnState | 103 + 309/Project/Source/SocEnhance.c | 23 |  |  |  | UINT16 u16LearnState |  |  |
| u16Flags | 103 + 309/Project/Source/SocEnhance.c | 24 |  |  |  | UINT16 u16Flags | host_set_snapshot, soc_save | soc_load_or_default, test_rebound_flag_clears_when_holdoff_expires |
| u16Reserved | 103 + 309/Project/Source/SocEnhance.c | 25 |  |  |  | UINT16 u16Reserved[4] |  |  |
| cap_full_as10 | 103 + 309/Project/Source/SocEnhance.c | 89 |  |  |  | UINT32 cap_full_as10 | soc_from_cap, soc_refresh_capacity_base, soc_update_save_mark | soc_export_public_fields, soc_integrate, soc_load_or_default, soc_save, soc_save_mark_changed, soc_set, soc_watch_refresh |
| cap_now_as10 | 103 + 309/Project/Source/SocEnhance.c | 90 |  |  |  | UINT32 cap_now_as10 | soc_apply_discharge_delta, soc_integrate, soc_load_or_default, soc_refresh_capacity_base, soc_set | soc_export_public_fields, soc_from_cap, soc_save, soc_watch_refresh |
| cycle_x100 | 103 + 309/Project/Source/SocEnhance.c | 91 |  |  |  | UINT32 cycle_x100 | SOC_ResetStoredSnapshotToDefault, soc_handle_command, soc_load_or_default, soc_param_lib_init, soc_update_save_mark | soc_add_discharge, soc_export_public_fields, soc_refresh_capacity_base, soc_save, soc_save_mark_changed, soc_soh_from_cycle, soc_watch_refresh |
| dsg_acc_as10 | 103 + 309/Project/Source/SocEnhance.c | 92 |  |  |  | UINT32 dsg_acc_as10 | soc_add_discharge, soc_handle_command, soc_load_or_default | soc_save, soc_watch_refresh |
| rem_mams | 103 + 309/Project/Source/SocEnhance.c | 93 |  |  |  | UINT32 rem_mams | soc_integrate, soc_set |  |
| rest_soc_ticks | 103 + 309/Project/Source/SocEnhance.c | 94 |  |  |  | UINT32 rest_soc_ticks | SOC_GetDebugInternals, soc_reset_rest_confidence | soc_apply_long_rest_down_step, soc_apply_rtc_rest_ocv, soc_update_rest_timer, soc_watch_refresh |
| stable_rest_soc_ticks | 103 + 309/Project/Source/SocEnhance.c | 96 |  |  |  | UINT32 stable_rest_soc_ticks | soc_apply_rtc_rest_ocv, soc_reset_rest_confidence, soc_update_rest_timer | SOC_GetDebugInternals, soc_watch_refresh |
| long_rest_down_soc_ticks | 103 + 309/Project/Source/SocEnhance.c | 97 |  |  |  | UINT32 long_rest_down_soc_ticks | soc_apply_long_rest_down_step, soc_clear_rest_down_target, soc_set_rest_down_target | soc_watch_refresh |
| full_ticks | 103 + 309/Project/Source/SocEnhance.c | 98 |  |  |  | UINT16 full_ticks | SOC_GetDebugInternals, soc_apply_full_empty | DbgPrint_SOC, SystemDebug_Snapshot, soc_watch_refresh |
| empty_ticks | 103 + 309/Project/Source/SocEnhance.c | 99 |  |  |  | UINT16 empty_ticks | SOC_GetDebugInternals, soc_apply_full_empty | DbgPrint_SOC, SystemDebug_Snapshot, soc_watch_refresh |
| display_ticks | 103 + 309/Project/Source/SocEnhance.c | 100 |  |  |  | UINT16 display_ticks | SOC_GetDebugInternals, soc_update_display_soc | DbgPrint_SOC, SystemDebug_Snapshot, soc_watch_refresh |
| sag_hold_ticks | 103 + 309/Project/Source/SocEnhance.c | 101 |  |  |  | UINT16 sag_hold_ticks | soc_load_or_default, soc_update_sag_hold | soc_sag_hold_blocks_calibration, soc_watch_refresh |
| rest_ref_vmin | 103 + 309/Project/Source/SocEnhance.c | 102 |  |  |  | UINT16 rest_ref_vmin | soc_reset_rest_confidence, soc_rest_voltage_stable | soc_apply_rtc_rest_ocv, soc_watch_refresh |
| rest_ref_vmax | 103 + 309/Project/Source/SocEnhance.c | 103 |  |  |  | UINT16 rest_ref_vmax | soc_reset_rest_confidence, soc_rest_voltage_stable | soc_apply_rtc_rest_ocv, soc_watch_refresh |
| snapshot_flags | 103 + 309/Project/Source/SocEnhance.c | 104 |  |  |  | UINT16 snapshot_flags | soc_load_or_default, soc_update_sag_hold, soc_update_save_mark | soc_save, soc_save_mark_changed, soc_watch_refresh |
| soc | 103 + 309/Project/Source/SocEnhance.c | 105 |  |  |  | UINT8 soc | CanFeidao_SendSoc1000ms, CtCan_AppGetStatus, DebugWatch_BindAll, LedBar_LimitSoc, LedBar_LoadSleepSoc, SocEnhance_DebugWatchBind, SystemDebug_Snapshot, host_soc_from_cap, host_true_soc, host_voltage_from_soc | DbgPrint_SOC, DbgPrint_Summary, SOC_ApplyRtcRelaxationCompensation, SOC_RequestSetOnce, SystemDebug_ModuleItem, host_cap_now_from_soc, host_set_snapshot, soc_apply_full_empty, soc_apply_long_rest_down_step, soc_apply_ocv_target_step |
| soh | 103 + 309/Project/Source/SocEnhance.c | 106 |  |  |  | UINT8 soh | CanFeidao_SendSoh5000ms, CtCan_AppGetStatus, SystemDebug_Snapshot, soc_refresh_capacity_base | DbgPrint_SOC, soc_add_discharge, soc_export_public_fields, soc_watch_refresh |
| display_soc | 103 + 309/Project/Source/SocEnhance.c | 107 |  |  |  | UINT8 display_soc | soc_load_or_default, soc_update_display_soc | soc_export_public_fields, soc_watch_refresh |
| rest_down_target | 103 + 309/Project/Source/SocEnhance.c | 108 |  |  |  | UINT8 rest_down_target | soc_apply_long_rest_down_step, soc_clear_rest_down_target, soc_set_rest_down_target | soc_watch_refresh |
| rest_down_valid | 103 + 309/Project/Source/SocEnhance.c | 109 |  |  |  | UINT8 rest_down_valid | soc_clear_rest_down_target, soc_set_rest_down_target | soc_apply_long_rest_down_step, soc_watch_refresh |
| mode | 103 + 309/Project/Source/SocEnhance.c | 110 |  |  |  | UINT8 mode | LP_GetBlockReason, LowPower_Request, SOC_GetDebugInternals, SOC_IntEnhance_Ctrl, SystemDebug_Snapshot, soc_apply_ocv_target_step, soc_empty_current_band, soc_heavy_discharge_active, soc_integrate, soc_integrate_current_ma | Conf_InitGpioMode, DbgPrint_LP, DbgPrint_SOC, DbgPrint_Summary, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, InitAFE1_Sleep, rtc_sleep, soc_apply_full_empty, soc_empty_tail_config |
| display_ready | 103 + 309/Project/Source/SocEnhance.c | 115 |  |  |  | UINT8 display_ready | soc_load_or_default, soc_update_display_soc |  |
| full_anchor | 103 + 309/Project/Source/SocEnhance.c | 116 |  |  |  | UINT8 full_anchor | SOC_GetDebugInternals, soc_apply_discharge_delta, soc_apply_full_empty, soc_set | DbgPrint_SOC, SystemDebug_Snapshot, soc_integrate, soc_watch_refresh |
| cap_full_as10 | 103 + 309/Project/Source/SocEnhance.c | 122 |  |  |  | UINT32 cap_full_as10 | soc_from_cap, soc_refresh_capacity_base, soc_update_save_mark | soc_export_public_fields, soc_integrate, soc_load_or_default, soc_save, soc_save_mark_changed, soc_set, soc_watch_refresh |
| snapshot_flags | 103 + 309/Project/Source/SocEnhance.c | 123 |  |  |  | UINT16 snapshot_flags | soc_load_or_default, soc_update_sag_hold, soc_update_save_mark | soc_save, soc_save_mark_changed, soc_watch_refresh |
| soc | 103 + 309/Project/Source/SocEnhance.c | 124 |  |  |  | UINT8 soc | CanFeidao_SendSoc1000ms, CtCan_AppGetStatus, DebugWatch_BindAll, LedBar_LimitSoc, LedBar_LoadSleepSoc, SocEnhance_DebugWatchBind, SystemDebug_Snapshot, host_soc_from_cap, host_true_soc, host_voltage_from_soc | DbgPrint_SOC, DbgPrint_Summary, SOC_ApplyRtcRelaxationCompensation, SOC_RequestSetOnce, SystemDebug_ModuleItem, host_cap_now_from_soc, host_set_snapshot, soc_apply_full_empty, soc_apply_long_rest_down_step, soc_apply_ocv_target_step |
| target | 103 + 309/Project/Source/SocEnhance.c | 130 |  |  |  | UINT8 target[SOC_EMPTY_BAND_COUNT] | soc_tail_rule_lookup, soc_update_display_soc | soc_apply_ocv_target_step, soc_apply_tail_step, soc_set_rest_down_target, soc_step, soc_watch_set_tail_state |
| ticks | 103 + 309/Project/Source/SocEnhance.c | 131 |  |  |  | UINT16 ticks[SOC_EMPTY_BAND_COUNT] | host_run_scenario, host_run_seconds, soc_tail_rule_lookup, soc_update_display_soc | SysTick_Config, soc_apply_tail_step, soc_watch_set_tail_state |
| ticks | 103 + 309/Project/Source/SocEnhance.c | 137 |  |  |  | UINT16 ticks | host_run_scenario, host_run_seconds, soc_tail_rule_lookup, soc_update_display_soc | SysTick_Config, soc_apply_tail_step, soc_watch_set_tail_state |
| s_soc | 103 + 309/Project/Source/SocEnhance.c | 141 | static |  |  | static SOC_STATE s_soc | SOC_IntEnhance_Ctrl, soc_add_discharge, soc_apply_discharge_delta, soc_apply_full_empty, soc_apply_long_rest_down_step, soc_apply_rtc_rest_ocv, soc_clear_rest_down_target, soc_from_cap, soc_handle_command, soc_integrate | SOC_ApplyRtcRelaxationCompensation, SOC_GetDebugInternals, SocEnhance_DebugWatchBind, soc_apply_ocv_target_step, soc_apply_tail_step, soc_display_target, soc_export_public_fields, soc_full_confirm_seconds, soc_sag_hold_blocks_calibration, soc_save |
| s_saved_soc | 103 + 309/Project/Source/SocEnhance.c | 143 | static |  |  | static SOC_SAVE_MARK s_saved_soc | soc_update_save_mark | SocEnhance_DebugWatchBind, soc_save_mark_changed |
| s_u32SocRtcRestAppliedSeconds | 103 + 309/Project/Source/SocEnhance.c | 144 | static |  |  | static UINT32 s_u32SocRtcRestAppliedSeconds | soc_apply_rtc_rest_ocv, soc_param_lib_init | SocEnhance_DebugWatchBind |
| s_soc_watch_rest_voltage_stable | 103 + 309/Project/Source/SocEnhance.c | 149 | static |  |  | static UINT8 s_soc_watch_rest_voltage_stable | soc_watch_set_rest_voltage_stable | SocEnhance_DebugWatchBind, soc_watch_refresh |
| u16_SOC_CycleT_Ever | 103 + 309/Project/Source/SocEnhance.h | 42 |  |  |  | UINT16 u16_SOC_CycleT_Ever | SOC_LoadConfigData | soc_handle_command, soc_load_or_default, soc_param_lib_init |
| u16_SOC_0_Vol | 103 + 309/Project/Source/SocEnhance.h | 43 |  |  |  | UINT16 u16_SOC_0_Vol | SOC_LoadConfigData | soc_empty_mv |
| u16_SOC_100_Vol | 103 + 309/Project/Source/SocEnhance.h | 44 |  |  |  | UINT16 u16_SOC_100_Vol | SOC_LoadConfigData | soc_full_mv |
| u8_SetSocOnce | 103 + 309/Project/Source/SocEnhance.h | 45 |  |  |  | UINT8 u8_SetSocOnce | SOC_RequestSetOnce, test_set_soc_once_command_saves_snapshot | soc_handle_command |
| u16_VCellMax | 103 + 309/Project/Source/SocEnhance.h | 48 |  |  |  | UINT16 u16_VCellMax | SOC_ApplyRtcRelaxationCompensation, SOC_UpdateSampleData | SystemDebug_Snapshot, soc_cell_delta, soc_full_confirm_seconds, soc_rest_voltage_stable, soc_voltage_valid, soc_watch_refresh |
| u16_VCellMin | 103 + 309/Project/Source/SocEnhance.h | 51 |  |  |  | UINT16 u16_VCellMin | SOC_ApplyRtcRelaxationCompensation, SOC_UpdateSampleData | SystemDebug_Snapshot, soc_cell_delta, soc_full_confirm_seconds, soc_ocv_percent, soc_rest_voltage_stable, soc_sag_hold_blocks_calibration, soc_tail_rule_lookup, soc_update_display_soc, soc_vmin_above_empty_offset, soc_voltage_valid |
| u16_Ichg | 103 + 309/Project/Source/SocEnhance.h | 52 |  |  |  | UINT16 u16_Ichg | SOC_UpdateSampleData | SystemDebug_Snapshot, soc_direction, soc_integrate_current_ma, soc_watch_refresh |
| u16_Idsg | 103 + 309/Project/Source/SocEnhance.h | 53 |  |  |  | UINT16 u16_Idsg | SOC_UpdateSampleData | SystemDebug_Snapshot, soc_direction, soc_empty_current_band, soc_heavy_discharge_active, soc_integrate_current_ma, soc_watch_refresh, test_board_self_consumption_integrates_during_relax |
| u8_SOC | 103 + 309/Project/Source/SocEnhance.h | 54 |  |  |  | UINT8 u8_SOC | soc_export_public_fields | SOC_PublishReportData, SystemDebug_Snapshot |
| u8_SOH | 103 + 309/Project/Source/SocEnhance.h | 57 |  |  |  | UINT8 u8_SOH | soc_export_public_fields | SOC_PublishReportData, SystemDebug_Snapshot |
| u16_CapacityNow | 103 + 309/Project/Source/SocEnhance.h | 58 |  |  |  | UINT16 u16_CapacityNow | soc_export_public_fields | SOC_PublishReportData, SystemDebug_Snapshot, test_board_self_consumption_integrates_during_relax, test_board_self_consumption_works_at_high_non_full_voltage, test_full_voltage_anchor_can_override_self_consumption, test_rtc_sleep_does_not_apply_board_self_consumption |
| u16_CapacityFull | 103 + 309/Project/Source/SocEnhance.h | 59 |  |  |  | UINT16 u16_CapacityFull | soc_export_public_fields | SOC_PublishReportData |
| u16_CapacityFactory | 103 + 309/Project/Source/SocEnhance.h | 60 |  |  |  | UINT16 u16_CapacityFactory | soc_export_public_fields | SOC_PublishReportData |
| u16_Cycle_times | 103 + 309/Project/Source/SocEnhance.h | 61 |  |  |  | UINT16 u16_Cycle_times | soc_export_public_fields | SOC_PublishReportData |
| u16_RefreshData_Flag | 103 + 309/Project/Source/SocEnhance.h | 62 |  |  |  | UINT16 u16_RefreshData_Flag | SOC_RequestCapacityReset, SOC_RequestSetOnce, soc_handle_command, test_set_soc_once_command_saves_snapshot | SOC_IntEnhance_Ctrl |
| u32CapFullAs10 | 103 + 309/Project/Source/SocEnhance.h | 69 |  |  |  | UINT32 u32CapFullAs10 | soc_watch_refresh |  |
| u32CapNowAs10 | 103 + 309/Project/Source/SocEnhance.h | 70 |  |  |  | UINT32 u32CapNowAs10 | soc_watch_refresh |  |
| u32CycleX100 | 103 + 309/Project/Source/SocEnhance.h | 71 |  |  |  | UINT32 u32CycleX100 | soc_watch_refresh |  |
| u32DsgAccAs10 | 103 + 309/Project/Source/SocEnhance.h | 72 |  |  |  | UINT32 u32DsgAccAs10 | soc_watch_refresh |  |
| u32RestTicks | 103 + 309/Project/Source/SocEnhance.h | 73 |  |  |  | UINT32 u32RestTicks | soc_watch_refresh |  |
| u32StableRestTicks | 103 + 309/Project/Source/SocEnhance.h | 74 |  |  |  | UINT32 u32StableRestTicks | soc_watch_refresh |  |
| u32LongRestDownTicks | 103 + 309/Project/Source/SocEnhance.h | 75 |  |  |  | UINT32 u32LongRestDownTicks | soc_watch_refresh |  |
| u16VCellMax | 103 + 309/Project/Source/SocEnhance.h | 76 |  |  |  | UINT16 u16VCellMax | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, RtcSleep_PortApplySocRtcRest, SystemDebug_Snapshot, new_todo_logi, soc_param_lib_init |
| u16VCellMin | 103 + 309/Project/Source/SocEnhance.h | 77 |  |  |  | UINT16 u16VCellMin | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, RtcSleep_PortApplySocRtcRest, RtcSleep_PortGetCellMinMv, RtcSleep_PortIsEmergencyWakeVoltage, SystemDebug_Snapshot, new_todo_logi, soc_param_lib_init |
| u16CellDelta | 103 + 309/Project/Source/SocEnhance.h | 78 |  |  |  | UINT16 u16CellDelta | soc_watch_refresh |  |
| u16Ichg | 103 + 309/Project/Source/SocEnhance.h | 79 |  |  |  | UINT16 u16Ichg | DataLoad_Current, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, CanFeidao_SendSoc1000ms, CanFeidao_SendStatus5000ms, CanFeidao_SendVoltageCurrent1000ms, RtcSleep_AfePortHasCurrentWake, RtcSleep_PortGetChargeCurrentMa, new_todo_logi, soc_param_lib_init |
| u16Idsg | 103 + 309/Project/Source/SocEnhance.h | 80 |  |  |  | UINT16 u16Idsg | soc_watch_refresh |  |
| u16FullTicks | 103 + 309/Project/Source/SocEnhance.h | 81 |  |  |  | UINT16 u16FullTicks | soc_watch_refresh |  |
| u16EmptyTicks | 103 + 309/Project/Source/SocEnhance.h | 82 |  |  |  | UINT16 u16EmptyTicks | soc_watch_refresh |  |
| u16DisplayTicks | 103 + 309/Project/Source/SocEnhance.h | 83 |  |  |  | UINT16 u16DisplayTicks | soc_watch_refresh |  |
| u16SagHoldTicks | 103 + 309/Project/Source/SocEnhance.h | 84 |  |  |  | UINT16 u16SagHoldTicks | soc_watch_refresh |  |
| u16RestRefVmin | 103 + 309/Project/Source/SocEnhance.h | 85 |  |  |  | UINT16 u16RestRefVmin | soc_watch_refresh |  |
| u16RestRefVmax | 103 + 309/Project/Source/SocEnhance.h | 86 |  |  |  | UINT16 u16RestRefVmax | soc_watch_refresh |  |
| u16EmptyTailTarget | 103 + 309/Project/Source/SocEnhance.h | 87 |  |  |  | UINT16 u16EmptyTailTarget | soc_watch_set_tail_state |  |
| u16EmptyTailTicks | 103 + 309/Project/Source/SocEnhance.h | 88 |  |  |  | UINT16 u16EmptyTailTicks | soc_watch_set_tail_state |  |
| u16SnapshotFlags | 103 + 309/Project/Source/SocEnhance.h | 89 |  |  |  | UINT16 u16SnapshotFlags | soc_watch_refresh |  |
| u8Mode | 103 + 309/Project/Source/SocEnhance.h | 90 |  |  |  | UINT8 u8Mode | soc_watch_refresh | test_board_self_consumption_integrates_during_relax |
| u8LastMode | 103 + 309/Project/Source/SocEnhance.h | 91 |  |  |  | UINT8 u8LastMode | soc_watch_refresh |  |
| u8InternalSoc | 103 + 309/Project/Source/SocEnhance.h | 92 |  |  |  | UINT8 u8InternalSoc | soc_watch_refresh | test_startup_ocv_uses_real_c_code |
| u8DisplaySoc | 103 + 309/Project/Source/SocEnhance.h | 93 |  |  |  | UINT8 u8DisplaySoc | soc_watch_refresh | test_startup_ocv_uses_real_c_code |
| u8Soh | 103 + 309/Project/Source/SocEnhance.h | 94 |  |  |  | UINT8 u8Soh | soc_watch_refresh |  |
| u8RestDownValid | 103 + 309/Project/Source/SocEnhance.h | 95 |  |  |  | UINT8 u8RestDownValid | soc_watch_refresh |  |
| u8RestDownTarget | 103 + 309/Project/Source/SocEnhance.h | 96 |  |  |  | UINT8 u8RestDownTarget | soc_watch_refresh |  |
| u8FullAnchor | 103 + 309/Project/Source/SocEnhance.h | 97 |  |  |  | UINT8 u8FullAnchor | soc_watch_refresh |  |
| u8LowTailActive | 103 + 309/Project/Source/SocEnhance.h | 98 |  |  |  | UINT8 u8LowTailActive | soc_watch_set_tail_state |  |
| u8CalibrationAllowed | 103 + 309/Project/Source/SocEnhance.h | 99 |  |  |  | UINT8 u8CalibrationAllowed | soc_watch_refresh |  |
| u8SagHoldBlocksCalibration | 103 + 309/Project/Source/SocEnhance.h | 100 |  |  |  | UINT8 u8SagHoldBlocksCalibration | soc_watch_refresh |  |
| u8RestVoltageStable | 103 + 309/Project/Source/SocEnhance.h | 101 |  |  |  | UINT8 u8RestVoltageStable | soc_watch_refresh |  |
| u8LastCalibSource | 103 + 309/Project/Source/SocEnhance.h | 102 |  |  |  | UINT8 u8LastCalibSource | soc_watch_set_calib_source | test_startup_ocv_uses_real_c_code |
| u8LastSocBefore | 103 + 309/Project/Source/SocEnhance.h | 103 |  |  |  | UINT8 u8LastSocBefore | soc_watch_set_calib_source |  |
| u8LastSocAfter | 103 + 309/Project/Source/SocEnhance.h | 104 |  |  |  | UINT8 u8LastSocAfter | soc_watch_set_calib_source |  |
| u8LastPublishForce | 103 + 309/Project/Source/SocEnhance.h | 105 |  |  |  | UINT8 u8LastPublishForce | soc_watch_refresh |  |
| SOC_Enhance_Element | 103 + 309/Project/Source/SocEnhance.h | 107 |  | extern |  | extern struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element |  |  |
| type | 103 + 309/Project/Source/SystemDebug.c | 47 |  |  |  | uint8_t type | SystemDebug_Event, SystemDebug_ReadEventRing | DbgPrint_EventRing |
| val0 | 103 + 309/Project/Source/SystemDebug.c | 48 |  |  |  | uint8_t val0 | SystemDebug_Event, SystemDebug_ReadEventRing |  |
| val1 | 103 + 309/Project/Source/SystemDebug.c | 49 |  |  |  | uint8_t val1 | SystemDebug_Event, SystemDebug_ReadEventRing |  |
| extra | 103 + 309/Project/Source/SystemDebug.c | 50 |  |  |  | uint16_t extra | SystemDebug_Event, SystemDebug_ReadEventRing | DbgPrint_EventRing |
| head | 103 + 309/Project/Source/SystemDebug.c | 56 |  |  |  | uint8_t head | SystemDebug_Event, feidao_can_clear_tx_queue, feidao_can_dequeue_tx | SystemDebug_ReadEventRing, feidao_can_queue_has_request |
| count | 103 + 309/Project/Source/SystemDebug.c | 57 |  |  |  | uint8_t count | CtCan_AppReadRegs, CtCan_AppWriteRegs, CtDebugLog_EncodeLatest, Sci_PutZeroWordsBE, Sci_RangeFits, SystemDebug_Event, feidao_can_clear_tx_queue, feidao_can_dequeue_tx, feidao_can_enqueue_tx, handle_bms_read | Can_GetDebugSnapshot, DbgPrint_EventRing, EEPROM_WordBlockInRange, Sci_ApplyOtherElementSideEffects, Sci_ApplyProtectSideEffects, Sci_CopyWords, Sci_PutBytes, Sci_RangeOverlaps, Sci_WrValuesInRange, Sci_WriteWordsFromRequest |
| fault | 103 + 309/Project/Source/SystemDebug.c | 58 |  |  | volatile | volatile struct SYSTEM_DEBUG fault | DebugHooks_RuntimeRecordEvents, Fault_DebugWatchBind, SystemDebug_Event, SystemDebug_Snapshot | SH367309_RecordFaultOnActive, SystemDebug_RefreshModuleStates |
| fault_valid | 103 + 309/Project/Source/SystemDebug.c | 59 |  |  | volatile | volatile uint8_t fault_valid | SystemDebug_Event |  |
| s_dbgRt | 103 + 309/Project/Source/SystemDebug.c | 61 | static |  |  | static DBG_RUNTIME s_dbgRt | SystemDebug_Event | DbgPrint_EventRing, SystemDebug_DebugWatchBind, SystemDebug_ReadEventRing |
| b_in | 103 + 309/Project/Source/SystemDebug.h | 79 |  |  |  | uint16_t b_in | SystemDebug_Snapshot | DbgPrint_IO |
| a_out | 103 + 309/Project/Source/SystemDebug.h | 80 |  |  |  | uint16_t a_out | SystemDebug_Snapshot | DbgPrint_IO |
| b_out | 103 + 309/Project/Source/SystemDebug.h | 81 |  |  |  | uint16_t b_out | SystemDebug_Snapshot | DbgPrint_IO |
| chg_in | 103 + 309/Project/Source/SystemDebug.h | 82 |  |  |  | uint8_t chg_in | SystemDebug_Snapshot | DbgPrint_IO |
| sw_key | 103 + 309/Project/Source/SystemDebug.h | 83 |  |  |  | uint8_t sw_key | SystemDebug_Snapshot | DbgPrint_IO |
| mcu_wk | 103 + 309/Project/Source/SystemDebug.h | 84 |  |  |  | uint8_t mcu_wk | SystemDebug_Snapshot | DbgPrint_IO |
| cmnt_en | 103 + 309/Project/Source/SystemDebug.h | 85 |  |  |  | uint8_t cmnt_en | SystemDebug_Snapshot | Conf_InitMainPowerRails, DbgPrint_IO |
| dc_en | 103 + 309/Project/Source/SystemDebug.h | 86 |  |  |  | uint8_t dc_en | SystemDebug_Snapshot | Conf_InitMainPowerRails, DbgPrint_IO |
| dbg_led | 103 + 309/Project/Source/SystemDebug.h | 87 |  |  |  | uint8_t dbg_led | SystemDebug_Snapshot |  |
| afe_ctlc | 103 + 309/Project/Source/SystemDebug.h | 88 |  |  |  | uint8_t afe_ctlc | SystemDebug_Snapshot |  |
| afe_pro_en | 103 + 309/Project/Source/SystemDebug.h | 89 |  |  |  | uint8_t afe_pro_en | SystemDebug_Snapshot |  |
| m_stb | 103 + 309/Project/Source/SystemDebug.h | 90 |  |  |  | uint8_t m_stb | SystemDebug_Snapshot | Conf_InitMainPowerRails, DbgPrint_IO |
| ad_en | 103 + 309/Project/Source/SystemDebug.h | 91 |  |  |  | uint8_t ad_en | SystemDebug_Snapshot | Conf_InitMainPowerRails, DbgPrint_IO |
| adc_bus_en | 103 + 309/Project/Source/SystemDebug.h | 92 |  |  |  | uint8_t adc_bus_en | SystemDebug_Snapshot | Conf_InitMainPowerRails, DbgPrint_IO |
| _2727_en | 103 + 309/Project/Source/SystemDebug.h | 93 |  |  |  | uint8_t _2727_en | SystemDebug_Snapshot | DbgPrint_IO |
| sw_dsg | 103 + 309/Project/Source/SystemDebug.h | 98 |  |  |  | uint8_t sw_dsg | SystemDebug_Snapshot | DbgPrint_IO, DbgPrint_Summary |
| hw_dsg_fet | 103 + 309/Project/Source/SystemDebug.h | 99 |  |  |  | uint8_t hw_dsg_fet | SystemDebug_Snapshot | DbgPrint_IO |
| hw_chg_fet | 103 + 309/Project/Source/SystemDebug.h | 100 |  |  |  | uint8_t hw_chg_fet | SystemDebug_Snapshot | DbgPrint_IO |
| feature | 103 + 309/Project/Source/SystemDebug.h | 105 |  |  |  | uint32_t feature | DebugWatch_BindAll, SystemDebug_Snapshot |  |
| err_lo | 103 + 309/Project/Source/SystemDebug.h | 106 |  |  |  | uint16_t err_lo | SystemDebug_Snapshot |  |
| err_hi | 103 + 309/Project/Source/SystemDebug.h | 107 |  |  |  | uint16_t err_hi | SystemDebug_Snapshot |  |
| max_gap_ticks | 103 + 309/Project/Source/SystemDebug.h | 112 |  |  |  | uint32_t max_gap_ticks | SystemDebug_ModuleHeartbeat, SystemDebug_RecordWatchdogFeed |  |
| run_cnt | 103 + 309/Project/Source/SystemDebug.h | 113 |  |  |  | uint32_t run_cnt | SystemDebug_ModuleHeartbeat | SystemDebug_ModuleBuildStaleMask |
| ready_mask | 103 + 309/Project/Source/SystemDebug.h | 118 |  |  |  | uint32_t ready_mask | SystemDebug_ModuleApplyState |  |
| busy_mask | 103 + 309/Project/Source/SystemDebug.h | 119 |  |  |  | uint32_t busy_mask | SystemDebug_ModuleApplyState |  |
| error_mask | 103 + 309/Project/Source/SystemDebug.h | 120 |  |  |  | uint32_t error_mask | SystemDebug_ModuleApplyState |  |
| stale_mask | 103 + 309/Project/Source/SystemDebug.h | 121 |  |  |  | uint32_t stale_mask | SystemDebug_ModuleBuildStaleMask, SystemDebug_RefreshModuleStates |  |
| last_id | 103 + 309/Project/Source/SystemDebug.h | 122 |  |  |  | uint8_t last_id | IrqDebug_CountFast, SystemDebug_ModuleHeartbeat, SystemDebug_SnapshotMcuResources |  |
| last_tick | 103 + 309/Project/Source/SystemDebug.h | 123 |  |  |  | uint32_t last_tick | SystemDebug_ModuleHeartbeat | SystemDebug_ModuleBuildStaleMask |
| cfgr | 103 + 309/Project/Source/SystemDebug.h | 145 |  |  |  | uint32_t cfgr | SystemDebug_SnapshotMcuResources |  |
| ahbenr | 103 + 309/Project/Source/SystemDebug.h | 146 |  |  |  | uint32_t ahbenr | SystemDebug_SnapshotMcuResources |  |
| apb1enr | 103 + 309/Project/Source/SystemDebug.h | 147 |  |  |  | uint32_t apb1enr | SystemDebug_SnapshotMcuResources |  |
| apb2enr | 103 + 309/Project/Source/SystemDebug.h | 148 |  |  |  | uint32_t apb2enr | SystemDebug_SnapshotMcuResources |  |
| bdcr | 103 + 309/Project/Source/SystemDebug.h | 149 |  |  |  | uint32_t bdcr | RTC_PrepareExistingClock, SystemDebug_SnapshotMcuResources |  |
| csr | 103 + 309/Project/Source/SystemDebug.h | 150 |  |  |  | uint32_t csr | Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_DataInit, Sci_ModbusOnRxIdle, Sci_ModbusProtocolFeed, Sci_ModbusResetMessage, SystemDebug_SnapshotMcuResources | Sci_ModbusIsBusy |
| sysclk_src | 103 + 309/Project/Source/SystemDebug.h | 151 |  |  |  | uint8_t sysclk_src | SystemDebug_SnapshotMcuResources |  |
| hse_ready | 103 + 309/Project/Source/SystemDebug.h | 152 |  |  |  | uint8_t hse_ready | SystemDebug_SnapshotMcuResources |  |
| pll_ready | 103 + 309/Project/Source/SystemDebug.h | 153 |  |  |  | uint8_t pll_ready | SystemDebug_SnapshotMcuResources |  |
| lsi_ready | 103 + 309/Project/Source/SystemDebug.h | 154 |  |  |  | uint8_t lsi_ready | SystemDebug_SnapshotMcuResources |  |
| ispr0 | 103 + 309/Project/Source/SystemDebug.h | 159 |  |  |  | uint32_t ispr0 | SystemDebug_SnapshotMcuResources |  |
| iabr0 | 103 + 309/Project/Source/SystemDebug.h | 160 |  |  |  | uint32_t iabr0 | SystemDebug_SnapshotMcuResources |  |
| scb_icsr | 103 + 309/Project/Source/SystemDebug.h | 161 |  |  |  | uint32_t scb_icsr | SystemDebug_SnapshotMcuResources |  |
| scb_shcsr | 103 + 309/Project/Source/SystemDebug.h | 162 |  |  |  | uint32_t scb_shcsr | SystemDebug_SnapshotMcuResources |  |
| systick_ctrl | 103 + 309/Project/Source/SystemDebug.h | 163 |  |  |  | uint32_t systick_ctrl | SystemDebug_SnapshotMcuResources |  |
| systick_val | 103 + 309/Project/Source/SystemDebug.h | 164 |  |  |  | uint32_t systick_val | SystemDebug_SnapshotMcuResources |  |
| exti_imr | 103 + 309/Project/Source/SystemDebug.h | 165 |  |  |  | uint32_t exti_imr | SystemDebug_SnapshotMcuResources |  |
| exti_pr | 103 + 309/Project/Source/SystemDebug.h | 166 |  |  |  | uint32_t exti_pr | IrqDebug_RecordEvent, SystemDebug_SnapshotMcuResources |  |
| irq_tim3_10ms | 103 + 309/Project/Source/SystemDebug.h | 167 |  |  |  | uint32_t irq_tim3_10ms | SystemDebug_SnapshotMcuResources |  |
| irq_tim4_ledbar | 103 + 309/Project/Source/SystemDebug.h | 168 |  |  |  | uint32_t irq_tim4_ledbar | SystemDebug_SnapshotMcuResources |  |
| irq_rtc_sec | 103 + 309/Project/Source/SystemDebug.h | 169 |  |  |  | uint32_t irq_rtc_sec | SystemDebug_SnapshotMcuResources |  |
| irq_rtc_alarm | 103 + 309/Project/Source/SystemDebug.h | 170 |  |  |  | uint32_t irq_rtc_alarm | SystemDebug_SnapshotMcuResources |  |
| irq_exti0_chg | 103 + 309/Project/Source/SystemDebug.h | 171 |  |  |  | uint32_t irq_exti0_chg | SystemDebug_SnapshotMcuResources |  |
| irq_exti9_key | 103 + 309/Project/Source/SystemDebug.h | 172 |  |  |  | uint32_t irq_exti9_key | SystemDebug_SnapshotMcuResources |  |
| irq_usart1 | 103 + 309/Project/Source/SystemDebug.h | 173 |  |  |  | uint32_t irq_usart1 | SystemDebug_SnapshotMcuResources |  |
| irq_can1_rx0 | 103 + 309/Project/Source/SystemDebug.h | 174 |  |  |  | uint32_t irq_can1_rx0 | SystemDebug_SnapshotMcuResources |  |
| irq_unhandled | 103 + 309/Project/Source/SystemDebug.h | 175 |  |  |  | uint32_t irq_unhandled | SystemDebug_SnapshotMcuResources |  |
| last_id | 103 + 309/Project/Source/SystemDebug.h | 176 |  |  |  | uint16_t last_id | IrqDebug_CountFast, SystemDebug_ModuleHeartbeat, SystemDebug_SnapshotMcuResources |  |
| last_vectactive | 103 + 309/Project/Source/SystemDebug.h | 177 |  |  |  | uint16_t last_vectactive | IrqDebug_CountFast, SystemDebug_SnapshotMcuResources | IrqDebug_RecordEvent |
| current_phase | 103 + 309/Project/Source/SystemDebug.h | 178 |  |  |  | uint8_t current_phase | IrqDebug_SetPhase, SystemDebug_SnapshotMcuResources | IrqDebug_CountFast |
| event_count | 103 + 309/Project/Source/SystemDebug.h | 179 |  |  |  | uint8_t event_count | IrqDebug_RecordEvent, SystemDebug_SnapshotMcuResources |  |
| usart2_sr | 103 + 309/Project/Source/SystemDebug.h | 184 |  |  |  | uint16_t usart2_sr | SystemDebug_SnapshotMcuResources |  |
| usart3_sr | 103 + 309/Project/Source/SystemDebug.h | 185 |  |  |  | uint16_t usart3_sr | SystemDebug_SnapshotMcuResources |  |
| can_msr | 103 + 309/Project/Source/SystemDebug.h | 186 |  |  |  | uint16_t can_msr | SystemDebug_SnapshotMcuResources |  |
| can_tsr | 103 + 309/Project/Source/SystemDebug.h | 187 |  |  |  | uint32_t can_tsr | SystemDebug_SnapshotMcuResources |  |
| can_rf0r | 103 + 309/Project/Source/SystemDebug.h | 188 |  |  |  | uint32_t can_rf0r | SystemDebug_SnapshotMcuResources |  |
| can_esr | 103 + 309/Project/Source/SystemDebug.h | 189 |  |  |  | uint32_t can_esr | SystemDebug_SnapshotMcuResources |  |
| adc1_sr | 103 + 309/Project/Source/SystemDebug.h | 190 |  |  |  | uint16_t adc1_sr | SystemDebug_SnapshotMcuResources |  |
| dma1_isr | 103 + 309/Project/Source/SystemDebug.h | 191 |  |  |  | uint32_t dma1_isr | SystemDebug_SnapshotMcuResources |  |
| tim3_sr | 103 + 309/Project/Source/SystemDebug.h | 192 |  |  |  | uint16_t tim3_sr | SystemDebug_SnapshotMcuResources |  |
| tim4_sr | 103 + 309/Project/Source/SystemDebug.h | 193 |  |  |  | uint16_t tim4_sr | SystemDebug_SnapshotMcuResources |  |
| flash_sr | 103 + 309/Project/Source/SystemDebug.h | 194 |  |  |  | uint16_t flash_sr | SystemDebug_SnapshotMcuResources |  |
| pwr_csr | 103 + 309/Project/Source/SystemDebug.h | 195 |  |  |  | uint16_t pwr_csr | SystemDebug_SnapshotMcuResources |  |
| pin | 103 + 309/Project/Source/SystemDebug.h | 200 |  |  |  | uint8_t pin | LedBar_GetPinIndex, LedBar_PinToOutput, LedBar_PinWrite, SystemDebug_SnapshotMcuResources | Conf_InitGpioMode, Conf_InitWakeupInputExti, LedBar_PinToOutputMode |
| por | 103 + 309/Project/Source/SystemDebug.h | 201 |  |  |  | uint8_t por | SystemDebug_SnapshotMcuResources |  |
| software | 103 + 309/Project/Source/SystemDebug.h | 202 |  |  |  | uint8_t software | SystemDebug_SnapshotMcuResources |  |
| iwdg | 103 + 309/Project/Source/SystemDebug.h | 203 |  |  |  | uint8_t iwdg | SystemDebug_SnapshotMcuResources |  |
| wwdg | 103 + 309/Project/Source/SystemDebug.h | 204 |  |  |  | uint8_t wwdg | SystemDebug_SnapshotMcuResources |  |
| low_power | 103 + 309/Project/Source/SystemDebug.h | 205 |  |  |  | uint8_t low_power | DebugWatch_BindAll, SystemDebug_SnapshotMcuResources | SystemDebug_ModuleItem |
| bus_off | 103 + 309/Project/Source/SystemDebug.h | 210 |  |  |  | uint8_t bus_off | Can_GetDebugSnapshot | DbgPrint_CAN, DbgPrint_Summary, SystemDebug_RefreshModuleStates, SystemDebug_Snapshot |
| tx_queue | 103 + 309/Project/Source/SystemDebug.h | 211 |  |  |  | uint8_t tx_queue | Can_GetDebugSnapshot | DbgPrint_CAN, SystemDebug_Snapshot |
| esr | 103 + 309/Project/Source/SystemDebug.h | 212 |  |  |  | uint16_t esr | Can_GetDebugSnapshot | DbgPrint_CAN, SystemDebug_Snapshot |
| ready | 103 + 309/Project/Source/SystemDebug.h | 217 |  |  |  | uint8_t ready | ADC_ResetAnlogCalSchedule, ADC_StopForLowPower, App_AnlogCal, SystemDebug_Snapshot | ADC_IsReady, DbgPrint_LP, SystemDebug_RefreshModuleStates |
| reserved | 103 + 309/Project/Source/SystemDebug.h | 218 |  |  |  | uint16_t reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| block | 103 + 309/Project/Source/SystemDebug.h | 219 |  |  |  | uint32_t block | SystemDebug_Snapshot, lp_deep, lp_select | DbgPrint_LP, DbgPrint_Summary, DebugHooks_RuntimeRecordEvents, can_handle_commit, can_handle_data, load_next_block, send_data_frame |
| sleep_sec | 103 + 309/Project/Source/SystemDebug.h | 220 |  |  |  | uint32_t sleep_sec | SystemDebug_Snapshot | DbgPrint_LP, DbgPrint_Wakeup |
| elapsed_sec | 103 + 309/Project/Source/SystemDebug.h | 221 |  |  |  | uint32_t elapsed_sec | SystemDebug_Snapshot | DbgPrint_LP, DbgPrint_Wakeup |
| hiccup_cycles | 103 + 309/Project/Source/SystemDebug.h | 222 |  |  |  | uint32_t hiccup_cycles | SystemDebug_Snapshot | DbgPrint_LP, DbgPrint_Wakeup |
| last_wake_src | 103 + 309/Project/Source/SystemDebug.h | 223 |  |  |  | uint8_t last_wake_src | SystemDebug_Snapshot | DbgPrint_Wakeup |
| typec_cur_ma | 103 + 309/Project/Source/SystemDebug.h | 228 |  |  |  | uint16_t typec_cur_ma | SystemDebug_Snapshot |  |
| vbat_mv | 103 + 309/Project/Source/SystemDebug.h | 229 |  |  |  | uint32_t vbat_mv | SystemDebug_Snapshot |  |
| raw_vbus | 103 + 309/Project/Source/SystemDebug.h | 230 |  |  |  | uint16_t raw_vbus | SystemDebug_Snapshot |  |
| raw_cur | 103 + 309/Project/Source/SystemDebug.h | 231 |  |  |  | uint16_t raw_cur | SystemDebug_Snapshot |  |
| raw_mos | 103 + 309/Project/Source/SystemDebug.h | 232 |  |  |  | uint16_t raw_mos | SystemDebug_Snapshot |  |
| soh | 103 + 309/Project/Source/SystemDebug.h | 238 |  |  |  | uint8_t soh | CanFeidao_SendSoh5000ms, CtCan_AppGetStatus, SystemDebug_Snapshot, soc_refresh_capacity_base | DbgPrint_SOC, soc_add_discharge, soc_export_public_fields, soc_watch_refresh |
| cap_now | 103 + 309/Project/Source/SystemDebug.h | 239 |  |  |  | uint16_t cap_now | CanFeidao_SendStatus5000ms, SystemDebug_Snapshot, host_pack_step, host_run_scenario | DbgPrint_SOC, host_true_soc |
| vmax | 103 + 309/Project/Source/SystemDebug.h | 240 |  |  |  | uint16_t vmax | SystemDebug_Snapshot, host_pack_voltage, host_run_scenario | DbgPrint_SOC, DbgPrint_Summary, host_emit_row, host_init_with_voltage, host_run_seconds, host_tick |
| vmin | 103 + 309/Project/Source/SystemDebug.h | 241 |  |  |  | uint16_t vmin | SystemDebug_Snapshot, host_pack_voltage, host_run_scenario | DbgPrint_SOC, DbgPrint_Summary, host_emit_row, host_init_with_voltage, host_run_seconds, host_tick |
| ichg | 103 + 309/Project/Source/SystemDebug.h | 242 |  |  |  | uint16_t ichg | SystemDebug_Snapshot | DbgPrint_SOC, DbgPrint_Summary, SOC_UpdateSampleData, host_run_seconds, host_tick |
| idsg | 103 + 309/Project/Source/SystemDebug.h | 243 |  |  |  | uint16_t idsg | SystemDebug_Snapshot | DbgPrint_SOC, DbgPrint_Summary, SOC_UpdateSampleData, host_run_seconds, host_tick |
| init_over | 103 + 309/Project/Source/SystemDebug.h | 244 |  |  |  | uint8_t init_over | SystemDebug_Snapshot | DbgPrint_SOC |
| vtotal | 103 + 309/Project/Source/SystemDebug.h | 245 |  |  |  | uint16_t vtotal | SystemDebug_Snapshot |  |
| mode | 103 + 309/Project/Source/SystemDebug.h | 246 |  |  |  | uint8_t mode | LP_GetBlockReason, LowPower_Request, SOC_GetDebugInternals, SOC_IntEnhance_Ctrl, SystemDebug_Snapshot, soc_apply_ocv_target_step, soc_empty_current_band, soc_heavy_discharge_active, soc_integrate, soc_integrate_current_ma | Conf_InitGpioMode, DbgPrint_LP, DbgPrint_SOC, DbgPrint_Summary, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, InitAFE1_Sleep, rtc_sleep, soc_apply_full_empty, soc_empty_tail_config |
| last_mode | 103 + 309/Project/Source/SystemDebug.h | 248 |  |  |  | uint8_t last_mode | SOC_GetDebugInternals, soc_integrate | SystemDebug_Snapshot, soc_watch_refresh |
| rest_ticks | 103 + 309/Project/Source/SystemDebug.h | 249 |  |  |  | uint32_t rest_ticks |  | DbgPrint_SOC, SystemDebug_Snapshot |
| stable_ticks | 103 + 309/Project/Source/SystemDebug.h | 250 |  |  |  | uint32_t stable_ticks |  | DbgPrint_SOC, SystemDebug_Snapshot |
| full_ticks | 103 + 309/Project/Source/SystemDebug.h | 251 |  |  |  | uint16_t full_ticks | SOC_GetDebugInternals, soc_apply_full_empty | DbgPrint_SOC, SystemDebug_Snapshot, soc_watch_refresh |
| empty_ticks | 103 + 309/Project/Source/SystemDebug.h | 252 |  |  |  | uint16_t empty_ticks | SOC_GetDebugInternals, soc_apply_full_empty | DbgPrint_SOC, SystemDebug_Snapshot, soc_watch_refresh |
| full_anchor | 103 + 309/Project/Source/SystemDebug.h | 253 |  |  |  | uint8_t full_anchor | SOC_GetDebugInternals, soc_apply_discharge_delta, soc_apply_full_empty, soc_set | DbgPrint_SOC, SystemDebug_Snapshot, soc_integrate, soc_watch_refresh |
| display_ticks | 103 + 309/Project/Source/SystemDebug.h | 254 |  |  |  | uint16_t display_ticks | SOC_GetDebugInternals, soc_update_display_soc | DbgPrint_SOC, SystemDebug_Snapshot, soc_watch_refresh |
| bstatus3 | 103 + 309/Project/Source/SystemDebug.h | 259 |  |  |  | uint8_t bstatus3 | SystemDebug_Snapshot |  |
| fault1 | 103 + 309/Project/Source/SystemDebug.h | 260 |  |  |  | uint8_t fault1 | SystemDebug_Snapshot |  |
| cur_raw | 103 + 309/Project/Source/SystemDebug.h | 261 |  |  |  | uint16_t cur_raw | SystemDebug_Snapshot |  |
| pec_err | 103 + 309/Project/Source/SystemDebug.h | 262 |  |  |  | uint16_t pec_err | SystemDebug_Snapshot |  |
| cell_min_mv | 103 + 309/Project/Source/SystemDebug.h | 263 |  |  |  | uint16_t cell_min_mv | SystemDebug_Snapshot |  |
| cell_max_mv | 103 + 309/Project/Source/SystemDebug.h | 264 |  |  |  | uint16_t cell_max_mv | SystemDebug_Snapshot |  |
| third | 103 + 309/Project/Source/SystemDebug.h | 269 |  |  |  | uint16_t third | Fault_DebugWatchBind, SystemDebug_Snapshot | SystemDebug_RefreshModuleStates |
| mdl1 | 103 + 309/Project/Source/SystemDebug.h | 270 |  |  |  | uint16_t mdl1 | SystemDebug_Snapshot | SystemDebug_RefreshModuleStates |
| mdl3 | 103 + 309/Project/Source/SystemDebug.h | 271 |  |  |  | uint16_t mdl3 | SystemDebug_Snapshot | SystemDebug_RefreshModuleStates |
| remain_sec | 103 + 309/Project/Source/SystemDebug.h | 276 |  |  |  | uint32_t remain_sec | SystemDebug_Snapshot |  |
| e2prom_flag | 103 + 309/Project/Source/SystemDebug.h | 281 |  |  |  | uint8_t e2prom_flag | SystemDebug_Snapshot | SystemDebug_RefreshModuleStates |
| busy | 103 + 309/Project/Source/SystemDebug.h | 282 |  |  |  | uint8_t busy | Sci_IsAnyPortBusy, StorageFlash_BeginWrite, StorageFlash_EndWrite, SystemDebug_Snapshot | StorageFlash_IsBusy |
| blank | 103 + 309/Project/Source/SystemDebug.h | 287 |  |  |  | uint8_t blank | APP_LedBar, LedBar_Clear, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_PrepareForStop, LedBar_SetIndicators, LedBar_SetNumber, LedBar_ShowSleepSocPreview, SystemDebug_RefreshModuleStates | LedBar_BuildCurrentFrame, SystemDebug_Snapshot |
| number | 103 + 309/Project/Source/SystemDebug.h | 288 |  |  |  | uint8_t number | APP_LedBar, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_SetNumber, LedBar_ShowSleepSocPreview | DbgPrint_All, LedBar_BuildCurrentFrame, SystemDebug_Snapshot |
| indicators | 103 + 309/Project/Source/SystemDebug.h | 289 |  |  |  | uint8_t indicators | LedBar_GetDebugSnapshot | SystemDebug_Snapshot |
| disp_10ms | 103 + 309/Project/Source/SystemDebug.h | 290 |  |  |  | uint16_t disp_10ms | LedBar_GetDebugSnapshot | SystemDebug_Snapshot |
| frame_len | 103 + 309/Project/Source/SystemDebug.h | 291 |  |  |  | uint8_t frame_len | LedBar_GetDebugSnapshot, respond | SystemDebug_Snapshot |
| scan_idx | 103 + 309/Project/Source/SystemDebug.h | 292 |  |  |  | uint8_t scan_idx | LedBar_GetDebugSnapshot | SystemDebug_Snapshot |
| key_active | 103 + 309/Project/Source/SystemDebug.h | 293 |  |  |  | uint8_t key_active | LedBar_GetDebugSnapshot, LedBar_Init | LedBar_ServiceSwitch, SystemDebug_RefreshModuleStates, SystemDebug_Snapshot |
| charge_icon | 103 + 309/Project/Source/SystemDebug.h | 294 |  |  |  | uint8_t charge_icon | LedBar_GetDebugSnapshot | DbgPrint_All, SystemDebug_Snapshot |
| percent_icon | 103 + 309/Project/Source/SystemDebug.h | 295 |  |  |  | uint8_t percent_icon | LedBar_GetDebugSnapshot | DbgPrint_All, SystemDebug_Snapshot |
| loop_max_us | 103 + 309/Project/Source/SystemDebug.h | 300 |  |  |  | uint32_t loop_max_us | SystemDebug_LoopEnter | DbgPrint_Summary |
| max_us | 103 + 309/Project/Source/SystemDebug.h | 305 |  |  |  | uint32_t max_us | SystemDebug_ProfileRecord | SystemDebug_LoopEnter |
| call_cnt | 103 + 309/Project/Source/SystemDebug.h | 306 |  |  |  | uint32_t call_cnt | SystemDebug_ProfileRecord |  |
| last_feed_tick | 103 + 309/Project/Source/SystemDebug.h | 319 |  |  |  | uint32_t last_feed_tick | SystemDebug_RecordWatchdogFeed |  |
| last_gap_ticks | 103 + 309/Project/Source/SystemDebug.h | 320 |  |  |  | uint32_t last_gap_ticks | SystemDebug_RecordWatchdogFeed |  |
| max_gap_ticks | 103 + 309/Project/Source/SystemDebug.h | 321 |  |  |  | uint32_t max_gap_ticks | SystemDebug_ModuleHeartbeat, SystemDebug_RecordWatchdogFeed |  |
| pr | 103 + 309/Project/Source/SystemDebug.h | 322 |  |  |  | uint16_t pr | SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |  |
| rlr | 103 + 309/Project/Source/SystemDebug.h | 323 |  |  |  | uint16_t rlr | SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |  |
| sr | 103 + 309/Project/Source/SystemDebug.h | 324 |  |  |  | uint16_t sr | SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |  |
| last_source | 103 + 309/Project/Source/SystemDebug.h | 325 |  |  |  | uint8_t last_source | SystemDebug_RecordWatchdogFeed |  |
| iwdg_reset | 103 + 309/Project/Source/SystemDebug.h | 326 |  |  |  | uint8_t iwdg_reset | SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources |  |
| afe_get_cnt | 103 + 309/Project/Source/SystemDebug.h | 331 |  |  |  | uint32_t afe_get_cnt |  |  |
| can_rcv_cnt | 103 + 309/Project/Source/SystemDebug.h | 332 |  |  |  | uint32_t can_rcv_cnt | SystemDebug_Snapshot, USB_LP_CAN1_RX0_IRQHandler | Can_IsBusy, Can_PeekBusy |
| rtc_sleep_cnt | 103 + 309/Project/Source/SystemDebug.h | 333 |  |  |  | uint32_t rtc_sleep_cnt | SystemDebug_Snapshot |  |
| rtc_sec_cnt | 103 + 309/Project/Source/SystemDebug.h | 334 |  |  |  | uint32_t rtc_sec_cnt | RTC_IRQHandler, SystemDebug_Snapshot |  |
| rtc_alm_cnt | 103 + 309/Project/Source/SystemDebug.h | 335 |  |  |  | uint32_t rtc_alm_cnt | RTC_HandleAlarmWakeup, SystemDebug_Snapshot |  |
| sci1_irq_cnt | 103 + 309/Project/Source/SystemDebug.h | 336 |  |  |  | uint32_t sci1_irq_cnt | SystemDebug_Snapshot, USART1_IRQHandler |  |
| pa0_irq_cnt | 103 + 309/Project/Source/SystemDebug.h | 337 |  |  |  | uint16_t pa0_irq_cnt | SystemDebug_Snapshot |  |
| key_irq_cnt | 103 + 309/Project/Source/SystemDebug.h | 338 |  |  |  | uint16_t key_irq_cnt | SystemDebug_Snapshot |  |
| tick_10ms | 103 + 309/Project/Source/SystemDebug.h | 339 |  |  |  | uint32_t tick_10ms | DebugWatch_BindAll, IrqDebug_RecordEvent, SystemDebug_Snapshot | DbgPrint_Summary |
| g_dbg | 103 + 309/Project/Source/SystemDebug.h | 365 |  | extern |  | extern struct SYSTEM_DEBUG g_dbg |  |  |
| s_st_SysTimePending | 103 + 309/Project/Source/System_Init.c | 6 | static |  | volatile | static volatile union SYS_TIME s_st_SysTimePending | SysTime_LatchTaskFlags, SysTime_Post10msTick, SysTime_ResetCounters | SysTime_HasPendingTaskFlags, SystemInit_DebugWatchBind |
| s_u32Sys10msTickCount | 103 + 309/Project/Source/System_Init.c | 7 | static |  | volatile | static volatile UINT32 s_u32Sys10msTickCount = 0U | SysTime_Post10msTick, SysTime_ResetCounters | SysTime_Get10msTickCount, SystemInit_DebugWatchBind |
| s_u8Cnt50ms | 103 + 309/Project/Source/System_Init.c | 8 | static |  |  | static UINT8 s_u8Cnt50ms = 0 | SysTime_Post10msTick, SysTime_ResetCounters | SystemInit_DebugWatchBind |
| s_u8Cnt100ms | 103 + 309/Project/Source/System_Init.c | 10 | static |  |  | static UINT8 s_u8Cnt100ms = 0 | SysTime_Post10msTick, SysTime_ResetCounters | SystemInit_DebugWatchBind |
| s_u8Cnt200ms | 103 + 309/Project/Source/System_Init.c | 11 | static |  |  | static UINT8 s_u8Cnt200ms = 0 | SysTime_Post10msTick, SysTime_ResetCounters | SystemInit_DebugWatchBind |
| s_u8Cnt1000ms | 103 + 309/Project/Source/System_Init.c | 12 | static |  |  | static UINT8 s_u8Cnt1000ms = 0 | SysTime_Post10msTick, SysTime_ResetCounters | SystemInit_DebugWatchBind |
| fac_us | 103 + 309/Project/Source/System_Init.c | 13 | static |  |  | static UINT8 fac_us = 0 | InitDelay | SystemInit_DebugWatchBind, __delay_us |
| fac_ms | 103 + 309/Project/Source/System_Init.c | 15 | static |  |  | static UINT16 fac_ms = 0 | InitDelay | SystemInit_DebugWatchBind, __delay_ms |
| s_u16Sys200msOverflowCnt | 103 + 309/Project/Source/System_Init.c | 20 | static |  | volatile | static volatile UINT16 s_u16Sys200msOverflowCnt = 0U | SysTime_Post200msTaskPeriod, SysTime_ResetCounters | SysTime_Get200msTaskOverflowCount, SystemInit_DebugWatchBind |
| b1Sys50msFlag | 103 + 309/Project/Source/System_Init.h | 59 |  |  |  | UINT16 b1Sys50msFlag : 1 | SysTime_Post10msTick |  |
| b1Sys100msFlag | 103 + 309/Project/Source/System_Init.h | 60 |  |  |  | UINT16 b1Sys100msFlag : 1 | APP_LedBar, SysTime_Post10msTick |  |
| b1Sys200msFlag | 103 + 309/Project/Source/System_Init.h | 61 |  |  |  | UINT16 b1Sys200msFlag : 1 | SysTime_Post10msTick |  |
| b1Sys1000msFlag | 103 + 309/Project/Source/System_Init.h | 62 |  |  |  | UINT16 b1Sys1000msFlag : 1 | SysTime_Post10msTick | App_LogRecord, RtcSleep_PortIsOneSecondTick |
| reserved | 103 + 309/Project/Source/System_Init.h | 63 |  |  |  | UINT16 reserved : 11 | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| u8CBC_CHG_Cnt | 103 + 309/Project/Source/System_Init.h | 70 |  |  |  | UINT8 u8CBC_CHG_Cnt |  |  |
| u8CBC_DSG_ErrFlag | 103 + 309/Project/Source/System_Init.h | 71 |  |  |  | UINT8 u8CBC_DSG_ErrFlag |  |  |
| u8CBC_DSG_Cnt | 103 + 309/Project/Source/System_Init.h | 72 |  |  |  | UINT8 u8CBC_DSG_Cnt |  |  |
| s_system_onoff_func | 103 + 309/Project/Source/System_Monitor.c | 4 | static |  | volatile | static volatile union System_OnOFF_Function s_system_onoff_func | InitSystemMonitorData_EEPROM, SystemFeature_SetById | SystemFeature_GetMask, SystemFeature_IsSocFixed, SystemFeature_IsSocZero, SystemMonitor_DebugWatchBind |
| s_system_status | 103 + 309/Project/Source/System_Monitor.c | 5 | static |  | volatile | static volatile union System_Status s_system_status | InitSystemMonitorData_EEPROM, SystemRuntime_MarkBootReady, SystemRuntime_SetAfeStatus, SystemRuntime_SetMosStatus, SystemRuntime_SetProjectVersion | SystemMonitor_DebugWatchBind, SystemRuntime_GetStatusSnapshot, SystemRuntime_IsChargeMosOpen, SystemRuntime_IsDischargeMosOpen |
| u8ErrFlag_Com_AFE2 | 103 + 309/Project/Source/System_Monitor.h | 88 |  |  |  | UINT8 u8ErrFlag_Com_AFE2 |  |  |
| u8ErrFlag_Com_Can | 103 + 309/Project/Source/System_Monitor.h | 89 |  |  |  | UINT8 u8ErrFlag_Com_Can |  |  |
| u8ErrFlag_Com_EEPROM | 103 + 309/Project/Source/System_Monitor.h | 90 |  |  |  | UINT8 u8ErrFlag_Com_EEPROM |  |  |
| u8ErrFlag_Com_SPI | 103 + 309/Project/Source/System_Monitor.h | 91 |  |  |  | UINT8 u8ErrFlag_Com_SPI |  |  |
| u8ErrFlag_Com_Upper | 103 + 309/Project/Source/System_Monitor.h | 93 |  |  |  | UINT8 u8ErrFlag_Com_Upper |  |  |
| u8ErrFlag_Com_Client | 103 + 309/Project/Source/System_Monitor.h | 94 |  |  |  | UINT8 u8ErrFlag_Com_Client |  |  |
| u8ErrFlag_Com_Screen | 103 + 309/Project/Source/System_Monitor.h | 95 |  |  |  | UINT8 u8ErrFlag_Com_Screen |  |  |
| u8ErrFlag_Com_Wifi | 103 + 309/Project/Source/System_Monitor.h | 96 |  |  |  | UINT8 u8ErrFlag_Com_Wifi |  |  |
| u8ErrFlag_Com_BlueTooth | 103 + 309/Project/Source/System_Monitor.h | 98 |  |  |  | UINT8 u8ErrFlag_Com_BlueTooth |  |  |
| u8ErrFlag_Com_App | 103 + 309/Project/Source/System_Monitor.h | 99 |  |  |  | UINT8 u8ErrFlag_Com_App |  |  |
| u8ErrFlag_CBC_CHG | 103 + 309/Project/Source/System_Monitor.h | 100 |  |  |  | UINT8 u8ErrFlag_CBC_CHG |  |  |
| u8ErrFlag_Store_EEPROM | 103 + 309/Project/Source/System_Monitor.h | 101 |  |  |  | UINT8 u8ErrFlag_Store_EEPROM |  |  |
| u8ErrFlag_HSE | 103 + 309/Project/Source/System_Monitor.h | 103 |  |  |  | UINT8 u8ErrFlag_HSE |  |  |
| u8ErrFlag_LSE | 103 + 309/Project/Source/System_Monitor.h | 104 |  |  |  | UINT8 u8ErrFlag_LSE |  |  |
| u8ErrFlag_Vdelta_OVER | 103 + 309/Project/Source/System_Monitor.h | 105 |  |  |  | UINT8 u8ErrFlag_Vdelta_OVER |  |  |
| u8ErrFlag_Balanced | 103 + 309/Project/Source/System_Monitor.h | 106 |  |  |  | UINT8 u8ErrFlag_Balanced |  |  |
| u8ErrFlag_ADC | 103 + 309/Project/Source/System_Monitor.h | 108 |  |  |  | UINT8 u8ErrFlag_ADC |  |  |
| u8ErrFlag_Reserved21 | 103 + 309/Project/Source/System_Monitor.h | 109 |  |  |  | UINT8 u8ErrFlag_Reserved21 |  |  |
| u8ErrFlag_Reserved22 | 103 + 309/Project/Source/System_Monitor.h | 110 |  |  |  | UINT8 u8ErrFlag_Reserved22 |  |  |
| u8ErrFlag_CBC_DSG | 103 + 309/Project/Source/System_Monitor.h | 111 |  |  |  | UINT8 u8ErrFlag_CBC_DSG | Fault_ChangeToMCU |  |
| u8ErrFlag_SOC_Cail | 103 + 309/Project/Source/System_Monitor.h | 113 |  |  |  | UINT8 u8ErrFlag_SOC_Cail |  |  |
| u8ErrFlag_TempBreak | 103 + 309/Project/Source/System_Monitor.h | 114 |  |  |  | UINT8 u8ErrFlag_TempBreak |  |  |
| u8Res6 | 103 + 309/Project/Source/System_Monitor.h | 115 |  |  |  | UINT8 u8Res6 |  |  |
| b1StartUpFlag_Balance | 103 + 309/Project/Source/System_Monitor.h | 125 |  |  |  | UINT8 b1StartUpFlag_Balance :1 |  |  |
| b1StartUpFlag_Protect | 103 + 309/Project/Source/System_Monitor.h | 126 |  |  |  | UINT8 b1StartUpFlag_Protect :1 |  |  |
| b1StartUpFlag_MOS | 103 + 309/Project/Source/System_Monitor.h | 127 |  |  |  | UINT8 b1StartUpFlag_MOS :1 |  |  |
| b1StartUpFlag_Relay | 103 + 309/Project/Source/System_Monitor.h | 128 |  |  |  | UINT8 b1StartUpFlag_Relay :1 |  |  |
| b1StartUpFlag_ADC | 103 + 309/Project/Source/System_Monitor.h | 130 |  |  |  | UINT8 b1StartUpFlag_ADC :1 |  |  |
| b1StartUpFlag_CAN | 103 + 309/Project/Source/System_Monitor.h | 131 |  |  |  | UINT8 b1StartUpFlag_CAN :1 |  |  |
| b1StartUpFlag_Reserved7 | 103 + 309/Project/Source/System_Monitor.h | 132 |  |  |  | UINT8 b1StartUpFlag_Reserved7 :1 |  |  |
| b1StartUpFlag_Reserved8 | 103 + 309/Project/Source/System_Monitor.h | 133 |  |  |  | UINT8 b1StartUpFlag_Reserved8 :1 |  |  |
| b1StartUpFlag_AFE1 | 103 + 309/Project/Source/System_Monitor.h | 135 |  |  |  | UINT8 b1StartUpFlag_AFE1 :1 |  |  |
| b1StartUpFlag_AFE2 | 103 + 309/Project/Source/System_Monitor.h | 136 |  |  |  | UINT8 b1StartUpFlag_AFE2 :1 |  |  |
| b1StartUpFlag_BlueT | 103 + 309/Project/Source/System_Monitor.h | 137 |  |  |  | UINT8 b1StartUpFlag_BlueT :1 |  |  |
| bRcved7 | 103 + 309/Project/Source/System_Monitor.h | 138 |  |  |  | UINT8 bRcved7 :1 |  |  |
| bRcved8 | 103 + 309/Project/Source/System_Monitor.h | 140 |  |  |  | UINT8 bRcved8 :1 |  |  |
| bRcved9 | 103 + 309/Project/Source/System_Monitor.h | 141 |  |  |  | UINT8 bRcved9 :1 |  |  |
| bRcved10 | 103 + 309/Project/Source/System_Monitor.h | 142 |  |  |  | UINT8 bRcved10 :1 |  |  |
| bRcved11 | 103 + 309/Project/Source/System_Monitor.h | 143 |  |  |  | UINT8 bRcved11 :8 |  |  |
| bRcved12 | 103 + 309/Project/Source/System_Monitor.h | 145 |  |  |  | UINT8 bRcved12 :8 |  |  |
| b1Status_MOS_PRE | 103 + 309/Project/Source/System_Monitor.h | 155 |  |  |  | UINT8 b1Status_MOS_PRE :1 |  |  |
| b1Status_MOS_CHG | 103 + 309/Project/Source/System_Monitor.h | 156 |  |  |  | UINT8 b1Status_MOS_CHG :1 | SystemRuntime_SetMosStatus | SystemRuntime_IsChargeMosOpen |
| b1Status_MOS_DSG | 103 + 309/Project/Source/System_Monitor.h | 157 |  |  |  | UINT8 b1Status_MOS_DSG :1 | SystemRuntime_SetMosStatus | SystemRuntime_IsDischargeMosOpen |
| b1Status_Relay_PRE | 103 + 309/Project/Source/System_Monitor.h | 158 |  |  |  | UINT8 b1Status_Relay_PRE :1 |  |  |
| b1Status_Relay_CHG | 103 + 309/Project/Source/System_Monitor.h | 160 |  |  |  | UINT8 b1Status_Relay_CHG :1 |  |  |
| b1Status_Relay_DSG | 103 + 309/Project/Source/System_Monitor.h | 161 |  |  |  | UINT8 b1Status_Relay_DSG :1 |  |  |
| b1Status_Relay_MAIN | 103 + 309/Project/Source/System_Monitor.h | 162 |  |  |  | UINT8 b1Status_Relay_MAIN :1 |  |  |
| b1Status_ReservedHeat | 103 + 309/Project/Source/System_Monitor.h | 163 |  |  |  | UINT8 b1Status_ReservedHeat :1 |  |  |
| b1Status_ReservedCool | 103 + 309/Project/Source/System_Monitor.h | 165 |  |  |  | UINT8 b1Status_ReservedCool :1 |  |  |
| b1Status_AFE1 | 103 + 309/Project/Source/System_Monitor.h | 166 |  |  |  | UINT8 b1Status_AFE1 :1 | SystemRuntime_SetAfeStatus |  |
| b1Status_AFE2 | 103 + 309/Project/Source/System_Monitor.h | 167 |  |  |  | UINT8 b1Status_AFE2 :1 | SystemRuntime_SetAfeStatus |  |
| b1Status_Balance | 103 + 309/Project/Source/System_Monitor.h | 168 |  |  |  | UINT8 b1Status_Balance :1 |  |  |
| b1Status_ToSleep | 103 + 309/Project/Source/System_Monitor.h | 170 |  |  |  | UINT8 b1Status_ToSleep :1 | SystemRuntime_MarkBootReady |  |
| b1Status_BnCloseIO | 103 + 309/Project/Source/System_Monitor.h | 171 |  |  |  | UINT8 b1Status_BnCloseIO :1 |  |  |
| b1Status_ReservedHeatCloseIO | 103 + 309/Project/Source/System_Monitor.h | 172 |  |  |  | UINT8 b1Status_ReservedHeatCloseIO :1 |  |  |
| b1Status_SysLimits | 103 + 309/Project/Source/System_Monitor.h | 173 |  |  |  | UINT8 b1Status_SysLimits :1 |  |  |
| b1Status_CBCCloseIO | 103 + 309/Project/Source/System_Monitor.h | 175 |  |  |  | UINT8 b1Status_CBCCloseIO :1 |  |  |
| b1Status_DriverExtCtrl | 103 + 309/Project/Source/System_Monitor.h | 176 |  |  |  | UINT8 b1Status_DriverExtCtrl:1 |  |  |
| bRcved6 | 103 + 309/Project/Source/System_Monitor.h | 177 |  |  |  | UINT8 bRcved6 :1 |  |  |
| b4Status_ProjectVer | 103 + 309/Project/Source/System_Monitor.h | 178 |  |  |  | UINT8 b4Status_ProjectVer :4 | SystemRuntime_SetProjectVersion |  |
| bRcved11 | 103 + 309/Project/Source/System_Monitor.h | 180 |  |  |  | UINT8 bRcved11 :8 |  |  |
| b1OnOFF_BMS_Source | 103 + 309/Project/Source/System_Monitor.h | 191 |  |  |  | UINT8 b1OnOFF_BMS_Source :1 |  |  |
| b1OnOFF_MOS_Relay | 103 + 309/Project/Source/System_Monitor.h | 192 |  |  |  | UINT8 b1OnOFF_MOS_Relay :1 |  |  |
| b1OnOFF_Relay_Rec | 103 + 309/Project/Source/System_Monitor.h | 193 |  |  |  | UINT8 b1OnOFF_Relay_Rec :1 |  |  |
| b1OnOFF_SOC_Fixed | 103 + 309/Project/Source/System_Monitor.h | 195 |  |  |  | UINT8 b1OnOFF_SOC_Fixed :1 |  | SystemFeature_IsSocFixed |
| b1OnOFF_ReservedHeat | 103 + 309/Project/Source/System_Monitor.h | 197 |  |  |  | UINT8 b1OnOFF_ReservedHeat :1 |  |  |
| b1OnOFF_ReservedCool | 103 + 309/Project/Source/System_Monitor.h | 198 |  |  |  | UINT8 b1OnOFF_ReservedCool :1 |  |  |
| b1OnOFF_AFE1 | 103 + 309/Project/Source/System_Monitor.h | 199 |  |  |  | UINT8 b1OnOFF_AFE1 :1 |  |  |
| b1OnOFF_AFE2 | 103 + 309/Project/Source/System_Monitor.h | 200 |  |  |  | UINT8 b1OnOFF_AFE2 :1 |  |  |
| b1OnOFF_Sleep | 103 + 309/Project/Source/System_Monitor.h | 202 |  |  |  | UINT8 b1OnOFF_Sleep :1 |  |  |
| b1OnOFF_SOC_Zero | 103 + 309/Project/Source/System_Monitor.h | 203 |  |  |  | UINT8 b1OnOFF_SOC_Zero :1 |  | SystemFeature_IsSocZero |
| bRcved5 | 103 + 309/Project/Source/System_Monitor.h | 204 |  |  |  | UINT8 bRcved5 :1 |  |  |
| bRcved1 | 103 + 309/Project/Source/System_Monitor.h | 205 |  |  |  | UINT8 bRcved1 :4 |  |  |
| bRcved2 | 103 + 309/Project/Source/System_Monitor.h | 207 |  |  |  | UINT8 bRcved2 :8 |  |  |
| bRcved3 | 103 + 309/Project/Source/System_Monitor.h | 209 |  |  |  | UINT8 bRcved3 :8 |  |  |
| System_ErrFlag | 103 + 309/Project/Source/System_Monitor.h | 212 |  | extern | volatile | extern volatile struct SYSTEM_ERROR System_ErrFlag | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi |
| last_ext_comm_cnt_can | 103 + 309/Project/Source/conf/conf.h | 94 |  |  |  | uint32_t last_ext_comm_cnt_can | Can_IsBusy | Can_PeekBusy |
| can_rcv_cnt | 103 + 309/Project/Source/conf/conf.h | 95 |  |  |  | uint32_t can_rcv_cnt | SystemDebug_Snapshot, USB_LP_CAN1_RX0_IRQHandler | Can_IsBusy, Can_PeekBusy |
| test_main_cycle | 103 + 309/Project/Source/conf/conf.h | 96 |  |  |  | uint64_t test_main_cycle |  |  |
| App_AFEGet_cnt | 103 + 309/Project/Source/conf/conf.h | 97 |  |  |  | uint32_t App_AFEGet_cnt |  |  |
| App_SH367309_Monitor_cnt | 103 + 309/Project/Source/conf/conf.h | 98 |  |  |  | uint32_t App_SH367309_Monitor_cnt |  |  |
| sci1_irq_cnt | 103 + 309/Project/Source/conf/conf.h | 99 |  |  |  | uint32_t sci1_irq_cnt | SystemDebug_Snapshot, USART1_IRQHandler |  |
| sci2_irq_cnt | 103 + 309/Project/Source/conf/conf.h | 101 |  |  |  | uint32_t sci2_irq_cnt | USART2_IRQHandler |  |
| sci3_irq_cnt | 103 + 309/Project/Source/conf/conf.h | 102 |  |  |  | uint32_t sci3_irq_cnt | USART3_IRQHandler |  |
| cnt_PA0_irq | 103 + 309/Project/Source/conf/conf.h | 103 |  |  |  | uint16_t cnt_PA0_irq | EXTI0_IRQHandler | SystemDebug_Snapshot |
| cnt_bms1_keyirq | 103 + 309/Project/Source/conf/conf.h | 105 |  |  |  | uint16_t cnt_bms1_keyirq | EXTI9_5_IRQHandler | SystemDebug_Snapshot |
| pec_err_cnt | 103 + 309/Project/Source/conf/conf.h | 106 |  |  |  | uint16_t pec_err_cnt |  |  |
| CHG | 103 + 309/Project/Source/conf/conf.h | 107 |  |  |  | uint16_t CHG |  | DataLoad_Current |
| DSG | 103 + 309/Project/Source/conf/conf.h | 109 |  |  |  | uint16_t DSG |  | DataLoad_Current |
| cnt_enter_chg_open | 103 + 309/Project/Source/conf/conf.h | 110 |  |  |  | uint16_t cnt_enter_chg_open |  |  |
| cnt_enter_dsg_open | 103 + 309/Project/Source/conf/conf.h | 112 |  |  |  | uint16_t cnt_enter_dsg_open |  |  |
| wakeup_reason | 103 + 309/Project/Source/conf/conf.h | 113 |  |  |  | uint8_t wakeup_reason |  |  |
| wakeup_rtc | 103 + 309/Project/Source/conf/conf.h | 115 |  |  |  | bool wakeup_rtc | InitRunAfterStopWakeup, Monitor_TempBreak |  |
| time_enter_rtc | 103 + 309/Project/Source/conf/conf.h | 116 |  |  |  | uint8_t time_enter_rtc |  | lp_idle, lp_sync |
| power_on | 103 + 309/Project/Source/conf/conf.h | 117 |  |  |  | bool power_on | Can_GetDebugSnapshot | DbgPrint_CAN, DbgPrint_Summary, SystemDebug_Snapshot |
| enter_rtc_delay | 103 + 309/Project/Source/conf/conf.h | 118 |  |  |  | uint16_t enter_rtc_delay |  |  |
| rtc_sleep_cnt | 103 + 309/Project/Source/conf/conf.h | 120 |  |  |  | uint32_t rtc_sleep_cnt | SystemDebug_Snapshot |  |
| rtc_sec_cnt | 103 + 309/Project/Source/conf/conf.h | 121 |  |  |  | uint32_t rtc_sec_cnt | RTC_IRQHandler, SystemDebug_Snapshot |  |
| rtc_alm_cnt | 103 + 309/Project/Source/conf/conf.h | 122 |  |  |  | uint32_t rtc_alm_cnt | RTC_HandleAlarmWakeup, SystemDebug_Snapshot |  |
| rtc_irq_cnt | 103 + 309/Project/Source/conf/conf.h | 123 |  |  |  | uint32_t rtc_irq_cnt |  |  |
| isdebugenable | 103 + 309/Project/Source/conf/conf.h | 124 |  |  |  | uint8_t isdebugenable | DataLoad_Current |  |
| typec_curr_sim | 103 + 309/Project/Source/conf/conf.h | 126 |  |  |  | bool typec_curr_sim |  | ADC_GetTypeCOutCurrentMilliAmp |
| typc_curr | 103 + 309/Project/Source/conf/conf.h | 127 |  |  |  | uint16_t typc_curr |  | ADC_GetTypeCOutCurrentMilliAmp |
| sys_time | 103 + 309/Project/Source/conf/conf.h | 129 |  | extern |  | extern Time_T sys_time |  |  |
| g_stLowPowerRtcStatus | 103 + 309/Project/Source/rtc_sleep.c | 14 |  |  | volatile | volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus = { NO_SLEEP, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U} | LP_GetBlockReason, LP_RecordLastSleepSeconds, LowPower_Request, lp_deep, lp_idle, lp_select, lp_sync, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | DbgPrint_Wakeup, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, DebugWatch_BindAll, LP_GetLastSleepSeconds, SystemDebug_Snapshot, rtc_sleep |
| rtc | 103 + 309/Project/Source/rtc_sleep.h | 41 |  |  |  | uint8_t rtc | RTC_DebugWatchBind, lp_sync | DbgPrint_Wakeup |
| comm | 103 + 309/Project/Source/rtc_sleep.h | 42 |  |  |  | uint8_t comm | LP_GetBlockReason, Sci_DebugWatchBind |  |
| reserved | 103 + 309/Project/Source/rtc_sleep.h | 43 |  |  |  | uint8_t reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| idle | 103 + 309/Project/Source/rtc_sleep.h | 44 |  |  |  | uint16_t idle | lp_deep, lp_idle, lp_select |  |
| idleMax | 103 + 309/Project/Source/rtc_sleep.h | 45 |  |  |  | uint16_t idleMax | lp_sync |  |
| force | 103 + 309/Project/Source/rtc_sleep.h | 46 |  |  |  | uint16_t force | lp_deep |  |
| reserved16 | 103 + 309/Project/Source/rtc_sleep.h | 47 |  |  |  | uint16_t reserved16 |  |  |
| vlow | 103 + 309/Project/Source/rtc_sleep.h | 48 |  |  |  | uint32_t vlow | lp_deep |  |
| block | 103 + 309/Project/Source/rtc_sleep.h | 49 |  |  |  | uint32_t block | SystemDebug_Snapshot, lp_deep, lp_select | DbgPrint_LP, DbgPrint_Summary, DebugHooks_RuntimeRecordEvents, can_handle_commit, can_handle_data, load_next_block, send_data_frame |
| sleep | 103 + 309/Project/Source/rtc_sleep.h | 50 |  |  |  | uint32_t sleep | DebugWatch_BindAll, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_PrepareForStop, LedBar_SetSleep, LedBar_ShowSleepSocPreview, SleepDeal_DebugWatchBind, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | APP_LedBar, DbgPrint_All, LedBar_BuildCurrentFrame, LedBar_IsActiveForLowPower, LedBar_Scan1ms, SystemDebug_Snapshot |
| last | 103 + 309/Project/Source/rtc_sleep.h | 51 |  |  |  | uint32_t last | ADC_ResetAnlogCalSchedule, App_AnlogCal, LP_RecordLastSleepSeconds, RTC_WKTimeConfig, soc_table_percent | LP_GetLastSleepSeconds, RTC_GetLastWakeupPeriodSeconds, SystemDebug_Snapshot |
| cycles | 103 + 309/Project/Source/rtc_sleep.h | 52 |  |  |  | uint32_t cycles | CanFeidao_SendSoh5000ms, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle, soc_export_public_fields | SystemDebug_CycCntToUs, SystemDebug_Snapshot |
| g_stLowPowerRtcStatus | 103 + 309/Project/Source/rtc_sleep.h | 54 |  | extern | volatile | extern volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus | LP_GetBlockReason, LP_RecordLastSleepSeconds, LowPower_Request, lp_deep, lp_idle, lp_select, lp_sync, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | DbgPrint_Wakeup, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, DebugWatch_BindAll, LP_GetLastSleepSeconds, SystemDebug_Snapshot, rtc_sleep |
| g_irq_t | 103 + 309/Project/Source/rtc_sleep.h | 56 |  | extern |  | extern enum irqWakeup g_irq_t |  |  |
| last_ext_comm_cnt_can | build/host_tests/conf_board_self_0/conf.h | 94 |  |  |  | uint32_t last_ext_comm_cnt_can | Can_IsBusy | Can_PeekBusy |
| can_rcv_cnt | build/host_tests/conf_board_self_0/conf.h | 95 |  |  |  | uint32_t can_rcv_cnt | SystemDebug_Snapshot, USB_LP_CAN1_RX0_IRQHandler | Can_IsBusy, Can_PeekBusy |
| test_main_cycle | build/host_tests/conf_board_self_0/conf.h | 96 |  |  |  | uint64_t test_main_cycle |  |  |
| App_AFEGet_cnt | build/host_tests/conf_board_self_0/conf.h | 97 |  |  |  | uint32_t App_AFEGet_cnt |  |  |
| App_SH367309_Monitor_cnt | build/host_tests/conf_board_self_0/conf.h | 98 |  |  |  | uint32_t App_SH367309_Monitor_cnt |  |  |
| sci1_irq_cnt | build/host_tests/conf_board_self_0/conf.h | 99 |  |  |  | uint32_t sci1_irq_cnt | SystemDebug_Snapshot, USART1_IRQHandler |  |
| sci2_irq_cnt | build/host_tests/conf_board_self_0/conf.h | 101 |  |  |  | uint32_t sci2_irq_cnt | USART2_IRQHandler |  |
| sci3_irq_cnt | build/host_tests/conf_board_self_0/conf.h | 102 |  |  |  | uint32_t sci3_irq_cnt | USART3_IRQHandler |  |
| cnt_PA0_irq | build/host_tests/conf_board_self_0/conf.h | 103 |  |  |  | uint16_t cnt_PA0_irq | EXTI0_IRQHandler | SystemDebug_Snapshot |
| cnt_bms1_keyirq | build/host_tests/conf_board_self_0/conf.h | 105 |  |  |  | uint16_t cnt_bms1_keyirq | EXTI9_5_IRQHandler | SystemDebug_Snapshot |
| pec_err_cnt | build/host_tests/conf_board_self_0/conf.h | 106 |  |  |  | uint16_t pec_err_cnt |  |  |
| CHG | build/host_tests/conf_board_self_0/conf.h | 107 |  |  |  | uint16_t CHG |  | DataLoad_Current |
| DSG | build/host_tests/conf_board_self_0/conf.h | 109 |  |  |  | uint16_t DSG |  | DataLoad_Current |
| cnt_enter_chg_open | build/host_tests/conf_board_self_0/conf.h | 110 |  |  |  | uint16_t cnt_enter_chg_open |  |  |
| cnt_enter_dsg_open | build/host_tests/conf_board_self_0/conf.h | 112 |  |  |  | uint16_t cnt_enter_dsg_open |  |  |
| wakeup_reason | build/host_tests/conf_board_self_0/conf.h | 113 |  |  |  | uint8_t wakeup_reason |  |  |
| wakeup_rtc | build/host_tests/conf_board_self_0/conf.h | 115 |  |  |  | bool wakeup_rtc | InitRunAfterStopWakeup, Monitor_TempBreak |  |
| time_enter_rtc | build/host_tests/conf_board_self_0/conf.h | 116 |  |  |  | uint8_t time_enter_rtc |  | lp_idle, lp_sync |
| power_on | build/host_tests/conf_board_self_0/conf.h | 117 |  |  |  | bool power_on | Can_GetDebugSnapshot | DbgPrint_CAN, DbgPrint_Summary, SystemDebug_Snapshot |
| enter_rtc_delay | build/host_tests/conf_board_self_0/conf.h | 118 |  |  |  | uint16_t enter_rtc_delay |  |  |
| rtc_sleep_cnt | build/host_tests/conf_board_self_0/conf.h | 120 |  |  |  | uint32_t rtc_sleep_cnt | SystemDebug_Snapshot |  |
| rtc_sec_cnt | build/host_tests/conf_board_self_0/conf.h | 121 |  |  |  | uint32_t rtc_sec_cnt | RTC_IRQHandler, SystemDebug_Snapshot |  |
| rtc_alm_cnt | build/host_tests/conf_board_self_0/conf.h | 122 |  |  |  | uint32_t rtc_alm_cnt | RTC_HandleAlarmWakeup, SystemDebug_Snapshot |  |
| rtc_irq_cnt | build/host_tests/conf_board_self_0/conf.h | 123 |  |  |  | uint32_t rtc_irq_cnt |  |  |
| isdebugenable | build/host_tests/conf_board_self_0/conf.h | 124 |  |  |  | uint8_t isdebugenable | DataLoad_Current |  |
| typec_curr_sim | build/host_tests/conf_board_self_0/conf.h | 126 |  |  |  | bool typec_curr_sim |  | ADC_GetTypeCOutCurrentMilliAmp |
| typc_curr | build/host_tests/conf_board_self_0/conf.h | 127 |  |  |  | uint16_t typc_curr |  | ADC_GetTypeCOutCurrentMilliAmp |
| sys_time | build/host_tests/conf_board_self_0/conf.h | 129 |  | extern |  | extern Time_T sys_time |  |  |
| last_ext_comm_cnt_can | build/host_tests/conf_board_self_1000/conf.h | 94 |  |  |  | uint32_t last_ext_comm_cnt_can | Can_IsBusy | Can_PeekBusy |
| can_rcv_cnt | build/host_tests/conf_board_self_1000/conf.h | 95 |  |  |  | uint32_t can_rcv_cnt | SystemDebug_Snapshot, USB_LP_CAN1_RX0_IRQHandler | Can_IsBusy, Can_PeekBusy |
| test_main_cycle | build/host_tests/conf_board_self_1000/conf.h | 96 |  |  |  | uint64_t test_main_cycle |  |  |
| App_AFEGet_cnt | build/host_tests/conf_board_self_1000/conf.h | 97 |  |  |  | uint32_t App_AFEGet_cnt |  |  |
| App_SH367309_Monitor_cnt | build/host_tests/conf_board_self_1000/conf.h | 98 |  |  |  | uint32_t App_SH367309_Monitor_cnt |  |  |
| sci1_irq_cnt | build/host_tests/conf_board_self_1000/conf.h | 99 |  |  |  | uint32_t sci1_irq_cnt | SystemDebug_Snapshot, USART1_IRQHandler |  |
| sci2_irq_cnt | build/host_tests/conf_board_self_1000/conf.h | 101 |  |  |  | uint32_t sci2_irq_cnt | USART2_IRQHandler |  |
| sci3_irq_cnt | build/host_tests/conf_board_self_1000/conf.h | 102 |  |  |  | uint32_t sci3_irq_cnt | USART3_IRQHandler |  |
| cnt_PA0_irq | build/host_tests/conf_board_self_1000/conf.h | 103 |  |  |  | uint16_t cnt_PA0_irq | EXTI0_IRQHandler | SystemDebug_Snapshot |
| cnt_bms1_keyirq | build/host_tests/conf_board_self_1000/conf.h | 105 |  |  |  | uint16_t cnt_bms1_keyirq | EXTI9_5_IRQHandler | SystemDebug_Snapshot |
| pec_err_cnt | build/host_tests/conf_board_self_1000/conf.h | 106 |  |  |  | uint16_t pec_err_cnt |  |  |
| CHG | build/host_tests/conf_board_self_1000/conf.h | 107 |  |  |  | uint16_t CHG |  | DataLoad_Current |
| DSG | build/host_tests/conf_board_self_1000/conf.h | 109 |  |  |  | uint16_t DSG |  | DataLoad_Current |
| cnt_enter_chg_open | build/host_tests/conf_board_self_1000/conf.h | 110 |  |  |  | uint16_t cnt_enter_chg_open |  |  |
| cnt_enter_dsg_open | build/host_tests/conf_board_self_1000/conf.h | 112 |  |  |  | uint16_t cnt_enter_dsg_open |  |  |
| wakeup_reason | build/host_tests/conf_board_self_1000/conf.h | 113 |  |  |  | uint8_t wakeup_reason |  |  |
| wakeup_rtc | build/host_tests/conf_board_self_1000/conf.h | 115 |  |  |  | bool wakeup_rtc | InitRunAfterStopWakeup, Monitor_TempBreak |  |
| time_enter_rtc | build/host_tests/conf_board_self_1000/conf.h | 116 |  |  |  | uint8_t time_enter_rtc |  | lp_idle, lp_sync |
| power_on | build/host_tests/conf_board_self_1000/conf.h | 117 |  |  |  | bool power_on | Can_GetDebugSnapshot | DbgPrint_CAN, DbgPrint_Summary, SystemDebug_Snapshot |
| enter_rtc_delay | build/host_tests/conf_board_self_1000/conf.h | 118 |  |  |  | uint16_t enter_rtc_delay |  |  |
| rtc_sleep_cnt | build/host_tests/conf_board_self_1000/conf.h | 120 |  |  |  | uint32_t rtc_sleep_cnt | SystemDebug_Snapshot |  |
| rtc_sec_cnt | build/host_tests/conf_board_self_1000/conf.h | 121 |  |  |  | uint32_t rtc_sec_cnt | RTC_IRQHandler, SystemDebug_Snapshot |  |
| rtc_alm_cnt | build/host_tests/conf_board_self_1000/conf.h | 122 |  |  |  | uint32_t rtc_alm_cnt | RTC_HandleAlarmWakeup, SystemDebug_Snapshot |  |
| rtc_irq_cnt | build/host_tests/conf_board_self_1000/conf.h | 123 |  |  |  | uint32_t rtc_irq_cnt |  |  |
| isdebugenable | build/host_tests/conf_board_self_1000/conf.h | 124 |  |  |  | uint8_t isdebugenable | DataLoad_Current |  |
| typec_curr_sim | build/host_tests/conf_board_self_1000/conf.h | 126 |  |  |  | bool typec_curr_sim |  | ADC_GetTypeCOutCurrentMilliAmp |
| typc_curr | build/host_tests/conf_board_self_1000/conf.h | 127 |  |  |  | uint16_t typc_curr |  | ADC_GetTypeCOutCurrentMilliAmp |
| sys_time | build/host_tests/conf_board_self_1000/conf.h | 129 |  | extern |  | extern Time_T sys_time |  |  |
| last_ext_comm_cnt_can | build/host_tests/conf_board_self_30/conf.h | 94 |  |  |  | uint32_t last_ext_comm_cnt_can | Can_IsBusy | Can_PeekBusy |
| can_rcv_cnt | build/host_tests/conf_board_self_30/conf.h | 95 |  |  |  | uint32_t can_rcv_cnt | SystemDebug_Snapshot, USB_LP_CAN1_RX0_IRQHandler | Can_IsBusy, Can_PeekBusy |
| test_main_cycle | build/host_tests/conf_board_self_30/conf.h | 96 |  |  |  | uint64_t test_main_cycle |  |  |
| App_AFEGet_cnt | build/host_tests/conf_board_self_30/conf.h | 97 |  |  |  | uint32_t App_AFEGet_cnt |  |  |
| App_SH367309_Monitor_cnt | build/host_tests/conf_board_self_30/conf.h | 98 |  |  |  | uint32_t App_SH367309_Monitor_cnt |  |  |
| sci1_irq_cnt | build/host_tests/conf_board_self_30/conf.h | 99 |  |  |  | uint32_t sci1_irq_cnt | SystemDebug_Snapshot, USART1_IRQHandler |  |
| sci2_irq_cnt | build/host_tests/conf_board_self_30/conf.h | 101 |  |  |  | uint32_t sci2_irq_cnt | USART2_IRQHandler |  |
| sci3_irq_cnt | build/host_tests/conf_board_self_30/conf.h | 102 |  |  |  | uint32_t sci3_irq_cnt | USART3_IRQHandler |  |
| cnt_PA0_irq | build/host_tests/conf_board_self_30/conf.h | 103 |  |  |  | uint16_t cnt_PA0_irq | EXTI0_IRQHandler | SystemDebug_Snapshot |
| cnt_bms1_keyirq | build/host_tests/conf_board_self_30/conf.h | 105 |  |  |  | uint16_t cnt_bms1_keyirq | EXTI9_5_IRQHandler | SystemDebug_Snapshot |
| pec_err_cnt | build/host_tests/conf_board_self_30/conf.h | 106 |  |  |  | uint16_t pec_err_cnt |  |  |
| CHG | build/host_tests/conf_board_self_30/conf.h | 107 |  |  |  | uint16_t CHG |  | DataLoad_Current |
| DSG | build/host_tests/conf_board_self_30/conf.h | 109 |  |  |  | uint16_t DSG |  | DataLoad_Current |
| cnt_enter_chg_open | build/host_tests/conf_board_self_30/conf.h | 110 |  |  |  | uint16_t cnt_enter_chg_open |  |  |
| cnt_enter_dsg_open | build/host_tests/conf_board_self_30/conf.h | 112 |  |  |  | uint16_t cnt_enter_dsg_open |  |  |
| wakeup_reason | build/host_tests/conf_board_self_30/conf.h | 113 |  |  |  | uint8_t wakeup_reason |  |  |
| wakeup_rtc | build/host_tests/conf_board_self_30/conf.h | 115 |  |  |  | bool wakeup_rtc | InitRunAfterStopWakeup, Monitor_TempBreak |  |
| time_enter_rtc | build/host_tests/conf_board_self_30/conf.h | 116 |  |  |  | uint8_t time_enter_rtc |  | lp_idle, lp_sync |
| power_on | build/host_tests/conf_board_self_30/conf.h | 117 |  |  |  | bool power_on | Can_GetDebugSnapshot | DbgPrint_CAN, DbgPrint_Summary, SystemDebug_Snapshot |
| enter_rtc_delay | build/host_tests/conf_board_self_30/conf.h | 118 |  |  |  | uint16_t enter_rtc_delay |  |  |
| rtc_sleep_cnt | build/host_tests/conf_board_self_30/conf.h | 120 |  |  |  | uint32_t rtc_sleep_cnt | SystemDebug_Snapshot |  |
| rtc_sec_cnt | build/host_tests/conf_board_self_30/conf.h | 121 |  |  |  | uint32_t rtc_sec_cnt | RTC_IRQHandler, SystemDebug_Snapshot |  |
| rtc_alm_cnt | build/host_tests/conf_board_self_30/conf.h | 122 |  |  |  | uint32_t rtc_alm_cnt | RTC_HandleAlarmWakeup, SystemDebug_Snapshot |  |
| rtc_irq_cnt | build/host_tests/conf_board_self_30/conf.h | 123 |  |  |  | uint32_t rtc_irq_cnt |  |  |
| isdebugenable | build/host_tests/conf_board_self_30/conf.h | 124 |  |  |  | uint8_t isdebugenable | DataLoad_Current |  |
| typec_curr_sim | build/host_tests/conf_board_self_30/conf.h | 126 |  |  |  | bool typec_curr_sim |  | ADC_GetTypeCOutCurrentMilliAmp |
| typc_curr | build/host_tests/conf_board_self_30/conf.h | 127 |  |  |  | uint16_t typc_curr |  | ADC_GetTypeCOutCurrentMilliAmp |
| sys_time | build/host_tests/conf_board_self_30/conf.h | 129 |  | extern |  | extern Time_T sys_time |  |  |
| s_can_bitrate | firmware/comm_tool_f103ret6/source/app/ct_app.c | 12 | static |  |  | static uint32_t s_can_bitrate = CT_CAN_DEFAULT_BITRATE | handle_set_can | handle_info |
| s_node_id | firmware/comm_tool_f103ret6/source/app/ct_app.c | 13 | static |  |  | static uint8_t s_node_id = CT_NODE_ID_DEFAULT | handle_set_can | handle_can_diag, handle_info, handle_upgrade |
| s_app_can_addr | firmware/comm_tool_f103ret6/source/app/ct_app.c | 14 | static |  |  | static uint8_t s_app_can_addr = 0u | handle_set_can | handle_bms_aging_ctrl, handle_bms_aging_set_hours, handle_bms_read, handle_bms_write, handle_can_diag, handle_enter_iap, handle_info, handle_upgrade |
| ide | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 8 |  |  |  | uint8_t ide | CtCan_IapPollAck, CtCan_IapSendData, CtCan_IapWaitAck, can_rx_push, can_send_ack, send_app_cmd, send_iap_ctrl | CtBoard_CanSend, CtCan_ReadFactoryAgingBroadcast, can_decode_request, decode_app_ack, decode_app_word_frame |
| dlc | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 9 |  |  |  | uint8_t dlc | CtCan_IapSendData, can_rx_push, can_send_ack, send_app_cmd, send_iap_ctrl | CtBoard_CanSend, CtCan_IapPollAck, CtCan_IapWaitAck, CtCan_ReadFactoryAgingBroadcast, can_decode_request, decode_app_ack, decode_app_word_frame |
| data | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 10 |  |  |  | uint8_t data[8] | CanFeidao_PutU16Be, CanFeidao_PutU32Be, CanFeidao_SendCap5000ms, CanFeidao_SendFactoryTime5000ms, CanFeidao_SendSoc1000ms, CanFeidao_SendSoh5000ms, CanFeidao_SendStatus5000ms, CanFeidao_SendVersion5000ms, CanFeidao_SendVoltageCurrent1000ms, CtBoard_UartWrite | CanFeidao_SendFrame, CtBoard_CanSend, CtCan_IapSendData, CtCan_ReadFactoryAgingBroadcast, CtCrc16_Calc, CtFlash_Write, EEPROM_ApplyRWParamData, EEPROM_LoadRWParametersFromFlash, EEPROM_RWParamDataIsValid, EEPROM_SaveRWParametersToFlash |
| tx_ok | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 16 |  |  |  | uint32_t tx_ok | CtBoard_CanSend | handle_can_diag |
| tx_fail | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 17 |  |  |  | uint32_t tx_fail | CtBoard_CanSend | handle_can_diag |
| tx_timeout | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 18 |  |  |  | uint32_t tx_timeout | CtBoard_CanSend | handle_can_diag |
| rx_count | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 19 |  |  |  | uint32_t rx_count | can_rx_push | handle_can_diag |
| rx_drop | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 20 |  |  |  | uint32_t rx_drop | can_rx_push | handle_can_diag |
| last_esr | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 21 |  |  |  | uint32_t last_esr | can_diag_latch_regs | handle_can_diag |
| last_tsr | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 22 |  |  |  | uint32_t last_tsr | can_diag_latch_regs | handle_can_diag |
| last_msr | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 23 |  |  |  | uint32_t last_msr | can_diag_latch_regs | handle_can_diag |
| last_rf0r | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 24 |  |  |  | uint32_t last_rf0r | can_diag_latch_regs | handle_can_diag |
| last_tx_id | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 25 |  |  |  | uint32_t last_tx_id | CtBoard_CanSend | handle_can_diag |
| last_rx_id | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 26 |  |  |  | uint32_t last_rx_id | can_rx_push | handle_can_diag |
| last_tx_ide | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 27 |  |  |  | uint8_t last_tx_ide | CtBoard_CanSend | handle_can_diag |
| last_tx_dlc | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 28 |  |  |  | uint8_t last_tx_dlc | CtBoard_CanSend | handle_can_diag |
| last_tx_status | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 29 |  |  |  | uint8_t last_tx_status | CtBoard_CanSend | handle_can_diag |
| last_rx_ide | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 30 |  |  |  | uint8_t last_rx_ide | can_rx_push | handle_can_diag |
| last_rx_dlc | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 31 |  |  |  | uint8_t last_rx_dlc | can_rx_push | handle_can_diag |
| last_rx_data | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 32 |  |  |  | uint8_t last_rx_data[8] |  | can_rx_push, handle_can_diag |
| magic_inv | firmware/comm_tool_f103ret6/source/app/ct_boot_control.h | 8 |  |  |  | uint32_t magic_inv | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid |
| request | firmware/comm_tool_f103ret6/source/app/ct_boot_control.h | 9 |  |  |  | uint32_t request | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, AppUpgrade_MailboxCrc, boot_crc, boot_request_valid, legacy_send_read_ack, legacy_send_write_ack, serial_send_ack, serial_send_error, serial_send_read_regs |
| request_inv | firmware/comm_tool_f103ret6/source/app/ct_boot_control.h | 10 |  |  |  | uint32_t request_inv | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid |
| crc | firmware/comm_tool_f103ret6/source/app/ct_boot_control.h | 11 |  |  |  | uint32_t crc | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, CtCrc16_Update, CtProtocol_Encode, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, boot_consume_iap_request, can_handle_end, can_handle_start | AppUpgrade_IsIapRequested, boot_request_valid |
| s_last_gateway_error | firmware/comm_tool_f103ret6/source/app/ct_can_gateway.c | 22 | static |  |  | static uint8_t s_last_gateway_error | CtCan_AppReadRegs | CtCan_GetLastGatewayError |
| tick_ms | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 7 |  |  |  | uint32_t tick_ms | CtDebugLog_Record | CtDebugLog_EncodeLatest |
| module | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 8 |  |  |  | uint8_t module | CtDebugLog_Record, SystemDebug_ModuleApplyState, SystemDebug_ModuleBuildStaleMask, SystemDebug_ModuleHeartbeat, SystemDebug_RefreshModuleStates | CtDebugLog_EncodeLatest, SystemDebug_ModuleItem, SystemDebug_ModuleMask |
| event | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 9 |  |  |  | uint8_t event | CtDebugLog_Record, LogEvent_EEPROM, LogRecord_CanSaveEvent, SystemDebug_Event | CtDebugLog_EncodeLatest, LogEvent_Record, LogRecord_IsEntryValid, LogRecord_MarkEventSaved, SystemDebug_ReadEventRing |
| value0 | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 10 |  |  |  | uint16_t value0 | CtDebugLog_Record, feidao_can_fill_aging_ack, feidao_can_handle_app_cmd_data | CtDebugLog_EncodeLatest, feidao_can_app_send_ack |
| value1 | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 11 |  |  |  | uint16_t value1 | CtDebugLog_Record, feidao_can_fill_aging_ack, feidao_can_handle_app_cmd_data | CtDebugLog_EncodeLatest, feidao_can_app_send_ack |
| s_next_seq | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 16 | static |  |  | static uint16_t s_next_seq | CtDebugLog_Clear, CtDebugLog_Record |  |
| s_dropped | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 17 | static |  |  | static uint16_t s_dropped | CtDebugLog_Clear, CtDebugLog_Record | CtDebugLog_EncodeLatest |
| s_head | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 18 | static |  |  | static uint8_t s_head | CtDebugLog_Clear, CtDebugLog_Record | CtDebugLog_EncodeLatest |
| s_count | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 19 | static |  |  | static uint8_t s_count | CtDebugLog_Clear, CtDebugLog_Record | CtDebugLog_EncodeLatest |
| app_addr | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 8 |  |  |  | uint32_t app_addr | CtFlash_Begin, CtUpgrade_StartWithAppAddress, app_addr_supported, handle_fw_begin | handle_fw_info, valid_app_vector |
| size | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 9 |  |  |  | uint32_t size | CtFlash_Begin, CtUpgrade_StartWithAppAddress, can_handle_start, handle_fw_begin, handle_fw_end, soc_ocv_table | CtCan_IapSendStart, CtFlash_End, CtUpgrade_Task, app_addr_supported, handle_fw_info, load_next_block, soc_ocv_percent, soc_table_percent |
| crc16 | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 10 |  |  |  | uint16_t crc16 | CtFlash_Begin, handle_fw_begin, handle_fw_end | CtCan_IapSendEnd, CtCan_IapSendStart, CtFlash_End, CtUpgrade_Task, handle_fw_info |
| reserved | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 11 |  |  |  | uint16_t reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| crc32 | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 12 |  |  |  | uint32_t crc32 | CtFlash_Begin, handle_fw_begin, handle_fw_end | CtFlash_End, handle_fw_info |
| valid | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 13 |  |  |  | uint32_t valid | CtFlash_End, CtFlash_Init, CtUpgrade_StartWithAppAddress, StorageFlash_SaveJournalPage, boot_consume_iap_request, soc_load_or_default | handle_fw_info |
| flags | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 32 |  |  |  | uint8_t flags | CtProtocol_Feed, LogEvent_Record, LogRecord_RequestSleep, LogRecord_RequestStartup | App_LogRecord, CtProtocol_Encode, host_set_snapshot |
| seq | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 33 |  |  |  | uint16_t seq | CtDebugLog_Record, CtProtocol_Feed, IrqDebug_RecordEvent, can_handle_data, decode_app_word_frame, send_data_frame | CtCan_AppReadRegs, CtCan_IapSendData, CtDebugLog_EncodeLatest, CtProtocol_Encode, CtUpgrade_Task, feidao_can_app_send_word_frame, iap_data_id, respond |
| cmd | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 34 |  |  |  | uint8_t cmd | CtCan_AppAgingControl, CtProtocol_Feed, CtSelfIap_PollCan, can_decode_request, can_handle_ctrl, feidao_can_handle_app_cmd_data | CtApp_HandleFrame, CtCan_IapPollAck, CtCan_IapWaitAck, CtProtocol_Encode, LowPower_ConfigWakeupExti, can_build_ack, can_build_nack, can_send_ack, can_send_nack, command_allowed_during_upgrade |
| status | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 35 |  |  |  | uint8_t status | CAN_OperatingModeRequest, CtApp_Poll, CtProtocol_Feed, DebugWatch_BindAll, FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseAllBank1Pages, FLASH_EraseAllBank2Pages, FLASH_EraseAllPages, FLASH_EraseOptionBytes | CtProtocol_Encode, can_build_ack, can_send_ack, feidao_can_app_send_ack, respond |
| length | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 36 |  |  |  | uint16_t length | CtBoard_UartWrite, CtDebugLog_EncodeLatest, CtFlash_Read, CtFlash_Write, CtProtocol_Feed, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear, LedBar_Scan1ms | APP_LedBar, CanFeidao_SendFrame, CtApp_HandleFrame, CtCrc16_Calc, CtCrc16_Update, FlashProgramBytesVerified, LedBar_BuildGreedyFrameFromStart, LedBar_FrameEquals, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot |
| payload | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 37 |  |  |  | uint8_t payload[CT_UART_MAX_PAYLOAD] | CtProtocol_Feed, handle_bms_aging_status, handle_can_diag, handle_fw_info, handle_info, handle_upgrade_status, serial_write_block | CtProtocol_Encode, StorageFlash_LoadJournalPage, StorageFlash_LoadJournalPair, StorageFlash_LoadPair, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, StorageFlash_SaveJournalPage, StorageFlash_SaveJournalPair, StorageFlash_SavePair, StorageFlash_WriteSlot |
| index | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 43 |  |  |  | uint16_t index | CRC_CalcBlockCRC, CtDebugLog_EncodeLatest, CtProtocol_Feed, CtProtocol_Init, LedBar_FrameEquals, LedBar_FrameTransitionCost, LedBar_GetPinIndex, Sci_RecordBackIndex, feidao_can_queue_has_request, parser_restart_with_byte | ADC_GetRaw, ADC_GetResult, Sci_GetSocTableWord, Sci_GetWrValue, Sci_PutBytes, Sci_PutLatestFaultWords, Sci_PutWordBE, Sci_PutZeroWordsBE, SystemDebug_ReadEventRing |
| payload_length | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 44 |  |  |  | uint16_t payload_length | CtProtocol_Feed, CtProtocol_Init, parser_restart_with_byte | StorageFlash_RecordSpan |
| raw | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 45 |  |  |  | uint8_t raw[10u + CT_UART_MAX_PAYLOAD + 2u] | CtProtocol_Feed, InitADC, parser_restart_with_byte | ADC_GetRaw, ADC_UpdateMosTemp, ADC_UpdateTypeCCurrent, ADC_UpdateVbc, InitADC_DMA |
| frame | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 46 |  |  |  | CtFrame frame | CtApp_HandleFrame, CtBoard_CanSend, CtCan_IapPollAck, CtCan_IapSendData, CtCan_IapWaitAck, CtCan_ReadFactoryAgingBroadcast, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear | APP_LedBar, CtBoard_CanRecv, CtCan_AppReadRegs, CtSelfIap_PollCan, LedBar_BuildCurrentFrame, LedBar_BuildGreedyFrameFromStart, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot, LedBar_ImproveFrameOrder, LedBar_Init |
| s_legacy_index | firmware/comm_tool_f103ret6/source/app/ct_self_iap.c | 22 | static |  |  | static uint8_t s_legacy_index | CtSelfIap_FeedUartByte, legacy_reset_parser | legacy_check_frame_timeout |
| s_legacy_expect | firmware/comm_tool_f103ret6/source/app/ct_self_iap.c | 23 | static |  |  | static uint8_t s_legacy_expect | CtSelfIap_FeedUartByte, legacy_reset_parser | legacy_handle_frame |
| s_legacy_last_rx_ms | firmware/comm_tool_f103ret6/source/app/ct_self_iap.c | 24 | static |  |  | static uint32_t s_legacy_last_rx_ms | CtSelfIap_FeedUartByte | legacy_check_frame_timeout |
| s_reset_pending | firmware/comm_tool_f103ret6/source/app/ct_self_iap.c | 25 | static |  |  | static uint8_t s_reset_pending | CtSelfIap_Init, CtSelfIap_Task, iap_task_1ms, schedule_reset |  |
| s_reset_time_ms | firmware/comm_tool_f103ret6/source/app/ct_self_iap.c | 26 | static |  |  | static uint32_t s_reset_time_ms | schedule_reset | CtSelfIap_Task, iap_task_1ms |
| node | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 45 |  |  |  | uint8_t node | CtUpgrade_StartWithAppAddress, can_handle_hello, can_handle_rx, can_handle_start, can_reset_runtime | CtCan_IapPollAck, CtCan_IapSendCommit, CtCan_IapSendData, CtCan_IapSendEnd, CtCan_IapSendHello, CtCan_IapSendStart, CtCan_IapWaitAck, CtUpgrade_Start, CtUpgrade_Task, can_ack_id |
| app_can_addr | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 46 |  |  |  | uint8_t app_can_addr | CtUpgrade_StartWithAppAddress | CtUpgrade_Task |
| frame_index | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 47 |  |  |  | uint8_t frame_index | load_next_block, send_data_frame |  |
| frames_this_block | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 48 |  |  |  | uint8_t frames_this_block | load_next_block | send_data_frame |
| phase_start_ms | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 49 |  |  |  | uint32_t phase_start_ms | set_phase | CtUpgrade_Task, handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait |
| offset | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 50 |  |  |  | uint32_t offset | FlashErasePageVerified, FlashProgramBytesVerified, Sci_WrRegs_0x10_AFE_Parameters, Sci_WrRegs_0x10_OtherElement, Sci_WrRegs_0x10_Protect, StorageFlash_IsAreaBlank, StorageFlash_LoadJournalPage, System_ErrorField, finish_committed_block, handle_fw_data | CanFeidao_PutU16Be, CanFeidao_PutU32Be, CtFlash_Read, CtFlash_Write, Sci_ApplyOtherElementSideEffects, Sci_ApplyProtectSideEffects, Sci_RangeFits, Sci_WrValuesInRange, Sci_WriteWordsFromRequest, flash_range_valid |
| seq | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 51 |  |  |  | uint16_t seq | CtDebugLog_Record, CtProtocol_Feed, IrqDebug_RecordEvent, can_handle_data, decode_app_word_frame, send_data_frame | CtCan_AppReadRegs, CtCan_IapSendData, CtDebugLog_EncodeLatest, CtProtocol_Encode, CtUpgrade_Task, feidao_can_app_send_word_frame, iap_data_id, respond |
| block_seq | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 52 |  |  |  | uint16_t block_seq | can_handle_commit, finish_committed_block | CtCan_IapSendCommit, CtUpgrade_Task |
| chunk_len | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 53 |  |  |  | uint16_t chunk_len | load_next_block | CtUpgrade_Task, finish_committed_block |
| block_crc | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 54 |  |  |  | uint16_t block_crc | can_handle_commit, load_next_block | CtCan_IapSendCommit, CtUpgrade_Task |
| last_tx_ms | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 55 |  |  |  | uint32_t last_tx_ms | send_hello_and_mark | handle_fast_hello_wait, handle_iap_hello_wait |
| block | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 56 |  |  |  | uint8_t block[CT_IAP_BLOCK_BYTES] | SystemDebug_Snapshot, lp_deep, lp_select | DbgPrint_LP, DbgPrint_Summary, DebugHooks_RuntimeRecordEvents, can_handle_commit, can_handle_data, load_next_block, send_data_frame |
| s_status | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 58 | static |  |  | static CtUpgradeStatus s_status | CtUpgrade_Abort, CtUpgrade_Init, CtUpgrade_StartWithAppAddress, CtUpgrade_Task, finish_committed_block, set_error | CtUpgrade_GetStatus, handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait, set_phase |
| s_ctx | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 60 | static |  |  | static CtUpgradeContext s_ctx | CtUpgrade_Abort, CtUpgrade_StartWithAppAddress, CtUpgrade_Task, finish_committed_block, load_next_block, reset_context, send_hello_and_mark, set_error, set_phase | handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait, send_data_frame |
| s_abort | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 61 | static |  |  | static uint8_t s_abort | CtUpgrade_Abort, CtUpgrade_Init, CtUpgrade_StartWithAppAddress | CtUpgrade_Task |
| percent | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 8 |  |  |  | uint8_t percent | CtUpgrade_StartWithAppAddress, CtUpgrade_Task, finish_committed_block | CtUpgrade_Abort, handle_upgrade_status, set_error, set_phase |
| last_error | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 9 |  |  |  | uint8_t last_error | can_send_ack, can_send_nack, iap_task_1ms, set_error | CtUpgrade_Abort, CtUpgrade_Task, handle_upgrade_status |
| reserved | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 10 |  |  |  | uint8_t reserved | EEPROM_BuildRWParamData, IrqDebug_RecordEvent, StorageFlash_ProgramRecord |  |
| written | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 11 |  |  |  | uint32_t written | can_handle_commit, finish_committed_block | can_handle_end, handle_upgrade_status |
| total | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 12 |  |  |  | uint32_t total | CtProtocol_Encode, CtUpgrade_StartWithAppAddress | IrqDebug_CountFast, Sci_RangeFits, SystemDebug_SnapshotMcuResources, finish_committed_block, handle_upgrade_status |
| expect_seq | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 13 |  |  |  | uint16_t expect_seq | CtCan_IapPollAck, CtCan_IapWaitAck, can_handle_data | can_build_ack, can_build_nack, can_handle_end, handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait, handle_upgrade_status |
| s_rx_tail | firmware/comm_tool_f103ret6/source/bsp/board_can.c | 12 | static |  | volatile | static volatile uint8_t s_rx_tail | BoardUart_Init, BoardUart_ReadByte, can_hw_init, can_rx_pop | can_rx_push, rx_push |
| s_rx_queue | firmware/comm_tool_f103ret6/source/bsp/board_can.c | 13 | static |  |  | static CtCanFrame s_rx_queue[BOARD_CAN_RX_QUEUE_SIZE] |  | can_rx_pop, can_rx_push |
| s_diag | firmware/comm_tool_f103ret6/source/bsp/board_can.c | 14 | static |  |  | static CtCanDiag s_diag | CtBoard_CanSend, can_diag_latch_regs, can_rx_push | CtBoard_CanClearDiag, CtBoard_CanGetDiag |
| s_rx_tail | firmware/comm_tool_f103ret6/source/bsp/board_uart.c | 13 | static |  | volatile | static volatile uint16_t s_rx_tail | BoardUart_Init, BoardUart_ReadByte, can_hw_init, can_rx_pop | can_rx_push, rx_push |
| s_rx_buf | firmware/comm_tool_f103ret6/source/bsp/board_uart.c | 14 | static |  |  | static uint8_t s_rx_buf[BOARD_UART_RX_BUF_SIZE] | rx_push | BoardUart_ReadByte |
| s_tx_head | firmware/comm_tool_f103ret6/source/bsp/board_uart.c | 15 | static |  | volatile | static volatile uint16_t s_tx_head | BoardUart_Init, tx_empty, tx_push | tx_full, tx_irq_service |
| s_tx_tail | firmware/comm_tool_f103ret6/source/bsp/board_uart.c | 16 | static |  | volatile | static volatile uint16_t s_tx_tail | BoardUart_Init, tx_irq_service | tx_empty, tx_full |
| s_tx_buf | firmware/comm_tool_f103ret6/source/bsp/board_uart.c | 17 | static |  |  | static uint8_t s_tx_buf[BOARD_UART_TX_BUF_SIZE] | tx_push | tx_irq_service |
| magic_inv | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 80 |  |  |  | uint32_t magic_inv | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid |
| request | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 81 |  |  |  | uint32_t request | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, AppUpgrade_MailboxCrc, boot_crc, boot_request_valid, legacy_send_read_ack, legacy_send_write_ack, serial_send_ack, serial_send_error, serial_send_read_regs |
| request_inv | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 82 |  |  |  | uint32_t request_inv | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid |
| crc | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 83 |  |  |  | uint32_t crc | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, CtCrc16_Update, CtProtocol_Encode, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, boot_consume_iap_request, can_handle_end, can_handle_start | AppUpgrade_IsIapRequested, boot_request_valid |
| first_page_len | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 89 |  |  |  | uint16_t first_page_len | iap_flash_abort, iap_flash_begin, iap_flash_finish, iap_flash_write | vector_valid_in_buffer |
| page_erased | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 90 |  |  |  | uint8_t page_erased[IAP_FLASH_APP_PAGE_COUNT] | iap_flash_begin, iap_flash_ensure_page_erased | iap_flash_abort, iap_flash_finish |
| first_page | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 91 |  |  |  | uint8_t first_page[CT_SELF_FLASH_PAGE_SIZE] |  | iap_flash_begin, iap_flash_finish, iap_flash_write, vector_valid_in_buffer |
| node | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 97 |  |  |  | uint8_t node | CtUpgrade_StartWithAppAddress, can_handle_hello, can_handle_rx, can_handle_start, can_reset_runtime | CtCan_IapPollAck, CtCan_IapSendCommit, CtCan_IapSendData, CtCan_IapSendEnd, CtCan_IapSendHello, CtCan_IapSendStart, CtCan_IapWaitAck, CtUpgrade_Start, CtUpgrade_Task, can_ack_id |
| last_cmd | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 98 |  |  |  | uint8_t last_cmd | can_handle_ctrl |  |
| last_error | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 99 |  |  |  | uint8_t last_error | can_send_ack, can_send_nack, iap_task_1ms, set_error | CtUpgrade_Abort, CtUpgrade_Task, handle_upgrade_status |
| expect_seq | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 100 |  |  |  | uint16_t expect_seq | CtCan_IapPollAck, CtCan_IapWaitAck, can_handle_data | can_build_ack, can_build_nack, can_handle_end, handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait, handle_upgrade_status |
| block_seq | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 101 |  |  |  | uint16_t block_seq | can_handle_commit, finish_committed_block | CtCan_IapSendCommit, CtUpgrade_Task |
| block_bytes | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 102 |  |  |  | uint16_t block_bytes | can_handle_commit, can_handle_data |  |
| fw_crc | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 103 |  |  |  | uint16_t fw_crc | can_handle_start | can_handle_end |
| running_crc | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 104 |  |  |  | uint16_t running_crc | can_handle_commit, can_reset_runtime | can_handle_end |
| fw_size | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 105 |  |  |  | uint32_t fw_size | can_handle_start | can_handle_commit, can_handle_end |
| written | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 106 |  |  |  | uint32_t written | can_handle_commit, finish_committed_block | can_handle_end, handle_upgrade_status |
| last_rx_ms | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 107 |  |  |  | uint32_t last_rx_ms | can_handle_commit, can_handle_data, can_handle_start | iap_task_1ms |
| block | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 108 |  |  |  | uint8_t block[IAP_CAN_BLOCK_BYTES] | SystemDebug_Snapshot, lp_deep, lp_select | DbgPrint_LP, DbgPrint_Summary, DebugHooks_RuntimeRecordEvents, can_handle_commit, can_handle_data, load_next_block, send_data_frame |
| s_tick_ms | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 110 | static |  | volatile | static volatile uint32_t s_tick_ms | SysTick_Handler | Board_GetTickMs, can_handle_commit, can_handle_data, can_handle_start, iap_task_1ms, schedule_reset, serial_check_frame_timeout, serial_delay_ms, serial_feed |
| s_flash | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 112 | static |  |  | static IapFlashContext s_flash | StorageFlash_BeginWrite, StorageFlash_EndWrite, iap_flash_abort, iap_flash_begin, iap_flash_ensure_page_erased, iap_flash_finish, iap_flash_write, serial_write_block | Flash_DebugWatchBind, StorageFlash_IsBusy, serial_status_word, vector_valid_in_buffer |
| s_can | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 113 | static |  |  | static IapCanContext s_can | can_handle_commit, can_handle_ctrl, can_handle_data, can_handle_end, can_handle_hello, can_handle_rx, can_handle_start, can_reset_runtime, can_send_ack, can_send_nack | can_build_ack, can_build_nack, can_handle_abort |
| s_serial_rx | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 114 | static |  |  | static uint8_t s_serial_rx[IAP_SERIAL_RX_SIZE] | serial_feed, serial_handle_frame |  |
| s_serial_index | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 115 | static |  |  | static uint16_t s_serial_index | serial_feed, serial_reset_parser | serial_check_frame_timeout |
| s_serial_expect | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 116 | static |  |  | static uint16_t s_serial_expect | serial_feed, serial_reset_parser | serial_handle_frame |
| s_serial_block_count | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 117 | static |  |  | static uint16_t s_serial_block_count | serial_connect, serial_write_block | serial_status_word |
| s_serial_written | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 118 | static |  |  | static uint32_t s_serial_written | serial_complete, serial_connect, serial_write_block | serial_status_word |
| s_serial_last_rx_ms | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 119 | static |  |  | static uint32_t s_serial_last_rx_ms | serial_feed | serial_check_frame_timeout |
| s_serial_fault_count | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 120 | static |  |  | static uint8_t s_serial_fault_count | serial_record_fault, serial_send_error | serial_status_word |
| s_serial_irq_rx | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 121 | static |  | volatile | static volatile uint8_t s_serial_irq_rx[IAP_SERIAL_IRQ_RX_SIZE] | serial_irq_rx_push | serial_irq_rx_pop |
| s_serial_irq_head | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 122 | static |  | volatile | static volatile uint16_t s_serial_irq_head | serial_irq_rx_push, serial_irq_rx_reset | serial_irq_rx_pop |
| s_serial_irq_tail | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 123 | static |  | volatile | static volatile uint16_t s_serial_irq_tail | serial_irq_rx_pop, serial_irq_rx_reset | serial_irq_rx_push |
| s_serial_irq_tx | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 124 | static |  | volatile | static volatile uint8_t s_serial_irq_tx[IAP_SERIAL_IRQ_TX_SIZE] | serial_irq_tx_push | serial_irq_tx_service |
| s_serial_tx_head | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 125 | static |  | volatile | static volatile uint16_t s_serial_tx_head | serial_irq_tx_empty, serial_irq_tx_push, serial_irq_tx_reset | serial_irq_tx_full, serial_irq_tx_service |
| s_serial_tx_tail | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 126 | static |  | volatile | static volatile uint16_t s_serial_tx_tail | serial_irq_tx_reset, serial_irq_tx_service | serial_irq_tx_empty, serial_irq_tx_full |
| s_reset_pending | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 127 | static |  |  | static uint8_t s_reset_pending | CtSelfIap_Init, CtSelfIap_Task, iap_task_1ms, schedule_reset |  |
| s_reset_time_ms | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 128 | static |  |  | static uint32_t s_reset_time_ms | schedule_reset | CtSelfIap_Task, iap_task_1ms |
| s_frame | firmware/comm_tool_f103ret6/source/main.c | 7 | static |  |  | static CtFrame s_frame |  | main |
| System_ErrFlag | tools/soc_host_c_test.c | 18 |  |  | volatile | volatile struct SYSTEM_ERROR System_ErrFlag | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi |
| SeriesNum | tools/soc_host_c_test.c | 19 |  |  |  | UINT8 SeriesNum = 10U | EEPROM_UpdateOtherElementRuntime, Sci_ApplyOtherElementSideEffects, host_apply_default_config | DataLoad_CellVolt, DataLoad_CellVoltMaxMinFind, DebugWatch_BindAll, Refresh_Parameters, UpdateVoltageFromBqMaximo, host_init_with_voltage, host_tick, new_todo_logi |
| s_host_typec_out_current_mA | tools/soc_host_c_test.c | 21 | static |  |  | static UINT16 s_host_typec_out_current_mA | host_reset_state, test_typec_output_current_converts_to_battery_equivalent | ADC_GetTypeCOutCurrentMilliAmp |
| s_host_vbat_mV | tools/soc_host_c_test.c | 22 | static |  |  | static UINT32 s_host_vbat_mV | host_reset_state, host_tick | ADC_GetVbatMilliVolt |
| s_host_afe_current_sample_seq | tools/soc_host_c_test.c | 23 | static |  |  | static UINT32 s_host_afe_current_sample_seq | host_reset_state, host_tick | AfeCurrent_GetSeq |
| s_flash_soc | tools/soc_host_c_test.c | 24 | static |  |  | static STORAGE_FLASH_SOC_DATA s_flash_soc | StorageFlash_SaveSocData, host_set_snapshot | StorageFlash_LoadSocData, host_internal_soc, host_reset_state, test_rebound_flag_clears_when_holdoff_expires |
| s_flash_soc_valid | tools/soc_host_c_test.c | 26 | static |  |  | static UINT8 s_flash_soc_valid | StorageFlash_SaveSocData, host_reset_state, host_set_snapshot | StorageFlash_LoadSocData |
| s_failures | tools/soc_host_c_test.c | 27 | static |  |  | static unsigned s_failures | host_check, host_check_range_u32, host_check_u32 | main |
| s_host_feature | tools/soc_host_c_test.c | 28 | static |  | volatile | static volatile union System_OnOFF_Function s_host_feature | SystemFeature_SetById | SystemFeature_GetMask, SystemFeature_IsSocFixed, SystemFeature_IsSocZero, host_reset_state |
| System_ErrFlag | tools/soc_host_visual_trace.c | 14 |  |  | volatile | volatile struct SYSTEM_ERROR System_ErrFlag | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi |
| SeriesNum | tools/soc_host_visual_trace.c | 15 |  |  |  | UINT8 SeriesNum = 10U | EEPROM_UpdateOtherElementRuntime, Sci_ApplyOtherElementSideEffects, host_apply_default_config | DataLoad_CellVolt, DataLoad_CellVoltMaxMinFind, DebugWatch_BindAll, Refresh_Parameters, UpdateVoltageFromBqMaximo, host_init_with_voltage, host_tick, new_todo_logi |
| s_host_typec_out_current_mA | tools/soc_host_visual_trace.c | 17 | static |  |  | static UINT16 s_host_typec_out_current_mA | host_reset_state, test_typec_output_current_converts_to_battery_equivalent | ADC_GetTypeCOutCurrentMilliAmp |
| s_host_vbat_mV | tools/soc_host_visual_trace.c | 18 | static |  |  | static UINT32 s_host_vbat_mV | host_reset_state, host_tick | ADC_GetVbatMilliVolt |
| s_host_afe_current_sample_seq | tools/soc_host_visual_trace.c | 19 | static |  |  | static UINT32 s_host_afe_current_sample_seq | host_reset_state, host_tick | AfeCurrent_GetSeq |
| s_flash_soc | tools/soc_host_visual_trace.c | 20 | static |  |  | static STORAGE_FLASH_SOC_DATA s_flash_soc | StorageFlash_SaveSocData, host_set_snapshot | StorageFlash_LoadSocData, host_internal_soc, host_reset_state, test_rebound_flag_clears_when_holdoff_expires |
| s_flash_soc_valid | tools/soc_host_visual_trace.c | 22 | static |  |  | static UINT8 s_flash_soc_valid | StorageFlash_SaveSocData, host_reset_state, host_set_snapshot | StorageFlash_LoadSocData |
| s_host_feature | tools/soc_host_visual_trace.c | 23 | static |  | volatile | static volatile union System_OnOFF_Function s_host_feature | SystemFeature_SetById | SystemFeature_GetMask, SystemFeature_IsSocFixed, SystemFeature_IsSocZero, host_reset_state |
| seconds | tools/soc_host_visual_trace.c | 28 |  |  |  | UINT16 seconds | RTC_IsWakeupPeriodSafe, soc_update_display_soc, test_rtc_ocv_ignores_upward_stable_target, test_rtc_ocv_waits_for_voltage_convergence | LP_RecordLastSleepSeconds, RTC_SetWakeupPeriodSeconds, RtcSleep_PortAddRuntimeSeconds, host_run_scenario, host_run_seconds, host_self_delta_as10, soc_add_rest_seconds |
| idsg_a10 | tools/soc_host_visual_trace.c | 29 |  |  |  | UINT16 idsg_a10 |  | host_emit_row, host_pack_step, host_pack_voltage, host_run_scenario |
| ichg_a10 | tools/soc_host_visual_trace.c | 30 |  |  |  | UINT16 ichg_a10 |  | host_emit_row, host_pack_step, host_pack_voltage, host_run_scenario |
| imbalance_mv | tools/soc_host_visual_trace.c | 31 |  |  |  | UINT16 imbalance_mv |  | host_pack_voltage, host_run_scenario |
| start_soc | tools/soc_host_visual_trace.c | 37 |  |  |  | double start_soc |  | host_run_scenario |
| segments | tools/soc_host_visual_trace.c | 38 |  |  |  | const HOST_SEGMENT *segments |  | host_run_scenario |
| segment_count | tools/soc_host_visual_trace.c | 39 |  |  |  | UINT16 segment_count |  | host_run_scenario |
| repeats | tools/soc_host_visual_trace.c | 40 |  |  |  | UINT16 repeats |  | host_run_scenario |
| s_ocv_table | tools/soc_host_visual_trace.c | 42 | static |  |  | static const UINT16 s_ocv_table[] = { 4160, 100, 4100, 95, 4050, 90, 3995, 85, 3935, 80, 3880, 75, 3835, 70, 3795, 65, 3760, 60, 3725, 55, 3695, 50, 3670, 45, 3645, 40, 3615, 35, 3585, 30, 3555, 25, 3525, 20, 3480, 1... |  | host_voltage_from_soc |
| s_city_commute | tools/soc_host_visual_trace.c | 50 | static |  |  | static const HOST_SEGMENT s_city_commute[] = { {"", 60, 0, 0, 4}, {"", 300, 80, 0, 4}, {"", 120, 220, 0, 6}, {"", 300, 120, 0, 4}, {"", 60, 0, 0, 4}, {"", 180, 350, 0, 10}, {"", 300, 100, 0, 4}, } |  |  |
| s_hill_climb | tools/soc_host_visual_trace.c | 60 | static |  |  | static const HOST_SEGMENT s_hill_climb[] = { {"", 120, 120, 0, 4}, {"", 240, 420, 0, 12}, {"", 180, 0, 0, 4}, {"", 180, 100, 0, 4}, } |  |  |
| s_fast_current_pulses | tools/soc_host_visual_trace.c | 67 | static |  |  | static const HOST_SEGMENT s_fast_current_pulses[] = { {"", 1, 30, 0, 4}, {"", 1, 260, 0, 8}, {"", 1, 80, 0, 4}, {"", 1, 420, 0, 12}, {"", 1, 0, 0, 4}, {"", 1, 160, 0, 6}, {"", 1, 320, 0, 10}, {"", 1, 40, 0, 4}, } |  |  |
| s_deep_cutoff | tools/soc_host_visual_trace.c | 78 | static |  |  | static const HOST_SEGMENT s_deep_cutoff[] = { {"", 240, 120, 0, 6}, {"", 420, 180, 0, 8}, {"", 240, 220, 0, 10}, } |  |  |
| s_charge_anchor | tools/soc_host_visual_trace.c | 84 | static |  |  | static const HOST_SEGMENT s_charge_anchor[] = { {"", 600, 0, 270, 4}, {"", 20, 0, 270, 4}, } |  |  |


## 高耦合变量

| 变量 | 文件 | 行 | 引用热度 | 写入者 | 读取者 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| data | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 10 | 77 | CanFeidao_PutU16Be, CanFeidao_PutU32Be, CanFeidao_SendCap5000ms, CanFeidao_SendFactoryTime5000ms, CanFeidao_SendSoc1000ms, CanFeidao_SendSoh5000ms, CanFeidao_SendStatus5000ms, CanFeidao_SendVersion5000ms, CanFeidao_SendVoltageCurrent1000ms, CtBoard_UartWrite, CtCan_IapPollAck, CtCan_IapSendCommit | CanFeidao_SendFrame, CtBoard_CanSend, CtCan_IapSendData, CtCan_ReadFactoryAgingBroadcast, CtCrc16_Calc, CtFlash_Write, EEPROM_ApplyRWParamData, EEPROM_LoadRWParametersFromFlash, EEPROM_RWParamDataIsValid, EEPROM_SaveRWParametersToFlash, FactoryAging_LoadDurationFromData, FlashProgramBytesVerified | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| result | 103 + 309/Project/Source/ADC.c | 6 | 56 | ADC_ClearTypeCOutCurrent, ADC_UpdateMosTemp, ADC_UpdateTypeCCurrent, ADC_UpdateVbc, AFE_CheckStatus, AFE_IsReady, CtBoard_SetCanBitrate, FlashErasePageVerified, FlashProgramBytesVerified, FlashProgramHalfWordVerified, FlashWriteOneHalfWord, I2C_Init | ADC_GetResult, MonitorAFE_UpdateChannel | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| length | 103 + 309/Project/Source/Flash.c | 26 | 55 | CtBoard_UartWrite, CtDebugLog_EncodeLatest, CtFlash_Read, CtFlash_Write, CtProtocol_Feed, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear, LedBar_Scan1ms, Sci_CopyProductIdBytes, StorageFlash_CalcCrc | APP_LedBar, CanFeidao_SendFrame, CtApp_HandleFrame, CtCrc16_Calc, CtCrc16_Update, FlashProgramBytesVerified, LedBar_BuildGreedyFrameFromStart, LedBar_FrameEquals, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot, LedBar_ImproveFrameOrder, LedBar_IsActiveForLowPower | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| length | 103 + 309/Project/Source/LedBar.c | 75 | 55 | CtBoard_UartWrite, CtDebugLog_EncodeLatest, CtFlash_Read, CtFlash_Write, CtProtocol_Feed, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear, LedBar_Scan1ms, Sci_CopyProductIdBytes, StorageFlash_CalcCrc | APP_LedBar, CanFeidao_SendFrame, CtApp_HandleFrame, CtCrc16_Calc, CtCrc16_Update, FlashProgramBytesVerified, LedBar_BuildGreedyFrameFromStart, LedBar_FrameEquals, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot, LedBar_ImproveFrameOrder, LedBar_IsActiveForLowPower | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| length | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 36 | 55 | CtBoard_UartWrite, CtDebugLog_EncodeLatest, CtFlash_Read, CtFlash_Write, CtProtocol_Feed, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear, LedBar_Scan1ms, Sci_CopyProductIdBytes, StorageFlash_CalcCrc | APP_LedBar, CanFeidao_SendFrame, CtApp_HandleFrame, CtCrc16_Calc, CtCrc16_Update, FlashProgramBytesVerified, LedBar_BuildGreedyFrameFromStart, LedBar_FrameEquals, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot, LedBar_ImproveFrameOrder, LedBar_IsActiveForLowPower | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 544 | 49 | ADC_AnalogWatchdogCmd, ADC_AnalogWatchdogSingleChannelConfig, ADC_AutoInjectedConvCmd, ADC_DiscModeChannelCountConfig, ADC_DiscModeCmd, ADC_ITConfig, ADC_Init, ADC_InjectedDiscModeCmd, I2C_ARPCmd, I2C_AcknowledgeConfig, I2C_CalculatePEC, I2C_Cmd | ADC_GetITStatus, USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1241 | 49 | ADC_AnalogWatchdogCmd, ADC_AnalogWatchdogSingleChannelConfig, ADC_AutoInjectedConvCmd, ADC_DiscModeChannelCountConfig, ADC_DiscModeCmd, ADC_ITConfig, ADC_Init, ADC_InjectedDiscModeCmd, I2C_ARPCmd, I2C_AcknowledgeConfig, I2C_CalculatePEC, I2C_Cmd | ADC_GetITStatus, USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 545 | 47 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration, ADC_TempSensorVrefintCmd, FLASH_EraseAllBank2Pages | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 916 | 47 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration, ADC_TempSensorVrefintCmd, FLASH_EraseAllBank2Pages | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1031 | 47 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration, ADC_TempSensorVrefintCmd, FLASH_EraseAllBank2Pages | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1163 | 47 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration, ADC_TempSensorVrefintCmd, FLASH_EraseAllBank2Pages | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1189 | 47 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration, ADC_TempSensorVrefintCmd, FLASH_EraseAllBank2Pages | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1243 | 47 | ADC_Cmd, ADC_DMACmd, ADC_ExternalTrigConvCmd, ADC_ExternalTrigInjectedConvCmd, ADC_ExternalTrigInjectedConvConfig, ADC_Init, ADC_ResetCalibration, ADC_SoftwareStartConvCmd, ADC_SoftwareStartInjectedConvCmd, ADC_StartCalibration, ADC_TempSensorVrefintCmd, FLASH_EraseAllBank2Pages | ADC_GetCalibrationStatus, ADC_GetResetCalibrationStatus, ADC_GetSoftwareStartConvStatus, ADC_GetSoftwareStartInjectedConvCmdStatus, I2C_GetITStatus, SPI_I2S_GetITStatus, USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 594 | 41 | CRC_ResetDR, DAC_Cmd, DAC_DMACmd, DAC_ITConfig, DAC_Init, DAC_WaveGenerationCmd, DBGMCU_Config, EnableLowPowerDebug, FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseAllBank1Pages, FLASH_EraseAllPages | DAC_GetITStatus, RCC_GetFlagStatus, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 756 | 41 | CRC_ResetDR, DAC_Cmd, DAC_DMACmd, DAC_ITConfig, DAC_Init, DAC_WaveGenerationCmd, DBGMCU_Config, EnableLowPowerDebug, FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseAllBank1Pages, FLASH_EraseAllPages | DAC_GetITStatus, RCC_GetFlagStatus, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 790 | 41 | CRC_ResetDR, DAC_Cmd, DAC_DMACmd, DAC_ITConfig, DAC_Init, DAC_WaveGenerationCmd, DBGMCU_Config, EnableLowPowerDebug, FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseAllBank1Pages, FLASH_EraseAllPages | DAC_GetITStatus, RCC_GetFlagStatus, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 906 | 41 | CRC_ResetDR, DAC_Cmd, DAC_DMACmd, DAC_ITConfig, DAC_Init, DAC_WaveGenerationCmd, DBGMCU_Config, EnableLowPowerDebug, FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseAllBank1Pages, FLASH_EraseAllPages | DAC_GetITStatus, RCC_GetFlagStatus, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| soc | 103 + 309/Project/Source/SocEnhance.c | 105 | 38 | CanFeidao_SendSoc1000ms, CtCan_AppGetStatus, DebugWatch_BindAll, LedBar_LimitSoc, LedBar_LoadSleepSoc, SocEnhance_DebugWatchBind, SystemDebug_Snapshot, host_soc_from_cap, host_true_soc, host_voltage_from_soc, soc_add_discharge, soc_apply_discharge_delta | DbgPrint_SOC, DbgPrint_Summary, SOC_ApplyRtcRelaxationCompensation, SOC_RequestSetOnce, SystemDebug_ModuleItem, host_cap_now_from_soc, host_set_snapshot, soc_apply_full_empty, soc_apply_long_rest_down_step, soc_apply_ocv_target_step, soc_apply_tail_step, soc_display_target | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| soc | 103 + 309/Project/Source/SocEnhance.c | 124 | 38 | CanFeidao_SendSoc1000ms, CtCan_AppGetStatus, DebugWatch_BindAll, LedBar_LimitSoc, LedBar_LoadSleepSoc, SocEnhance_DebugWatchBind, SystemDebug_Snapshot, host_soc_from_cap, host_true_soc, host_voltage_from_soc, soc_add_discharge, soc_apply_discharge_delta | DbgPrint_SOC, DbgPrint_Summary, SOC_ApplyRtcRelaxationCompensation, SOC_RequestSetOnce, SystemDebug_ModuleItem, host_cap_now_from_soc, host_set_snapshot, soc_apply_full_empty, soc_apply_long_rest_down_step, soc_apply_ocv_target_step, soc_apply_tail_step, soc_display_target | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| frame | 103 + 309/Project/Source/LedBar.c | 86 | 36 | CtApp_HandleFrame, CtBoard_CanSend, CtCan_IapPollAck, CtCan_IapSendData, CtCan_IapWaitAck, CtCan_ReadFactoryAgingBroadcast, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear, LedBar_Scan1ms, LedBar_SwapFrameRoutes | APP_LedBar, CtBoard_CanRecv, CtCan_AppReadRegs, CtSelfIap_PollCan, LedBar_BuildCurrentFrame, LedBar_BuildGreedyFrameFromStart, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot, LedBar_ImproveFrameOrder, LedBar_Init, LedBar_IsActiveForLowPower, LedBar_RefreshOutput | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| frame | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 46 | 36 | CtApp_HandleFrame, CtBoard_CanSend, CtCan_IapPollAck, CtCan_IapSendData, CtCan_IapWaitAck, CtCan_ReadFactoryAgingBroadcast, LedBar_ApplyFrame, LedBar_BuildFrameFromMask, LedBar_FrameAddRoute, LedBar_FrameClear, LedBar_Scan1ms, LedBar_SwapFrameRoutes | APP_LedBar, CtBoard_CanRecv, CtCan_AppReadRegs, CtSelfIap_PollCan, LedBar_BuildCurrentFrame, LedBar_BuildGreedyFrameFromStart, LedBar_FrameTransitionCost, LedBar_GetDebugSnapshot, LedBar_ImproveFrameOrder, LedBar_Init, LedBar_IsActiveForLowPower, LedBar_RefreshOutput | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 905 | 35 | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag, USART_ClearITPendingBit, WWDG_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus, SPI_I2S_GetITStatus, Sci_PortArmReceiver | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1058 | 35 | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag, USART_ClearITPendingBit, WWDG_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus, SPI_I2S_GetITStatus, Sci_PortArmReceiver | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1165 | 35 | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag, USART_ClearITPendingBit, WWDG_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus, SPI_I2S_GetITStatus, Sci_PortArmReceiver | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1195 | 35 | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag, USART_ClearITPendingBit, WWDG_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus, SPI_I2S_GetITStatus, Sci_PortArmReceiver | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1259 | 35 | ADC_ClearFlag, ADC_ClearITPendingBit, DAC_ClearFlag, DAC_ClearITPendingBit, FLASH_ClearFlag, SPI_I2S_ClearFlag, SPI_I2S_ClearITPendingBit, TIM_ClearFlag, TIM_ClearITPendingBit, USART_ClearFlag, USART_ClearITPendingBit, WWDG_ClearFlag | ADC_GetFlagStatus, ADC_GetITStatus, BOARD_UART_IRQHandler, DAC_GetFlagStatus, DAC_GetITStatus, FLASH_GetBank1Status, FLASH_GetFlagStatus, FLASH_GetStatus, IWDG_GetFlagStatus, SPI_I2S_GetFlagStatus, SPI_I2S_GetITStatus, Sci_PortArmReceiver | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| count | 103 + 309/Project/Source/Can_HDX.c | 62 | 34 | CtCan_AppReadRegs, CtCan_AppWriteRegs, CtDebugLog_EncodeLatest, Sci_PutZeroWordsBE, Sci_RangeFits, SystemDebug_Event, feidao_can_clear_tx_queue, feidao_can_dequeue_tx, feidao_can_enqueue_tx, handle_bms_read, handle_bms_write, legacy_handle_frame | Can_GetDebugSnapshot, DbgPrint_EventRing, EEPROM_WordBlockInRange, Sci_ApplyOtherElementSideEffects, Sci_ApplyProtectSideEffects, Sci_CopyWords, Sci_PutBytes, Sci_RangeOverlaps, Sci_WrValuesInRange, Sci_WriteWordsFromRequest, SystemDebug_ReadEventRing, can_has_pending_work | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| count | 103 + 309/Project/Source/SystemDebug.c | 57 | 34 | CtCan_AppReadRegs, CtCan_AppWriteRegs, CtDebugLog_EncodeLatest, Sci_PutZeroWordsBE, Sci_RangeFits, SystemDebug_Event, feidao_can_clear_tx_queue, feidao_can_dequeue_tx, feidao_can_enqueue_tx, handle_bms_read, handle_bms_write, legacy_handle_frame | Can_GetDebugSnapshot, DbgPrint_EventRing, EEPROM_WordBlockInRange, Sci_ApplyOtherElementSideEffects, Sci_ApplyProtectSideEffects, Sci_CopyWords, Sci_PutBytes, Sci_RangeOverlaps, Sci_WrValuesInRange, Sci_WriteWordsFromRequest, SystemDebug_ReadEventRing, can_has_pending_work | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_soc | 103 + 309/Project/Source/SocEnhance.c | 141 | 33 | SOC_IntEnhance_Ctrl, soc_add_discharge, soc_apply_discharge_delta, soc_apply_full_empty, soc_apply_long_rest_down_step, soc_apply_rtc_rest_ocv, soc_clear_rest_down_target, soc_from_cap, soc_handle_command, soc_integrate, soc_load_or_default, soc_param_lib_init | SOC_ApplyRtcRelaxationCompensation, SOC_GetDebugInternals, SocEnhance_DebugWatchBind, soc_apply_ocv_target_step, soc_apply_tail_step, soc_display_target, soc_export_public_fields, soc_full_confirm_seconds, soc_sag_hold_blocks_calibration, soc_save, soc_save_mark_changed, soc_update_save_mark | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| status | 103 + 309/Project/Source/DebugWatch.h | 115 | 32 | CAN_OperatingModeRequest, CtApp_Poll, CtProtocol_Feed, DebugWatch_BindAll, FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseAllBank1Pages, FLASH_EraseAllBank2Pages, FLASH_EraseAllPages, FLASH_EraseOptionBytes, FLASH_ErasePage, FLASH_ProgramHalfWord | CtProtocol_Encode, can_build_ack, can_send_ack, feidao_can_app_send_ack, respond | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| status | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 35 | 32 | CAN_OperatingModeRequest, CtApp_Poll, CtProtocol_Feed, DebugWatch_BindAll, FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseAllBank1Pages, FLASH_EraseAllBank2Pages, FLASH_EraseAllPages, FLASH_EraseOptionBytes, FLASH_ErasePage, FLASH_ProgramHalfWord | CtProtocol_Encode, can_build_ack, can_send_ack, feidao_can_app_send_ack, respond | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| Data | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 161 | 29 | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, TwiSendData, feidao_can_app_send_ack, feidao_can_app_send_word_frame | BKP_WriteBackupRegister, CAN_Transmit, CEC_SendDataByte, CRC_CalcCRC, DAC_SetChannel1Data, DAC_SetChannel2Data, FLASH_ProgramHalfWord, FLASH_ProgramOptionByteData, FLASH_ProgramWord, I2C_SendData, SDIO_WriteData, SPI_I2S_SendData | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| Data | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 189 | 29 | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, TwiSendData, feidao_can_app_send_ack, feidao_can_app_send_word_frame | BKP_WriteBackupRegister, CAN_Transmit, CEC_SendDataByte, CRC_CalcCRC, DAC_SetChannel1Data, DAC_SetChannel2Data, FLASH_ProgramHalfWord, FLASH_ProgramOptionByteData, FLASH_ProgramWord, I2C_SendData, SDIO_WriteData, SPI_I2S_SendData | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| offset | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 50 | 28 | FlashErasePageVerified, FlashProgramBytesVerified, Sci_WrRegs_0x10_AFE_Parameters, Sci_WrRegs_0x10_OtherElement, Sci_WrRegs_0x10_Protect, StorageFlash_IsAreaBlank, StorageFlash_LoadJournalPage, System_ErrorField, finish_committed_block, handle_fw_data, legacy_status_word, serial_status_word | CanFeidao_PutU16Be, CanFeidao_PutU32Be, CtFlash_Read, CtFlash_Write, Sci_ApplyOtherElementSideEffects, Sci_ApplyProtectSideEffects, Sci_RangeFits, Sci_WrValuesInRange, Sci_WriteWordsFromRequest, flash_range_valid, iap_flash_program_direct, iap_flash_write | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_ledbar | 103 + 309/Project/Source/LedBar.c | 159 | 27 | APP_LedBar, LedBar_ApplyFrame, LedBar_Clear, LedBar_EnsureInit, LedBar_Init, LedBar_IsActiveForLowPower, LedBar_PrepareForStop, LedBar_RequestSocDisplayWindow, LedBar_RequestStartupDisplayWindow, LedBar_Scan1ms, LedBar_ScanTimerInit, LedBar_ServiceMcuWakeFilter | LedBar_BuildCurrentFrame, LedBar_DebugWatchBind, LedBar_GetDebugSnapshot, LedBar_IsDisplayRequested, LedBar_IsMcuWakeActive, LedBar_SetIndicatorState | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16Buffer | 103 + 309/Project/Source/Sci_Upper.h | 115 | 27 | Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_DataInit, Sci_HostReadWords, Sci_HostWriteWords, Sci_ModbusProtocolFeed, Sci_ModbusResetMessage, Sci_WrRegsByteCountValid | CRC_verify, Sci_CopyProductIdBytes, Sci_Deal_ReadRegs_0x03, Sci_Deal_WrReg_0x06, Sci_Deal_WrRegs_0x10, Sci_GetWrRegNum, Sci_GetWrValue, Sci_ModbusGetTxBuffer, Sci_WrReg_0x06_BMS_FunctionOFF, Sci_WrReg_0x06_BMS_FunctionON, Sci_WrReg_0x06_Reset_AFE_Parameters, Sci_WrReg_0x06_Reset_EventRecord | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| payload | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 37 | 27 | CtProtocol_Feed, handle_bms_aging_status, handle_can_diag, handle_fw_info, handle_info, handle_upgrade_status, serial_write_block | CtProtocol_Encode, StorageFlash_LoadJournalPage, StorageFlash_LoadJournalPair, StorageFlash_LoadPair, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, StorageFlash_SaveJournalPage, StorageFlash_SaveJournalPair, StorageFlash_SavePair, StorageFlash_WriteSlot, handle_bms_aging_ctrl, handle_bms_aging_set_hours | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| node | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 45 | 27 | CtUpgrade_StartWithAppAddress, can_handle_hello, can_handle_rx, can_handle_start, can_reset_runtime | CtCan_IapPollAck, CtCan_IapSendCommit, CtCan_IapSendData, CtCan_IapSendEnd, CtCan_IapSendHello, CtCan_IapSendStart, CtCan_IapWaitAck, CtUpgrade_Start, CtUpgrade_Task, can_ack_id, can_ctrl_id, can_send_ack | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| node | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 97 | 27 | CtUpgrade_StartWithAppAddress, can_handle_hello, can_handle_rx, can_handle_start, can_reset_runtime | CtCan_IapPollAck, CtCan_IapSendCommit, CtCan_IapSendData, CtCan_IapSendEnd, CtCan_IapSendHello, CtCan_IapSendStart, CtCan_IapWaitAck, CtUpgrade_Start, CtUpgrade_Task, can_ack_id, can_ctrl_id, can_send_ack | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| mode | 103 + 309/Project/Source/SocEnhance.c | 110 | 25 | LP_GetBlockReason, LowPower_Request, SOC_GetDebugInternals, SOC_IntEnhance_Ctrl, SystemDebug_Snapshot, soc_apply_ocv_target_step, soc_empty_current_band, soc_heavy_discharge_active, soc_integrate, soc_integrate_current_ma, soc_low_tail_config, soc_update_display_soc | Conf_InitGpioMode, DbgPrint_LP, DbgPrint_SOC, DbgPrint_Summary, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, InitAFE1_Sleep, rtc_sleep, soc_apply_full_empty, soc_empty_tail_config, soc_update_rest_timer, soc_update_sag_hold | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| mode | 103 + 309/Project/Source/SystemDebug.h | 246 | 25 | LP_GetBlockReason, LowPower_Request, SOC_GetDebugInternals, SOC_IntEnhance_Ctrl, SystemDebug_Snapshot, soc_apply_ocv_target_step, soc_empty_current_band, soc_heavy_discharge_active, soc_integrate, soc_integrate_current_ma, soc_low_tail_config, soc_update_display_soc | Conf_InitGpioMode, DbgPrint_LP, DbgPrint_SOC, DbgPrint_Summary, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, InitAFE1_Sleep, rtc_sleep, soc_apply_full_empty, soc_empty_tail_config, soc_update_rest_timer, soc_update_sag_hold | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_factory_aging | 103 + 309/Project/Source/FactoryAging.c | 41 | 23 | FactoryAging_AddRunningTicks, FactoryAging_ApplyRunningMos, FactoryAging_EnterRunningFromHost, FactoryAging_Finish, FactoryAging_GetRemainingSeconds, FactoryAging_IsActive, FactoryAging_LoadDurationFromData, FactoryAging_LoadRuntimeStateForHost, FactoryAging_LoadStoredProgress, FactoryAging_MarkDone, FactoryAging_ResetMosCache, FactoryAging_ResetTimeByHost | FactoryAging_DebugWatchBind, FactoryAging_GetDuration10ms, FactoryAging_GetState, FactoryAging_SaveProgressBeforeSleep | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| AckType | 103 + 309/Project/Source/Sci_Upper.h | 113 | 23 | CRC_verify, Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_Deal_ReadRegs_0x03, Sci_Deal_WrReg_0x06, Sci_Deal_WrRegs_0x10, Sci_HostReadWords, Sci_HostWriteWords, Sci_ModbusProcessFrame, Sci_ModbusResetMessage, Sci_SetWrError, Sci_WrReg_0x06_BMS_FunctionOFF |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ErrorType | 103 + 309/Project/Source/Sci_Upper.h | 114 | 23 | CRC_verify, Sci_Deal_ReadRegs_0x03, Sci_Deal_WrReg_0x06, Sci_Deal_WrRegs_0x10, Sci_HostReadWords, Sci_HostWriteWords, Sci_ModbusProcessFrame, Sci_ModbusResetMessage, Sci_SetWrError, Sci_WrReg_0x06_BMS_FunctionOFF, Sci_WrReg_0x06_BMS_FunctionON, Sci_WrReg_0x06_Reset_AFE_Parameters | Sci_ACK_0x03, Sci_ACK_0x06_0x10 | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| cmd | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 34 | 22 | CtCan_AppAgingControl, CtProtocol_Feed, CtSelfIap_PollCan, can_decode_request, can_handle_ctrl, feidao_can_handle_app_cmd_data | CtApp_HandleFrame, CtCan_IapPollAck, CtCan_IapWaitAck, CtProtocol_Encode, LowPower_ConfigWakeupExti, can_build_ack, can_build_nack, can_send_ack, can_send_nack, command_allowed_during_upgrade, decode_app_ack, feidao_can_app_send_ack | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CFGR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1078 | 21 | CEC_Init, RCC_ADCCLKConfig, RCC_DeInit, RCC_HCLKConfig, RCC_PCLK1Config, RCC_PCLK2Config, RCC_PLLConfig, RCC_SYSCLKConfig, SetSysClockTo24, SetSysClockTo36, SetSysClockTo48, SetSysClockTo56 | CEC_Cmd, CEC_GetITStatus, RCC_GetClocksFreq, RCC_GetSYSCLKSource, SystemCoreClockUpdate, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| crc | 103 + 309/Project/Source/Flash.c | 28 | 21 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, CtCrc16_Update, CtProtocol_Encode, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, boot_consume_iap_request, can_handle_end, can_handle_start, can_send_ack, crc16_update | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| crc | 103 + 309/Project/Source/Flash.c | 45 | 21 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, CtCrc16_Update, CtProtocol_Encode, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, boot_consume_iap_request, can_handle_end, can_handle_start, can_send_ack, crc16_update | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| crc | firmware/comm_tool_f103ret6/source/app/ct_boot_control.h | 11 | 21 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, CtCrc16_Update, CtProtocol_Encode, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, boot_consume_iap_request, can_handle_end, can_handle_start, can_send_ack, crc16_update | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| index | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 43 | 21 | CRC_CalcBlockCRC, CtDebugLog_EncodeLatest, CtProtocol_Feed, CtProtocol_Init, LedBar_FrameEquals, LedBar_FrameTransitionCost, LedBar_GetPinIndex, Sci_RecordBackIndex, feidao_can_queue_has_request, parser_restart_with_byte, serial_tx_next, tx_next | ADC_GetRaw, ADC_GetResult, Sci_GetSocTableWord, Sci_GetWrValue, Sci_PutBytes, Sci_PutLatestFaultWords, Sci_PutWordBE, Sci_PutZeroWordsBE, SystemDebug_ReadEventRing | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| crc | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 83 | 21 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, CtCrc16_Update, CtProtocol_Encode, StorageFlash_ProgramRecord, StorageFlash_ReadSlot, boot_consume_iap_request, can_handle_end, can_handle_start, can_send_ack, crc16_update | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CCER | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1203 | 19 | TI1_Config, TI2_Config, TI3_Config, TI4_Config, TIM_CCxCmd, TIM_CCxNCmd, TIM_EncoderInterfaceConfig, TIM_OC1Init, TIM_OC1NPolarityConfig, TIM_OC1PolarityConfig, TIM_OC2Init, TIM_OC2NPolarityConfig |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| DR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 562 | 18 | CRC_CalcBlockCRC, CRC_CalcCRC, I2C_Send7bitAddress, I2C_SendData, SPI_I2S_SendData, Sci_PortIRQHandler, USART_SendData, dbg_uart_putc | ADC_GetConversionValue, BOARD_UART_IRQHandler, CRC_GetCRC, I2C_ReceiveData, InitADC_DMA, SPI_I2S_ReceiveData, Sci_PortArmReceiver, Sci_PortHandleError, USART_ReceiveData, serial_clear_overrun | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| DR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1037 | 18 | CRC_CalcBlockCRC, CRC_CalcCRC, I2C_Send7bitAddress, I2C_SendData, SPI_I2S_SendData, Sci_PortIRQHandler, USART_SendData, dbg_uart_putc | ADC_GetConversionValue, BOARD_UART_IRQHandler, CRC_GetCRC, I2C_ReceiveData, InitADC_DMA, SPI_I2S_ReceiveData, Sci_PortArmReceiver, Sci_PortHandleError, USART_ReceiveData, serial_clear_overrun | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| DR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1167 | 18 | CRC_CalcBlockCRC, CRC_CalcCRC, I2C_Send7bitAddress, I2C_SendData, SPI_I2S_SendData, Sci_PortIRQHandler, USART_SendData, dbg_uart_putc | ADC_GetConversionValue, BOARD_UART_IRQHandler, CRC_GetCRC, I2C_ReceiveData, InitADC_DMA, SPI_I2S_ReceiveData, Sci_PortArmReceiver, Sci_PortHandleError, USART_ReceiveData, serial_clear_overrun | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| DR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1237 | 18 | CRC_CalcBlockCRC, CRC_CalcCRC, I2C_Send7bitAddress, I2C_SendData, SPI_I2S_SendData, Sci_PortIRQHandler, USART_SendData, dbg_uart_putc | ADC_GetConversionValue, BOARD_UART_IRQHandler, CRC_GetCRC, I2C_ReceiveData, InitADC_DMA, SPI_I2S_ReceiveData, Sci_PortArmReceiver, Sci_PortHandleError, USART_ReceiveData, serial_clear_overrun | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| DLC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 157 | 17 | CAN_Receive, CAN_Transmit, CanFeidao_SendFrame, CtBoard_CanSend, can_handle_ctrl, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame, feidao_can_handle_rx_msg | can_handle_commit, can_handle_data, can_handle_end, can_handle_hello, can_handle_start, can_rx_push, feidao_can_enqueue_tx | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| DLC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 185 | 17 | CAN_Receive, CAN_Transmit, CanFeidao_SendFrame, CtBoard_CanSend, can_handle_ctrl, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame, feidao_can_handle_rx_msg | can_handle_commit, can_handle_data, can_handle_end, can_handle_hello, can_handle_start, can_rx_push, feidao_can_enqueue_tx | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| elapsed10ms | 103 + 309/Project/Source/FactoryAging.c | 31 | 17 | FactoryAging_AddRunningTicks, FactoryAging_GetRemainingSeconds, FactoryAging_LoadBkp, FactoryAging_LoadRuntimeStateForHost, FactoryAging_LoadStoredProgress, FactoryAging_MarkDone, FactoryAging_ResetTimeByHost, FactoryAging_SaveBkp, FactoryAging_SetDurationHoursByHost, FactoryAging_Start, FactoryAging_StartByHost, FactoryAging_StopByHost | FactoryAging_BkpCrc, FactoryAging_ClampElapsed, FactoryAging_SaveProgressBeforeSleep, FactoryAging_SaveStoredProgress, FactoryAging_Task | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| id | 103 + 309/Project/Source/IrqDebug.h | 60 | 16 | CtCan_IapSendData, CtCan_ReadFactoryAgingBroadcast, IrqDebug_RecordEvent, can_rx_push, can_send_ack, send_app_cmd, send_iap_ctrl | CtBoard_CanSend, CtCan_IapPollAck, CtCan_IapWaitAck, IrqDebug_Count, IrqDebug_CountFast, Sci_BmsFunctionIdIsSupported, can_decode_request, decode_app_ack, decode_app_word_frame | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| g_stLowPowerRtcStatus | 103 + 309/Project/Source/rtc_sleep.c | 14 | 16 | LP_GetBlockReason, LP_RecordLastSleepSeconds, LowPower_Request, lp_deep, lp_idle, lp_select, lp_sync, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | DbgPrint_Wakeup, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, DebugWatch_BindAll, LP_GetLastSleepSeconds, SystemDebug_Snapshot, rtc_sleep | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| g_stLowPowerRtcStatus | 103 + 309/Project/Source/rtc_sleep.h | 54 | 16 | LP_GetBlockReason, LP_RecordLastSleepSeconds, LowPower_Request, lp_deep, lp_idle, lp_select, lp_sync, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | DbgPrint_Wakeup, DebugHooks_RuntimeAfterLowPower, DebugHooks_RuntimeRecordEvents, DebugWatch_BindAll, LP_GetLastSleepSeconds, SystemDebug_Snapshot, rtc_sleep | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CCMR1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1199 | 15 | TI1_Config, TI2_Config, TIM_ClearOC1Ref, TIM_ClearOC2Ref, TIM_EncoderInterfaceConfig, TIM_ForcedOC1Config, TIM_ForcedOC2Config, TIM_OC1FastConfig, TIM_OC1Init, TIM_OC1PreloadConfig, TIM_OC2FastConfig, TIM_OC2Init |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_adc | 103 + 309/Project/Source/ADC.c | 13 | 15 | ADC_ClearTypeCOutCurrent, ADC_ResetAnlogCalSchedule, ADC_StopForLowPower, ADC_UpdateMosTemp, ADC_UpdateTypeCCurrent, ADC_UpdateVbc, App_AnlogCal, InitADC | ADC_DebugWatchBind, ADC_GetRaw, ADC_GetResult, ADC_GetTypeCOutCurrentMilliAmp, ADC_GetVbatMilliVolt, ADC_IsReady, InitADC_DMA | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| mailbox | 103 + 309/Project/Source/Can_HDX.c | 63 | 15 | AppUpgrade_IsIapRequested, AppUpgrade_RequestIap, CtBoard_CanSend, CtBoot_ClearRequest, CtBoot_RequestIap, InitCan, boot_consume_iap_request, boot_request_valid, can_has_sleep_blocking_work, can_transmit, feidao_can_abort_tx, feidao_can_service_tx | can_has_pending_work, feidao_can_cancel_tx, feidao_can_clear_tx_done | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| sleep | 103 + 309/Project/Source/LedBar.c | 81 | 15 | DebugWatch_BindAll, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_PrepareForStop, LedBar_SetSleep, LedBar_ShowSleepSocPreview, SleepDeal_DebugWatchBind, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | APP_LedBar, DbgPrint_All, LedBar_BuildCurrentFrame, LedBar_IsActiveForLowPower, LedBar_Scan1ms, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| sleep | 103 + 309/Project/Source/rtc_sleep.h | 50 | 15 | DebugWatch_BindAll, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_PrepareForStop, LedBar_SetSleep, LedBar_ShowSleepSocPreview, SleepDeal_DebugWatchBind, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle | APP_LedBar, DbgPrint_All, LedBar_BuildCurrentFrame, LedBar_IsActiveForLowPower, LedBar_Scan1ms, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CFGR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1090 | 14 | RCC_DeInit, RCC_PLL2Config, RCC_PLL3Config, RCC_PREDIV1Config, RCC_PREDIV2Config, SetSysClockTo24, SetSysClockTo36, SetSysClockTo48, SetSysClockTo56, SetSysClockTo72, SystemInit | I2S_Init, RCC_GetClocksFreq, SystemCoreClockUpdate | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CFGR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1095 | 14 | RCC_DeInit, RCC_PLL2Config, RCC_PLL3Config, RCC_PREDIV1Config, RCC_PREDIV2Config, SetSysClockTo24, SetSysClockTo36, SetSysClockTo48, SetSysClockTo56, SetSysClockTo72, SystemInit | I2S_Init, RCC_GetClocksFreq, SystemCoreClockUpdate | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CRL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1107 | 14 | GPIO_Init, LedBar_AllPinsHiZ, RTC_ClearFlag, RTC_ClearITPendingBit, RTC_EnterConfigMode, RTC_ExitConfigMode, RTC_WaitForSynchro, RTC_WaitForSynchroSafe, SystemInit_ExtMemCtl | LedBar_PinModeF1, RTC_GetFlagStatus, RTC_GetITStatus, RTC_WaitForLastTask, RTC_WaitForLastTaskSafe | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CCMR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1201 | 14 | TI3_Config, TI4_Config, TIM_ClearOC3Ref, TIM_ClearOC4Ref, TIM_ForcedOC3Config, TIM_ForcedOC4Config, TIM_OC3FastConfig, TIM_OC3Init, TIM_OC3PreloadConfig, TIM_OC4FastConfig, TIM_OC4Init, TIM_OC4PreloadConfig |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| GPIO_Mode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_gpio.h | 96 | 14 | Conf_InitGpioMode, GPIO_Init, GPIO_StructInit, InitADC_GPIO, InitAFE1_Sleep, InitCan_GPIO, Sci_InitCommonPort, board_gpio_init, board_uart_gpio_init, can_gpio_init, iap_can_init, iap_gpio_init |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| pvProtocolCtx | 103 + 309/Project/Source/Sci_Upper.c | 55 | 14 |  | Sci_InitCommonPort, Sci_ModbusGetTxBuffer, Sci_ModbusGetTxLength, Sci_ModbusIsBusy, Sci_ModbusOnRxIdle, Sci_ModbusOnTxComplete, Sci_ModbusProcessFrame, Sci_ModbusProtocolFeed, Sci_ModbusResetProtocol, Sci_PortAbortTransfer, Sci_PortFinishTx, Sci_PortIRQHandler | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| size | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 9 | 14 | CtFlash_Begin, CtUpgrade_StartWithAppAddress, can_handle_start, handle_fw_begin, handle_fw_end, soc_ocv_table | CtCan_IapSendStart, CtFlash_End, CtUpgrade_Task, app_addr_supported, handle_fw_info, load_next_block, soc_ocv_percent, soc_table_percent | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| seq | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 33 | 14 | CtDebugLog_Record, CtProtocol_Feed, IrqDebug_RecordEvent, can_handle_data, decode_app_word_frame, send_data_frame | CtCan_AppReadRegs, CtCan_IapSendData, CtDebugLog_EncodeLatest, CtProtocol_Encode, CtUpgrade_Task, feidao_can_app_send_word_frame, iap_data_id, respond | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| seq | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 51 | 14 | CtDebugLog_Record, CtProtocol_Feed, IrqDebug_RecordEvent, can_handle_data, decode_app_word_frame, send_data_frame | CtCan_AppReadRegs, CtCan_IapSendData, CtDebugLog_EncodeLatest, CtProtocol_Encode, CtUpgrade_Task, feidao_can_app_send_word_frame, iap_data_id, respond | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_can | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 113 | 14 | can_handle_commit, can_handle_ctrl, can_handle_data, can_handle_end, can_handle_hello, can_handle_rx, can_handle_start, can_reset_runtime, can_send_ack, can_send_nack, iap_task_1ms | can_build_ack, can_build_nack, can_handle_abort | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| GPIO_Speed | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_gpio.h | 93 | 13 | Conf_InitGpioMode, GPIO_StructInit, InitAFE1_Sleep, InitCan_GPIO, Sci_InitCommonPort, board_gpio_init, board_uart_gpio_init, can_gpio_init, iap_can_init, iap_gpio_init, iap_uart_init, initAFE1_IIC | GPIO_Init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_tx | 103 + 309/Project/Source/Can_HDX.c | 92 | 13 | InitCan, can_has_sleep_blocking_work, feidao_can_abort_tx, feidao_can_clear_tx_queue, feidao_can_dequeue_tx, feidao_can_enqueue_tx, feidao_can_service_tx | Can_DebugWatchBind, Can_GetDebugSnapshot, can_has_pending_work, feidao_can_queue_has_request, feidao_can_service_read_block_stream, respond | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| request | 103 + 309/Project/Source/Flash.c | 43 | 13 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, AppUpgrade_MailboxCrc, boot_crc, boot_request_valid, legacy_send_read_ack, legacy_send_write_ack, serial_send_ack, serial_send_error, serial_send_read_regs | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16_VCellMin | 103 + 309/Project/Source/SocEnhance.h | 51 | 13 | SOC_ApplyRtcRelaxationCompensation, SOC_UpdateSampleData | SystemDebug_Snapshot, soc_cell_delta, soc_full_confirm_seconds, soc_ocv_percent, soc_rest_voltage_stable, soc_sag_hold_blocks_calibration, soc_tail_rule_lookup, soc_update_display_soc, soc_vmin_above_empty_offset, soc_voltage_valid, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| request | firmware/comm_tool_f103ret6/source/app/ct_boot_control.h | 9 | 13 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, AppUpgrade_MailboxCrc, boot_crc, boot_request_valid, legacy_send_read_ack, legacy_send_write_ack, serial_send_ack, serial_send_error, serial_send_read_regs | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_ctx | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 60 | 13 | CtUpgrade_Abort, CtUpgrade_StartWithAppAddress, CtUpgrade_Task, finish_committed_block, load_next_block, reset_context, send_hello_and_mark, set_error, set_phase | handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait, send_data_frame | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| request | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 81 | 13 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, AppUpgrade_MailboxCrc, boot_crc, boot_request_valid, legacy_send_read_ack, legacy_send_write_ack, serial_send_ack, serial_send_error, serial_send_read_regs | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 706 | 12 | CAN_CancelTransmit, CAN_ClearFlag, CAN_ClearITPendingBit | CAN_GetFlagStatus, CAN_GetITStatus, CAN_Transmit, CAN_TransmitStatus, SystemDebug_SnapshotMcuResources, can_diag_latch_regs, can_has_pending_work, can_has_sleep_blocking_work, feidao_can_cancel_tx | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 915 | 12 | FLASH_ClearFlag, FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_NANDDeInit | FLASH_GetBank2Status, FLASH_GetFlagStatus, FSMC_GetFlagStatus, FSMC_GetITStatus, I2C_CheckEvent, I2C_GetLastEvent, I2C_GetPEC | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 962 | 12 | FLASH_ClearFlag, FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_NANDDeInit | FLASH_GetBank2Status, FLASH_GetFlagStatus, FSMC_GetFlagStatus, FSMC_GetITStatus, I2C_CheckEvent, I2C_GetLastEvent, I2C_GetPEC | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1041 | 12 | FLASH_ClearFlag, FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_NANDDeInit | FLASH_GetBank2Status, FLASH_GetFlagStatus, FSMC_GetFlagStatus, FSMC_GetITStatus, I2C_CheckEvent, I2C_GetLastEvent, I2C_GetPEC | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| IDE | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 150 | 12 | CAN_Receive, CAN_Transmit, CanFeidao_SendFrame, CtBoard_CanSend, can_rx_push, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame, feidao_can_transmit | can_handle_rx, feidao_can_handle_rx_msg | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| IDE | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 178 | 12 | CAN_Receive, CAN_Transmit, CanFeidao_SendFrame, CtBoard_CanSend, can_rx_push, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame, feidao_can_transmit | can_handle_rx, feidao_can_handle_rx_msg | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| source | 103 + 309/Project/Source/Can_HDX.c | 54 | 12 | RtcSleep_AfePortHasAfeWake, RtcSleep_AfePortHasCurrentWake, RtcSleep_PortOnWakeupSource, feidao_can_enqueue_tx, isException | RtcSleep_PortHasAfeWake, RtcSleep_PortHasCurrentWake, SystemDebug_RecordWatchdogFeed, feidao_can_queue_has_request, feidao_can_service_tx, feidao_can_transmit, soc_watch_set_calib_source | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_app | 103 + 309/Project/Source/Can_HDX.c | 103 | 12 | InitCan, feidao_can_clear_app_cmd_queue, feidao_can_handle_app_cmd_data, feidao_can_queue_app_cmd, feidao_can_service_enter_iap_delay, feidao_can_service_read_block_stream, feidao_can_start_read_block_stream, feidao_can_stop_read_block_stream, feidao_can_take_app_cmd | Can_DebugWatchBind, can_has_pending_work, can_has_sleep_blocking_work | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_flash | 103 + 309/Project/Source/Flash.c | 52 | 12 | StorageFlash_BeginWrite, StorageFlash_EndWrite, iap_flash_abort, iap_flash_begin, iap_flash_ensure_page_erased, iap_flash_finish, iap_flash_write, serial_write_block | Flash_DebugWatchBind, StorageFlash_IsBusy, serial_status_word, vector_valid_in_buffer | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16Ichg | 103 + 309/Project/Source/Sci_Upper.h | 73 | 12 | DataLoad_Current, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, CanFeidao_SendSoc1000ms, CanFeidao_SendStatus5000ms, CanFeidao_SendVoltageCurrent1000ms, RtcSleep_AfePortHasCurrentWake, RtcSleep_PortGetChargeCurrentMa, new_todo_logi, soc_param_lib_init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| cycle_x100 | 103 + 309/Project/Source/SocEnhance.c | 91 | 12 | SOC_ResetStoredSnapshotToDefault, soc_handle_command, soc_load_or_default, soc_param_lib_init, soc_update_save_mark | soc_add_discharge, soc_export_public_fields, soc_refresh_capacity_base, soc_save, soc_save_mark_changed, soc_soh_from_cycle, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16Ichg | 103 + 309/Project/Source/SocEnhance.h | 79 | 12 | DataLoad_Current, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, CanFeidao_SendSoc1000ms, CanFeidao_SendStatus5000ms, CanFeidao_SendVoltageCurrent1000ms, RtcSleep_AfePortHasCurrentWake, RtcSleep_PortGetChargeCurrentMa, new_todo_logi, soc_param_lib_init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ide | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 8 | 12 | CtCan_IapPollAck, CtCan_IapSendData, CtCan_IapWaitAck, can_rx_push, can_send_ack, send_app_cmd, send_iap_ctrl | CtBoard_CanSend, CtCan_ReadFactoryAgingBroadcast, can_decode_request, decode_app_ack, decode_app_word_frame | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| dlc | firmware/comm_tool_f103ret6/source/app/ct_board_port.h | 9 | 12 | CtCan_IapSendData, can_rx_push, can_send_ack, send_app_cmd, send_iap_ctrl | CtBoard_CanSend, CtCan_IapPollAck, CtCan_IapWaitAck, CtCan_ReadFactoryAgingBroadcast, can_decode_request, decode_app_ack, decode_app_word_frame | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_flash | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 112 | 12 | StorageFlash_BeginWrite, StorageFlash_EndWrite, iap_flash_abort, iap_flash_begin, iap_flash_ensure_page_erased, iap_flash_finish, iap_flash_write, serial_write_block | Flash_DebugWatchBind, StorageFlash_IsBusy, serial_status_word, vector_valid_in_buffer | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| NVIC_IRQChannelPreemptionPriority | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/misc.h | 52 | 11 | BoardUart_Init, Conf_InitWakeupInputExti, InitCan_NVIC, InitTimer, LedBar_ScanTimerInit, RTC_AlarmConfig, RTC_NVIC_Config, Sci_InitCommonPort, can_hw_init, iap_uart_init | NVIC_Init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| NVIC_IRQChannelSubPriority | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/misc.h | 57 | 11 | BoardUart_Init, Conf_InitWakeupInputExti, InitCan_NVIC, InitTimer, LedBar_ScanTimerInit, RTC_AlarmConfig, RTC_NVIC_Config, Sci_InitCommonPort, can_hw_init, iap_uart_init | NVIC_Init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| NVIC_IRQChannelCmd | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/misc.h | 61 | 11 | BoardUart_Init, Conf_InitWakeupInputExti, InitCan_NVIC, InitTimer, LedBar_ScanTimerInit, RTC_AlarmConfig, RTC_NVIC_Config, Sci_InitCommonPort, can_hw_init, iap_uart_init | NVIC_Init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| blank | 103 + 309/Project/Source/LedBar.c | 82 | 11 | APP_LedBar, LedBar_Clear, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_PrepareForStop, LedBar_SetIndicators, LedBar_SetNumber, LedBar_ShowSleepSocPreview, SystemDebug_RefreshModuleStates | LedBar_BuildCurrentFrame, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_log_record | 103 + 309/Project/Source/LogRecord.c | 18 | 11 | App_LogRecord, EEPROM_ResetData_EventRecord_ToDefault, LogEvent_EEPROM, LogEvent_Record, LogRecord_MarkEventSaved, LogRecord_RequestSleep, LogRecord_RequestStartup, ReadEEPROM_EventRecord_Parameters | LogRecord_CanSaveEvent, LogRecord_DebugWatchBind, Sci_ACK_0x03_ReadRegs_EventRecord | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_rtc | 103 + 309/Project/Source/RTC.c | 12 | 11 | App_RTC, RTC_ClearStopWakeup, RTC_HandleAlarmWakeup, RTC_IRQHandler, RTC_SetWakeupPeriodSeconds, RTC_WKTimeConfig | Get_RTC_Time, RTC_DebugWatchBind, RTC_GetLastWakeupPeriodSeconds, RTC_GetWakeupPeriodSeconds, RTC_IsStopWakeup | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16VCellMin | 103 + 309/Project/Source/Sci_Upper.h | 65 | 11 | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, RtcSleep_PortApplySocRtcRest, RtcSleep_PortGetCellMinMv, RtcSleep_PortIsEmergencyWakeVoltage, SystemDebug_Snapshot, new_todo_logi, soc_param_lib_init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16VCellMin | 103 + 309/Project/Source/SocEnhance.h | 77 | 11 | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, RtcSleep_PortApplySocRtcRest, RtcSleep_PortGetCellMinMv, RtcSleep_PortIsEmergencyWakeVoltage, SystemDebug_Snapshot, new_todo_logi, soc_param_lib_init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| blank | 103 + 309/Project/Source/SystemDebug.h | 287 | 11 | APP_LedBar, LedBar_Clear, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_PrepareForStop, LedBar_SetIndicators, LedBar_SetNumber, LedBar_ShowSleepSocPreview, SystemDebug_RefreshModuleStates | LedBar_BuildCurrentFrame, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_status | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 58 | 11 | CtUpgrade_Abort, CtUpgrade_Init, CtUpgrade_StartWithAppAddress, CtUpgrade_Task, finish_committed_block, set_error | CtUpgrade_GetStatus, handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait, set_phase | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SeriesNum | tools/soc_host_c_test.c | 19 | 11 | EEPROM_UpdateOtherElementRuntime, Sci_ApplyOtherElementSideEffects, host_apply_default_config | DataLoad_CellVolt, DataLoad_CellVoltMaxMinFind, DebugWatch_BindAll, Refresh_Parameters, UpdateVoltageFromBqMaximo, host_init_with_voltage, host_tick, new_todo_logi | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SeriesNum | tools/soc_host_visual_trace.c | 15 | 11 | EEPROM_UpdateOtherElementRuntime, Sci_ApplyOtherElementSideEffects, host_apply_default_config | DataLoad_CellVolt, DataLoad_CellVoltMaxMinFind, DebugWatch_BindAll, Refresh_Parameters, UpdateVoltageFromBqMaximo, host_init_with_voltage, host_tick, new_todo_logi | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| seconds | tools/soc_host_visual_trace.c | 28 | 11 | RTC_IsWakeupPeriodSafe, soc_update_display_soc, test_rtc_ocv_ignores_upward_stable_target, test_rtc_ocv_waits_for_voltage_convergence | LP_RecordLastSleepSeconds, RTC_SetWakeupPeriodSeconds, RtcSleep_PortAddRuntimeSeconds, host_run_scenario, host_run_seconds, host_self_delta_as10, soc_add_rest_seconds | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 596 | 10 | BKP_ClearFlag, BKP_ClearITPendingBit, CEC_ClearFlag, CEC_ClearITPendingBit, RCC_ClearFlag | CEC_GetITStatus, PWR_GetFlagStatus, RCC_GetFlagStatus, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| MSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 705 | 10 | CAN_ClearFlag, CAN_ClearITPendingBit | CAN_GetFlagStatus, CAN_GetITStatus, CAN_Init, CAN_OperatingModeRequest, CAN_Sleep, CAN_WakeUp, SystemDebug_SnapshotMcuResources, can_diag_latch_regs | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ESR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 710 | 10 | CAN_ClearFlag, CAN_ClearITPendingBit | CAN_GetFlagStatus, CAN_GetITStatus, CAN_GetLSBTransmitErrorCounter, CAN_GetLastErrorCode, CAN_GetReceiveErrorCounter, Can_GetDebugSnapshot, SystemDebug_SnapshotMcuResources, can_diag_latch_regs | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ESR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 740 | 10 | CAN_ClearFlag, CAN_ClearITPendingBit | CAN_GetFlagStatus, CAN_GetITStatus, CAN_GetLSBTransmitErrorCounter, CAN_GetLastErrorCode, CAN_GetReceiveErrorCounter, Can_GetDebugSnapshot, SystemDebug_SnapshotMcuResources, can_diag_latch_regs | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 741 | 10 | BKP_ClearFlag, BKP_ClearITPendingBit, CEC_ClearFlag, CEC_ClearITPendingBit, RCC_ClearFlag | CEC_GetITStatus, PWR_GetFlagStatus, RCC_GetFlagStatus, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1068 | 10 | BKP_ClearFlag, BKP_ClearITPendingBit, CEC_ClearFlag, CEC_ClearITPendingBit, RCC_ClearFlag | CEC_GetITStatus, PWR_GetFlagStatus, RCC_GetFlagStatus, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CSR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1086 | 10 | BKP_ClearFlag, BKP_ClearITPendingBit, CEC_ClearFlag, CEC_ClearITPendingBit, RCC_ClearFlag | CEC_GetITStatus, PWR_GetFlagStatus, RCC_GetFlagStatus, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SMCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1191 | 10 | TIM_ETRClockMode1Config, TIM_ETRClockMode2Config, TIM_ETRConfig, TIM_EncoderInterfaceConfig, TIM_ITRxExternalClockConfig, TIM_InternalClockConfig, TIM_SelectInputTrigger, TIM_SelectMasterSlaveMode, TIM_SelectSlaveMode, TIM_TIxExternalClockConfig |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1245 | 10 | Sci_InitCommonPort, USART_DMACmd, USART_HalfDuplexCmd, USART_Init, USART_IrDACmd, USART_IrDAConfig, USART_OneBitMethodCmd, USART_SmartCardCmd, USART_SmartCardNACKCmd | USART_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ExtId | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 147 | 10 | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, can_handle_rx, can_send_ack, can_send_nack | CAN_Transmit, can_handle_data, can_handle_start, can_rx_push | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ExtId | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 175 | 10 | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, can_handle_rx, can_send_ack, can_send_nack | CAN_Transmit, can_handle_data, can_handle_start, can_rx_push | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TIM_OCPolarity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 92 | 10 | InitADC_TIMER, TIM_OCStructInit | TIM_OC1Init, TIM_OC1PolarityConfig, TIM_OC2Init, TIM_OC2PolarityConfig, TIM_OC3Init, TIM_OC3PolarityConfig, TIM_OC4Init, TIM_OC4PolarityConfig | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| cap_full_as10 | 103 + 309/Project/Source/SocEnhance.c | 89 | 10 | soc_from_cap, soc_refresh_capacity_base, soc_update_save_mark | soc_export_public_fields, soc_integrate, soc_load_or_default, soc_save, soc_save_mark_changed, soc_set, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| cap_full_as10 | 103 + 309/Project/Source/SocEnhance.c | 122 | 10 | soc_from_cap, soc_refresh_capacity_base, soc_update_save_mark | soc_export_public_fields, soc_integrate, soc_load_or_default, soc_save, soc_save_mark_changed, soc_set, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| block | 103 + 309/Project/Source/SystemDebug.h | 219 | 10 | SystemDebug_Snapshot, lp_deep, lp_select | DbgPrint_LP, DbgPrint_Summary, DebugHooks_RuntimeRecordEvents, can_handle_commit, can_handle_data, load_next_block, send_data_frame | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| block | 103 + 309/Project/Source/rtc_sleep.h | 49 | 10 | SystemDebug_Snapshot, lp_deep, lp_select | DbgPrint_LP, DbgPrint_Summary, DebugHooks_RuntimeRecordEvents, can_handle_commit, can_handle_data, load_next_block, send_data_frame | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| block | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.c | 56 | 10 | SystemDebug_Snapshot, lp_deep, lp_select | DbgPrint_LP, DbgPrint_Summary, DebugHooks_RuntimeRecordEvents, can_handle_commit, can_handle_data, load_next_block, send_data_frame | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| expect_seq | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 13 | 10 | CtCan_IapPollAck, CtCan_IapWaitAck, can_handle_data | can_build_ack, can_build_nack, can_handle_end, handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait, handle_upgrade_status | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| expect_seq | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 100 | 10 | CtCan_IapPollAck, CtCan_IapWaitAck, can_handle_data | can_build_ack, can_build_nack, can_handle_end, handle_ack_wait, handle_fast_hello_wait, handle_iap_hello_wait, handle_upgrade_status | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| block | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 108 | 10 | SystemDebug_Snapshot, lp_deep, lp_select | DbgPrint_LP, DbgPrint_Summary, DebugHooks_RuntimeRecordEvents, can_handle_commit, can_handle_data, load_next_block, send_data_frame | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_tick_ms | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 110 | 10 | SysTick_Handler | Board_GetTickMs, can_handle_commit, can_handle_data, can_handle_start, iap_task_1ms, schedule_reset, serial_check_frame_timeout, serial_delay_ms, serial_feed | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| RF0R | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 707 | 9 | CAN_ClearFlag, CAN_ClearITPendingBit, CAN_FIFORelease, CAN_Receive | CAN_GetFlagStatus, CAN_GetITStatus, CAN_MessagePending, SystemDebug_SnapshotMcuResources, can_diag_latch_regs | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| PR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 893 | 9 | EXTI_ClearFlag, EXTI_ClearITPendingBit, EXTI_DeInit, IWDG_SetPrescaler | EXTI_GetFlagStatus, EXTI_GetITStatus, IrqDebug_CountFast, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| PR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1056 | 9 | EXTI_ClearFlag, EXTI_ClearITPendingBit, EXTI_DeInit, IWDG_SetPrescaler | EXTI_GetFlagStatus, EXTI_GetITStatus, IrqDebug_CountFast, SystemDebug_RecordWatchdogFeed, SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| RTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 153 | 9 | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame | CAN_Transmit, can_handle_rx | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| RTR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h | 181 | 9 | CAN_Receive, CanFeidao_SendFrame, CtBoard_CanSend, can_send_ack, can_send_nack, feidao_can_app_send_ack, feidao_can_app_send_word_frame | CAN_Transmit, can_handle_rx | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| tables | 103 + 309/Project/Source/DebugWatch.h | 212 | 9 | ADC_DebugWatchBind, CanFeidaoFrames_DebugWatchBind, I2C_AFE1_DebugWatchBind, LedBar_DebugWatchBind, RTC_DebugWatchBind, SH367309Data_DebugWatchBind, SH367309Func_DebugWatchBind, SocEnhance_DebugWatchBindTables, SystemMonitor_DebugWatchBind |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16VCellMax | 103 + 309/Project/Source/Sci_Upper.h | 64 | 9 | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, RtcSleep_PortApplySocRtcRest, SystemDebug_Snapshot, new_todo_logi, soc_param_lib_init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16IDischg | 103 + 309/Project/Source/Sci_Upper.h | 74 | 9 | DataLoad_Current, host_init_with_voltage, host_tick | App_SOC, CanFeidao_SendStatus5000ms, CanFeidao_SendVoltageCurrent1000ms, RtcSleep_AfePortHasCurrentWake, RtcSleep_PortGetDischargeCurrentMa, soc_param_lib_init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| cap_now_as10 | 103 + 309/Project/Source/SocEnhance.c | 90 | 9 | soc_apply_discharge_delta, soc_integrate, soc_load_or_default, soc_refresh_capacity_base, soc_set | soc_export_public_fields, soc_from_cap, soc_save, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16VCellMax | 103 + 309/Project/Source/SocEnhance.h | 76 | 9 | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick, soc_watch_refresh | App_SOC, RtcSleep_PortApplySocRtcRest, SystemDebug_Snapshot, new_todo_logi, soc_param_lib_init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| vmax | 103 + 309/Project/Source/SystemDebug.h | 240 | 9 | SystemDebug_Snapshot, host_pack_voltage, host_run_scenario | DbgPrint_SOC, DbgPrint_Summary, host_emit_row, host_init_with_voltage, host_run_seconds, host_tick | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| vmin | 103 + 309/Project/Source/SystemDebug.h | 241 | 9 | SystemDebug_Snapshot, host_pack_voltage, host_run_scenario | DbgPrint_SOC, DbgPrint_Summary, host_emit_row, host_init_with_voltage, host_run_seconds, host_tick | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_system_status | 103 + 309/Project/Source/System_Monitor.c | 5 | 9 | InitSystemMonitorData_EEPROM, SystemRuntime_MarkBootReady, SystemRuntime_SetAfeStatus, SystemRuntime_SetMosStatus, SystemRuntime_SetProjectVersion | SystemMonitor_DebugWatchBind, SystemRuntime_GetStatusSnapshot, SystemRuntime_IsChargeMosOpen, SystemRuntime_IsDischargeMosOpen | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_app_can_addr | firmware/comm_tool_f103ret6/source/app/ct_app.c | 14 | 9 | handle_set_can | handle_bms_aging_ctrl, handle_bms_aging_set_hours, handle_bms_read, handle_bms_write, handle_can_diag, handle_enter_iap, handle_info, handle_upgrade | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| event | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 9 | 9 | CtDebugLog_Record, LogEvent_EEPROM, LogRecord_CanSaveEvent, SystemDebug_Event | CtDebugLog_EncodeLatest, LogEvent_Record, LogRecord_IsEntryValid, LogRecord_MarkEventSaved, SystemDebug_ReadEventRing | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TIM_ICPolarity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 118 | 8 | TIM_ICStructInit, TIM_PWMIConfig | TI1_Config, TI2_Config, TI3_Config, TI4_Config, TIM_ICInit, TIM_TIxExternalClockConfig | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| last | 103 + 309/Project/Source/ADC.c | 7 | 8 | ADC_ResetAnlogCalSchedule, App_AnlogCal, LP_RecordLastSleepSeconds, RTC_WKTimeConfig, soc_table_percent | LP_GetLastSleepSeconds, RTC_GetLastWakeupPeriodSeconds, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| g_u32CS_Res_AFE | 103 + 309/Project/Source/DataDeal.c | 75 | 8 | AppInit_InitRuntimeState, DataLoad_CurrentMilliAmpToRaw, DataLoad_CurrentRawToMilliAmp, EEPROM_UpdateOtherElementRuntime, Refresh_Parameters, Sci_ApplyOtherElementSideEffects | DataDeal_DebugWatchBind, InitShortCur | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| g_u32CS_Res_AFE | 103 + 309/Project/Source/DataDeal.h | 201 | 8 | AppInit_InitRuntimeState, DataLoad_CurrentMilliAmpToRaw, DataLoad_CurrentRawToMilliAmp, EEPROM_UpdateOtherElementRuntime, Refresh_Parameters, Sci_ApplyOtherElementSideEffects | DataDeal_DebugWatchBind, InitShortCur | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| phase | 103 + 309/Project/Source/IrqDebug.h | 62 | 8 | CtUpgrade_Abort, CtUpgrade_Task, IrqDebug_CountFast, IrqDebug_RecordEvent, reset_context, set_error, set_phase | IrqDebug_SetPhase | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| phase | 103 + 309/Project/Source/IrqDebug.h | 68 | 8 | CtUpgrade_Abort, CtUpgrade_Task, IrqDebug_CountFast, IrqDebug_RecordEvent, reset_context, set_error, set_phase | IrqDebug_SetPhase | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| number | 103 + 309/Project/Source/LedBar.c | 83 | 8 | APP_LedBar, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_SetNumber, LedBar_ShowSleepSocPreview | DbgPrint_All, LedBar_BuildCurrentFrame, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| indicator_mask | 103 + 309/Project/Source/LedBar.c | 84 | 8 | APP_LedBar, LedBar_Init, LedBar_SetIndicatorState, LedBar_SetIndicators, LedBar_ShowSleepSocPreview | LedBar_BuildCurrentFrame, LedBar_BuildTargetMask, LedBar_GetDebugSnapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| last | 103 + 309/Project/Source/RTC.c | 9 | 8 | ADC_ResetAnlogCalSchedule, App_AnlogCal, LP_RecordLastSleepSeconds, RTC_WKTimeConfig, soc_table_percent | LP_GetLastSleepSeconds, RTC_GetLastWakeupPeriodSeconds, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| AFE_Parameters_RS485_Struction | 103 + 309/Project/Source/SH367309_DataDeal.c | 16 | 8 |  | AFE_CopyCurValues, AFE_RestoreCurValues, EEPROM_ResetData_AFE_ParametersToDefault, ReadEEPROM_AFE_Parameters, Refresh_Parameters, SH367309Data_DebugWatchBind, Sci_ACK_0x03_RW_AFE_Parameters, Sci_WrRegs_0x10_AFE_Parameters | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| csr | 103 + 309/Project/Source/Sci_Upper.h | 108 | 8 | Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_DataInit, Sci_ModbusOnRxIdle, Sci_ModbusProtocolFeed, Sci_ModbusResetMessage, SystemDebug_SnapshotMcuResources | Sci_ModbusIsBusy | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| soh | 103 + 309/Project/Source/SocEnhance.c | 106 | 8 | CanFeidao_SendSoh5000ms, CtCan_AppGetStatus, SystemDebug_Snapshot, soc_refresh_capacity_base | DbgPrint_SOC, soc_add_discharge, soc_export_public_fields, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| full_anchor | 103 + 309/Project/Source/SocEnhance.c | 116 | 8 | SOC_GetDebugInternals, soc_apply_discharge_delta, soc_apply_full_empty, soc_set | DbgPrint_SOC, SystemDebug_Snapshot, soc_integrate, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16_VCellMax | 103 + 309/Project/Source/SocEnhance.h | 48 | 8 | SOC_ApplyRtcRelaxationCompensation, SOC_UpdateSampleData | SystemDebug_Snapshot, soc_cell_delta, soc_full_confirm_seconds, soc_rest_voltage_stable, soc_voltage_valid, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16_Idsg | 103 + 309/Project/Source/SocEnhance.h | 53 | 8 | SOC_UpdateSampleData | SystemDebug_Snapshot, soc_direction, soc_empty_current_band, soc_heavy_discharge_active, soc_integrate_current_ma, soc_watch_refresh, test_board_self_consumption_integrates_during_relax | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| csr | 103 + 309/Project/Source/SystemDebug.h | 150 | 8 | Sci_ACK_0x03, Sci_ACK_0x06_0x10, Sci_DataInit, Sci_ModbusOnRxIdle, Sci_ModbusProtocolFeed, Sci_ModbusResetMessage, SystemDebug_SnapshotMcuResources | Sci_ModbusIsBusy | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| soh | 103 + 309/Project/Source/SystemDebug.h | 238 | 8 | CanFeidao_SendSoh5000ms, CtCan_AppGetStatus, SystemDebug_Snapshot, soc_refresh_capacity_base | DbgPrint_SOC, soc_add_discharge, soc_export_public_fields, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| full_anchor | 103 + 309/Project/Source/SystemDebug.h | 253 | 8 | SOC_GetDebugInternals, soc_apply_discharge_delta, soc_apply_full_empty, soc_set | DbgPrint_SOC, SystemDebug_Snapshot, soc_integrate, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| number | 103 + 309/Project/Source/SystemDebug.h | 288 | 8 | APP_LedBar, LedBar_GetDebugSnapshot, LedBar_Init, LedBar_SetNumber, LedBar_ShowSleepSocPreview | DbgPrint_All, LedBar_BuildCurrentFrame, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| last | 103 + 309/Project/Source/rtc_sleep.h | 51 | 8 | ADC_ResetAnlogCalSchedule, App_AnlogCal, LP_RecordLastSleepSeconds, RTC_WKTimeConfig, soc_table_percent | LP_GetLastSleepSeconds, RTC_GetLastWakeupPeriodSeconds, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| module | firmware/comm_tool_f103ret6/source/app/ct_debug_log.c | 8 | 8 | CtDebugLog_Record, SystemDebug_ModuleApplyState, SystemDebug_ModuleBuildStaleMask, SystemDebug_ModuleHeartbeat, SystemDebug_RefreshModuleStates | CtDebugLog_EncodeLatest, SystemDebug_ModuleItem, SystemDebug_ModuleMask | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| crc16 | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 10 | 8 | CtFlash_Begin, handle_fw_begin, handle_fw_end | CtCan_IapSendEnd, CtCan_IapSendStart, CtFlash_End, CtUpgrade_Task, handle_fw_info | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| raw | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 45 | 8 | CtProtocol_Feed, InitADC, parser_restart_with_byte | ADC_GetRaw, ADC_UpdateMosTemp, ADC_UpdateTypeCCurrent, ADC_UpdateVbc, InitADC_DMA | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CTRL | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 537 | 7 | InitDelay, SysTick_CLKSourceConfig, SysTick_Config, __delay_ms, __delay_us, jump_to_app | SystemDebug_SnapshotMcuResources | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| RF1R | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 708 | 7 | CAN_ClearFlag, CAN_ClearITPendingBit, CAN_FIFORelease, CAN_Receive | CAN_GetFlagStatus, CAN_GetITStatus, CAN_MessagePending | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TIM_OCNPolarity | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 95 | 7 | TIM_OCStructInit | TIM_OC1Init, TIM_OC1NPolarityConfig, TIM_OC2Init, TIM_OC2NPolarityConfig, TIM_OC3Init, TIM_OC3NPolarityConfig | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TIM_ICSelection | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 121 | 7 | TIM_ICStructInit, TIM_PWMIConfig | TI1_Config, TI2_Config, TI3_Config, TI4_Config, TIM_ICInit | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TIM_ICFilter | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 127 | 7 | TIM_ICStructInit | TI1_Config, TI2_Config, TI3_Config, TI4_Config, TIM_ICInit, TIM_PWMIConfig | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ready | 103 + 309/Project/Source/ADC.c | 11 | 7 | ADC_ResetAnlogCalSchedule, ADC_StopForLowPower, App_AnlogCal, SystemDebug_Snapshot | ADC_IsReady, DbgPrint_LP, SystemDebug_RefreshModuleStates | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| pin | 103 + 309/Project/Source/LedBar.c | 69 | 7 | LedBar_GetPinIndex, LedBar_PinToOutput, LedBar_PinWrite, SystemDebug_SnapshotMcuResources | Conf_InitGpioMode, Conf_InitWakeupInputExti, LedBar_PinToOutputMode | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| soc_display_10ms | 103 + 309/Project/Source/LedBar.c | 90 | 7 | LedBar_Init, LedBar_RequestSocDisplayWindow, LedBar_RequestStartupDisplayWindow, LedBar_ServiceSwitch | LedBar_GetDebugSnapshot, LedBar_IsActiveForLowPower, LedBar_IsDisplayRequested | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| flags | 103 + 309/Project/Source/LogRecord.c | 9 | 7 | CtProtocol_Feed, LogEvent_Record, LogRecord_RequestSleep, LogRecord_RequestStartup | App_LogRecord, CtProtocol_Encode, host_set_snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16VCellTotle | 103 + 309/Project/Source/Sci_Upper.h | 69 | 7 | DataLoad_CellVoltMaxMinFind, host_init_with_voltage, host_tick | CanFeidao_SendCap5000ms, CanFeidao_SendVoltageCurrent1000ms, SOC_GetPackVoltageForTypeCMv, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_sleep | 103 + 309/Project/Source/SleepDeal.c | 12 | 7 | IsSleepStartUp, SleepDeal_IsBootFromSleepChargerWakeup, SleepDeal_MarkBootFromSleepChargerWakeup | SleepDeal_DebugWatchBind, SleepDeal_GetExternalCommCounter, SleepDeal_IsBootFromSleepStartup, SleepDeal_RecordExternalComm | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| target | 103 + 309/Project/Source/SocEnhance.c | 130 | 7 | soc_tail_rule_lookup, soc_update_display_soc | soc_apply_ocv_target_step, soc_apply_tail_step, soc_set_rest_down_target, soc_step, soc_watch_set_tail_state | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ticks | 103 + 309/Project/Source/SocEnhance.c | 131 | 7 | host_run_scenario, host_run_seconds, soc_tail_rule_lookup, soc_update_display_soc | SysTick_Config, soc_apply_tail_step, soc_watch_set_tail_state | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ticks | 103 + 309/Project/Source/SocEnhance.c | 137 | 7 | host_run_scenario, host_run_seconds, soc_tail_rule_lookup, soc_update_display_soc | SysTick_Config, soc_apply_tail_step, soc_watch_set_tail_state | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16_CapacityNow | 103 + 309/Project/Source/SocEnhance.h | 58 | 7 | soc_export_public_fields | SOC_PublishReportData, SystemDebug_Snapshot, test_board_self_consumption_integrates_during_relax, test_board_self_consumption_works_at_high_non_full_voltage, test_full_voltage_anchor_can_override_self_consumption, test_rtc_sleep_does_not_apply_board_self_consumption | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| pin | 103 + 309/Project/Source/SystemDebug.h | 200 | 7 | LedBar_GetPinIndex, LedBar_PinToOutput, LedBar_PinWrite, SystemDebug_SnapshotMcuResources | Conf_InitGpioMode, Conf_InitWakeupInputExti, LedBar_PinToOutputMode | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ready | 103 + 309/Project/Source/SystemDebug.h | 217 | 7 | ADC_ResetAnlogCalSchedule, ADC_StopForLowPower, App_AnlogCal, SystemDebug_Snapshot | ADC_IsReady, DbgPrint_LP, SystemDebug_RefreshModuleStates | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| System_ErrFlag | 103 + 309/Project/Source/System_Monitor.h | 212 | 7 | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| valid | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 13 | 7 | CtFlash_End, CtFlash_Init, CtUpgrade_StartWithAppAddress, StorageFlash_SaveJournalPage, boot_consume_iap_request, soc_load_or_default | handle_fw_info | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| flags | firmware/comm_tool_f103ret6/source/app/ct_protocol.h | 32 | 7 | CtProtocol_Feed, LogEvent_Record, LogRecord_RequestSleep, LogRecord_RequestStartup | App_LogRecord, CtProtocol_Encode, host_set_snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| percent | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 8 | 7 | CtUpgrade_StartWithAppAddress, CtUpgrade_Task, finish_committed_block | CtUpgrade_Abort, handle_upgrade_status, set_error, set_phase | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| last_error | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 9 | 7 | can_send_ack, can_send_nack, iap_task_1ms, set_error | CtUpgrade_Abort, CtUpgrade_Task, handle_upgrade_status | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| total | firmware/comm_tool_f103ret6/source/app/ct_upgrade_manager.h | 12 | 7 | CtProtocol_Encode, CtUpgrade_StartWithAppAddress | IrqDebug_CountFast, Sci_RangeFits, SystemDebug_SnapshotMcuResources, finish_committed_block, handle_upgrade_status | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| last_error | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 99 | 7 | can_send_ack, can_send_nack, iap_task_1ms, set_error | CtUpgrade_Abort, CtUpgrade_Task, handle_upgrade_status | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| System_ErrFlag | tools/soc_host_c_test.c | 18 | 7 | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| System_ErrFlag | tools/soc_host_visual_trace.c | 14 | 7 | Fault_ChangeToMCU | DebugWatch_BindAll, Sci_ACK_0x03_ReadRegs_Data, SystemDebug_Snapshot, System_ErrorField, host_reset_state, new_todo_logi | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.h | 161 | 6 | DMA_Cmd, DMA_DeInit, DMA_ITConfig, DMA_Init, I2C_FastModeDutyCycleConfig, I2C_Init |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| OPTKEYR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 904 | 6 | FLASH_BootConfig, FLASH_EnableWriteProtection, FLASH_EraseOptionBytes, FLASH_ProgramOptionByteData, FLASH_ReadOutProtection, FLASH_UserOptionByteConfig |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 976 | 6 | FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_NANDDeInit | FSMC_GetFlagStatus, FSMC_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SR4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 990 | 6 | FSMC_ClearFlag, FSMC_ClearITPendingBit, FSMC_ITConfig, FSMC_PCCARDDeInit | FSMC_GetFlagStatus, FSMC_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CRH | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1003 | 6 | GPIO_Init, LedBar_AllPinsHiZ, RTC_ITConfig, SystemInit_ExtMemCtl | LedBar_PinModeF1, RTC_GetITStatus | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| CCR | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h | 1043 | 6 | DMA_Cmd, DMA_DeInit, DMA_ITConfig, DMA_Init, I2C_FastModeDutyCycleConfig, I2C_Init |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TIM_CounterMode | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 53 | 6 | InitADC_TIMER, InitTimer, LedBar_ScanTimerInit, TIM_TimeBaseStructInit | TIM_CounterModeConfig, TIM_TimeBaseInit | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TIM_OutputState | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 82 | 6 | InitADC_TIMER, TIM_OCStructInit | TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| TIM_Pulse | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_tim.h | 88 | 6 | InitADC_TIMER, TIM_OCStructInit | TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| read_block_active | 103 + 309/Project/Source/Can_HDX.c | 89 | 6 | InitCan, feidao_can_service_read_block_stream, feidao_can_start_read_block_stream, feidao_can_stop_read_block_stream | can_has_pending_work, can_has_sleep_blocking_work | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| zeroState | 103 + 309/Project/Source/DataDeal.c | 38 | 6 | AfeCurrent_IsStartupZeroDone, AfeCurrent_PrepareStartupZero, AfeCurrent_StartupZeroCal, DataLoad_CurrentApplyAutoZero, DataLoad_CurrentMarkZeroPending, DataLoad_CurrentSetZeroOffset |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| g_u16CalibCoefK | 103 + 309/Project/Source/DataDeal.c | 72 | 6 | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| g_i16CalibCoefB | 103 + 309/Project/Source/DataDeal.c | 74 | 6 | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| g_i16CalibCoefB | 103 + 309/Project/Source/DataDeal.h | 199 | 6 | EEPROM_LoadDefaultCalib | DataDeal_DebugWatchBind, DataLoad_CellVoltMaxMinFind, DataLoad_CurrentApplyCalib, DataLoad_Temperature, Sci_ACK_0x03_RW_Data_Cali | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| fault | 103 + 309/Project/Source/DebugHooks.c | 16 | 6 | DebugHooks_RuntimeRecordEvents, Fault_DebugWatchBind, SystemDebug_Event, SystemDebug_Snapshot | SH367309_RecordFaultOnActive, SystemDebug_RefreshModuleStates | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| lp | 103 + 309/Project/Source/DebugHooks.c | 17 | 6 | DebugHooks_RuntimeRecordEvents, SystemDebug_Snapshot | DbgPrint_LP, DbgPrint_Summary, DbgPrint_Wakeup, SystemDebug_RefreshModuleStates | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| error | 103 + 309/Project/Source/DebugWatch.h | 116 | 6 | DebugWatch_BindAll | InitShortCur, Sci_ApplyOtherElementSideEffects, Sci_SetWrError, feidao_can_app_status_from_host_error, serial_send_error | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| fault | 103 + 309/Project/Source/DebugWatch.h | 208 | 6 | DebugHooks_RuntimeRecordEvents, Fault_DebugWatchBind, SystemDebug_Event, SystemDebug_Snapshot | SH367309_RecordFaultOnActive, SystemDebug_RefreshModuleStates | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| records | 103 + 309/Project/Source/Flash.c | 36 | 6 | EEPROM_ResetData_EventRecord_ToDefault, LogEvent_EEPROM, StorageFlash_LoadLogData, StorageFlash_SaveLogData | ReadEEPROM_EventRecord_Parameters, Sci_ACK_0x03_ReadRegs_EventRecord | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| magic_inv | 103 + 309/Project/Source/Flash.c | 42 | 6 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| request_inv | 103 + 309/Project/Source/Flash.c | 44 | 6 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16SocNow | 103 + 309/Project/Source/Flash.h | 63 | 6 | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_load_or_default, soc_save | host_internal_soc | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| scan_timer_enabled | 103 + 309/Project/Source/LedBar.c | 89 | 6 | LedBar_Init, LedBar_StartScanTimer, LedBar_StopScanTimer | APP_LedBar, LedBar_ApplyFrame, LedBar_IsActiveForLowPower | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| records | 103 + 309/Project/Source/LogRecord.c | 8 | 6 | EEPROM_ResetData_EventRecord_ToDefault, LogEvent_EEPROM, StorageFlash_LoadLogData, StorageFlash_SaveLogData | ReadEEPROM_EventRecord_Parameters, Sci_ACK_0x03_ReadRegs_EventRecord | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u8FlashUpdateFlag | 103 + 309/Project/Source/Sci_Upper.c | 26 | 6 | App_FlashUpdate, Sci_PortFinishTx, feidao_can_service_enter_iap_delay | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| pstProtocolOps | 103 + 309/Project/Source/Sci_Upper.c | 56 | 6 |  | Sci_InitCommonPort, Sci_PortAbortTransfer, Sci_PortFinishTx, Sci_PortIRQHandler, Sci_PortIsBusy, Sci_PortService | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u8FlashUpdateFlag | 103 + 309/Project/Source/Sci_Upper.h | 478 | 6 | App_FlashUpdate, Sci_PortFinishTx, feidao_can_service_enter_iap_delay | LP_GetBlockReason, Sci_DebugWatchBind, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| u16SocNow | 103 + 309/Project/Source/SocEnhance.c | 15 | 6 | SOC_ResetStoredSnapshotToDefault, StorageFlash_LoadSocData, host_set_snapshot, soc_load_or_default, soc_save | host_internal_soc | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| rest_soc_ticks | 103 + 309/Project/Source/SocEnhance.c | 94 | 6 | SOC_GetDebugInternals, soc_reset_rest_confidence | soc_apply_long_rest_down_step, soc_apply_rtc_rest_ocv, soc_update_rest_timer, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| snapshot_flags | 103 + 309/Project/Source/SocEnhance.c | 104 | 6 | soc_load_or_default, soc_update_sag_hold, soc_update_save_mark | soc_save, soc_save_mark_changed, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| snapshot_flags | 103 + 309/Project/Source/SocEnhance.c | 123 | 6 | soc_load_or_default, soc_update_sag_hold, soc_update_save_mark | soc_save, soc_save_mark_changed, soc_watch_refresh | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| fault | 103 + 309/Project/Source/SystemDebug.c | 58 | 6 | DebugHooks_RuntimeRecordEvents, Fault_DebugWatchBind, SystemDebug_Event, SystemDebug_Snapshot | SH367309_RecordFaultOnActive, SystemDebug_RefreshModuleStates | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| cap_now | 103 + 309/Project/Source/SystemDebug.h | 239 | 6 | CanFeidao_SendStatus5000ms, SystemDebug_Snapshot, host_pack_step, host_run_scenario | DbgPrint_SOC, host_true_soc | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| ichg | 103 + 309/Project/Source/SystemDebug.h | 242 | 6 | SystemDebug_Snapshot | DbgPrint_SOC, DbgPrint_Summary, SOC_UpdateSampleData, host_run_seconds, host_tick | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| idsg | 103 + 309/Project/Source/SystemDebug.h | 243 | 6 | SystemDebug_Snapshot | DbgPrint_SOC, DbgPrint_Summary, SOC_UpdateSampleData, host_run_seconds, host_tick | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_system_onoff_func | 103 + 309/Project/Source/System_Monitor.c | 4 | 6 | InitSystemMonitorData_EEPROM, SystemFeature_SetById | SystemFeature_GetMask, SystemFeature_IsSocFixed, SystemFeature_IsSocZero, SystemMonitor_DebugWatchBind | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| cycles | 103 + 309/Project/Source/rtc_sleep.h | 52 | 6 | CanFeidao_SendSoh5000ms, rtc_sleep_prepare_rtc, rtc_sleep_run_hiccup_cycle, soc_export_public_fields | SystemDebug_CycCntToUs, SystemDebug_Snapshot | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| magic_inv | firmware/comm_tool_f103ret6/source/app/ct_boot_control.h | 8 | 6 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| request_inv | firmware/comm_tool_f103ret6/source/app/ct_boot_control.h | 10 | 6 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| app_addr | firmware/comm_tool_f103ret6/source/app/ct_flash_store.h | 8 | 6 | CtFlash_Begin, CtUpgrade_StartWithAppAddress, app_addr_supported, handle_fw_begin | handle_fw_info, valid_app_vector | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_rx_tail | firmware/comm_tool_f103ret6/source/bsp/board_can.c | 12 | 6 | BoardUart_Init, BoardUart_ReadByte, can_hw_init, can_rx_pop | can_rx_push, rx_push | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_rx_tail | firmware/comm_tool_f103ret6/source/bsp/board_uart.c | 13 | 6 | BoardUart_Init, BoardUart_ReadByte, can_hw_init, can_rx_pop | can_rx_push, rx_push | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| magic_inv | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 80 | 6 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| request_inv | firmware/comm_tool_f103ret6/source/iap/ct_iap.c | 82 | 6 | AppUpgrade_RequestIap, CtBoot_ClearRequest, CtBoot_RequestIap, boot_consume_iap_request | AppUpgrade_IsIapRequested, boot_request_valid | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_flash_soc | tools/soc_host_c_test.c | 24 | 6 | StorageFlash_SaveSocData, host_set_snapshot | StorageFlash_LoadSocData, host_internal_soc, host_reset_state, test_rebound_flag_clears_when_holdoff_expires | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| s_flash_soc | tools/soc_host_visual_trace.c | 20 | 6 | StorageFlash_SaveSocData, host_set_snapshot | StorageFlash_LoadSocData, host_internal_soc, host_reset_state, test_rebound_flag_clears_when_holdoff_expires | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| OtherElement | 103 + 309/Project/Source/DataDeal.h | 200 | 0 |  |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SH367309_Reg_Store | 103 + 309/Project/Source/SH367309_Func.h | 239 | 0 |  |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| g_stCellInfoReport | 103 + 309/Project/Source/Sci_Upper.h | 482 | 0 |  |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| SOC_Enhance_Element | 103 + 309/Project/Source/SocEnhance.h | 107 | 0 |  |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| sys_time | 103 + 309/Project/Source/conf/conf.h | 129 | 0 |  |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| sys_time | build/host_tests/conf_board_self_0/conf.h | 129 | 0 |  |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| sys_time | build/host_tests/conf_board_self_1000/conf.h | 129 | 0 |  |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |
| sys_time | build/host_tests/conf_board_self_30/conf.h | 129 | 0 |  |  | 关键共享状态，后续重构优先保持字段语义和协议映射不变 |


## 单位与并发提示

- `g_stCellInfoReport.u16VCell[]`：单体电压，源码按mV填充；`61001` 表示未用串。
- `g_stCellInfoReport.u16Ichg/u16IDischg`：报告电流为0.1A，`DataLoad_CurrentMilliAmpToA10()` 从mA四舍五入得到。
- `SOC_Enhance_Element.u16_Ichg/u16_Idsg`：SOC内部使用的0.1A电流，已经扣除Type-C等效电流后的净值。
- `s_soc.*_ticks`：SOC 200ms tick；`g_stLowPowerRtcStatus.*` 部分字段是秒计数，部分是状态/原因位。
- `sys_time.can_rcv_cnt` 在CAN ISR写，`Can_IsBusy()` 读取并更新 `last_ext_comm_cnt_can`；这是低功耗外部通信busy判定的副作用查询。

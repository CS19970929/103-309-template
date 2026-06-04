# 03 主循环运行流程

状态：已按 `Runtime_RunOnce()` 源码顺序梳理。参考源码：`Runtime.c`、`System_Init.c`、`DataDeal.c`、`SH367309_Func.c`、`SOC.c`、`SocEnhance.c`、`Sci_Upper.c`、`ADC.c`、`rtc_sleep.c`、`Can_HDX.c`、`Flash.c`、`LogRecord.c`。

## 目录
- [主循环顺序](#主循环顺序)
- [周期任务表](#周期任务表)
- [AFE数据采集流程](#afe数据采集流程)
- [保护与MOS流程](#保护与mos流程)
- [SOC流程](#soc流程)
- [通信流程](#通信流程)
- [低功耗流程](#低功耗流程)
- [存储与日志流程](#存储与日志流程)

## 主循环顺序

```text
Runtime_RunOnce
└── Runtime_RunNormalOnce
    ├── Runtime_RunFrontTasks
    │   ├── SysTime_LatchTaskFlags
    │   ├── DebugHooks_RuntimeAfterSysTime
    │   ├── FactoryAging_Task
    │   ├── APP_LedBar
    │   ├── App_AFEGet
    │   └── DebugHooks_RuntimeSnapshot
    ├── Runtime_RunIoAndPowerTasks
    │   ├── AppInit_ServiceSci -> App_CommonUpper
    │   ├── App_AnlogCal
    │   ├── rtc_sleep
    │   └── App_Can
    ├── Runtime_RunBackgroundTasks
    │   ├── App_FlashUpdate
    │   ├── App_LogRecord
    │   ├── App_ProID_Deal
    │   └── Feed_IWatchDog
    └── DebugHooks_RuntimeDebugPrint
```

## 周期任务表

| 函数 | 频率/触发 | 条件 | 主要行为 | 关键状态 | 阻塞风险 | 影响 |
| --- | --- | --- | --- | --- | --- | --- |
| SysTime_LatchTaskFlags | 每圈最先执行 | 主循环每圈 | 原子读取s_st_SysTimePending到g_st_SysTimeFlag并清pending | g_st_SysTimeFlag | 关中断极短临界区 | 后续任务只看本轮锁存标志 |
| FactoryAging_Task | 每圈，内部用10ms tick累计 | PROJECT_CFG_FACTORY_AGING_ENABLE=1 | 老化状态机、MOS维持、进度保存 | s_factory_aging, Flash, MOS | 可能Flash保存 | 影响MOS/低功耗blocker/CAN aging命令 |
| APP_LedBar | 每圈，内部100ms刷新/1ms ISR扫描 | 显示请求、按键、唤醒窗口 | LED帧、按键滤波、SOC显示 | s_ledbar | 不应长阻塞 | 影响低功耗LED blocker |
| App_AFEGet | 200ms pending period | SysTime_Take200msTaskPeriod()!=0 | AFE采样、电压/温度/电流、保护、MOS、SOC | g_stCellInfoReport, s_data, SH367309_Reg_Store, SOC状态 | AFE I2C/MTP可能等待；异常时恢复AFE | 保护/SOC/MOS核心链路 |
| AppInit_ServiceSci -> App_CommonUpper | 每圈 | SCI端口busy/帧pending | 解析Modbus请求、构造应答、执行写寄存器副作用 | SCI端口状态、g_stCellInfoReport、参数、u8FlashUpdateFlag | TX由中断推进；业务处理可能写Flash | 通信协议和参数写入口 |
| App_AnlogCal | 有10ms tick增量时 | SysTime_Get10msTickCount变化 | 处理DMA ADC采样，更新MOS温、总压、Type-C电流 | s_adc | 无长阻塞 | 给SOC Type-C等效放电/熔断逻辑提供输入 |
| rtc_sleep | 每秒判断 | g_st_SysTimeFlag.b1Sys1000msFlag | 低功耗blocker、idle计数、HICCUP/NORMAL/DEEP模式执行 | g_stLowPowerRtcStatus, sys_time, s_sleep | HICCUP会进入STOP循环；NORMAL/DEEP会reset | 强影响低功耗/SOC休眠补偿/日志 |
| App_Can | 每圈 | 10ms tick用于周期调度和超时 | 周期帧、APP命令、读块流、TX队列、IAP延迟 | s_tx, s_runtime, s_app, sys_time | CAN取消TX有短等待 | CAN通信/低功耗busy/IAP入口 |
| App_FlashUpdate | 每圈 | u8FlashUpdateFlag==1且_IAP | 关MOS、10ms延时、复位进IAP | u8FlashUpdateFlag | __delay_ms(10) | 影响IAP/启动地址安全 |
| App_LogRecord | 1000ms flag | 每秒 | 故障/启动/睡眠事件防抖和Flash日志保存 | s_log_record, su32_Interval_S_Tcnt | Flash写可能占用 | 存储日志/低功耗blocker |
| App_ProID_Deal | 每圈 | 无 | 当前为空心跳占位 | ProductionInfor只在InitProID写 | 无 | 协议读取生产信息 |
| Feed_IWatchDog/IWDG_Feed | 每圈后台末尾、延时中也调用 | PROJECT_CFG_WDOG_ENABLE=1 | 刷新IWDG并记录debug feed源 | SystemDebug/硬件IWDG | 无 | 防止长延时/主循环卡死复位 |


## AFE数据采集流程

`App_AFEGet()` 只在 `SysTime_Take200msTaskPeriod()` 成功时执行。当前200ms链路：

```text
App_AFEGet
├── MCUO_DEBUG_LED1 toggle
├── MonitorAFE(0, UpdateVoltageFromBqMaximo())
│   ├── UpdateVoltageFromBqMaximo: MTPRead AFE温度/电芯/CADC原始值
│   └── MonitorAFE: AFE错误计数、恢复、System_ErrFlag、低功耗请求
├── DataLoad_CellVolt
├── DataLoad_CellVoltMaxMinFind
├── DataLoad_Temperature
├── DataLoad_TemperatureMaxMinFind
├── DataLoad_Current
│   ├── 原始CADC符号转换
│   ├── 运行期auto-zero
│   ├── mA换算和0.1A报告
│   └── __VIRTURE_CURRENT__ 调试电流覆盖
├── AfeCurrent_NextSeq
├── App_SH367309
├── new_todo_logi
└── App_SOC
```

关键数据输出：`g_stCellInfoReport.u16VCell[]`、`u16VCellMax/Min/Delta/Totle`、`u16Temperature[]`、`u16Ichg/u16IDischg`、`SH367309_Reg_Store`、`SOC_Enhance_Element`。单位需要按字段分别看：电芯mV、总压字段为0.01V或由代码除以10后的协议值、电流报告为0.1A。

## 保护与MOS流程

- `App_SH367309_Monitor()` 读取 `MTP_BALANCEH` 起的5字节状态，更新 `SystemRuntime_SetMosStatus()`，把AFE硬件故障转换成 `g_stCellInfoReport.unMdlFault_Third`，并记录 `Fault_record_Third2`。
- `SH367309_UpdataAfeConfig()` 由 `AFE_PARAM_WRITE_Flag` 触发，写AFE ROM参数，失败会重置flag并置错误。
- `new_todo_logi()` 处理充电检测、MOS过温关断/恢复、认证熔断和AFE错误时关闭 `CTLC`，属于保护/MOS/低功耗耦合点。
- `MosStartup_ApplyInitialState()` 只在初始化AFE时执行，根据5V充电和老化状态决定初始MOS。

## SOC流程

`App_SOC()` 通过 `AfeCurrent_GetSeq()` 判断是否有新的AFE电流样本。新样本才执行 `SOC_IntEnhance_Ctrl()`；否则仅发布已有SOC报告。`SOC_GetNetCurrentForCalc()` 会把 Type-C 输出折算成电池侧等效放电电流。

`SOC_IntEnhance_Ctrl()` 顶层顺序固定：命令处理 -> 方向判定 -> 库仑积分 -> sag hold -> 低电尾段/满电校准 -> 静置OCV -> 保存快照 -> 发布显示SOC。这个顺序是后续重构必须保留的行为边界。

## 通信流程

- SCI：USART ISR 负责收发推进和置帧状态，主循环 `App_CommonUpper()` 执行Modbus帧处理。写保护/OtherElement/SOC/RTC/SN/Flash connect寄存器有直接副作用，例如 `InitData_SOC()`、`SOC_RequestCapacityReset()`、`AFE_PARAM_WRITE_Flag=1`、`u8FlashUpdateFlag`。
- CAN：RX0 ISR只收帧入 `s_app.cmd_queue`；主循环 `App_Can()` 处理命令。APP命令可读写Modbus寄存器、开始/停止老化、请求IAP，并通过TX队列发ACK/读块流。

## 低功耗流程

`rtc_sleep()` 每秒运行：先 `lp_select()` 计算 blocker，再根据 `g_stLowPowerRtcStatus.mode` 进入 `NORMAL_MODE`/`DEEP_MODE` reset-sleep 或 `HICCUP_MODE` RTC STOP循环。blocker 包含充放电电流、SCI/CAN忙、按键、外部通信计数变化、老化、Flash忙、IAP、三层故障、LED活动。

## 存储与日志流程

- 参数：`InitE2PROM()` 启动加载；SCI/CAN写寄存器时通过 `EEPROM_SaveRWParametersToFlash()`、`StorageFlash_SaveAfeData()` 等保存。
- SOC：`soc_save_if_needed()` 在SOC核心流程中按 `SOC_SAVE_MARK` 变化保存。
- 日志：`App_LogRecord()` 每秒防重复写事件；`LogRecord_RequestStartup/Sleep` 置启动/休眠日志请求。
- IAP：`u8FlashUpdateFlag` 由SCI/CAN或Flash connect路径置位，`App_FlashUpdate()` 关MOS后复位。

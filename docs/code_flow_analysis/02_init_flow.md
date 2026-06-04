# 02 初始化阶段流程

状态：已按 `AppInit_Boot()` 进入 `while(1)` 前的真实顺序梳理。参考源码：`AppInit.c`、`System_Init.c`、`conf.c`、`EEPROM.c`、`I2C_AFE1.c`、`Can_HDX.c`、`ADC.c`、`SOC.c`、`SocEnhance.c`、`RTC.c`。

## 目录
- [初始化顺序总览](#初始化顺序总览)
- [关键宏状态](#关键宏状态)
- [初始化函数明细](#初始化函数明细)
- [隐藏副作用](#隐藏副作用)

## 初始化顺序总览

| 顺序 | 函数 | 模块 | 是否可能阻塞/等待 | 核心副作用 |
| --- | --- | --- | --- | --- |
| 1 | SystemInit | STM32系统时钟/CMSIS初始化；由启动文件先调用一次，业务初始化又调用一次 | 未检查返回；硬故障由异常处理接管 | 否；但会重配系统时钟 |
| 2 | InitDelay | 配置SysTick延时系数 fac_us/fac_ms | 无返回；参数来自 SystemCoreClock | __delay_ms 会阻塞并喂狗 |
| 3 | IsSleepStartUp | 读取BKP sleep flag，若上次 reset-sleep 则配置IO/EXTI并进入STOP等待合法唤醒 | 无返回；可能在启动阶段长时间停留STOP/按键预览循环 | 高：会影响启动路径和LED预览 |
| 4 | InitNVIC | 设置中断优先级分组 NVIC_PriorityGroup_1 | 无返回 | 影响所有ISR抢占关系 |
| 5 | InitIO | 初始化运行态GPIO、电源轨、AFE CTLC、ADC模拟输入、debug LED等 | 无返回 | 会改变电源轨和AFE控制脚 |
| 6 | InitUSART_CommonUpper | 初始化启用SCI端口和Modbus协议状态 | 无返回 | PROJECT_CFG_SCI1_ROLE=1 启用SCI1；SCI2/3未见当前启用宏 |
| 7 | InitE2PROM | 加载默认参数，再从Flash加载RW参数、AFE参数、事件记录并执行升级参数策略 | 加载失败会置System_ErrFlag并尝试保存默认 | 影响保护阈值、SOC配置、AFE写回标志 |
| 8 | InitAFE1 | 初始化SH367309 I2C/AFE，关闭CTLC、复位AFE、写参数、初始MOS、冷/热启动电流零点校准 | AFE_IsReady等待最多约1s；零点校准含多次ms延时 | 高：可能阻塞、控制MOS/AFE |
| 9 | InitCan | 初始化CAN TX/app队列、GPIO、NVIC、CAN1和全接收滤波 | CAN_Init返回未上报 | 影响CAN收发、低功耗busy判定 |
| 10 | InitADC | 清零ADC运行态，配置GPIO、TIM2触发、DMA1、ADC1校准 | 校准超时置ERROR_ADC并关闭ADC | ADC结果供总压/MOS温/Type-C电流 |
| 11 | InitData_SOC | 从OtherElement装载SOC配置并初始化增强SOC状态机 | StorageFlash加载失败则默认/OCV并保存 | 影响SOC初值、SOH、容量、发布字段 |
| 12 | InitTimer | 配置TIM3 10ms tick，清零pending标志并启动中断 | 无返回 | 主循环所有周期任务基准 |
| 13 | __enable_irq | 开启总中断 | 无返回 | 从此TIM3/USART/CAN/RTC等ISR可进入 |
| 14 | EnableLowPowerDebug | 按 __EnableLowPowerDebug__ 控制 DBGMCU sleep/stop/standby debug bit | 无返回 | 当前conf.h定义该宏，低功耗调试保持开启 |
| 15 | AppInit_InitRuntimeState | 初始化系统状态、版本、LED、生产ID、启动日志请求 | 无返回 | 将BMS状态标记为boot ready |
| 16 | Init_RTC | 配置备份域、RTC时钟、时间、秒中断、闹钟 | 时钟失败直接返回并留错误状态 | RTC秒中断和STOP唤醒基础 |


## 关键宏状态

| 宏名 | 值 | 状态 | 定义文件 | 行 |
| --- | --- | --- | --- | --- |
| PROJECT_CFG_BUILD_PROFILE | 0 | 已定义，值为0（逻辑关闭） | 103 + 309/Project/Source/conf/Project_BuildGuard.h | 19 |
| PROJECT_CFG_BAT_TYPE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 14 |
| PROJECT_CFG_BAT_CHEMISTRY | 0 | 已定义，值为0（逻辑关闭） | 103 + 309/Project/Source/conf/Project_Config.h | 19 |
| PROJECT_CFG_HOST_WRITE_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_BuildGuard.h | 11 |
| PROJECT_CFG_WDOG_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 56 |
| PROJECT_CFG_RTC_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 59 |
| PROJECT_CFG_IAP_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 62 |
| PROJECT_CFG_FACTORY_AGING_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 65 |
| PROJECT_CFG_VIRTUAL_CURRENT_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 71 |
| PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 74 |
| PROJECT_CFG_DEBUG_WATCH_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_BuildGuard.h | 23 |
| PROJECT_CFG_DEBUG_MONITOR_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_BuildGuard.h | 27 |
| PROJECT_CFG_IRQ_DEBUG_ENABLE | 0 | 已定义，值为0（逻辑关闭） | 103 + 309/Project/Source/conf/Project_BuildGuard.h | 31 |
| PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_BuildGuard.h | 35 |
| PROJECT_CFG_UART1_WAKEUP_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 99 |
| PROJECT_CFG_RS485_WAKEUP_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 102 |
| PROJECT_CFG_SCI1_ROLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 110 |
| PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS | 15 | 已定义 | 103 + 309/Project/Source/conf/Project_Config.h | 135 |
| PROJECT_CFG_SOC_REST_OCV_SECONDS | 1800 | 已定义 | 103 + 309/Project/Source/conf/Project_Config.h | 171 |
| PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA | 30 | 已定义 | 103 + 309/Project/Source/conf/Project_BuildGuard.h | 7 |
| PROJECT_CFG_LEDBAR_SLEEP_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 213 |
| PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 226 |
| PROJECT_CFG_UPGRADE_PARAM_RESET_AFE | 0 | 已定义，值为0（逻辑关闭） | 103 + 309/Project/Source/conf/Project_Config.h | 232 |
| PROJECT_CFG_UPGRADE_PARAM_RESET_PROTECT | 0 | 已定义，值为0（逻辑关闭） | 103 + 309/Project/Source/conf/Project_Config.h | 235 |
| PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG | 0 | 已定义，值为0（逻辑关闭） | 103 + 309/Project/Source/conf/Project_Config.h | 241 |
| PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT | 0 | 已定义，值为0（逻辑关闭） | 103 + 309/Project/Source/conf/Project_Config.h | 244 |
| PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD | 1 | 已定义，值为1（逻辑开启） | 103 + 309/Project/Source/conf/Project_Config.h | 247 |
| PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME | 0 | 已定义，值为0（逻辑关闭） | 103 + 309/Project/Source/conf/Project_Config.h | 250 |
| TERNARYLI | #elif (PROJECT_CFG_BAT_CHEMISTRY == 1) | 已定义 | 103 + 309/Project/Source/conf/conf.h | 30 |
| LIFEPO | #else | 已定义 | 103 + 309/Project/Source/conf/conf.h | 32 |
| wdog_enable | #endif | 已定义 | 103 + 309/Project/Source/conf/conf.h | 38 |
| __FUNC_RTC__ | #endif | 已定义 | 103 + 309/Project/Source/conf/conf.h | 42 |
| UART1_WAKEUP_ENABLE | #endif | 已定义 | 103 + 309/Project/Source/conf/conf.h | 46 |
| RS485_WAKEUP_ENABLE | #endif | 已定义 | 103 + 309/Project/Source/conf/conf.h | 50 |
| __VIRTURE_CURRENT__ | #endif | 已定义 | 103 + 309/Project/Source/conf/conf.h | 54 |
| _DI_SWITCH_longKEY_ONOFF | #endif | 已定义 | 103 + 309/Project/Source/conf/conf.h | 58 |
| _IAP | #endif | 已定义 | 103 + 309/Project/Source/conf/conf.h | 62 |
| _COMMOM_UPPER_SCI1 | #elif (PROJECT_CFG_SCI1_ROLE != 0) | 已定义 | 103 + 309/Project/Source/conf/conf.h | 66 |
| _COMMOM_UPPER_SCI2 |  | 未定义/当前分支未启用 | 未找到定义 |  |
| _COMMOM_UPPER_SCI3 |  | 未定义/当前分支未启用 | 未找到定义 |  |
| DISP_VBAT_AND_TEMP_ |  | 未定义/当前分支未启用 | 未找到定义 |  |
| _UL_RENZHENG_ENABLE_ | #define DISP_VBAT_AND_TEMP_ | 已定义 | 103 + 309/Project/Source/conf/conf.h | 10 |
| __EnableLowPowerDebug__ | #define EEPROM_VALUE_BEGIN_FLAG PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG | 已定义 | 103 + 309/Project/Source/conf/conf.h | 13 |
| AFE_CURRENT_KB_CALIB_ENABLE | 0U | 已定义 | 103 + 309/Project/Source/DataDeal.c | 19 |


## 初始化函数明细

| 函数 | 调用位置 | 初始化内容 | 修改/依赖变量 | 寄存器/外设 | 失败行为 | 隐藏副作用 |
| --- | --- | --- | --- | --- | --- | --- |
| SystemInit | startup Reset_Handler 与 AppInit_InitDevice | STM32系统时钟/CMSIS初始化；由启动文件先调用一次，业务初始化又调用一次 | SystemCoreClock 等标准库全局 | HSE/PLL/SysClk，具体见 system_stm32f10x.c | 未检查返回；硬故障由异常处理接管 | 否；但会重配系统时钟 |
| InitDelay | AppInit_InitDevice / STOP恢复 | 配置SysTick延时系数 fac_us/fac_ms | fac_us, fac_ms | SysTick CTRL/LOAD/VAL 在延时函数中使用 | 无返回；参数来自 SystemCoreClock | __delay_ms 会阻塞并喂狗 |
| IsSleepStartUp | AppInit_InitDevice | 读取BKP sleep flag，若上次 reset-sleep 则配置IO/EXTI并进入STOP等待合法唤醒 | s_sleep.boot_sleep, s_sleep.chg_wake, BKP_DR2/DR3 | PWR/BKP/EXTI/GPIO/STOP | 无返回；可能在启动阶段长时间停留STOP/按键预览循环 | 高：会影响启动路径和LED预览 |
| InitNVIC | AppInit_InitDevice | 设置中断优先级分组 NVIC_PriorityGroup_1 | 无业务全局 | NVIC priority grouping | 无返回 | 影响所有ISR抢占关系 |
| InitIO | AppInit_InitDevice | 初始化运行态GPIO、电源轨、AFE CTLC、ADC模拟输入、debug LED等 | sys_time间接无 | GPIOA/B/C/D/E、AFIO、电源使能脚 | 无返回 | 会改变电源轨和AFE控制脚 |
| InitUSART_CommonUpper | AppInit_InitDevice via AppInit_InitSci宏 | 初始化启用SCI端口和Modbus协议状态 | g_stSciPort1, g_stCurrentMsgPtr_SCI1, g_u8SCITxBuff | USART1/GPIO/NVIC；SCI2/3按宏分支 | 无返回 | PROJECT_CFG_SCI1_ROLE=1 启用SCI1；SCI2/3未见当前启用宏 |
| InitE2PROM | AppInit_InitDevice | 加载默认参数，再从Flash加载RW参数、AFE参数、事件记录并执行升级参数策略 | PRT_E2ROMParas, OtherElement, g_u16CalibCoefK, g_i16CalibCoefB, SeriesNum, g_u32CS_Res_AFE | 内部Flash页/双槽journal | 加载失败会置System_ErrFlag并尝试保存默认 | 影响保护阈值、SOC配置、AFE写回标志 |
| InitAFE1 | AppInit_InitDevice | 初始化SH367309 I2C/AFE，关闭CTLC、复位AFE、写参数、初始MOS、冷/热启动电流零点校准 | s_data.cur, SH367309_Reg_Store, AFE_PARAM_WRITE_Flag | GPIOB PB8/PB9 bit-bang I2C、AFE MTP、MOS | AFE_IsReady等待最多约1s；零点校准含多次ms延时 | 高：可能阻塞、控制MOS/AFE |
| InitCan | AppInit_InitDevice | 初始化CAN TX/app队列、GPIO、NVIC、CAN1和全接收滤波 | s_tx, s_runtime, s_app | CAN1、GPIOA11/12、CMNT_EN、USB_LP_CAN1_RX0_IRQn | CAN_Init返回未上报 | 影响CAN收发、低功耗busy判定 |
| InitADC | AppInit_InitDevice / STOP恢复 | 清零ADC运行态，配置GPIO、TIM2触发、DMA1、ADC1校准 | s_adc | DMA1_CH1、TIM2、ADC1、GPIOA/PB | 校准超时置ERROR_ADC并关闭ADC | ADC结果供总压/MOS温/Type-C电流 |
| InitData_SOC | AppInit_InitDevice | 从OtherElement装载SOC配置并初始化增强SOC状态机 | SOC_Enhance_Element, s_soc, s_saved_soc | Flash SOC snapshot | StorageFlash加载失败则默认/OCV并保存 | 影响SOC初值、SOH、容量、发布字段 |
| InitTimer | AppInit_InitDevice | 配置TIM3 10ms tick，清零pending标志并启动中断 | g_st_SysTimeFlag, s_st_SysTimePending, s_u32Sys10msTickCount | TIM3/NVIC | 无返回 | 主循环所有周期任务基准 |
| __enable_irq | AppInit_InitDevice | 开启总中断 | 无 | PRIMASK | 无返回 | 从此TIM3/USART/CAN/RTC等ISR可进入 |
| EnableLowPowerDebug | AppInit_InitDevice | 按 __EnableLowPowerDebug__ 控制 DBGMCU sleep/stop/standby debug bit | 无 | DBGMCU->CR | 无返回 | 当前conf.h定义该宏，低功耗调试保持开启 |
| AppInit_InitRuntimeState | AppInit_Boot | 初始化系统状态、版本、LED、生产ID、启动日志请求 | s_system_status, s_system_onoff_func, g_u32CS_Res_AFE, ProductionInfor, s_log_record | 无直接外设 | 无返回 | 将BMS状态标记为boot ready |
| Init_RTC | AppInit_Boot末尾 | 配置备份域、RTC时钟、时间、秒中断、闹钟 | s_rtc, RTC_time, BKP_DR1 | PWR/BKP/RCC/RTC/NVIC/EXTI17 | 时钟失败直接返回并留错误状态 | RTC秒中断和STOP唤醒基础 |


## 隐藏副作用

- `IsSleepStartUp()` 可能在启动阶段进入 STOP 等待合法按键或充电唤醒，不是普通“读取标志”函数。
- `InitE2PROM()` 不访问外置EEPROM；当前实现使用内部Flash storage pair/journal，`ReadEEPROM_*` 旧接口返回固定值。
- `InitAFE1()` 会关闭 `CTLC`、复位AFE、写参数、应用初始MOS状态，并可能执行电流零点校准。
- `InitTimer()` 在 `__enable_irq()` 前启动TIM3配置，但真正中断在总中断打开后生效。
- `EnableLowPowerDebug()` 当前因 `__EnableLowPowerDebug__` 定义而保留 STOP/SLEEP debug，这会影响实测低功耗电流。

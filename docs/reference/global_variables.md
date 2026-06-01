# BMS 项目全局变量总览

> 基于完整源码分析, 列出所有跨文件的全局变量
> 更新日期: 2026-06-01

---

## 变量分类索引

| 类别 | 数量 | 描述 |
|------|------|------|
| 系统时基与调试 | ~30 | sys_time, g_st_SysTimeFlag |
| 系统状态与错误 | ~26 | System_ErrFlag, s_system_status |
| AFE 数据 | ~10 | SH367309_Read_AFE1, Registers_AFE1 |
| 电池数据与校准 | ~100+ | g_stCellInfoReport, g_u16CalibCoefK/B |
| SOC 核心 | ~35 | SOC_Enhance_Element |
| 保护参数 | ~70+ | PRT_E2ROMParas, Fault_Flag_* |
| ADC | ~4 | g_i32ADCResult, g_u32Vbat_mV |
| CAN | ~2 | g_stCanErrorSnapshot, g_stCanLowPowerStatus |
| 低功耗 | ~6 | g_stLowPowerRtcStatus, RTC_ExtComCnt |
| LED | ~1 | key_release_wakeup |
| Flash/升级 | ~2 | u8FlashUpdateFlag, u8FlashUpdateE2PROM |
| 日志 | ~1 | su32_Interval_S_Tcnt |
| 老化 | ~6 | s_u8FactoryAgingState (静态全局) |

---

## 1. 系统时基与调试 (conf.c)

```c
Time_T sys_time;  // 全局系统时间/计数器
```

| 字段 | 类型 | 说明 |
|------|------|------|
| can_rcv_cnt | uint32_t | CAN 接收帧计数 |
| test_driver_cnt | uint32_t | 驱动测试计数 |
| test_main_cycle | uint64_t | 主循环计数 |
| App_AFEGet_cnt | uint32_t | AFE 获取次数 |
| App_SH367309_Monitor_cnt | uint32_t | AFE 监控次数 |
| App_beep_cnt | uint32_t | 蜂鸣器计数 |
| sci1_irq_cnt | uint32_t | 串口1中断计数 |
| sci2_irq_cnt | uint32_t | 串口2中断计数 |
| sci3_irq_cnt | uint32_t | 串口3中断计数 |
| cnt_PA0_irq | uint16_t | PA0中断计数 |
| cnt_bms1_keyirq | uint16_t | 按键中断计数 |
| pec_err_cnt | uint16_t | PEC错误计数 |
| isdebugenable | uint8_t | 调试使能 |
| CHG | uint16_t | 充电状态 |
| DSG | uint16_t | 放电状态 |
| cnt_enter_chg_open | uint16_t | 充电MOS开次数 |
| cnt_enter_dsg_open | uint16_t | 放电MOS开次数 |
| wakeup_reason | uint8_t | 唤醒原因 |
| wakeup_rtc | bool | RTC唤醒标志 |
| time_enter_rtc | uint8_t | 进入RTC时间 |
| power_on | bool | 上电标志 |
| test_cnt1 | uint16_t | 测试计数 |
| enter_rtc_delay | uint16_t | 进入RTC延迟 |
| rtc_sleep_cnt | uint32_t | RTC休眠次数 |
| rtc_sec_cnt | uint32_t | RTC秒中断计数 |
| rtc_alm_cnt | uint32_t | RTC闹钟计数 |
| rtc_irq_cnt | uint32_t | RTC中断计数 |
| typc_curr | uint16_t | TypeC电流 |

---

## 2. 时基标志 (System_Init.c)

```c
volatile union SYS_TIME g_st_SysTimeFlag;
```

| 位字段 | 说明 |
|--------|------|
| b1Sys10msFlag | 10ms 任务标志 |
| b1Sys50msFlag | 50ms 任务标志 |
| b1Sys100msFlag | 100ms 任务标志 |
| b1Sys200msFlag | 200ms 任务标志 |
| b1Sys1000msFlag | 1000ms 任务标志 |

---

## 3. 系统状态与错误 (System_Monitor.c)

```c
volatile struct SYSTEM_ERROR System_ErrFlag;
```

包含 23 个 UINT8 错误标志: AFE1, AFE2, CAN, EEPROM, SPI, Upper, Client, Screen, Wifi, Bluetooth, App, CBC_CHG, CBC_DSG, EEPROM_Store, HSE, LSE, Vdelta, Balance, ADC, TempBreak, SOC_Cal, 等

```c
static volatile union System_Status s_system_status;      // 系统状态
static volatile union System_OnOFF_Function s_system_onoff_func; // 功能开关
```

System_Status 位:
- b1StartUpBMS, b1Status_MOS_PRE/CHG/DSG, b1Status_AFE1/2, b1Status_Balance, b1Status_ToSleep, b1Status_BnCloseIO, b1Status_CBCCloseIO, b4Status_ProjectVer

System_OnOFF_Function 位:
- b1OnOFF_Balance, b1OnOFF_MOS_Relay, b1OnOFF_SOC_Fixed, b1OnOFF_SOC_Zero, b1OnOFF_Sleep, b1OnOFF_AFE1/2

---

## 4. AFE 数据

```c
// I2C_AFE1.c
struct SH367309_Read SH367309_Read_AFE1;  // AFE读取的电压/电流/温度
AFEDATA Registers_AFE1;                    // AFE寄存器原始数据

// SH367309_Func.c
SH367309_REG_STORE SH367309_Reg_Store;     // AFE完整寄存器存储
```

---

## 5. 电池数据与校准 (DataDeal.c)

```c
UINT16 g_u16CalibCoefK[KB_NUM];       // 校准K系数 (47个, 每节电压/电流/温度)
INT16  g_i16CalibCoefB[KB_NUM];       // 校准B系数 (47个)
UINT16 CopperLoss[CompensateNUM];      // 铜损补偿值 (16个)
UINT16 CopperLoss_Num[CompensateNUM];  // 铜损补偿数量 (16个)
struct OTHER_ELEMENT OtherElement;     // 其他可配置参数 (32个字段)
UINT32 g_u32CS_Res_AFE;               // AFE电流采样电阻比
UINT32 g_u32AfeCurrentSampleSeq;      // AFE电流采样序号
UINT16 g_u16TypeCOutCurrent_mA;       // TypeC输出电流 (static)
UINT32 g_u32Vbat_mV;                  // 电池总压
```

### OtherElement 字段说明

| 字段 | 默认值 | 说明 |
|------|--------|------|
| u16Balance_OpenVoltage | 4160/3300mV | 均衡开启电压 |
| u16Balance_OpenWindow | 30/50mV | 均衡窗口 |
| u16Balance_CloseWindow | 20mV | 均衡关闭窗口 |
| u16CS_Cur_CHGmax | 计算值 | 充电电流量程 |
| u16CS_Cur_DSGmax | 计算值 | 放电电流量程 |
| u16CBC_DelayT | 2000 | CBC延迟(μs×10) |
| u16CBC_Cur_DSG | 2000 | CBC放电电流(A×10) |
| u16Soc_TableSelect | SOC_TABLE_TERNARYLI | SOC OCV表选择 |
| u16CurLimit_Vdelta | 1000mV | 限流压差 |
| u16CurLimit_Cur | 30(A×10) | 限流电流 |
| u16Sleep_VNormal | 3200mV | 普通休眠电压 |
| u16Sleep_TimeNormal | 7200分钟 | 普通休眠时间 |
| u16Sleep_Vlow | 3000mV | 低压休眠电压 |
| u16Sleep_TimeVlow | 1440分钟 | 低压休眠时间 |
| u16Sleep_VirCur_Chg | 10(A×10) | 休眠虚拟充电电流 |
| u16Sleep_VirCur_Dsg | 10(A×10) | 休眠虚拟放电电流 |
| u16Sleep_RTC_WakeUpTime | 240分钟 | RTC唤醒周期 |
| u16Sleep_TimeRTC | 3分钟 | 进入RTC休眠时间 |
| u16Soc_Ah | BMS_CAPCITY/10 | 电池容量(10×Ah) |
| u16Soc_Cycle_times | 3 | 循环次数 |
| u16Soc_V_100 | 4180/3600mV | SOC=100%电压 |
| u16Soc_V_0 | 3000mV | SOC=0%电压 |
| u16Sys_SeriesNum | 10 | 电池串数 |
| u16Sys_CS_Res | 2mΩ | 采样电阻 |
| u16Sys_CS_Res_Num | 3/4 | 采样电阻数量 |
| u16Sys_PreChg_Time | 10s | 预充电时间 |

---

## 6. 通讯上报数据 (Sci_Upper.c)

```c
struct stCell_Info g_stCellInfoReport;  // 核心全局: 所有上报数据的集合
```

**stCell_Info 结构**:

| 字段 | 说明 |
|------|------|
| u16VCell[32] | 32节电芯电压(mV) |
| u16VCellMax/Min | 最高/最低电芯电压 |
| u16VCellMaxPosition/MinPosition | 位置 |
| u16VCellDelta | 压差(mV) |
| u16VCellTotle | 总压(V×100) |
| u16Temperature[TEMP_NUM] | 温度数组 (+40°C)×10 |
| u16TempMax/Min | 最高/最低温度 |
| u16Ichg | 充电电流 (A×10) |
| u16IDischg | 放电电流 (A×10) |
| SocElement (SOC_CAL_ELEMENT_UPPER) | SOC/SOH/容量/循环 |
| unMdlFault_First/Second/Third | 三级故障标志 |
| u16BalanceFlag1/2 | 均衡标志 |

### 升级标志

```c
UINT8 u8FlashUpdateFlag;       // 非0: 请求跳转IAP
UINT8 u8FlashUpdateE2PROM;     // 非0: 请求写Flash参数
```

---

## 7. SOC 核心 (SocEnhance.c)

```c
struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;
```

| 字段 | 说明 |
|------|------|
| u16_SOC_Ah | 电池容量(10×Ah) |
| u16_SOC_CycleT_Ever/Limit | 循环次数/上限 |
| u16_SOC_TableSelect | OCV表选择 |
| u16_SOC_0_Vol/100_Vol | 0%/100%电压 |
| SOC_Table_CanSet[42] | 运行时OCV表 |
| u8_SetSocOnce | 一次性SOC设置 |
| u16_VCellMax/Min | 电芯电压 |
| u16_Ichg/Idsg | 充放电电流 |
| u16_SOC_InitOver | 初始化完成标志 |
| u8_SOC | 当前SOC(%) |
| u8_SOH | 当前SOH(%) |
| u16_CapacityNow | 当前容量(Ah×100) |
| u16_CapacityFull | 满充容量 |
| u16_CapacityFactory | 出厂容量 |
| u16_Cycle_times | 循环次数 |
| u8_SOC_OCV_Cali | OCV校准标志 |
| u16_RefreshData_Flag | 刷新数据标志 |

### SOC调试观察 (仅 DEBUG_WATCH)

```c
struct SOC_DEBUG_WATCH * const g_dbg_soc_watch;
```

---

## 8. 保护参数 (Fault.c)

```c
struct PRT_E2ROM_PARAS PRT_E2ROMParas;       // 保护参数 (65字)
union FAULT_FLAG_FIRST  Fault_Flag_Fisrt;    // 一级故障
union FAULT_FLAG_SECOND Fault_Flag_Second;   // 二级故障
union FAULT_FLAG_THIRD  Fault_Flag_Third;    // 三级故障

UINT16 Fault_record_Third[Record_len];        // 三级故障记录
UINT16 Fault_record_First2[Record_len];       // 故障记录2
UINT16 Fault_record_Second2[Record_len];
UINT16 Fault_record_Third2[Record_len];

UINT8  FaultPoint_Third;                     // 故障指针
UINT8  FaultPoint_First2;
UINT8  FaultPoint_Second2;
UINT8  FaultPoint_Third2;
```

---

## 9. ADC

```c
// ADC.c
INT32  g_i32ADCResult[ADC_NUM];       // ADC计算结果 (全局)
UINT32 g_u32Vbat_mV;                  // 电池总压(mV)

static UINT16 g_u16TypeCOutCurrent_mA; // TypeC输出电流 (模块内)
```

---

## 10. CAN (仅 DEBUG_WATCH)

```c
volatile struct CAN_ERROR_SNAPSHOT  g_stCanErrorSnapshot;   // CAN错误快照
volatile struct CAN_LOW_POWER_STATUS g_stCanLowPowerStatus;  // CAN低功耗状态
```

---

## 11. 低功耗

```c
// rtc_sleep.c
volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus;

// SleepDeal.c
UINT8 RTC_ExtComCnt;  // 外部通讯计数

// app_lowpower.c (static)
static LP_Runtime_t s_lp_runtime;  // 低功耗运行时状态
```

### LOW_POWER_RTC_STATUS

| 字段 | 说明 |
|------|------|
| mode | 当前模式 (NORMAL/HICCUP/DEEP/NO_SLEEP) |
| readyToSleep | 准备休眠 |
| blockReason | 阻塞原因 |
| rtcWake | RTC唤醒标志 |
| delaySeconds | 延迟秒数 |
| delayTargetSeconds | 目标延迟 |
| elapsedSeconds | 已休眠秒数 |

---

## 12. LED

```c
// LedBar.c
bool key_release_wakeup = false;  // 按键释放唤醒标志
```

LedBar 主要状态存储在静态全局 `s_ledbar`:
- 显示数字、图标掩码、扫描帧、SOC显示窗口、按键滤波状态

---

## 13. 工厂老化 (FactoryAging.c, 静态)

```c
static UINT8  s_u8FactoryAgingState;           // 状态 (UNINIT/RUNNING/DONE/STOPPED)
static UINT32 s_u32FactoryAgingElapsed10ms;    // 已用时间
static UINT16 s_u16FactoryAgingDurationHours;  // 时长(小时)
static UINT8  s_u8FactoryAgingMosMode;         // MOS模式
```

---

## 14. 日志

```c
UINT32 su32_Interval_S_Tcnt;  // 时间秒计数
```

---

## 15. 产品信息

```c
// AppInit.c
UINT8 SeriesNum = 10;  // 电池串数
```

---

## 16. 备份域寄存器使用

| 寄存器 | 用途 |
|--------|------|
| BKP_DR4 | LED 休眠 SOC (含 MAGIC) |
| BKP_DR5 | LED 休眠 SOC 取反校验 |
| BKP_DR6 | 工厂老化 MAGIC |
| BKP_DR7 | 工厂老化 MAGIC 取反 |
| BKP_DR8 | 工厂老化 已用时间 LO |
| BKP_DR9 | 工厂老化 已用时间 HI |
| BKP_DR10 | 工厂老化 CRC |

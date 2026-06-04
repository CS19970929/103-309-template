# 103-309 BMS 当前项目模块地图

> 范围说明：本次审查以当前主工程 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` 及其列入工程的 `103 + 309/Project/Source` 源码为准。`SH3673520+STM32F072CBT6 DemoCode...`、`build/generated_templates`、`firmware/comm_tool_f103ret6` 视为参考或工具工程，不按当前 BMS App 运行路径判定需求。
> 硬性边界：本文件记录当前模块事实；2026-06-04 已同步 ADC 直接采样源码变更，未修改 Keil 工程或编译配置。

## 1. 项目目录结构

| 路径 | 作用 | 本次判断 |
|---|---|---|
| `103 + 309/Project/Source` | 当前主 App 源码目录，包含启动、采样、AFE、SOC、通信、低功耗、存储、LED、老化等模块 | 当前 review 的核心 |
| `103 + 309/Project/Source/conf` | 产品配置、GPIO 映射、编译宏映射、低功耗 IO 配置 | 高风险配置入口 |
| `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0` | STM32F1 标准外设库和启动文件 | 当前目标为 STM32F103 系列 |
| `103 + 309/Project/Users` | Keil 工程、目标配置、输出目录 | 只读检查；未修改 |
| `tools` | 安全烧录脚本、SOC 测试 UI、检查脚本等 | 后续验证和量产规则应继续固化在这里 |
| `docs` | 现有架构、设计、测试、风险文档 | 本次新增 `docs/review` |
| `firmware/comm_tool_f103ret6` | 独立通信工具固件/IAP 工程 | 不属于当前 BMS App 主循环 |
| `SH3673520+STM32F072CBT6 DemoCode...` | SH3673520 + STM32F072 参考工程 | 可用于后续 AFE/F0 适配参考，不作为当前实现 |
| `build/generated_templates*` | 生成模板和测试输出 | 不作为当前真实固件 |

## 2. 当前主工程文件清单

Keil 工程列入的业务源码主要包括：

| 模块 | 文件 |
|---|---|
| 启动与运行时 | `main.c`, `AppInit.c/.h`, `Runtime.c/.h`, `System_Init.c/.h`, `System_Monitor.c/.h`, `Fault.c/.h` |
| 配置与 IO | `conf/Project_Config.h`, `conf/Project_BuildGuard.h`, `conf/conf.h`, `conf/conf_gpio.h`, `conf/conf.c` |
| ADC / Type-C | `ADC.c/.h` |
| AFE SH367309 | `I2C_AFE1.c/.h`, `SH367309_Func.c/.h`, `SH367309_DataDeal.c/.h`, `ShortFunc.c/.h`, `rtc_sleep_afe_sh367309.c` |
| 数据聚合与保护辅助 | `DataDeal.c/.h`, `MosStartup.c/.h`, `PubFunc.c/.h` |
| SOC | `SOC.c/.h`, `SocEnhance.c/.h` |
| 参数与 Flash | `EEPROM.c/.h`, `Flash.c/.h`, `Flash64KAppTest.c/.h`, `UpgradeParamPolicy.h` |
| UART/Modbus 上位机协议 | `Sci_Upper.c/.h` |
| CAN | `Can_HDX.c/.h`, `CanFeidaoFrames.c/.h` |
| RTC/低功耗 | `RTC.c/.h`, `rtc_sleep.c/.h`, `rtc_sleep_port.c/.h`, `SleepDeal.c/.h`, `LowPowerSleep.c/.h`, `bsp_clock.c/.h`, `bsp_power.c/.h`, `bsp_rtc.c/.h` |
| LED / 按键显示 | `LedBar.c/.h` |
| 日志/事件/产品信息 | `LogRecord.c/.h`, `ProductionID.c/.h`, `easylogger/*` |
| 工厂老化 | `FactoryAging.c/.h` |

## 3. 当前配置画像

| 配置项 | 当前值 | 代码证据 | 影响 |
|---|---:|---|---|
| 构建档位 | `FD_Release` 不覆盖非 0 build profile | Keil 工程 + `tools/project_check.py` | 量产 target |
| 电池类型 | `PROJECT_CFG_BAT_TYPE 1` | `Project_Config.h:31` | 代码中对应 BAT_SLAVE |
| 电芯体系 | `PROJECT_CFG_BAT_CHEMISTRY 0` | `Project_Config.h` | 三元锂 OCV 表 |
| SOC 运行时表 | 关闭 | `Project_Config.h` | 上位机写 SOC 表不参与量产算法 |
| Host 写参数 | 开启 | `Project_Config.h:45` | 上位机可写保护/OtherElement/AFE 参数 |
| IWDG | 开启 | `Project_Config.h:82` | 低功耗 RTC 周期被限制 |
| RTC | 开启 | `Project_Config.h:86` | 支持 STOP/RTC 唤醒/SOC 休眠补偿 |
| UART1 唤醒 | 开启 | `Project_Config.h:94` | PB7 可作为唤醒源 |
| RS485 唤醒 | 开启 | `Project_Config.h:102` | PB12 作为通信唤醒源 |
| 虚拟电流 | 开启 | `Project_Config.h:110` | 调试入口存在；当前 `App_AFEGet()` 主路径调用 `DataLoad_Current()`，测试电流必须保持隔离 |
| 长按关机 | 开启 | `Project_Config.h:122` | LED/按键模块可进入 DEEP_MODE |
| IAP 跳转 | 开启 | `Project_Config.h:159` | 上位机/CAN 可请求进 IAP |
| 工厂老化 | 开启 | `Project_Config.h:163` | 默认 3 天运行时老化 |
| SOC_TEST 兼容占位 | 保留 padding | `Sci_Upper.c:828-829`, `Sci_Upper.h:138-140` | 当前无活动 SOC 注入式测试模式；16 word 0 仅保留协议长度 |
| LedBar 睡眠 | 开启 | `Project_Config.h:356` | 睡眠前需要保存/显示 SOC |
| 升级参数策略 | 开启，版本 `0x0004` | `Project_Config.h:393-433` | 当前会重置 SOC snapshot 和事件记录 |

## 4. main 主循环逻辑

真实入口很短：

```text
main()
  AppInit_Boot()
  while (1)
    Runtime_RunOnce()
```

证据：

- `main.c:7` 调用 `AppInit_Boot()`。
- `main.c:11` 在 `while(1)` 内调用 `Runtime_RunOnce()`。

## 5. 启动初始化逻辑

`AppInit_Boot()` 分成设备初始化和运行态初始化：

```text
AppInit_Boot()
  AppInit_InitDevice()
    SystemInit()
    InitDelay()
    SleepDeal_HandleBootSleepStartup()
    jtag_disableAndConfIO()
    InitNVIC()
    InitIO()
    InitUSART_CommonUpper()
    InitE2PROM()
    InitAFE1()
    InitCan()
    InitADC()
    InitData_SOC()
    InitTimer()
    __enable_irq()
  AppInit_InitRuntimeState()
    InitSystemMonitorData_EEPROM()
    g_u32CS_Res_AFE = ...
    system boot ready flag/version/product id/log
  Init_RTC()
```

证据：

- `AppInit.c:10-38` 初始化外设和任务依赖。
- `AppInit.c:34-42` 初始化系统运行状态、产品信息和启动日志请求。
- `AppInit.c:44-52` 启动 RTC；低功耗运行态入口在主循环内调用。

## 6. 任务调度逻辑

当前没有 RTOS，主循环由 `Runtime_RunOnce()` 串行调度：

| 调度段 | 调用顺序 | 主要功能 | 代码证据 |
|---|---|---|---|
| 前台快任务 | `SysTime_LatchTaskFlags()` -> `FactoryAging_Task()` -> `APP_LedBar()` -> `App_AFEGet()` | 锁存时基、老化、LED、AFE/SOC 200ms 数据链 | `Runtime.c:15-22` |
| IO 和电源任务 | `AppInit_ServiceSci()` -> `App_AnlogCal()` -> `rtc_sleep()` -> `App_Can()` | UART/Modbus、ADC、低功耗、CAN | `Runtime.c:24-31` |
| 后台任务 | `App_FlashUpdate()` -> `App_LogRecord()` -> `App_ProID_Deal()` -> `Feed_IWatchDog` | IAP 复位、日志、PROID heartbeat hook、喂狗 | `Runtime.c:47-60` |

关键事实：

- `App_SOC()` 没有在 `Runtime_RunIoAndPowerTasks()` 中直接调用，那里是注释状态；真实 SOC 更新在 `App_AFEGet()` 内部触发。
- AFE/SOC 主采样节拍由 `SysTime_Take200msTaskPeriod()` 消费，理论周期 200ms。

## 7. 中断使用情况

| 中断 | 入口 | 作用 | 风险关注 |
|---|---|---|---|
| `TIM3_IRQHandler` | `System_Init.c:293-300` | 10ms 系统节拍，置 10/50/100/200/1000ms 标志 | 所有软调度依赖它；STOP 后会重建 TIM3 |
| `TIM4_IRQHandler` | `LedBar.c:957-964` | Charlieplexing LED 扫描 | LED GPIO 与低功耗 IO 强耦合 |
| `USB_LP_CAN1_RX0_IRQHandler` | `Can_HDX.c:995-1005` | CAN FIFO0 收包并入应用命令队列 | 队列只有 4 帧，命令需确认丢帧策略 |
| `USART1_IRQHandler` 间接 | 工程启动文件 -> `Sci1_CommonUpper_IRQHandler()` | Modbus RTU 收发、IDLE 帧结束 | 当前主要上位机入口 |
| `USART2/USART3` | 条件编译 | 当前配置关闭 | 后续模板化时需保留接口还是删除要确认 |
| `RTC_IRQHandler` | `RTC.c:536-550` | RTC 秒中断和 Alarm 唤醒 | 低功耗/SOC 补偿依赖 |
| `RTCAlarm_IRQHandler` | `RTC.c:531-534` | STOP 中 RTC Alarm 唤醒 | 与 IWDG 周期限制相关 |
| EXTI 线 | `conf.c` 配置 | CHG_IN、SW、UART1 RX、RS485/AFE 通信、MCU_WK 等唤醒源 | 唤醒源定义直接影响功耗和用户体验 |
| fault handlers | `Fault.c/.h`, `FaultSnapshot.h` | Cortex fault 记录 | 需要硬件/异常注入验证 |

## 8. 定时器使用情况

| 定时器 | 配置/入口 | 当前作用 |
|---|---|---|
| `TIM3` | `InitTimer()`，100kHz 基准、Period 999 | 10ms 系统节拍；产生 50/100/200/1000ms 软件标志 |
| `TIM2` | `InitADC_TIMER()`，100kHz 基准、Period 999、CC2 | ADC1 外部触发源，约 10ms 一轮 DMA 采样 |
| `TIM4` | `LedBar_ScanTimerInit()` | LED 扫描定时器，当前 `LedBar.c` 内部固定 50 个 100kHz tick |
| RTC | `Init_RTC()`, `RTC_WKTimeConfig()` | 1s RTC、STOP 唤醒、低功耗休眠累计 |
| IWDG | `Init_IWDG()` | 独立看门狗；运行态和部分阻塞延时中喂狗 |

## 9. 通信协议入口

### 9.1 UART / Modbus

入口链：

```text
USARTx IRQ
  Sci_PortIRQHandler()
    Sci_ModbusProtocolFeed()
main loop
  App_CommonUpper()
    Sci_PortService()
      Sci_ModbusProcessFrame()
        CRC_verify()
        Sci_Deal_ReadRegs_0x03() / Sci_Deal_WrReg_0x06() / Sci_Deal_WrRegs_0x10()
        Sci_ACK_0x03() / Sci_ACK_0x06_0x10()
```

关键地址：

- 只读实时窗口：`0xD000`, `0xD100`, `0xD200`, `0xD300`，见 `Sci_Upper.h:139-151`。
- 产品信息读取：`0xC002`，48 个寄存器，见 `Sci_Upper.c:769-773`。
- 保护参数：`0x2100` 起。
- SOC 表/铜损/RTC：`0x2200` 起；SOC runtime table 已删除，写表固定拒绝，读表返回编译期 OCV 表。
- 均衡/系统/睡眠/SOC/串数/采样电阻：`0x2300` 起。
- IAP/Flash connect：`0xFFFD`。

### 9.2 CAN

当前 CAN 包含两层：

| 层 | 文件 | 行为 |
|---|---|---|
| 周期广播 | `CanFeidaoFrames.c` | 发送 `0x14F80200 | chd_index` 扩展帧：电压电流、容量、SOC、SOH、版本、状态、老化剩余时间 |
| 应用命令 | `Can_HDX.c` | 标准帧命令 `0x60` / ack `0x61`，头 `A5 5A`，CRC16，支持读写 Modbus 寄存器、进入 IAP、老化控制 |

关键证据：

- CAN 周期 1000ms / 5000ms：`Can_HDX.c:20-24`, `Can_HDX.c:368-388`。
- CAN 应用命令：`Can_HDX.c:29-54`, `Can_HDX.c:609-775`。
- 老化剩余时间广播：`CanFeidaoFrames.c:244-260`，对应 `0x14F80208`。
- 进入 IAP 返回 `0x08 0x48` 并延迟复位：`Can_HDX.c:640-656`, `Can_HDX.c:788-799`。

## 10. 参数存储入口

当前对外仍保留 `EEPROM` 命名，但实现已经迁移到内部 Flash：

```text
InitE2PROM()
  EEPROM_LoadDefaultRuntimeData()
  EEPROM_LoadRWParametersFromFlash()
  ReadEEPROM_AFE_Parameters()
  ReadEEPROM_EventRecord_Parameters()
  UpgradeParamPolicy_ApplyOnce()
```

| 数据 | 存储 API | 地址/结构 |
|---|---|---|
| SOC snapshot | `StorageFlash_LoadSocData()`, `StorageFlash_SaveSocData()` | `0x0801E000/0x0801E800`，带 V2 兼容 V1 |
| AFE 参数 | `StorageFlash_LoadAfeData()`, `StorageFlash_SaveAfeData()` | `0x0801C000/0x0801C800` |
| RW 参数 | `StorageFlash_LoadRwParamData()`, `StorageFlash_SaveRwParamData()` | `0x0801C400/0x0801CC00` |
| 事件记录 | `StorageFlash_LoadLogData()`, `StorageFlash_SaveLogData()` | `0x0801D000/0x0801D800` |
| 工厂老化 | `StorageFlash_LoadFactoryAgingData()`, `StorageFlash_SaveFactoryAgingData()` | `0x0801F400` journal page |
| IAP mailbox | `AppUpgrade_RequestIap()` | SRAM `0x20004FE0` |

风险点：

- `Flash.h` 把持久化区域固定在 `0x0801C000` 以后；运行时 `StorageFlash_PrintBootCheck()` 发现 Flash 小于 128KB 会跳过后 64K 检查，但其他保存路径仍需要确认实际硬件 Flash 容量。
- 旧 EEPROM 读写函数现在返回固定值或失败，说明外部 EEPROM 已经被抽空，但命名仍可能误导。

## 11. SOC 数据流

```text
AFE/ADC/参数
  DataLoad_CellVolt()
  DataLoad_Temperature()
  DataLoad_Current()
  s_data.afeSeq++
App_SOC()
  AfeCurrent_GetSeq()
  SOC_UpdateSampleData()
  SOC_IntEnhance_Ctrl()
    coulomb integration
    full anchor
    empty tail
    rest OCV
    sag holdoff
    display smoothing
    persistence
  SOC_PublishReportData()
    g_stCellInfoReport.SocElement
```

关键证据：

- SOC 初始化：`SOC.c:109-114`。
- SOC 更新只在 AFE sample sequence 变化时执行：`SOC.c:116-142`。
- 休眠前保存 snapshot：`SocEnhance.c:1678-1685`。
- RTC 休眠补偿：`SOC_ApplyRtcRelaxationCompensation()`；当前只推进静置 OCV，不额外扣 RTC 自耗。
- Type-C 等效电流并入 SOC：`SOC_GetNetCurrentForCalc()`。

重大确认点：

- 当前 `App_AFEGet()` 中已调用 `DataLoad_Current()`，见 `DataDeal.c:1063-1085`。旧文档中“实际调用 `test_Autocurrent_cycle()`”的结论已过期；后续必须保持测试电流入口和量产主路径隔离。

## 12. ADC / AFE 数据流

### 12.1 ADC / Type-C

```text
TIM2_CC2 trigger
  ADC1 scan conversion
  DMA1_Channel1 -> g_u16ADCVal[]
App_AnlogCal()
  discard first 10ms tick after InitADC()
  ADC_UpdateMosTemp()
  ADC_UpdateVbc()
  ADC_UpdateTypeCCurrent()
```

当前 DMA 实际采 3 路：

| ADC 通道 | GPIO | 用途 |
|---|---|---|
| ADC_Channel_9 | PB1 | `ADC_TEMP_MOS1` / NMOS 温度 |
| ADC_Channel_2 | PA2 | Type-C 输出电流 |
| ADC_Channel_1 | PA1 | VBC / 总压分压 |

当前事实：

- ADC raw 仍由 TIM2_CC2 触发 ADC1 scan，并由 DMA1_Channel1 写入 `s_adc.raw[]`。
- `ADC_ResetAnlogCalSchedule()` 在冷启动或 RTC STOP 唤醒重新 `InitADC()` 后丢弃 1 个 10ms tick。
- VBC、MOS 温度、Type-C 电流已去掉软件平均/IIR 滤波，均由最新 raw 直接换算。
- Type-C 电流保留 `AD_CurZeroDeadband` 和最大值限幅；AFE/SOC 主链路不变。

证据：`ADC.c:90-123`, `ADC.c:215-261`, `ADC.c:247-250`, `ADC.c:375-452`。

### 12.2 AFE SH367309

```text
InitAFE1()
  InitIIC_AFE()
  close_ctlc()
  InitAFE1_Sleep(0)
  SH367309_UpdataAfeConfig()
  MosStartup_ApplyInitialState()

App_AFEGet() every 200ms
  UpdateVoltageFromBqMaximo()
    MTPRead(MTP_TEMP1, 46, ...)
    Registers_AFE1 -> SH367309_Read_AFE1
  DataLoad_CellVolt()
  DataLoad_Temperature()
  DataLoad_Current()
  App_SH367309()
  new_todo_logi()
  App_SOC()
```

关键文件：

- `I2C_AFE1.c`：bit-bang I2C、寄存器读写、AFE 初始化。
- `SH367309_DataDeal.c`：保护参数/AFE ROM 参数生成和写入。
- `SH367309_Func.c`：AFE 状态读、MOS control、fault mapping、AFE sleep。
- `DataDeal.c`：把 AFE 原始数据转为 `g_stCellInfoReport`。

## 13. 低功耗入口

运行态低功耗入口：

```text
Runtime_RunIoAndPowerTasks()
  rtc_sleep()
    LP_GetBlockReason()
```

睡眠路径：

```text
rtc_sleep()
  lp_update_sleep_request()
  HICCUP_MODE:
    LowPowerSleep_SaveCoreState()
    Init_RTC()
    IOstatus_RTCMode()
    InitWakeUp_RTCMode()
    Sys_StopMode()
    InitRunAfterStopWakeup()
    SOC_ApplyRtcRelaxationCompensation()
    low_power_refresh_rtc_status()
  NORMAL/DEEP:
    RtcSleep_PortCommitResetSleep()
    SleepDeal_Continue()
    Save state, AFE sleep, MCU reset
```

阻塞睡眠条件包括：

- 充/放电电流大于 10mA。
- 任意 SCI/CAN 忙。
- MCU_WK/key active。
- AFE 不允许 sleep。
- Flash busy 或待写参数。
- IAP pending。
- fault active。
- LedBar active。

当前 CAN 策略：`Can_PrepareSleep()` 在进入睡眠前关闭 CMNT；RTC 周期唤醒后不主动广播 CAN；真正唤醒恢复后 `InitCan()` 重新打开 CMNT，再由主循环通信。

证据：`rtc_sleep.c`, `rtc_sleep_port.c`, `LowPowerSleep.c`, `SleepDeal.c`。

## 14. IWDG 喂狗路径

| 路径 | 证据 | 说明 |
|---|---|---|
| 初始化 | `System_Init.c:33-48` | LSI + prescaler/reload；RTC 开启时 reload 更大 |
| 主循环末尾 | `Runtime.c:40-42` | 每轮后台任务后喂狗 |
| 阻塞延时 | `System_Init.c:160-173` | `__delay_ms()` 内喂狗 |
| STOP 前后 | `rtc_sleep_port.c:118-123` | 进入/退出 STOP 前后喂狗 |
| RTC 周期保护 | `RTC.c:386-390`, `RTC.c:406-417` | IWDG 开启时 wakeup period 被限制到 10s |

## 15. LED 显示路径

当前 LedBar 是 Charlieplexing 数码/图标显示：

```text
Runtime_RunFrontTasks()
  APP_LedBar()
    LedBar_ServiceMcuWake()
    LedBar_ServiceSwitch()
    sleep pending -> save sleep SOC / sleep pins
    startup display window
    display SOC + percent icon + charge/discharge icon
TIM4_IRQHandler()
  LedBar_Scan1ms()
```

关键行为：

- 低功耗前保存 SOC 到 BKP_DR4/DR5。
- 睡眠唤醒过程中可显示保存的 SOC preview。
- 长按按键可触发 `DEEP_MODE` 并进入 `SleepDeal_Continue()`。
- 当前 `LedBar_IsFaultActive()` 只返回状态，`APP_LedBar()` 内 fault 分支为空，故当前未看到故障显示模式落地。

## 16. Bootloader / IAP 路径

当前源码常量：

- IAP 起始地址：`FLASH_ADDR_IAP_START 0x08000000`，见 `Flash.h:4`。
- App 起始地址：`FLASH_ADDR_APP_START 0x08004800`，见 `Flash.h:5`。
- App -> IAP 请求使用 SRAM mailbox `0x20004FE0`，见 `Flash.c:12-14`, `Flash.c:699-738`。
- IAP 请求后通过 MCU reset 进入 bootloader/IAP，见 `Flash.c:1115-1137`。

安全脚本：

- `tools/soc_flash_app_safe.ps1` 默认烧录 `0x08004800`。
- 如果地址不是 `0x08004800` 会拒绝执行。
- 默认 dry-run，必须加 `-Flash` 才真正烧录。

需要确认的风险：

- 当前仓库未找到 `103 + 309/Project/Users/Objects/FD_Release.sct` 文件；Keil 工程 XML 同时出现 `IROM 0x08000000` 和内存槽 `0x8004800`，而 `ScatterFile` 为空。后续不能只相信 IDE UI，必须以最终 map/bin 和安全脚本验证 App 起始地址。

## 17. 当前最关键的数据总线

| 数据 | 生产者 | 消费者 | 说明 |
|---|---|---|---|
| `g_stCellInfoReport` | `DataDeal.c`, `SOC.c`, `SH367309_Func.c` | Modbus、CAN、LED、低功耗、保护日志 | 全局核心状态，耦合最高 |
| `OtherElement` | `EEPROM.c`, `Sci_Upper.c` | SOC、低功耗、AFE 参数、系统配置 | 运行参数集合 |
| `PRT_E2ROMParas` | `EEPROM.c`, `Sci_Upper.c` | AFE 参数生成、SOC 校准 gate | 保护阈值集合 |
| `System_ErrFlag` | `System_Monitor.c`, AFE/ADC/Flash/Log | Modbus/CAN/LED/低功耗 | 系统错误和状态桥 |
| `RTC_time` / `su32_Interval_S_Tcnt` | `RTC.c`, `rtc_sleep_port.c` | Modbus、日志、老化 | 时间和累计运行时间 |
| `SOC_Enhance_Element` | `SocEnhance.c` | SOC publish、低功耗补偿、调试 | SOC 内部状态 |
| `AfeCurrent_GetSeq()` | `DataDeal.c` | `SOC.c` | SOC 新样本触发条件 |

## 18. 当前疑似未落地或历史残留入口

| 主题 | 证据 | 当前判断 |
|---|---|---|
| 主动均衡任务 | 工程内没有看到 `App_CellBalance()` 进入 `Runtime_RunOnce()`；只有 `OtherElement` 均衡参数、AFE balance 寄存器读取、错误位 | 如果产品要求主动均衡，这是缺口；如果不要求，应清理为兼容参数 |
| 标定写入 | `Sci_WrRegs_0x10_CalibCoef()` 和 `Sci_WrReg_0x06_Reset_CalibCoef()` 主体 `#if 0` | 上位机地址可能保留但写入实际关闭 |
| RTC 参数写入 | `Sci_WrRegs_0x10_RTC()` 空函数 | 地址存在但写入无效 |
| 铜损表写入 | `Sci_WrRegs_0x10_CopperLoss()` 空函数 | 地址存在但写入无效 |
| 故障 LED 显示 | `LedBar_IsFaultActive()` 存在，`APP_LedBar()` fault 分支为空 | 可能是未完成需求 |
| 虚拟电流循环 | 当前 `App_AFEGet()` 主路径已调用 `DataLoad_Current()`；旧虚拟电流主路径描述已过期 | 量产必须保持真实电流主路径，测试注入只能在测试 profile/测试固件中使用 |
| 状态变量复杂度 | `s_ledbar.initialized` 主循环懒初始化、`g_stLowPowerRtcStatus.readyToSleep`、`ProductionID.c` 的 `su8_StartUpFlag` 等已分批收口；剩余状态见专项文档 | 详见 `docs/review/state_variable_audit.md`，后续继续按确认项分批净删减 |

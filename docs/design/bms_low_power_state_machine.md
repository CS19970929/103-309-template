# BMS 低功耗业务状态机分析

本文是 BmsLogicAgent 第一阶段只读分析结果。范围只覆盖 BMS 业务层约束，不改源码；引用均来自当前项目文件、函数或宏。

## 1. 当前低功耗业务入口

当前主循环在 `103 + 309/Project/Source/main.c:5` 调用 `AppInit_Boot()`，随后循环执行 `Runtime_RunOnce()`。实际运行顺序在 `103 + 309/Project/Source/Runtime.c:12-36`：

1. 前置任务：`SysTime_LatchTaskFlags()`、`FactoryAging_Task()`、`APP_LedBar()`、`App_AFEGet()`。
2. IO 与电源任务：`AppInit_ServiceSci()`、`App_AnlogCal()`、`App_LowPowerProcess()`、`App_Can()`。
3. 后台任务：`StorageFlash_AppUseTest_Task()`、`App_FlashUpdate()`、`App_LogRecord()`、`App_ProID_Deal()`、`Feed_IWatchDog`。

关键影响：

- `App_LowPowerProcess()` 早于 `App_Can()`、`App_FlashUpdate()`、`App_LogRecord()`。后续框架做最终入睡判断时，必须显式检查通信、升级、Flash/日志待处理状态，不能只依赖主循环顺序。
- `Runtime.c:23` 中 `App_SOC()` 当前被注释，SOC 在低功耗链路里主要依赖 `SOC_SaveSnapshotBeforeSleep()` 和 `SOC_ApplyRtcRelaxationCompensation()`，不能假设每轮主循环都会跑完整 SOC 任务。

当前低功耗模式定义在 `103 + 309/Project/Source/rtc_sleep.h:27-29`：

| 当前模式 | 含义 | 当前入口 |
| --- | --- | --- |
| `NORMAL_MODE` | 复位式普通睡眠 | `SleepDeal_Continue()` 写 BKP 标志、AFE sleep、`MCU_RESET()` |
| `HICCUP_MODE` | RTC 周期 Stop 打嗝睡眠 | `rtc_sleep_run_hiccup_cycle()` 直接 Stop/唤醒/服务窗口循环 |
| `DEEP_MODE` | 复位式深度睡眠 | 与 `NORMAL_MODE` 同类，但使用 deep IO 配置 |
| `NO_SLEEP` | 不睡眠 | `LowPower_Request(NO_SLEEP)` |

`103 + 309/Project/Source/SleepDeal.c:83-113` 的 `SleepDeal_Continue()` 会调用 `LowPowerSleep_SaveResetState()`，写 `FLASH_NORMAL_SLEEP_VALUE / FLASH_HICCUP_SLEEP_VALUE / FLASH_DEEP_SLEEP_VALUE`，然后 `InitAFE1_Sleep(0)`、`AFE_Sleep()`、`MCU_RESET()`。复位后 `IsSleepStartUp()` 根据 BKP 标志进入对应 Stop 循环，见 `SleepDeal.c:186-239`。

`HICCUP_MODE` 的 RTC 周期 Stop 不走复位路径。`103 + 309/Project/Source/rtc_sleep.c:303-345` 的 `rtc_sleep_run_hiccup_cycle()` 在入睡前调用 `RtcSleep_PortPrepareRtcStop()`，Stop 返回后恢复外设，再判断是否继续下一轮。

## 2. 当前业务约束

### 2.1 保护状态

保护状态的主要来源是 SH367309 的 `BSTATUS1/2/3`：

- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:11-27` 的 `RtcSleep_AfePortIsSleepBlocked()` 读取 `MTP_BSTATUS1` 连续 3 字节；只要 `REG_BSTATUS1.all`、`REG_BSTATUS2.all`、`REG_BSTATUS3.bits.L0V` 或 `REG_BSTATUS3.bits.PCHG_FET` 非零，就返回 AFE 不空闲，阻止 RTC idle sleep。
- `103 + 309/Project/Source/SH367309_Func.c:228-305` 的 `Fault_ChangeToMCU()` 将 AFE 的 `OV/UV/OCD/OCC/UTC/OTC/UTD/OTD/SC` 映射到 `g_stCellInfoReport.unMdlFault_Third` 和 `System_ErrFlag`，其中 UV 触发时还会关闭 `GPIO_DC_EN`、`GPIO_2727_EN`，见 `SH367309_Func.c:256-263`。
- `103 + 309/Project/Source/LogRecord.c:192-209` 的 `App_LogRecord()` 会记录 OVP、UVP、OCP、温度、压差、AFE/EEPROM/CBC 等事件。

结论：

- 活跃保护故障不应进入普通 RTC Stop 连睡。当前代码已通过 `RtcSleep_AfePortIsSleepBlocked()` 阻塞 HICCUP，但阻塞原因粒度仍是 `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE`。
- 过放/低压是深度低功耗候选，不应被工厂老化、显示、普通通信阻塞。当前 `rtc_sleep.c:154-178` 已先判断低压深睡，再判断 MCU_WAKE 和工厂老化，优先级方向正确。

### 2.2 MOS 状态

MOS 状态既有启动策略，也有运行态同步：

- `103 + 309/Project/Source/MosStartup.c:47-60` 的 `MosStartup_ApplyInitialState()`：有 `GPIO_CHG_IN` 充电输入时开充电关放电；工厂老化可进入双 MOS；否则开放电关充电。
- `103 + 309/Project/Source/System_Monitor.c:72-86` 的 `SystemRuntime_SetMosStatus()`、`SystemRuntime_IsChargeMosOpen()`、`SystemRuntime_IsDischargeMosOpen()` 保存运行态 MOS 状态。
- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:74-106` 的 `RtcSleep_AfePortHasAfeWake()` 在 RTC 唤醒后读取 AFE 状态，调用 `SystemRuntime_SetMosStatus()` 和 `Fault_ChangeToMCU()`；如果放电 MOS 关闭或 `unMdlFault_Third` 非零，就退出连睡。
- `103 + 309/Project/Source/Flash.c:1078-1089` 的 `App_FlashUpdate()` 在进入 IAP 前关闭 CHG/DSG MOS 并复位。

结论：

- MOS 状态异常或被保护关闭时，应退出 RTC 周期睡眠，回到运行态重新同步保护、日志和通信。
- 进入升级/IAP 前的 MOS 关闭是业务动作，低功耗框架必须把升级视为禁止休眠原因，不能在 `u8FlashUpdateFlag` 待处理时先睡。

### 2.3 AFE 状态

AFE 相关约束贯穿睡前和醒后：

- `103 + 309/Project/Source/SH367309_Func.c:65-70` 的 `AFE_Sleep()` 设置 `REG_MTP_CONF.bits.SLEEP = 1` 并写 `MTP_CONF`。
- `103 + 309/Project/Source/SleepDeal.c:109-113` 的复位式睡眠会先 `InitAFE1_Sleep(0)`，再 `AFE_Sleep()`。
- `103 + 309/Project/Source/rtc_sleep_port.c:108-116` 的 `RtcSleep_PortPrepareRtcStop()` 在 RTC Stop 前调用 `LowPowerSleep_SaveCoreState()`，再初始化 RTC 和唤醒 IO。
- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:30-42` 的 `RtcSleep_AfePortUpdateRtcData()` 在 RTC 唤醒后更新电压、温度和最大最小值；失败则 `isException()` 返回异常。
- `103 + 309/Project/Source/DataDeal.c:807-850` 的 `DataLoad_Current()` 重新计算 `u16Ichg/u16IDischg`。

结论：

- AFE 通信失败、AFE 正在写 EEPROM、AFE BSTATUS 非空、PCHG/L0V 等状态应阻塞普通 RTC Stop。
- RTC 唤醒后必须先更新 AFE 数据，再决定继续连睡或退出运行态。当前 `rtc_sleep.c:245-262` 的 `isException()` 顺序符合这个方向。

### 2.4 SOC 静置校准

SOC 与低功耗的关键关系：

- `103 + 309/Project/Source/LowPowerSleep.c:5-9` 的 `LowPowerSleep_SaveCoreState()` 会在睡前执行 `SOC_SaveSnapshotBeforeSleep()`。
- `103 + 309/Project/Source/SocEnhance.c:1678-1685` 的 `SOC_SaveSnapshotBeforeSleep()` 只有在 `SOC_Enhance_Element.u16_SOC_InitOver` 后才保存。
- `103 + 309/Project/Source/rtc_sleep_port.c:166-178` 的 `RtcSleep_PortApplySocRtcRest()` 使用 RTC 休眠秒数、最低/最高单体电压调用 `SOC_ApplyRtcRelaxationCompensation()`。
- `103 + 309/Project/Source/SocEnhance.c:1464-1516` 的 `soc_apply_rtc_rest_ocv()` 会累加 RTC 静置秒数、自耗补偿、静置稳定计时和 OCV 目标锁存。
- `103 + 309/Project/Source/SocEnhance.c:1739-1764` 的 `SOC_ApplyRtcRelaxationCompensation()` 若 SOC 变化会 `soc_save_current_snapshot()`。
- `103 + 309/Project/Source/conf/Project_Config.h:270-274` 当前 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT=0`、`PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT=0`，即 SOC 校准默认不因保护/系统故障阻塞。

结论：

- RTC 休眠时间必须作为 SOC 静置输入保留。后续 `LP_GetLastSleepSeconds()` 应直接服务 SOC 休眠补偿。
- 如果后续启用 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT` 或 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT`，低功耗退出条件和 SOC 校准阻塞条件要同步评估，避免故障状态下继续做 OCV 下修。

### 2.5 Flash 参数保存和日志保存

Flash/日志当前是同步写，但缺少统一 busy/pending 查询：

- `103 + 309/Project/Source/EEPROM.c:161-173` 的 `EEPROM_SaveRWParametersToFlash()` 构建 RW 参数并调用 `StorageFlash_SaveRwParamData()`。
- `103 + 309/Project/Source/Flash.c:259-342` 的 `StorageFlash_SavePair()` 通过擦页、写记录、读回校验保存成对参数区。
- `103 + 309/Project/Source/Flash.c:456-565` 的 `StorageFlash_SaveJournalPair()` 通过 journal 页写入 SOC/日志等记录。
- `103 + 309/Project/Source/Flash.c:760-897` 分别保存 SOC、AFE、RW 参数、日志。
- `103 + 309/Project/Source/Flash.c:926-938` 的 `StorageFlash_SaveFactoryAgingData()` 保存老化状态。
- `103 + 309/Project/Source/LogRecord.c:97-120` 的 `LogEvent_EEPROM()` 将事件写入 `StorageFlash_SaveLogData()`。
- `103 + 309/Project/Source/rtc_sleep_port.c:91-99` 的 `RtcSleep_PortCommitResetSleep()` 在复位式睡眠前直接记录 `BMS_SLEEP`。

结论：

- 当前同步写函数返回前理论上 Flash 已完成，但没有 `Flash_IsBusy()` 或 `StorageFlash_IsBusy()` 可供低功耗统一查询。
- 后续框架应增加 Flash busy/pending 适配层。至少要在 `u8FlashUpdateFlag/u8FlashUpdateE2PROM`、参数保存、日志保存、SOC 快照、工厂老化保存未完成时阻塞休眠。

### 2.6 工厂老化

工厂老化与低功耗的约束：

- `103 + 309/Project/Source/FactoryAging.c:378-385` 的 `FactoryAging_IsActive()` 运行态返回 active。
- `103 + 309/Project/Source/rtc_sleep_port.c:51-54` 将 `FactoryAging_IsActive()` 暴露给低功耗策略。
- `103 + 309/Project/Source/rtc_sleep.c:190-195` 在非低压路径中，工厂老化 active 会取消 RTC idle sleep。
- `103 + 309/Project/Source/FactoryAging.c:358-372` 的 `FactoryAging_SaveProgressBeforeSleep()` 在睡前保存进度。
- `103 + 309/Project/Source/FactoryAging.c:531-562` 的 `FactoryAging_Task()` 正常推进老化时间并周期保存。

结论：

- 工厂老化运行中应阻塞普通 RTC idle sleep。
- 低压/过放深睡优先级应高于工厂老化。当前 `rtc_sleep.c:154-178` 的低压判断在 `FactoryAging_IsActive()` 前，符合“过放休眠第一优先级”。

### 2.7 过放深度休眠

当前深睡触发条件：

- `103 + 309/Project/Source/rtc_sleep.c:7-9` 定义 `LOW_POWER_FORCE_DEEP_SLEEP_MV=2800`、`LOW_POWER_FORCE_DEEP_SLEEP_SECONDS=60`、`LOW_POWER_DEEP_SLEEP_ICHG_LIMIT=5`。
- `rtc_sleep.c:154-163`：最低单体小于等于 2800mV 且充电电流小于等于 5，持续 60 秒后 `entersleep(DEEP_MODE)`。
- `rtc_sleep.c:167-178`：最低单体小于等于 `OtherElement.u16Sleep_Vlow` 且充电电流小于等于 5，持续 `OtherElement.u16Sleep_TimeVlow` 分钟后 `entersleep(DEEP_MODE)`。
- `103 + 309/Project/Source/DataDeal.c:63-107` 的 `charger_detect_and_keyLogi_200ms()` 在充电状态解除后调用 `entersleep(DEEP_MODE)`。
- `103 + 309/Project/Source/Sci_Upper.c:2160-2176` 的 `Sci_WrReg_0x06_BMS_FunctionON()` 对功能号 `0x0A` 调用 `entersleep(DEEP_MODE)`，即上位机可命令立即进入休眠。

结论：

- 过放深睡应保持最高业务优先级，但应增加硬件充电输入 `GPIO_CHG_IN` 的显式判断，避免插充电器时仍按低电压进入 deep。
- `DEEP_MODE` 当前仍是复位式 Stop 流程，不是 STM32 Standby。文档和后续接口应避免把当前 deep 误称为硬件 Standby。

## 3. 推荐最小状态机

第一版不追求最低电流，只追求稳定睡眠、稳定唤醒、通信不乱、保护不丢、IWDG 不误复位。建议将现有逻辑收敛为以下状态：

| 状态 | 进入条件 | 主要动作 | 退出条件 |
| --- | --- | --- | --- |
| `LP_STATE_RUN` | 上电、唤醒恢复、阻塞存在 | 正常跑保护、AFE、SOC、通信、Flash、LED、IWDG | 1 秒节拍到达且可评估低功耗 |
| `LP_STATE_IDLE_CHECK` | `SysTime` 1 秒节拍 | 汇总阻塞位图；低压深睡优先判断；累计 idle 秒数 | 有阻塞回 RUN；idle 达标进 PREPARE；深睡达标进 DEEP |
| `LP_STATE_PREPARE_SLEEP` | 无阻塞且准备进入 Stop/复位式 sleep | 二次检查阻塞；保存 SOC、老化、CAN；必要时写睡眠日志 | 保存成功进 STOP 或 DEEP；失败回 RUN |
| `LP_STATE_STOP_SLEEP` | RTC 周期 Stop | 配置 RTC、唤醒 EXTI、关闭外设、进入 Stop | RTC/外部/AFE/电流唤醒 |
| `LP_STATE_WAKEUP_RESTORE` | Stop 返回 | 恢复时钟和外设；同步 AFE/MOS/保护；更新 SOC RTC rest | 无异常继续 STOP；异常回 RUN |
| `LP_STATE_DEEP_STANDBY` | 低压深睡、上位机强制睡眠、充电移除后深睡 | 当前阶段映射到 `DEEP_MODE` 复位式 Stop；未来可扩展 Standby | 充电器或按键唤醒 |
| `LP_STATE_ERROR` | RTC/AFE/Flash/恢复失败 | 退出低功耗，保留诊断，避免继续连睡 | 人工或业务恢复后回 RUN |

## 4. 哪些状态禁止休眠

这里的“禁止休眠”主要指禁止普通 RTC idle Stop 和复位式普通睡眠；过放深睡另按优先级处理。

| 状态 | 当前依据 | 建议策略 |
| --- | --- | --- |
| 充电或放电电流存在 | `rtc_sleep.c:132-136` 使用 `u16Ichg/u16IDischg > 10` 阻塞 | 禁止普通 Stop；充电输入存在时也禁止 deep |
| MCU_WAKE/按键/显示交互 | `rtc_sleep.c:139-141`、`SleepDeal.c:22-79`、`LedBar.c:1040-1043` | 禁止 idle Stop，低压 deep 可覆盖但要保存显示 SOC |
| 工厂老化运行 | `rtc_sleep.c:190-195`、`FactoryAging_IsActive()` | 禁止普通 Stop；低压 deep 优先 |
| 串口/Modbus/CAN 外部通信活跃 | `Sci_IsAnyPortBusy()` 在 `Sci_Upper.c:1678-1690`，`RTC_ExtComCnt` 在 `Sci_Upper.c:1479` 递增，CAN 状态在 `Can_HDX.c:896-929` | 第一版应显式禁止 Stop，避免通信中途掉电 |
| AFE BSTATUS 非空或 AFE 通信失败 | `RtcSleep_AfePortIsSleepBlocked()`、`RtcSleep_AfePortUpdateRtcData()` | 禁止普通 Stop；唤醒后异常则回 RUN |
| Flash/日志/参数保存中或待保存 | `EEPROM_SaveRWParametersToFlash()`、`StorageFlash_Save*()`、`LogEvent_EEPROM()` | 禁止所有可恢复 sleep，直到保存完成或失败处理完成 |
| IAP/升级待处理 | `Sci_WrRegs_0x10_FlashConnect()` 置 `u8FlashUpdateE2PROM`，`App_FlashUpdate()` 处理 `u8FlashUpdateFlag` | 禁止低功耗，优先走升级复位 |
| 保护故障未同步 | `Fault_ChangeToMCU()`、`App_SH367309_Monitor()` | 禁止普通 Stop；低压/过放转 deep |
| IWDG 周期不安全 | 当前文档由 IwdgAgent 给出，源码尚无统一 bit | 禁止非复位 Stop，除非 RTC 周期小于 IWDG 安全窗口 |

## 5. 哪些状态应该进入深度低功耗

| 深度低功耗原因 | 当前依据 | 建议策略 |
| --- | --- | --- |
| 强制低压，最低单体 <= 2800mV 持续 60s 且充电电流 <= 5 | `rtc_sleep.c:7-9`、`rtc_sleep.c:154-163` | 进入 `DEEP_MODE`，优先级高于工厂老化/显示/通信 |
| 参数低压，最低单体 <= `OtherElement.u16Sleep_Vlow` 持续 `u16Sleep_TimeVlow` 分钟且充电电流 <= 5 | `rtc_sleep_port.c:36-44`、`rtc_sleep.c:167-178` | 进入 `DEEP_MODE`，参数来自上位机可写 SleepElement |
| 充电唤醒后拔除充电 | `DataDeal.c:63-107` | 当前会 `entersleep(DEEP_MODE)`；后续需结合硬件充电输入去抖 |
| 上位机命令立即休眠 | `Sci_Upper.c:2160-2176` 功能号 `0x0A` | 允许进入 deep，但要先完成通信应答和保存 |
| AFE/EEPROM 持续异常 | `DataDeal.c:979-1025` 5 分钟后 `entersleep(NORMAL_MODE)` | 当前是 NORMAL；是否升级为 deep 需结合故障类型，第一版不扩大 |

## 6. 第一阶段结论

1. 当前项目已经有低功耗业务雏形：`rtc_sleep.c` 负责策略，`rtc_sleep_port.c` 负责业务适配，`SleepDeal.c` 负责复位式睡眠，`LowPowerSleep.c` 负责睡前公共保存。
2. 过放深睡优先级当前基本正确：`rtc_sleep.c` 先判断低压，再判断 MCU_WAKE、工厂老化和普通阻塞。
3. 最大缺口不是“没有低功耗”，而是阻塞原因不完整：Flash/日志 busy、升级 pending、串口 busy、CAN active、LED active、IWDG unsafe 没有统一进入低功耗判断。
4. 当前 `DEEP_MODE` 不是硬件 Standby，而是写 BKP 标志、AFE sleep、MCU reset 后进入 Stop 等待唤醒。后续文档和接口必须准确命名。
5. 下一阶段最小实现应先保留现有业务语义，把分散判断整理成 `LP_CanSleep()` 和 `LP_GetBlockReason()`，不要大规模重构保护、SOC、AFE、Flash、CAN、Modbus。

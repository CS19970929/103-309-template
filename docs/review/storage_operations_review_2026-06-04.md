# 存储相关操作、逻辑与风险梳理

状态：已按当前源码静态验证  
日期：2026-06-04  
范围：BMS App 主工程的内部 Flash、BKP 备份寄存器、RTC 备份域、AFE MTP、伪 EEPROM 兼容层、参数/日志/SOC/老化/升级相关存储逻辑。  
未验证：未连接硬件，未构建最新 map，未实测 Flash 擦写寿命和掉电恢复。

## 参考源码

- `103 + 309/Project/Source/Flash.h`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/EEPROM.c`
- `103 + 309/Project/Source/EEPROM.h`
- `103 + 309/Project/Source/SH367309_DataDeal.c`
- `103 + 309/Project/Source/SH367309_DataDeal.h`
- `103 + 309/Project/Source/I2C_AFE1.c`
- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/LogRecord.c`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/LowPowerSleep.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/Sci_Upper.h`
- `103 + 309/Project/Source/System_Monitor.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c`
- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- `tools/soc_flash_app_safe.ps1`

## 结论摘要

当前源码已经没有真正的外部 EEPROM 读写主路径。`EEPROM.c` 是兼容层：启动时先加载运行默认值，再从内部 Flash 加载保护参数、OtherElement、AFE 参数和事件记录；旧的 `ReadEEPROM_*` / `WriteEEPROM_*` 函数是空实现或失败返回。

内部 Flash 持久化区固定从 `0x0801C000` 开始，按当前 `STM32F10X_MD` 配置使用 1KB page。参数类使用 A/B 双槽，SOC 和日志使用双页 journal，工厂老化使用单页 journal，升级参数策略使用单独半字标志页。

BKP 备份寄存器当前承担短状态保存：RTC 初始化标记、睡眠启动标记、LED 睡前 SOC、工厂老化秒级进度、Cortex fault reason。睡眠标记虽然仍使用 `FLASH_*_SLEEP_VALUE` 命名，但当前介质是 `BKP_DR2/DR3`，不是 Flash。

当前最大风险不是单个 Flash 写函数，而是边界一致性和错误可见性：源码存储区在 `0x0801C000+`，安全脚本要求 App 从 `0x08004800` 烧录，但 Keil XML 仍存在 `IROM 0x08000000/0x10000` 和空 `ScatterFile` 口径；另外 `ERROR_EEPROM_STORE` 当前不会被 `System_ERROR_UserCallback()` 计数，导致存储失败可能不可见。

## 存储介质总览

| 介质 | 当前用途 | 生命周期 | 关键边界 |
|---|---|---|---|
| 内部 Flash | AFE 参数、保护参数、OtherElement、事件日志、SOC 快照、工厂老化状态、升级参数策略版本 | 断电保持 | `0x0801C000` 起的尾部存储区，依赖实际 128KB Flash |
| BKP 备份寄存器 | RTC init marker、睡眠标志、LED 睡前 SOC、老化秒级进度、fault reason | 备份域保持，掉 VBAT 或 `BKP_DeInit()` 会丢失 | `BKP_DR1..DR12` 已占用 |
| RTC counter/alarm | 时间、STOP alarm 唤醒、RTC 休眠补偿 | 备份域保持 | RTC 初始化可能重置备份域 |
| AFE MTP/EEPROM | SH367309 内部配置寄存器 `0x00..0x19` | AFE 内部保持 | 通过 `MTPWriteROM()` 写，写后回读 25 字节校验 |
| RAM 运行态 | Product ID、系统功能开关、一次设 SOC 命令等 | 复位丢失 | 当前没有进入内部 Flash |

## 内部 Flash 地址图

当前 `Flash.h` 的持久化区如下。因 `uvprojx` 定义了 `STM32F10X_MD`，源码 page size 走 1KB 分支。

| 地址 | 大小 | 当前用途 | API | 格式 |
|---|---:|---|---|---|
| `0x08000000` | IAP 起始 | IAP/Bootloader | `FLASH_ADDR_IAP_START` | 固定安全边界 |
| `0x08004800` | App 起始 | 正常 App | `FLASH_ADDR_APP_START`、安全脚本 | 固定安全边界 |
| `0x0801C000` | 1KB | AFE 参数 Slot A | `StorageFlash_Load/SaveAfeData()` | A/B 双槽 |
| `0x0801C400` | 1KB | RW 参数 Slot A | `StorageFlash_Load/SaveRwParamData()` | A/B 双槽 |
| `0x0801C800` | 1KB | AFE 参数 Slot B | 同上 | A/B 双槽 |
| `0x0801CC00` | 1KB | RW 参数 Slot B | 同上 | A/B 双槽 |
| `0x0801D000` | 2KB | 事件日志 Slot A | `StorageFlash_Load/SaveLogData()` | journal |
| `0x0801D800` | 2KB | 事件日志 Slot B | 同上 | journal |
| `0x0801E000` | 2KB | SOC Slot A | `StorageFlash_Load/SaveSocData()` | journal，兼容旧 V1 |
| `0x0801E800` | 2KB | SOC Slot B | 同上 | journal，兼容旧 V1 |
| `0x0801F000` | 1KB | 升级参数策略版本 | `UpgradeParamPolicy_ApplyOnce()` | 单半字写，整页擦 |
| `0x0801F400` | 1KB | 工厂老化状态 | `StorageFlash_Load/SaveFactoryAgingData()` | 单页 journal |
| `0x0801F800` | 1KB | 旧升级标志保留 | 当前 App 不写 | legacy |
| `0x0801FC00` | 1KB | 旧睡眠标志保留 | 当前 App 不写 | legacy |

风险点：`0x0801C000+` 要求实际芯片至少 128KB Flash。`StorageFlash_PrintBootCheck()` 会读取 Flash size register 并在小于 128KB 时打印跳过检查，但其它保存路径没有因为小于 128KB 而禁止写入。

## Flash 写入框架

`StorageFlash` 记录统一包含 `magic/version/length/sequence/crc/reserved` 头。读取时检查 magic、版本、长度、CRC；双槽读取选择较大 sequence；写入后立即回读校验。

`FlashErasePageVerified()` 擦除后逐半字检查全 `0xFFFF`；`FlashProgramHalfWordVerified()` 编程后读回比较。`FlashWriteOneHalfWord()` 会先擦整页再写一个 halfword，目前用于升级参数策略版本页。

`StorageFlash_IsBusy()` 只是运行态 busy 标志，用于低功耗阻塞和 debug 状态，不是互斥锁。当前 Flash 写均在主循环上下文内触发，未看到 ISR 内部直接擦写 Flash。

约束：`StorageFlash_CalcCrc()` 把长度强转为 `UINT8` 传给 `Sci_CRC16RTU()`。当前 payload 均小于 255 字节，暂时没溢出；后续如果扩大结构体，这会成为隐患。

## 启动加载链路

启动入口是 `main() -> AppInit_Boot()`。

`AppInit_InitDevice()` 中的存储相关顺序：

1. `IsSleepStartUp()`：先读取 BKP 睡眠标志，决定是否进入复位后待唤醒流程。
2. `InitE2PROM()`：加载默认运行参数，再加载内部 Flash 中的 RW 参数、AFE 参数、事件记录，最后执行升级参数策略。
3. `InitAFE1()`：使用已加载的 AFE 参数更新 SH367309。
4. `InitData_SOC()`：从 `OtherElement` 加载 SOC 配置，再从 Flash 加载 SOC 快照。
5. `Init_RTC()`：初始化 RTC 和备份域 marker。

运行主循环 `Runtime_RunOnce()` 中，和存储有关的任务顺序：

1. `FactoryAging_Task()`：老化状态更新和周期保存。
2. `App_AFEGet()`：200ms 数据采样、AFE 参数写入应用、SOC 计算与保存。
3. `rtc_sleep()`：低功耗决策，睡眠前保存 SOC/老化/CAN 状态。
4. `App_Can()` / `AppInit_ServiceSci()`：CAN/Modbus 写寄存器会触发参数保存或升级请求。
5. `App_FlashUpdate()`：进入 IAP reset。
6. `App_LogRecord()`：事件日志保存。

## 伪 EEPROM 兼容层

`EEPROM.c` 的真实职责：

- `EEPROM_LoadDefaultRuntimeData()`：加载默认保护参数、校准 K/B、OtherElement，并清除 EEPROM 通信/存储错误标志。
- `EEPROM_LoadRWParametersFromFlash()`：从内部 Flash 读取保护参数和 OtherElement；读取失败或范围校验失败时，记录 `ERROR_EEPROM_STORE` 并把当前默认值保存到 Flash。
- `ReadEEPROM_AFE_Parameters()`：从内部 Flash 读取 AFE 参数；失败或越界时恢复默认并保存。
- `ReadEEPROM_EventRecord_Parameters()`：从内部 Flash 读取事件日志；失败或非法时清空并保存。
- `UpgradeParamPolicy_ApplyOnce()`：按版本和配置决定是否重置 AFE、保护参数、均衡开启电压、SOC 配置、SOC 快照、事件记录、老化时间。

旧接口状态：

| 接口 | 当前行为 |
|---|---|
| `ReadEEPROM_Byte()` | 固定返回 `0xFF` |
| `WriteEEPROM_Byte()` | 固定返回 `0` |
| `ReadEEPROM_Word_NoZone()` | 固定返回 `0xFFFF` |
| `WriteEEPROM_Word_NoZone()` | 固定返回 `0` |

结论：外部 EEPROM 已被内部 Flash 替代，但文件名和部分注释仍保留 EEPROM 语义，后续可考虑在确认协议兼容后改名或收敛接口。

## RW 参数：保护阈值和 OtherElement

RW 参数 payload 是 `STORAGE_FLASH_RW_PARAM_DATA`：

- `protect[65]`：对应 `PRT_E2ROMParas`。
- `other[32]`：对应 `OtherElement`。
- `reserved[24]`：写入时填 `0xFFFF`，保留协议/结构位置。

保护参数 `PRT_E2ROMParas` 覆盖 13 组保护阈值，每组 5 个 word：

- 单节过压
- 单节低压
- 总压过压
- 总压低压
- 充电过流
- 放电过流
- 充电高温
- 充电低温
- 放电高温
- 放电低温
- MOS 高温
- 压差过大
- SOC 低

OtherElement 32 words 当前包含：

- 均衡开启电压、开启窗口、关闭窗口和保留位。
- 采样/短路相关参数：充放电电流上限、短路延时、短路电流。
- SOC 配置：表选择、容量、初始循环次数、满电/空电电压。
- 睡眠阈值：正常睡眠电压/时间、低压睡眠电压/时间、虚拟充放电电流、RTC 唤醒时间。
- 系统参数：串数、采样电阻、采样电阻倍率、预充时间。

写入入口：

| 来源 | 地址/命令 | 行为 |
|---|---|---|
| Modbus `0x10` | `0x2100` 起 65 words | 校验范围、快照、写入 RAM、保存 RW 参数，失败回滚 |
| Modbus `0x10` | `0x2300` 起 32 words | 校验范围、快照、写入 RAM、保存 RW 参数，失败回滚 |
| Modbus `0x06` | `0x1002` reset protect | 恢复保护默认值并保存 |
| Modbus `0x06` | `0x1003` reset OtherElement | 恢复 OtherElement 默认值并保存 |
| CAN service | 写寄存器命令 | 通过 `Sci_HostWriteWords()` 复用上述 Modbus 写逻辑 |

副作用：

- 保护参数中电压保护相关字段变更会调用 `InitData_SOC()`。
- OtherElement 的均衡/短路/系统采样字段变更会置 `AFE_PARAM_WRITE_Flag`，后续写 SH367309。
- OtherElement 的 SOC 配置字段变更会调用 `InitData_SOC()` 和 `SOC_RequestCapacityReset()`。
- 串数/采样电阻字段变更会更新 `SeriesNum` 和 `g_u32CS_Res_AFE`。

## AFE 参数与 AFE MTP

AFE 参数有两层：

1. MCU 内部 Flash 保存 24 个 `curValue`。
2. SH367309 内部 MTP/EEPROM 保存最终寄存器配置。

上位机写 `0x2400..0x2417` 时，`Sci_WrRegs_0x10_AFE_Parameters()` 先把 24 个参数写入 RAM，再调用 `StorageFlash_SaveAfeData()`。如果 MCU Flash 保存失败，会恢复 RAM 快照并返回负响应。

`AFE_PARAM_WRITE_Flag` 置位后，200ms 周期的 `App_SH367309()` 调用 `SH367309_UpdataAfeConfig()`：

- `Refresh_Parameters()` 根据 RS485 参数、OtherElement 和 MTP 温度参考值生成 `AFE_ROM_PARAMETERS_Struction`。
- 先读 AFE MTP `0x00..0x18` 比较差异。
- 有差异时打开 `MCUO_AFE_VPRO`，逐字节 `MTPWriteROM()` 写入。
- 写后再读 `0x00..0x18` 校验，不一致则置 `ERROR_AFE1`。

风险点：如果 AFE MTP 写失败，`AFE_PARAM_WRITE_Flag` 会重新置 1，后续周期会继续尝试。需要确认这是否会在 AFE 异常时造成过密重试。

## SOC 快照

SOC 使用 `0x0801E000/0x0801E800` 双页 journal，payload 是 `STORAGE_FLASH_SOC_DATA`，当前版本 `V2`，兼容旧 `V1`。

保存内容包括：

- 当前 SOC。
- 循环次数 `cycle_x100`。
- 当前容量、满充容量。
- 已放电学习量。
- rebound hold 等 snapshot flags。

加载逻辑：

- `InitData_SOC()` 从 `OtherElement` 加载 SOC 配置。
- `soc_param_lib_init()` 初始化运行态。
- `soc_load_or_default()` 读取 Flash 快照；若有效则恢复容量/SOC/cycle/flags；若无效则按 OCV 或默认 60% 初始化并保存一次。

保存触发：

- 每次 `SOC_IntEnhance_Ctrl()` 末尾调用 `soc_save_if_needed()`。
- 判重字段是 SOC、cycle、满充容量、snapshot flags。
- 主机一次设 SOC、容量重置会强制保存。
- RTC 休眠补偿改变 SOC 时保存。
- 进入低功耗前 `SOC_SaveSnapshotBeforeSleep()` 也会检查保存。

磨损判断：不是 200ms 盲写，但 SOC 每 1% 改变会保存一次。低压尾端校准、长静置校准、异常电压抖动场景需要做写频率实测。

## 事件日志

事件日志使用 `0x0801D000/0x0801D800` 双页 journal。每条记录 2 字节：事件 ID + 时间映射值；总共 100 条环形记录。

写入入口：

- `LogRecord_RequestStartup()` 置启动日志标志。
- `LogRecord_RequestSleep()` 置睡眠日志标志。
- `App_LogRecord()` 每秒检查三级保护、AFE2 错误、EEPROM 错误、CBC 错误。
- `LogEvent_EEPROM()` 写入内存环形记录后调用 `StorageFlash_SaveLogData()`。

写入节流：

- `PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` 当前为 3600 秒。
- `BMS_START_UP` 和 `BMS_SLEEP` 不受重复间隔限制。
- 同一事件重复记录依赖 latch，故障解除后再次触发才会记录。

读取/清除：

- `0xC008` 起读取最近 100 条事件。
- `0x1007` 写 `0x0001` 清空事件记录并保存。

风险点：日志每次保存的是完整 100 条快照，不是只追加单条记录；journal 降低擦写次数，但异常频繁启停或多事件变化仍需要磨损估算。

## 工厂老化存储

工厂老化同时用 BKP 和 Flash：

- BKP：`BKP_DR6..DR10` 保存 magic、反码、elapsed low/high、CRC。
- Flash：`0x0801F400` 保存 `elapsed10ms/state/durationHours`。

保存策略：

- BKP 默认每 1 秒保存一次进度。
- Flash 默认每 7200 秒保存一次，或者在启动、停止、完成、设置时强制保存。
- Flash 使用单页 journal，不是每秒擦写。

恢复策略：

- 启动时读取 Flash 和 BKP。
- 如果 BKP 进度大于 Flash 进度，采用 BKP 进度。
- 完成状态 `DONE` 优先。

结论：这个设计对 Flash 寿命相对友好，BKP 用来降低断电损失；但备份域被清除或 `BKP_DeInit()` 后，秒级进度会丢失，只剩 Flash 的上次保存点。

## BKP 备份寄存器占用表

| BKP 寄存器 | 当前用途 | 校验 |
|---|---|---|
| `BKP_DR1` | RTC 初始化 marker `0xA5A5` | 单值 |
| `BKP_DR2` | 睡眠启动标志 | `DR3` 反码 |
| `BKP_DR3` | 睡眠启动标志反码 | 反码 |
| `BKP_DR4` | LED 睡前 SOC，带 `0x5A00` magic | `DR5` 反码 |
| `BKP_DR5` | LED 睡前 SOC 反码 | 反码 |
| `BKP_DR6` | 工厂老化 magic | `DR7` 反码 |
| `BKP_DR7` | 工厂老化 magic 反码 | 反码 |
| `BKP_DR8` | 工厂老化 elapsed low16 | `DR10` CRC |
| `BKP_DR9` | 工厂老化 elapsed high16 | `DR10` CRC |
| `BKP_DR10` | 工厂老化 CRC | CRC |
| `BKP_DR11` | Cortex fault reason | `DR12` 反码 |
| `BKP_DR12` | Cortex fault reason 反码 | 反码 |

风险点：`RTC_ClockConfig()` 在首次 RTC 初始化时调用 `BKP_DeInit()`，`RTC_ReinitWithLsiClock()` 也会调用 `BKP_DeInit()`。这会清空所有 BKP 状态，包括睡眠标志、LED 睡前 SOC、老化短期进度和 fault reason。BKP 只能作为短期状态，不应当当作可靠长期存储。

## 睡眠/低功耗相关存储

进入 reset-sleep 前：

1. `LowPowerSleep_SaveResetState()` 调用 `LowPowerSleep_SaveCoreState()`。
2. `LowPowerSleep_SaveCoreState()` 依次执行 `Can_PrepareSleep()`、`SOC_SaveSnapshotBeforeSleep()`、`FactoryAging_SaveProgressBeforeSleep()`。
3. `LedBar_SaveSleepSoc()` 把当前 SOC 写入 `BKP_DR4/DR5`。
4. `BootFlag_Write()` 把睡眠模式写入 `BKP_DR2/DR3`。
5. AFE sleep 后 MCU reset。

复位后：

- `IsSleepStartUp()` 读取 `BKP_DR2/DR3`。
- 对 HICCUP/NORMAL/DEEP 休眠，先清标志，再进入低功耗等待唤醒。
- 充电唤醒会写 `FLASH_SLEEP_CHARGER_WAKE_VALUE` 到 BKP 标志。

低功耗阻塞：

- `LP_GetBlockReason()` 会在 `StorageFlash_IsBusy()` 或 `u8FlashUpdateE2PROM != 0` 时设置 `LP_BLOCK_FLASH_BUSY`。
- `u8FlashUpdateFlag != 0` 时设置 `LP_BLOCK_UPGRADE`。

结论：睡眠标记已从 Flash 迁移到 BKP，避免每次睡眠擦写 Flash；但命名仍带 `FLASH_*`，容易误解。

## 升级/IAP 相关存储

当前 App 请求进入 IAP 不写 `FLASH_ADDR_UPDATE_FLAG`。`AppUpgrade_RequestIap()` 写 SRAM mailbox `0x20004FE0`：

- magic `0x49415031`
- request `0x5AA55AA5`
- magic/request 反码
- XOR CRC

Modbus `0xFFFD` 或 CAN `ENTER_IAP` 会先写 mailbox，再设置延迟 reset 标志。串口路径通过 `u8FlashUpdateE2PROM` 等待应答发完后设置 `u8FlashUpdateFlag`；CAN 路径延迟若干 10ms tick 后设置 `u8FlashUpdateFlag`。

`FLASH_ADDR_UPDATE_FLAG 0x0801F800` 当前只保留宏和旧文档语义，主 App 源码没有写它。

烧录安全边界：

- `tools/soc_flash_app_safe.ps1` 默认只允许 `0x08004800`。
- 脚本明确拒绝把 App 写到 `0x08000000`。
- 当前仓库没有在源控中找到 `103 + 309/Project/Users/Objects/FD_Release.sct`，且 Keil XML 同时存在 `IROM 0x08000000/0x10000`、`OCR_RVCT4 0x08004800/0x20000`、空 `ScatterFile`。必须用最终 map 验证 App 实际链接地址。

## Product ID / SN / 版本

`ProductionInfor` 当前是 RAM 运行态：

- 启动时 `InitProID()` 从编译期默认字符串加载。
- `0xC002` 读取序列号、硬件版本、软件版本。
- `0xFFF0/0xFFF1/0xFFF2` 写入会修改 RAM 中的 `ProductionInfor`。

当前没有看到 `ProductionInfor` 保存到 `StorageFlash_*`。因此写 SN/版本后复位会丢失。若量产要求 SN/版本可现场写入并保持，需要单独确认并设计持久化区或并入 RW 参数。

## 当前写入口分类

| 写入口 | 是否持久化 | 当前行为 |
|---|---|---|
| `0x2100` 保护参数 | 是 | 范围校验、保存 RW_PARAM、失败回滚 |
| `0x2300` OtherElement | 是 | 范围校验、保存 RW_PARAM、失败回滚 |
| `0x2400` AFE 参数 | 是 | 保存 MCU Flash，后续写 AFE MTP |
| `0x1002` reset protect | 是 | 默认保护参数保存 |
| `0x1003` reset OtherElement | 是 | 默认 OtherElement 保存 |
| `0x1006` reset AFE 参数 | 是 | 默认 AFE 参数保存 |
| `0x1007` reset event record | 是 | 清空事件日志保存 |
| `0x1005` set SOC once | 是，间接 | 先置命令，SOC 任务消费后保存快照 |
| CAN aging start/stop/reset/set-hours | 是 | 保存老化 Flash 和 BKP |
| `0xFFFD` flash connect | 否，SRAM mailbox | 请求 IAP reset |
| `0xFFF0..0xFFF2` SN/版本 | 否 | 只改 RAM |
| `0x2000` 校准 K/B | 否 | 写处理函数为空，可能 ACK 成功但无动作 |
| `0x2200` SOC 表 | 否 | 写入被负响应拒绝 |
| `0x2200` 后的 RTC 区 | 否 | `Sci_WrRegs_0x10_RTC()` 为空，可能 ACK 成功但无动作 |

## 风险清单

| ID | 风险 | 证据 | 影响 | 建议 |
|---|---|---|---|---|
| STOR-RISK-001 | Flash 容量/链接/存储区边界不完全闭环 | `Flash.h` 使用 `0x0801C000+`；`uvprojx` 同时有 `IROM 0x08000000`、`OCR_RVCT4 0x08004800`、空 scatter | P0：可能覆盖 IAP、App 或越界写 Flash | 必须用最终 map、芯片 Flash size register、烧录脚本一起验证 |
| STOR-RISK-002 | `ERROR_EEPROM_STORE` 不会被计数 | `System_ERROR_UserCallback()` 对 `ERROR_EEPROM_STORE` 跳过自增 | Flash 保存失败可能不可见，日志也可能不记录 EEPROM_ERR | 优先修复错误标志可见性，再做其它存储优化 |
| STOR-RISK-003 | 空写 handler 可能 ACK 成功但无动作 | `Sci_WrRegs_0x10_CalibCoef()`、`Sci_WrRegs_0x10_RTC()`、`Sci_WrReg_0x06_Reset_CalibCoef()` 为空 | 上位机误以为校准/RTC 写入成功 | 空功能应显式 NEG，或实现持久化 |
| STOR-RISK-004 | Product ID 写入不持久 | SN/版本写入口只改 RAM | 现场写 SN/版本后复位丢失 | 确认需求：只读默认值还是允许持久化写 |
| STOR-RISK-005 | `FlashWriteOneHalfWord()` 是整页擦后写半字 | 当前用于 `0x0801F000`，函数名看不出整页擦 | 后续误用会擦掉同页其它数据 | 限定为专用 API 或加地址白名单 |
| STOR-RISK-006 | BKP 会被 `BKP_DeInit()` 清空 | RTC 初次初始化和 LSI reinit 都会调用 | 睡眠标志、老化短进度、fault reason 可能丢失 | 文档明确 BKP 只作短状态；关键状态仍需 Flash |
| STOR-RISK-007 | AFE MTP 失败后可能密集重试 | `AFE_PARAM_WRITE_Flag` 失败后重新置 1 | AFE 异常时可能反复写 MTP | 增加失败退避或上电一次重试策略 |
| STOR-RISK-008 | 日志保存整块快照 | 每次日志事件保存 202-byte payload | 异常频发时 Flash 磨损增加 | 增加写频率统计或改为更细粒度追加 |
| STOR-RISK-009 | SOC 小步变化会触发保存 | 判重字段含 SOC | 长期低压尾端/静置校准可能写放大 | 用 host replay 统计典型工况写次数 |
| STOR-RISK-010 | CRC 长度强转 `UINT8` | `StorageFlash_CalcCrc()` | 结构体扩展到 256 字节以上会校验错误 | 保持 payload 小于 255，或改 CRC 接口类型 |

## 建议的后续确认项

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| STOR-REQ-001 | 量产硬件必须保证 `0x0801C000+` 内部 Flash 可用 | `Flash.h` 存储地址、`StorageFlash_PrintBootCheck()` | 运行保存路径默认直接使用尾部地址 | P0 | UNKNOWN | 当前量产芯片真实 Flash 是 128KB 还是 64KB 兼容料？ | 先以芯片读数和 map 确认，再允许写尾部存储区 | 待确认 |
| STOR-REQ-002 | App 链接和烧录必须固定 `0x08004800`，不能覆盖 IAP | `Flash.h`、安全脚本、`uvprojx` 冲突 | 安全脚本固定 `0x08004800`，Keil XML 口径不单一 | P0 | MUST_KEEP | 是否把 map 检查作为发布前强制门禁？ | 保留 `0x08004800` 并强制 map 校验 | 待确认 |
| STOR-REQ-003 | 存储失败必须可见 | `StorageFlash_*` 失败调用 `ERROR_EEPROM_STORE`，但错误函数不计数 | 可能无法在 `D000` 状态和日志中看见 | P1 | CHANGE_NEEDED | 是否优先修复 `ERROR_EEPROM_STORE` 标志？ | 优先修，且回归日志 `EEPROM_ERR` | 待确认 |
| STOR-REQ-004 | SN/硬件版本/软件版本是否需要持久化 | `ProductionID.c`、`Sci_WrRegs_0x10_SN_Version()` | 当前只保存在 RAM | P1 | UNKNOWN | 上位机写 SN/版本是否要求掉电保持？ | 若要求量产写入，新增受保护持久化设计 | 待确认 |
| STOR-REQ-005 | 校准 K/B 和 RTC 写入口是否保留 | `Sci_WrRegs_0x10_CalibCoef()`、`Sci_WrRegs_0x10_RTC()` 为空 | 可能写成功但无动作 | P1 | CHANGE_NEEDED | 当前上位机是否仍会写校准/RTC？ | 不支持则显式 NEG；支持则实现并保存 | 待确认 |
| STOR-REQ-006 | SOC 快照是否必须保留 | `soc_load_or_default()`、`soc_save_if_needed()` | 冷启动恢复 SOC/capacity/cycle | P1 | MUST_KEEP | SOC 掉电恢复是否是必须功能？ | 保留，但增加写频率统计 | 待确认 |
| STOR-REQ-007 | 事件日志是否必须保留 | `LogRecord.c`、`0xC008` | 100 条事件记录持久化 | P2 | KEEP_BUT_REFACTOR | 上位机/售后是否依赖事件日志？ | 保留，后续评估写频率和记录格式 | 待确认 |
| STOR-REQ-008 | 工厂老化进度是否必须掉电保持 | `FactoryAging.c` | BKP 秒级保存，Flash 2 小时/状态保存 | P1 | MUST_KEEP | 老化过程掉电后是否必须继续累计？ | 保留 BKP+Flash 双层策略 | 待确认 |
| STOR-REQ-009 | 睡眠标志是否继续使用 BKP | `SleepDeal.c` | 不再写 `FLASH_ADDR_SLEEP_FLAG` | P1 | MUST_KEEP | 是否确认不要恢复 Flash 睡眠标志？ | 继续用 BKP，减少 Flash 磨损 | 待确认 |
| STOR-REQ-010 | AFE MTP 重试策略是否需要限流 | `SH367309_UpdataAfeConfig()` | 失败后下周期继续尝试 | P2 | CHANGE_NEEDED | AFE MTP 写失败时是否允许持续重试？ | 增加退避/错误锁存策略 | 待确认 |

## 建议测试

1. 构建后检查 map：`LR_IROM1` 和 `ER_IROM1` 必须为 `0x08004800`，App 不能覆盖 `0x0801C000+` 存储区。
2. 上电读取 Flash size register，确认实际芯片容量至少 128KB。
3. 对 `StorageFlash_SaveRwParamData()`、`SaveAfeData()`、`SaveSocData()`、`SaveLogData()`、`SaveFactoryAgingData()` 做 host 或板端回读校验。
4. 人为破坏一个 slot 的 magic/CRC，验证双槽或 journal 能选择另一份有效数据。
5. 模拟 Flash 写失败，确认 `ERROR_STATUS_EEPROM_STORE` 能在 `0xD000` 状态和事件日志中可见。
6. 回归 Modbus/CAN 写保护参数、OtherElement、AFE 参数，确认失败回滚和 ACK 语义。
7. 验证 `0x2000` 校准、RTC 写、SN 写的当前 ACK 行为，决定是实现还是显式拒绝。
8. 用 SOC replay 统计 24h/7d 工况下 `StorageFlash_SaveSocData()` 调用次数。
9. 用异常事件注入统计日志写入次数和 page erase 周期。
10. 老化运行超过 2 小时，断电/复位后验证 BKP+Flash 进度恢复。

## 后续简化方向

1. 把 `EEPROM.c` 从“外部 EEPROM 名称”逐步收敛为 `StorageParam` 或类似边界；先不改协议地址。
2. 给 `FlashWriteOneHalfWord()` 加专用命名或地址白名单，避免误用整页擦写。
3. 空写 handler 改成统一显式 NEG，避免“写成功但无动作”。
4. Product ID 是否持久化先等需求确认，不要直接塞进现有 RW_PARAM。
5. 为 SOC/日志/老化增加写计数 debug watch，先拿数据再决定是否优化 journal。
6. 修复 `ERROR_EEPROM_STORE` 可见性，这是进入存储重构前最小且高收益的改动。

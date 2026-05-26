# 103-309 BMS 从源码反推的需求确认表

> 说明：本表不是最终需求规格书，而是从当前源码、宏定义、协议入口和工程配置反推出来的“现状需求”。每条需求都需要后续由你确认是否保留、修改或删除。
> 字段约定：
> - 对外可见：是否会被用户、上位机、CAN、硬件端口或量产流程感知。
> - 上位机协议：是否影响 Modbus/CAN/PC 工具兼容。
> - 安全：是否可能影响 BMS 保护、MOS、充放电、低压、升级安全。
> - 低功耗：是否影响 sleep/STOP/RTC/IWDG/唤醒。
> - 兼容性：是否影响历史固件、客户协议、参数区、硬件版本。

## 1. 系统启动需求

| ID | 需求描述 | 代码证据 | 涉及文件 | 当前实现方式 | 对外可见 | 上位机协议 | 安全 | 低功耗 | 兼容性 | Codex 初步判断 |
|---|---|---|---|---|---|---|---|---|---|---|
| REQ-SYS-001 | 系统启动必须先完成硬件、参数、AFE、CAN、ADC、SOC、TIM3 初始化，再进入主循环 | `main.c:7-11`, `AppInit.c:10-38` | `main.c`, `AppInit.c` | 单次启动初始化，无 RTOS | 是 | 间接 | 是 | 是 | 是 | MUST_KEEP；启动顺序不能随意改 |
| REQ-SYS-002 | 支持从 sleep/reset flag 恢复启动，并在真正进入运行态前处理唤醒合法性 | `AppInit.c:16`, `SleepDeal.c:186-243` | `SleepDeal.c`, `conf.c` | 读取 BKP_DR2/DR3，按 NORMAL/HICCUP/DEEP 进入 STOP 等待合法唤醒 | 是 | 否 | 是 | 是 | 是 | MUST_KEEP，但唤醒条件需要实测确认 |
| REQ-SYS-003 | 默认构建必须是量产 profile 0 | `Project_Config.h:17`, `Project_BuildGuard.h` | `Project_Config.h`, `Project_BuildGuard.h` | 宏配置 + build guard | 间接 | 间接 | 是 | 间接 | 是 | MUST_KEEP；测试功能必须隔离 |
| REQ-SYS-004 | 当前主 MCU 目标是 STM32F103/标准外设库，工程仍保留向 F0/其他模板演进空间 | `uvprojx` 使用 Cortex-M3/STM32F103C8，`STM32F10x_StdPeriph_Lib_V3.5.0` | Keil 工程、StdPeriph | 使用 SPL 和少量寄存器直接操作 | 否 | 否 | 间接 | 间接 | 是 | MUST_KEEP；后续做 F0/F1 兼容需抽 BSP |
| REQ-SYS-005 | 系统运行状态、功能特性、错误标志需要被统一上报 | `Sci_Upper.c:820-845`, `System_Monitor.h` | `System_Monitor.c/.h`, `Sci_Upper.c` | 全局错误/状态结构映射到 `0xD000+` | 是 | 是 | 是 | 间接 | 是 | MUST_KEEP；但全局状态边界可重构 |

## 2. 参数初始化与 Flash/EEPROM/参数存储需求

| ID | 需求描述 | 代码证据 | 涉及文件 | 当前实现方式 | 对外可见 | 上位机协议 | 安全 | 低功耗 | 兼容性 | Codex 初步判断 |
|---|---|---|---|---|---|---|---|---|---|---|
| REQ-FLASH-001 | 项目必须保留 EEPROM API 名称兼容，但实际参数存储使用内部 Flash | `EEPROM.c:313-347`, `Flash.h:22-35` | `EEPROM.c/.h`, `Flash.c/.h` | 旧 EEPROM 函数空实现，核心参数走 `StorageFlash_*` | 间接 | 是 | 是 | 间接 | 是 | KEEP_BUT_REFACTOR；命名应文档化，避免误判 |
| REQ-FLASH-002 | 保护参数和 OtherElement 参数必须有默认值、合法范围检查和 Flash 持久化 | `EEPROM.c:33-188`, `Sci_Upper.c:1818-1935` | `EEPROM.c`, `Sci_Upper.c`, `DataDeal.h` | 默认值加载、范围检查、写入失败回滚 | 是 | 是 | 是 | 否 | 是 | MUST_KEEP；写权限策略需确认 |
| REQ-FLASH-003 | SOC snapshot 必须持久化并兼容 V1/V2 格式 | `Flash.c:740-801`, `SocEnhance.c:561-668` | `Flash.c/.h`, `SocEnhance.c` | journal pair，V1 转 V2，休眠前保存 | 间接 | 间接 | 是 | 是 | 是 | MUST_KEEP；Flash 容量和写频率需验证 |
| REQ-FLASH-004 | AFE 参数必须能从 Flash 加载，必要时恢复默认并写入 AFE MTP/寄存器 | `SH367309_DataDeal.c:251-375`, `Flash.c:803-835` | `SH367309_DataDeal.c`, `Flash.c` | 24 words AFE 参数双槽保存 | 是 | 是 | 是 | 否 | 是 | MUST_KEEP，但生产现场写 AFE 参数需权限确认 |
| REQ-FLASH-005 | 事件/故障记录必须持久化，且重复记录有最小间隔 | `Project_Config.h:229`, `Flash.c:872-929`, `LogRecord.c` | `LogRecord.c`, `Flash.c` | 事件环形记录 + Flash journal | 是 | 是 | 间接 | 否 | 是 | MUST_KEEP；日志粒度可后续整理 |
| REQ-FLASH-006 | 工厂老化状态和已运行时间必须掉电保持 | `FactoryAging.c:129-182`, `FactoryAging.c:199-243`, `Flash.c:931-975` | `FactoryAging.c`, `Flash.c` | BKP 每秒级保存，Flash 每 2 小时/关键动作保存 | 是 | 是 | 间接 | 是 | 是 | UNKNOWN；需确认老化需求是否长期保留 |
| REQ-FLASH-007 | 升级后可一次性执行参数重置策略 | `Project_Config.h:393-433`, `EEPROM.c:228-310` | `UpgradeParamPolicy.h`, `EEPROM.c` | 版本 flag `0x0004`，当前重置 SOC snapshot 和事件记录 | 是 | 是 | 是 | 间接 | 是 | KEEP_BUT_REFACTOR；每次策略必须写文档 |
| REQ-FLASH-008 | 内部 Flash 持久化区域固定使用后 64K 地址 | `Flash.h:7-30`, `Flash.c:1017-1025` | `Flash.h`, `Flash.c` | `0x0801C000` 以后作为存储区 | 否 | 间接 | 是 | 是 | 是 | UNKNOWN；必须确认实际芯片容量/量产型号 |

## 3. ADC / AFE / 采集需求

| ID | 需求描述 | 代码证据 | 涉及文件 | 当前实现方式 | 对外可见 | 上位机协议 | 安全 | 低功耗 | 兼容性 | Codex 初步判断 |
|---|---|---|---|---|---|---|---|---|---|---|
| REQ-ADC-001 | ADC1 必须通过 TIM2_CC2 触发、DMA1_Channel1 扫描 3 路模拟量 | `ADC.c:90-123`, `ADC.c:149-261` | `ADC.c/.h` | ADC1 scan + DMA circular，通道 PB1/PA2/PA1 | 否 | 间接 | 是 | 是 | 是 | MUST_KEEP；通道表需要整理 |
| REQ-ADC-002 | Type-C 输出电流要折算成等效电池电流进入 SOC | `ADC.c:367-418`, `SOC.c:104-172` | `ADC.c`, `SOC.c` | PA2 电流平滑后按 VBUS/Vbat 折算 | 间接 | 是 | 是 | 否 | 是 | UNKNOWN；需确认 Type-C 功能是否当前产品必需 |
| REQ-ADC-003 | VBC/总压 ADC 要参与系统监控和 Type-C 折算 | `ADC.c:431-458` | `ADC.c` | PA1 分压滤波得到 `g_u32VbatAdcMv` | 间接 | 间接 | 是 | 否 | 是 | MUST_KEEP，但和 AFE 总压的一致性需验证 |
| REQ-ADC-004 | 低功耗前 ADC/TIM2/DMA 必须停止，唤醒后重新初始化 | `ADC.c:268-285`, `conf.c:114-118`, `conf.c:392-421` | `ADC.c`, `conf.c` | STOP 前 `ADC_StopForLowPower()`，唤醒后 `InitADC()` | 否 | 否 | 间接 | 是 | 是 | MUST_KEEP |
| REQ-AFE-001 | 当前 AFE 驱动固定为 SH367309，使用软件 I2C 读写 | `Project_Config.h:76`, `I2C_AFE1.h:1-66`, `I2C_AFE1.c` | `I2C_AFE1.c/.h`, `SH367309_*` | 读 MTP/状态寄存器，CRC8 校验 | 否 | 间接 | 是 | 是 | 是 | KEEP_BUT_REFACTOR；长期目标需要 AFE 抽象层 |
| REQ-AFE-002 | 启动时必须初始化 AFE、写保护配置、执行启动零点/初始 MOS 策略 | `I2C_AFE1.c:688-714` | `I2C_AFE1.c`, `MosStartup.c` | reset AFE，更新配置，应用初始 MOS 状态 | 是 | 否 | 是 | 否 | 是 | MUST_KEEP |
| REQ-AFE-003 | AFE 配置由保护参数和 OtherElement 转换为 ROM 寄存器并写入 | `SH367309_DataDeal.c:39-113`, `SH367309_DataDeal.c:182-249` | `SH367309_DataDeal.c` | 比较 ROM，差异写入，reset AFE，再打开驱动 | 是 | 是 | 是 | 否 | 是 | MUST_KEEP，但硬编码项需确认 |
| REQ-AFE-004 | AFE 状态/故障必须映射到 BMS fault flags 和日志 | `SH367309_Func.c:228-305`, `SH367309_Func.c:307-390` | `SH367309_Func.c`, `System_Monitor.h` | 读 status，映射 OVP/UVP/OCP/CBC/MOS 状态 | 是 | 是 | 是 | 间接 | 是 | MUST_KEEP |
| REQ-AFE-005 | AFE 当前采样必须驱动 SOC 更新序号 | `DataDeal.c:807-871`, `DataDeal.c:1225-1249`, `SOC.c:203-237` | `DataDeal.c`, `SOC.c` | 200ms 更新一次并递增 `g_u32AfeCurrentSampleSeq` | 是 | 是 | 是 | 间接 | 是 | CHANGE_NEEDED；当前实际电流路径疑似错误 |
| REQ-AFE-006 | AFE 在低功耗前必须进入 sleep，并在唤醒后恢复 I2C/配置 | `SleepDeal.c:109-114`, `rtc_sleep_afe_sh367309.c`, `conf.c:392-421` | `SleepDeal.c`, `rtc_sleep_afe_sh367309.c`, `conf.c` | sleep/reset 或 STOP 恢复后重新初始化接口 | 是 | 否 | 是 | 是 | 是 | MUST_KEEP |

## 4. 电压、电流、温度采集需求

| ID | 需求描述 | 代码证据 | 涉及文件 | 当前实现方式 | 对外可见 | 上位机协议 | 安全 | 低功耗 | 兼容性 | Codex 初步判断 |
|---|---|---|---|---|---|---|---|---|---|---|
| REQ-MEAS-001 | 电芯电压按实际串数映射，未使用电芯填 61001 | `DataDeal.c:137-173` | `DataDeal.c`, `DataDeal.h` | 从 AFE cell 数组映射到 `u16VCell[]`，13 串有特殊跳位 | 是 | 是 | 是 | 否 | 是 | MUST_KEEP；13 串跳位需硬件确认 |
| REQ-MEAS-002 | 最高/最低单体和总压必须根据串数、K/B 校准计算 | `DataDeal.c:175-220` | `DataDeal.c` | 遍历 `SeriesNum`，计算 max/min/total | 是 | 是 | 是 | 否 | 是 | MUST_KEEP |
| REQ-MEAS-003 | 温度包含 AFE 两路温度、MOS ADC 温度，ENV2/ENV3 当前强制 -40 | `DataDeal.c:227-277` | `DataDeal.c`, `ADC.c` | 两路 AFE temp + MOS temp，两个环境温度占位 | 是 | 是 | 是 | 间接 | 是 | UNKNOWN；需确认温度探头数量和上位机显示预期 |
| REQ-MEAS-004 | 充电/放电电流分别上报，并支持启动零点和死区 | `DataDeal.c:807-871` | `DataDeal.c`, `I2C_AFE1.c` | AFE CADC raw -> 充/放电 mA/0.1A | 是 | 是 | 是 | 否 | 是 | MUST_KEEP，但当前调用被虚拟电流替代 |
| REQ-MEAS-005 | 调试虚拟电流可在特定 debug 状态下注入 | `Project_Config.h:110`, `DataDeal.c:856-868`, `DataDeal.c:1040-1084` | `DataDeal.c`, `conf.h` | `__VIRTURE_CURRENT__` 和 test loop | 是 | 是 | 是 | 否 | 否 | CHANGE_NEEDED；必须编译隔离，不能混入量产主路径 |

## 5. 保护逻辑、均衡、MOS 控制需求

| ID | 需求描述 | 代码证据 | 涉及文件 | 当前实现方式 | 对外可见 | 上位机协议 | 安全 | 低功耗 | 兼容性 | Codex 初步判断 |
|---|---|---|---|---|---|---|---|---|---|---|
| REQ-PROT-001 | OVP/UVP/OCP/OTP/UTP 等保护阈值必须可由参数驱动并下发 AFE | `DataDeal.h:225-231`, `SH367309_DataDeal.c:39-113` | `DataDeal.h`, `SH367309_DataDeal.c` | `PRT_E2ROMParas` -> AFE ROM fields | 是 | 是 | 是 | 否 | 是 | MUST_KEEP |
| REQ-PROT-002 | AFE status 必须驱动 fault flags、日志和 CAN exception_status | `SH367309_Func.c:228-305`, `CanFeidaoFrames.c:169-241` | `SH367309_Func.c`, `CanFeidaoFrames.c` | status bits -> fault + CAN 状态 | 是 | 是 | 是 | 间接 | 是 | MUST_KEEP |
| REQ-PROT-003 | `new_todo_logi()` 内存在充电器检测、MOS 温度、UL/RF_EN 熔断类逻辑 | `DataDeal.c:1086-1223` | `DataDeal.c` | 200ms 逻辑中直接控制 MOS/低功耗/RF_EN | 是 | 间接 | 是 | 是 | 未知 | UNKNOWN；强客户/认证需求必须确认 |
| REQ-BAL-001 | 参数表保留均衡开压/窗口/保留位，并允许上位机读写 | `DataDeal.h:141-149`, `Sci_Upper.h:466-478`, `Sci_Upper.c:1937-1940` | `DataDeal.h`, `Sci_Upper.c/.h` | OtherElement 中保存，0x2300 起读写 | 是 | 是 | 可能 | 否 | 是 | UNKNOWN；当前看不到主动均衡任务 |
| REQ-BAL-002 | AFE balance status / CBC error 必须被读取并上报 | `SH367309_Func.c:307-390`, `System_Monitor.h` | `SH367309_Func.c`, `System_Monitor.h` | 读 `MTP_BALANCEH` 5 bytes，映射 CBC_DSG | 是 | 是 | 是 | 否 | 是 | KEEP_BUT_REFACTOR；status 和主动控制需区分 |
| REQ-MOS-001 | 启动时 MOS 初始状态由 `MosStartup` 和 AFE driver 控制 | `I2C_AFE1.c:688-714` | `MosStartup.c/.h`, `I2C_AFE1.c` | 根据启动状态/5V 充电决定初始 MOS | 是 | 否 | 是 | 是 | 是 | MUST_KEEP |
| REQ-MOS-002 | AFE MOS 可通过 `SH367309_DriverMos_Ctrl()` 控制 CHG/DSG | `SH367309_Func.c:189-210` | `SH367309_Func.c` | 写 `MTP_CONF` 中 CHGON/DSGON | 是 | 间接 | 是 | 否 | 是 | MUST_KEEP |
| REQ-MOS-003 | 充电器插拔会打开/关闭 CHG/DSG 并可能进入深睡 | `DataDeal.c:63-107` | `DataDeal.c`, `SleepDeal.c` | CHG_IN low -> charge path；拔除后 deep sleep | 是 | 否 | 是 | 是 | 未知 | UNKNOWN；需确认真实产品体验 |

## 6. SOC 需求

| ID | 需求描述 | 代码证据 | 涉及文件 | 当前实现方式 | 对外可见 | 上位机协议 | 安全 | 低功耗 | 兼容性 | Codex 初步判断 |
|---|---|---|---|---|---|---|---|---|---|---|
| REQ-SOC-001 | SOC 以 200ms AFE 电流采样为主驱动，不允许无新样本重复积分 | `SOC.c:203-237` | `SOC.c`, `DataDeal.c` | sample seq 变化才更新 | 是 | 是 | 是 | 间接 | 是 | MUST_KEEP |
| REQ-SOC-002 | SOC snapshot 失效时可由 OCV/default 60% 初始化 | `SocEnhance.c:609-668` | `SocEnhance.c` | Flash valid -> load；否则 OCV/default 并保存 | 是 | 是 | 是 | 是 | 是 | MUST_KEEP；初始 60% 策略需确认 |
| REQ-SOC-003 | SOC 支持库仑积分、SOH、循环次数、容量学习字段 | `SocEnhance.h:19-78`, `SocEnhance.c:747-804` | `SocEnhance.c/.h` | 内部 cap/cycle/learn 状态 | 是 | 是 | 间接 | 间接 | 是 | KEEP_BUT_REFACTOR |
| REQ-SOC-004 | 满电必须满足电压/持续时间/阈值条件，且 1% 步进到 100% | `Project_Config.h:235-258`, `SocEnhance.c:913-940`, `SocEnhance.c:1268-1318` | `SocEnhance.c`, `Project_Config.h` | full anchor + fast confirm + 1% step | 是 | 是 | 是 | 否 | 是 | MUST_KEEP |
| REQ-SOC-005 | 低压尾段必须通过 V0 和 empty tail 限制 SOC 虚高 | `Project_Config.h:308-333`, `SocEnhance.c:1104-1266` | `SocEnhance.c` | tail target、empty fast、display low | 是 | 是 | 是 | 是 | 是 | MUST_KEEP |
| REQ-SOC-006 | sag/rebound holdoff 必须避免低压瞬态导致 OCV 误校准 | `Project_Config.h:277-281`, `SocEnhance.c:980-1014` | `SocEnhance.c` | sag holdoff 秒计数 | 是 | 否 | 是 | 是 | 是 | MUST_KEEP |
| REQ-SOC-007 | 静置 OCV 校准必须满足静置时间、稳定时间、目标步进 | `Project_Config.h:285-304`, `SocEnhance.c:1320-1516` | `SocEnhance.c` | rest OCV deferred target | 是 | 否 | 是 | 是 | 是 | MUST_KEEP |
| REQ-SOC-008 | 显示 SOC 需要平滑，低电量允许更快下降 | `Project_Config.h:319-333`, `SocEnhance.c:1518-1590` | `SocEnhance.c`, `LedBar.c` | normal/chg/low 不同步进周期 | 是 | 是 | 是 | 否 | 是 | MUST_KEEP |
| REQ-SOC-009 | SOC 测试模式必须在量产关闭，但保留 `0xD300` 状态窗口 | `Project_Config.h:340-346`, `SOC.c:288-324`, `Sci_Upper.c:855-863` | `SOC.c`, `Sci_Upper.c` | 关闭时 `supported=0` | 是 | 是 | 是 | 否 | 是 | MUST_KEEP；隔离规则必须保留 |
| REQ-SOC-010 | 上位机可写 SOC 命令/参数应触发 SOC 重新初始化或指定动作 | `SocEnhance.c:1592-1643`, `Sci_Upper.c:605-666` | `SocEnhance.c`, `Sci_Upper.c` | refresh flag、capacity reset、one-shot set SOC | 是 | 是 | 是 | 否 | 是 | UNKNOWN；现场写权限需确认 |

## 7. 通信和上位机兼容需求

| ID | 需求描述 | 代码证据 | 涉及文件 | 当前实现方式 | 对外可见 | 上位机协议 | 安全 | 低功耗 | 兼容性 | Codex 初步判断 |
|---|---|---|---|---|---|---|---|---|---|---|
| REQ-UART-001 | UART1 使用 Modbus RTU，固定 19200 8N1，PB6/PB7 remap | `Sci_Upper.c:1610-1735`, `conf_gpio.h` | `Sci_Upper.c`, `conf_gpio.h` | USART1 中断接收，IDLE/长度判帧 | 是 | 是 | 间接 | 是 | 是 | MUST_KEEP |
| REQ-UART-002 | Modbus 支持 0x03/0x06/0x10，带 CRC16 和异常码 | `Sci_Upper.h:12-20`, `Sci_Upper.c:1215-1344` | `Sci_Upper.c/.h` | 帧 feed/process/ack 分离 | 是 | 是 | 间接 | 否 | 是 | MUST_KEEP |
| REQ-UART-003 | `0xD000/0xD100/0xD200/0xD300` 只读窗口必须兼容上位机 | `Sci_Upper.h:139-151`, `Sci_Upper.c:787-864` | `Sci_Upper.c/.h` | 从 `g_stCellInfoReport`、RTC、fault、debug status 输出 | 是 | 是 | 是 | 是 | 是 | MUST_KEEP |
| REQ-UART-004 | `0xC002` 必须返回 BMS 序列号、硬件版本、软件版本各 16 bytes | `Sci_Upper.c:769-773` | `Sci_Upper.c`, `ProductionID.c` | 48 bytes product info | 是 | 是 | 否 | 否 | 是 | MUST_KEEP；默认值需替换/确认 |
| REQ-UART-005 | Host 写参数在当前量产宏下开启 | `Project_Config.h:45`, `Sci_Upper.c:314-367`, `Sci_Upper.c:668-737` | `Project_Config.h`, `Sci_Upper.c` | 保护/Other/SN/IAP 等地址可写 | 是 | 是 | 是 | 是 | 是 | UNKNOWN；量产权限策略必须确认 |
| REQ-CAN-001 | CAN 必须周期广播飞道协议扩展帧 `0x14F80200+index` | `CanFeidaoFrames.c:5-35` | `CanFeidaoFrames.c`, `Can_HDX.c` | 1000ms/5000ms frame dispatch | 是 | 是 | 是 | 是 | 是 | MUST_KEEP；协议字段需锁定 |
| REQ-CAN-002 | CAN 应用命令必须支持读写 Modbus 寄存器、状态查询、IAP、老化控制 | `Can_HDX.c:29-54`, `Can_HDX.c:609-775` | `Can_HDX.c`, `Sci_Upper.c` | 标准帧 0x60/0x61，A5/5A + CRC | 是 | 是 | 是 | 是 | 是 | MUST_KEEP；写权限与 IAP 必须受控 |
| REQ-CAN-003 | CAN 低功耗 RTC 唤醒时需要短时间上电服务周期帧 | `Can_HDX.c:947-979`, `rtc_sleep.c:329-333` | `Can_HDX.c`, `rtc_sleep.c` | RTC wake 后发送 1000ms mask，等待 1.5s 或完成 | 是 | 是 | 否 | 是 | 是 | UNKNOWN；需确认待机广播需求 |
| REQ-CAN-004 | CAN bus-off 需要监控、清队列、恢复计数 | `Can_HDX.c:390-412` | `Can_HDX.c` | ESR BOFF 检测，清 mailbox/queue | 是 | 是 | 间接 | 否 | 是 | MUST_KEEP |

## 8. LED / RTC 低功耗 / IWDG / IAP / 客户需求

| ID | 需求描述 | 代码证据 | 涉及文件 | 当前实现方式 | 对外可见 | 上位机协议 | 安全 | 低功耗 | 兼容性 | Codex 初步判断 |
|---|---|---|---|---|---|---|---|---|---|---|
| REQ-LED-001 | LED 数码/图标显示 SOC 和状态，采用 Charlieplexing 扫描 | `LedBar.c:83-144`, `LedBar.c:938-964`, `LedBar.c:966-1035` | `LedBar.c/.h` | TIM4 扫描，主循环更新显示内容 | 是 | 否 | 间接 | 是 | 是 | KEEP_BUT_REFACTOR；GPIO/低功耗强耦合 |
| REQ-LED-002 | 睡眠前保存 SOC，唤醒/按键预览显示 sleep SOC | `LedBar.c:856-899`, `SleepDeal.c:33-80` | `LedBar.c`, `SleepDeal.c` | BKP_DR4/DR5 保存，按键显示窗口 | 是 | 否 | 否 | 是 | 是 | MUST_KEEP；交互体验需确认 |
| REQ-LED-003 | 长按按键触发 DEEP_MODE 关机 | `Project_Config.h:122`, `LedBar.c:674-682` | `LedBar.c`, `SleepDeal.c` | 500ms 左右长按后 sleep/reset | 是 | 否 | 是 | 是 | 未知 | UNKNOWN；需确认按键产品定义 |
| REQ-LP-001 | 低功耗由运行态 idle check、RTC hiccup STOP、reset sleep 三类路径组成 | `app_lowpower.c:91-146`, `rtc_sleep.c:421-481`, `SleepDeal.c:83-115` | `app_lowpower.c`, `rtc_sleep.c`, `SleepDeal.c` | HICCUP_MODE STOP 循环；NORMAL/DEEP 写 flag 后 reset | 是 | 否 | 是 | 是 | 是 | MUST_KEEP |
| REQ-LP-002 | 低电压必须触发深睡，且 2.8V 以下 60s 强制 deep sleep | `rtc_sleep.c:8-10`, `rtc_sleep.c:155-180` | `rtc_sleep.c`, `rtc_sleep_port.c` | min cell <= 2800mV 强制；低压阈值来自 OtherElement | 是 | 否 | 是 | 是 | 是 | MUST_KEEP；阈值需硬件确认 |
| REQ-LP-003 | RTC wake period 在 IWDG 开启时最大 10s | `RTC.c:17-18`, `RTC.c:386-390`, `RTC.c:406-417` | `RTC.c` | `RTC_WAKEUP_IWDG_SAFE_SECONDS 10` | 否 | 否 | 是 | 是 | 是 | UNKNOWN；功耗目标与看门狗策略冲突需确认 |
| REQ-IWDG-001 | 运行态必须启用 IWDG 并在主循环/阻塞等待中喂狗 | `Project_Config.h:82`, `System_Init.c:33-48`, `Runtime.c:40-42` | `System_Init.c`, `Runtime.c` | LSI IWDG + `Feed_IWatchDog` 宏 | 间接 | 否 | 是 | 是 | 是 | MUST_KEEP |
| REQ-IAP-001 | IAP/Bootloader 地址为 `0x08000000`，App 地址为 `0x08004800` | `Flash.h:4-5`, `tools/soc_flash_app_safe.ps1:17-20` | `Flash.h`, `tools/soc_flash_app_safe.ps1`, Keil 工程 | SRAM mailbox 请求 IAP，安全脚本只烧 App 地址 | 是 | 是 | 是 | 否 | 是 | MUST_KEEP；工程 scatter/map 必须复核 |
| REQ-IAP-002 | 上位机/CAN 可请求进入 IAP，先 ack 再延迟复位 | `Sci_Upper.c:1962-1984`, `Can_HDX.c:640-656`, `Can_HDX.c:788-799` | `Sci_Upper.c`, `Can_HDX.c`, `Flash.c` | 设置 mailbox，`u8FlashUpdateFlag` 后 reset | 是 | 是 | 是 | 否 | 是 | MUST_KEEP；权限和误触发防护需确认 |
| REQ-CUST-001 | 工厂老化默认 3 天，仅累计 MCU 运行态时间，睡眠时间不计入 | `Project_Config.h:163-167`, `FactoryAging.c:345-357` | `FactoryAging.c`, `Project_Config.h` | TIM3 10ms tick 累计，STOP 后 delta=0 | 是 | 是 | 间接 | 是 | 未知 | UNKNOWN；是否当前客户仍需要 |
| REQ-CUST-002 | 老化剩余时间必须通过 CAN 广播和上位机可读 | `CanFeidaoFrames.c:244-260`, `Can_HDX.c:441-457` | `CanFeidaoFrames.c`, `Can_HDX.c`, `FactoryAging.c` | `0x14F80208` 广播分钟，ack 返回小时 | 是 | 是 | 否 | 是 | 是 | MUST_KEEP if 老化保留 |
| REQ-CUST-003 | 产品 ID 当前有默认硬件/软件/SN 字符串，运行态处理后供 `0xC002` 读取 | `DataDeal.h:182-187`, `ProductionID.c`, `Sci_Upper.c:769-773` | `ProductionID.c`, `DataDeal.h`, `Sci_Upper.c` | 默认字符串 + host 写入缓存 | 是 | 是 | 否 | 否 | 是 | CHANGE_NEEDED；默认 `"cs-666-8888"` 不能用于量产 |

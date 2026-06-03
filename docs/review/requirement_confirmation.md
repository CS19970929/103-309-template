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
| REQ-AFE-005 | AFE 当前采样必须驱动 SOC 更新序号 | `DataDeal.c`, `SOC.c` | `DataDeal.c`, `SOC.c` | 200ms 更新一次并通过 `AfeCurrent_GetSeq()` 暴露采样序号 | 是 | 是 | 是 | 间接 | 是 | MUST_KEEP；当前主路径由真实 AFE 电流采样驱动 |
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
| REQ-SOC-009 | 当前无活动 SOC 注入式测试模式；必须保留 SOC_TEST 兼容 padding/协议长度 | `SOC.c:144-162` 为 `#if 0` 空壳，`Sci_Upper.c:828-829` 填充 16 word 0，`Sci_Upper.h:138-140` 定义 padding 长度 | `SOC.c`, `Sci_Upper.c/.h` | 不调用测试状态函数；只保留兼容占位 | 是 | 是 | 是 | 否 | 是 | MUST_KEEP padding；`#if 0` 空壳可后续低风险确认删除 |
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
| REQ-CAN-003 | RTC 休眠中不再周期广播 CAN | `Can_HDX.c`, `rtc_sleep.c`, `RTC.c` | `Can_HDX.c`, `rtc_sleep.c`, `RTC.c` | 睡前 `Can_PrepareSleep()` 关闭 CMNT；RTC 周期唤醒不调用 CAN 服务；唤醒恢复后 `InitCan()` 打开 CMNT | 是 | 是 | 否 | 是 | 是 | CHANGE_NEEDED；用户已确认 |
| REQ-CAN-004 | CAN bus-off 恢复交给 bxCAN ABOM | `Can_HDX.c`, `InitCan_CAN1()` | `Can_HDX.c` | `CAN_ABOM=ENABLE`；软件 bus-off 状态机已删除；debug 只读 ESR BOFF | 是 | 是 | 间接 | 否 | 是 | CHANGE_NEEDED；用户已确认 |

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

## 9. BMS App IO 与 RTC 低功耗专项确认（2026-05-27）

状态：部分验证

专项文档：`docs/review/bms_app_io_low_power_compare_2026-05-27.md`

参考源码：

- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Can_HDX.c`

| Requirement ID | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 用户决策 |
|---|---|---|---|---|---|
| REQ-IO-RTC-001 | 正常模式 IO 映射必须保持现有硬件定义 | `conf_gpio.h`, `conf.c:InitIO()` | 相对基准 commit 未发现明显 IO 映射错配 | MUST_KEEP | 待确认 |
| REQ-IO-RTC-002 | RTC STOP 前应关闭 ADC/TIM2/DMA、CAN、LedBar、TIM3 并设置 GPIO 低漏电状态 | `ADC.c`, `conf.c:IOstatus_RTCMode()`, `Can_HDX.c`, `LedBar.c` | 已有 STOP 前关闭路径 | MUST_KEEP | 待确认 |
| REQ-IO-RTC-003 | `PA3 / 2737_EN` 在 RTC 模式下排除模拟输入 | `conf.c:IOstatus_RTCMode()` | GPIOA 模拟化时排除 PA3 | UNKNOWN | 待确认 |
| REQ-IO-RTC-004 | `PB14 / AFE1_CTL` 在 RTC 模式下排除模拟输入 | `conf.c:IOstatus_RTCMode()` | GPIOB 模拟化时排除 PB14 | UNKNOWN | 待确认 |
| REQ-IO-RTC-005 | `PB0 / AFE1_PRO_EN` 唤醒后是否需要显式恢复 | `conf.c:InitIO()`, `conf.c:InitIO_rtc()` | 正常初始化有 PB0，RTC 唤醒恢复未显式恢复 | UNKNOWN | 待确认 |
| REQ-IO-RTC-006 | RTC 唤醒后必须恢复 ADC、USART、CAN、TIM3、AFE I2C | `conf.c:InitRunAfterStopWakeup()` | 当前统一恢复这些外设 | MUST_KEEP | 待确认 |
| REQ-IO-RTC-007 | IWDG 开启时 RTC 唤醒周期不得超过 10 秒 | `RTC.c` | 当前限制为 10 秒 | CONFLICT | 待确认 |
| REQ-IO-RTC-008 | RTC 唤醒后不主动运行 CAN 周期广播 | `Can_HDX.c`, `rtc_sleep.c`, `RTC.c` | 当前删除 RTC wake CAN 服务；恢复后由运行态 `InitCan()` 打开 CMNT 并通信 | CHANGE_NEEDED | 已确认 |

## 10. 状态变量净删减专项确认（2026-06-02）

状态：部分验证

专项文档：`docs/review/state_variable_audit.md`

参考源码：

- `103 + 309/Project/Source/main.c`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep.h`
- `103 + 309/Project/Source/LogRecord.c`
- `103 + 309/Project/Source/System_Monitor.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/ProductionID.c`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/LogRecord.c`

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-SV-001 | LedBar 初始化应由固定启动顺序显式完成，而不是由每个 API 懒初始化兜底 | `AppInit.c:36-45`, `LedBar.c:171-177`, `LedBar.c:1034-1067`, `LedBar.c:1299-1368`, `conf/conf.c:273-349` | `LedBar_Init()` 已在启动运行态初始化阶段显式调用；`APP_LedBar()` 不再懒初始化；外部 API、STOP 前 GPIO、TIM4 ISR/debug 仍保留 `initialized` 防护 | 若误删 `initialized` 或在 RTC STOP 唤醒恢复中重复 `LedBar_Init()`，仍可能导致显示窗口、防抖状态或扫描状态被重置 | 已处理，保留安全保护 | 是否允许把 `LedBar_Init()` 显式加入启动流程，并逐步删除分散懒初始化？ | 已执行；本批不改 RTC 睡前/唤醒后恢复链，不把 `LedBar_Init()` 加入 `InitRunAfterStopWakeup()` | 已执行 |
| REQ-SV-002 | 低功耗提交流程应只保留一个清晰状态源，避免 `readyToSleep` 同时服务提交、LED、日志和 debug | `rtc_sleep.c`, `rtc_sleep.h`, `LedBar.c`, `LogRecord.c`, `Runtime.c`, `SystemDebug.c`, `tools/stlink_bms_monitor.ps1` | `readyToSleep` 字段和 `LowPower_IsToSleepPending()/LowPower_ClearToSleepFlag()` 已删除；`rtc_sleep()` 用本地 `sleep_mode` 执行 HICCUP/NORMAL/DEEP；LED/日志不再消费低功耗 ready；debug/ST-Link ready 由 `mode != NO_SLEEP` 派生 | 若提交点遗漏会丢 sleep SOC 或 BMS_SLEEP；本批保留 `RtcSleep_PortCommitResetSleep()`、`SleepDeal_Continue()`、`LowPowerSleep_SaveResetState()` 和 HICCUP STOP 准备链 | 已处理 | 是否允许把 `readyToSleep` 改成本地提交决策，并把 LED/日志收尾放入明确的 sleep commit 流程？ | 已执行；不改 HICCUP STOP、reset sleep、BMS_SLEEP 日志、sleep SOC 保存和外设恢复顺序 | 已执行 |
| REQ-SV-003 | 纯 debug/status 镜像字段不应混入控制状态结构 | `rtc_sleep.h:50-58`, `rtc_sleep.c:86-92`, `SystemDebug.c:536-541` | `g_stLowPowerRtcStatus` 同时保存控制状态和展示字段 | 继续混用会让维护者误以为展示字段参与低功耗控制 | KEEP_BUT_REFACTOR | 是否允许把只读展示字段迁移到 `SystemDebug` 或明确标记为 debug mirror？ | 同意先文档标记，源码阶段单独处理 | 待确认 |
| REQ-SV-004 | 产品信息初始化不应依赖主循环内的一次性 `static flag` | `ProductionID.c`, `ProductionID.h`, `AppInit.c`, `Runtime.c:57` | `InitProID()` 已收口到启动运行态初始化；`App_ProID_Deal()` 保留为空 hook，维持 `DBG_MODULE_PROID` heartbeat | 仍需上位机读 `0xC002` 48 个寄存器确认默认信息 | 已处理 | 是否允许把 `InitProID()` 放到启动初始化，主循环只保留真实后台处理？ | 已按低风险批次执行 | 已执行 |
| REQ-SV-005 | 按键、`MCU_WK`、SOC sample seq、AFE fault 计数等真实历史状态必须保留 | `LedBar.c:890-1009`, `SOC.c:116-142`, `DataDeal.c:825-917` | 这些状态用于防抖、边沿、去重积分、持续故障判断 | 误删会造成误唤醒、重复积分、故障恢复失效 | MUST_KEEP | 是否接受“不是所有状态变量都删，只删重复事实/残留阶段”的边界？ | 保留这些历史状态，只做命名和职责整理 | 待确认 |
| REQ-SV-006 | `DataDeal.c` 中客户逻辑状态必须先确认需求归属，不能直接按“变量多”删除 | `DataDeal.c:51-95`, `DataDeal.c:930-1055` | 充电器插拔、MOS 过温、UL 认证、RF_EN 熔断类逻辑混在 200ms 链路 | 直接删除可能改变安全输出和客户认证行为 | UNKNOWN | `charger_detect_and_keyLogi_200ms()` 和 `new_todo_logi()` 内这些状态是当前产品需求、认证需求，还是历史残留？ | 先列入需求确认，不进第一批删除 | 待确认 |
| REQ-SV-007 | 同一模块、同一生命周期、同一调试视角的私有运行态变量应优先收口到模块 runtime 结构体 | `FactoryAging.c:28-37`, `FactoryAging.c:45-627` | 老化模块原有 10 个文件级静态变量分别保存 state、elapsed、last tick、BKP/Flash 保存节流、finish retry 和 MOS mode | 替换错误会影响老化进度、完成保存或 MOS 模式缓存；本批次不改持久化格式和对外接口 | 已处理 | 是否允许对单文件私有运行态做结构体收口，提升 Keil Watch 可读性？ | 已按低风险结构体收口批次执行 | 已执行 |
| REQ-SV-008 | 日志模块私有运行态应集中管理，同时保留外部补偿时间符号 | `LogRecord.c`, `LogRecord.h`, `rtc_sleep_port.c` | 日志记录点、记录数组、startup/sleep 请求 flag、重复记录抑制和事件边沿 latch 已收口到 `LogRecordRuntime s_log_record`；`su32_Interval_S_Tcnt` 仍保留为外部符号 | 误搬 `su32_Interval_S_Tcnt` 会影响 RTC 睡眠秒数补偿；误清事件 latch 会影响日志去重 | 已处理 | 是否允许先收口私有状态，保留跨模块时间累计符号？ | 已执行；不改日志格式、Flash 保存格式和低功耗补偿接口 | 已执行 |
| REQ-SV-009 | AFE 电流零点运行态应集中管理，但不改变电流算法和 SOC 样本序号 | `DataDeal.c`, `DataDeal.h`, `SOC.c` | 启动零点、零点 offset、last raw、stable count、ready、zero state 和采样序号已收口到 `DATA_RUNTIME s_data`；外部通过 `AfeCurrent_GetSeq()` 读取 | 替换错误会影响电流方向、零点、自学习和 SOC 积分；本批次只做字段替换 | 已处理 | 是否允许对 AFE current zero 私有状态做结构体收口，保留算法和采样序号接口？ | 已执行；不改 CADC、换算公式、deadband、sample seq | 已执行 |

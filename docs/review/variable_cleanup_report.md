# 当前项目变量清理审查报告

> 状态：已完成源码分析与低风险清理执行闭环；初版报告为只读分析，后续已按“只处理明确低风险变量”的边界完成代码收口。
>
> 日期：2026-05-27
>
> 范围：以 `103 + 309/Project/Source` 业务源码为主，包含 `conf` 配置目录；不逐项审查 STM32 标准外设库和 EasyLogger 第三方库内部变量。`STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c` 仅作为项目中断入口参考。

## 1. 分析方法

本次没有依赖旧文档作为结论来源。旧的 `项目变量梳理.md` 只作为对照参考，因为其中部分行号和变量形态已经与当前源码不完全一致。

本次使用的证据线：

- 源码级 `rg` 引用统计：检查全局变量、`extern`、文件级 `static`、跨模块读写点。
- 本机只读语法检查：对当前业务 `.c` 文件运行 `clang -fsyntax-only -Wall -Wextra -Wunused-variable -Wunused-but-set-variable -Wunused-function`。
- Keil 工程配置对照：当前 Release 配置为 `STM32F10X_MD, USE_STDPERIPH_DRIVER`，`PROJECT_CFG_BUILD_PROFILE 0`，`PROJECT_CFG_AFE_TYPE 1`，即当前活动 AFE 路径是 `sh36xx`。

语法检查结果：

- 未发现编译器直接报告的 `unused variable`。
- 发现一个变量使用风险：`Sci_Upper.c:1008` 的局部变量 `i` 在 `Sci_ACK_0x03()` 内存在条件路径未初始化后被用于 CRC 的风险。
- 发现一个未使用函数：`Sci_Upper.c:464` 的 `Sci_ResetCalibCoefIndex()`。它不是变量，但说明该区域存在旧清理遗留。

## 2. 总体结论

当前变量问题不是简单的“大量未使用局部变量”，而是以下几类维护风险：

1. 有一批变量只在本文件使用，却以全局变量或 `extern` 暴露，后续可以优先 `static` 化或移除头文件声明。
2. ADC/Type-C、RTC wake、故障记录、SCI 端口运行态存在重复状态或镜像变量，短期能工作，但长期容易出现口径漂移。
3. 少量变量属于疑似遗留死变量：定义后只清零、只设置、不被消费，删除前需要确认是否被 Keil Watch、量产调试或协议保留使用。
4. `g_stCellInfoReport`、`OtherElement`、`SOC_Enhance_Element`、`g_st_SysTimeFlag`、`System_ErrFlag` 等变量是跨模块契约，不能按普通“全局变量清理”直接移动或改名。

## 3. 未使用或疑似遗留变量

说明：下表是源码引用统计发现的候选，不代表可以直接删除。涉及上位机协议、调试观察、Flash 参数和低功耗唤醒的变量必须先确认外部依赖。

| 变量 | 位置 | 当前源码使用情况 | 判断 | 建议 |
|---|---|---|---|---|
| `gu8_WakeUp_Type` | `rtc_sleep.c:15`, `rtc_sleep.h:37` | 仅定义和 `extern`，未见读写 | 疑似遗留 | 若确认无协议/调试依赖，删除变量和头文件声明 |
| `curr_offset` | `EEPROM.c:6`, `EEPROM.h:73` | 定义后在 `EEPROM_LoadDefaultRuntimeData()` 置 0，未见读取 | 疑似遗留 | 确认旧 EEPROM offset 逻辑已完全迁移到 Flash 后删除 |
| `OffsetValue_CHG` | `EEPROM.c:7`, `EEPROM.h:74` | 定义后置 0，未见读取 | 疑似遗留 | 同 `curr_offset` |
| `OffsetValue_DSG` | `EEPROM.c:8`, `EEPROM.h:75` | 定义后置 0，未见读取 | 疑似遗留 | 同 `curr_offset` |
| `CBC_Element` | `System_Init.c:6`, `System_Init.h:81` | 仅定义和 `extern`，未见业务访问 | 疑似遗留 | 确认 CBC 历史结构不再由协议或保护逻辑读取后删除 |
| `gu8_Reset_EventRecord` | `LogRecord.c:8`, `LogRecord.h:47` | 定义后在日志复位流程清 0，未见读取 | 疑似遗留或未完成入口 | 先确认上位机“清事件记录”是否应使用该标志 |
| `AFE_ResetFlag` | `SH367309_DataDeal.c:6`, `SH367309_DataDeal.h:249` | 被置 1，但未见读取 | 疑似遗留 | 确认 AFE 参数复位后是否需要触发 MCU/AFE 重启 |
| `g_u16IoutOffsetAD` | `ADC.c:13` | 在 `ADC_Current_Smooth()` 每次置 0，未见读取 | 死写候选 | 可以优先删除或改为局部调试量，需确认旧 Type-C 电流零点逻辑 |
| `g_u16TypeCBatEquivCurrent_mA` | `ADC.c:16`, `ADC.h:52` | `SOC.c` 计算后写入，未见后续读取；`A10` 版本用于返回 | 镜像冗余候选 | 若无调试显示需求，可删除 mA 镜像或通过 getter 暴露 |
| `FaultPoint_First` / `FaultPoint_Second` | `Fault.c:15-16`, `Fault.h:394-395` | 仅定义和 `extern`，未见读写 | 一级/二级旧故障指针遗留 | 删除前确认旧协议地址是否保留空洞 |
| `FaultWarnRecord()` | `Fault.c:26-28` | 空实现，不写 `FaultPoint_First/Second/Third` | 相关函数风险 | 不是变量，但解释了旧故障记录变量为何长期不更新 |

特别注意：`Fault_record_Third` 和 `FaultPoint_Third` 仍被 `Sci_Upper.c:762-763` 的读寄存器路径读取，但当前 `FaultWarnRecord()` 是空实现，未见写入该旧环形缓冲。它们不应简单按未使用删除，而应先确认协议上是否仍需要旧故障记录窗口；否则会把“读到空旧记录”的行为变成协议行为变化。

## 4. 重复变量和镜像状态

| 变量组 | 位置 | 重复形态 | 风险 | 建议 |
|---|---|---|---|---|
| Type-C 输出电流 | `ADC.c`, `ADC.h`, `SOC.c` | 原先多个全局/镜像同时表达 Type-C 电流；当前收口为 `s_adc.typec`，外部通过 `ADC_GetTypeCOutCurrentMilliAmp()` 读取 | 状态源已减少，后续只需避免重新增加等价镜像 | 保持 `ADC_GetTypeCOutCurrentMilliAmp()` 作为 mA 出口，SOC.c 负责折算电池侧等效放电电流 |
| Type-C 电池侧等效电流 | `SOC.c` | 原全局镜像已删除，`SOC_GetTypeCBatEquivCurrentA10()` 直接按 Type-C 输出功率、pack voltage 和效率计算 A10 | 不能把 Type-C 输出 mA 直接加到 SOC 放电 A10 | 保持当前直接计算返回，不新增 mA/A10 镜像状态 |
| RTC 唤醒状态 | `RTC.c:505`, `rtc_sleep.c:13-16`, `conf/conf.c:394-416`, `conf/conf.h:250-251` | `is_rtc_wakekup`、`is_wakeup`、`g_irq_t`、`gu8_WakeUp_Type`、`g_stLowPowerRtcStatus.rtcWake`、`sys_time.wakeup_rtc` 同时描述唤醒 | STOP 唤醒、RTC Alarm、运行期 sleep 状态容易不同步 | 短期不要动；中期合并为 `RTC_WAKE_CONTEXT`，只保留一个真相源，其他字段由快照生成 |
| 故障记录 | `Fault.c:9-21`, `Sci_Upper.c:762-812` | 旧 `Fault_record_Third/FaultPoint_Third` 与新 `Fault_record_First2/Second2/Third2` 并存 | 协议读旧窗口时可能读不到当前真实故障 | 先确认上位机使用哪个窗口，再决定迁移或保留兼容空洞 |
| SOC 对外发布 | `SocEnhance.c:130`, `SocEnhance.c:545-558`, `Sci_Upper.c:25` | `SOC_Enhance_Element` 是 SOC 内外桥，`g_stCellInfoReport.SocElement` 是通信上报快照 | 两套 SOC 字段必须按固定顺序同步，不能随意删一套 | 保持现状，后续只可通过明确的 publish 函数收口 |
| SCI 端口状态 | `Sci_Upper.c:4-27`, `Sci_Upper.c:105-139` | 每个端口有 `g_stCurrentMsgPtr_SCIx`、错误计数、TxEnable、TxFinishFlag，再由 `g_stSciPortx` 以指针组合 | 状态分散，新增端口时容易漏绑字段 | 后续可把这些字段内嵌到 `SCI_PORT_RUNTIME`，外部只保留必要 API |
| CAN 低功耗状态快照 | `Can_HDX.c:7-15`, `Can_HDX.c:64-95`, `Can_HDX.c:228-240` | 内部 `s_u8BusActive/s_u8TxQueueCount/...` 与 `g_stCanLowPowerStatus` 镜像 | 快照字段可能滞后于内部状态 | 保留快照作为诊断出口，但应明确刷新点，不要让外部直接改 |
| AFE 短路保护表 | `SH367309_DataDeal.c:10/12`, `ShortFunc.c:4-5` | 当前 `PROJECT_CFG_AFE_TYPE=1` 时 `ShortFunc.c` 的 bq 表不编译；若切到 bq 且仍编译 SH367309 文件，会出现同名 `AFE_SCV/AFE_SCT` 风险 | 跨 AFE 配置时可能链接冲突或误用表 | 后续改成 `s_bq_afe_scv/s_bq_afe_sct` 与 `s_sh_afe_scv/s_sh_afe_sct`，不要复用同名全局 |

## 5. 不必要全局变量和可 `static` 化候选

下表优先级高于“结构性重构”，因为它们大多只改链接可见性，不改变运行逻辑。但仍需逐项编译验证。

| 变量 | 位置 | 当前跨文件使用 | 建议 |
|---|---|---|---|
| `g_u16ADCValFilter` | `ADC.c:3` | 无 C 文件外部读取；DMA 地址在 `ADC.c` 内配置 | 可改为 `static __IO`，保持类型和位宽不变 |
| `g_u32ADCValFilter2` | `ADC.c:5` | 仅 `ADC.c` 使用 | 可改为 `static`；类型 `INT32` 不能随便改 |
| `g_u16TypeCOutCurrent_A10` | `ADC.c:15`, `ADC.h:51` | 原先仅 `ADC.c` 写和读 | 已删除，改为 `ADC_Current_Smooth()` 内局部 `typec_current_A10` |
| `g_u16TypeCOutOffsetAD` | `ADC.c:18`, `ADC.h:54` | 仅 `ADC.c` 使用 | 可 `static`，或与零点逻辑一起删除 |
| `g_u16TypeCOutStableAD` | `ADC.c:19`, `ADC.h:55` | 原先仅写不读 | 已删除 |
| `g_u16TypeCOutDelta_mV` | `ADC.c:20`, `ADC.h:56` | 原先仅作为同函数换算中间量 | 已局部化 |
| `g_u16VbcStableAD` | `ADC.c:21`, `ADC.h:57` | 原先仅写不读 | 已删除 |
| `g_u16VbcAdc_mV` | `ADC.c:22`, `ADC.h:58` | 原先仅作为同函数换算中间量 | 已局部化 |
| `gu16_BusCurr_CHG` / `gu16_BusCurr_DSG` | `ADC.c:24-25`, `ADC.h:60-61` | 原先仅写不读 | 已删除 |
| `iSheldTemp_10K` | `ADC.c:28` | 仅 `ADC.c` 使用 | 可改为 `static const` |
| `g_u16BusOff_InitTestCnt` / `g_u16BusOff_RecoverCnt` | `Can_HDX.c:14-15` | 仅 `Can_HDX.c` 使用 | 可 `static`，更好是并入 CAN runtime/status |
| `u8IICFaultcnt1/2`, `u8WakeCnt1/2` | `DataDeal.c:3-6` | 仅 `DataDeal.c` 使用 | 可 `static`，后续合并为 AFE channel runtime 数组 |
| `g_stAfeCurrentObserve` | `DataDeal.c:47`, `DataDeal.h:252` | 当前无外部 C 文件读取 | 如果只是调试观察，可受 `PROJECT_CFG_DEBUG_WATCH_ENABLE` 控制或提供只读 getter |
| `iSheldTemp_10K_AFE` | `I2C_AFE1.c:7` | 仅 `I2C_AFE1.c` 使用 | 可改为 `static const` |
| `CRC8Table` | `I2C_AFE1.c:69` | 仅 `I2C_AFE1.c` 使用 | 可改为 `static const` |
| `key_release_wakeup` | `LedBar.c:22` | 仅 `LedBar.c` 使用 | 可改为 `static` |
| `BMS_LOG_POINT` / `BMS_LOG_RECORD` | `LogRecord.c:5-6` | 仅 `LogRecord.c` 使用 | 可改为 `static`，对外通过日志 API 访问 |
| `TimeDisplay` | `RTC.c:3` | 仅 `RTC.c` 使用 | 可改为 `static __IO`，但必须确认 RTC IRQ handler 同文件 |
| `Systmtime` | `RTC.c:7` | 仅 `RTC.c` 用作默认时间 | 可 `static`，并考虑是否 `const` |
| `month_days` | `RTC.c:9` | 仅 `RTC.c` 宏 `Days_in_month()` 使用 | 可 `static const` |
| `g_u8SCITxBuff` | `Sci_Upper.c:23` | 仅 `Sci_Upper.c` 使用 | 可 `static`，不应继续作为裸全局 |
| `g_stCurrentMsgPtr_SCI1/2/3` | `Sci_Upper.c:4/10/17`, `Sci_Upper.h:516-521` | 当前未见其他 C 文件直接使用 | 可先去掉无用 `extern`，再评估是否内嵌到 `SCI_PORT_RUNTIME` |
| `gu8_TxEnable_SCI1/2/3` | `Sci_Upper.c:6/12/19`, `Sci_Upper.h:508-513` | 当前未见其他 C 文件直接使用 | 若 RS485 方向控制只在 `Sci_Upper.c` 内完成，可隐藏 |
| `AFE_Parameters_RS485_Struction` | `SH367309_DataDeal.c:17`, `SH367309_DataDeal.h:250` | 当前未见其他 C 文件直接访问 | 可隐藏，协议读写通过函数完成更稳 |
| `AFE_OCD2V` | `SH367309_DataDeal.c:9` | 仅 `SH367309_DataDeal.c` 使用 | 可 `static const` |
| `g_irq_t` | `rtc_sleep.c:13`, `rtc_sleep.h:25` | 当前只在 `rtc_sleep.c` 使用 | 可隐藏，外部通过 wake source API 查询 |
| `is_wakeup` | `rtc_sleep.c:14`, `conf/conf.c:394` | 只被置位，未见读取 | 疑似可删除；若保留，应明确读取方 |
| `g_stLowPowerRtcStatus` | `rtc_sleep.c:16`, `rtc_sleep.h:61` | 当前只在 `rtc_sleep.c` 内更新和读取 | 若无协议/调试读取，可隐藏或改为只读快照 API |

## 6. 可以局部化或删除的变量

| 变量 | 当前形态 | 局部化方向 |
|---|---|---|
| `g_u16IoutOffsetAD` | 全局变量，每次 `ADC_Current_Smooth()` 置 0，无读取 | 删除，或改成 `ADC_Current_Smooth()` 内局部变量；如果未来恢复零点校准，则并入 `ADC_TypeCState` |
| `curr_offset` / `OffsetValue_CHG` / `OffsetValue_DSG` | EEPROM 迁移后仍保留的全局变量 | 若不再保存外部 EEPROM offset，直接删除；若仍要显示旧偏移，放入存储迁移上下文结构 |
| `gu8_Reset_EventRecord` | 日志复位标志只清零不消费 | 若上位机复位日志已经由函数直接完成，应删除；若需要异步复位，则补齐消费点 |
| `AFE_ResetFlag` | AFE 参数复位后置 1 但未消费 | 若需要复位 AFE，应补消费点；否则删除 |
| `is_wakeup` | STOP 恢复时置 `true`，未见读取 | 删除或并入统一 wake context |
| `g_u16TypeCBatEquivCurrent_mA` | 只作为计算中间结果保存 | 改为 `SOC_GetTypeCBatEquivCurrentA10()` 内局部变量，保留返回值即可 |

## 7. 可以合并到结构体的变量

| 建议结构体 | 可合并变量 | 目的 |
|---|---|---|
| `ADC_Runtime_t` | `g_u16ADCValFilter`, `g_u32ADCValFilter2`, `g_i32ADCResult`, Type-C current 相关变量、VBC 中间变量 | 把 ADC 原始值、滤波值、物理量和兼容镜像集中管理 |
| `AFE_MonitorRuntime_t` | `u8IICFaultcnt1/2`, `u8WakeCnt1/2` | 用数组按 AFE channel 管理，避免双 AFE 复制变量 |
| `SCI_PORT_RUNTIME` 内嵌状态 | `g_stCurrentMsgPtr_SCIx`, `gu16_CommuErrCnt_SCIx`, `gu8_TxEnable_SCIx`, `gu8_TxFinishFlag_SCIx` | 当前已经用结构体指针组合，下一步可把实际存储也内嵌 |
| `CAN_Runtime_t` | Tx 队列、App cmd 队列、read block 状态、write pending 状态 | 当前 `Can_HDX.c` 文件级状态仍较多，后续只在确有收益时继续收口 |
| `RTC_WakeContext_t` | `is_rtc_wakekup`, `g_irq_t`, `g_stLowPowerRtcStatus.rtcWake`, `sys_time.wakeup_rtc`, `sys_time.wakeup_reason` | 建立 RTC/EXTI/STOP 唤醒唯一真相源 |
| `FaultRecordRuntime_t` | `Fault_record_*`, `FaultPoint_*`, `Fault_record_*2`, `FaultPoint_*2` | 明确旧记录窗口和当前记录窗口，避免协议读错 |
| `LogRecordRuntime_t` | `BMS_LOG_POINT`, `BMS_LOG_RECORD`, `s_log_record_flag`, 日志保存节流状态 | 日志模块内部状态收口，方便持久化和复位 |

## 8. 高风险变量，不能随便动

| 变量 | 风险原因 | 修改前必须确认 |
|---|---|---|
| `g_stCellInfoReport` | RS485 `0xD000` 实时窗口、CAN 周期帧、SOC 发布、LedBar、日志、保护状态都读取它 | 协议字段顺序、单位、更新时序、CAN 上报是否兼容 |
| `OtherElement` | 可写参数集合，影响串数、采样电阻、SOC 参数、低功耗阈值、均衡参数 | Flash 持久化布局、上位机寄存器地址、默认值和范围 |
| `PRT_E2ROMParas` | 保护阈值参数集合 | 上位机保护参数读写、保护动作、Flash RW_PARAM 数据布局 |
| `g_u16CalibCoefK` / `g_i16CalibCoefB` | ADC/AFE/温度校准参数，协议可写、Flash 保存 | 校准索引、默认值、写入地址、量产校准流程 |
| `g_u32CS_Res_AFE` | AFE 电流换算核心参数，由 `OtherElement` 派生 | 电流方向、mA/A10 单位、SOC 积分、保护阈值 |
| `AfeCurrent_GetSeq()` / `s_data.afeSeq` | SOC 通过它判断 AFE 电流样本是否更新 | 200ms 采样节拍、SOC 是否漏算或重复积分 |
| `SOC_Enhance_Element` | SOC 模块对外参数和运行结果桥接结构 | SOC 单步校准、显示 SOC、RTC 补偿、上位机一次设 SOC |
| `SOC_Table_Set` / `SOC_Table_Default` / `SOC_Table_LiFePO` / `SocTable_TernaryLi` / `SocTable_LiFePO2` | OCV 表与上位机 SOC 表、化学体系选择相关；`SOC_Table_Set/Default` 只在 runtime table 宏路径下有引用，当前量产宏关闭 | 当前 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE`、编译期化学体系选择、协议写表策略；若重新启用 runtime table，先补齐定义并编译验证 |
| `g_st_SysTimeFlag` | TIM3 10ms tick 锁存后驱动主循环任务 | 中断-主循环同步、任务周期、低功耗恢复 |
| `System_ErrFlag` | 系统错误位集合，被保护、通信、上位机读取 | 错误位序、保留位、旧 Heat/Cool 清理后的 reserved 兼容 |
| `SH367309_Reg_Store` / `Registers_AFE1` / `SH367309_Read_AFE1` | AFE 寄存器和采样值镜像 | AFE 通信、故障解析、温度/电压/电流装载 |
| `AFE_ROM_PARAMETERS_Struction` | SH367309 MTP/ROM 参数打包结构，`ShortFunc.c` 通过函数内 `extern` 使用 | AFE 写参数流程、短路保护档位、MTP 写入顺序 |
| `u8FlashUpdateFlag` / `u8FlashUpdateE2PROM` | 参数写入后落库、低功耗阻断和 IAP 相关 | 上位机写参后保存时机、低功耗期间是否允许睡眠 |
| `is_rtc_wakekup` / `RTC_ExtComCnt` / `sys_time` | RTC 唤醒、通信活动、低功耗策略共同使用 | STOP 唤醒恢复、RTC Alarm 清除、CAN/SCI 活动阻断 |
| `ProductionInfor` | BMS 序列号、硬件版本、软件版本来源，`0xC002` 读取依赖 | 48 个寄存器布局、上位机显示兼容 |

## 9. 建议清理顺序

### 9.1 第一阶段：低风险可见性收口

优先处理只在本文件使用且不影响协议布局的变量：

- `iSheldTemp_10K`, `iSheldTemp_10K_AFE`, `CRC8Table`, `month_days` 改为 `static const`。
- `key_release_wakeup`, `BMS_LOG_POINT`, `BMS_LOG_RECORD`, `g_u8SCITxBuff` 改为 `static`。
- `g_u16BusOff_InitTestCnt`, `g_u16BusOff_RecoverCnt` 改为 `static` 或并入 CAN runtime。

验证重点：编译通过，协议输出不变。

### 9.2 第二阶段：删除疑似死变量

需要先确认外部依赖：

- `gu8_WakeUp_Type`
- `curr_offset`, `OffsetValue_CHG`, `OffsetValue_DSG`
- `CBC_Element`
- `gu8_Reset_EventRecord`
- `AFE_ResetFlag`
- `g_u16IoutOffsetAD`
- `FaultPoint_First`, `FaultPoint_Second`

验证重点：`tools/project_check.py`、上位机读写关键寄存器、事件记录复位、低功耗唤醒。

### 9.3 第三阶段：结构体收口

这一阶段会改变较多模块内部形态，不建议与功能 bug 修复混在同一提交：

- ADC/Type-C 状态合并。
- SCI 端口 runtime 内嵌。
- RTC wake context 合并。
- Fault record 旧/新窗口裁决。
- CAN runtime 继续收口。

验证重点：主循环 200ms 节拍、SOC 积分、RS485 读写、CAN 周期帧、RTC STOP 唤醒、IAP 安全入口。

## 10. 当前不建议立刻动的点

- 不要直接删除 `g_stCellInfoReport` 或把它拆字段，协议和 CAN 都依赖它。
- 不要直接把 `OtherElement` 拆成多个小结构体，Flash 布局和上位机地址会受影响。
- 不要直接移除 `SOC_Enhance_Element`，它是旧接口与新 SOC 核心之间的兼容桥。
- 不要只因为 `g_stCanLowPowerStatus` 当前外部 C 代码没读取就删除，它可能是预留诊断快照；应先确认是否计划映射到上位机或 Keil Watch。
- 不要在同一轮里同时做 `static` 化、删除变量、协议窗口调整和结构体重构；变量清理容易引入“编译能过但协议口径变了”的问题。

## 11. 后续验证建议

如果后续进入代码修改阶段，建议每个小批次至少执行：

```bash
python3 tools/project_check.py
python3 tools/soc_replay_test.py
python3 tools/run_soc_host_c_test.py
git diff --check
```

涉及编译可见性变化时，再补充当前使用过的只读语法检查：

```bash
find '103 + 309/Project/Source' -maxdepth 2 -type f -name '*.c' ! -path '*/easylogger/*' -print0 \
  | sort -z \
  | while IFS= read -r -d '' file; do
      clang -std=gnu99 -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER \
        -I'103 + 309/Project/Lib' \
        -I'103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers' \
        -I'103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc' \
        -I'103 + 309/Project/Source' \
        -I'103 + 309/Project/Source/conf' \
        -I'103 + 309/Project/Source/easylogger/inc' \
        -Wall -Wextra -Wunused-variable -Wunused-but-set-variable -Wunused-function \
        -Wno-pointer-to-int-cast -Wno-unused-parameter -Wno-missing-braces \
        -Wno-constant-conversion -Wno-tautological-constant-out-of-range-compare \
        -fsyntax-only "$file"
    done
```

## 12. 本轮低风险清理执行记录

> 状态：已按“只清理明确低风险变量”的边界完成第二轮收口。处理原则是：不改协议寄存器和结构体布局，不改表值，不改 Flash 存储格式，不改保护阈值语义；对 `__IO` / `volatile` 变量仅在确认所有读写都在同一 `.c` 文件内后收窄链接可见性，保留原类型和读写路径。
>
> 日期：2026-05-27

### 12.1 已清理项

| 变量 | 文件 | 本轮处理 | 风险说明 |
|---|---|---|---|
| `BMS_LOG_POINT` / `BMS_LOG_RECORD` | `103 + 309/Project/Source/LogRecord.c` | 已改为 `static`，删除头文件旧注释 extern | 日志仍通过 `StorageFlash_SaveLogData()` / `StorageFlash_LoadLogData()` 读写，未改存储格式和 API |
| `gu8_Reset_EventRecord` | `103 + 309/Project/Source/LogRecord.c/.h` | 已删除定义、extern 和唯一清零点 | 仅写不读，未参与事件记录复位行为 |
| `g_u8SCITxBuff` | `103 + 309/Project/Source/Sci_Upper.c` | 已改为 `static` | 仅 SCI 模块内部组包使用，不改变帧格式 |
| `g_stCurrentMsgPtr_SCI1/2/3` | `103 + 309/Project/Source/Sci_Upper.c/.h` | 已改为 `static` 并删除 extern | 端口绑定仍在 `Sci_Upper.c` 内完成，外部无直接引用 |
| `gu16_CommuErrCnt_SCI1/2/3`、`gu8_TxEnable_SCI1/2/3`、`gu8_TxFinishFlag_SCI1/2/3` | `103 + 309/Project/Source/Sci_Upper.c/.h` | 已改为 `static`，删除无外部引用的 extern | 仅收窄链接可见性，不改变 RS485 方向控制逻辑 |
| `Sci_ResetCalibCoefIndex()` | `103 + 309/Project/Source/Sci_Upper.c` | 已删除未使用静态函数 | 不是变量；同步清理 `clang` 报出的未使用遗留函数 |
| `Sci_ACK_0x03()` 局部 `i` | `103 + 309/Project/Source/Sci_Upper.c` | 已初始化为 `0U` | 修复条件路径未初始化后参与 CRC 的风险 |
| `TimeDisplay` | `103 + 309/Project/Source/RTC.c` | 已改为 `static __IO` | `RTC_IRQHandler()` 与消费点均在同文件，保留 `__IO` 语义 |
| `g_u16ADCValFilter` | `103 + 309/Project/Source/ADC.c` | 已改为 `static __IO` | DMA/滤波读写均在 `ADC.c` 内，保留 `__IO` 和位宽 |
| `g_u32ADCValFilter2` | `103 + 309/Project/Source/ADC.c` | 已改为 `static` | 仅 ADC 内部缓存，未改 `INT32` 类型 |
| `g_u16IoutOffsetAD`、`g_u16TypeCOutOffsetAD` | `103 + 309/Project/Source/ADC.c/.h` | 已删除 | 原逻辑每次置 0 且无读取 |
| `g_u16TypeCOutCurrent_mA`、`g_u16TypeCOutCurrent_A10` | `103 + 309/Project/Source/ADC.c/.h` | 已删除全局镜像；当前 Type-C 输出电流保存在 `s_adc.typec`，通过 `ADC_GetTypeCOutCurrentMilliAmp()` 读取 | SOC 通过 getter 读取 mA 值，避免裸全局暴露和重复状态源 |
| `g_u16TypeCBatEquivCurrent_mA`、`g_u16TypeCBatEquivCurrent_A10` | `103 + 309/Project/Source/ADC.c/.h`, `SOC.c` | 已删除全局镜像，`SOC_GetTypeCBatEquivCurrentA10()` 直接返回计算结果 | 保留 Type-C 输出电流换算为电池侧等效电流的行为 |
| `g_u16TypeCOutStableAD`、`g_u16TypeCOutDelta_mV`、`g_u16VbcStableAD`、`g_u16VbcAdc_mV` | `103 + 309/Project/Source/ADC.c/.h` | 已删除或局部化 | 仅 ADC 内部换算中间态，不再保留跨调用镜像 |
| `gu16_BusCurr_CHG` / `gu16_BusCurr_DSG` | `103 + 309/Project/Source/ADC.c/.h` | 已删除 | legacy mirror 无读取方；ADC 对外读取统一走 `ADC_GetResult()` 和专用 getter |
| `u8IICFaultcnt1/2`、`u8WakeCnt1/2` | `103 + 309/Project/Source/DataDeal.c` | 已改为 `static` | 仅 AFE monitor 内部计数 |
| `AFE_ResetFlag` | `103 + 309/Project/Source/SH367309_DataDeal.c/.h` | 已删除定义、extern 和唯一赋值 | 仅写不读，未参与 AFE reset 后续流程 |
| `AFE_Parameters_RS485_Struction` | `103 + 309/Project/Source/SH367309_DataDeal.c/.h` | 已改为 `static`，删除 extern | 参数读写仍通过本文件函数完成，未改结构体内容 |
| `AFE_OCD1V_OCCV`、`AFE_OCD2V`、`AFE_OVT_UVT`、`AFE_OCD1T`、`AFE_OCCT_OCD2T` | `103 + 309/Project/Source/SH367309_DataDeal.c` | 已改为文件内 `static const` 并统一命名 | 只读 AFE 档位表，未改表值 |
| `AFE_SCV` / `AFE_SCT` | `SH367309_DataDeal.c`, `ShortFunc.c` | 已拆成 `g_u16ShAfeScvTable` / `g_u16ShAfeSctTable` 与 `s_bq_afe_scv` / `s_bq_afe_sct` | 避免不同 AFE 配置下同名表冲突；SH 表仍供 `ShortFunc.c` 只读使用 |
| `g_irq_t` | `103 + 309/Project/Source/rtc_sleep.c/.h` | 已改为 `static` 并删除 extern | 仅低功耗模块内部使用，未改 wake source 枚举语义 |
| `g_stLowPowerRtcStatus` | `103 + 309/Project/Source/rtc_sleep.c/.h` | 已改为 `static volatile` 并删除 extern | 保留 `volatile`，所有读写在 `rtc_sleep.c` 内 |
| `is_wakeup`、`gu8_WakeUp_Type` | `rtc_sleep.c/.h`, `conf/conf.c` | 已删除变量、extern 和唯一置位 | 仅写不读，STOP 恢复仍走 `InitRunAfterStopWakeup()` |
| `curr_offset`、`OffsetValue_CHG`、`OffsetValue_DSG` | `103 + 309/Project/Source/EEPROM.c/.h` | 已删除定义、extern 和默认清零 | 仅写不读，不影响 Flash 默认参数装载 |
| `CBC_Element` | `103 + 309/Project/Source/System_Init.c/.h` | 已删除定义和 extern | 仅定义未使用 |
| `FaultPoint_First` / `FaultPoint_Second` | `103 + 309/Project/Source/Fault.c/.h` | 已删除定义和 extern | 仅旧一级/二级指针遗留，无读写 |
| `FaultWarnRecord()` | `103 + 309/Project/Source/Fault.c` | 已删除空实现和未使用声明 | 保留仍被协议读取的旧 `Fault_record_Third/FaultPoint_Third` |
| `tools/project_check.py` | `tools/project_check.py` | Type-C SOC 门禁已支持 getter 路径 | 检查行为仍是“输出电流必须先换算为电池侧等效电流” |
| `tools/soc_host_c_test.c` | `tools/soc_host_c_test.c` | host 测试补 `ADC_GetTypeCOutCurrentMilliAmp()`、`ADC_GetVbatMilliVolt()`、`AfeCurrent_GetSeq()` 桩函数，测试侧假变量改为 `s_host_*` | 测试继续验证 Type-C 电流换算、Vbat fallback 和新样本触发 |
| `tools/soc_host_visual_trace.c` | `tools/soc_host_visual_trace.c` | 可视化 trace 工具补 `ADC_GetTypeCOutCurrentMilliAmp()`、`ADC_GetVbatMilliVolt()`、`AfeCurrent_GetSeq()` 桩函数，测试侧假变量改为 `s_host_*` | 保持 host trace 工具与 SOC/ADC 新接口一致 |

### 12.2 已保留项

以下变量仍按高风险边界保留，不在本轮继续动：

- `g_stCellInfoReport`、`OtherElement`、`PRT_E2ROMParas`：协议、Flash、保护和上位机可见核心结构。
- `g_u16CalibCoefK` / `g_i16CalibCoefB`、`g_u32CS_Res_AFE`：校准、采样电阻和电流换算核心参数。
- `AfeCurrent_GetSeq()` / `s_data.afeSeq`、`SOC_Enhance_Element`、SOC OCV 表：SOC 积分、OCV 和运行时桥接核心状态。
- `g_st_SysTimeFlag`、`System_ErrFlag`：调度和错误位跨模块契约。
- `SH367309_Reg_Store` / `Registers_AFE1` / `SH367309_Read_AFE1`、`AFE_ROM_PARAMETERS_Struction`：AFE 寄存器镜像和 MTP 参数打包结构。
- `u8FlashUpdateFlag` / `u8FlashUpdateE2PROM`：上位机写参后落库和低功耗阻断状态。
- `is_rtc_wakekup` / `RTC_ExtComCnt` / `sys_time`：RTC 唤醒和低功耗策略共享状态。
- `ProductionInfor`：`0xC002` 产品信息读取来源。
- `Fault_record_Third` / `FaultPoint_Third`：旧故障记录窗口仍被 `Sci_Upper.c` 读取，不能按未使用变量删除。
- `g_stAfeCurrentObserve`：受 `PROJECT_CFG_DEBUG_WATCH_ENABLE` 保护，保留为调试观察出口。

### 12.3 本轮验证结果

最新 SOC 专项验证结果以 `docs/design/soc_design.md` 和 `docs/review/test_plan.md` 为准；本文件只保留变量清理脉络，避免重复维护测试结论。

## 13. 剩余建议执行裁决

> 状态：当前报告中的“低风险可见性收口、只写不读变量删除、明显重复临时/镜像变量删除”已经执行完毕。CAN runtime 已作为单独小专项完成内部状态收口；剩余建议是协议/Flash/SOC/AFE/低功耗边界更强的结构化重构，不应继续混入本次 CAN 清理提交。

### 13.1 已完成的原建议

| 原建议 | 当前裁决 |
|---|---|
| `BMS_LOG_POINT` / `BMS_LOG_RECORD` 改为 `static` | 已执行 |
| `g_u8SCITxBuff` 改为 `static` | 已执行 |
| `g_stCurrentMsgPtr_SCIx`、`gu8_TxEnable_SCIx` 去 `extern` | 已执行 |
| `TimeDisplay` 改为 `static __IO` | 已执行，保留 `__IO` |
| `gu8_WakeUp_Type`、`is_wakeup` 删除 | 已执行 |
| `curr_offset` / `OffsetValue_CHG` / `OffsetValue_DSG` 删除 | 已执行 |
| `CBC_Element` 删除 | 已执行 |
| `gu8_Reset_EventRecord` 删除 | 已执行 |
| `AFE_ResetFlag` 删除 | 已执行 |
| `g_u16IoutOffsetAD` 删除 | 已执行 |
| `FaultPoint_First` / `FaultPoint_Second` 删除 | 已执行 |
| ADC 内部缓存和 Type-C/VBC 中间变量可见性收口 | 已执行，SOC 通过 getter 读取 Type-C 输出电流 |
| `u8IICFaultcnt1/2`、`u8WakeCnt1/2` 可见性收口 | 已执行 |
| `AFE_Parameters_RS485_Struction` 可见性收口 | 已执行 |
| `g_irq_t`、`g_stLowPowerRtcStatus` 可见性收口 | 已执行，保留 `volatile` |
| AFE/BQ 短路保护表同名全局收口 | 已执行 |
| `CAN_Runtime_t` / CAN 文件级状态收口 | 已执行，`Can_HDX.c` 内部合并为 `s_tx`、`s_runtime`、`s_app`，同时删除 header 遗留未使用宏和旧 union |

### 13.2 不继续执行的结构性建议

| 建议结构体 | 当前裁决 | 原因 |
|---|---|---|
| `ADC_Runtime_t` | 暂不执行 | 会继续触碰 ADC 原始值、Type-C 电流、SOC 输入和 legacy mirror，需要单独做采样/SOC 回归 |
| `AFE_MonitorRuntime_t` | 暂不执行 | 会改变双 AFE channel 错误计数和唤醒计数组织方式 |
| `SCI_PORT_RUNTIME` 内嵌状态 | 暂不执行 | 属于通信端口运行态重构，需要覆盖 RS485 多端口、方向控制和收发缓冲验证 |
| `RTC_WakeContext_t` | 暂不执行 | RTC/EXTI/STOP 唤醒唯一真相源重构，属于低功耗关键链路 |
| `FaultRecordRuntime_t` | 暂不执行 | 故障记录新旧窗口涉及协议兼容，需要确认上位机当前读取窗口 |
| `LogRecordRuntime_t` | 暂不执行 | 日志记录与 Flash 持久化相关，需确认存储布局和上位机读取行为 |

### 13.3 当前完成定义

截至本轮，文档内所有可以在不改变协议布局、Flash 布局、表值、保护阈值和业务时序的前提下完成的变量清理建议已经执行。后续若继续推进，应另起专项：

- `ADC_Runtime_t` 专项：只处理 ADC/Type-C 状态结构化，并配套 SOC/Type-C 回归。
- `SCI_PORT_RUNTIME` 专项：只处理 SCI 端口状态内嵌，并配套 RS485 读写回归。
- `RTC_WakeContext_t` 专项：只处理低功耗唤醒真相源，并配套 STOP/RTC/CAN 唤醒回归。
- `FaultRecordRuntime_t` 专项：先确认上位机读取旧/新故障窗口，再决定迁移或保留兼容空洞。

# 项目文档盘点

> 文档状态：CURRENT
> 源码验证：PARTIAL
> 主要参考源码：`main.c`, `Runtime.c`, `AppInit.c`, `DataDeal.c`, `SOC.c`, `SocEnhance.c`, `ADC.c`, `I2C_AFE1.c`, `SH367309_Func.c`, `SH367309_DataDeal.c`, `Sci_Upper.c`, `Can_HDX.c`, `CanFeidaoFrames.c`, `Flash.c`, `EEPROM.c`, `RTC.c`, `rtc_sleep.c`, `app_lowpower.c`, `LedBar.c`, `FactoryAging.c`
> 最后更新时间：2026-05-26
> 未确认事项：旧根目录文档尚未移动或删除；`build/`、`.probes/`、第三方 demo 和 CMake 生成文本按“生成/外部参考”处理，不纳入权威文档。

## 1. 扫描统计

| 范围 | 数量 | 处理策略 |
|---|---:|---|
| 全仓库 Markdown/TXT/PDF/DOC/DOCX 类文件 | 238 | 全量扫描，区分维护文档、生成文件、外部参考 |
| `docs/` 下现有文档 | 52 | 本轮重点盘点和一致性核查 |
| 根目录 + `103 + 309/Project` 项目文档 | 151 | 作为历史方案、测试记录和旧 review 归类，不直接信任 |
| `build/`, `.probes/`, demo code, CMake 缓存, license/log | 87 | 视为生成/外部/工具资料，不合并到权威 docs |

## 2. `docs/` 现有文档盘点

| 文档路径 | 文档名称 | 类型 | 主要内容 | 是否重复 | 是否可能过期 | 是否需要源码核对 | 建议处理方式 |
|---|---|---|---|---|---|---|---|
| `docs/BMS_BOOT_CONTROL_REFACTOR_2026-05-23.md` | IAP 启动与升级可靠性重构 | BOOTLOADER_IAP | App->IAP mailbox、地址规则、防断电 | 是 | 部分 | 是 | 合并到 `docs/design/bootloader_iap_design.md`，保留索引 |
| `docs/BMS_CAN_IAP_PROTOCOL.md` | CAN-IAP 升级协议 | BOOTLOADER_IAP | IAP CAN 升级帧、状态、断电恢复 | 是 | 未完全核对 IAP 工程 | 是 | 暂保留为 comm tool/IAP 参考，主 App 文档只引用 |
| `docs/BMS_CAN_IAP_RELIABILITY_STATUS_2026-05-22.md` | CAN-IAP 可靠性记录 | CHANGE_LOG | 实施记录和未测项 | 是 | 可能 | 是 | 合并摘要到 changelog，建议归档 |
| `docs/BMS_CAN_MODULE_REFACTOR_2026-05-23.md` | BMS App CAN 模块重构说明 | CAN_MODBUS | CAN 队列、过滤、低功耗状态 | 是 | 部分 | 是 | 合并源码已验证部分到 `protocol_design.md` |
| `docs/BMS_CAN_SERVICE_PROTOCOL.md` | BMS App CAN 服务协议 | CAN_MODBUS | `0x60/0x61` 服务、读写寄存器、老化 | 是 | 基本当前 | 是 | 作为 CAN 权威来源之一，合并到 `protocol_design.md` |
| `docs/BMS_COMM_TOOL_READ_WRITE_2026-05-23.md` | comm tool 读写 BMS | PROTOCOL | PC/comm tool 到 BMS 读写 | 是 | 部分 | 是 | 保留为上位机参考，固件行为合并到协议设计 |
| `docs/BMS_SERIAL_IAP_REFACTOR_2026-05-22.md` | 串口 IAP 重构记录 | BOOTLOADER_IAP | 串口 IAP 侧逻辑 | 是 | 未核 IAP 工程 | 是 | 归入 IAP 参考区，不作为 BMS App 当前主文档 |
| `docs/CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md` | CAN 老化和 SOC 控制 | CAN_MODBUS | 老化控制、SOC 写入、上位机入口 | 是 | 基本当前 | 是 | 合并到 `protocol_design.md` 与 `project_overview.md` |
| `docs/CAN_HEARTBEAT_DIAG_2026-05-23.md` | CAN 心跳诊断 | TEST | comm tool/IAP 心跳诊断 | 是 | 未核工具工程 | 是 | 保留为工具诊断文档 |
| `docs/COMM_ARCH_OPTIMIZATION_2026-05-26.md` | 通讯架构优化记录 | ARCHITECTURE | PC/comm tool/BMS 三端优化 | 是 | 当前但偏记录 | 是 | 合并结论到 `protocol_design.md`，记录进 changelog |
| `docs/COMM_TOOL_CAN_IAP_ARCHITECTURE_2026-05-22.md` | comm tool CAN-IAP 方案 | BOOTLOADER_IAP | 工具固件和 IAP 系统方案 | 是 | 部分 | 是 | 保留为工具/IAP 参考 |
| `docs/COMM_TOOL_F103RET6_KEIL_PORT_2026-05-23.md` | comm tool Keil 工程 | PROJECT_OVERVIEW | 独立 comm tool 工程说明 | 否 | 未核 | 否 | 暂保留，不并入 BMS App 权威文档 |
| `docs/COMM_TOOL_RELIABILITY_TEST_2026-05-23.md` | comm tool 可靠性测试 | TEST | READ_BLOCK、升级可靠性 | 是 | 部分 | 是 | 合并测试方法到 `docs/test/test_plan.md` |
| `docs/COMM_TOOL_SERIAL_IAP_DEBUG_2026-05-26.md` | 串口 IAP 调试记录 | TEST | COM3 复现、旧协议问题 | 是 | 当前记录 | 是 | 作为 debug 记录保留，摘要进 changelog |
| `docs/COMM_TOOL_SERIAL_PROTOCOL.md` | PC 与 comm tool 串口协议 | PROTOCOL | PC-comm tool 协议 | 否 | 未核工具工程 | 否 | 保留，不作为 BMS App 协议主文档 |
| `docs/COMM_TOOL_UART_SELECT_2026-05-25.md` | COMM TOOL 串口选择 | MODULE_DESIGN | 工具串口选择 | 否 | 未核工具工程 | 否 | 暂保留 |
| `docs/COMM_TOOL_UPGRADE_UI_2026-05-23.md` | comm tool 升级上位机 | TEST | UI 流程、实时监控 | 是 | 部分 | 是 | 与上位机文档合并后归档 |
| `docs/FACTORY_AGING_MODE_REQUIREMENTS_2026-05-26.md` | 老化模式运行定义 | MODULE_DESIGN | 老化状态、MOS、显示要求 | 是 | 基本当前 | 是 | 合并到 overview/protocol/test，待确认老化是否保留 |
| `docs/FACTORY_AGING_UPGRADE_RESET_CONFIG_2026-05-26.md` | 老化时间升级重置配置 | STORAGE | 升级参数策略相关 | 是 | 当前 | 是 | 合并到 `storage_design.md` |
| `docs/LEDBAR_SYSTEM_STATE_REFACTOR_2026-05-25.md` | LedBar 与系统状态收口 | LED_DISPLAY | LedBar runtime、系统状态 | 是 | 部分 | 是 | 合并当前源码内容到 `led_display_design.md`（待建） |
| `docs/MAP文件阅读与项目体积优化指南.md` | MAP 文件阅读指南 | TEST | map 地址、体积优化 | 否 | 需复核最新 map | 是 | 暂保留为工程指南 |
| `docs/RTC_SLEEP_PORT_REFACTOR_2026-05-25.md` | RTC Sleep Port 分层 | LOW_POWER | `rtc_sleep.c` 与 port 分层 | 是 | 基本当前 | 是 | 合并到 `low_power_design.md` |
| `docs/feidao-can-protocol-v1.6-analysis.md` | 飞道 CAN 协议解析 | PROTOCOL | 协议原文解析、字段、疑点 | 否 | 源协议参考 | 是 | 保留为协议研究；实现文档以源码为准 |
| `docs/low_power_rtc_change_log.md` | RTC 低功耗变更记录 | CHANGE_LOG | 低功耗 docs/代码变更记录 | 是 | 部分 | 是 | 合并摘要到 `docs/changelog/change_log.md`，归档候选 |
| `docs/low_power_rtc_final_report.md` | RTC 低功耗最终报告 | LOW_POWER | 低功耗总结、官方资料、风险 | 是 | 部分 | 是 | 合并已验证内容到 `low_power_design.md` |
| `docs/low_power_rtc_migration_plan.md` | RTC 低功耗迁移计划 | OLD_PLAN | 迁移目标、阶段 | 是 | 部分已实施 | 是 | 归档候选，当前计划以 `refactor_plan.md` 为准 |
| `docs/rtc_low_power_strategy.md` | RTC 与低功耗策略调研 | LOW_POWER | Stop/Standby/RTC/IWDG 规则 | 是 | 部分 | 是 | 合并原则到 `low_power_design.md` |
| `docs/architecture/low_power_industry_architecture.md` | 低功耗通用架构 | ARCHITECTURE | 行业架构与当前项目依据 | 是 | 部分 | 是 | 作为参考保留，权威行为合并 |
| `docs/current/clock_usage_analysis.md` | 当前时钟用法 | LOW_POWER | 时钟初始化与恢复 | 是 | 基本当前 | 是 | 合并到 `low_power_design.md` |
| `docs/current/iwdg_usage_analysis.md` | IWDG 用法 | LOW_POWER | IWDG 时序和限制 | 是 | 基本当前 | 是 | 合并到 `low_power_design.md` |
| `docs/current/low_power_current_usage.md` | 低功耗电流使用 | LOW_POWER | 电流阻塞 sleep | 是 | 部分 | 是 | 合并 |
| `docs/current/mcu_resource_related_to_low_power.md` | 低功耗相关资源 | LOW_POWER | MCU 资源与低功耗 | 是 | 部分 | 是 | 合并 |
| `docs/current/peripheral_sleep_analysis.md` | 外设睡眠分析 | LOW_POWER | 外设 sleep/resume | 是 | 部分 | 是 | 合并 |
| `docs/current/rtc_usage_analysis.md` | RTC 用法分析 | LOW_POWER | RTC 初始化/Alarm | 是 | 基本当前 | 是 | 合并 |
| `docs/design/bms_low_power_state_machine.md` | BMS 低功耗状态机 | LOW_POWER | 低功耗状态机设计 | 是 | 部分 | 是 | 合并到 `low_power_design.md` |
| `docs/design/clock_restore_after_stop.md` | Stop 后时钟恢复 | LOW_POWER | clock restore | 是 | 基本当前 | 是 | 合并 |
| `docs/design/iwdg_low_power_strategy.md` | IWDG 低功耗策略 | LOW_POWER | IWDG vs RTC | 是 | 基本当前 | 是 | 合并 |
| `docs/design/low_power_api_state_machine.md` | 低功耗 API 状态机 | LOW_POWER | API/状态 | 是 | 部分 | 是 | 合并 |
| `docs/design/low_power_block_reason.md` | 低功耗阻塞原因 | LOW_POWER | block reason 位图 | 是 | 基本当前 | 是 | 合并 |
| `docs/design/low_power_integration_scope.md` | 第三阶段集成范围 | OLD_PLAN | 低功耗接入计划 | 是 | 已实施一部分 | 是 | 归档候选 |
| `docs/design/low_power_minimal_architecture.md` | 最小低功耗架构 | OLD_PLAN | 早期设计 | 是 | 部分已实施 | 是 | 归档候选 |
| `docs/design/low_power_phase2_design_summary.md` | 第二阶段设计汇总 | OLD_PLAN | 阶段性设计 | 是 | 已实施一部分 | 是 | 归档候选 |
| `docs/design/peripheral_sleep_resume_plan.md` | 外设休眠恢复计划 | LOW_POWER | 外设矩阵 | 是 | 部分 | 是 | 合并 |
| `docs/design/rtc_wakeup_design.md` | RTC 周期唤醒设计 | LOW_POWER | RTC Stop 流程 | 是 | 基本当前 | 是 | 合并 |
| `docs/implementation/low_power_phase3_minimal_implementation.md` | 第三阶段实现记录 | CHANGE_LOG | 低功耗实现记录 | 是 | 部分 | 是 | 摘要进 changelog，归档候选 |
| `docs/research/rtc_stop_standby_rules.md` | RTC Stop/Standby 规则 | LOW_POWER | 官方规则清单 | 是 | 稳定参考 | 是 | 保留研究；摘要合并 |
| `docs/research/stm32_low_power_research.md` | STM32F0/F1 低功耗调研 | LOW_POWER | 官方资料调研 | 是 | 稳定参考 | 是 | 保留研究；摘要合并 |
| `docs/risk/low_power_risk_list.md` | 低功耗风险清单 | LOW_POWER | 风险项 | 是 | 部分 | 是 | 合并到 review 风险 |
| `docs/test/low_power_manual_test_steps.md` | 低功耗手工测试步骤 | TEST | 手工测试步骤 | 是 | 部分 | 是 | 合并到 `docs/test/test_plan.md` |
| `docs/test/low_power_test_matrix.md` | 低功耗测试矩阵 | TEST | 低功耗测试矩阵 | 是 | 基本当前 | 是 | 合并到 `docs/test/test_plan.md` |
| `docs/review/module_map.md` | 当前模块地图 | ARCHITECTURE | 本轮源码 review 产物 | 否 | 当前 | 是 | 保留为 review 证据 |
| `docs/review/requirement_confirmation.md` | 需求反推表 | MODULE_DESIGN | 本轮源码 review 产物 | 否 | 当前 | 是 | 保留为需求确认基线 |
| `docs/review/requirement_questions.md` | 需求确认问题表 | MODULE_DESIGN | 本轮源码 review 产物 | 否 | 当前 | 是 | 保留 |
| `docs/review/full_project_review.md` | 全项目 review | ARCHITECTURE | 本轮源码 review 产物 | 否 | 当前 | 是 | 保留 |
| `docs/review/refactor_plan.md` | 后续重构计划 | OLD_PLAN | 本轮源码 review 产物 | 否 | 当前 | 是 | 保留 |
| `docs/review/test_plan.md` | Review 测试计划 | TEST | 本轮源码 review 产物 | 是 | 当前 | 是 | 合并到 `docs/test/test_plan.md` |

## 3. 根目录项目文档盘点摘要

根目录散落文档数量较大，建议按主题合并，不直接保留为权威入口。

| 文档路径 | 文档名称 | 类型 | 主要内容 | 是否重复 | 是否可能过期 | 是否需要源码核对 | 建议处理方式 |
|---|---|---|---|---|---|---|---|
| `README.md`, `AGENTS.md`, `BMS_DAILY_DEV_WORKFLOW.md`, `项目协作与发布检查清单.md`, `项目自动化检查与发布流程.md` | 项目入口和协作规则 | PROJECT_OVERVIEW | 协作规则、脚本入口、发布检查 | 是 | 部分 | 是 | 入口迁移到 `docs/README.md`；AGENTS 保持根规则 |
| `PROJECT_REVIEW_2026-05-09.md`, `项目逻辑完整梳理与架构简化建议_2026-05-24.md`, `项目运行流程与时序源码梳理_2026-05-16.md`, `项目全模块外设功能驱动审计_2026-05-17.md` | 历史 review | ARCHITECTURE | 项目级审查、运行链路、外设审计 | 是 | 部分 | 是 | 摘要合并到 `architecture.md` 和本轮 review，旧文档归档候选 |
| `SOC_*.md`, `SOC*.md`, `BMS_SOC_STRATEGY_COMPARISON.md`, `SOC完整运行流程说明.md`, `SOC逻辑与参数影响梳理.md` | SOC 系列文档 | SOC | SOC 策略、测试、上位机、体验调参 | 是 | 部分 | 是 | 合并到 `docs/design/soc_design.md` 和 `docs/test/test_plan.md` |
| `ADC_配置调研与当前方案.md`, `ADC总压分压计算说明.md`, `TypeC_ADC电流采样与计算说明.md`, `AFE*.md`, `MONITOR_AFE_LOGIC_OPTIMIZATION.md` | ADC/AFE 系列 | ADC_AFE | ADC、AFE 电流、零点、监控 | 是 | 部分 | 是 | 合并到 `docs/design/adc_afe_design.md` |
| `COMMUNICATION_*.md`, `CAN_*.md`, `CAN*.md`, `SCI_PROTOCOL_SIZE_OPTIMIZATION_2026-05-22.md` | 通信系列 | CAN_MODBUS | 地址表、写参数、CAN 重构、IAP 上位机 | 是 | 部分 | 是 | 合并到 `docs/design/protocol_design.md`；协议细表后续拆到 `docs/protocol/` |
| `EEPROM_*.md`, `RW_PARAMETER_FLASH_STORAGE.md`, `STORAGE_LAYOUT_REPORT.md`, `Flash磨损寿命*.md`, `升级参数策略说明.md`, `后64K_SOC_AFE快速测试说明.md` | 存储系列 | STORAGE | EEPROM/Flash/后 64K/升级策略 | 是 | 部分 | 是 | 合并到 `docs/design/storage_design.md` |
| `RTC_*.md`, `LOW_POWER_*.md`, `休眠*.md`, `系统时钟系统梳理.md`, `运行架构与时基重构方案.md` | 低功耗/时钟系列 | LOW_POWER | RTC/STOP/CAN 低功耗/时钟 | 是 | 部分 | 是 | 合并到 `docs/design/low_power_design.md` |
| `LEDBAR_*.md`, `LED软件框架与时序梳理.md`, `数码管*.md`, `开关长按*.md`, `睡眠充电拔除*.md` | LED/显示系列 | LED_DISPLAY | Charlieplexing、按键、显示窗口 | 是 | 部分 | 是 | 后续生成 `docs/design/led_display_design.md`；本轮先在 overview/module_map 标记 |
| `TEST_PENDING*.md`, `SOC_TEST*.md`, `SOC_HOST_VALIDATION_PLAN.md`, `SOC_RIDE_SIM_REPORT.md` | 测试系列 | TEST | 待测、SOC 测试、上位机测试 | 是 | 部分 | 是 | 合并到 `docs/test/test_plan.md` |
| `MODULE_*`, `PROJECT_*_2026-05-22.md`, `TODO*.md`, `UNUSED_SYMBOL_CLEANUP_2026-05-22.md`, `HEAT_COOL_IODRIVERS_REMOVAL_2026-05-22.md` | 重构计划/清理记录 | OLD_PLAN | 减码、历史移除、旧计划 | 是 | 可能 | 是 | 归档候选；有效约束转入 `refactor_plan.md` |

## 4. 生成/外部/参考文件

| 文档路径 | 类型 | 处理方式 |
|---|---|---|
| `build/generated_templates/**/docs/**/*.md` | TEMP / generated | 不纳入权威 docs；仅作为模板生成输出参考 |
| `build/generated_templates_test/**/docs/**/*.md` | TEMP / generated | 不纳入权威 docs |
| `build/firmware_rewrite_cmake/**/*.txt` | TEMP / build output | 生成缓存，不纳入 docs |
| `.probes/rtt/*.md` | UNKNOWN / external tool | 工具说明，不纳入 BMS 文档体系 |
| `SH3673520+STM32F072CBT6 DemoCode.../**/*.txt` | ADC_AFE / external demo | F0/SH3673520 参考，不作为当前 App 行为 |
| `firmware/comm_tool_f103ret6/source/app/README.md` | PROJECT_OVERVIEW / tool firmware | 独立 comm tool 固件文档，保留在工具工程内 |

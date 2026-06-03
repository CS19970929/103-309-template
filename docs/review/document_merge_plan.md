# 文档合并计划

> 文档状态：CURRENT
> 源码验证：PARTIAL
> 最后更新时间：2026-06-03
> 未确认事项：本轮只执行 SOC 文档合并和权威入口更新，不删除、不移动旧文档。

| 操作 ID | 操作类型 | 源文档 | 目标文档 | 操作说明 | 是否需要用户确认 | 风险 |
|---|---|---|---|---|---|---|
| DOC-001 | KEEP | `AGENTS.md` | 原路径 | 保留仓库协作规则和烧录安全约束 | 否 | 低 |
| DOC-002 | REWRITE_FROM_SOURCE | `docs/review/module_map.md`, 源码 | `docs/module_map.md` | 按当前源码生成权威模块地图 | 否 | 低 |
| DOC-003 | REWRITE_FROM_SOURCE | 源码 + 本轮 review | `docs/architecture.md` | 重写当前架构，不沿用旧阶段结论 | 否 | 低 |
| DOC-004 | REWRITE_FROM_SOURCE | 源码 + `requirement_confirmation.md` | `docs/project_overview.md` | 项目总览按源码现状描述 | 否 | 低 |
| DOC-005 | MERGE | `EEPROM_*`, `RW_PARAMETER_FLASH_STORAGE.md`, `STORAGE_LAYOUT_REPORT.md`, `Flash磨损寿命*.md`, `升级参数策略说明.md` | `docs/design/storage_design.md` | 只合并当前内部 Flash 实现；外部 EEPROM 旧布局标历史 | 否 | 中 |
| DOC-006 | MERGE | `COMMUNICATION_*`, `docs/BMS_CAN_SERVICE_PROTOCOL.md`, `docs/CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md` | `docs/design/protocol_design.md` | 合并 Modbus/CAN/UART 当前行为和风险 | 否 | 中 |
| DOC-007 | MERGE_DONE | `docs/design/soc_design.md`, `docs/review/soc_current_logic_2026-06-02.md`, `docs/review/soc_simplification_candidates_2026-06-02.md`, SOC 历史 devlog | `docs/design/soc_design.md` | `soc_design.md` 已收口为当前权威入口，包含源码 review、主流程、tail 表、自耗/RTC 口径、存储、调试、测试和风险；旧 review/devlog 只保留历史追溯，不作为当前事实 | 否 | 低 |
| DOC-008 | MERGE | `ADC*.md`, `AFE*.md`, `TypeC_ADC*.md` | `docs/design/adc_afe_design.md` | 合并 ADC/AFE 当前数据流和 P0 电流问题 | 否 | 中 |
| DOC-009 | MERGE | `docs/current/*.md`, `docs/design/low_power_*.md`, `RTC_*.md`, `休眠*.md` | `docs/design/low_power_design.md` | 合并当前低功耗源码行为，旧阶段设计不再作为结论 | 否 | 中 |
| DOC-010 | MERGE | `docs/review/test_plan.md`, `docs/test/low_power_test_matrix.md`, `SOC_TEST*.md` | `docs/test/test_plan.md` | 生成统一测试总计划 | 否 | 低 |
| DOC-011 | KEEP | `docs/feidao-can-protocol-v1.6-analysis.md` | 原路径 | 保留为源协议研究，不作为实现结论 | 否 | 低 |
| DOC-012 | ARCHIVE | `CAN通信逻辑与低功耗策略分析.md` | `docs/archive/old_docs/` | 与当前 CAN App 请求处理冲突，建议归档 | 是 | 低 |
| DOC-013 | ARCHIVE | `CAN低功耗发送调度说明.md` | `docs/archive/old_docs/` | 旧 CAN 上电/断电状态机说明已过时 | 是 | 低 |
| DOC-014 | ARCHIVE | `STORAGE_LAYOUT_REPORT.md` | `docs/archive/old_docs/` | 外部 EEPROM 旧布局，与当前内部 Flash 不一致 | 是 | 低 |
| DOC-015 | ARCHIVE | `docs/low_power_rtc_migration_plan.md` 和低功耗阶段设计文档 | `docs/archive/old_docs/` | 阶段计划已被当前实现和新设计替代 | 是 | 低 |
| DOC-016 | DELETE_CANDIDATE | `TEST_PENDING copy.md` | 无 | 与 `TEST_PENDING.md` 重复，暂不删除 | 是 | 低 |
| DOC-017 | NEED_USER_CONFIRM | LED/LedBar 系列 | 后续 `docs/design/led_display_design.md` | 充电图标、故障显示、长按时间需确认后正式合并 | 是 | 中 |
| DOC-018 | NEED_USER_CONFIRM | IAP/Bootloader 系列 | 后续 `docs/design/bootloader_iap_design.md` | 需核 IAP/comm tool 独立工程和最终 map 地址 | 是 | 高 |
| DOC-019 | ARCHIVE | `build/generated_templates*/**/*.md` | 不归档，仅保留生成目录 | 生成输出不纳入权威文档 | 否 | 低 |
| DOC-020 | KEEP | `docs/COMM_TOOL_SERIAL_PROTOCOL.md` | 原路径 | 独立工具协议，后续单独核 | 否 | 中 |

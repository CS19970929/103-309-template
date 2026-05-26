# 文档变更记录

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：当前主工程源码和 `docs/review/*`
最后更新时间：2026-05-27
未确认事项：`NEED_CONFIRM` 文档仍需用户确认是否保留；部分旧文档仍被 `tools/project_check.py` 固定引用。

## 2026-05-27 文档清理与合并

### 本次删除

- 删除确认已合并、过时、重复、临时或旧方案性质的 Markdown 文档 115 份。
- 删除 `TEST_PENDING copy.md`，保留 `TEST_PENDING.md`。
- 没有保留 `docs/archive/old_docs/` 旧文档副本；如需找回旧文档，可从 Git 历史恢复。

删除依据和完整清单见：

- `docs/review/document_cleanup_report.md`

### 本次新增

- `docs/protocol/modbus_register_map.md`
- `docs/protocol/can_protocol.md`
- `docs/protocol/uart_protocol.md`
- `docs/design/led_display_design.md`
- `docs/design/bootloader_iap_design.md`

### 本次更新

- `docs/README.md`
- `docs/archive/README.md`
- `docs/changelog/change_log.md`

### 本次源码修改

没有修改源码。

### 暂不删除

以下类型文档保留为 `NEED_CONFIRM`：

- 被 `tools/project_check.py` 固定引用的旧文档。
- 根目录协作、发布、调试、TODO 类流程文档。
- comm tool 子工程文档。
- 当前工作区已有用户修改的文档。

## 2026-05-26 文档体系整理

### 本次新增

- `docs/README.md`
- `docs/project_overview.md`
- `docs/architecture.md`
- `docs/module_map.md`
- `docs/design/storage_design.md`
- `docs/design/protocol_design.md`
- `docs/design/soc_design.md`
- `docs/design/adc_afe_design.md`
- `docs/design/low_power_design.md`
- `docs/test/test_plan.md`
- `docs/archive/README.md`
- `docs/review/document_inventory.md`
- `docs/review/document_source_consistency.md`
- `docs/review/document_duplicate_analysis.md`
- `docs/review/document_structure_plan.md`
- `docs/review/document_merge_plan.md`

### 本次源码修改

没有修改源码。

### 本次合并内容

本轮只做低风险“内容合并”和“权威入口创建”，没有移动、删除旧文档。

已合并到权威文档的主题：

- 项目总览和架构。
- Flash/EEPROM 兼容层和后 64K 存储。
- Modbus/CAN 通信关系。
- SOC 当前算法链路。
- ADC/AFE 当前数据流。
- RTC/低功耗/IWDG 当前行为。
- 全项目测试计划。

### 仍需确认

1. 是否归档旧低功耗阶段文档。
2. 是否归档旧 CAN 低功耗文档。
3. 是否归档旧外部 EEPROM 布局文档。
4. 是否后续删除 `TEST_PENDING copy.md`。
5. 是否补建 `docs/protocol/*`, `docs/design/led_display_design.md`, `docs/design/bootloader_iap_design.md`。

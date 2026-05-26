# 103-309 BMS 文档入口

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source` 当前主工程源码
最后更新时间：2026-05-26
未确认事项：电流真实路径、均衡需求、老化保留策略、Host 写权限、实际 Flash 容量、低功耗 CAN 广播策略仍需用户确认。

## 1. 使用原则

1. 源码是第一可信来源。
2. 本目录下新增的权威文档只描述“当前源码已验证行为”和“待确认事项”。
3. 根目录大量旧文档和阶段记录只能作为历史参考，不能直接当成当前需求。
4. 文档与源码冲突时，以源码为准，并把冲突记录到 `docs/review/document_source_consistency.md`。

## 2. 当前权威文档

| 文档 | 用途 |
|---|---|
| `docs/project_overview.md` | 项目背景、硬件平台、主要功能、当前状态 |
| `docs/architecture.md` | 软件分层、主循环、任务调度、数据流、控制流 |
| `docs/module_map.md` | 模块和源码文件对应关系 |
| `docs/design/storage_design.md` | Flash/EEPROM 兼容层/参数存储设计 |
| `docs/design/protocol_design.md` | Modbus/CAN/UART 通信设计和兼容风险 |
| `docs/design/soc_design.md` | SOC 输入、输出、算法流程、持久化和风险 |
| `docs/design/adc_afe_design.md` | ADC/AFE 数据流、采样、校准和当前问题 |
| `docs/design/low_power_design.md` | RTC/STOP/IWDG/唤醒/低功耗阻塞 |
| `docs/test/test_plan.md` | 编译、协议、存储、SOC、低功耗和硬件实测计划 |
| `docs/changelog/change_log.md` | 文档整理和后续变更记录 |

## 3. Review 与需求确认文档

| 文档 | 用途 |
|---|---|
| `docs/review/module_map.md` | 本轮源码扫描细节 |
| `docs/review/requirement_confirmation.md` | 从源码反推的需求清单 |
| `docs/review/requirement_questions.md` | 需要用户确认的问题表 |
| `docs/review/full_project_review.md` | 全项目 review 结论 |
| `docs/review/refactor_plan.md` | 后续分阶段重构计划 |
| `docs/review/document_inventory.md` | 文档盘点 |
| `docs/review/document_source_consistency.md` | 文档与源码一致性检查 |
| `docs/review/document_duplicate_analysis.md` | 重复文档分析 |
| `docs/review/document_structure_plan.md` | 新文档结构方案 |
| `docs/review/document_merge_plan.md` | 文档合并计划 |

## 4. 历史文档边界

旧文档暂不删除、不移动。建议后续在用户确认后，把过时或重复文档移动到 `docs/archive/old_docs/`，并保留索引。

当前已识别的高重复主题：

- RTC/低功耗阶段设计。
- SOC 策略、测试、上位机需求。
- CAN/Modbus/comm tool/IAP 文档。
- EEPROM/Flash/后 64K 存储文档。
- LedBar/数码管显示文档。

## 5. 维护规则

1. 修改源码前，先确认需求表中的相关项。
2. 修改协议、地址、Flash 布局、IAP、低功耗、SOC、AFE/MOS 时，必须同步更新对应设计文档和测试计划。
3. 新文档开头必须包含：文档状态、源码验证、主要参考源码、最后更新时间、未确认事项。
4. 临时记录可以保留，但不能放在权威文档入口中作为当前结论。

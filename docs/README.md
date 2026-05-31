# 103-309 BMS 文档入口

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source` 当前主工程源码
最后更新时间：2026-05-31（目录整理后）
未确认事项：虚拟电流调试入口隔离、均衡需求、老化保留策略、Host 写权限、实际 Flash 容量、低功耗 CAN 广播策略、存储失败可见性和升级清参策略仍需用户确认。

## 1. 使用原则

1. 源码是第一可信来源。
2. 本目录下新增的权威文档只描述”当前源码已验证行为”和”待确认事项”。
3. 已归档旧文档（`docs/archive/`）只能作为历史参考，不能直接当成当前需求。
4. 文档与源码冲突时，以源码为准，并把冲突记录到 `docs/review/document_source_consistency.md`。
5. 根目录只保留 AGENTS.md（协作规则）、CLAUDE.md（项目简介）、README.md（导航）、TODO.md（待办）。其余文档已移入 `docs/` 子目录。

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
| `docs/design/led_display_design.md` | LedBar/Charlieplexing 显示、按键、sleep SOC 和风险 |
| `docs/design/bootloader_iap_design.md` | Bootloader/IAP 地址、进入 IAP 触发路径和烧录安全 |
| `docs/protocol/modbus_register_map.md` | Modbus 地址窗口和高风险寄存器入口 |
| `docs/protocol/can_protocol.md` | CAN 周期广播和 App 命令当前实现 |
| `docs/protocol/uart_protocol.md` | UART/RS485 串口协议入口和低功耗关系 |
| `docs/protocol/COMMUNICATION_ADDRESS_INDEX.md` | 通信地址索引表（来自根目录重定位） |
| `docs/test/test_plan.md` | 编译、协议、存储、SOC、低功耗和硬件实测计划 |
| `docs/test/TEST_PENDING.md` | 待测试清单（来自根目录重定位） |
| `docs/workflow/BMS_DAILY_DEV_WORKFLOW.md` | 日常开发工作流（来自根目录重定位） |
| `docs/workflow/项目协作与发布检查清单.md` | 协作与发布检查清单（来自根目录重定位） |
| `docs/workflow/项目自动化检查与发布流程.md` | 自动化检查与发布流程（来自根目录重定位） |
| `docs/changelog/change_log.md` | 文档整理和后续变更记录 |

## 3. Review 与需求确认文档

| 文档 | 用途 |
|---|---|
| `docs/review/module_map.md` | 本轮源码扫描细节 |
| `docs/review/current_goal_fact_sync_2026-05-31.md` | 当前目标、源码事实、过期文档冲突、需求确认焦点和分阶段执行计划 |
| `docs/review/current_goal_completion_audit_2026-05-31.md` | 当前长期目标完成度审计、证据矩阵、未完成项和验证边界 |
| `docs/review/flash_iap_address_gate_plan_2026-05-31.md` | App/IAP/Flash 存储/SRAM mailbox 地址门禁方案 |
| `docs/review/s1_flash_iap_user_confirmation_2026-05-31.md` | S1 App 结束地址、IAP SRAM mailbox、Release map 和 MCU Flash 容量的用户确认包 |
| `docs/review/afe_safety_gate_plan_2026-05-31.md` | AFE 参数写入、通信失败和 watchdog 安全门禁方案 |
| `docs/review/s2_s3_afe_user_confirmation_2026-05-31.md` | S2/S3 AFE 写参权限、非法参数响应、fail-safe、watchdog 和虚拟电流隔离的用户确认包 |
| `docs/review/low_power_comm_wake_gate_plan_2026-05-31.md` | reset-sleep 唤醒源、通信活跃和 CAN RTC 服务门禁方案 |
| `docs/review/s4_low_power_comm_user_confirmation_2026-05-31.md` | S4 reset-sleep 唤醒源、通信活跃、CAN RTC service 和 IWDG 取舍的用户确认包 |
| `docs/review/protocol_write_ack_gate_plan_2026-05-31.md` | Modbus/CAN App 写入口空实现、`#if 0` 和 ACK 语义门禁方案 |
| `docs/review/s5_protocol_ack_user_confirmation_2026-05-31.md` | S5 协议空实现/`#if 0` 写入口、错误码和 CAN App 复用语义的用户确认包 |
| `docs/review/storage_upgrade_gate_plan_2026-05-31.md` | Flash/EEPROM 存储失败可见性和升级清参策略门禁方案 |
| `docs/review/s7_storage_upgrade_user_confirmation_2026-05-31.md` | S7 存储失败可见性、低功耗阻塞和 upgrade policy `0x0005` 清参策略的用户确认包 |
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

旧文档不作为当前结论。本轮按用户最新要求，已经删除确认被合并、过时、重复、临时或旧方案性质的文档；删除清单和保留原因见 `docs/review/document_cleanup_report.md`。

仍有一批旧文档被 `tools/project_check.py` 固定引用，暂时保留原路径，避免破坏现有验证脚本。后续如要彻底收口，需要先把检查脚本依赖迁移到新的权威文档。

当前已识别的高重复主题：

- RTC/低功耗阶段设计。
- SOC 策略、测试、上位机需求。
- CAN/Modbus/comm tool/IAP 文档。
- EEPROM/Flash/后 64K 存储文档。
- LedBar/数码管显示文档。

清理原则：

1. 已合并、过时、重复、临时和旧方案文档删除，不长期归档。
2. 当前结论以 `docs/design/*`, `docs/protocol/*`, `docs/review/*` 为准。
3. 若需要找回已删除旧文档，从 Git 历史恢复。
4. 不确定业务价值的文档列为 `NEED_CONFIRM`，暂不删除。

## 5. 维护规则

1. 修改源码前，先确认需求表中的相关项。
2. 修改协议、地址、Flash 布局、IAP、低功耗、SOC、AFE/MOS 时，必须同步更新对应设计文档和测试计划。
3. 新文档开头必须包含：文档状态、源码验证、主要参考源码、最后更新时间、未确认事项。
4. 临时记录可以保留，但不能放在权威文档入口中作为当前结论。

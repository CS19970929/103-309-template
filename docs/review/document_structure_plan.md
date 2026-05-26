# 推荐文档结构设计

> 文档状态：CURRENT
> 源码验证：PARTIAL
> 最后更新时间：2026-05-26
> 未确认事项：本轮不移动、不删除旧文档；目录结构先以新增权威文档实现。

## 1. 推荐的新目录结构

```text
docs/
  README.md
  project_overview.md
  architecture.md
  module_map.md

  design/
    storage_design.md
    protocol_design.md
    soc_design.md
    adc_afe_design.md
    low_power_design.md
    led_display_design.md        # 后续建议新增
    bootloader_iap_design.md     # 后续建议新增

  protocol/
    modbus_register_map.md       # 后续从 Sci_Upper.h 生成
    can_protocol.md              # 后续从 Can_HDX/CanFeidaoFrames 生成
    uart_protocol.md             # 后续从 Sci_Upper 生成

  test/
    test_plan.md
    protocol_regression_test.md  # 后续拆分
    storage_test_plan.md         # 后续拆分
    soc_test_plan.md             # 后续拆分
    low_power_test_plan.md       # 后续拆分
    hardware_test_checklist.md   # 后续拆分

  review/
    module_map.md
    full_project_review.md
    requirement_confirmation.md
    requirement_questions.md
    refactor_plan.md
    test_plan.md
    document_inventory.md
    document_source_consistency.md
    document_duplicate_analysis.md
    document_structure_plan.md
    document_merge_plan.md

  changelog/
    change_log.md

  archive/
    README.md
    old_docs/                    # 需用户确认后再移动旧文档
```

## 2. 每个权威文档职责

| 文档 | 职责 |
|---|---|
| `docs/README.md` | 文档入口、权威文档索引、维护规则、历史文档边界 |
| `docs/project_overview.md` | 项目背景、硬件平台、当前功能、已知风险 |
| `docs/architecture.md` | 软件分层、主循环、任务调度、数据流、控制流 |
| `docs/module_map.md` | 模块到源码文件映射、依赖和重构建议 |
| `docs/design/storage_design.md` | 内部 Flash/EEPROM 兼容层/SOC snapshot/日志/老化持久化 |
| `docs/design/protocol_design.md` | Modbus/CAN/UART 关系、协议兼容风险和写副作用 |
| `docs/design/soc_design.md` | SOC 输入、输出、算法、持久化、待确认问题 |
| `docs/design/adc_afe_design.md` | ADC/AFE 数据流、采样、滤波、校准、当前问题 |
| `docs/design/low_power_design.md` | RTC/STOP/IWDG/唤醒/阻塞原因/风险 |
| `docs/test/test_plan.md` | 全项目测试总计划和硬件实测清单 |
| `docs/changelog/change_log.md` | 文档整理和后续变更记录 |

## 3. 旧文档合并目标

| 旧文档/文档组 | 合并到 |
|---|---|
| 项目 review/运行流程/外设审计系列 | `docs/architecture.md`, `docs/module_map.md`, `docs/review/full_project_review.md` |
| SOC 系列文档 | `docs/design/soc_design.md`, `docs/test/test_plan.md` |
| ADC/AFE 系列文档 | `docs/design/adc_afe_design.md` |
| EEPROM/Flash/后 64K/升级参数文档 | `docs/design/storage_design.md` |
| Modbus/CAN/通信地址/comm tool 固件行为 | `docs/design/protocol_design.md`，后续拆 `docs/protocol/*` |
| RTC/低功耗/时钟/IWDG 文档 | `docs/design/low_power_design.md`, `docs/test/test_plan.md` |
| LED/LedBar 文档 | 后续 `docs/design/led_display_design.md` |
| IAP/Bootloader 文档 | 后续 `docs/design/bootloader_iap_design.md` |
| 阶段性 change log | `docs/changelog/change_log.md` 摘要，原文归档 |

## 4. 建议归档的旧文档

建议归档但本轮不移动：

- 旧低功耗阶段设计：`docs/low_power_rtc_migration_plan.md`, `docs/design/low_power_phase2_design_summary.md`, `docs/design/low_power_minimal_architecture.md`, `docs/design/low_power_integration_scope.md`。
- 旧 CAN 低功耗阶段文档：`CAN低功耗发送调度说明.md`, `CAN_RTC低功耗重构说明_2026-05-14.md`, `CAN通信逻辑与低功耗策略分析.md`。
- 旧外部 EEPROM 布局：`STORAGE_LAYOUT_REPORT.md`, `EEPROM_LAYOUT_OPTIMIZATION.md`。
- 旧 SOC 阶段修复/简化记录：`SOC_SIMPLIFY_CHARGE_FIX.md`, `SOC_MODULE_SIMPLIFICATION_2026-05-12.md`。
- 旧 LED 阶段记录：`LEDBAR_RUNTIME_REFACTOR.md`, `LEDBAR_STABILITY_SIMPLIFY_2026-05-15.md`。

## 5. 暂时保留的旧文档

- `AGENTS.md`：仓库协作规则，仍然有效。
- `docs/feidao-can-protocol-v1.6-analysis.md`：源协议研究，不等同当前实现。
- `docs/COMM_TOOL_SERIAL_PROTOCOL.md`：comm tool 独立协议，后续单独核。
- `docs/BMS_CAN_IAP_PROTOCOL.md`：IAP 协议参考，后续按 IAP 工程核。
- `STM32官方外设配置学习与检查手册_2026-05-17.md`：官方资料索引价值高，建议保留为研究文档。

## 6. 需要用户确认后才能合并的内容

1. 量产电流路径是否恢复真实 AFE CADC。
2. 均衡是否为当前产品需求。
3. 老化是否保留在量产固件。
4. Host 写参数是否需要权限模式。
5. 实际 MCU Flash 容量和 App/IAP 地址最终口径。
6. LED 充电图标/故障显示/长按时间定义。
7. RTC 休眠中是否需要 CAN 周期广播。

## 7. 后续新增文档规则

1. 每份权威文档开头必须写：文档状态、源码验证状态、主要参考源码、最后更新时间、未确认事项。
2. 当前行为必须引用源码；历史方案必须标记为历史或归档候选。
3. 不确定内容只能写入“待确认”，不能写成结论。
4. 新增协议字段必须同时更新协议文档、测试计划和上位机兼容说明。
5. 大改源码前必须先更新 `docs/review/requirement_questions.md` 或对应设计文档。

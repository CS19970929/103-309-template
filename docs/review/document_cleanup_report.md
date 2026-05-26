# 文档清理报告

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source`, `tools/project_check.py`, `docs/design/*`, `docs/protocol/*`, `docs/review/*`
最后更新时间：2026-05-27
未确认事项：部分旧文档仍被 `tools/project_check.py` 固定引用，暂不删除；部分流程/工具文档是否继续保留需要用户确认。

## 1. 清理结论

本轮按“源码是第一可信来源”的原则整理项目文档：

- 删除旧文档：115 份。
- 新增权威补充文档：5 份。
- 更新入口/变更记录：`docs/README.md`, `docs/archive/README.md`, `docs/changelog/change_log.md`。
- 未修改源码、Keil 工程、编译配置、协议行为。
- 没有长期保留 `docs/archive/old_docs/` 副本；如需找回旧文档，可从 Git 历史恢复。

## 2. 新增或保留的权威文档

| 文档 | 作用 |
|---|---|
| `docs/README.md` | 当前 docs 入口和维护规则 |
| `docs/project_overview.md` | 项目总览 |
| `docs/architecture.md` | 当前主循环、任务调度、数据流和控制流 |
| `docs/module_map.md` | 模块到源码文件的映射 |
| `docs/design/storage_design.md` | 参数存储和 Flash 设计 |
| `docs/design/protocol_design.md` | 通信架构总览 |
| `docs/design/soc_design.md` | SOC 当前设计 |
| `docs/design/adc_afe_design.md` | ADC/AFE 当前设计 |
| `docs/design/low_power_design.md` | RTC/STOP/IWDG 低功耗当前设计 |
| `docs/design/led_display_design.md` | LedBar/Charlieplexing 显示当前设计 |
| `docs/design/bootloader_iap_design.md` | Bootloader/IAP 和烧录安全当前设计 |
| `docs/protocol/modbus_register_map.md` | Modbus 地址窗口和关键寄存器入口 |
| `docs/protocol/can_protocol.md` | CAN 周期广播和 App 命令当前实现 |
| `docs/protocol/uart_protocol.md` | UART/RS485 当前实现 |
| `docs/test/test_plan.md` | 项目测试计划 |
| `docs/review/*` | 本轮 review、需求确认、文档一致性和重构计划 |

## 3. 删除判定规则

| 判定 | 删除条件 | 处理 |
|---|---|---|
| 已合并 | 内容已经被 `docs/design/*`, `docs/protocol/*`, `docs/review/*` 覆盖 | 删除旧文档 |
| 明显过时 | 与当前源码行为不一致，例如旧低功耗/CAN/EEPROM 口径 | 删除旧文档 |
| 重复文档 | 多份阶段记录描述同一模块，且权威文档已有当前结论 | 删除旧文档 |
| 临时记录 | 一次性测试、构建、阶段说明，无长期维护价值 | 删除旧文档 |
| 旧方案 | 与当前长期目标或当前源码不一致的旧方案 | 删除旧文档 |
| 不确定 | 可能仍被脚本、流程或业务使用 | 标记 `NEED_CONFIRM`，暂不删除 |

## 4. 删除文档分组

| 分组 | 删除内容 | 合并到/替代文档 |
|---|---|---|
| ADC/AFE/MOS | ADC 配置、总压分压、AFE 冷启动、电流零点、Type-C 电流、MOS 启动阶段记录 | `docs/design/adc_afe_design.md` |
| Storage/EEPROM/参数 | EEPROM 旧布局、Flash 磨损、OtherElement 升级、后 64K 测试、参数可修改性 | `docs/design/storage_design.md` |
| SOC | SOC 策略、测试 UI、骑行仿真、校准、简化阶段记录 | `docs/design/soc_design.md`, `docs/test/test_plan.md` |
| Low Power/RTC | 低功耗迁移计划、RTC 阶段设计、CAN 低功耗旧方案、低功耗测试矩阵 | `docs/design/low_power_design.md` |
| LED/LedBar | Charlieplexing 阶段记录、旧 LedBar runtime、旧 3 秒长按说明 | `docs/design/led_display_design.md` |
| CAN/Modbus/IAP/comm tool | 旧通信布局、IAP 上位机计划、comm tool 阶段记录、SCI 优化记录 | `docs/design/protocol_design.md`, `docs/protocol/*`, `docs/design/bootloader_iap_design.md` |
| Project review/old plan | 旧全项目 review、资源评估、模块瘦身计划、外设审计阶段记录 | `docs/review/full_project_review.md`, `docs/architecture.md`, `docs/module_map.md` |
| Duplicate temp | `TEST_PENDING copy.md` | `TEST_PENDING.md` |

## 5. 已删除文档清单

### ADC / AFE / MOS

- `ADC_配置调研与当前方案.md`
- `ADC总压分压计算说明.md`
- `AFE冷上电初始化与电流零点校准说明.md`
- `AFE电流计算与自动零点补偿说明.md`
- `MONITOR_AFE_LOGIC_OPTIMIZATION.md`
- `MOS_CTL启动时序重构说明_2026-05-18.md`
- `TypeC_ADC电流采样与计算说明.md`

### Storage / EEPROM / 参数

- `COMMUNICATION_EEPROM_FLAG_MAPPING.md`
- `COMMUNICATION_EEPROM_FLAG_REFACTOR_DEBUG.md`
- `EEPROM_LAYOUT_OPTIMIZATION.md`
- `EEPROM_WRITEFLAG_CLEANUP.md`
- `Flash磨损寿命与日志限频优化说明_2026-05-13.md`
- `OtherElement升级字段覆盖说明.md`
- `RW_PARAMETER_FLASH_STORAGE.md`
- `STORAGE_LAYOUT_REPORT.md`
- `升级参数策略说明.md`
- `参数修改方式与可修改性梳理.md`
- `后64K_SOC_AFE快速测试说明.md`

### SOC

- `BMS_SOC_STRATEGY_COMPARISON.md`
- `SOC_CALIBRATION_STRATEGY.md`
- `SOC_CONFIG_GATE_OPTIMIZATION.md`
- `SOC_CURRENT_SIM_TEST_REPORT_2026-05-12.md`
- `SOC_EXPERIENCE_TUNING_CONFIG_2026-05-12.md`
- `SOC_HOST_VALIDATION_PLAN.md`
- `SOC_KEIL_WATCH_DEBUG_2026-05-12.md`
- `SOC_MCU_RIDE_TEST_MODE.md`
- `SOC_MCU_SAFE_FLASH_GUIDE.md`
- `SOC_MODULE_LOGIC.md`
- `SOC_MODULE_SIMPLIFICATION_2026-05-12.md`
- `SOC_RIDE_SIM_REPORT.md`
- `SOC_SIMPLIFY_CHARGE_FIX.md`
- `SOC_TEST_EXECUTION_REPORT.md`
- `SOC_TEST_REQUIREMENTS_SUMMARY.md`
- `SOC_TEST_UI_EXE_BUILD.md`
- `SOC_TEST_UI_STARTUP_AND_PERSISTENCE.md`
- `SOC_TEST_UPPER_COMPUTER_FUNCTION_SPEC.md`
- `SOC_UPPER_TEST_REQUIREMENTS.md`
- `SOC完整运行流程说明.md`
- `SOC板端自耗补偿说明_2026-05-13.md`
- `SOC逻辑与参数影响梳理.md`

### Low Power / RTC

- `CAN_RTC低功耗广播修复说明.md`
- `CAN_RTC低功耗重构说明_2026-05-14.md`
- `CAN低功耗发送调度说明.md`
- `CAN通信逻辑与低功耗策略分析.md`
- `LOW_POWER_CAN_RTC_REFACTOR_2026-05-15.md`
- `RTC_CAN自适应休眠说明.md`
- `RTC_STOP低功耗配置修复说明_2026-05-14.md`
- `休眠低功耗逻辑梳理与优化建议.md`
- `休眠唤醒数码管显示10秒说明.md`
- `睡眠充电拔除与数码管图标规则_2026-05-15.md`
- `docs/architecture/low_power_industry_architecture.md`
- `docs/current/clock_usage_analysis.md`
- `docs/current/iwdg_usage_analysis.md`
- `docs/current/low_power_current_usage.md`
- `docs/current/mcu_resource_related_to_low_power.md`
- `docs/current/peripheral_sleep_analysis.md`
- `docs/current/rtc_usage_analysis.md`
- `docs/design/bms_low_power_state_machine.md`
- `docs/design/clock_restore_after_stop.md`
- `docs/design/iwdg_low_power_strategy.md`
- `docs/design/low_power_api_state_machine.md`
- `docs/design/low_power_block_reason.md`
- `docs/design/low_power_integration_scope.md`
- `docs/design/low_power_minimal_architecture.md`
- `docs/design/low_power_phase2_design_summary.md`
- `docs/design/peripheral_sleep_resume_plan.md`
- `docs/design/rtc_wakeup_design.md`
- `docs/implementation/low_power_phase3_minimal_implementation.md`
- `docs/low_power_rtc_change_log.md`
- `docs/low_power_rtc_final_report.md`
- `docs/low_power_rtc_migration_plan.md`
- `docs/research/rtc_stop_standby_rules.md`
- `docs/research/stm32_low_power_research.md`
- `docs/risk/low_power_risk_list.md`
- `docs/rtc_low_power_strategy.md`
- `docs/test/low_power_manual_test_steps.md`
- `docs/test/low_power_test_matrix.md`

### LED / LedBar

- `LEDBAR_GPIO_CHARLIE_DISPLAY_PLAN.md`
- `LEDBAR_RUNTIME_REFACTOR.md`
- `LEDBAR_STABILITY_SIMPLIFY_2026-05-15.md`
- `LED软件框架与时序梳理.md`
- `docs/LEDBAR_SYSTEM_STATE_REFACTOR_2026-05-25.md`
- `开关长按休眠3秒计时修复说明.md`
- `数码管GPIO查理复用重写说明.md`

### CAN / Modbus / IAP / comm tool

- `CAN_IAP_UPPER_COMPUTER_PLAN.md`
- `COMMUNICATION_LAYOUT_REPORT.md`
- `COMMUNICATION_WRITE_DETAIL.md`
- `IAP_APP_TIM3_HANDOFF_FIX_2026-05-18.md`
- `SCI_PROTOCOL_SIZE_OPTIMIZATION_2026-05-22.md`
- `docs/BMS_BOOT_CONTROL_REFACTOR_2026-05-23.md`
- `docs/BMS_CAN_MODULE_REFACTOR_2026-05-23.md`
- `docs/BMS_COMM_TOOL_READ_WRITE_2026-05-23.md`
- `docs/CAN_HEARTBEAT_DIAG_2026-05-23.md`
- `docs/COMM_ARCH_OPTIMIZATION_2026-05-26.md`
- `docs/COMM_TOOL_RELIABILITY_TEST_2026-05-23.md`
- `docs/COMM_TOOL_SERIAL_IAP_DEBUG_2026-05-26.md`
- `docs/FACTORY_AGING_MODE_REQUIREMENTS_2026-05-26.md`
- `docs/FACTORY_AGING_UPGRADE_RESET_CONFIG_2026-05-26.md`

### Project review / old plan

- `App_AnlogCal时基修改影响说明.md`
- `MCU资源分布与架构优化评估.md`
- `MODULE_CORE_REQUIREMENTS_SIZE_FIRST_2026-05-22.md`
- `MODULE_DEEP_SIZE_REFACTOR_2026-05-22.md`
- `PROJECT_RESOURCE_ARCH_OPTIMIZATION_2026-05-22.md`
- `PROJECT_REVIEW_2026-05-09.md`
- `TODO_OPTIMIZATION_PLAN_2026-05-22.md`
- `System_Monitor模块梳理.md`
- `STM32外设配置驱动官方资料审计_2026-05-17.md`
- `STM32官方外设配置学习与检查手册_2026-05-17.md`
- `出厂老化模式与时基说明.md`
- `系统时钟系统梳理.md`
- `运行架构与时基重构方案.md`
- `项目全模块外设功能驱动审计_2026-05-17.md`
- `项目宏定义梳理.md`
- `项目逻辑完整梳理与架构简化建议_2026-05-24.md`

### Duplicate / temp

- `TEST_PENDING copy.md`

## 6. NEED_CONFIRM：暂不删除文档

### 被 `tools/project_check.py` 固定引用

这些旧文档当前仍是自动检查脚本的输入。为避免破坏现有验证链，本轮不删除。

| 文档 | 暂不删除原因 | 后续建议 |
|---|---|---|
| `项目运行流程与时序源码梳理_2026-05-16.md` | `FLOW_DOC` 固定引用 | 迁移脚本到 `docs/architecture.md` |
| `COMMUNICATION_ADDRESS_INDEX.md` | 通信地址检查引用 | 迁移到 `docs/protocol/modbus_register_map.md` |
| `CAN_RUNTIME_REFACTOR.md` | CAN runtime 检查引用 | 迁移到 `docs/protocol/can_protocol.md` |
| `CAN_MODULE_SIMPLIFY_2026-05-15.md` | CAN 模块检查引用 | 迁移到 `docs/protocol/can_protocol.md` |
| `PROJECT_ARCH_REFACTOR_2026-05-22.md` | 架构检查引用 | 迁移到 `docs/architecture.md` |
| `PROJECT_REFACTOR_REQUIREMENTS_2026-05-22.md` | 重构需求检查引用 | 迁移到 `docs/review/refactor_plan.md` |
| `HEAT_COOL_IODRIVERS_REMOVAL_2026-05-22.md` | 清理检查引用 | 迁移到 `docs/review/full_project_review.md` |
| `UNUSED_SYMBOL_CLEANUP_2026-05-22.md` | 清理检查引用 | 迁移到 `docs/review/full_project_review.md` |
| `RTC_STANDBY_SLEEP_OPTIMIZATION_2026-05-22.md` | 低功耗检查引用 | 迁移到 `docs/design/low_power_design.md` |
| `docs/RTC_SLEEP_PORT_REFACTOR_2026-05-25.md` | 低功耗检查引用 | 迁移到 `docs/design/low_power_design.md` |
| `docs/COMM_TOOL_CAN_IAP_ARCHITECTURE_2026-05-22.md` | comm tool/IAP 检查引用 | 迁移到 `docs/design/bootloader_iap_design.md` |
| `docs/COMM_TOOL_SERIAL_PROTOCOL.md` | comm tool 串口协议检查引用 | 迁移到 `docs/protocol/uart_protocol.md` 或保留为工具协议专文 |
| `docs/BMS_CAN_SERVICE_PROTOCOL.md` | CAN service 检查引用 | 迁移到 `docs/protocol/can_protocol.md` |
| `docs/CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md` | 老化/SOC 检查引用 | 迁移到 `docs/protocol/can_protocol.md` |
| `docs/COMM_TOOL_UPGRADE_UI_2026-05-23.md` | 上位机 UI 检查引用 | 后续整理为上位机专文 |
| `docs/BMS_CAN_IAP_PROTOCOL.md` | CAN-IAP 检查引用 | 迁移到 `docs/design/bootloader_iap_design.md` |
| `docs/BMS_CAN_IAP_RELIABILITY_STATUS_2026-05-22.md` | CAN-IAP 检查引用 | 迁移到 `docs/design/bootloader_iap_design.md` |
| `docs/BMS_SERIAL_IAP_REFACTOR_2026-05-22.md` | 串口 IAP 检查引用 | 迁移到 `docs/design/bootloader_iap_design.md` |
| `docs/COMM_TOOL_F103RET6_KEIL_PORT_2026-05-23.md` | comm tool Keil 检查引用 | 后续整理到 comm tool 独立文档 |
| `docs/COMM_TOOL_UART_SELECT_2026-05-25.md` | comm tool UART 检查引用 | 后续整理到 comm tool 独立文档 |

### 可能仍有流程或业务价值

| 文档 | 暂不删除原因 |
|---|---|
| `AGENTS.md` | 仓库协作规则和烧录安全规则 |
| `README.md` | 根目录入口 |
| `BMS_DAILY_DEV_WORKFLOW.md` | 日常开发流程，是否并入 docs 需确认 |
| `项目协作与发布检查清单.md` | 发布/协作流程入口，是否并入 docs 需确认 |
| `项目自动化检查与发布流程.md` | 自动化流程文档，是否并入 docs 需确认 |
| `DEBUG_WATCH_GUIDE.md` | 调试观察变量指南 |
| `RUNTIME_FACTORY_AGING_REFACTOR_2026-05-15.md` | 老化逻辑可能仍有业务价值 |
| `TEST_PENDING.md` | 待测事项入口 |
| `TODO.md` | 项目待办入口 |
| `docs/MAP文件阅读与项目体积优化指南.md` | map/体积优化方法文档 |
| `docs/feidao-can-protocol-v1.6-analysis.md` | 外部协议分析参考 |
| `firmware/comm_tool_f103ret6/source/app/README.md` | comm tool 子工程说明 |
| `tools/codex-skills/bms-soc-module-optimizer/SKILL.md` | Codex skill 文档 |
| `项目变量梳理.md` | 当前工作区已有用户修改，本轮不处理 |
| `103 + 309/Project/Source/todo.md` | 源码目录内待办，是否保留需用户确认 |

## 7. 后续建议

1. 先确认 `NEED_CONFIRM` 文档是否继续保留。
2. 再把 `tools/project_check.py` 中的旧文档依赖迁移到新的权威文档。
3. 迁移完成后，再删除剩余旧阶段文档。
4. 后续代码重构时，只维护 `docs/README.md` 中列出的权威文档。

# BMS 项目文档索引

> 项目: 103-309 BMS (STM32F103 + SH367309)
> 最后更新: 2026-06-03
> 原则: 源码是第一可信来源，文档与源码冲突时以源码为准。

---

## 📖 快速导航

| 我想... | 看这里 |
|---------|--------|
| 了解项目整体情况 | [项目总览](reference/project_overview.md) |
| 查看某个模块的完整功能 | [模块完整参考](reference/module_reference.md) |
| 查找全局变量 | [全局变量清单](reference/global_variables.md) |
| 查找宏配置 | [宏配置参考](reference/macro_config_reference.md) |
| 了解模块与文件对应关系 | [模块地图](reference/module_map.md) |
| 查找 Modbus 寄存器地址 | [通讯地址索引](reference/COMMUNICATION_ADDRESS_INDEX.md) |
| 了解软件架构设计 | [架构文档](reference/architecture.md) |
| 查看 CAN/Modbus 协议 | [协议文档](#7-协议文档-protocol) |
| 查看某模块的设计方案 | [设计文档](#6-设计文档-design) |
| 查看变更记录 | [变更日志](changelog/change_log.md) |
| 查看本轮状态变量净删减审计 | [状态变量净删减专项审计](review/state_variable_audit.md) |
| 查看当前 SOC 完整逻辑和校准条件 | [SOC 模块设计与源码审查](design/soc_design.md) |
| 排查无放电静置 SOC 快降 | [SOC 无放电静置快降分析](review/soc_rest_fast_drop_analysis_2026-06-03.md) |
| 查看 SOC 源码简化执行记录 | [SOC 简化执行记录](review/soc_simplification_candidates_2026-06-02.md) |
| 查看历史开发记录 | [开发日志](#8-开发日志-devlog) |
| 📟 Keil Watch 调试 | [SystemDebug 指南](guides/SYSTEM_DEBUG_GUIDE.md) |
| 找操作指南 | [使用指南](#9-使用指南-guides) |
| 了解量产分支变更 | [量产变更报告](changelog/production_release_changelog.md) |
| 了解量产需清理什么 | [量产清理分析](reference/production_cleanup_analysis.md) |

---

## 📁 目录结构

```
docs/
├── INDEX.md                    ← 你在这里
├── README.md                   ← 文档体系说明
│
├── reference/                  ← 核心参考文档（长期维护）
│   ├── project_overview.md         项目总览
│   ├── module_reference.md         各模块完整功能/逻辑/变量
│   ├── macro_config_reference.md   所有宏配置及派生关系
│   ├── global_variables.md         全局变量完整清单
│   ├── module_map.md               模块→文件映射
│   ├── architecture.md             软件架构
│   ├── COMMUNICATION_ADDRESS_INDEX.md  Modbus 寄存器地址索引
│   ├── production_cleanup_analysis.md  量产分支清理分析
│   └── 项目变量梳理.md              项目变量梳理(中文)
│
├── design/                     ← 模块设计文档
│   ├── adc_afe_design.md           ADC/AFE 设计
│   ├── bootloader_iap_design.md    IAP/Bootloader 设计
│   ├── led_display_design.md       LED 数码管设计
│   ├── low_power_design.md         低功耗设计
│   ├── protocol_design.md          协议设计
│   ├── soc_design.md               SOC 算法设计
│   └── storage_design.md           Flash 存储设计
│
├── protocol/                   ← 协议文档
│   ├── uart_protocol.md            UART/RS485 协议
│   ├── modbus_register_map.md      Modbus 寄存器映射
│   ├── can_protocol.md             CAN 飞道协议
│   ├── feidao-can-protocol-v1.6-analysis.md  飞道 CAN v1.6 分析
│   ├── BMS_CAN_IAP_PROTOCOL.md     CAN IAP 升级协议
│   ├── BMS_CAN_SERVICE_PROTOCOL.md CAN 服务协议
│   └── COMM_TOOL_SERIAL_PROTOCOL.md  Comm Tool 串口协议
│
├── changelog/                  ← 变更记录
│   └── change_log.md              文档变更日志
│
├── devlog/                     ← 开发日志（历史记录，不再更新）
│   ├── CAN_MODULE_SIMPLIFY_2026-05-15.md
│   ├── CAN_RUNTIME_REFACTOR.md
│   ├── RUNTIME_FACTORY_AGING_REFACTOR_2026-05-15.md
│   ├── HEAT_COOL_IODRIVERS_REMOVAL_2026-05-22.md
│   ├── PROJECT_ARCH_REFACTOR_2026-05-22.md
│   ├── PROJECT_REFACTOR_REQUIREMENTS_2026-05-22.md
│   ├── RTC_STANDBY_SLEEP_OPTIMIZATION_2026-05-22.md
│   ├── UNUSED_SYMBOL_CLEANUP_2026-05-22.md
│   ├── BMS_CAN_IAP_RELIABILITY_STATUS_2026-05-22.md
│   ├── BMS_SERIAL_IAP_REFACTOR_2026-05-22.md
│   ├── COMM_TOOL_* (5篇 Comm Tool 开发记录)
│   ├── CAN_APP_LOW_POWER_OPTIMIZATION_2026-05-27.md
│   ├── CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md
│   ├── LEDBAR_CHARGE_FLICKER_FIX_2026-05-27.md
│   ├── RTC_GPIO_SW_WAKE_LED_DISPLAY_2026-05-27.md
│   ├── RTC_SLEEP_PORT_REFACTOR_2026-05-25.md
│   ├── STLINK_BMS_MONITOR_2026-05-27.md
│   ├── 项目运行流程与时序源码梳理_2026-05-16.md
│   ├── RTC_LOW_POWER_LOGIC_2026-05-25.html
│   ├── COMM_TOOL_BMS_APP_REVIEW_LOGGING_2026-05-25.html
│   └── 项目逻辑完整梳理与架构简化建议_2026-05-24.html
│
├── guides/                     ← 使用指南
│   ├── BMS_DAILY_DEV_WORKFLOW.md   日常开发工作流
│   ├── DEBUG_WATCH_GUIDE.md        Keil Watch 调试指南
│   ├── MAP文件阅读与项目体积优化指南.md  MAP 文件优化
│   ├── TEST_PENDING.md             待测试项
│   ├── 项目协作与发布检查清单.md
│   └── 项目自动化检查与发布流程.md
│
├── review/                     ← 审查文档（历史审查记录）
│   ├── full_project_review.md      全项目审查
│   ├── requirement_confirmation.md 需求确认
│   ├── requirement_questions.md    待确认问题
│   ├── risk_list.md                风险清单
│   ├── refactor_plan.md            重构计划
│   ├── test_plan.md                测试计划
│   ├── document_cleanup_report.md  文档清理报告
│   ├── document_inventory.md       文档盘点
│   ├── document_duplicate_analysis.md 重复文档分析
│   ├── document_structure_plan.md  文档结构方案
│   ├── document_merge_plan.md      文档合并计划
│   ├── document_source_consistency.md 文档源码一致性
│   ├── variable_cleanup_report.md  变量清理报告
│   ├── state_variable_audit.md     状态变量净删减专项审计
│   ├── soc_current_logic_2026-06-02.md SOC 当前逻辑历史归档，已合并至 design/soc_design.md
│   ├── soc_rest_fast_drop_analysis_2026-06-03.md SOC 无放电静置快降分析
│   ├── soc_simplification_candidates_2026-06-02.md SOC 源码简化执行记录
│   ├── bms_app_io_low_power_compare_2026-05-27.md  IO 低功耗对比
│   ├── rtc_sleep_low_power_requirement_confirmation_2026-05-27.md
│   └── rtc_sleep_low_power_research_2026-05-27.md
│
├── test/                       ← 测试
│   └── test_plan.md               测试计划
│
└── archive/                    ← 归档（已过时文档）
    └── README.md
```

---

## 1. 核心参考文档 (reference/)

### 项目总览
- [project_overview.md](reference/project_overview.md) — 项目定位、硬件平台、软件架构、模块一览、数据流、文档索引

### 模块完整参考 (2026-06-01 新建)
- [module_reference.md](reference/module_reference.md) — **所有 20 个模块**的完整功能描述,包含:
  - 架构说明、全局变量、所有函数及功能、核心逻辑流程
  - Flash 地址映射、中断/外设资源分配
  - SOC/CAN/ADC/LED/RTC/AFE/Flash/FactoryAging 等详细分析

### 宏配置
- [macro_config_reference.md](reference/macro_config_reference.md) — 100+ 宏定义速查表:
  - 构建配置、硬件产品、功能开关、唤醒源、CAN/SOC/LED 配置
  - Project_Config.h → conf.h 派生关系表
  - Release 构建强制约束

### 全局变量
- [global_variables.md](reference/global_variables.md) — 80+ 全局变量完整清单:
  - 15 个分类，每个变量的类型、结构体字段、功能说明
  - 备份域 10 个 BKP 寄存器用途

### 模块地图
- [module_map.md](reference/module_map.md) — 模块→文件映射、数据流、中断使用、Flash 地址

### 架构
- [architecture.md](reference/architecture.md) — 软件分层、主循环、任务调度

### 通讯地址
- [COMMUNICATION_ADDRESS_INDEX.md](reference/COMMUNICATION_ADDRESS_INDEX.md) — Modbus 寄存器地址完整索引

### 量产清理
- [production_cleanup_analysis.md](reference/production_cleanup_analysis.md) — 量产分支可删除项分析 (40项, 18项确认清单)

### 其他
- [项目变量梳理.md](reference/项目变量梳理.md) — 中文变量梳理

---

## 2. 模块设计文档 (design/)

| 文档 | 模块 | 内容 |
|------|------|------|
| [soc_design.md](design/soc_design.md) | SOC | SOC 当前权威设计、源码 review、校准策略、风险和测试入口 |
| [adc_afe_design.md](design/adc_afe_design.md) | ADC/AFE | 数据流、采样、校准 |
| [low_power_design.md](design/low_power_design.md) | 低功耗 | RTC/STOP/IWDG/唤醒/阻塞逻辑 |
| [storage_design.md](design/storage_design.md) | 存储 | Flash/EEPROM 兼容层/参数存储 |
| [protocol_design.md](design/protocol_design.md) | 协议 | Modbus/CAN/UART 通信设计 |
| [led_display_design.md](design/led_display_design.md) | LED | Charlieplexing 显示、按键、休眠 |
| [bootloader_iap_design.md](design/bootloader_iap_design.md) | IAP | Bootloader/IAP 地址和升级流程 |

---

## 3. 协议文档 (protocol/)

| 文档 | 协议 |
|------|------|
| [uart_protocol.md](protocol/uart_protocol.md) | UART/RS485 Modbus 协议入口 |
| [modbus_register_map.md](protocol/modbus_register_map.md) | Modbus 寄存器地址映射 |
| [can_protocol.md](protocol/can_protocol.md) | CAN 飞道周期广播和应用命令 |
| [feidao-can-protocol-v1.6-analysis.md](protocol/feidao-can-protocol-v1.6-analysis.md) | 飞道 CAN v1.6 协议分析 |
| [BMS_CAN_IAP_PROTOCOL.md](protocol/BMS_CAN_IAP_PROTOCOL.md) | CAN IAP 升级协议 |
| [BMS_CAN_SERVICE_PROTOCOL.md](protocol/BMS_CAN_SERVICE_PROTOCOL.md) | CAN 服务协议 |
| [COMM_TOOL_SERIAL_PROTOCOL.md](protocol/COMM_TOOL_SERIAL_PROTOCOL.md) | Comm Tool 串口协议 |

---

## 4. 变更日志 (changelog/)

- [change_log.md](changelog/change_log.md) — 2026-05-26 至今的文档/源码变更
- [simplification_changelog.md](changelog/simplification_changelog.md) — 2026-06-01 项目优化简化 (删 6 文件, 9 函数, 12 字段)
- [production_release_changelog.md](changelog/production_release_changelog.md) — 量产分支清理 (~5500 行删除)

---

## 5. 开发日志 (devlog/)

历史开发记录，按日期排列。这些是一次性的任务记录，完成后不再更新，保留原因是有代码变更上下文可供追溯。

**2026-05-27:**
- [CAN_APP_LOW_POWER_OPTIMIZATION_2026-05-27.md](devlog/CAN_APP_LOW_POWER_OPTIMIZATION_2026-05-27.md)
- [LEDBAR_CHARGE_FLICKER_FIX_2026-05-27.md](devlog/LEDBAR_CHARGE_FLICKER_FIX_2026-05-27.md)
- [RTC_GPIO_SW_WAKE_LED_DISPLAY_2026-05-27.md](devlog/RTC_GPIO_SW_WAKE_LED_DISPLAY_2026-05-27.md)
- [STLINK_BMS_MONITOR_2026-05-27.md](devlog/STLINK_BMS_MONITOR_2026-05-27.md)

**2026-05-25:**
- [CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md](devlog/CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md)
- [COMM_TOOL_UART_SELECT_2026-05-25.md](devlog/COMM_TOOL_UART_SELECT_2026-05-25.md)
- [RTC_SLEEP_PORT_REFACTOR_2026-05-25.md](devlog/RTC_SLEEP_PORT_REFACTOR_2026-05-25.md)

**2026-05-23:**
- [COMM_TOOL_F103RET6_KEIL_PORT_2026-05-23.md](devlog/COMM_TOOL_F103RET6_KEIL_PORT_2026-05-23.md)
- [COMM_TOOL_UPGRADE_UI_2026-05-23.md](devlog/COMM_TOOL_UPGRADE_UI_2026-05-23.md)

**2026-05-22:**
- [BMS_CAN_IAP_RELIABILITY_STATUS_2026-05-22.md](devlog/BMS_CAN_IAP_RELIABILITY_STATUS_2026-05-22.md)
- [BMS_SERIAL_IAP_REFACTOR_2026-05-22.md](devlog/BMS_SERIAL_IAP_REFACTOR_2026-05-22.md)
- [COMM_TOOL_CAN_IAP_ARCHITECTURE_2026-05-22.md](devlog/COMM_TOOL_CAN_IAP_ARCHITECTURE_2026-05-22.md)
- [HEAT_COOL_IODRIVERS_REMOVAL_2026-05-22.md](devlog/HEAT_COOL_IODRIVERS_REMOVAL_2026-05-22.md)
- [PROJECT_ARCH_REFACTOR_2026-05-22.md](devlog/PROJECT_ARCH_REFACTOR_2026-05-22.md)
- [PROJECT_REFACTOR_REQUIREMENTS_2026-05-22.md](devlog/PROJECT_REFACTOR_REQUIREMENTS_2026-05-22.md)
- [RTC_STANDBY_SLEEP_OPTIMIZATION_2026-05-22.md](devlog/RTC_STANDBY_SLEEP_OPTIMIZATION_2026-05-22.md)
- [UNUSED_SYMBOL_CLEANUP_2026-05-22.md](devlog/UNUSED_SYMBOL_CLEANUP_2026-05-22.md)

**2026-05-16:**
- [项目运行流程与时序源码梳理_2026-05-16.md](devlog/项目运行流程与时序源码梳理_2026-05-16.md)

**2026-05-15:**
- [CAN_MODULE_SIMPLIFY_2026-05-15.md](devlog/CAN_MODULE_SIMPLIFY_2026-05-15.md)
- [RUNTIME_FACTORY_AGING_REFACTOR_2026-05-15.md](devlog/RUNTIME_FACTORY_AGING_REFACTOR_2026-05-15.md)

**HTML 分析报告:**
- [RTC_LOW_POWER_LOGIC_2026-05-25.html](devlog/RTC_LOW_POWER_LOGIC_2026-05-25.html)
- [COMM_TOOL_BMS_APP_REVIEW_LOGGING_2026-05-25.html](devlog/COMM_TOOL_BMS_APP_REVIEW_LOGGING_2026-05-25.html)
- [项目逻辑完整梳理与架构简化建议_2026-05-24.html](devlog/项目逻辑完整梳理与架构简化建议_2026-05-24.html)

---

## 6. 使用指南 (guides/)

| 文档 | 内容 |
|------|------|
| [BMS_DAILY_DEV_WORKFLOW.md](guides/BMS_DAILY_DEV_WORKFLOW.md) | 日常开发工作流 |
| [DEBUG_WATCH_GUIDE.md](guides/DEBUG_WATCH_GUIDE.md) | Keil Watch 调试指南 |
| [MAP文件阅读与项目体积优化指南.md](guides/MAP文件阅读与项目体积优化指南.md) | MAP 文件阅读和体积优化 |
| [TEST_PENDING.md](guides/TEST_PENDING.md) | 待测试项清单 |
| [项目协作与发布检查清单.md](guides/项目协作与发布检查清单.md) | 协作与发布检查 |
| [项目自动化检查与发布流程.md](guides/项目自动化检查与发布流程.md) | 自动化检查与发布 |
| [SYSTEM_DEBUG_GUIDE.md](guides/SYSTEM_DEBUG_GUIDE.md) | SystemDebug Keil Watch 调试监控 |

---

## 7. 审查文档 (review/)

历史审查记录和需求确认，用于追溯设计决策。

| 文档 | 内容 |
|------|------|
| [full_project_review.md](review/full_project_review.md) | 全项目审查结论 |
| [requirement_confirmation.md](review/requirement_confirmation.md) | 从源码反推的需求清单 |
| [requirement_questions.md](review/requirement_questions.md) | 待用户确认的问题 |
| [risk_list.md](review/risk_list.md) | 风险清单 |
| [refactor_plan.md](review/refactor_plan.md) | 分阶段重构计划 |
| [test_plan.md](review/test_plan.md) | 审查测试计划 |
| [document_cleanup_report.md](review/document_cleanup_report.md) | 文档清理报告 |
| [variable_cleanup_report.md](review/variable_cleanup_report.md) | 变量清理报告 |

---

## 8. 根目录文档

| 文档 | 作用 |
|------|------|
| `README.md` | 项目 README |
| `AGENTS.md` | AI Agent 行为规范 |
| `TODO.md` | 开发 TODO (活跃更新) |

---

## 9. 子工程文档

| 路径 | 项目 |
|------|------|
| `firmware/comm_tool_f103ret6/source/app/README.md` | Comm Tool 固件 |
| `tools/codex-skills/bms-soc-module-optimizer/SKILL.md` | Codex Skill |
| `103 + 309/Project/Source/todo.md` | BMS App TODO |

---

> **维护规则**: 修改源码涉及协议、Flash 布局、IAP、低功耗、SOC、AFE/MOS 时，同步更新 `reference/` 和 `design/` 下的对应文档。
> **文档状态**: 所有 `reference/` 下文档基于 2026-06-01 源码分析，`devlog/` 下为历史记录不再更新。

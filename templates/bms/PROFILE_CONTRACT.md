# BMS 模板 Profile 契约

## 目的

后续项目配置生成器只能读取 profile 和模板源生成新 Keil 工程，不能临时猜测 MCU、AFE、保护策略、Flash 地址或可选功能。本文档记录模板库必须长期遵守的约束。

应用层模板基线固定为当前 `103 + 309` 项目。旧 A002 项目只作为 `STM32F030 + BQ76940` 的 port 参考源，不允许把旧应用层 SOC、低功耗、保护和通信流程作为通用模板主线。

## Profile 必填字段

| 字段 | 含义 |
| --- | --- |
| `mcu_family` | MCU 系列，例如 `stm32f103`、`stm32f030`。 |
| `mcu_driver` | 驱动库口径，例如 `stm32f10x_stdperiph`、`stm32f0xx_stdperiph`。 |
| `afe_type` | AFE 型号或家族，例如 `sh367309`、`bq76940`。 |
| `board_profile` | 板级 profile 名称。 |
| `protection_mode` | `afe_hardware_only`、`mcu_software` 或 `hybrid`。 |
| `storage_backend` | 当前模板只允许 `internal_flash`。 |
| `template_role` | `canonical_application_baseline` 或 `mcu_afe_driver_reference`。 |
| `application_logic_source` | 当前固定为 `current_project`。 |
| `keil_project` | 模板 Keil 工程路径。 |
| `keil_targets` | 可生成或验证的 Keil Target 列表。 |
| `flash` | `flash_start`、`flash_size`、`iap_start`、`app_start`、`app_size`、`storage_start`、`storage_size`。 |

## 强制规则

1. F0/F1 不能共用同一个 Keil Target。一个 `.uvprojx` 可以有多个 Target，但每个 Target 必须独立设置 MCU、startup、StdPeriph、scatter、宏和 Flash Download 算法。
2. App 区不能覆盖 Storage 区，Storage 区不能越过 profile 声明的 Flash 末尾。
3. IAP、App、Storage 地址不能跨 profile 复用。
4. `fd_103_309` 使用 `STM32F103 + SH367309 + afe_hardware_only + internal_flash`。
5. `a002_f030_bq76940` 使用 `STM32F030 + BQ76940 + mcu_software + internal_flash`，但只作为 MCU/AFE port 参考，不作为应用层模板基线。
6. 旧外部 EEPROM 已废除，A002 模板不得再出现 24xx EEPROM 驱动、PB3/PB4 软件 I2C、PA15 WP 或外部 EEPROM 条件分支。
7. 通用模板应用层必须来自当前项目；移植 F0/BQ76940 或其他组合时，只替换 MCU port、AFE port、保护 owner 和配置头。
8. 可选功能必须通过 profile 和 feature gate 管理。默认不需要的 `CAN`、`LEDBAR`、`HEAT`、客户串口等功能不能在生成器中默认打开。
9. 源码和文档统一 UTF-8。Keil 工程文件必须保持 Keil 可打开，不允许为了脚本方便破坏 `.uvprojx` 格式。

## 源码边界

| 边界 | 当前文件 |
| --- | --- |
| Target/MCU/AFE | `Project_Target.h` |
| 保护 owner | `Project_Protection.h` |
| 功能裁剪 | `Project_Features.h` |
| 产品配置 | `Project_Template_Config.h` 或当前项目 `Project_Config.h` |
| 地址真相源 | `templates/bms/target_profiles.json` |
| 自动验收 | `tools/project_check.py` |

## 生成器原则

- 生成器只复制 profile 明确允许的模板源。
- 生成器必须先输出 dry-run 报告，再生成工程文件。
- `generate` 只允许 `template_role=canonical_application_baseline` 的 profile；`mcu_afe_driver_reference` 只能 dry-run，不能直接物化为应用工程。
- 生成器必须在报告里列出 IAP、App、Storage 地址、Keil Target、scatter、startup、Flash Download 算法和保护模式。
- 生成后必须运行 `tools/project_check.py` 或生成项目自己的检查脚本。

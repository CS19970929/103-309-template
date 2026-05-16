# BMS 通用模板库

本目录先维护稳定通用模板源，再做项目配置生成器。生成器后续只能读取这里的 profile 和源码池，不能临时拼接 MCU、AFE、Flash 地址或 Keil 配置。

当前原则已经固定：应用层逻辑直接使用当前 `103 + 309` 项目作为唯一基线；旧 A002 项目只作为 `STM32F030` MCU 驱动、`BQ76940` AFE 驱动、Keil/F0 地址和硬件资源参考，不继承旧应用层流程。

生成器和模板源的强制约束见 [PROFILE_CONTRACT.md](PROFILE_CONTRACT.md)。
通用模板应用层架构口径见 [GENERIC_TEMPLATE_ARCHITECTURE_2026-05-16.md](GENERIC_TEMPLATE_ARCHITECTURE_2026-05-16.md)。
A002 F0/BQ76940 的可用 port 参考见 [PORT_REFERENCE_A002_F030_BQ76940.md](PORT_REFERENCE_A002_F030_BQ76940.md)。

## 配置器入口

Mac / VSCode / Codex 环境：

```bash
python3 tools/bms_template_configurator.py list
python3 tools/bms_template_configurator.py dry-run --profile fd_103_309 --name demo_fd_103_309
python3 tools/bms_template_configurator.py generate --profile fd_103_309 --name demo_fd_103_309 --output generated/bms_projects
```

Windows / Keil 日常环境：

```powershell
.\tools\bms_template_configurator.ps1 list
.\tools\bms_template_configurator.ps1 dry-run --profile a002_f030_bq76940 --name demo_f0_bq76940
```

当前 `generate` 只允许 `fd_103_309` 这种 `canonical_application_baseline` profile。`a002_f030_bq76940` 仍是 port reference，只能 dry-run，直到 F0 MCU port 和 BQ76940 AFE port 完成物化规则。

## 当前 profile 角色

| Profile | MCU | AFE | 角色 | 存储策略 | 状态 |
| --- | --- | --- | --- | --- | --- |
| `fd_103_309` | `STM32F103` | `SH367309` | 当前项目应用层基线 | 内部 Flash | 通用模板主线 |
| `a002_f030_bq76940` | `STM32F030` | `BQ76940` | F0/BQ76940 port 参考 | 内部 Flash，外部 EEPROM 已废除 | 只参考驱动、地址、Keil 配置，不参考旧应用层 |

## 地址强约束

| Profile | IAP | App | App Size | Storage |
| --- | --- | --- | --- | --- |
| `fd_103_309` | `0x08000000` | `0x08004800` | `0x00017800` | `0x0801C000..0x0801FFFF` |
| `a002_f030_bq76940` | `0x08000000` | `0x08001C00` | `0x0000C400` | `0x0800E000..0x0800FFFF` |

这些地址写在 [target_profiles.json](target_profiles.json) 中，并由 `tools/project_check.py` 校验。后续项目配置器必须以该文件为真相源：

- 生成 Keil Target 时必须写入对应 MCU、startup、StdPeriph、scatter、Flash Download 算法和宏。
- F0/F1 不能共用同一个 Target。
- App 区不能覆盖 Storage 区。
- IAP 起始地址、App 起始地址、Storage 起始地址不能跨 profile 混用。

## 当前改造顺序

1. 稳定 `fd_103_309`：作为应用层通用模板基线，继续收口硬件保护、SOC、CAN、低功耗、Flash 存储和 feature gate。
2. 整理 `a002_f030_bq76940`：只保留为 F0/BQ76940 port 参考，外部 EEPROM 完全废除，地址、scatter、Keil 和驱动资源要可查可用。
3. 在当前项目应用层基线之上抽象 MCU port、AFE port、保护 owner 和可选功能。
4. 先用配置器 dry-run 审核 profile；当前只对 canonical profile 物化工程副本。
5. F0/BQ76940 port 层完成后，再开放对应 profile 的 Keil 工程生成。

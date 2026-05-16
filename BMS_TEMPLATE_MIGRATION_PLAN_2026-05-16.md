# BMS 通用模板库与项目配置器执行规划

## 总目标

把当前仓库维护成可长期复用的 BMS 通用模板源库：应用层逻辑以当前 `STM32F103 + SH367309` 项目为唯一基线；旧项目 `/Users/cs/Downloads/work/todo/a002-c022-c057` 只作为 `STM32F030` MCU 驱动、`BQ76940` AFE 驱动、Keil/F0 地址和历史硬件资源的移植参考，不再继承旧应用层 SOC、低功耗、保护和通信流程。

核心顺序固定为：先稳定模板库，再做项目配置生成器。

## 需求边界

1. 当前 F103 项目不能被模板化改造破坏出货路径，`FD_Release` 仍保持量产 profile。
2. 旧 F0 项目已纳入当前仓库，但定位改为驱动/地址/硬件参考源；其应用层逻辑不作为通用模板依据。
3. 通用模板应用层直接来自当前项目：运行时序、SOC、CAN、低功耗、存储、显示、保护配置入口都以当前项目源码为准。
4. 软硬件保护要配置清楚：当前 `SH367309` 项目以 AFE 硬件保护为主；后续 `BQ76940` 或其他 AFE 可通过 profile 切换为 MCU 软件保护或 hybrid，但应用层框架仍沿用当前项目模板。
5. F0/F1 的 Flash、IAP、App、Storage 地址必须分 profile 固化，不能靠手工记忆。
6. F0/F1 不能共用同一个 Keil Target；一个 `.uvprojx` 可以包含多个 Target，但每个 Target 必须独立设置 MCU、startup、StdPeriph、scatter、宏和 Flash Download 算法。
7. 不必要、失效、旧构建产物和 `old xxx` 文件不进入模板源。
8. 模板源源码和文档统一 UTF-8，保证 Mac/Codex/VSCode 可维护，同时避免引入 Keil 不兼容的工程格式变化。

## 固定地址表

| Profile | MCU + AFE | 保护策略 | IAP | App | App Size | Storage |
| --- | --- | --- | --- | --- | --- | --- |
| `fd_103_309` | `STM32F103 + SH367309` | 硬件保护 | `0x08000000` | `0x08004800` | `0x00017800` | `0x0801C000..0x0801FFFF` |
| `a002_f030_bq76940` | `STM32F030 + BQ76940` | 软件保护参考 profile | `0x08000000` | `0x08001C00` | `0x0000C400` | `0x0800E000..0x0800FFFF` |

这些地址以 `templates/bms/target_profiles.json` 为机器可读真相源。`fd_103_309` 同时是应用层模板基线；`a002_f030_bq76940` 是 F0/BQ76940 port 参考 profile，不代表旧 A002 应用层会进入通用模板。

## 分阶段规划

### 阶段 1：模板源纳入与地址安全

- 当前 F103 项目：Keil Target 的 code region 必须止于 `0x0801BFFF`，不能进入 `0x0801C000` 存储区。
- A002 F0 项目：App scatter 必须止于 `0x0800DFFF`，`0x0800E000..0x0800FFFF` 留给内部 Flash 存储。
- `tools/project_check.py` 必须检查两个 profile 的区间不重叠、Keil 配置一致、A002 模板源不含旧文件和构建产物。

### 阶段 2：A002 驱动参考源卫生处理

- 旧项目外部 EEPROM 完全废除，后续模板不再提供外部 EEPROM 后端。
- `EEPROM` 对外接口暂时保留为历史兼容 facade，便于旧参考源可读；实际读写固定映射到内部 Flash。
- 默认存储口径固定为内部 Flash：`PROJECT_CFG_STORAGE_INTERNAL_FLASH = 1`。
- 删除 PB3/PB4/PA15 外部 EEPROM I2C/WP 路径、24xx EEPROM BSP 模板和旧参数草稿，避免后续生成器误用。
- Flash 半字写入必须保留整页数据，不能因为写一个参数擦掉同页其他参数。

### 阶段 3：保护策略统一

- F103 + SH367309：默认 `afe_hardware_only`，软件保护逻辑只做镜像、显示、通信和后续可选兜底。
- F0 + BQ76940：作为 port profile 默认 `mcu_software`，后续在当前项目应用层框架下接入软件保护 owner。
- 后续模板切换只改 profile、配置头和 port 层，不允许在业务模块里散落判断 AFE 型号。

### 阶段 4：通用模板应用层抽象

- 应用层：以当前项目 `103 + 309/Project/Source` 为基线，抽象 runtime、SOC、CAN、低功耗、存储、显示、保护 owner 和通信接口。
- MCU port：把 startup、system clock、interrupt、StdPeriph、Flash/IAP、GPIO/EXTI、CAN/UART/RTC 依赖集中到 `stm32f103` / `stm32f030` profile，不污染应用层。
- AFE port：把 SH367309、BQ76940 等采样、MOS 控制、硬件保护/latch 状态收口到统一接口。
- 保护策略：当前项目硬件保护为主；新 AFE 需要软件保护时，通过 `Project_Protection.h` 和 feature gate 打开软件保护 owner。
- 可选功能：CAN、LEDBar、Heat、Client UART 等都必须有 profile 开关，默认只按当前项目需求打开。

### 阶段 5：生成器设计

- 输入：目标 MCU、AFE、保护策略、通信接口、低功耗需求、存储后端、是否需要 CAN/LED/Heat。
- 处理：读取 `target_profiles.json` 和模板源，不临时猜地址、不复制未知旧文件。
- 输出：基于当前项目应用层模板的新 Keil `.uvprojx`、scatter、配置头、port 源码目录、生成报告和项目检查脚本入口。
- Windows 日常流程：Keil 管理和调试；VSCode 阅读；生成器提供 PowerShell 入口。
- Mac 日常流程：Codex/VSCode 改代码和跑静态检查；不依赖上板调试。
- 当前先提供 `tools/bms_template_configurator.py list/show/dry-run/generate` 和 `tools/bms_template_configurator.ps1`。`generate` 只允许 canonical 应用基线 profile，port reference profile 只允许 dry-run，避免生成错误工程。

## 当前执行状态

- 已纳入 A002 旧项目的可维护源码、Keil 工程、scatter 和文档；定位调整为 F0/BQ76940 port 参考源。
- 已建立 profile 地址表。
- 已把通用模板方向调整为“当前项目应用层基线 + profile port”，并把文档、profile 和检查脚本收口。
- 已实现第一阶段项目配置器：`fd_103_309` 可生成当前项目应用层工程副本；`a002_f030_bq76940` 只输出 dry-run 和 port 参考信息。F0/BQ76940 的真正 Keil 工程物化放在下一阶段，避免在 port 层尚未完全抽象前生成看似完整但不可调试的工程。

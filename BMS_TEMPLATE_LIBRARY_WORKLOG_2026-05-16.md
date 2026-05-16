# BMS 通用模板库改造工作记录

## 需求确认

当前顺序不是先做生成器，而是：

1. 先把旧项目 `/Users/cs/Downloads/work/todo/a002-c022-c057` 纳入当前仓库。
2. 通用模板的应用层直接使用当前 `103 + 309` 项目，不再参考旧 A002 应用层逻辑。
3. 旧项目只整理为 `STM32F030 + BQ76940` 的 MCU/AFE 驱动、Keil/F0 地址和硬件资源参考源。
4. 当前应用层模板基线和 port profile 都稳定后，再做项目配置生成器。

完整执行规划见 [BMS_TEMPLATE_MIGRATION_PLAN_2026-05-16.md](BMS_TEMPLATE_MIGRATION_PLAN_2026-05-16.md)。该文档作为后续防跑偏主线，原则是：先稳定模板库，再做项目配置生成器。

## 已完成

- 已将旧项目按白名单纳入 `templates/bms/sources/a002_f030_bq76940/`。
- 未纳入旧项目 `.git`、`.DS_Store`、`DebugConfig`、Keil 中间产物和旧 `old xxx` 源码。
- 已将模板副本源码转换为 UTF-8，外部旧项目原目录不修改。
- 已新增 `templates/bms/target_profiles.json`，把 F103/F0 的 IAP、App、Storage 地址作为机器可读真相源，并记录 profile 角色。
- 已新增 `templates/bms/README.md`，明确当前项目应用层是通用模板基线，A002 只作为 F0/BQ76940 port 参考。
- A002 旧外部 EEPROM 已完全废除，模板主入口改为 `Storage_Init()` / `Storage_Task()`，旧 `EEPROM` / `E2P` 符号仅作为逻辑地址兼容层保留。
- A002 主循环已接入 `PROJECT_FEATURE_SOC/LOW_POWER/RTC/RS485/HEAT/LEDBAR` 门控，便于后续按 profile 屏蔽不需要的功能。
- A002 文档已修正 `IdleSleep`：当前源码没有 `IdleSleep.c`，低功耗路径以 `SleepDeal` + `RTC` 为准。
- 已新增 `templates/bms/sources/a002_f030_bq76940/docs/A002_REFACTOR_STATUS_2026-05-16.md` 记录 A002 改造状态。
- 已新增 `templates/bms/GENERIC_TEMPLATE_ARCHITECTURE_2026-05-16.md` 和 `templates/bms/PORT_REFERENCE_A002_F030_BQ76940.md`，把当前项目应用层基线与 A002 port 参考边界分开。
- 当前项目 `BoardControl.c` 已从 `main.c` 拆出板级 MOS/工厂模式控制，Keil Release/Debug Target 均已加入。
- 已新增 `tools/bms_template_configurator.py` 和 Windows 入口 `tools/bms_template_configurator.ps1`，提供 profile `list/show/dry-run`，并支持对 canonical profile `fd_103_309` 生成当前项目应用层工程副本。
- `tools/project_check.py` 已把配置器、profile、A002 port 参考源、地址隔离和当前项目应用层基线纳入门禁。

## 地址隔离原则

| Profile | IAP | App | Storage | 说明 |
| --- | --- | --- | --- | --- |
| `fd_103_309` | `0x08000000` | `0x08004800` | `0x0801C000..0x0801FFFF` | 当前 F103 + SH367309，硬件保护主线 |
| `a002_f030_bq76940` | `0x08000000` | `0x08001C00` | `0x0800E000..0x0800FFFF` | 旧 F030 + BQ76940，port 参考 profile |

F0/F1 不能共用同一个 Keil Target。一个 `.uvprojx` 可以有多个 Target，但每个 Target 必须独立设置 MCU、startup、StdPeriph、scatter、宏和 Flash Download 算法。

## 已收口的当前处理项

- A002 模板源旧外部 EEPROM 完全废除，`EEPROM` 历史接口只作为内部 Flash 参数存储兼容层保留。
- A002 App scatter 收口到 `0x08001C00..0x0800DFFF`，`0x0800E000..0x0800FFFF` 保留为内部 Flash 存储区。
- 当前 F103 Keil Target 的 App code region 收口到 `0x08004800..0x0801BFFF`，避免链接器未来把代码放进 `0x0801C000` 存储区。
- 通用模板方向已按用户要求调整为：当前项目应用层为唯一基线，旧 A002 只作为 port 参考源。
- 配置器当前阶段明确应用层来自 `103 + 309/Project/Source`。`generate` 只允许 `canonical_application_baseline` profile；A002 作为 port reference 只能 `dry-run`，防止误生成未完成端口工程。

## 后续任务

1. 继续从当前项目抽象通用应用层边界：runtime、SOC、CAN、低功耗、Storage、保护 owner、显示和通信。
2. 继续梳理 MCU port：F0 StdPeriph 与 F103 StdPeriph 的 startup、Flash、IAP、时基和外设差异。
3. 继续梳理 AFE port：SH367309 与 BQ76940 的采样、MOS 控制、硬件保护/latch 状态接口差异。
4. 形成通用配置头和 generator 输入字段，保证只换 port/profile，不换应用层流程。
5. 下一阶段把 F0/BQ76940 port 层补齐到生成器：只能从当前项目应用层基线复制业务代码，再按 profile 替换 MCU port、AFE port、scatter、配置头和 Keil Target。

# 存储失败可见性与升级清参门禁方案

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`Flash.c`, `Flash.h`, `EEPROM.c`, `UpgradeParamPolicy.h`, `System_Monitor.c`, `System_Monitor.h`, `DataDeal.c`, `app_lowpower.c`, `Project_Config.h`, `FactoryAging.c`, `SocEnhance.c`, `LogRecord.c`
最后更新时间：2026-05-31
未确认事项：`ERROR_EEPROM_STORE` 是否必须成为可见 latch、存储失败是否阻塞低功耗或限制输出、升级策略 `0x0005` 是否用于当前量产包、老化时间 reset 是否允许进入 `running-from-host`、SOC table reset 在当前 runtime table 关闭时是否需要改文档或改策略。

## 1. 本阶段目标

本阶段只做存储和升级策略的源码事实闭环，不修改 `.c/.h`、Flash 地址、数据结构、magic/version/CRC、协议寄存器、烧录脚本或 Keil 工程。

目标是把两个风险讲清楚：

1. `StorageFlash_Save*()` 失败会调用 `ERROR_EEPROM_STORE`，但当前 `System_ERROR_UserCallback()` 对该错误不递增错误位，导致失败可见性不足。
2. `UpgradeParamPolicy_ApplyOnce()` 当前 policy `0x0005` 会重置多类现场数据，其中 factory aging reset 不是单纯清零，而会调用 `FactoryAging_ResetTimeByHost()` 进入 host running 路径。

## 2. 源码确认事实

| 事实 ID | 当前源码事实 | 证据 | 判断 |
|---|---|---|---|
| FACT-STOR-001 | 持久化数据从 `0x0801C000` 起分布在 AFE/RW/log/SOC/upgrade flag/aging/sleep legacy 区域 | `Flash.h:7-29` | MUST_KEEP |
| FACT-STOR-002 | AFE/RW/log/SOC 使用 header、sequence、crc、双槽或 journal page，并在写后做校验 | `Flash.c:473-664` | MUST_KEEP |
| FACT-STOR-003 | 多数存储保存失败会调用 `System_ERROR_UserCallback(ERROR_EEPROM_STORE)` 并返回 0 | `Flash.c:496-660`, `EEPROM.c:165-182`, `FactoryAging.c:300`, `LogRecord.c:295` | MUST_KEEP，但错误语义需确认 |
| FACT-STOR-004 | `System_ERROR_UserCallback()` 明确排除了 `ERROR_EEPROM_STORE` 递增，`ERROR_STATUS_EEPROM_STORE` 可能不会被置位 | `System_Monitor.c:128-148`, `System_Monitor.h:20`, `System_Monitor.h:74` | CHANGE_NEEDED |
| FACT-STOR-005 | 业务代码检查过 `ERROR_STATUS_EEPROM_STORE` 作为 sleep delay 条件，但上述错误链路可能闭不上 | `DataDeal.c:1019-1024` | CHANGE_NEEDED |
| FACT-STOR-006 | 低功耗框架当前只因 `StorageFlash_IsBusy()` 或 `u8FlashUpdateE2PROM` 阻塞 sleep；持久化失败 latch 不是直接阻塞项 | `app_lowpower.c:48-50` | UNKNOWN |
| FACT-UPG-001 | 升级参数策略启用，policy version 是 `0x0005`，force reapply 是 0 | `Project_Config.h:411-451` | MUST_KEEP，发布前确认 |
| FACT-UPG-002 | 当前策略会 reset AFE、保护参数、SOC config、SOC snapshot、event record、factory aging time；不 reset balance open voltage | `Project_Config.h:419-447`, `EEPROM.c:238-287` | UNKNOWN |
| FACT-UPG-003 | `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE` 为 1，但实际动作还受 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE` 编译门控；当前 runtime table 为 0 时不会执行 SOC table reset | `EEPROM.c:255-257`, `Project_Config.h:431` | KEEP_BUT_REFACTOR |
| FACT-UPG-004 | factory aging reset 调用 `FactoryAging_ResetTimeByHost()`，会清 elapsed 并进入 `FactoryAging_EnterRunningFromHost()` | `EEPROM.c:283-287`, `FactoryAging.c:547-555` | UNKNOWN |
| FACT-UPG-005 | policy flag 在所有动作成功后才写入；若中途失败返回 0，下次启动可能重试，已完成的部分动作不会自动回滚 | `EEPROM.c:220-301` | CHANGE_NEEDED |

## 3. 当前缺口

| 缺口 ID | 缺口 | 风险 | 需要确认 |
|---|---|---|---|
| GAP-STOR-001 | 存储保存失败不可见或不可稳定观测 | 参数/SOC/log/aging 写失败后，上位机、日志、低功耗策略可能仍按正常运行处理 | `ERROR_EEPROM_STORE` 是否应置位、是否 latch、是否可清除 |
| GAP-STOR-002 | 存储失败后的系统动作未定义 | 持久化失败可能涉及参数安全、SOC 可用性、售后诊断和低功耗 | 只上报、阻塞低功耗、限制输出、复位或要求工装维修 |
| GAP-UPG-001 | policy `0x0005` 默认清参范围较大 | 升级后现场保护参数、SOC、事件记录、老化状态可能被非预期清空 | 哪些数据当前量产升级包必须保留 |
| GAP-UPG-002 | factory aging reset 的实际动作容易被误读为“只清零” | 当前会进入 host running 路径，可能影响老化流程状态 | 升级清老化是否允许启动/恢复老化 running 状态 |
| GAP-UPG-003 | SOC table reset 文档口径容易冲突 | 宏为 1，但当前 runtime table 关闭时不实际清表 | 文档写“宏开启但当前无动作”，还是调整宏避免误读 |

## 4. 门禁规则

| 门禁 ID | 规则 | 当前阶段动作 |
|---|---|---|
| GATE-STOR-001 | 所有 `StorageFlash_Save*()` 失败必须有可观测状态或明确说明为什么不 latch | 先需求确认，不改代码 |
| GATE-STOR-002 | 存储失败是否阻塞低功耗、限制 MOS/CTLC、触发复位，必须由产品安全策略确认 | 先写确认表 |
| GATE-STOR-003 | 不允许在未确认时修改 Flash 地址、slot 布局、magic/version/CRC、journal 机制 | 本阶段禁止源码修改 |
| GATE-UPG-001 | 每个 upgrade policy version 必须列出 reset/retain/impact/rollback | 从 `0x0005` 开始补文档 |
| GATE-UPG-002 | `PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY` 量产必须保持 0 | 当前源码符合 |
| GATE-UPG-003 | factory aging reset 行为必须按源码写清，不能只写“清零老化时间” | 当前文档同步 |

## 5. 需求确认表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-STOR-ERR-001 | Flash/EEPROM 存储失败必须可见 | `System_Monitor.c:128-148`, `Flash.c:496-660` | `ERROR_EEPROM_STORE` 被保存失败路径调用，但不递增错误位 | P1 | CHANGE_NEEDED | 存储失败是否必须在 `0xD000`/fault/log/LED/CAN 中可见？ | 设置独立 latch 或错误计数，并定义清除条件 | 待确认 |
| REQ-STOR-ERR-002 | 存储失败后的系统动作必须明确 | `DataDeal.c:1019-1024`, `app_lowpower.c:48-50` | 旧 sleep delay 检查存储错误，但低功耗框架只看 busy/pending | P1 | UNKNOWN | 存储失败是否阻塞 STOP、限制输出或要求复位？ | 至少阻塞低功耗并上报，安全动作另行确认 | 待确认 |
| REQ-UPG-POLICY-001 | upgrade policy `0x0005` 清参范围必须被确认 | `Project_Config.h:411-451`, `EEPROM.c:238-301` | 会 reset AFE、保护、SOC config、SOC snapshot、event、aging；不 reset balance | P0/P1 | UNKNOWN | 当前 `0x0005` 是否就是本批量产升级策略？ | 将清参策略与版本/客户/包类型绑定 | 待确认 |
| REQ-UPG-SOC-TABLE-001 | SOC table reset 的宏和实际动作必须不误导 | `EEPROM.c:255-257`, `Project_Config.h:431` | reset 宏为 1，但 runtime table disabled 时无实际动作 | P2 | KEEP_BUT_REFACTOR | 是否保留宏为 1，还是改为 0 以避免误读？ | 文档写清 compile gate，发布前再决定是否改宏 | 待确认 |
| REQ-UPG-AGING-001 | 老化时间 reset 是否允许进入 host running 路径 | `FactoryAging.c:547-555` | reset elapsed 后调用 `FactoryAging_EnterRunningFromHost()` | P1 | UNKNOWN | 升级清老化是否应只清计时，还是允许启动/恢复老化状态？ | 当前不改源码，先确认工厂老化流程 | 待确认 |

## 6. 分阶段计划

| 阶段 | 文件范围 | 动作 | 禁止修改 | 验证 |
|---|---|---|---|---|
| S7-D0 | `docs/review/*`, `docs/design/storage_design.md`, `docs/README.md` | 建立本门禁文档，修正升级/存储口径 | 禁止改源码、Flash 地址和策略宏 | `python3 tools/project_check.py -q`, `git diff --check` |
| S7-D1 | `System_Monitor.c/.h`, protocol/status 文档 | 用户确认后，让 `ERROR_EEPROM_STORE` 可见 | 禁止改变其他 error layout 和协议含义 | Keil 编译、错误注入、`0xD000`/fault 观测 |
| S7-D2 | `app_lowpower.c`, `DataDeal.c`, low-power 文档 | 用户确认后，定义存储失败是否阻塞 sleep/输出 | 禁止顺手调整其他 fault 策略 | STOP/RTC sleep 回归和存储失败注入 |
| S7-D3 | `Project_Config.h`, `EEPROM.c`, upgrade 文档 | 用户确认后，收敛 policy `0x0005` 或拆特殊包 | 禁止无版本号变更直接改清参范围 | 升级前后参数保留矩阵 |
| S7-D4 | `tools/project_check.py`, release checklist | 加只读策略检查和发布报告 | 禁止自动烧录/擦写 | project_check/report |

## 7. 当前验证边界

已完成源码静态核对和文档同步。尚未做：

- Keil Release 编译和 map 验证。
- Flash 写失败注入。
- 断电恢复测试。
- COM4/19200、CAN、ST-Link 或实物板验证。
- 上位机升级流程回归。

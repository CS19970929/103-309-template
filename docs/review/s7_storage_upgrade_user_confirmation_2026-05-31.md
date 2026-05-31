# S7 存储失败与升级清参用户确认包

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Flash.h`, `103 + 309/Project/Source/Flash.c`, `103 + 309/Project/Source/EEPROM.c`, `103 + 309/Project/Source/System_Monitor.c`, `103 + 309/Project/Source/System_Monitor.h`, `103 + 309/Project/Source/app_lowpower.c`, `103 + 309/Project/Source/DataDeal.c`, `103 + 309/Project/Source/conf/Project_Config.h`, `103 + 309/Project/Source/FactoryAging.c`, `docs/review/storage_upgrade_gate_plan_2026-05-31.md`
最后更新时间：2026-05-31
未确认事项：`ERROR_EEPROM_STORE` 是否可见并 latch、存储失败是否阻塞低功耗或限制输出、upgrade policy `0x0005` 是否用于当前量产包、老化 reset 是否允许进入 host running、SOC table reset 宏是否保持当前口径、升级失败后的部分动作是否需要恢复策略。

## 1. 本确认包目标

本确认包只用于确认 S7 存储失败和升级清参策略，不代表已经修复存储风险，也不允许直接修改源码。

确认前禁止改动：

1. Flash 地址、页大小、slot 布局、magic/version/CRC、journal 机制。
2. `FLASH_ADDR_APP_START 0x08004800`、`FLASH_ADDR_IAP_START 0x08000000` 和 `0x0801C000+` 持久化区。
3. `System_ERROR` enum 布局和客户可见错误位含义。
4. upgrade policy version、清参宏和老化流程。
5. 低功耗、MOS/CTLC、协议寄存器、上位机数据含义。

确认后的第一步也只应处理 `ERROR_EEPROM_STORE` 可见性，不应同时调整 Flash 布局、升级策略、低功耗安全动作或老化流程。

## 2. 当前源码事实

| Fact ID | 当前源码事实 | 证据 | 判断 |
|---|---|---|---|
| S7-FACT-001 | IAP 起始 `0x08000000`，App 起始 `0x08004800`；持久化页从 `0x0801C000` 起分布 | `Flash.h:4-29` | MUST_KEEP |
| S7-FACT-002 | AFE/RW/log/SOC 存储使用 header、sequence、crc、slot/journal，并在写后校验 | `Flash.c:473-664` | MUST_KEEP |
| S7-FACT-003 | 存储保存失败路径多处调用 `System_ERROR_UserCallback(ERROR_EEPROM_STORE)` 并返回 0 | `Flash.c:496-660`, `EEPROM.c`, `FactoryAging.c`, `LogRecord.c` | 错误入口存在 |
| S7-FACT-004 | `System_ERROR_UserCallback()` 对 `ERROR_EEPROM_STORE` 明确不递增错误位 | `System_Monitor.c:128-148` | 存储失败可见性不足 |
| S7-FACT-005 | `System_Monitor.h` 存在 `ERROR_STATUS_EEPROM_STORE` 枚举项 | `System_Monitor.h:20`, `System_Monitor.h:74` | 状态位存在但当前链路可能闭不上 |
| S7-FACT-006 | 低功耗当前主要因 `StorageFlash_IsBusy()` 或 `u8FlashUpdateE2PROM` 阻塞，不直接因持久化失败 latch 阻塞 | `app_lowpower.c` | 失败后系统动作未定义 |
| S7-FACT-007 | upgrade policy 启用，version 为 `0x0005`，force reapply 为 0 | `Project_Config.h:411-451` | 发布前必须确认 |
| S7-FACT-008 | 当前 policy reset AFE、保护参数、SOC config、SOC snapshot、event record、factory aging time；不 reset balance open voltage | `Project_Config.h:419-447`, `EEPROM.c:238-287` | 清参范围较大 |
| S7-FACT-009 | SOC table reset 宏为 1，但实际还受 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE` 门控 | `EEPROM.c:255-257`, `Project_Config.h:431` | 容易造成文档误读 |
| S7-FACT-010 | `FactoryAging_ResetTimeByHost()` 清 elapsed 后进入 `FactoryAging_EnterRunningFromHost()`，不是单纯清零 | `FactoryAging.c:547-555` | 老化升级行为必须确认 |
| S7-FACT-011 | policy flag 只有所有动作成功后才写入；失败返回 0，下次启动可能重试，已完成动作不会自动回滚 | `EEPROM.c:220-301` | 部分动作成功后的恢复策略不明确 |

## 3. 用户必须确认的决策

| Decision ID | 需要确认的问题 | 可选方向 | Codex 建议 | User decision placeholder |
|---|---|---|---|---|
| S7-DEC-001 | `ERROR_EEPROM_STORE` 是否必须在 `0xD000`/fault/log/CAN/LED 中可见？ | 可见并 latch / 只内部计数 / 保持现状 | 建议至少可见并定义清除条件，便于售后判断写失败 | 待确认 |
| S7-DEC-002 | 存储失败是否阻塞低功耗？ | 阻塞 STOP/RTC sleep / 只上报不阻塞 / 按数据类型区分 | 建议先阻塞低功耗并上报，MOS/CTLC 动作另行确认 | 待确认 |
| S7-DEC-003 | 存储失败是否限制输出或触发复位？ | 不限制输出 / 限制部分输出 / MCU reset / 要求工装维修 | 建议当前先不动 MOS/CTLC/reset，只补可见性和低功耗边界 | 待确认 |
| S7-DEC-004 | upgrade policy `0x0005` 是否是当前量产升级策略？ | 是 / 仅特殊升级包 / 需要拆新 policy version / 暂停自动清参 | 建议把 policy 绑定到版本/客户/包类型，确认后再动宏 | 待确认 |
| S7-DEC-005 | `0x0005` 是否允许 reset AFE、保护参数、SOC config、SOC snapshot、event record、aging time？ | 全部允许 / 部分保留 / 每类按客户确认 | 建议逐项保留/清除，不扩大当前清参范围 | 待确认 |
| S7-DEC-006 | 老化时间 reset 是否允许进入 host running 路径？ | 允许 / 只清计时不进入 running / 不清老化 / 特殊包处理 | 建议先确认工厂流程；若只想清零，应单独改 `FactoryAging_ResetTimeByHost()` 或新增专用 API | 待确认 |
| S7-DEC-007 | SOC table reset 宏是否保持当前口径？ | 保持宏 1 但文档说明 runtime table disabled 时无动作 / 改为 0 避免误读 / 开启 runtime table 后再处理 | 建议先文档说明 compile gate，发布前再决定是否改宏 | 待确认 |
| S7-DEC-008 | upgrade policy 部分动作成功后失败，是否需要回滚/补偿策略？ | 不回滚下次重试 / 增加分步状态 / 失败后进入维护状态 | 建议不在本阶段改存储格式；先在发布报告中列出部分成功风险 | 待确认 |

## 4. 用户填写模板

| Decision ID | 用户决策 | 备注/约束 |
|---|---|---|
| S7-DEC-001 |  |  |
| S7-DEC-002 |  |  |
| S7-DEC-003 |  |  |
| S7-DEC-004 |  |  |
| S7-DEC-005 |  |  |
| S7-DEC-006 |  |  |
| S7-DEC-007 |  |  |
| S7-DEC-008 |  |  |

## 5. 用户确认后的执行计划

| 阶段 | 文件范围 | 任务 | 禁止改动 | 验证 | 回滚 |
|---|---|---|---|---|---|
| S7-D1 | `System_Monitor.c`, `System_Monitor.h`, protocol/status 文档 | 让 `ERROR_EEPROM_STORE` 可见并定义清除条件 | 不改变其他 error layout、协议字段和状态位顺序 | Keil 编译；错误注入；`0xD000`/fault/CAN 可观测 | 单独 revert S7-D1 commit |
| S7-D2 | `app_lowpower.c`, `DataDeal.c`, 低功耗/存储文档 | 按确认策略处理存储失败是否阻塞 sleep | 不顺手调整其他 fault、MOS、CTLC、wake source | Flash 写失败注入；STOP/RTC sleep 阻塞/恢复一致 | 单独 revert S7-D2 commit |
| S7-D3 | `Project_Config.h`, `EEPROM.c`, upgrade 文档 | 收敛 policy `0x0005` 或拆特殊升级包策略 | 不改 Flash 地址、数据结构、magic/version/CRC | 升级前后 AFE/保护/SOC/event/aging 保留矩阵 | 单独 revert S7-D3 commit |
| S7-D4 | `FactoryAging.c`, `EEPROM.c`, 老化文档 | 如用户确认，调整老化 reset 语义 | 不破坏上位机老化入口和 `0x14F80208` 广播 | 老化 start/stop/reset/set hours、剩余时间 UI 和 CAN 广播回归 | 单独 revert S7-D4 commit |
| S7-D5 | `tools/project_check.py`, release checklist | 加只读 policy/report 检查 | 禁止自动擦写、烧录或修改配置宏 | `project_check.py -q` 和 release report 输出符合预期 | 单独 revert S7-D5 commit |

## 6. 当前验证边界

本确认包只完成源码阅读和文档化确认：

- 未修改 `.c/.h`、Keil 工程、编译宏、Flash 布局、协议行为或烧录脚本。
- 未运行 Keil/ARMCC Release 编译。
- 未做 Flash 写失败注入、断电恢复测试或升级包回归。
- 未连接 COM4、CAN adapter、ST-Link 或实物 BMS 板。
- 未运行原上位机升级/老化 UI 回归。

因此，本文件只能作为 S7 进入源码阶段前的用户确认输入，不能作为存储和升级策略问题已修复的证明。


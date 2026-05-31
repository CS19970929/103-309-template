# 103-309 当前目标与源码事实闭环

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/main.c`, `Runtime.c`, `DataDeal.c`, `Flash.h`, `Flash.c`, `EEPROM.c`, `System_Monitor.c`, `SH367309_DataDeal.c`, `I2C_AFE1.c`, `SH367309_Func.c`, `SleepDeal.c`, `conf/conf.c`, `Can_HDX.c`, `Sci_Upper.c`, `SocEnhance.c`, `FactoryAging.c`, `conf/Project_Config.h`, `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.sct`
最后更新时间：2026-05-31
未确认事项：未执行 Windows/Keil Release 编译，未生成当前 map，未接 COM4/19200、CAN、ST-Link 或实物板验证。

## 1. 当前意图判断

本仓库当前目标不是单点修 bug，也不是立即大规模搬目录，而是把 `103-309-template` 收敛为长期 BMS 工程资产：

1. 源码可信：当前事实以源码和脚本为准，旧文档只作参考。
2. 量产安全：App/IAP 地址、测试模式隔离、IWDG、Flash 存储和烧录入口必须固化。
3. 协议兼容：Modbus、CAN、上位机、IAP、老化、产品信息读取保持外部契约不变。
4. 低复杂度：优先净删减死代码、空实现、重复状态和无用包装，不为架构感增加层次。
5. 可验证、可回滚：每个阶段都有文件范围、禁止项、验证命令、实机条件和回滚点。

当前阶段是“需求澄清 + 文档/风险闭环 + 可执行计划”。除非用户明确批准，不进入固件源码修改。

当前目标完成度和未完成证据以 `docs/review/current_goal_completion_audit_2026-05-31.md` 为准。该文档不是完成声明，而是用于防止把局部文档闭环误判为整体目标完成。

## 2. 当前源码确认事实

| 事实 ID | 当前源码事实 | 证据 | 判断 |
|---|---|---|---|
| FACT-RUN-001 | 主入口仍是 `main -> AppInit_Boot -> Runtime_RunOnce` | `main.c:5-12` | MUST_KEEP |
| FACT-RUN-002 | 主循环按 front、IO/power、background 三段串行执行 | `Runtime.c:53-99` | MUST_KEEP |
| FACT-RUN-003 | AFE/SOC 200ms 主链路在 `App_AFEGet()` 内执行 | `Runtime.c:55-59`, `DataDeal.c:1225-1249` | MUST_KEEP |
| FACT-CUR-001 | 当前 `App_AFEGet()` 已调用真实 `DataLoad_Current()`，`test_Autocurrent_cycle()` 为注释状态 | `DataDeal.c:1238-1239` | 旧文档冲突已确认 |
| FACT-CFG-001 | 当前默认量产 profile 是 `PROJECT_CFG_BUILD_PROFILE 0` | `Project_Config.h:16-18` | MUST_KEEP |
| FACT-CFG-002 | 当前 Host 写寄存器开关为 `PROJECT_CFG_HOST_WRITE_ENABLE 1` | `Project_Config.h:43-45` | 高风险，需确认权限策略 |
| FACT-CFG-003 | 当前老化功能默认启用 | `Project_Config.h:179-181` | 客户可见，默认保留 |
| FACT-CFG-004 | 当前 SOC 测试模式默认关闭 | `Project_Config.h:354-359` | MUST_KEEP |
| FACT-IAP-001 | IAP 地址为 `0x08000000`，App 地址为 `0x08004800` | `Flash.h:4-5` | MUST_KEEP |
| FACT-FLASH-001 | 持久化存储页从 `0x0801C000` 开始 | `Flash.h:7-29` | 高风险，需 map/容量验证 |
| FACT-IAP-002 | App->IAP mailbox 地址为 `0x20004FE0` | `Flash.c:12` | 高风险，需 scatter/map 验证 |

## 3. 已发现的过期文档冲突

| 冲突 ID | 过期结论 | 当前源码事实 | 需要处理 |
|---|---|---|---|
| STALE-CUR-001 | 多份 review 文档写“量产主路径调用 `test_Autocurrent_cycle()`，未调用 `DataLoad_Current()`” | 当前 `DataDeal.c:1238-1239` 是 `DataLoad_Current()` 生效，`test_Autocurrent_cycle()` 注释 | 已在本轮修正关键 review 文档；后续继续清理根目录旧文档 |
| STALE-CUR-002 | `docs/project_overview.md` 把虚拟电流列为最关键风险 | 当前风险应改为“虚拟电流测试入口残留，是否删除/隔离需确认” | 已在本轮修正 |
| STALE-CUR-003 | `docs/review/document_source_consistency.md` 仍把“虚拟电流主路径”列为源码缺失项 | 当前应列为旧文档冲突，而非当前主路径事实 | 已在本轮修正 |

## 4. 当前需求确认焦点

| Requirement ID | 需求描述 | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| Q-GOAL-FLASH-001 | App 链接区必须与后 16KB 持久化区硬隔离 | `CommomSH367309_16series_103RCT6_C.sct:5-13`, `Flash.h:7-29` | scatter 允许 `0x08004800 + 0x20000`，覆盖 `0x0801C000+` 范围；当前缺最终 map | P0 | CHANGE_NEEDED | App 最大结束地址是否必须小于 `0x0801C000`？量产 MCU 容量是多少？ | 先补 map/bin 门禁，确认后再改 scatter | |
| Q-GOAL-IAP-001 | IAP SRAM mailbox 必须被 App scatter 保留 | `Flash.c:12`, `CommomSH367309_16series_103RCT6_C.sct:12-13` | `RW_IRAM1` 覆盖到 `0x20005000`，mailbox 位于尾部 | P0 | CHANGE_NEEDED | IAP 固件是否固定读取 `0x20004FE0`？是否允许保留尾部 RAM？ | 补 map 检查，确认后修 scatter | |
| Q-GOAL-AFE-001 | 主机写 AFE 参数前必须做实时范围校验 | `SH367309_DataDeal.c:249-283` | 写 `curValue` 后保存并置 `AFE_PARAM_WRITE_Flag`，写入口未见 min/max 校验 | P0 | CHANGE_NEEDED | 量产是否允许上位机/CAN 写 AFE 参数？非法值如何返回？ | 写前复用上电加载同级校验，非法返回协议错误 | |
| Q-GOAL-AFE-002 | AFE 通信失败后必须有 fail-safe 策略 | `I2C_AFE1.c:617-645`, `SH367309_Func.c:307-338` | 读失败主要置 `ERROR_AFE1`，旧数据和输出策略边界不清 | P0 | UNKNOWN | 连续 AFE 失败时应断 MOS、保持、复位还是只上报？ | 先定义失败阈值和安全动作，再改代码 | |
| Q-GOAL-AFE-003 | AFE watchdog 是否启用需要产品安全决策 | `SH367309_Func.c:136-148` | 注释说明未开 `ENWDT`，当前只开 CADC 和 MOS 控制位 | P0/P1 | UNKNOWN | SH367309 当前产品是否要求 AFE 独立 watchdog？ | 如不开，写明风险接受依据；如开，单独验证 MOS/均衡行为 | |
| Q-GOAL-MOS-001 | `new_todo_logi()` 中 MOS/CTLC/UL 逻辑必须确认归属 | `DataDeal.c:1086-1145` | 量产 200ms 路径内运行，含硬编码阈值和 CTLC 控制 | P1 | CHANGE_NEEDED | UL/熔断/CTLC 是当前客户需求还是历史残留？ | 先保持行为，后续拆到 `mos_ctrl` 或配置隔离 | |
| Q-GOAL-LP-001 | reset-sleep 合法唤醒源必须与 EXTI 配置一致 | `SleepDeal.c:22-75`, `conf.c:215-270` | EXTI 有 UART1/CHG/CMNT/MCU_WK，但合法启动唤醒主要看充电器/按键 | P1 | UNKNOWN | UART/CMNT/MCU_WK 是否应进入正常运行，还是只唤醒预览？ | 建立 wake source matrix 后再改 | |
| Q-GOAL-COMM-001 | 通信活跃判定不能因计数回绕或非 volatile 漏判 | `SleepDeal.c:4`, `Sci_Upper.c:1465-1483` | `RTC_ExtComCnt` 为 `UINT8` 且非 `volatile` | P1 | CHANGE_NEEDED | 是否允许改为 `volatile uint16_t/uint32_t` 或 last activity tick？ | 先补测试和观测，再小步替换 | |
| Q-GOAL-CAN-001 | CAN 关键帧可靠性与低功耗功耗目标需要取舍 | `Can_HDX.c:421-448`, `Can_HDX.c:1128-1182` | TX failed/timeout 会清 mailbox；RTC wake service 可等待约 1.5s | P1 | CONFLICT | 休眠中是否必须周期 CAN 可见？ACK/IAP/写寄存器/老化控制是否必须重试？ | 周期帧可丢，关键帧有限重试，RTC slice 有界 | |
| Q-GOAL-PROTO-001 | 空实现或 `#if 0` 写入口不能让上位机误判成功 | `Sci_Upper.c:963-995`, `Sci_Upper.c:1758-1805`, `Sci_Upper.c:1876-1883`, `Sci_Upper.c:2015-2067` | SOC 测试/校准旧主体关闭，校准 reset 主体关闭，铜损/RTC 写为空；因默认 POS ACK，可能正响应但无动作 | P1 | CHANGE_NEEDED | 这些地址是废弃、占位还是要恢复？Release 下返回什么错误码？ | 废弃入口显式返回不支持，测试入口仅 Factory/Test 开启 | |
| Q-GOAL-SOC-001 | SOC 校准是否必须避开保护/系统故障 | `Project_Config.h:286-292`, `SocEnhance.c:913-940`, `SocEnhance.c:1268-1295` | 两个 block 宏为 0，满电锚点主要按电压/压差/SOC 渐进到 100% | P1 | UNKNOWN | 满电锚点是否需要 charger-present/taper 条件？故障态是否阻断校准？ | 先确认体验与安全策略，再改算法 | |
| Q-GOAL-STORAGE-001 | Flash/EEPROM 存储失败必须可见并有恢复策略 | `System_Monitor.c:128-146`, `Flash.c:648-665`, `DataDeal.c:1019-1024` | `ERROR_EEPROM_STORE` 不递增错误位，写失败可观测性不足 | P1 | CHANGE_NEEDED | 存储失败应只上报、阻塞低功耗、限制输出还是复位？ | 先定义错误状态语义，再补上报/阻塞 | |
| Q-GOAL-UPGRADE-001 | 升级参数清除策略必须按量产/特殊包隔离 | `Project_Config.h:411-447`, `EEPROM.c:255-287`, `FactoryAging.c:547-555` | policy `0x0005` 默认清 AFE/保护/SOC 配置/SOC snapshot/事件/老化时间；SOC table reset 宏为 1 但当前受 runtime table 关闭门控不执行；老化 reset 会进入 host running 路径 | P0/P1 | UNKNOWN | `0x0005` 是否当前量产升级要求？哪些现场数据必须保留？老化 reset 是否允许启动/恢复老化 running？ | 默认不扩大清参；每个版本策略写 reset/retain/impact/rollback | |

## 5. 分阶段执行计划

| 阶段 | 目标文件 | 任务 | 禁止修改 | 风险 | 前置条件 | 验证 | 回滚 |
|---|---|---|---|---|---|---|---|
| S0 | `README.md`, `docs/README.md`, `docs/review/*` | 修正 source-first 当前事实、标记 stale 文档冲突、收敛需求确认表和权威阅读入口 | 禁止改源码/工程/协议/脚本 | 低 | 当前源码复核完成 | `python3 tools/project_check.py -q`, `git diff --check` | revert 文档 commit |
| S1 | `tools/project_check.py`, `docs/design/bootloader_iap_design.md`, `docs/review/*` | 增加 App 结束地址、后 16KB 存储区、IAP mailbox 的只读门禁设计；先文档后脚本 | 禁止改 scatter 和 Flash 地址 | 中 | 用户确认 App 最大边界和 IAP mailbox 规则 | project check 能识别 map 缺失/地址越界；Windows/Keil 生成 map 后复核 | revert 脚本/文档 commit |
| S2 | `SH367309_DataDeal.c`, `Sci_Upper.c`, 协议文档 | AFE 参数写入范围校验和错误码策略 | 禁止改 AFE MTP 时序、参数地址、寄存器含义 | 高 | 用户确认写权限和非法值返回策略 | Keil 编译；上位机写合法/非法 AFE 参数；MTP 写回验证 | 单独 revert |
| S3 | `I2C_AFE1.c`, `SH367309_Func.c`, `System_Monitor.c`, AFE 文档 | AFE 失败可见性与 fail-safe 策略 | 禁止直接改变 MOS 安全动作，除非策略确认 | 高 | 用户确认连续失败阈值和安全动作 | AFE 断线/CRC/I2C 失败实测，`0xD000`/fault/CAN 状态一致 | 单独 revert |
| S4 | `SleepDeal.c`, `conf.c`, `rtc_sleep.c`, `Can_HDX.c`, 低功耗文档 | wake source matrix 与通信活跃判定 | 禁止改 BKP flag 含义、唤醒源电平、CAN ID | 高 | 用户确认哪些源可进入正常运行 | STOP/reset-sleep、UART/CMNT/CHG/key/CAN 唤醒实测 | 低功耗单独分支 revert |
| S5 | `Sci_Upper.c`, `docs/protocol/*`, 上位机测试脚本 | 空实现/`#if 0` 写入口返回语义 | 禁止改地址、长度、正常读窗口、`0xC002`、`0xD300` | 中高 | 用户确认废弃/占位/恢复列表 | Modbus 0x03/0x06/0x10 回归，上位机回归 | 单独 revert |
| S6 | `SocEnhance.c`, `SOC.c`, `tools/soc_*`, SOC 文档 | SOC 校准阻断和满电锚点策略 | 禁止无测试改 OCV/full/empty/rest/smoothing | 高 | 用户确认体验优先还是安全优先 | host replay、充放电/静置/RTC 实测、`0xD000`/LED/CAN 一致 | 算法参数与代码分开 revert |
| S7 | `Flash.c`, `System_Monitor.c`, `DataDeal.c`, storage 文档 | 存储失败可见性与低功耗阻塞语义 | 禁止改 Flash 地址、数据结构、magic/version/CRC | 高 | 用户确认存储失败处置策略 | 写失败注入、断电恢复、`ERROR_STATUS_EEPROM_STORE` 可见 | 单独 revert |
| S8 | 已确认死代码相关文件 | 第一批净删减：`App_WarnCtrl`、注释旧实现、空函数、未用测试入口 | 禁止删客户可见协议、保护、MOS、低功耗、IAP、老化 | 中 | 用户逐项确认删除清单 | Keil 编译、project_check、协议 smoke test | 每类删除单独 commit |

## 6. 当前验证边界

已完成：

- 复核当前 `DataDeal.c`：真实电流路径已生效，虚拟电流主路径旧结论过期。
- 复核当前主运行链、配置宏、Flash/IAP 地址、scatter、AFE 写入口、AFE read fail、sleep wake、CAN RTC service、空实现写入口、SOC 校准、存储错误链路。
- 同步根 `README.md`、`docs/README.md` 和 `docs/review/refactor_plan.md`，把默认入口收敛到 source-first 权威文档和可执行 staged plan。
- 新增 `docs/review/current_goal_completion_audit_2026-05-31.md`，按目标要求逐项记录当前证据、状态、缺口、下一步和不能宣称完成的原因。
- 新增 `docs/review/s1_flash_iap_user_confirmation_2026-05-31.md`，把 S1 的 App 结束地址、IAP SRAM mailbox、Release map 和 MCU Flash 容量收敛为四个可直接填写的用户决策。
- 新增 `docs/review/s2_s3_afe_user_confirmation_2026-05-31.md`，把 AFE 写参权限、非法参数响应、fail-safe、watchdog 和虚拟电流隔离收敛为可直接填写的用户决策。
- 新增 `docs/review/s4_low_power_comm_user_confirmation_2026-05-31.md`，把 reset-sleep 唤醒源、通信活跃、CAN RTC service 和 IWDG 取舍收敛为可直接填写的用户决策。
- 新增 `docs/review/s5_protocol_ack_user_confirmation_2026-05-31.md`，把协议空实现/`#if 0` 写入口、错误码和 CAN App 复用语义收敛为可直接填写的用户决策。
- 新增 `docs/review/s7_storage_upgrade_user_confirmation_2026-05-31.md`，把 `ERROR_EEPROM_STORE` 可见性、低功耗阻塞、upgrade policy `0x0005` 和老化 reset 收敛为可直接填写的用户决策。
- 运行 `python3 tools/project_check.py -q`，结果为 147 OK、1 warning、0 errors。

未完成：

- 未执行 Windows/Keil `FD_Release` 编译。
- 未生成当前 `FD_Release.map`，因此 App 结束地址、ROM/RAM 用量和 mailbox 是否被占用仍未最终验证。
- 未烧录，不读取 COM4/19200，不做 CAN/ST-Link/实物板验证。
- 未运行完整上位机 UI 回归。

## 7. 下一步建议

1. 先由用户确认 `docs/review/s1_flash_iap_user_confirmation_2026-05-31.md` 中的四个 S1 决策，因为这是发布安全最高优先级。
2. 同步确认 `docs/review/s2_s3_afe_user_confirmation_2026-05-31.md` 中的 AFE 参数写权限、非法值响应、fail-safe、watchdog 和虚拟电流隔离策略，再进入 AFE 相关代码阶段。
3. 同步确认 `docs/review/s4_low_power_comm_user_confirmation_2026-05-31.md` 中的低功耗唤醒、通信活跃、CAN RTC service 和 IWDG 决策，再进入低功耗相关代码阶段。
4. 同步确认 `docs/review/s5_protocol_ack_user_confirmation_2026-05-31.md` 中的协议写入口 ACK 决策，再进入 Modbus/CAN App 写入口源码阶段。
5. 同步确认 `docs/review/s7_storage_upgrade_user_confirmation_2026-05-31.md` 中的存储失败和升级清参决策，再进入存储/升级源码阶段。
6. 低功耗、CAN、SOC 算法、协议、存储不要混在同一批改动里；每项单独分支/commit/验证。
7. 在用户确认前，本仓库源码保持不变，只继续修正文档事实和补验证计划。

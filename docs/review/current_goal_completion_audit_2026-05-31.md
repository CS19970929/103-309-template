# 当前目标完成度审计与证据矩阵

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码/脚本/文档：`AGENTS.md`, `README.md`, `docs/README.md`, `BMS_DAILY_DEV_WORKFLOW.md`, `项目协作与发布检查清单.md`, `项目自动化检查与发布流程.md`, `docs/review/current_goal_fact_sync_2026-05-31.md`, `docs/review/refactor_plan.md`, `docs/review/*_gate_plan_2026-05-31.md`, `main.c`, `Runtime.c`, `System_Init.c`, `DataDeal.c`, `Flash.h`, `Flash.c`, `tools/project_check.py`, `tools/soc_flash_app_safe.ps1`
最后更新时间：2026-05-31
未确认事项：本审计不是完成声明；未执行 Keil/ARMCC Release 编译、未生成当前 `FD_Release.map`、未接 COM4/CAN/ST-Link/实物板、未运行原上位机 exe 回归、未获得用户对 P0/P1 需求确认。

## 1. 当前意图与目标判断

当前目标是长期接管 103/309 BMS 工程协作，把固件、CAN/Modbus 上位机工具、测试脚本和文档体系收敛为“源码可信、量产安全、协议兼容、低复杂度、可验证、可回滚”的工程资产。

当前阶段仍是“需求澄清 + 文档/风险闭环 + 可执行计划”。本阶段只允许更新文档和只读检查结果，不允许修改 `.c/.h`、Keil 工程、编译宏、协议行为、Flash 布局、CAN ID、Modbus 寄存器、上位机数据含义或烧录脚本。

## 2. 审计状态定义

| 状态 | 含义 |
|---|---|
| 已按源码验证 | 当前源码/脚本已证明该事实成立，但不代表硬件端已验证 |
| 部分完成 | 已有文档、证据或计划，但仍缺用户确认、脚本门禁、构建产物或实机验证 |
| 待确认 | 源码可描述现状，但真实产品需求必须由用户确认 |
| 待实现 | 已确认目标方向或风险，但当前未改代码/脚本实现 |
| 环境缺失 | 需要 Windows/Keil、map、COM4/CAN/ST-Link/实物板或原上位机 exe 回归 |
| 不允许进行 | 当前阶段禁止做，必须等用户明确批准 |

## 3. 目标要求到证据矩阵

| 要求 ID | 目标要求 | 当前权威证据 | 当前状态 | 缺口/风险 | 下一步 |
|---|---|---|---|---|---|
| AUD-GOAL-001 | 以 source-first 方式确认事实 | `docs/review/current_goal_fact_sync_2026-05-31.md`, `docs/review/module_map.md`, `docs/review/document_source_consistency.md` | 部分完成 | 根目录旧文档仍多，后续仍需逐批标记冲突/归档候选 | 继续只读审查，所有新结论必须回到源码核对 |
| AUD-GOAL-002 | 当前主链路清楚：`main -> AppInit_Boot -> Runtime_RunOnce` | `main.c:5-12`, `Runtime.c:53-99`, `docs/review/current_goal_fact_sync_2026-05-31.md` | 已按源码验证 | 未做运行时断点/实机验证 | 后续 Keil/板端 baseline 时确认主循环节奏 |
| AUD-GOAL-003 | TIM3 10ms tick 与 200ms AFE/SOC 主链路清楚 | `System_Init.c:103-130`, `DataDeal.c:1225-1249` | 已按源码验证 | 未用示波器/调试器确认 10ms/200ms 实际节奏 | S0 保持文档；源码阶段前补 Keil/调试验证 |
| AUD-GOAL-004 | 当前真实电流路径不再误判 | `DataDeal.c:1238-1239`, `docs/review/document_source_consistency.md` | 已按源码验证 | `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE` 和 `sys_time.isdebugenable` 仍需确认是否隔离 | 用户确认后决定删除或迁移 `test_Autocurrent_cycle()`/虚拟电流 |
| AUD-GOAL-005 | 不改源码、不改协议、不改 Flash 布局 | 当前 `git status` 未显示 `103 + 309/Project/Source/*.c/*.h` 修改；本轮只改文档 | 已按当前工作树验证 | 工作树有多份未提交文档和未跟踪目录，提交前必须白名单 | 继续只改文档；提交时只 stage 本阶段目标文件 |
| AUD-GOAL-006 | 输出 staged plan，包含 files/forbidden/risk/prereqs/validation/rollback | `docs/review/refactor_plan.md`, `docs/review/current_goal_fact_sync_2026-05-31.md` | 部分完成 | S1-S8 计划已有；更细的 task-by-task 源码执行计划需等用户确认具体边界 | 用户确认某阶段后再拆到 task 级 |
| AUD-GOAL-007 | stale 文档冲突清楚 | `docs/review/document_source_consistency.md` | 部分完成 | 已标虚拟电流、CAN active、升级老化 reset 等冲突；根目录历史文档仍需继续收敛 | 后续按模块继续加冲突行，不直接删除旧文档 |

## 4. 硬性外部契约审计

| 契约 ID | 必须保留内容 | 当前证据 | 当前状态 | 缺口/风险 | 下一步 |
|---|---|---|---|---|---|
| AUD-CONTRACT-001 | IAP `0x08000000`、App `0x08004800` | `Flash.h:4-5`, `tools/soc_flash_app_safe.ps1:17-20` | 已按源码/脚本验证 | Release map 缺失，不能证明最终链接产物 | S1 先补 map/地址门禁计划，后续 Keil 构建验证 |
| AUD-CONTRACT-002 | 禁止裸写 App bin 到 `0x08000000` | `tools/soc_flash_app_safe.ps1:17-20`, `BMS_DAILY_DEV_WORKFLOW.md` | 已按脚本验证 | 未在当前机器执行 PowerShell dry-run；未烧录 | 保持只读；需要烧录时只用安全脚本 |
| AUD-CONTRACT-003 | App 不覆盖 `0x0801C000+` 持久化区 | `Flash.h:7-29`, scatter `LR_IROM1 0x08004800 0x00020000` | 部分完成 | scatter 理论上覆盖存储区；`project_check.py` 当前未检查结束地址 | 用户确认后先改 `tools/project_check.py` 门禁，再考虑 scatter |
| AUD-CONTRACT-004 | SRAM mailbox `0x20004FE0` 不被普通 RAM 覆盖 | `Flash.c:12`, scatter `RW_IRAM1 0x20000000 0x00005000` | 待确认 | mailbox 位于当前 RAM 尾部范围内，未被 scatter 保留 | 用户确认 IAP 固件地址后，单独处理 map/scatter |
| AUD-CONTRACT-005 | 量产 `PROJECT_CFG_BUILD_PROFILE 0`，SOC 测试关闭 | `Project_Config.h`, `tools/project_check.py` release safe defaults | 已按源码/脚本验证 | 未做板端 `0xD300 supported=0` 验证 | Keil/板端 baseline 时读 `0xD300` |
| AUD-CONTRACT-006 | Modbus `0xD000`, `0xD300`, `0xC002` 不改 | `docs/protocol/modbus_register_map.md`, `Sci_Upper.c`, `docs/review/protocol_write_ack_gate_plan_2026-05-31.md` | 部分完成 | 未跑原上位机/串口回归；空写入口 ACK 仍待确认 | S5 用户确认后改 ACK 语义或文档化 |
| AUD-CONTRACT-007 | CAN 周期帧、CAN App、IAP、老化控制和 `0x14F80208` 不破坏 | `docs/design/protocol_design.md`, `Can_HDX.c`, `CanFeidaoFrames.c` | 部分完成 | 未抓 CAN 帧；关键帧可靠性/低功耗取舍未确认 | S4/S5 单独确认并实测 |
| AUD-CONTRACT-008 | SN/HW/SW 来自 `0xC002` 48 寄存器并在原上位机显示 | `AGENTS.md`, `docs/design/protocol_design.md`, `Sci_Upper.c` | 部分完成 | 未运行原 `BMS_CommTool_Upgrade_UI.exe` 回归 | 上位机阶段必须编译覆盖原 exe 并 UI 验证 |
| AUD-CONTRACT-009 | 老化剩余时间在原上位机界面单独可见 | `AGENTS.md`, `CanFeidaoFrames.c`, `docs/design/protocol_design.md` | 部分完成 | 未跑上位机 UI；升级清老化会进入 host running 路径需确认 | S7/S5 合并确认老化升级策略与 UI 回归 |

## 5. 当前 P0/P1 门禁完成度

| 门禁 ID | 主题 | 当前文档 | 当前状态 | 未完成证据 |
|---|---|---|---|---|
| GATE-S1 | Flash/App/IAP 地址隔离 | `docs/review/flash_iap_address_gate_plan_2026-05-31.md`, `docs/review/s1_flash_iap_user_confirmation_2026-05-31.md` | 部分完成 | `tools/project_check.py` 只检查 map base 和 size warning，缺结束地址/mailbox/strict map fail；S1 用户决策尚未填写 |
| GATE-S2 | AFE 参数写入范围校验 | `docs/review/afe_safety_gate_plan_2026-05-31.md`, `docs/review/s2_s3_afe_user_confirmation_2026-05-31.md` | 待确认 | Host/CAN 写权限、非法值错误码、温度参数额外范围尚未填写用户决策，未改源码 |
| GATE-S3 | AFE fail-safe/watchdog | `docs/review/afe_safety_gate_plan_2026-05-31.md`, `docs/review/s2_s3_afe_user_confirmation_2026-05-31.md` | 待确认 | 连续失败阈值、安全动作、ENWDT 策略和虚拟电流隔离尚未填写用户决策，未改源码 |
| GATE-S4 | reset-sleep 唤醒源/通信活跃/CAN RTC service | `docs/review/low_power_comm_wake_gate_plan_2026-05-31.md`, `docs/review/s4_low_power_comm_user_confirmation_2026-05-31.md` | 待确认 | UART1/CMNT/MCU_WK 合法启动、按键时长、通信活跃真相源、CAN RTC 可见性、关键 ACK 重试和 IWDG 周期取舍尚未填写用户决策 |
| GATE-S5 | 空实现协议 ACK | `docs/review/protocol_write_ack_gate_plan_2026-05-31.md`, `docs/review/s5_protocol_ack_user_confirmation_2026-05-31.md` | 待确认 | 空实现统一 NEG、Release 错误码、SOC 测试、校准、铜损、RTC 写入口归属和 CAN App 复用语义尚未填写用户决策 |
| GATE-S6 | SOC 校准阻断/满电锚点 | `docs/review/current_goal_fact_sync_2026-05-31.md`, `docs/review/requirement_questions.md` | 待确认 | 故障阻断、charger-present/taper 条件未确认 |
| GATE-S7 | 存储失败可见性/升级清参 | `docs/review/storage_upgrade_gate_plan_2026-05-31.md`, `docs/review/s7_storage_upgrade_user_confirmation_2026-05-31.md` | 待确认 | `ERROR_EEPROM_STORE` 可见性、低功耗阻塞、输出限制、policy `0x0005` 清参范围、老化 reset 和部分动作失败策略尚未填写用户决策 |
| GATE-S8 | 低复杂度净删减 | `docs/refactor/02_delete_or_keep_list.md`, `docs/review/refactor_plan.md` | 待确认 | 删除候选未逐项确认，不能改源码 |

## 6. 本机验证矩阵

| 验证项 | 当前命令/证据 | 结果 | 覆盖范围 | 不覆盖范围 |
|---|---|---|---|---|
| 静态项目检查 | `python3 tools/project_check.py -q` | 147 OK / 1 warning / 0 errors | 配置、脚本、文档依赖、部分 Release 保护 | 缺 `FD_Release.map`，无法证明最终链接布局 |
| diff 空白检查 | `git diff --check` | 通过 | 文档和文本变更空白格式 | 不证明业务正确 |
| SOC replay | `python3 tools/soc_replay_test.py` | 47 tests passed | SOC Python replay 模型与部分 C 参数一致性 | 不证明板端 AFE/电流/RTC/CAN/上位机 |
| Keil Release 编译 | 未运行 | 环境缺失 | 无 | ROM/RAM/map/bin/链接脚本/Keil 编码未验证 |
| COM4/Modbus | 未运行 | 环境缺失 | 无 | `0xD000/0xD300/0xC002` 板端行为未验证 |
| CAN 抓包 | 未运行 | 环境缺失 | 无 | 飞道周期帧、CAN App、老化广播未验证 |
| ST-Link/烧录 | 未运行 | 环境缺失 | 无 | IAP、App 烧录、Flash 容量、运行快照未验证 |
| 原上位机 exe | 未运行 | 环境缺失 | 无 | `BMS_CommTool_Upgrade_UI.exe` UI 显示和老化入口未验证 |

## 7. 当前不应宣称完成的原因

1. 用户尚未确认 P0/P1 需求表中的关键决策。
2. `FD_Release.map` 缺失，无法证明 App 结束地址、RAM mailbox 和 ROM/RAM 用量。
3. `tools/project_check.py` 尚未实现 S1 的结束地址、mailbox、strict map fail 等硬门禁。
4. 未执行 Keil/ARMCC 真编译，未生成当前 bin/hex/map。
5. 未进行 COM4/CAN/ST-Link/实物板验证。
6. 未运行原上位机 exe 回归。
7. 当前工作树还有未提交文档和未跟踪目录，提交前必须白名单处理。

## 8. 下一步建议

1. 继续保持文档阶段，不进入源码。
2. 优先让用户确认 `docs/review/s1_flash_iap_user_confirmation_2026-05-31.md` 中的 `S1-DEC-001` 到 `S1-DEC-004`。
3. 同步让用户确认 `docs/review/s2_s3_afe_user_confirmation_2026-05-31.md` 中的 `S2-DEC-*` 和 `S3-DEC-*`，避免 AFE 写参、fail-safe、watchdog 和虚拟电流隔离混在同一批源码改动。
4. 同步让用户确认 `docs/review/s4_low_power_comm_user_confirmation_2026-05-31.md` 中的 `S4-DEC-*`，避免 wake source、通信活跃、CAN RTC service 和 IWDG 混在同一批低功耗源码改动。
5. 同步让用户确认 `docs/review/s5_protocol_ack_user_confirmation_2026-05-31.md` 中的 `S5-DEC-*`，避免显式 NEG、SOC 测试、校准、铜损和 RTC 写入混在同一批协议改动。
6. 同步让用户确认 `docs/review/s7_storage_upgrade_user_confirmation_2026-05-31.md` 中的 `S7-DEC-*`，避免存储错误可见性、低功耗动作和升级清参策略混在同一批存储改动。
7. 用户确认 S1 后，第一项源码外实现应只改 `tools/project_check.py`，为 map 结束地址、SRAM mailbox 和 strict map 缺失增加只读门禁。
8. scatter、源码、协议和上位机改动都必须另起阶段，逐项确认后再做。

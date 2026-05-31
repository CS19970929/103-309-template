# 协议写入口 ACK 语义门禁方案

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Sci_Upper.c`, `103 + 309/Project/Source/Sci_Upper.h`, `103 + 309/Project/Source/conf/Project_Config.h`, `docs/design/protocol_design.md`, `docs/protocol/modbus_register_map.md`
最后更新时间：2026-05-31
未确认事项：校准写入、校准 reset、铜损表写入、RTC 写入、SOC 注入测试入口是否废弃、占位、恢复，还是只允许 Factory/Test profile 使用；Release 下不支持入口应返回 `RS485_ERROR_CMD_INVALID`、`RS485_ERROR_NO_PERMISSION` 还是其他兼容错误码。

## 1. 目标

本方案只定义 Modbus/CAN App 共享写入口的 ACK 语义、风险和后续门禁，不修改源码、寄存器地址、CAN App 命令、`0xD000/0xD300/0xC002` 数据含义或上位机行为。

S5 阶段目标是解决一类明确风险：空实现或 `#if 0` 的写入口不能让上位机、CAN App 或测试脚本误判“写入成功”。

## 2. 当前源码事实

| ID | 当前事实 | 证据 | 判断 |
|---|---|---|---|
| FACT-PROTO-001 | Modbus 帧处理开始时默认 `AckType=RS485_ACK_POS`, `ErrorType=RS485_ERROR_NULL` | `Sci_ModbusProcessFrame()` | 空 handler 若不改 ACK，会返回正响应 |
| FACT-PROTO-002 | `0x06` / `0x10` ACK 只看 `AckType`；POS 时返回原写命令前 6 bytes，NEG 时返回异常码 | `Sci_ACK_0x06_0x10()` | MUST_KEEP |
| FACT-PROTO-003 | `PROJECT_CFG_HOST_WRITE_ENABLE=1`，当前量产允许进入写入口 | `Project_Config.h` | 高风险，需确认权限策略 |
| FACT-PROTO-004 | CAN App 写寄存器复用 `Sci_HostWriteWords()`，后者复用 `Sci_Deal_WrReg_0x06()` / `Sci_Deal_WrRegs_0x10()` | `Sci_HostWriteWords()`, `Can_HDX.c` | Modbus 写风险会同步影响 CAN App |
| FACT-PROTO-005 | `0x2500` SOC 注入测试写入口主体被 `#if 0` 关闭，函数不设置 NEG | `Sci_WrRegs_0x10_SocTestMode()` | 当前可能正 ACK 但无动作 |
| FACT-PROTO-006 | 校准多寄存器写 `Sci_WrRegs_0x10_CalibCoef()` 主体被 `#if 0` 关闭，函数不设置 NEG | `Sci_WrRegs_0x10_CalibCoef()` | 当前可能正 ACK 但无动作 |
| FACT-PROTO-007 | 校准 reset 单寄存器写 `Sci_WrReg_0x06_Reset_CalibCoef()` 主体被 `#if 0` 关闭，函数不设置 NEG | `Sci_WrReg_0x06_Reset_CalibCoef()` | 当前可能正 ACK 但无动作 |
| FACT-PROTO-008 | 铜损表写 `Sci_WrRegs_0x10_CopperLoss()` 和 RTC 写 `Sci_WrRegs_0x10_RTC()` 为空函数，函数不设置 NEG | `Sci_Upper.c` | 当前可能正 ACK 但无动作 |
| FACT-PROTO-009 | SOC runtime table 写在 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE=0` 时会显式返回 `RS485_ERROR_CMD_INVALID` | `Sci_WrRegs_0x10_SocTable()` | 这个入口当前已有明确失败语义 |
| FACT-PROTO-010 | 保护参数和 OtherElement 写入口已有长度、范围和 Flash 保存失败回滚处理 | `Sci_WrRegs_0x10_Protect()`, `Sci_WrRegs_0x10_OtherElement()` | 当前不属于空 ACK 风险，但仍需权限确认 |

## 3. 高风险写入口清单

| 写入口 | 地址/触发 | 当前行为 | 风险 | 建议状态 |
|---|---|---|---|---|
| `Sci_WrRegs_0x10_SocTestMode()` | `0x2500` | 主体 `#if 0`，默认 POS ACK | 量产 `0xD300 supported=0` 正常，但写 `0x2500` 可能让工具误判注入成功 | Release 明确返回不支持；测试 profile 才允许执行 |
| `Sci_WrRegs_0x10_CalibCoef()` | 校准 K/B pair start | 主体 `#if 0`，默认 POS ACK | 上位机校准写入显示成功但 K/B 不变 | 若废弃，明确 NEG；若保留，恢复完整校验和保存 |
| `Sci_WrReg_0x06_Reset_CalibCoef()` | `0x1000` | 主体 `#if 0`，默认 POS ACK | 工装以为恢复默认校准，但实际无动作 | 与校准写入口同策略 |
| `Sci_WrRegs_0x10_CopperLoss()` | `RS485_CMD_ADDR_COPPERLOSS1` | 空函数，默认 POS ACK | 铜损参数可读但写入无效，旧上位机可能误判 | 确认废弃/占位/恢复 |
| `Sci_WrRegs_0x10_RTC()` | `RS485_CMD_ADDR_RTC_TIME_YEAR` | 空函数，默认 POS ACK | RTC 可读不可写但写命令可能正响应 | 若产品不支持设置 RTC，应明确 NEG |

## 4. 门禁规则草案

这些规则是待确认的执行方案，不代表本轮已经修改代码。

| Gate ID | 门禁规则 | 检查来源 | 建议失败策略 |
|---|---|---|---|
| G-PROTO-001 | 所有进入 `0x06/0x10` dispatch 的 handler 必须显式完成动作，或显式设置 `AckType=NEG` 和 `ErrorType` | `Sci_Upper.c` | fail |
| G-PROTO-002 | Release 下被禁用的测试入口必须返回 `RS485_ERROR_NO_PERMISSION` 或经确认的兼容错误码，不能正 ACK | `Project_Config.h`, `Sci_Upper.c` | fail |
| G-PROTO-003 | 废弃但保留地址的写入口必须在 `docs/protocol/modbus_register_map.md` 标注“读保留/写不支持” | `docs/protocol/*` | fail |
| G-PROTO-004 | CAN App 写寄存器回归必须覆盖同一错误码，因为它复用 `Sci_HostWriteWords()` | `Can_HDX.c`, `Sci_Upper.c` | fail |
| G-PROTO-005 | 不得改 `0xD000`, `0xD300`, `0xC002`, `0xFFFD`, CAN App 命令 ID 或 payload 含义 | `Sci_Upper.h`, `Can_HDX.c` | fail |

## 5. 需要用户确认的需求表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-PROTO-ACK-001 | 空实现写入口不能返回正 ACK | `Sci_ModbusProcessFrame()` 默认 POS；多个 handler 空实现 | 写命令可能正响应但没有副作用 | 上位机/CAN App/测试脚本误判成功 | CHANGE_NEEDED | 是否允许把这些入口统一改成显式不支持？ | 建议 Release 返回不支持错误码 | 待确认 |
| REQ-PROTO-SOC-TEST-001 | `0x2500` SOC 注入写入口必须与量产隔离 | `Sci_WrRegs_0x10_SocTestMode()` 主体关闭；`PROJECT_CFG_SOC_TEST_MODE_ENABLE=0` | 量产 `0xD300 supported=0`，但写 `0x2500` 语义不清 | 测试工具误判注入样本成功 | CHANGE_NEEDED | 测试 profile 是否仍需要 `0x2500` 写入口？ | Release 明确无权限，Test profile 恢复执行 | 待确认 |
| REQ-PROTO-CALIB-001 | 校准写入和 reset 是否仍是量产/工装需求 | `Sci_WrRegs_0x10_CalibCoef()`, `Sci_WrReg_0x06_Reset_CalibCoef()` | 入口存在但主体关闭 | 工装校准流程可能失效 | UNKNOWN | 量产是否还通过 Modbus 写 K/B 校准？ | 若不用，返回不支持；若使用，恢复保存闭环 | 待确认 |
| REQ-PROTO-COPPER-001 | 铜损表写入口是否保留 | `Sci_WrRegs_0x10_CopperLoss()` 空函数，读路径仍读 `CopperLoss` | 可读但写无动作 | 旧上位机误判写成功 | UNKNOWN | 铜损表是否当前产品仍需要？ | 不需要则写不支持，需要则单独实现和验证 | 待确认 |
| REQ-PROTO-RTC-001 | RTC 写入口是否保留 | `Sci_WrRegs_0x10_RTC()` 空函数 | RTC 写无动作但可能正响应 | 时间设置入口不一致 | UNKNOWN | 上位机是否需要设置 RTC？ | 默认不支持并文档化，若需要另立 RTC 写任务 | 待确认 |

## 6. 分阶段执行计划

| 阶段 | 文件范围 | 动作 | 禁止改动 | 验证 | 回滚 |
|---|---|---|---|---|---|
| S5-D0 | `docs/review/*`, `docs/design/protocol_design.md`, `docs/protocol/modbus_register_map.md`, `docs/README.md` | 建立协议写 ACK 门禁方案，修正文档口径 | 禁止改 `.c/.h`、协议地址、CAN ID、上位机数据含义 | `python3 tools/project_check.py -q`, `git diff --check` | 回滚本轮文档 patch |
| S5-D1 | `Sci_Upper.c`, `docs/protocol/*` | 用户确认后只把空 handler 改成显式 NEG，不恢复功能 | 不改地址、长度、读窗口、`0xD000/0xD300/0xC002/0xFFFD` | Modbus 0x06/0x10 写不支持入口返回预期错误码；CAN App write 返回同一错误 | 单独 revert |
| S5-D2 | `Sci_Upper.c`, `tools/*`, SOC 文档 | 用户确认后恢复或隔离 `0x2500` SOC 注入测试 | 不影响量产 `0xD300 supported=0` | SOC test profile 写 `0x2500` 生效；Release 明确拒绝 | 单独 revert |
| S5-D3 | `Sci_Upper.c`, storage/校准文档 | 用户确认后恢复校准写入或明确废弃 | 不改 K/B 地址和读出格式 | 工装写 K/B、reset、断电恢复或不支持错误码回归 | 单独 revert |
| S5-D4 | `Sci_Upper.c`, RTC/铜损文档 | 用户确认后处理 RTC/铜损写入口 | 不改现有读窗口 | 写入生效或明确拒绝；上位机回归 | 单独 revert |

## 7. 当前验证边界

本轮只做到源码和文档层面的证据同步：

- 未修改 Modbus、CAN App、上位机或固件源码。
- 未接 COM4、CAN adapter 或真实 BMS 板。
- 未验证实际上位机在这些写入口上的 UI 行为。
- 未执行 Keil/ARMCC 真构建。

因此，本轮不能声称协议写入口问题已修复，只能作为后续确认和小步实现的输入。

# S5 协议写入口 ACK 用户确认包

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Sci_Upper.c`, `103 + 309/Project/Source/Sci_Upper.h`, `103 + 309/Project/Source/conf/Project_Config.h`, `103 + 309/Project/Source/Can_HDX.c`, `docs/review/protocol_write_ack_gate_plan_2026-05-31.md`, `docs/protocol/modbus_register_map.md`
最后更新时间：2026-05-31
未确认事项：空实现写入口是否统一返回不支持、Release 下 SOC 注入写入口错误码、校准写入/reset 是否恢复、铜损表写入是否保留、RTC 写入是否支持、CAN App 复用写寄存器时的错误码兼容策略。

## 1. 本确认包目标

本确认包只用于确认 S5 的协议写入口 ACK 语义，不代表已经修复协议风险，也不允许直接修改源码。

确认前禁止改动：

1. `0xD000`, `0xD300`, `0xC002`, `0xFFFD` 等客户可见地址和字段含义。
2. Modbus `0x03/0x06/0x10` 帧格式、CRC、长度、寄存器地址和读窗口。
3. CAN App command ID、payload、读写寄存器语义和 `0x14F80208` 老化广播。
4. 原上位机 UI 行为和 `BMS_CommTool_Upgrade_UI.exe` 生成名称。
5. SOC 测试模式量产隔离规则。

确认后的第一步也只应处理空实现/`#if 0` 写入口的显式 NEG，不应同时恢复校准功能、SOC 注入功能、RTC 写入或铜损写入。

## 2. 当前源码事实

| Fact ID | 当前源码事实 | 证据 | 判断 |
|---|---|---|---|
| S5-FACT-001 | `Sci_ModbusProcessFrame()` 每帧开始默认 `AckType=RS485_ACK_POS`, `ErrorType=RS485_ERROR_NULL` | `Sci_Upper.c:1292-1298` | 空 handler 若不改 ACK，会返回正响应 |
| S5-FACT-002 | `Sci_ACK_0x06_0x10()` 在 POS 时返回写命令前 6 bytes，NEG 时返回异常码 | `Sci_Upper.c:1059-1082` | MUST_KEEP |
| S5-FACT-003 | `Sci_HostWriteWords()` 复用 `Sci_Deal_WrReg_0x06()` / `Sci_Deal_WrRegs_0x10()`，CAN App 写寄存器会继承同一 ACK 语义 | `Sci_Upper.c:1131-1180` | Modbus 风险会同步影响 CAN App |
| S5-FACT-004 | `Sci_WrRegs_0x10_SocTestMode()` 主体被 `#if 0` 关闭，函数本体不设置 NEG | `Sci_Upper.c:963-995` | 当前可能正 ACK 但无动作 |
| S5-FACT-005 | `Sci_WrRegs_0x10_CalibCoef()` 主体被 `#if 0` 关闭，函数本体不设置 NEG | `Sci_Upper.c:1758-1805` | 当前可能正 ACK 但无动作 |
| S5-FACT-006 | `Sci_WrReg_0x06_Reset_CalibCoef()` 主体被 `#if 0` 关闭，函数本体不设置 NEG | `Sci_Upper.c:2015-2067` | 当前可能正 ACK 但无动作 |
| S5-FACT-007 | `Sci_WrRegs_0x10_CopperLoss()` 和 `Sci_WrRegs_0x10_RTC()` 是空函数 | `Sci_Upper.c:1876-1883` | 当前可能正 ACK 但无动作 |
| S5-FACT-008 | SOC runtime table 写入口在 runtime table disabled 时已有显式 NEG | `docs/review/protocol_write_ack_gate_plan_2026-05-31.md` | 不属于本轮空 ACK 风险 |

## 3. 用户必须确认的决策

| Decision ID | 需要确认的问题 | 可选方向 | Codex 建议 | User decision placeholder |
|---|---|---|---|---|
| S5-DEC-001 | 空实现或 `#if 0` 写入口是否允许统一改为显式 NEG？ | 统一返回不支持 / 保持旧正 ACK 兼容 / 按地址逐项决定 | 建议 Release 统一返回不支持，避免上位机和 CAN App 误判写成功 | 待确认 |
| S5-DEC-002 | Release 下不支持入口使用什么错误码？ | `RS485_ERROR_CMD_INVALID` / `RS485_ERROR_NO_PERMISSION` / `RS485_ERROR_DATA_INVALID` / 旧工具兼容码 | 建议测试/权限类用 `RS485_ERROR_NO_PERMISSION`，废弃/未实现类用 `RS485_ERROR_CMD_INVALID` | 待确认 |
| S5-DEC-003 | `0x2500` SOC 注入写入口是否在 Test profile 恢复？ | Test profile 恢复 / 完全废弃 / 只保留 `0xD300` 状态查询 | 建议量产 Release 明确拒绝；如仍需要自动化测试，再单独恢复 Test profile | 待确认 |
| S5-DEC-004 | 校准 K/B 写入和 reset 是否仍是工装需求？ | 恢复完整保存闭环 / 改为显式不支持 / 保留地址只读占位 | 若当前工装不用，先显式不支持；若要恢复，必须单独做校准保存和断电回归 | 待确认 |
| S5-DEC-005 | 铜损表写入口是否保留？ | 恢复写入 / 写不支持但读保留 / 删除文档中的写能力 | 建议写不支持但保留读窗口，避免破坏旧读协议 | 待确认 |
| S5-DEC-006 | RTC 写入口是否支持？ | 支持上位机设置 RTC / 写不支持只读 / 仅 Factory/Test 支持 | 建议默认写不支持并文档化；如需要设置时间，另立 RTC 写任务 | 待确认 |
| S5-DEC-007 | CAN App 写寄存器遇到这些不支持入口时是否返回同一错误码？ | 与 Modbus 完全一致 / CAN App 自定义错误 / 上位机重试策略处理 | 建议与 Modbus 一致，因为 `Sci_HostWriteWords()` 已复用同一 dispatch | 待确认 |

## 4. 用户填写模板

| Decision ID | 用户决策 | 备注/约束 |
|---|---|---|
| S5-DEC-001 |  |  |
| S5-DEC-002 |  |  |
| S5-DEC-003 |  |  |
| S5-DEC-004 |  |  |
| S5-DEC-005 |  |  |
| S5-DEC-006 |  |  |
| S5-DEC-007 |  |  |

## 5. 用户确认后的执行计划

| 阶段 | 文件范围 | 任务 | 禁止改动 | 验证 | 回滚 |
|---|---|---|---|---|---|
| S5-D1 | `Sci_Upper.c`, `docs/protocol/*` | 只把空实现/`#if 0` 写入口改成显式 NEG，不恢复功能 | 不改地址、长度、读窗口、`0xD000/0xD300/0xC002/0xFFFD`、CAN ID | Modbus `0x06/0x10` 对不支持入口返回确认错误码；CAN App write 返回同一错误 | 单独 revert S5-D1 commit |
| S5-D2 | `Sci_Upper.c`, `tools/soc_*`, SOC 文档 | 如用户确认，恢复或隔离 `0x2500` SOC 注入测试 | 不影响量产 `PROJECT_CFG_BUILD_PROFILE 0` 和 `0xD300 supported=0` | Test profile 写 `0x2500` 生效；Release 明确拒绝；SOC replay 通过 | 单独 revert S5-D2 commit |
| S5-D3 | `Sci_Upper.c`, 校准/存储文档 | 如用户确认，恢复校准 K/B 写入或明确废弃 | 不改 K/B 地址、读出格式和工装可见字段 | 工装写 K/B、reset、断电恢复或不支持错误码回归 | 单独 revert S5-D3 commit |
| S5-D4 | `Sci_Upper.c`, RTC/铜损文档 | 如用户确认，处理 RTC/铜损写入口 | 不改现有读窗口和协议占位 | 写入生效或明确拒绝；原上位机回归 | 单独 revert S5-D4 commit |

## 6. 当前验证边界

本确认包只完成源码阅读和文档化确认：

- 未修改 `.c/.h`、Keil 工程、编译宏、协议行为或烧录脚本。
- 未连接 COM4、CAN adapter 或真实 BMS 板。
- 未运行原上位机 UI 回归。
- 未验证 CAN App 写寄存器对这些入口的实际 UI/脚本表现。

因此，本文件只能作为 S5 进入源码阶段前的用户确认输入，不能作为协议写入口问题已修复的证明。


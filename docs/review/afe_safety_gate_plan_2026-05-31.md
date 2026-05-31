# AFE 参数与失效安全门禁执行方案

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/SH367309_DataDeal.c`, `103 + 309/Project/Source/SH367309_DataDeal.h`, `103 + 309/Project/Source/SH367309_Func.c`, `103 + 309/Project/Source/I2C_AFE1.c`, `103 + 309/Project/Source/DataDeal.c`, `103 + 309/Project/Source/System_Monitor.c`, `docs/review/requirement_questions.md`, `docs/review/risk_list.md`
最后更新时间：2026-05-31
未确认事项：量产是否允许主机/CAN 写 AFE 参数、非法 AFE 参数的协议错误码、AFE 连续通信失败阈值、失败后安全动作、SH367309 AFE watchdog 是否必须启用。

## 1. 目标

本方案只定义 AFE 安全边界、门禁和确认项，不修改源码、协议寄存器、AFE MTP 时序、MOS 控制策略或上位机行为。

S2/S3 阶段目标是把 SH367309 AFE 的三类高风险行为变成可确认、可验证、可回滚的工程任务：

1. AFE 参数写入前必须校验范围，不允许非法保护阈值直接保存并进入 MTP/ROM 更新。
2. AFE 通信失败后必须有明确 fail-safe 策略，不允许“只置错误但继续沿用旧状态”的边界不清。
3. AFE watchdog 是否启用必须由产品安全策略确认，不能只依赖历史注释。

## 2. 当前源码事实

| ID | 当前事实 | 证据 | 判断 |
|---|---|---|---|
| FACT-AFE-001 | AFE 参数结构自带 `curValue/defaultValue/maxValue/minValue` | `SH367309_DataDeal.h` 中 `AFE_Value_Typedef` | 可复用为写前校验基础 |
| FACT-AFE-002 | 上电从 Flash 读取 AFE 参数时会逐项检查 `minValue/maxValue`，失败则恢复默认 | `ReadEEPROM_AFE_Parameters()` | 已有加载侧校验 |
| FACT-AFE-003 | 主机写 `0x2400..0x2417` 时直接写 `curValue`，随后保存 Flash 并置 `AFE_PARAM_WRITE_Flag` | `Sci_WrRegs_0x10_AFE_Parameters()` | 写入口缺校验 |
| FACT-AFE-004 | `SH367309_UpdataAfeConfig()` 在 `AFE_PARAM_WRITE_Flag` 后执行 `Refresh_Parameters()`、比较 MTP、写 ROM、reset AFE | `SH367309_DataDeal.c` | 非法写入可能进入硬件配置链 |
| FACT-AFE-005 | `Refresh_Parameters()` 会把温度参数换算为 NTC 表索引 | `SH367309_DataDeal.c` | 除 min/max 外还需要温度索引上限确认 |
| FACT-AFE-006 | `MTPRead()` 失败会重试一次，再调用 `System_ERROR_UserCallback(ERROR_AFE1)` | `I2C_AFE1.c` | 有错误上报，但未定义连续失败动作 |
| FACT-AFE-007 | `App_SH367309_Monitor()` 只有读成功分支更新 MOS/fault 状态，读失败分支没有明确 fail-safe 处理 | `SH367309_Func.c` | 旧状态可能继续被使用 |
| FACT-AFE-008 | `SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 注释说明未打开 AFE watchdog，实际只设置 CADC 和 MOS 控制位 | `SH367309_Func.c` | watchdog 策略未被正式确认 |
| FACT-AFE-009 | 当前 200ms 主链路调用 `DataLoad_Current()`，`test_Autocurrent_cycle()` 已注释；但 `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE=1` 时，`sys_time.isdebugenable==1` 仍可覆盖电流 | `Project_Config.h`, `conf.h`, `DataDeal.c` | 真实电流主路径已恢复，但调试覆盖仍需隔离确认 |

## 3. 当前缺口

| 缺口 | 影响 | 当前处理 |
|---|---|---|
| AFE 写入口未复用加载侧范围校验 | 非法保护阈值、温度参数或短路/均衡档位可能被保存并触发 MTP 更新 | 先确认写权限和错误码，再做小步代码修改 |
| AFE 参数 `min/max` 本身可能不足以保护所有换算边界 | 温度参数会进入 `iSheldTemp_10K_NTC[]` 索引，必须确认合法范围和编码含义 | S2 先做参数表审计，不直接改值 |
| AFE 通信失败只上报错误，没有连续失败阈值和安全动作 | 旧电压、温度、MOS 状态可能继续影响 SOC、显示、低功耗和保护辅助判断 | S3 先定义 fail-safe 策略 |
| AFE watchdog 是否关闭缺少正式需求依据 | MCU 卡死或 I2C 异常时，AFE 侧独立关 MOS/清均衡能力是否需要不明确 | 需要 datasheet/客户安全策略/实测确认 |
| 虚拟电流编译开关仍在量产 profile 打开 | debug 状态下仍可覆盖真实 AFE 电流 | 单独纳入测试入口隔离，不混入本轮 AFE 写参修改 |

## 4. 后续门禁规则草案

这些规则是待确认的执行方案，不代表本轮已经修改代码。

| Gate ID | 门禁规则 | 检查来源 | 建议失败策略 |
|---|---|---|---|
| G-AFE-001 | 写 `0x2400..0x2417` 前逐项校验 `curValue` 是否在 `minValue/maxValue` 内 | AFE 参数表 + 写入请求 | 返回 Modbus 异常，不保存，不置 `AFE_PARAM_WRITE_Flag` |
| G-AFE-002 | 写入失败时必须恢复写前 snapshot，不改变 Flash 中有效参数 | `Sci_WrRegs_0x10_AFE_Parameters()` | fail |
| G-AFE-003 | 温度参数必须额外证明不会越界访问 NTC 表 | 参数编码 + NTC 表大小 | fail |
| G-AFE-004 | `AFE_PARAM_WRITE_Flag` 只能在所有校验和保存成功后置位 | 写入口代码审查 | fail |
| G-AFE-005 | AFE 连续读失败计数必须可观测，并定义阈值 | `MTPRead()`, `App_SH367309_Monitor()` | 未确认前不改 MOS 动作 |
| G-AFE-006 | fail-safe 策略必须明确是否阻塞低功耗、是否关闭 CTLC/MOS、是否复位 MCU | 需求确认表 + 实机测试 | 策略未确认时禁止源码实现 |
| G-AFE-007 | AFE watchdog 启用/关闭必须写入设计文档和测试计划 | `SH367309_Func.c`, datasheet/实测 | 未确认时保持现状 |

## 5. 需要用户确认的需求表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-AFE-WRITE-001 | 主机/CAN 写 AFE 参数前必须做实时范围校验 | `Sci_WrRegs_0x10_AFE_Parameters()` 直接写 `curValue`; `ReadEEPROM_AFE_Parameters()` 有加载侧校验 | 写入后保存 Flash，并可触发 MTP 更新 | 非法保护阈值进入硬件配置 | CHANGE_NEEDED | 量产是否允许主机/CAN 写 AFE 参数？ | 若允许，写前严格校验；若不允许，先加权限策略 | 待确认 |
| REQ-AFE-WRITE-002 | 非法 AFE 参数的协议响应必须明确 | `Sci_WrRegs_0x10_AFE_Parameters()` 当前仅保存失败时设 `RS485_ERROR_CMD_INVALID` | 非法值目前不会被拦截 | 旧上位机可能误判成功 | CHANGE_NEEDED | 非法值应返回 `RS485_ERROR_DATA_INVALID`、`CMD_INVALID`，还是兼容旧工具静默限幅？ | 建议返回数据非法，不静默限幅 | 待确认 |
| REQ-AFE-WRITE-003 | 温度参数编码和 NTC 表索引上限必须确认 | `Refresh_Parameters()` 使用 `iSheldTemp_10K_NTC[AFE_TEMPERATURE[i]]` | 当前写入口可写任意 word | 温度表越界或 AFE 温度阈值异常 | CHANGE_NEEDED | AFE 温度参数合法范围是否应限制到 NTC 表有效索引？ | 先做参数表审计，再补写前校验 | 待确认 |
| REQ-AFE-FAIL-001 | AFE 连续通信失败阈值必须定义 | `MTPRead()` 失败只置 `ERROR_AFE1`; `App_SH367309_Monitor()` 读失败无 else | 错误可上报，但安全动作不明确 | AFE 状态未知时继续运行 | UNKNOWN | 连续失败多少次进入 fail-safe？恢复条件是什么？ | 先计数和上报，不立即改变 MOS，策略确认后实现 | 待确认 |
| REQ-AFE-FAIL-002 | AFE fail-safe 动作必须定义 | `SH367309_Func.c`, `System_Monitor.c`, `app_lowpower.c` | 低功耗、MOS、SOC 仍可能使用旧状态 | 输出状态和低功耗边界不清 | UNKNOWN | 失败后是断 CHG/DSG MOS、关闭 CTLC、阻塞低功耗、复位 MCU，还是只上报？ | 建议至少标记数据无效并阻塞低功耗；MOS 动作需客户确认 | 待确认 |
| REQ-AFE-WDT-001 | AFE watchdog 是否启用必须正式确认 | `SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 注释说明未开启 WDT | 只开启 CADC 和 MOS 控制位 | AFE 侧独立保护不足或误动作风险未定 | UNKNOWN | SH367309 当前产品是否要求 AFE watchdog？ | 若不开，写清风险接受依据；若开，单独验证 MOS/均衡行为 | 待确认 |
| REQ-AFE-CURRENT-001 | 虚拟电流必须从量产主路径隔离 | `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE=1`; `DataLoad_Current()` debug 条件下覆盖电流; `test_Autocurrent_cycle()` 已注释 | 主函数调用真实电流，但 debug 覆盖仍存在 | 调试状态误进入量产会影响 SOC/CAN/保护辅助判断 | CHANGE_NEEDED | 现场是否还需要虚拟电流调试？ | 迁移到 Factory/Test profile 或关闭 Release 宏 | 待确认 |

## 6. 分阶段执行计划

| 阶段 | 文件范围 | 动作 | 禁止改动 | 验证 | 回滚 |
|---|---|---|---|---|---|
| S2-D0 | `docs/review/*`, `docs/design/adc_afe_design.md`, `docs/README.md` | 建立本方案，修正 AFE/电流相关过期文档 | 禁止改源码、协议、AFE 参数地址、MTP 时序 | `python3 tools/project_check.py -q`, `git diff --check` | 回滚本轮文档 patch |
| S2-D1 | `SH367309_DataDeal.c/.h` | 用户确认后补 AFE 参数写前校验，不改 MTP 写入流程 | 不改寄存器地址、字段含义、默认值 | host/unit 用例：合法写成功、非法写拒绝、Flash 不变 | 单独 revert |
| S2-D2 | `docs/protocol/*`, `tools/comm_tool_host.py` 或测试脚本 | 明确非法写入错误码并补上位机回归脚本 | 不改旧上位机正常读写流程 | Modbus `0x2400` 合法/非法写回归 | 单独 revert |
| S3-D1 | `I2C_AFE1.c`, `SH367309_Func.c`, `System_Monitor.c` | 用户确认后增加 AFE 连续失败观测和状态出口 | 不直接改变 MOS 动作 | AFE 断线/CRC/I2C fail 模拟或实测 | 单独 revert |
| S3-D2 | `app_lowpower.c`, `SleepDeal.c`, AFE 文档 | 用户确认后把 AFE fail 状态纳入低功耗阻塞或安全动作 | 不改唤醒源和协议 | `0xD000`/CAN fault/低功耗阻塞一致性 | 单独 revert |
| S4-D1 | `SH367309_Func.c` | 用户确认后评估 AFE watchdog 开关 | 不和写参/fail-safe 混在同一提交 | MOS/均衡/watchdog 溢出实测 | 单独 revert |

## 7. 当前验证边界

本轮只做到源码和文档层面的证据同步：

- 未修改 AFE 写参数代码。
- 未修改 SH367309 MTP/ROM 写入时序。
- 未修改 MOS/CTLC/低功耗安全动作。
- 未运行 Keil/ARMCC 真构建。
- 未连接 AFE、COM4、CAN、ST-Link 或真实 BMS 板。
- 未查阅并落地 SH367309 官方 datasheet；如只能使用第三方资料，必须标记来源可信度。

因此，本轮不能声称 AFE 安全问题已修复，只能作为用户确认和后续小步实现的输入。

## 8. 下一步建议

1. 用户先确认 `REQ-AFE-WRITE-001` 到 `REQ-AFE-WRITE-003`，决定是否允许量产写 AFE 参数和非法值返回策略。
2. 确认后先实现 S2-D1 写前校验，只改 `SH367309_DataDeal.c/.h`，不碰 fail-safe 和 watchdog。
3. 写参校验验证通过后，再进入 S3 的 fail-safe 策略；MOS/CTLC 动作必须单独确认，不和写参校验合并。
4. AFE watchdog 必须放在最后独立评估，避免把 watchdog 行为变化混入参数写入或通信失败处理。

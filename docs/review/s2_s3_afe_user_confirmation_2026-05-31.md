# S2/S3 AFE 参数与失效安全用户确认包

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/SH367309_DataDeal.c`, `103 + 309/Project/Source/SH367309_DataDeal.h`, `103 + 309/Project/Source/SH367309_Func.c`, `103 + 309/Project/Source/I2C_AFE1.c`, `103 + 309/Project/Source/DataDeal.c`, `103 + 309/Project/Source/System_Monitor.c`, `103 + 309/Project/Source/conf/Project_Config.h`, `docs/review/afe_safety_gate_plan_2026-05-31.md`
最后更新时间：2026-05-31
未确认事项：量产是否允许主机/CAN 写 AFE 参数、非法参数协议响应、温度参数额外范围、AFE 连续失败阈值、失败后的安全动作、SH367309 watchdog 策略、虚拟电流调试入口隔离。

## 1. 本确认包目标

本确认包只用于让用户确认 S2/S3 的产品需求和安全边界，不代表已经修复 AFE 风险，也不允许直接修改源码。

确认前禁止改动：

1. `SH367309_DataDeal.c/.h` 中 AFE 参数写入逻辑。
2. SH367309 MTP/ROM 写入时序、参数地址、默认值和寄存器含义。
3. MOS、CTLC、低功耗、IWDG、reset 和 fail-safe 动作。
4. Modbus/CAN 地址、帧格式、错误码语义和上位机显示行为。

确认后的第一步也只应进入 S2 写参数校验，不应把 fail-safe、watchdog、虚拟电流和协议清理混在同一批源码提交中。

## 2. 当前源码事实

| Fact ID | 当前源码事实 | 证据 | 判断 |
|---|---|---|---|
| S2S3-FACT-001 | AFE 参数结构已有 `curValue/defaultValue/maxValue/minValue` | `SH367309_DataDeal.h` 中 `AFE_Value_Typedef` | 可作为写前范围校验基础 |
| S2S3-FACT-002 | 上电加载 AFE 参数时会按 `minValue/maxValue` 检查，非法则恢复默认值 | `ReadEEPROM_AFE_Parameters()` | 加载侧已有保护 |
| S2S3-FACT-003 | 主机写 `0x2400..0x2417` 时直接写 `curValue`，随后保存 Flash 并置 `AFE_PARAM_WRITE_Flag` | `Sci_WrRegs_0x10_AFE_Parameters()` | 写入口缺实时校验 |
| S2S3-FACT-004 | `AFE_PARAM_WRITE_Flag` 后会进入 `Refresh_Parameters()`、MTP 比较、ROM 写入和 AFE reset 链路 | `SH367309_UpdataAfeConfig()` | 非法参数可能进入硬件配置链 |
| S2S3-FACT-005 | 部分温度参数会换算为 NTC 表索引 | `Refresh_Parameters()` | 仅靠普通 min/max 可能不足 |
| S2S3-FACT-006 | `MTPRead()` 失败会重试一次，再调用 `System_ERROR_UserCallback(ERROR_AFE1)` | `I2C_AFE1.c` | 有错误上报入口 |
| S2S3-FACT-007 | `App_SH367309_Monitor()` 读失败分支没有明确 fail-safe 处理 | `SH367309_Func.c` | 旧状态可能继续影响后续判断 |
| S2S3-FACT-008 | `SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 注释说明未打开 AFE watchdog，实际只设置 CADC 和 MOS 控制位 | `SH367309_Func.c` | watchdog 策略未正式确认 |
| S2S3-FACT-009 | 当前 200ms 主路径调用 `DataLoad_Current()`，但 `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE=1` 时仍存在 debug 条件覆盖电流的入口 | `DataDeal.c`, `Project_Config.h` | 真实电流主路径已恢复，调试入口仍需隔离 |

## 3. 用户必须确认的决策

| Decision ID | 需要确认的问题 | 可选方向 | Codex 建议 | User decision placeholder |
|---|---|---|---|---|
| S2-DEC-001 | 量产固件是否允许主机/CAN 写 AFE 参数？ | 允许但严格校验 / 只允许 Factory/Test / 完全禁止量产写 | 默认保留现有可写能力，但先补写前校验；若业务不需要量产写，再单独做权限隔离 | 待确认 |
| S2-DEC-002 | 非法 AFE 参数应如何响应？ | 返回数据非法 / 返回命令非法 / 静默限幅 / 保持旧兼容成功响应 | 建议返回数据非法，不保存 Flash，不置 `AFE_PARAM_WRITE_Flag`，不静默限幅 | 待确认 |
| S2-DEC-003 | AFE 参数的 `min/max` 是否足够？ | 只用现有 min/max / 温度和表索引增加专门校验 / 重新审计参数表 | 建议先保留现有表值，温度索引类参数增加额外越界校验 | 待确认 |
| S3-DEC-001 | AFE 连续通信失败多少次进入故障态？恢复条件是什么？ | 固定次数阈值 / 时间阈值 / 只上报不进入故障态 | 建议先增加可观测计数和状态，不直接改变 MOS；阈值由用户确认 | 待确认 |
| S3-DEC-002 | AFE fail-safe 动作是什么？ | 只上报 / 标记数据无效 / 阻塞低功耗 / 关闭 CTLC/MOS / MCU reset | 建议至少标记 AFE 数据无效并阻塞低功耗；关闭 MOS/CTLC 必须单独确认和实测 | 待确认 |
| S3-DEC-003 | SH367309 AFE watchdog 是否启用？ | 保持关闭 / 启用 ENWDT / 先查 datasheet 和台架验证 | 建议保持现状，先补风险说明；启用必须独立小步验证 MOS、均衡和 reset 行为 | 待确认 |
| S2S3-DEC-004 | 虚拟电流调试入口是否保留？ | 保留但仅 Factory/Test / Release 关闭宏 / 删除旧测试入口 | 建议量产 Release 关闭或隔离到 Factory/Test，不和 AFE 写参校验同一提交 | 待确认 |

## 4. 用户填写模板

| Decision ID | 用户决策 | 备注/约束 |
|---|---|---|
| S2-DEC-001 |  |  |
| S2-DEC-002 |  |  |
| S2-DEC-003 |  |  |
| S3-DEC-001 |  |  |
| S3-DEC-002 |  |  |
| S3-DEC-003 |  |  |
| S2S3-DEC-004 |  |  |

## 5. 用户确认后的执行计划

| 阶段 | 文件范围 | 任务 | 禁止改动 | 验证 | 回滚 |
|---|---|---|---|---|---|
| S2-D1 | `SH367309_DataDeal.c`, `SH367309_DataDeal.h` | 给 `0x2400..0x2417` AFE 写参数入口增加写前校验和失败回滚 | 不改参数地址、默认值、MTP 写入时序、MOS 动作、CAN/Modbus 地址 | 合法写成功；非法写失败；非法写不保存 Flash、不置 `AFE_PARAM_WRITE_Flag` | 单独 revert S2-D1 commit |
| S2-D2 | `docs/protocol/*`, 通信测试脚本 | 明确非法写入错误码并补回归用例 | 不改旧上位机正常读写路径，不新增 exe 名称 | Modbus `0x2400` 合法/非法写回归；旧读窗口不变 | 单独 revert S2-D2 commit |
| S3-D1 | `I2C_AFE1.c`, `SH367309_Func.c`, `System_Monitor.c` | 增加 AFE 连续失败计数、状态出口和可观测错误位 | 不改变 MOS/CTLC/低功耗动作 | 模拟或实测 AFE 读失败，确认错误状态可见 | 单独 revert S3-D1 commit |
| S3-D2 | `app_lowpower.c`, `SleepDeal.c`, AFE/低功耗文档 | 用户确认后把 AFE fail 状态纳入低功耗阻塞或安全动作 | 不改唤醒源电平、不改协议、不改未确认的 MOS 策略 | AFE fail 时低功耗阻塞、`0xD000`/CAN fault/显示一致 | 单独 revert S3-D2 commit |
| S3-D3 | `SH367309_Func.c`, AFE 文档和测试计划 | 单独评估 AFE watchdog | 不和写参校验、fail-safe 混在同一提交 | watchdog 溢出、MOS、均衡、reset 行为台架验证 | 单独 revert S3-D3 commit |
| S2S3-D4 | `Project_Config.h`, `DataDeal.c`, 测试文档 | 隔离或关闭虚拟电流调试入口 | 不改变真实 `DataLoad_Current()` 主路径 | Release 下无法触发虚拟电流覆盖；Factory/Test 入口按确认保留 | 单独 revert S2S3-D4 commit |

## 6. 当前验证边界

本确认包只完成源码阅读和文档化确认：

- 未修改 `.c/.h`、Keil 工程、编译宏、协议行为或烧录脚本。
- 未运行 Keil/ARMCC Release 编译。
- 未连接 COM4、CAN、ST-Link 或实物 BMS 板。
- 未实测 SH367309 AFE 断线、CRC/I2C 异常、MTP 写入或 watchdog 行为。
- 未获得 SH367309 官方 datasheet 的本地可信副本；若后续使用第三方资料，必须标记为 `unverified`。

因此，本文件只能作为 S2/S3 进入源码阶段前的用户确认输入，不能作为安全问题已修复的证明。


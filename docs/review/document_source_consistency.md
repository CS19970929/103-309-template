# 文档与源码一致性检查

> 文档状态：CURRENT
> 源码验证：PARTIAL
> 主要参考源码：`Runtime.c`, `DataDeal.c`, `SOC.c`, `SocEnhance.c`, `ADC.c`, `I2C_AFE1.c`, `SH367309_*`, `Sci_Upper.c`, `Can_HDX.c`, `CanFeidaoFrames.c`, `Flash.c`, `EEPROM.c`, `rtc_sleep.c`, `LedBar.c`, `FactoryAging.c`
> 最后更新时间：2026-05-26
> 未确认事项：未执行硬件测试；comm tool 独立工程和 IAP 工程只做边界判断。

## 1. 当前确认一致的核心行为

| 文档路径 | 对照源码文件 | 一致性状态 | 不一致内容 | 源码真实行为 | 风险 | 建议处理 |
|---|---|---|---|---|---|---|
| `docs/review/module_map.md` | 全主工程源码 | VERIFIED | 无，本轮从源码生成 | `main -> AppInit_Boot -> Runtime_RunOnce`，任务链和模块边界按当前源码记录 | 低 | 保留为本轮审查基线 |
| `docs/review/requirement_confirmation.md` | 全主工程源码 | VERIFIED | 无，本轮从源码生成 | 需求反推均带源码证据 | 低 | 保留 |
| `docs/BMS_CAN_SERVICE_PROTOCOL.md` | `Can_HDX.c`, `Sci_Upper.c`, `CanFeidaoFrames.c` | PARTIAL | 文档为协议说明，未覆盖 `Can_IsBusy()`、RTC wake service、队列清理全部实现 | 当前 CAN App 确实支持 status/IAP/read/write/read_block/aging；老化时间广播来自 `0x14F80208` | 中 | 合并到 `protocol_design.md`，补源码未写部分 |
| `docs/CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md` | `Can_HDX.c`, `FactoryAging.c`, `CanFeidaoFrames.c` | VERIFIED | 未确认业务是否仍需保留 | 老化 start/stop/reset/set hours 和剩余分钟广播在源码中存在 | 中 | 合并，但标记“需求待确认” |
| `docs/FACTORY_AGING_MODE_REQUIREMENTS_2026-05-26.md` | `FactoryAging.c`, `MosStartup.c` | PARTIAL | MOS 规则需硬件确认 | 老化运行态、停止/完成、BKP+Flash 保存符合源码 | 中 | 合并到 overview/test |
| `docs/FACTORY_AGING_UPGRADE_RESET_CONFIG_2026-05-26.md` | `Project_Config.h`, `EEPROM.c` | VERIFIED | 无明显冲突 | `PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME 0`，升级策略当前不重置老化时间 | 低 | 合并到 `storage_design.md` |
| `docs/RTC_SLEEP_PORT_REFACTOR_2026-05-25.md` | `rtc_sleep.c`, `rtc_sleep_port.c`, `app_lowpower.c` | VERIFIED | 行号可能随源码变化 | 当前确实有 core/port 分层 | 低 | 合并到 `low_power_design.md` |
| `docs/current/clock_usage_analysis.md` | `System_Init.c`, `bsp_clock.c`, `conf.c` | PARTIAL | 需补当前 STOP 后 `InitRunAfterStopWakeup()` 细节 | 主时钟恢复、TIM3/ADC/CAN/USART 重建存在 | 中 | 合并 |
| `docs/current/iwdg_usage_analysis.md` | `System_Init.c`, `RTC.c`, `Runtime.c` | VERIFIED | 无核心冲突 | IWDG 开启，RTC wake 最大 10s | 中 | 合并 |
| `docs/test/low_power_test_matrix.md` | `rtc_sleep.c`, `app_lowpower.c`, `Sci_Upper.c` | PARTIAL | 部分测试命令依赖 Windows/COM4 环境 | 测试目标与源码路径基本一致 | 低 | 合并到统一测试计划 |

## 2. 明显过时或冲突的文档/内容

| 文档路径 | 对照源码文件 | 一致性状态 | 不一致内容 | 源码真实行为 | 风险 | 建议处理 |
|---|---|---|---|---|---|---|
| `CAN通信逻辑与低功耗策略分析.md` | `Can_HDX.c` | CONFLICT | 文档称当前 CAN “基本不处理请求帧” | 当前 `Can_HDX.c:609-775` 已处理 App command：status/IAP/read/write/read_block/aging | 中 | 归档候选；不要作为当前 CAN 行为引用 |
| `CAN低功耗发送调度说明.md` | `Can_HDX.c` | OUTDATED | 文档描述发送前上电、发送后断电状态机 | 当前 CAN 使用队列、周期调度、`GPIO_CMNT_EN` sleep prepare；不再是旧非阻塞上电窗口描述 | 中 | 归档候选 |
| `CAN_RTC低功耗重构说明_2026-05-14.md` | `app_lowpower.c`, `Can_HDX.c` | OUTDATED | 文档称 RTC 周期唤醒仍有 CAN 状态机服务 | 当前低功耗阻塞只看 `Sci_IsAnyPortBusy()/Can_IsBusy()`；RTC 周期唤醒不再主动广播 CAN | 中 | 归档或标注旧结论 |
| `COMMUNICATION_LAYOUT_REPORT.md` | `Can_HDX.c`, `Sci_Upper.c` | OUTDATED | 文档称 CAN 不承担 EEPROM 地址映射 | 当前 CAN App read/write 复用 `Sci_HostReadWords/WriteWords`，可桥接 Modbus 寄存器 | 中 | 归档候选；当前协议以 `protocol_design.md` 为准 |
| `EEPROM_LAYOUT_OPTIMIZATION.md` | `EEPROM.c`, `Flash.c` | PARTIAL | 文档已自标历史 EEPROM 布局 | 当前外部 EEPROM API 基本空实现，参数走内部 Flash | 低 | 保留为历史，当前实现合并到 `storage_design.md` |
| `STORAGE_LAYOUT_REPORT.md` | `EEPROM.c`, `Flash.c` | OUTDATED | 以外部 EEPROM 地址布局为主 | 当前 RW/Afe/SOC/log/aging 走 `StorageFlash_*` | 中 | 归档候选 |
| `SOC_SIMPLIFY_CHARGE_FIX.md` | `SocEnhance.c` | PARTIAL | 文档称移除部分复杂策略，但当前源码仍有 rest OCV、sag hold、empty tail 等增强逻辑 | 当前 SOC 并非完全简化版本 | 中 | 归档为历史修复记录 |
| `SOC_MODULE_SIMPLIFICATION_2026-05-12.md` | `SocEnhance.c` | PARTIAL | 可能描述旧简化版本 | 当前 SOC 已有多项增强策略和 snapshot V2 | 中 | 归档候选 |
| `OtherElement升级字段覆盖说明.md` | `Project_Config.h`, `UpgradeParamPolicy.h` | CONFLICT | 文档示例可能写 `PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE 1` | 当前源码为 `0` | 中 | 归档或更新为当前配置 |
| `开关长按休眠3秒计时修复说明.md` | `LedBar.c` | CONFLICT | 标题/文档称 3 秒 | 当前 `LEDBAR_KEY_LONG_PRESS_10MS 50u`，约 500ms | 中 | 标注冲突，需确认真实长按需求 |
| `LEDBAR_RUNTIME_REFACTOR.md` | `LedBar.c` | PARTIAL | 文档提 `s_ledbar_runtime` 命名，当前源码为 `s_ledbar` | 行为大体一致，命名过时 | 低 | 合并行为，归档旧命名 |
| `docs/LEDBAR_SYSTEM_STATE_REFACTOR_2026-05-25.md` | `LedBar.c`, `System_Monitor.c` | PARTIAL | 文档描述系统状态收口，需逐字段复核 | 当前 LedBar runtime 已集中，但 fault 显示分支为空 | 中 | 合并确认部分，保留问题 |
| `docs/low_power_rtc_final_report.md` | `app_lowpower.c`, `rtc_sleep.c` | PARTIAL | 文档部分行号/阶段描述已过期 | 当前已有 `LP_State_t` 和 block reason 位图 | 低 | 合并当前行为，旧报告归档 |

## 3. 文档缺失：源码已有但旧文档未统一说明

| 缺失项 | 对照源码文件 | 一致性状态 | 不一致内容 | 源码真实行为 | 风险 | 建议处理 |
|---|---|---|---|---|---|---|
| 量产主路径虚拟电流 | `DataDeal.c:1063-1085` | 已按当前源码修正 | 旧文档曾记录主路径调用 `test_Autocurrent_cycle()` | 当前主路径调用 `DataLoad_Current()`，测试电流入口必须保持隔离 | 高 | 已在当前权威 review 文档中修正；后续做门禁防止回流 |
| `0xC002` 产品信息固定 48 寄存器 | `Sci_Upper.c:769-773` | PARTIAL | 上位机文档有要求，固件权威 docs 缺少统一说明 | 当前读取 SN/HW/SW 各 16 bytes | 中 | 合并到 `protocol_design.md` |
| CAN RTC wake service | `Can_HDX.c:952-979` | PARTIAL | 多份旧 CAN/低功耗文档互相冲突 | RTC 唤醒可短时发 CAN 周期帧 | 中 | 合并到 `low_power_design.md` 并标待确认 |
| AFE 均衡开压硬编码 | `SH367309_DataDeal.c:58-59` | CONFLICT | 均衡参数文档默认可写即生效 | 源码硬编码 4160 | 高 | 列入待确认 |
| Host 写权限 release 开启 | `Project_Config.h:45` | PARTIAL | 部分文档谈写权限，但缺少量产风险总表 | 当前写保护/Other/AFE/IAP 等入口存在 | 高 | 合并到 `protocol_design.md` 风险 |

## 4. 无法从当前源码确认的文档

| 文档路径 | 对照源码文件 | 一致性状态 | 不一致内容 | 源码真实行为 | 风险 | 建议处理 |
|---|---|---|---|---|---|---|
| `docs/COMM_TOOL_SERIAL_PROTOCOL.md` | `firmware/comm_tool_f103ret6` | UNKNOWN | 本轮主目标是 BMS App，未深入核 comm tool 工程 | 不判定 | 中 | 保留为工具文档，后续单独审查 |
| `docs/BMS_CAN_IAP_PROTOCOL.md` | `firmware/comm_tool_f103ret6`, IAP 工程 | UNKNOWN | 未按 IAP 工程逐行核对 | BMS App 只负责进入 IAP | 中 | 保留参考，BMS App 只写进入 IAP 行为 |
| `SH3673520+STM32F072CBT6 DemoCode...` 内文档 | Demo 源码 | UNKNOWN | 外部参考工程，不代表当前 App | 不判定 | 低 | 不纳入当前 docs 权威体系 |

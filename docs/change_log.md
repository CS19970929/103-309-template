# 变更记录

文档状态：已按源码部分验证
最后更新时间：2026-06-02
说明：长期详细变更记录见 `docs/changelog/change_log.md`；本文件按仓库协作规则保留为顶层入口。

## 2026-06-02 状态变量净删减专项审计

本次只做文档和源码只读审查，未修改 `.c/.h`、Keil 工程、编译宏、协议、IAP/App 地址和烧录脚本。

更新内容：

- 新增 `docs/review/state_variable_audit.md`，按当前源码梳理 `s_ledbar.initialized`、`g_stLowPowerRtcStatus.readyToSleep`、LedBar 防抖状态、低功耗计时、日志边沿、系统状态、DataDeal 客户逻辑状态等变量。
- 更新 `docs/review/requirement_confirmation.md`，新增 `REQ-SV-*` 状态变量净删减需求确认表。
- 更新 `docs/review/requirement_questions.md`，新增 `Q-SV-*` 确认问题，并修正旧的 AFE/SOC 电流主路径描述：当前 `App_AFEGet()` 已调用 `DataLoad_Current()`。
- 更新 `docs/review/refactor_plan.md`，新增阶段 11 状态变量净删减专项阶段。
- 更新 `docs/review/risk_list.md`，新增状态变量净删减风险。
- 更新 `docs/review/test_plan.md`，新增状态变量净删减专项测试项。
- 更新 `docs/review/full_project_review.md`、`docs/review/module_map.md`、`docs/design/soc_design.md`、`docs/design/adc_afe_design.md`、`docs/review/document_source_consistency.md`，同步当前调度链与 AFE/SOC 数据流事实。

后续源码修改前必须先确认 `REQ-SV-*` 或 `Q-SV-*`。

## 2026-06-02 SV-01 产品信息初始化低风险净删减

本批次只处理 `ProductionID.c` 的一次性初始化 flag，不修改 Modbus/CAN 协议、`0xC002` 字段、产品信息默认值、Flash/IAP 地址、SOC、低功耗和 LED。

源码修改：

- `AppInit.c`：启动运行态初始化阶段显式调用 `InitProID()`。
- `ProductionID.h`：声明 `InitProID()`。
- `ProductionID.c`：删除 `App_ProID_Deal()` 内部 `su8_StartUpFlag` 和懒初始化逻辑；保留空 hook 供 `Runtime.c` 维持 `DBG_MODULE_PROID` heartbeat。

文档修改：

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/review/module_map.md`、`docs/reference/module_reference.md`，将 `Q-SV-004 / SV-01` 标记为已执行，并同步 PROID heartbeat hook 口径。

验证：

- `rg -n "su8_StartUpFlag" "103 + 309/Project/Source"`：源码无残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `AppInit.c` 和 `ProductionID.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板运行或上位机 `0xC002` 读取。

## 2026-06-02 SV-STRUCT-01 FactoryAging 运行态结构体收口

本批次只处理 `FactoryAging.c` 单文件私有运行态变量，不修改老化业务状态机、BKP/Flash 保存格式、CAN/Modbus 可见接口、低功耗策略、MOS 控制函数和配置宏。

源码修改：

- `FactoryAging.c`：新增 `FactoryAgingRuntime`。
- `FactoryAging.c`：将原 10 个散落的 `s_u*FactoryAging*` 静态变量收口为 `s_factory_aging.state/elapsed10ms/lastTick/lastBkpSave10ms/lastFlashSave10ms/nextFinishRetry10ms/durationHours/bkpSaveValid/flashSaveValid/mosMode`。

文档修改：

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md` 和 `docs/test_plan.md`，补充 `SV-AGING-001 / SV-STRUCT-01 / REQ-SV-007`。

验证：

- `rg -n "s_u8FactoryAging|s_u16FactoryAging|s_u32FactoryAging" "103 + 309/Project/Source/FactoryAging.c"`：源码无旧符号残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `FactoryAging.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板老化 start/stop/reset/set hours、CAN `0x14F80208` 广播或上位机老化时间读取。

## 2026-06-02 SV-STRUCT-02 LogRecord 运行态结构体收口

本批次只处理 `LogRecord.c` 单文件私有日志状态，不修改日志事件编码、事件记录格式、Flash 保存格式、Modbus 事件读取接口、低功耗 sleep 日志触发接口和 `su32_Interval_S_Tcnt` 外部补偿符号。

源码修改：

- `LogRecord.c`：新增 `LogRecordRuntime`。
- `LogRecord.c`：将原 `BMS_LOG_POINT`、`BMS_LOG_RECORD`、`s_log_record_flag`、日志 uptime、重复保存抑制数组、事件 latch 和 CBC 温度变化缓存收口为 `s_log_record`。
- `LogRecord.c`：保留 `su32_Interval_S_Tcnt`，因为 `rtc_sleep_port.c` 仍用它补偿 RTC sleep 秒数。

文档修改：

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md` 和 `docs/test_plan.md`，补充 `SV-STRUCT-02 / REQ-SV-008`。

验证：

- `rg -n "BMS_LOG_POINT|BMS_LOG_RECORD|s_log_record_flag|s_u32_LogRecord|s_u8_LogRecord|su8_Event|su8_CBC_Temp" "103 + 309/Project/Source/LogRecord.c"`：源码无旧私有符号残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `LogRecord.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板 startup/sleep/fault 日志、事件读取或 reset event record 测试。

## 2026-06-02 SV-STRUCT-03 AFE current zero 运行态结构体收口

本批次只处理 `DataDeal.c` 中 AFE current zero 私有状态，不修改 CADC 读取、mA/A10 换算公式、自动零点学习算法、输出 deadband、200ms 采样节拍、`g_u32AfeCurrentSampleSeq`、SOC 积分触发和 Modbus/CAN 电流字段。

源码修改：

- `DataDeal.c`：新增 `AFE_CURRENT_RUNTIME`。
- `DataDeal.c`：将启动冷/热零点参数选择、zero offset Q4、last raw、stable count、ready 和 zero state 收口为 `s_afe_current`。

文档修改：

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md` 和 `docs/test_plan.md`，补充 `SV-STRUCT-03 / REQ-SV-009`。

验证：

- `rg -n "s_i32AfeCurrent|s_u8AfeCurrent" "103 + 309/Project/Source/DataDeal.c"`：源码无旧私有符号残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `DataDeal.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板冷/热启动零点、真实充放电方向、`0xD000` 电流或 SOC sample seq 实测。

## 2026-06-02 SV-CLEAN-02 LedBar 显式初始化收口

本批次只处理 LedBar 主调度入口的初始化时序，不删除 `s_ledbar.initialized`，不修改 Charlieplexing 路由、显示窗口、防抖、TIM4 扫描、STOP 前 GPIO、RTC 睡前/唤醒后恢复链和低功耗判定。

源码修改：

- `AppInit.c`：在启动运行态初始化阶段显式调用 `LedBar_Init()`。
- `LedBar.c`：删除 `APP_LedBar()` 入口的 `LedBar_EnsureInit()` 懒初始化调用。
- `LedBar.c`：保留 `LedBar_EnsureInit()`、`s_ledbar.initialized`、外部 API 和 TIM4 ISR 防护；`LedBar_Init()` 不启动 TIM4，TIM4 仍只由 `LedBar_StartScanTimer()` 打开。

低功耗边界：

- RTC STOP 前仍走 `RtcSleep_PortPrepareRtcStop()` -> `IOstatus_RTCMode()` -> `LedBar_PrepareForStop()`，负责关扫描、灭灯和 GPIO 低漏电准备。
- RTC STOP 唤醒后仍走 `RtcSleep_PortRestoreAfterStop()` -> `InitRunAfterStopWakeup()`，恢复时钟、IO、ADC、USART/SCI、CAN、TIM3 和 AFE IIC；本批次不在唤醒恢复中调用 `LedBar_Init()`，避免周期唤醒重置显示窗口、防抖和扫描运行态。

文档修改：

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md` 和 `docs/test_plan.md`，将 `SV-CLEAN-02 / REQ-SV-001 / Q-SV-001` 标记为已执行，并补充 RTC 睡前/唤醒后不完整重置 LedBar runtime 的边界。

验证：

- `rg -n "LedBar_Init\\(|LedBar_EnsureInit\\(|void APP_LedBar" "103 + 309/Project/Source/AppInit.c" "103 + 309/Project/Source/LedBar.c"`：确认启动阶段显式初始化，`APP_LedBar()` 不再懒初始化，外部 API/ISR 防护保留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `AppInit.c` 和 `LedBar.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板 LED 启动/按键/TIM4 扫描、STOP 电流或 RTC 唤醒恢复实测。

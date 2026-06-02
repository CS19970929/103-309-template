# 文档变更记录

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：当前主工程源码和 `docs/review/*`
最后更新时间：2026-06-02
未确认事项：`NEED_CONFIRM` 文档仍需用户确认是否保留；部分旧文档仍被 `tools/project_check.py` 固定引用。

## 2026-06-02 状态变量净删减专项审计

### 本次文档修改

- 新增 `docs/review/state_variable_audit.md`，按当前源码梳理状态变量职责、可删候选、必须保留历史状态和第一批建议批次。
- 更新 `docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`，新增 `REQ-SV-*` / `Q-SV-*` 状态变量净删减确认、风险和测试项。
- 更新 `docs/review/full_project_review.md`、`docs/review/module_map.md`、`docs/design/soc_design.md`、`docs/design/adc_afe_design.md`、`docs/review/document_source_consistency.md`，修正旧的 AFE/SOC 电流主路径描述；当前 `App_AFEGet()` 已调用 `DataLoad_Current()`。
- 新增顶层入口 `docs/change_log.md` 和 `docs/test_plan.md`，用于满足仓库协作规则并指向长期文档。
- 更新 `docs/README.md` 和 `docs/INDEX.md`，加入状态变量专项入口。

### 边界说明

- 本次未修改 `.c/.h`、Keil 工程、编译宏、协议、Flash/IAP 地址和烧录脚本。
- 后续源码净删减前必须先确认 `REQ-SV-*` 或 `Q-SV-*`。
- 第一批建议只处理 `ProductionID.c` 一次性 flag 或 LedBar 显式初始化这类低风险项；`readyToSleep` 需先确认 sleep commit 顺序。

## 2026-06-02 SV-01 产品信息初始化低风险净删减

### 本次源码修改

- `AppInit.c`：在启动运行态初始化阶段显式调用 `InitProID()`，让 `0xC002` 默认产品信息在进入主循环前准备好。
- `ProductionID.h`：补充 `InitProID()` 声明。
- `ProductionID.c`：删除 `App_ProID_Deal()` 内部 `su8_StartUpFlag` 和懒初始化逻辑；保留空 hook，维持 `Runtime.c` 中 `DBG_MODULE_PROID` heartbeat。

### 本次文档修改

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/review/module_map.md`、`docs/reference/module_reference.md` 和 `docs/change_log.md`，将 `Q-SV-004 / SV-01` 标记为已执行，并同步 PROID heartbeat hook 口径。

### 安全边界

- 未修改 `0xC002` 寄存器地址、SN/HW/SW 字段顺序、默认字符串、Modbus/CAN 协议、Flash/IAP 地址、SOC、低功耗、LED 和 AFE。
- 真板/上位机 `0xC002` 读取仍需后续实测确认。

### 本次验证

- `rg -n "su8_StartUpFlag" "103 + 309/Project/Source"`：源码无残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `AppInit.c` 和 `ProductionID.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板运行或上位机 `0xC002` 读取。

## 2026-06-02 SV-STRUCT-01 FactoryAging 运行态结构体收口

### 本次源码修改

- `FactoryAging.c`：新增 `FactoryAgingRuntime`，将老化模块原有 10 个文件级静态变量收口为 `s_factory_aging` 字段。
- 字段覆盖老化状态、累计 10ms、最近 tick、BKP/Flash 保存节流、完成失败重试、用户配置时长和 MOS 模式缓存。

### 本次文档修改

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/test_plan.md` 和 `docs/change_log.md`，补充 `SV-AGING-001 / SV-STRUCT-01 / REQ-SV-007`。

### 安全边界

- 未修改老化状态机、BKP/Flash 持久化格式、CAN/Modbus 协议、`0x14F80208` 广播字段、低功耗策略、MOS 控制函数、Flash/IAP 地址和配置宏。
- 本批次只是变量所有权收口，便于 Keil Watch 观察 `s_factory_aging`。

### 本次验证

- `rg -n "s_u8FactoryAging|s_u16FactoryAging|s_u32FactoryAging" "103 + 309/Project/Source/FactoryAging.c"`：源码无旧符号残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `FactoryAging.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板老化 start/stop/reset/set hours、CAN `0x14F80208` 广播或上位机老化时间读取。

## 2026-06-02 SV-STRUCT-02 LogRecord 运行态结构体收口

### 本次源码修改

- `LogRecord.c`：新增 `LogRecordRuntime`，将日志模块私有运行态收口到 `s_log_record`。
- 收口字段包括事件记录点、事件记录数组、startup/sleep 请求 flag、运行秒计数、重复保存抑制数组、普通事件 latch 和 CBC 温度变化缓存。
- 保留 `su32_Interval_S_Tcnt` 外部符号，因为 `rtc_sleep_port.c` 仍通过它补偿 RTC sleep 秒数。

### 本次文档修改

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/test_plan.md` 和 `docs/change_log.md`，补充 `SV-STRUCT-02 / REQ-SV-008`。

### 安全边界

- 未修改日志事件编码、事件记录格式、Flash 保存格式、Modbus 事件读取接口、低功耗 sleep 日志触发接口、`su32_Interval_S_Tcnt` 跨模块补偿语义、Flash/IAP 地址和配置宏。

### 本次验证

- `rg -n "BMS_LOG_POINT|BMS_LOG_RECORD|s_log_record_flag|s_u32_LogRecord|s_u8_LogRecord|su8_Event|su8_CBC_Temp" "103 + 309/Project/Source/LogRecord.c"`：源码无旧私有符号残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `LogRecord.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板 startup/sleep/fault 日志、事件读取或 reset event record 测试。

## 2026-06-02 SV-STRUCT-03 AFE current zero 运行态结构体收口

### 本次源码修改

- `DataDeal.c`：新增 `AFE_CURRENT_RUNTIME`，将 AFE current zero 私有运行态收口到 `s_afe_current`。
- 收口字段包括启动 cold/warm zero 参数选择、zero offset Q4、last raw signed、stable count、ready 和 zero state。

### 本次文档修改

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/test_plan.md` 和 `docs/change_log.md`，补充 `SV-STRUCT-03 / REQ-SV-009`。

### 安全边界

- 未修改 CADC 读取、mA/A10 换算公式、自动零点学习算法、输出 deadband、200ms 采样节拍、`g_u32AfeCurrentSampleSeq`、SOC 积分触发、Modbus/CAN 电流字段、Flash/IAP 地址和配置宏。

### 本次验证

- `rg -n "s_i32AfeCurrent|s_u8AfeCurrent" "103 + 309/Project/Source/DataDeal.c"`：源码无旧私有符号残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `DataDeal.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板冷/热启动零点、真实充放电方向、`0xD000` 电流或 SOC sample seq 实测。

## 2026-06-02 SV-CLEAN-02 LedBar 显式初始化收口

### 本次源码修改

- `AppInit.c`：在启动运行态初始化阶段显式调用 `LedBar_Init()`。
- `LedBar.c`：删除 `APP_LedBar()` 入口的 `LedBar_EnsureInit()` 懒初始化调用。
- `LedBar.c`：保留 `LedBar_EnsureInit()`、`s_ledbar.initialized`、外部 API 和 TIM4 ISR 防护；`LedBar_Init()` 不启动 TIM4，TIM4 仍只由 `LedBar_StartScanTimer()` 打开。

### 本次文档修改

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/test_plan.md` 和 `docs/change_log.md`，将 `SV-CLEAN-02 / REQ-SV-001 / Q-SV-001` 标记为已执行。

### 安全边界

- 未删除 `s_ledbar.initialized`，不修改 Charlieplexing 路由、显示窗口、防抖、TIM4 扫描、低功耗阻塞判定、STOP 前 GPIO 和 RTC 睡前/唤醒后恢复链。
- RTC STOP 前仍由 `RtcSleep_PortPrepareRtcStop()` -> `IOstatus_RTCMode()` -> `LedBar_PrepareForStop()` 负责关扫描、灭灯和 GPIO 低漏电准备。
- RTC STOP 唤醒后仍由 `RtcSleep_PortRestoreAfterStop()` -> `InitRunAfterStopWakeup()` 恢复时钟、IO、ADC、USART/SCI、CAN、TIM3 和 AFE IIC；本批次不在唤醒恢复中调用 `LedBar_Init()`，避免周期唤醒重置显示窗口、防抖和扫描运行态。

### 本次验证

- `rg -n "LedBar_Init\\(|LedBar_EnsureInit\\(|void APP_LedBar" "103 + 309/Project/Source/AppInit.c" "103 + 309/Project/Source/LedBar.c"`：确认启动阶段显式初始化，`APP_LedBar()` 不再懒初始化，外部 API/ISR 防护保留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `AppInit.c` 和 `LedBar.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板 LED 启动/按键/TIM4 扫描、STOP 电流或 RTC 唤醒恢复实测。

## 2026-06-02 SV-CLEAN-03 readyToSleep 低功耗提交收口

### 本次源码修改

- `rtc_sleep.h`：删除 `LOW_POWER_RTC_STATUS.readyToSleep` 字段，删除 `LowPower_IsToSleepPending()` 和 `LowPower_ClearToSleepFlag()` 声明。
- `rtc_sleep.c`：删除 ready 置位/二次判断/清除流程；`rtc_sleep()` 改为读取本地 `sleep_mode = g_stLowPowerRtcStatus.mode` 后直接提交 HICCUP/NORMAL/DEEP。
- `LedBar.c`：删除 `APP_LedBar()` 中对 `LowPower_IsToSleepPending()` 的分支；reset sleep 的 SOC 保存仍由 `SleepDeal_Continue()` -> `LowPowerSleep_SaveResetState()` 完成，HICCUP STOP 前 GPIO 仍由 `LedBar_PrepareForStop()` 完成。
- `LogRecord.c`：删除 `BMS_SLEEP` 日志保存后清低功耗 ready 的跨模块副作用。
- `Runtime.c` / `SystemDebug.c`：低功耗 busy/`g_dbg.lp.ready` 改为由 `mode != NO_SLEEP` 派生。
- `tools/stlink_bms_monitor.ps1`：按新 `LOW_POWER_RTC_STATUS` 布局解析 `mode/blockReason/rtcWake`，`RtcReady` 改为由 `mode != NO_SLEEP` 派生。
- `tools/project_check.py`：门禁改为检查旧 ready API 消失、`rtc_sleep()` 使用本地 `sleep_mode` 提交。

### 本次文档修改

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/review/low_power_requirement_alignment_2026-06-02.md`、`docs/review/full_project_review.md`、`docs/review/module_map.md`、`docs/reference/module_reference.md`、`docs/reference/global_variables.md`、`docs/test_plan.md` 和 `docs/change_log.md`，将 `SV-CLEAN-03 / REQ-SV-002 / Q-SV-002` 标记为已执行。

### 安全边界

- 未修改 HICCUP STOP 执行器、reset sleep 提交点、BMS_SLEEP 日志格式、sleep SOC 保存、CAN/AFE/ADC/TIM 恢复顺序、协议寄存器和 IAP/App 地址。
- `SystemDebug.g_dbg.lp.ready` 与 ST-Link `RtcReady` 只是派生观察值，不再是控制字段。

### 本次验证

- `rg -n "readyToSleep|LowPower_IsToSleepPending|LowPower_ClearToSleepFlag" "103 + 309/Project/Source" tools`：源码旧 ready 字段/API 无残留；`todo.md` 用户记录和 project_check 规则说明除外。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `rtc_sleep.c`、`LedBar.c`、`LogRecord.c`、`Runtime.c`、`SystemDebug.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和旧文档引用，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板 HICCUP/NORMAL/DEEP、STOP 电流、BMS_SLEEP 日志读取、sleep SOC 读取或 ST-Link 监控实测。

## 2026-06-02 低功耗 review 问题修复

### 本次源码修改

- `Can_HDX.c/.h`：新增无副作用 `Can_PeekBusy()`；`Can_IsBusy()` 保留给低功耗路径，用于确认并消费 CAN 接收活动。
- `SystemDebug.c` / `Runtime.c`：debug 状态刷新和 CAN heartbeat 改用 `Can_PeekBusy()`，避免在 `rtc_sleep()` 前消耗 CAN RX 变化。
- `Project_Config.h` / `System_Init.c`：`PROJECT_CFG_WDOG_ENABLE` 默认改回 `1`；`Init_IWDG()` 和 `IWDG_Feed()` 都按该宏门控，和 RTC wake 安全窗口保持一致。
- `rtc_sleep.c` / `rtc_sleep_port.c/.h`：删除 `RtcSleep_PortRequestSleepLog()` 和 reset sleep prepare helper；reset sleep 日志请求、事件记录和 `SleepDeal_Continue()` 收口到一个提交点，避免重复准备动作。

### 本次文档修改

- 更新 `docs/design/low_power_design.md`、`docs/review/low_power_requirement_alignment_2026-06-02.md`、`docs/review/risk_list.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/test_plan.md`、`docs/reference/module_reference.md`，同步 CAN busy 语义、IWDG 门控和 reset sleep 提交边界。

## 2026-06-02 低功耗官方调研与第一批源码简化

### 本次源码修改

- `conf/conf.h`：删除无条件 `__EnableLowPowerDebug__`，Release 默认由 `EnableLowPowerDebug()` 清除 DBGMCU 低功耗调试保持位；需要 STOP 内调试时必须显式定义该宏。
- `app_lowpower.c/.h`：删除未被主路径调用的 `LP_SetWakeupPeriod()`、`LP_BeforeSleep()`、`LP_AfterWakeup()`、`LP_EnterStop()`。
- `app_lowpower.c/.h`：删除无消费者的 `LP_State_t`、`LP_GetState()` 和 `s_lp_runtime.state` 缓存。
- `app_lowpower.c/.h`：删除整个独立 wrapper 模块；`LP_BLOCK_*`、`LP_GetBlockReason()`、`LP_GetLastSleepSeconds()`、`LP_RecordLastSleepSeconds()` 收口到 `rtc_sleep.c/h`，Keil 工程同步移除 `app_lowpower.c`。
- `rtc_sleep.c`：删除无源码调用的 `LP_CanSleep()`，直接用 `LP_GetBlockReason()` 映射粗粒度 block reason，避免电流/按键判断写两遍。
- `Runtime.c`：删除一行 wrapper `LP_Task()`，运行态低功耗主路径收敛为 `Runtime_RunOnce()` 直接调用 `rtc_sleep()`。
- `SystemDebug.c`：删除已不存在的 `LP_BLOCK_AFE_BUSY` bit4 打印分支。
- `rtc_sleep.h`：删除当前不会触发的 `LP_BLOCK_AFE_BUSY` 和 `LP_BLOCK_IWDG_UNSAFE`，保留真实使用的 block bit。
- `rtc_sleep.c/.h`：删除未调用的 `low_power_cancel_rtc()`、`low_power_is_idle_rtc_request()`、`get_rtc_soc()`、`set_rtc_soc()` 和无消费者 `s_u8RtcSoc`。
- `rtc_sleep_port.c/.h`、`rtc_sleep_afe_port.h`、`rtc_sleep_afe_sh367309.c`：删除未使用的 `RtcSleep_PortGetCellMaxMv()`、`RtcSleep_PortIsFactoryAgingActive()`、`RtcSleep_PortIsAfeSleepBlocked()` 和 `RtcSleep_AfePortIsSleepBlocked()`。
- `rtc_sleep.c` / `rtc_sleep_port.c/.h`：`RtcSleep_PortPrepareRtcStop()` 删除未使用参数；`RtcSleep_PortApplySocRtcRest()` 改为 `void`，只保留 SOC 休眠补偿副作用。
- `rtc_sleep.c`：按用户确认接入老化阻塞 RTC 策略，`FactoryAging_IsActive()` 只阻止空闲进入 `HICCUP_MODE` RTC STOP，不阻止低压或外部请求的 `DEEP_MODE/NORMAL_MODE` reset sleep。
- `tools/stlink_bms_monitor.ps1`：不再强制依赖旧 `s_lp_runtime` 符号；旧 ELF 存在该符号时只读两个 word，新 ELF 没有该符号时继续运行。

### 本次文档修改

- 新增 `docs/review/low_power_official_industry_research_2026-06-02.md`，汇总 STM32 官方低功耗逻辑、BMS 行业低功耗分层和当前项目映射。
- 更新 `docs/review/low_power_requirement_alignment_2026-06-02.md`、`docs/design/low_power_design.md`、`docs/review/requirement_questions.md`、`docs/review/risk_list.md`、`docs/review/refactor_plan.md`、`docs/review/test_plan.md`、`docs/reference/module_reference.md`，同步第一批已处理项和剩余待确认风险。

### 安全边界

- 未修改 Modbus/CAN 协议、CAN ID、寄存器地址、IAP/App 地址、AFE sleep 行为、老化逻辑、按键/拔 5V 产品交互和 Flash/EEPROM 存储格式。
- 低功耗主路径为 `Runtime_RunOnce()` -> `rtc_sleep()`。
- AFE not idle、`OtherElement` 普通休眠/RTC 参数仍需确认后再决定接入或删除；FactoryAging active 已确认并按“只阻塞 RTC、不阻塞 deep/reset sleep”接入。

### 本次验证

- `rg` 确认源码和 Keil 工程中无 `app_lowpower.c/h`、`LP_SetWakeupPeriod`、`LP_EnterStop`、`LP_BeforeSleep`、`LP_AfterWakeup`、`LP_GetState`、`LP_CanSleep`、`LP_Task`、`LP_STATE_*`、`LP_BLOCK_AFE_BUSY`、`LP_BLOCK_IWDG_UNSAFE`、`get_rtc_soc`、`set_rtc_soc`、`s_u8RtcSoc`、`s_lp_runtime` 残留引用。
- `git diff --check` 在本次白名单源码、Keil 工程、工具和文档范围通过；全仓检查仍受用户已有 `todo.md` trailing whitespace 影响。
- `clang -fsyntax-only` 检查 `rtc_sleep.c`、`rtc_sleep_port.c`、`rtc_sleep_afe_sh367309.c`、`System_Init.c`、`SystemDebug.c`、`Runtime.c`、`AppInit.c` 通过，使用 `STM32F10X_MD`、`USE_STDPERIPH_DRIVER`、当前 StdPeriph include 路径。
- `tools/project_check.py --quiet` 仍是当前仓库基线失败：`87 OK / 1 warning / 41 errors`，主要来自缺历史固定文件、编码和配置宏/BuildGuard 检查。
- 未执行 Keil `FD_Release` 编译、烧录、STOP 电流、ST-Link `DBGMCU->CR` 读取和 COM/CAN 实测。

## 2026-06-02 低功耗需求与实现对齐

### 本次文档修改

- 新增 `docs/review/low_power_requirement_alignment_2026-06-02.md`，按当前源码梳理低功耗启动链路、运行态 HICCUP STOP、reset sleep、唤醒恢复、BKP 分配、当前电源控制和需求确认表。
- 更新 `docs/design/low_power_design.md`，标记当前源码与旧设计口径的差异，包括 DBGMCU Release 位、IWDG 宏与实际启用、FactoryAging/AFE not idle 阻塞、Sleep 参数有效性。
- 更新 `docs/review/requirement_questions.md`，新增低功耗简化前必须确认的问题。
- 更新 `docs/review/risk_list.md`，新增 IWDG、DBGMCU、老化、AFE、Sleep 参数和未使用 wrapper 风险。
- 更新 `docs/review/test_plan.md`，补充 DBGMCU、IWDG、老化、AFE、Sleep 参数和 HICCUP 前 AFE 功耗状态测试项。
- 更新 `docs/review/refactor_plan.md`，把 RTC 低功耗/IWDG 阶段拆成先确认、再小批次净删减的计划。

### 边界说明

- 本次未修改源码、协议、Keil 工程、编译宏、IAP/App 地址和烧录脚本。
- 当前目标是需求对齐，不做低功耗行为修复；后续任何源码简化必须先逐条确认需求表。
- `tools/project_check.py --quiet` 当前仍是仓库基线失败：`87 OK / 1 warning / 41 errors`，主要是缺历史固定文档、缺 `easylogger/inc/elog_cfg.h`、编码/宏检查等，不是本次文档整理引入。

## 2026-06-02 CAN 运行态调度和 debug 快照继续简化

### 本次源码修改

- `Can_HDX.c`：删除运行态 `bus_active/no_ack_cnt/probe_active/last_probe_tick` 及相关 active/probe/no-ACK 软件退避逻辑。
- `Can_HDX.c`：运行态固定按 1000ms/5000ms 周期调度飞道广播；发送成功、失败和超时只释放 mailbox/queue，不再切换探测模式。
- `CanFeidaoFrames.h`：删除 `CAN_FEIDAO_RTC_PROBE_MSG_MASK`，保留 1000ms/5000ms 周期 mask。
- `SystemDebug.h/.c`、`Can_HDX.h/.c`：`g_dbg.can` 和 `Can_GetDebugSnapshot()` 收敛为 `power_on/bus_off/tx_queue/esr` 四个字段。
- `Project_Config.h`：删除不再使用的 CAN active hold 配置块。
- `tools/project_check.py`：新增旧 active/probe/no-ACK 状态和 debug 占位字段回流检查。

### 兼容性说明

- 未修改 CAN ID、payload、App `0x60/0x61` 命令、Modbus 寄存器桥接和 IAP/App 地址。
- `CAN_NART = ENABLE` 仍关闭硬件自动重发；无 ACK 时不做软件退避，只按固定周期继续尝试。
- `CAN_ABOM = ENABLE` 保留；bus-off 当前状态仍可通过 `CAN1->ESR` 和 `g_dbg.can.bus_off` 观察。

## 2026-06-02 CAN 电源、RTC 休眠和 bus-off 简化

### 本次源码修改

- `Can_HDX.c/.h`：删除 `Can_RtcWakeService()`、`Can_GetIdleRtcPeriodSeconds()`、`Can_IsBusActive()` 和软件 bus-off monitor。
- `Can_HDX.c`：运行态初始化打开 `GPIO_CMNT_EN`；`Can_PrepareSleep()` 睡前关闭 CMNT；debug 的 bus-off 位改为只读 `CAN1->ESR`。
- `RTC.c`：默认 RTC wake period 固定为 10s，不再向 CAN 查询 active/idle 周期。
- `rtc_sleep.c` / `rtc_sleep_port.c/.h`：RTC HICCUP 周期唤醒后不再调用 CAN 周期服务。
- `Project_Config.h`：删除 `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS`。
- `tools/project_check.py`：门禁改为检查 RTC CAN 服务已删除、ABOM 保留、CMNT 睡前关闭。

### 兼容性说明

- 未修改 CAN ID、payload、App `0x60/0x61` 命令、Modbus 寄存器桥接和 IAP/App 地址。
- RTC STOP 中不再周期广播 CAN；唤醒恢复后由 `InitCan()` 打开 CMNT 并恢复运行态通信。
- bus-off 恢复依赖 `CAN_ABOM = ENABLE`，软件不再统计进入/恢复次数。

## 2026-06-02 SystemDebug 模块健康总览增强

### 本次源码修改

- `SystemDebug.h`：新增 `DBG_MODULE_ID`、`DBG_MODULE_STATE_*` 和 `g_dbg.module`，用于按模块观察运行心跳、ready、busy、error 和 stale 状态。
- `SystemDebug.c`：新增 `SystemDebug_ModuleHeartbeat()`，记录每个模块的 `last_tick/max_gap_ticks/run_cnt`，并维护 `alive_mask/ready_mask/busy_mask/error_mask/stale_mask`。
- `Runtime.c`：在主循环任务执行后接入模块心跳，覆盖 `systime/aging/led/afe/soc/snapshot/sci/adc/low_power/can/flash/log/proid/watchdog/runtime` 等模块。
- `SystemDebug.c`：根据已有系统错误、CAN busoff、Flash busy、低功耗 ready、保护 fault 等状态刷新模块 `busy_mask` 和 `error_mask`。

### 安全约束

- 未修改业务流程、协议寄存器、CAN ID、CAN payload、IAP/App 地址和 SOC/AFE 参数。
- `PROJECT_CFG_DEBUG_MONITOR_ENABLE=0` 时 `SystemDebug_ModuleHeartbeat()` 为空实现。
- `stale_mask` 仅基于心跳 tick 判断，阈值为 200 个 10ms tick，约 2s。

### 本次验证

- Keil `FD_Release` build：`0 Error(s), 0 Warning(s)`。
- 生成 `103 + 309/Project/Users/Objects/FD_Release.bin`，大小 61100 bytes。

## 2026-06-02 SystemDebug MCU 资源、耗时和喂狗快照增强

### 本次源码修改

- `SystemDebug.h`：新增 `g_dbg.rcc`、`g_dbg.irq`、`g_dbg.periph`、`g_dbg.reset` 四个子结构体，用于 Keil Watch 展开查看 MCU 时钟、中断、外设寄存器和复位来源。
- `SystemDebug.h` / `Runtime.c`：新增 `g_dbg.profile`，记录整轮主循环、前台任务、IO/低功耗/CAN 任务、后台任务和 Debug 打印的 `last_us/max_us/call_cnt`。
- `SystemDebug.h` / `System_Init.c`：新增 `g_dbg.watchdog`，记录 `IWDG_Feed()` 次数、最近喂狗 tick、最近/最大喂狗间隔、IWDG PR/RLR/SR 和 IWDG reset 标志。
- `SystemDebug.c`：新增 `SystemDebug_SnapshotMcuResources()`，在 `SystemDebug_Snapshot()` 中只读采集 RCC、NVIC/SCB/SysTick、EXTI、USART、CAN、ADC、DMA1、TIM3/TIM4、FLASH、PWR 关键寄存器。
- `SystemDebug.c`：外设寄存器读取前先检查 RCC 对应时钟使能；未使能时字段填 0，避免把“外设未开”和“状态为 0”混淆。
- `System_Monitor.c`：修复删除 `ERROR_CAN` 后基础错误 offset 表未同步的问题，并将 `ERROR_REMOVE_*` / `ERROR_STATUS_*` 改为显式映射，避免枚举顺序变化导致错误位错读/错清。

### 安全约束

- 未读 USART `DR`、CAN FIFO 数据寄存器等有副作用的寄存器。
- 未清除 pending、reset、错误标志；`RCC->CSR` 仅做快照，复位来源仍保留给调试观察。
- 仅在 `PROJECT_CFG_DEBUG_MONITOR_ENABLE` 下生效，关闭该宏时 `SystemDebug` 为空实现。
- `ERROR_REMOVE_CAN` / `ERROR_STATUS_CAN` 当前不映射到任何基础错误位，符合去掉 CAN 通信错误位的方向，不会错映射到 EEPROM。

### 本次验证

- Keil `FD_Release` build：`0 Error(s), 0 Warning(s)`。
- 生成 `103 + 309/Project/Users/Objects/FD_Release.bin`，大小 59768 bytes。

### 兼容性说明

- 未修改 Modbus/CAN 协议、寄存器地址、CAN ID、payload、IAP 地址、AFE 参数和 SOC 算法。
- `g_dbg` 结构体布局发生扩展，仅用于调试 Watch；不作为对外通信协议。

## 2026-06-02 LedBar 初始化回归修复

### 本次源码修改

- `LedBar.c`：恢复 `LedBar_Init()` 的一次性初始化保护，避免 `APP_LedBar()` 每轮主循环重置显示窗口、按键滤波、扫描帧和 TIM4 状态。
- `LedBar.c`：恢复 TIM4 扫描定时器初始化状态保护，非空显示帧更新时不重复重配 TIM4，空帧/STOP 前仍会关闭扫描定时器和 GPIO。
- `LedBar.c`：恢复按键和 `MCU_WK` 二值滤波的首次采样预置，避免启动时把已有电平误判为新边沿。
- `SystemDebug.c`：保留 `g_dbg.soc.init_over` 调试字段布局，并在当前运行快照中固定填充为 1，避免打印未更新的旧值。

### 问题根因

`02bb091` 删除初始化完成类变量时，把 LedBar 运行态保护一并删除，导致 `Runtime_RunOnce()` 每轮调用 `APP_LedBar()` 时都会重新执行 `LedBar_Init()`。这会让 `startup_display_armed` 和 `soc_display_10ms` 无法自然保持/归零，表现为数码管持续闪烁，并可能让 `LP_BLOCK_LED_ACTIVE` 长时间阻塞低功耗。

### 本次验证

- Keil `FD_Release` rebuild：`0 Error(s), 0 Warning(s)`。
- 生成 `103 + 309/Project/Users/Objects/FD_Release.bin`，大小 58360 bytes。
- 上电后数码管只在启动显示窗口内显示，不应因为主循环重复初始化而持续闪烁。
- 单击/唤醒显示 SOC 后，显示窗口结束应熄屏并释放 `LP_BLOCK_LED_ACTIVE`。

### 兼容性说明

- 未修改 Modbus/CAN 协议、寄存器地址、CAN ID、payload、IAP 地址、AFE 参数和 SOC 算法。
- 未改变启动/按键/唤醒显示窗口时长，只恢复运行态保持。

## 2026-05-27 RTC/STOP 低功耗进不去修复

### 本次源码修改

- `LedBar.c`：修复 `LedBar_IsActiveForLowPower()`，`startup_display_armed` 只作为启动显示已触发标志，不再作为低功耗阻塞条件；真实显示窗口、帧扫描和扫描定时器仍会阻塞 STOP，保留用户显示体验。
- `System_Init.c` / `System_Init.h`：`EnableLowPowerDebug()` 在 Debug 构建打开低功耗调试保持，在 Release 构建显式清除 `DBGMCU_CR_DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP`。
- `AppInit.c`：启动阶段统一调用 `EnableLowPowerDebug()`，避免 Release 继承调试器残留的 DBGMCU 低功耗调试位。

### 本次验证

- Keil `FD_Release` rebuild：`0 Error(s), 0 Warning(s)`。
- 安全脚本烧录 App 到 `0x08004800`，未覆盖 IAP。
- ST-Link 读取确认 Release 下 `DBGMCU_CR = 0x00000000`。
- Release 继续运行后普通 ST-Link attach 失败，符合目标进入 STOP 且 DBG_STOP 关闭后的预期。

### 兼容性说明

- 未修改 Modbus/CAN 协议、CAN ID、payload、IAP 入口、AFE 保护配置和参数存储格式。
- 未关闭启动/唤醒 SOC 显示窗口，只修复窗口结束后的低功耗释放。

## 2026-05-27 ST-Link BMS 长期监控工具

### 本次新增

- `tools/stlink_bms_monitor.ps1`：长期监控板子、MCU 和 BMS 低功耗状态，支持 `ReleaseProxy` 和 `DebugProbe` 两种模式。
- `docs/STLINK_BMS_MONITOR_2026-05-27.md`：记录工具用途、命令、输出字段和 RTC STOP 检测限制。

### 设计说明

- `ReleaseProxy` 不打开 DBG_STOP，适合真实功耗监控，通过 ST-Link attach 失败判断目标大概率处于 STOP 或调试域关闭。
- `DebugProbe` 会尝试临时打开 `DBGMCU_CR_DBG_STOP`，用于在 RTC STOP 中读取 RAM 状态，但不适合作为功耗实测依据。

### 本次验证

- `ReleaseProxy -Count 1` 可正常输出 `LOW_POWER_OR_DBG_OFF` 或 `TIMEOUT_LOW_POWER_OR_DBG_OFF` 并生成 CSV/summary。
- `DebugProbe -Count 1 -DebugPrepareAttempts 1` 在板子已处于 Release STOP 时不会卡死，会提示需要唤醒/复位后再接入诊断监控。

### 后续增强

- attach 成功时额外读取 MCU ID、RCC/PWR/SCB fault 寄存器、BMS 电压/电流/SOC/SOH/fault、Type-C 电流、AFE 采样序号、Flash 写入标志、老化状态和低功耗参数。

## 2026-05-27 App_Can 低功耗优化

### 本次源码修改

- `Can_HDX.c`：关闭 bxCAN 自动重发，`CAN_NART = ENABLE`，避免无 ACK 时持续重发。
- `Can_HDX.c`：新增 CAN 收发器按需上电/断电，发送前等待 `PROJECT_CFG_CAN_POWER_STABLE_TICKS`，空闲后关闭 `GPIO_CMNT_EN`。
- `Can_HDX.c`：新增 no-ACK 退避；连续失败或超时达到阈值后停止完整业务广播，只保留轻量探测。
- `CanFeidaoFrames.h`：RTC/idle probe 探测帧缩减为单帧 `CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS`。
- `app_lowpower.c`：低功耗框架不再因 CAN bus active 永久阻塞 RTC STOP，只在 CAN busy 时阻塞。

### 本次文档更新

- `docs/CAN_APP_LOW_POWER_OPTIMIZATION_2026-05-27.md`

### 兼容性说明

- 未修改 CAN ID、payload、App 命令、Modbus 桥接、IAP 入口和老化时间广播含义。
- active 总线仍保持 1000 ms / 5000 ms 完整周期广播；idle 总线默认 10 s 轻量探测。

## 2026-05-27 CAN 低功耗配置与 idle sleep 预留

### 本次源码修改

- `Can_HDX.c`：将 RTC 唤醒 CAN 广播周期改为 `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS`，默认 `1s`，保持当前客户可见行为。
- `Can_HDX.c`：新增 CAN active 超时保持，`PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS` 默认 `10s`；最后一次 TX ACK 或 RX 帧后超时清除 active，避免历史 CAN active 永久阻塞低功耗。
- `Can_HDX.c`：CAN active 标志和时间戳按中断/主循环共享状态处理。
- `Runtime.c`：预留 STM32 运行态 idle sleep (`WFI`) 入口，只在系统 tick、SCI、CAN、Flash、IAP/参数写入均空闲时进入。
- `Project_Config.h`：新增 CAN 低功耗和 idle sleep 配置项；`PROJECT_CFG_IDLE_SLEEP_ENABLE` 默认 `0`，待硬件实测后再决定是否量产打开。

### 本次文档更新

- `docs/protocol/can_protocol.md`
- `docs/design/low_power_design.md`
- `docs/changelog/change_log.md`

### 兼容性说明

- 未修改 CAN ID、payload、App 命令、Modbus 桥接语义、IAP 入口。
- 未修改保护逻辑、MOS 控制、AFE 配置、参数存储格式。
- CAN RTC 周期广播默认仍为 `1s`。

## 2026-05-27 文档清理与合并

### 本次删除

- 删除确认已合并、过时、重复、临时或旧方案性质的 Markdown 文档 115 份。
- 删除 `TEST_PENDING copy.md`，保留 `TEST_PENDING.md`。
- 没有保留 `docs/archive/old_docs/` 旧文档副本；如需找回旧文档，可从 Git 历史恢复。

删除依据和完整清单见：

- `docs/review/document_cleanup_report.md`

### 本次新增

- `docs/protocol/modbus_register_map.md`
- `docs/protocol/can_protocol.md`
- `docs/protocol/uart_protocol.md`
- `docs/design/led_display_design.md`
- `docs/design/bootloader_iap_design.md`

### 本次更新

- `docs/README.md`
- `docs/archive/README.md`
- `docs/changelog/change_log.md`

### 本次源码修改

没有修改源码。

### 暂不删除

以下类型文档保留为 `NEED_CONFIRM`：

- 被 `tools/project_check.py` 固定引用的旧文档。
- 根目录协作、发布、调试、TODO 类流程文档。
- comm tool 子工程文档。
- 当前工作区已有用户修改的文档。

## 2026-05-26 文档体系整理

### 本次新增

- `docs/README.md`
- `docs/project_overview.md`
- `docs/architecture.md`
- `docs/module_map.md`
- `docs/design/storage_design.md`
- `docs/design/protocol_design.md`
- `docs/design/soc_design.md`
- `docs/design/adc_afe_design.md`
- `docs/design/low_power_design.md`
- `docs/test/test_plan.md`
- `docs/archive/README.md`
- `docs/review/document_inventory.md`
- `docs/review/document_source_consistency.md`
- `docs/review/document_duplicate_analysis.md`
- `docs/review/document_structure_plan.md`
- `docs/review/document_merge_plan.md`

### 本次源码修改

没有修改源码。

### 本次合并内容

本轮只做低风险“内容合并”和“权威入口创建”，没有移动、删除旧文档。

已合并到权威文档的主题：

- 项目总览和架构。
- Flash/EEPROM 兼容层和后 64K 存储。
- Modbus/CAN 通信关系。
- SOC 当前算法链路。
- ADC/AFE 当前数据流。
- RTC/低功耗/IWDG 当前行为。
- 全项目测试计划。

### 仍需确认

1. 是否归档旧低功耗阶段文档。
2. 是否归档旧 CAN 低功耗文档。
3. 是否归档旧外部 EEPROM 布局文档。
4. 是否后续删除 `TEST_PENDING copy.md`。
5. 是否补建 `docs/protocol/*`, `docs/design/led_display_design.md`, `docs/design/bootloader_iap_design.md`。

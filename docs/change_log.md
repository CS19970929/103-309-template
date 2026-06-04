# 变更记录

## 2026-06-04 其他模块简化审查与 LedBar 净删减

源码变更：
- `LedBar.c/.h`：删除无外部调用的 `LedBar_SetSingleSegmentIndex()`、`LedBar_SetIndicators()`、`LedBar_SetIndicatorState()`。
- `LedBar.c/.h`：删除无消费者的 `s_ledbar.test_single_segment_id` 和 `LEDBAR_SINGLE_SEG_ID_MIN/MAX`。

文档变更：
- 新增 `docs/review/module_simplification_review_2026-06-04.md`，记录其它模块简化审查结论。
- 更新 LedBar 模块参考和变量梳理，移除已删除测试状态/API。

行为边界：
- 不修改 `APP_LedBar()` 显示策略、`TIM4_IRQHandler()` 扫描链路、BKP 休眠 SOC 保存/恢复、低功耗阻塞条件和 SOC/协议发布口径。
- `Runtime.c`、`LowPowerSleep.c`、`rtc_sleep.c` 已审查但暂不修改，原因见审查文档。

## 2026-06-04 SOC 函数粒度净删减

源码变更：
- `SocEnhance.c`：删除一层 `soc_publish()` 转发包装，发布函数直接写 public 字段并刷新 Debug Watch。
- `SocEnhance.c`：将只用一次的积分模式判断、长静置 OCV 单步和重放电判断合并回调用点。
- `SocEnhance.c`：将 mid-tail 删除后遗留的泛化 tail 查表 helper 收敛为单个 `soc_low_tail_config()`。
- 修正 snapshot load/save 附近缩进噪声。

文档变更：
- 新增 `docs/review/soc_function_granularity_review_2026-06-04.md`，记录 SOC 函数粒度 review 规则、保留边界和本轮合并清单。
- 更新 `docs/design/soc_design.md` 的已处理问题表。

行为边界：
- 不修改 low-tail 表、满电/静置/RTC 阈值、Flash snapshot、Modbus/CAN/LedBar 发布字段和 200ms 调度顺序。

## 2026-06-04 SOC 删除 display_soc 平滑和 Fixed/Zero 覆盖

源码变更：
- `SocEnhance.c/.h`：删除 `display_soc/display_ticks/display_ready` 状态、显示平滑函数和 `u8DisplaySoc` debug watch 字段；`g_stCellInfoReport.SocElement.u16Soc` 直接发布内部 `s_soc.soc`。
- `System_Monitor.c/.h`、`Sci_Upper.c`：删除 SOC Fixed/Zero 对外覆盖逻辑；历史接收位只保留 bit 位置占位，不再影响 SOC。
- `conf/Project_Config.h`、`conf/Project_BuildGuard.h`、`tools/project_check.py`：删除 `PROJECT_CFG_SOC_DISPLAY_*` 平滑配置和门禁检查。
- `tools/soc_replay_test.py`、host C 测试、可视化报告和 SOC UI：同步为“发布 SOC = 内部 SOC”的单一口径。

文档变更：
- 更新 `docs/design/soc_design.md`、模块参考、宏参考、变量梳理、风险清单、测试计划和 debug 指南。

当前结论：
- 自动校准最大步长仍为 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT = 1`。
- CAN、Modbus、LedBar 读取到的 SOC 不再经过独立显示平滑；Fixed/Zero 历史功能位不再覆盖 SOC。

## 2026-06-04 SOC 文档合并为单一活跃入口

文档变更：
- `docs/design/soc_design.md` 增加合并范围、源码证据索引、上位机/协议控制入口和测试结论表达规则。
- `docs/README.md`、`docs/INDEX.md` 的 SOC 快速入口收敛为 `docs/design/soc_design.md`。
- `docs/review/soc_rest_fast_drop_analysis_2026-06-03.md`、`docs/review/soc_simplification_candidates_2026-06-02.md`、`docs/review/soc_test_script_usage_2026-06-03.md` 和 `docs/devlog/CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md` 标记为历史参考。

当前结论：
- 当前 SOC 逻辑、快降排查、简化记录和测试边界统一以 `docs/design/soc_design.md` 为活跃权威文档。
- 本次不修改源码，不改变 SOC 算法、协议字段、Flash 布局、低功耗路径和显示策略。

## 2026-06-04 CAN 周期 TX 不再阻塞 RTC idle

源码变更：
- `Can_HDX.c/.h`：新增 `Can_HDX_TransmitPeriodic()`，TX 队列项增加来源标记，区分普通周期广播和请求类发送。
- `CanFeidaoFrames.c`：1000ms/5000ms 飞道周期广播改走 `Can_HDX_TransmitPeriodic()`；CAN ID、payload、周期 mask 不变。
- `Can_IsBusy()` 的低功耗判定收窄为请求类 TX、CAN App 命令队列、read-block stream、未归属硬件发送和 RX 活动阻塞；普通周期广播 TX pending 不再反复置 `LP_BLOCK_COMM`、清零 `g_stLowPowerRtcStatus.idle`。
- `Can_PeekBusy()` 仍保留完整 busy 观察语义，debug/heartbeat 可以继续看到周期 TX pending。

当前结论：
- 无 ACK 或无 CAN 对端时，周期广播队列不会长期阻止 RTC idle 累计；真正进入 STOP 前仍由 `Can_PrepareSleep()` 取消当前 TX、清队列并关闭 `GPIO_CMNT_EN`。
- CAN App ACK、READ_BLOCK 分包、命令队列和 RX 活动仍按通信忙处理，避免请求响应被 STOP 截断。
- 本次不修改 CAN ID、payload、Modbus/CAN App 协议、RTC wake 地址、IAP/Bootloader 地址、保护条件和硬件初始化参数。

验证边界：
- `git diff --check`：通过。
- `cc -fsyntax-only`：`Can_HDX.c`、`CanFeidaoFrames.c` 在临时 Debug profile 和强制 Release profile 下均通过。
- `python3 tools/project_check.py --quiet`：当前基线为 `103 OK / 1 warning / 13 errors`，失败项为仓库既有缺文件、编码和历史审计门禁。
- 需上板确认 no-ACK/无对端时 `g_stLowPowerRtcStatus.idle` 可累计到 `idleMax` 并进入 HICCUP，接入 CAN 对端后周期广播恢复。

## 2026-06-04 Debug 调试实现与 Release 量产隔离

源码变更：
- 新增 `DebugHooks.h/.c`，把 `Runtime.c` 中的 SystemDebug 事件、profile、模块心跳和 debug print 周期输出迁入 Debug-only hook 层。
- `Runtime.c` 只保留 `DebugHooks_Runtime*()` 调用点，不再直接出现 `SystemDebug_Event()`、`SystemDebug_ProfileRecord()`、`SystemDebug_GetCycleCount()`、`DBG_PROFILE_*`、`DBG_MODULE_*` 和 `DbgPrint_Summary()`。
- Keil `FD_Debug` target 保留并编译 `DebugHooks.c`、`DebugWatch.c`、`SystemDebug.c`、`IrqDebug.c`；`FD_Release` target 对这些 Debug-only 实现文件设置 `IncludeInBuild=0`，不参与编译和链接。
- 新增 `StartupDefaultHandler.c`，在 Release 下为空实现启动默认异常回调，避免为了 `IrqDebug_RecordUnhandledVector()` 链接整个 `IrqDebug.c`。
- `FD_Debug` 输出目录改为 `Objects_Debug` / `Listings_Debug`，`FD_Release` 保持 `Objects` / `Listings`，避免两个 target 共用 `.o` 导致增量构建串目标。
- `tools\bms_dev_workflow.ps1` 改为读取 Keil target 的 `OutputDirectory/OutputName` 定位产物。
- `tools/project_check.py` 新增 target 文件隔离检查和 `Runtime.c` 调试实现泄漏检查。

当前结论：
- `g_dbg` 不需要单独保留为 Watch 根入口；统一通过 `g_dbg_watch.system.snapshot` 查看。
- `FD_Debug` 保留完整调试目录和快照能力；`FD_Release` 不编译、不链接调试实现文件，业务代码只留下空 hook。
- 详细设计见 `docs/review/debug_release_isolation_2026-06-04.md`。

验证：
- `powershell -ExecutionPolicy Bypass -File tools\bms_dev_workflow.ps1 -Mode build -Target FD_Debug`：已通过，`Objects_Debug\FD_Debug.axf/bin`，Keil `0 Error(s), 0 Warning(s)`。
- `powershell -ExecutionPolicy Bypass -File tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release`：已通过，`Objects\FD_Release.axf/bin`，Keil `0 Error(s), 0 Warning(s)`。
- `FD_Release.lnp` 无 `debughooks/debugwatch/systemdebug/irqdebug` 对象；`FD_Debug.lnp` 包含这些对象。
- `py -3.9 tools\run_soc_host_c_test.py`：6 组 SOC host C 测试均通过，每组 19 项。
- `py -3.9 tools\project_check.py --quiet`：本次新增检查通过；脚本仍因仓库历史缺文件、历史编码和既有 runtime/ADC 审计项失败。
- 真机 Keil Watch 仍需验证 `g_dbg_watch.runtime.app`、`g_dbg_watch.system.snapshot` 和 `g_dbg_watch.system.irq` 展开结果。

文档状态：已按源码部分验证
最后更新时间：2026-06-04
说明：长期详细变更记录见 `docs/changelog/change_log.md`；本文件按仓库协作规则保留为顶层入口。

## 2026-06-04 Keil Debug Watch 全项目调试目录

源码变更：
- 扩展 `g_dbg_watch` 为全项目调试目录，新增 `runtime`、`comm`、`system`、`afe`、`fault`、`public_data`、`app`、`calib`、`tables` 分组。
- 将 FactoryAging、RTC、Runtime、SCI、I2C_AFE1、SH367309、Fault、ProductionID、CanFeidaoFrames、SystemDebug、IrqDebug 等状态挂到 `g_dbg_watch`。
- `g_dbg` 保留为 `SystemDebug` 内部快照实体，但 Keil Watch 不再单独添加它；统一通过 `g_dbg_watch.system.snapshot` 观察。
- `FD_Debug` 默认打开 `PROJECT_CFG_DEBUG_WATCH_ENABLE=1`、`PROJECT_CFG_DEBUG_MONITOR_ENABLE=1`、`PROJECT_CFG_IRQ_DEBUG_ENABLE=1`，并保持 `PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE=0`。
- `Project_BuildGuard.h` 增加 SystemDebug/IRQ debug 默认值、0/1 范围检查、IRQ event 依赖检查，并阻止 Release profile 误开任何调试开关。
- `tools/project_check.py` 同步检查 Debug target 的 SystemDebug/IRQ 宏，并修复 Release map SOC 表检查中的 `defines` 变量来源。

当前结论：
- Keil Watch 只需要添加 `g_dbg_watch`；系统快照看 `g_dbg_watch.system.snapshot`，IRQ 计数看 `g_dbg_watch.system.irq`。
- `g_dbg_watch` 仍是 Debug-only 符号，Release 不参与链接，业务 runtime 继续保持模块内 `static` 封装。

验证：
- `powershell -ExecutionPolicy Bypass -File tools\bms_dev_workflow.ps1 -Mode build -Target FD_Debug`：Keil 日志 `0 Error(s), 6 Warning(s)`。
- `powershell -ExecutionPolicy Bypass -File tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil 日志 `0 Error(s), 5 Warning(s)`。
- `py -3.9 tools\run_soc_host_c_test.py`：6 组 SOC host C 测试全部通过，每组 19 项。
- `py -3.9 tools\project_check.py`：已跑完整，Debug/Release 宏检查通过；仍有仓库既有缺文件、非 UTF-8、历史文档缺失和 ADC runtime 检查项失败。

## 2026-06-04 Keil Debug Watch 单根结构体入口

源码变更：
- 新增 `DebugWatch.h`，统一 `PROJECT_CFG_DEBUG_WATCH_ENABLE` 判断和 `DEBUG_WATCH_USED` 符号保留属性。
- 新增 `DebugWatch.c` 和唯一 Debug Watch 根变量 `g_dbg_watch`，集中挂载 ADC、DataDeal、CAN、LedBar、Sleep、Flash、Log、System、SOC 和关键全局状态。
- 在各模块中保留 Debug-only `*_DebugWatchBind()` 绑定函数，不再导出单模块 `g_dbg_*` 全局符号。
- `Project_BuildGuard.h` 为 `PROJECT_CFG_BUILD_PROFILE` 和 `PROJECT_CFG_DEBUG_WATCH_ENABLE` 补默认值，并阻止 profile 0 误开 Debug Watch。

当前结论：
- `FD_Debug` 下只需要在 Keil Watch 添加 `g_dbg_watch`，再展开 `adc`、`data`、`can_tx`、`can_runtime`、`can_app`、`ledbar`、`sleep`、`soc` 等字段。
- 业务 runtime 仍保持文件级 `static`，不改变协议、Flash 布局、保护逻辑、SOC 算法和低功耗策略。
- `FD_Release` 不生成这些新增调试符号。

验证边界：
- 需要分别构建 `FD_Debug` 和 `FD_Release`，确认 Debug 符号可用且 Release 门禁不误触发。
- 真机调试时以 `docs/guides/DEBUG_WATCH_GUIDE.md` 的符号清单为准添加 Watch。

## 2026-06-04 RTC 唤醒后 RF_EN 熔断判定增加 ADC ready 保护

源码变更：
- `ADC.c/.h` 增加 `ADC_IsReady()`，停止 ADC、ADC 重新初始化或 RTC STOP 唤醒后先置为 not ready，完成首个直接计算周期后置 ready。
- `DataDeal.c` 的 RF_EN 熔断逻辑只在 `ADC_IsReady()!=0` 时允许 ADC Vbat 参与 `4280mV * SeriesNum` 总压条件。
- `DataDeal.c` 的 AFE 通信错误分支在 Vbat/温度条件不满足或 AFE 错误消失时清零 `rong_fuse_afe_err_cnt`，避免非连续异常累计触发 `GPIO_RF_EN`。

当前结论：
- ADC 未完成采样/计算时，`ADC_GetVbatMilliVolt()` 仍可能为 0，但不会参与 RF_EN 的 ADC 总压条件。
- RF_EN 仍可由 AFE 温度、AFE 单体最大电压等已有条件触发；本次只隔离 RTC 唤醒初期 ADC Vbat 未 ready 或异常首值的风险。

验证边界：
- `git diff --check`：本次相关文件无新增 whitespace error，仅有仓库行尾 LF/CRLF 提示。
- `rg "ADC_IsReady|adc_vbat_fuse_ovp|Vbat_mv >= 4280"`：RF_EN 路径中 ADC Vbat 熔断条件已通过 `adc_vbat_fuse_ovp` 统一门控。
- `py -3.9 tools/project_check.py --quiet`：仍因仓库既有 `elog_cfg.h` 缺失、部分历史文件非 UTF-8、历史清理文档缺失、`test_Autocurrent_cycle` 缺失，以及脚本自身 `defines` NameError 失败；未发现本次 ADC ready/RF_EN 改动导致的新失败。
- `powershell -ExecutionPolicy Bypass -File tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release`：已生成 `FD_Release.axf/bin`；Keil 日志显示 `0 Error(s), 0 Warning(s)`。
- 需要覆盖 RTC STOP 唤醒后 0ms/10ms/20ms/200ms 的 `ADC_IsReady()`、`Vbat_mv` 和 RF_EN 输出。
- 需要验证 AFE 错误持续但 Vbat/温度条件间断时，`rong_fuse_afe_err_cnt` 不会跨间断累计。

## 2026-06-04 RTC 唤醒后 ADC 直接采样简化

源码变更：
- `ADC.c` 删除 VBC、MOS 温度、Type-C 电流的软件滤波缓存、计数器和 catch-up 逻辑。
- `ADC.c` 将 VBC、MOS 温度、Type-C 电流改为使用最新 DMA raw 直接计算；Type-C 保留零点死区和最大值限幅。
- `ADC.c` 在 `ADC_ResetAnlogCalSchedule()` 中增加 1 个 10ms tick 的首样本丢弃，保留低功耗唤醒后 ADC stop/reinit 路径。
- `ADC.h` 删除不再使用的 ADC 平均滤波宏，保留 Type-C 死区和换算参数。

文档变更：
- 新增 `docs/review/adc_rtc_wakeup_simplification_2026-06-04.md`。
- 更新 ADC/AFE 设计文档、需求确认表、需求问题表、风险清单、重构计划、模块地图、全项目 review 和测试入口。

当前结论：
- RTC 唤醒后 ADC raw 仍由 TIM2/ADC/DMA 周期采样；最终值不再等待 8/32 点平均或 IIR 收敛。
- `App_AnlogCal()` 改为 latest-sample 模式，不再按历史 10ms tick 补跑滤波。
- AFE 单体累加总压、AFE 电流 sample seq、SOC 主积分和 Modbus/CAN 协议字段含义保持不变。

验证边界：
- 旧 ADC 滤波符号扫描无命中。
- `git diff --check` 通过，仅有仓库换行转换提示。
- `py -3.9 tools/project_check.py --quiet`：本次新增 BOM 问题已修正；脚本仍因既有缺文件/非 UTF-8 源码/缺历史文档和自身 `NameError` 失败。
- `powershell -ExecutionPolicy Bypass -File tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil 编译通过，`0 Error(s), 0 Warning(s)`。
- 产物：`103 + 309/Project/Users/Objects/FD_Release.axf` 1036908 bytes；`FD_Release.bin` 52868 bytes。
- 真板冷启动、RTC STOP、Type-C 插拔、STOP 功耗仍需验证。

## 2026-06-04 应用层宏配置第一批收敛

源码变更：
- 删除无有效消费者的 `PROJECT_CFG_SCI2_ROLE` / `PROJECT_CFG_SCI3_ROLE` 配置项。
- 将 LedBar 扫描周期和 `MCU_WK` 防抖参数下沉为 `LedBar.c` 内部常量，删除测试常亮旧分支。
- `UPGRADE_PARAM_FORCE_REAPPLY` 固定为 0，不再依赖不存在的 `PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY`。

文档变更：
- 新增并更新应用层宏审查文档，更新宏参考、模块参考、LedBar 设计和协议映射中的当前事实。

验证边界：
- 本次不修改保护阈值、Flash/IAP 地址、协议寄存器、AFE 寄存器和 SOC 算法。

## 2026-06-04 SOC 宏配置第一批清理

源码变更：
- 删除失效的 `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE` / `UPGRADE_PARAM_RESET_SOC_TABLE` 升级策略宏；SOC runtime table 已删除，升级时不再存在可复位的运行时 SOC 表。
- 删除未启用的 SOC 校准故障阻断预留宏；当前 SOC 校准允许条件保持为电压有效和压差有效。
- 删除默认无效的 empty-tail soft target/tick 调参宏，并删除对应空条件编译分支；low-tail 表值和 tail 行为不变。

文档变更：
- 更新宏参考和模块参考中的升级策略默认值，移除已删除 SOC 配置宏说明。

验证边界：
- 本次不修改 tail 表、SOC 算法、协议窗口、Keil target define 和 Debug Watch 配置。

## 2026-06-04 SOC 删除 runtime table、手动 OCV、mid-tail

本次按用户确认继续净删减 SOC 模块，目标是让算法边界更简单、稳定、易读。

源码变更：
- 删除 SOC runtime table 宏、运行时数组、EEPROM 默认表装载和上位机写表条件分支；SOC 算法只使用 `PROJECT_CFG_BAT_CHEMISTRY` 选择的编译期 OCV 表，写 SOC 表固定返回错误。
- 删除手动 OCV 请求 API 和 `u16_RefreshData_Flag=1` 处理；命令路径只保留容量 reset 和 `SetSocOnce`。
- 删除 mid-tail 表、`mid_ticks`、mid-tail debug watch 字段、SystemDebug `midT` 输出和 Python replay mid-tail 模型；low-tail 表值不变。
- 删除未使用的 `PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS`、`PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS` 配置和 build guard 检查。

文档变更：
- 更新 `docs/design/soc_design.md`、模块参考、宏参考、变量梳理、风险清单、需求确认、测试计划、debug 指南和 SOC 相关 review 文档。
- 明确 reset sleep 不做秒数 SOC 补偿；HICCUP RTC STOP 才推进长静置慢速下修。

验证：
- 已执行源码/文档扫描，确认源码和工具中无 runtime table/manual OCV/mid-tail 残留路径。
- Keil、真板、RTC STOP、CAN/Modbus 在线验证仍需后续执行。

## 2026-06-03 SOC 模块复审、净删减与文档合并

本次按“减少无用代码、降低阅读成本、不动两个 tail 表、体验更稳定”的边界复审 SOC 模块，保留当前 tail 测试状态。

源码变更：
- `SocEnhance.c`：保留 low-tail/full/rest 主流程，关闭 mid-tail 运行链路；两个 mid-tail 表仍保留，`u8MidTailActive` 固定为 0。
- `SocEnhance.c`：删除短静置 deferred OCV 自动路径，自动静置只保留长静置慢速下修。
- `SocEnhance.c`：删除过多 helper，使 `SOC_IntEnhance_Ctrl()` 直线表达命令、积分、low-tail/full、rest、保存、发布顺序。
- `SocEnhance.c/.h`、`SOC.c`：删除无用 `u16_SOC_CycleT_Limit`、`u8_SOC_OCV_Cali`、`SOC_WATCH_BLOCK_REASON/u8LastBlockReason`、`soc_watch_set_block_reason()` 和空测试 stub。
- `SocEnhance.c`：删除 RTC 秒级板载自耗扣减；正常运行 `RELAX/CHG/DSG` 仍按 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` 计入容量积分。
- `SystemDebug.c/.h`：删除固定 0 或误导性的 SOC debug 字段，保留真实内部计数和当时仍存在的 `display_ticks`。
- `tools/soc_host_c_test.c`、`tools/soc_replay_test.py`：同步当前活动 tail 表、mid-tail 关闭、长静置慢下修和自耗/RTC 口径，补齐 host stub。

文档变更：
- 重写 `docs/design/soc_design.md` 为 SOC 当前唯一权威入口。
- 将 `docs/review/soc_current_logic_2026-06-02.md` 标记为历史归档。
- 更新 `docs/review/soc_rest_fast_drop_analysis_2026-06-03.md`、`docs/review/soc_simplification_candidates_2026-06-02.md`、模块参考、风险清单、测试计划和索引。

验证：
- `python3 tools/soc_replay_test.py`：43 项通过。
- `python3 tools/run_soc_host_c_test.py`：`30mA/0mA/1000mA` 和 debug-watch 组合均通过。
- Keil `FD_Release`、真板充放电、RTC STOP、CAN/Modbus 在线读取仍需后续验证。

## 2026-06-03 DataDeal/AFE 运行状态结构体化第 3 阶段

本次继续按“模块状态结构体 + 访问函数”的方向收口 `DataDeal` 运行态，只处理 AFE 电流零点状态、AFE 通信监控计数、AFE 故障 sleep delay 计数和 AFE 电流采样序号，不修改 AFE 电流公式、保护参数、通信协议镜像或持久化参数布局。

源码变更：
- `DataDeal.c`：新增 `DATA_RUNTIME s_data`，内部按 `cur/mon/afeSeq` 分组管理 AFE 电流零点状态、AFE 监控计数和采样序号；删除 `s_afe_current`、`u8IICFaultcnt1/2`、`u8WakeCnt1/2`、`su16_Sleep_DelayT1/T2/T3`、`g_u32AfeCurrentSampleSeq`。
- `DataDeal.h`：删除 `g_u32AfeCurrentSampleSeq` extern，新增 `AfeCurrent_GetSeq()`。
- `I2C_AFE1.c`、`SOC.c`：改为通过 `AfeCurrent_GetSeq()` 判断 AFE 电流采样序号。
- `tools/project_check.py`：新增 DataDeal 运行状态结构体化门禁。

验证：
- `git diff --check`：通过，仅有仓库既有 CRLF 换行提示。
- 旧 DataDeal/AFE 散变量 `rg` 检查：无命中。
- `py -3.9 tools/project_check.py --quiet`：`109 OK / 1 Warning / 39 Errors`；新增 DataDeal 门禁通过，剩余失败为仓库既有缺文件、编码和配置门禁问题。
- `./tools/bms_dev_workflow.ps1 -Mode build -Target FD_Release`：生成 `FD_Release.axf/bin`，Keil 日志显示 `0 Error(s), 3 Warning(s)`。
- 真板 AFE1/AFE2 通信异常计数、AFE 自动恢复、通信异常延时 sleep、启动零点校准和 SOC 200ms 新采样触发仍需上板确认。

## 2026-06-03 ADC 状态结构体化第 2 阶段

本次按全局状态结构体化方案继续处理 ADC 模块，目标是移除 ADC 模块的公开散变量，改为 `ADC_RUNTIME s_adc` 模块私有状态，并通过 getter 给 DataDeal、SOC 和 SystemDebug 读取。

源码变更：
- `ADC.c/.h`：新增 `ADC_RUNTIME s_adc`，收口 DMA raw、滤波缓存、结果数组、Vbat、Type-C 电流、调度 tick 和平滑计数；删除 `g_u16ADCValFilter`、`g_i32ADCResult`、`g_u32Vbat_mV`、`g_u16TypeCOutCurrent_mA` 等 extern 入口。
- `ADC.h`：新增 `ADC_GetResult()`、`ADC_GetRaw()`。
- `DataDeal.c`、`SOC.c`、`SystemDebug.c`：改为通过 ADC getter 读取 ADC 状态。
- `tools/project_check.py`：新增 ADC 状态结构体化门禁。

保持不变：
- ADC DMA 通道、TIM2_CC2 触发、采样通道顺序、Type-C 电流公式、VBC 分压公式和 SOC Type-C 等效电流路径不变。
- 不修改 `g_stCellInfoReport` 协议镜像、CAN/Modbus 字段或 Flash 持久化布局。

验证：
- `git diff --check`：通过，仅有仓库既有 CRLF 换行提示。
- 旧 ADC 散变量 `rg` 检查：无命中。
- `py -3.9 tools/project_check.py --quiet`：`106 OK / 1 Warning / 39 Errors`；新增 ADC 门禁通过，剩余失败为仓库既有缺文件、编码和配置门禁问题。
- `./tools/bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil 日志显示 `0 Error(s), 3 Warning(s)`，已生成 `FD_Release.axf/bin`。
- ADC DMA raw、MOS 温度、VBC、Type-C 电流仍需上板确认。

## 2026-06-03 全局状态结构体化第 1 阶段

本次按 `docs/review/global_state_struct_audit_2026-06-03.md` 的第 1 阶段执行低风险收口，只处理 `SystemDebug`、`Runtime`、`Flash`、`SleepDeal`、`RTC` 的散状态变量，不修改协议镜像、Flash 持久化布局、CAN/Modbus 帧格式或上位机可见数据含义。

源码变更：
- `SystemDebug.c`：新增 `DBG_RUNTIME s_dbgRt`，收口事件环、head/count 和故障快照状态。
- `Runtime.c`：新增 `APP_RUNTIME s_rt`，收口调试打印 tick、上次故障和上次低功耗模式。
- `Flash.c`：新增 `FLASH_RUNTIME s_flash`，收口 Flash busy 标志。
- `SleepDeal.c/.h`：新增 `SLEEP_RUNTIME s_sleep`，通过 `SleepDeal_RecordExternalComm()` 和 `SleepDeal_GetExternalCommCounter()` 管理外部通信计数。
- `RTC.c/.h`：新增 `RTC_RUNTIME s_rtc`，通过 `RTC_IsStopWakeup()` 和 `RTC_ClearStopWakeup()` 管理 RTC STOP 唤醒标志。
- `Sci_Upper.c`、`rtc_sleep.c`、`rtc_sleep_port.c`、`conf.c`：调用点切换到模块访问函数。
- `tools/project_check.py`：新增第 1 阶段门禁，防止旧散变量回流。

验证：
- `git diff --check`：通过，仅有仓库既有 CRLF 换行提示。
- `rg "RTC_ExtComCnt|is_rtc_wakekup|s_dbg_events|s_dbg_print_tick|s_u8StorageFlashBusy|TimeDisplay" "103 + 309/Project/Source"`：无命中。
- `py -3.9 tools/project_check.py --quiet`：`103 OK / 1 Warning / 39 Errors`；新增 `check_global_state_phase1()` 通过，剩余失败为仓库既有缺文件、编码和配置门禁问题。
- `./tools/bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil 日志显示 `0 Error(s), 3 Warning(s)`，已生成 `FD_Release.axf/bin`。
- RTC Alarm、USART 外部通信计数和 reset sleep 唤醒路径仍需上板验证。

## 2026-06-03 低功耗状态与阻塞原因收口

本次按用户确认方案简化 `rtc_sleep` 低功耗选择逻辑，不修改低压阈值、RTC STOP 执行器、reset sleep 提交路径、CAN/Modbus 协议、IAP/App 地址或量产/测试配置。

源码变更：
- `rtc_sleep.h/c`：删除 `LOW_POWER_RTC_BLOCK_*` 粗粒度阻塞枚举，统一使用 `LP_BLOCK_*` 位图；新增 `LP_BLOCK_EXT_COMM` 和 `LP_BLOCK_AGING`。
- `rtc_sleep.h/c`：删除 `s_u16IdleDelaySeconds`、`s_u32RtcSleepElapsedSeconds`、`s_u32RtcWakeCycles`、`s_u32LastSleepSeconds` 等独立低功耗状态变量，统一收口到 `g_stLowPowerRtcStatus` 的 `idle/block/sleep/last/cycles/vlow/force/comm` 字段。
- `rtc_sleep.c`：将 `low_power_select_sleep_mode()` 简化为 `lp_deep()`、`LP_GetBlockReason()`、`lp_idle()` 三段；`LP_GetBlockReason()` 成为唯一阻塞判断入口。
- `Runtime.c`、`SystemDebug.c/h`、`tools/stlink_bms_monitor.ps1`：同步读取新的 `g_stLowPowerRtcStatus` 布局和 `block` 位图，调试快照不再调用 `LP_GetBlockReason()`，避免提前消费外部通信变化。
- `tools/project_check.py`：新增门禁，防止旧粗粒度阻塞枚举和独立低功耗计数变量回流。

文档变更：
- 新增 `docs/review/low_power_state_bitmask_alignment_2026-06-03.md`，记录 `g_stLowPowerRtcStatus` 新字段、`LP_BLOCK_*` 唯一阻塞位图、低功耗选择流程和验证项。
- 更新 `docs/test_plan.md`、`docs/review/test_plan.md` 和 `docs/changelog/change_log.md`。

验证：
- `git diff --check`：通过，仅有仓库既有 CRLF 换行提示。
- `py -3.9 tools/project_check.py --quiet`：`101 OK / 0 Warnings / 39 Errors`；剩余失败为仓库既有缺失文件、编码和配置门禁问题，本次新增低功耗状态收口门禁通过。
- `./tools/bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil 日志显示 `0 Error(s), 3 Warning(s)`，已生成 `FD_Release.axf/bin`。
- 真板 RTC STOP、低压 deep、外部通信阻塞、老化阻塞、ST-Link 监控脚本仍需上板验证。

## 2026-06-03 中断计数实现

本次按已输出方案实现主固件中断计数，不修改 Modbus/CAN 协议、不新增上位机可见寄存器、不烧录。

源码变更：

- 新增 `IrqDebug.h/c`，提供 `g_stIrqDebug` 全局计数、分生命周期阶段计数、最近事件环和未实现向量兜底记录。
- 为异常、EXTI、USART、RTC、TIM3、TIM4、CAN RX0 增加中断计数插点；高频中断只做轻量计数。
- 在启动、运行、休眠准备、STOP 等待、STOP 唤醒原始窗口、STOP 恢复和 reset sleep 等待路径标记调试阶段。
- `SystemDebug` 的 `g_dbg.irq` 增加代表性中断计数摘要；完整数据仍直接观察 `g_stIrqDebug`。
- `startup_stm32f10x_hd.s` 默认 weak handler 记录 `VECTACTIVE` 后继续停住。
- Keil `FD_Release`/`FD_Debug` 两个 Target 加入 `IrqDebug.c`。

文档变更：

- 更新 `docs/review/interrupt_counter_plan_2026-06-03.md`、`docs/test_plan.md`、`docs/review/test_plan.md`，记录实施内容和验证项。

验证：

- `git diff --check`：通过；仅有 CRLF 换行提示。
- `tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil `FD_Release` 生成 `FD_Release.axf/bin`，日志显示 `0 Error(s), 3 Warning(s)`。
- `py -3.9 tools\project_check.py --quiet`：`100 OK / 0 Warnings / 39 Errors`；剩余失败为仓库既有缺失文件、编码和配置门禁类问题，不是本轮中断计数新增。
- 未执行上板 RTC STOP、按键/充电/CAN/USART 唤醒实测。

## 2026-06-03 SOC 主流程职责拆分

本次只做 `SOC_IntEnhance_Ctrl()` 可读性优化和保存判重状态收窄，不修改 SOC 功能、SOC 表、阈值、时间参数、tail 策略、RTC 补偿策略、显示平滑、协议字段和 Keil 工程。

源码修改：

- `SocEnhance.c`：新增 `soc_run_cycle_calibration()` 和 `soc_update_rest_after_cycle()`，把正常 200ms 周期中的 tail/full/deferred 校准阶段和 rest 后处理阶段从 `SOC_IntEnhance_Ctrl()` 主函数拆出。
- `SocEnhance.c`：将保存判重从完整 `SOC_STATE` 镜像收窄为 `SOC_SAVE_MARK`，只保留实际参与判重的 `soc/cycle_x100/cap_full_as10/snapshot_flags`。

文档修改：

- 更新 `docs/review/soc_simplification_candidates_2026-06-02.md`，追加 `SOC-SIM-09/10` 执行项和保持不变边界。

验证：

- `git diff --check`：通过。
- `clang -fsyntax-only SocEnhance.c`：通过。
- `python3 tools/soc_replay_test.py`：47 项通过。
- `python3 tools/project_check.py --quiet`：保持既有 `88 OK / 1 warning / 40 errors` 基线。
- 未执行 Keil `FD_Release` 编译、真板充放电、RTC STOP、CAN/Modbus 在线读取和 Keil watch 实测。

## 2026-06-03 SOC 无放电静置快降分析

本次只做文档分析和索引更新，不修改 `.c/.h`、Keil 工程、配置宏、SOC 表、校准阈值、时间参数、显示策略、RTC STOP 顺序和协议字段。

更新内容：

- 新增 `docs/review/soc_rest_fast_drop_analysis_2026-06-03.md`，按当前源码说明“无放电静置 SOC 快降”的可能来源、证据、排查顺序和后续优化边界。
- 更新 `docs/design/soc_design.md`、`docs/README.md`、`docs/INDEX.md`，加入该分析文档入口。

核心结论：

- 普通静置 OCV 在当前配置下不是秒级/分钟级快降主因。
- 无放电快降当前更可能来自 `RELAX` 模式下的 `EMPTY_TAIL`、低压显示快速追赶，或 RTC 休眠期间已提前推进长静置 OCV；`MID_TAIL` 运行链路已关闭。
- 是否调整 RELAX tail 属于功能体验变更，需先用 `u8LastCalibSource` 上板确认。

## 2026-06-03 SOC 源码简化候选执行

本次按 `docs/review/soc_simplification_candidates_2026-06-02.md` 执行 `SOC-SIM-01/02/03/04/05/06/07/08`，每个源码批次单独提交，不修改 SOC 功能、协议、时间参数、校准阈值、SOC 表、休眠顺序和用户可见显示策略。

源码修改：

- `SOC.c`：删除 `InitData_SOC()` 初始化后的重复 `SOC_PublishReportData()`。
- `SocEnhance.c/.h`：补尾端表注释；统一内部静置计数 `*_soc_ticks` 口径；增加 `SOC_RequestManualOcvRefresh()`、`SOC_RequestCapacityReset()`、`SOC_RequestSetOnce()`；整理 `SOC_IntEnhance_Ctrl()` 局部命名；拆分 `soc_publish()` 内部职责；明确 RTC 补偿游标含义。
- `Sci_Upper.c`：容量重算和一次性设置 SOC 改为调用 `SOC_Request*` 接口，不再直接写命令 shadow。
- `tools/project_check.py`：同步门禁检查，使其接受新的 `SOC_RequestCapacityReset()` 入口。

文档修改：

- 更新 `docs/review/soc_simplification_candidates_2026-06-02.md`，记录已执行批次、提交号、未做项和验证结果。

验证：

- 每批 `git diff --check`：通过。
- 每批 `clang -fsyntax-only`：通过。
- 每批 `python3 tools/soc_replay_test.py`：47 项通过。
- 每批 `python3 tools/project_check.py`：保持既有 `88 OK / 1 warning / 40 errors` 基线，失败项不是本轮 SOC 简化新增。
- 未执行 Keil 编译、真板、RTC STOP、CAN/Modbus 在线读取和 Keil watch 实测。

## 2026-06-02 SOC 文档合并与源码简化候选

本次只整理 SOC 相关文档，不修改 `.c/.h`、Keil 工程、编译宏、协议、SOC 表、校准阈值、休眠顺序和显示策略。

更新内容：

- 新增 `docs/review/soc_current_logic_2026-06-02.md`，按当前源码梳理 SOC 主链路、输入输出、11 类校准策略、具体条件、时间参数、RTC 休眠补偿和显示平滑口径。
- 重写 `docs/design/soc_design.md` 为 SOC 长期设计入口，指向当前逻辑详表和源码简化候选，避免旧 devlog 与当前源码事实混用。
- 新增 `docs/review/soc_simplification_candidates_2026-06-02.md`，列出只改写法、不改功能的低风险/中风险候选和验证边界。
- 更新 `docs/README.md`、`docs/INDEX.md`、`docs/review/document_merge_plan.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/test_plan.md`，同步 SOC 文档入口、风险和测试项。

后续源码简化前必须以 `docs/review/soc_simplification_candidates_2026-06-02.md` 为批次边界，不允许顺手改变 SOC 功能。

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

## 2026-06-02 SV-CLEAN-03 readyToSleep 低功耗提交收口

本批次处理 `g_stLowPowerRtcStatus.readyToSleep` 全局阶段变量，不修改 HICCUP STOP 执行器、reset sleep 提交点、BMS_SLEEP 日志格式、sleep SOC 保存、CAN/AFE/ADC/TIM 恢复顺序、协议寄存器和 IAP/App 地址。

源码修改：

- `rtc_sleep.h`：删除 `LOW_POWER_RTC_STATUS.readyToSleep` 字段，删除 `LowPower_IsToSleepPending()` 和 `LowPower_ClearToSleepFlag()` 声明。
- `rtc_sleep.c`：删除 ready 置位/二次判断/清除流程；`rtc_sleep()` 改为读取本地 `sleep_mode = g_stLowPowerRtcStatus.mode` 后直接提交 HICCUP/NORMAL/DEEP。
- `LedBar.c`：删除 `APP_LedBar()` 中对 `LowPower_IsToSleepPending()` 的分支；reset sleep 的 SOC 保存仍由 `SleepDeal_Continue()` -> `LowPowerSleep_SaveResetState()` 完成，HICCUP STOP 前 GPIO 仍由 `LedBar_PrepareForStop()` 完成。
- `LogRecord.c`：删除 `BMS_SLEEP` 日志保存后清低功耗 ready 的跨模块副作用。
- `Runtime.c` / `SystemDebug.c`：低功耗 busy/`g_dbg.lp.ready` 改为由 `mode != NO_SLEEP` 派生。
- `tools/stlink_bms_monitor.ps1`：按新 `LOW_POWER_RTC_STATUS` 布局解析 `mode/blockReason/rtcWake`，`RtcReady` 改为由 `mode != NO_SLEEP` 派生。
- `tools/project_check.py`：门禁改为检查旧 ready API 消失、`rtc_sleep()` 使用本地 `sleep_mode` 提交。

文档修改：

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/review/low_power_requirement_alignment_2026-06-02.md`、`docs/review/full_project_review.md`、`docs/review/module_map.md`、`docs/reference/module_reference.md`、`docs/reference/global_variables.md` 和 `docs/test_plan.md`，将 `SV-CLEAN-03 / REQ-SV-002 / Q-SV-002` 标记为已执行。

验证：

- `rg -n "readyToSleep|LowPower_IsToSleepPending|LowPower_ClearToSleepFlag" "103 + 309/Project/Source" tools`：源码旧 ready 字段/API 无残留；`todo.md` 用户记录和 project_check 规则说明除外。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `rtc_sleep.c`、`LedBar.c`、`LogRecord.c`、`Runtime.c`、`SystemDebug.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和旧文档引用，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板 HICCUP/NORMAL/DEEP、STOP 电流、BMS_SLEEP 日志读取、sleep SOC 读取或 ST-Link 监控实测。

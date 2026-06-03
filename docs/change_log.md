# 变更记录

文档状态：已按源码部分验证
最后更新时间：2026-06-03
说明：长期详细变更记录见 `docs/changelog/change_log.md`；本文件按仓库协作规则保留为顶层入口。

## 2026-06-03 SOC 模块复审、净删减与文档合并

本次按“减少无用代码、降低阅读成本、不动两个 tail 表”的边界复审 SOC 模块，保留当前 tail 测试状态。

源码变更：
- `SocEnhance.c`：恢复 low/mid tail 主流程和 `soc_apply_mid_tail()`，修复局部变量未初始化风险；删除过多 helper，使 `SOC_IntEnhance_Ctrl()` 直线表达命令、积分、tail/full/deferred、rest、保存、发布顺序。
- `SocEnhance.c/.h`、`SOC.c`：删除无用 `u16_SOC_CycleT_Limit`、`u8_SOC_OCV_Cali`、`SOC_WATCH_BLOCK_REASON/u8LastBlockReason`、`soc_watch_set_block_reason()` 和空测试 stub。
- `SocEnhance.c`：删除 RTC 秒级板载自耗扣减；正常运行 `RELAX/CHG/DSG` 仍按 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` 计入容量积分。
- `SystemDebug.c/.h`：删除固定 0 或误导性的 SOC debug 字段，保留真实内部计数和 `display_ticks`。
- `tools/soc_host_c_test.c`、`tools/soc_replay_test.py`：同步当前活动 tail 表和自耗/RTC 口径，补齐 host stub。

文档变更：
- 重写 `docs/design/soc_design.md` 为 SOC 当前唯一权威入口。
- 将 `docs/review/soc_current_logic_2026-06-02.md` 标记为历史归档。
- 更新 `docs/review/soc_rest_fast_drop_analysis_2026-06-03.md`、`docs/review/soc_simplification_candidates_2026-06-02.md`、模块参考、风险清单、测试计划和索引。

验证：
- `python3 tools/soc_replay_test.py`：47 项通过。
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
- 无放电快降更可能来自 `RELAX` 模式下的 `EMPTY_TAIL` / `MID_TAIL`、低压显示快速追赶，或 RTC 休眠期间已提前补偿。
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

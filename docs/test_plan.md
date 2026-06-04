# 测试计划

文档状态：部分验证
最后更新时间：2026-06-04
说明：完整 review 后测试计划见 `docs/review/test_plan.md`；本文件按仓库协作规则保留为顶层入口。

## DataDeal/AFE 运行状态结构体化第 3 阶段测试入口

专项变更：AFE 电流零点运行态、AFE 通信监控计数、通信异常 sleep delay 和 AFE 电流采样序号已收口到 `s_data`；外部模块通过 `AfeCurrent_GetSeq()` 读取采样序号。

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 编译 | Keil `FD_Release` | 编译通过，无新增未解析符号 |
| 静态门禁 | `py -3.9 tools/project_check.py --quiet` | DataDeal 运行状态结构体化门禁通过 |
| 旧符号检查 | `rg "g_u32AfeCurrentSampleSeq|u8IICFaultcnt|u8WakeCnt|su16_Sleep_DelayT|s_afe_current" "103 + 309/Project/Source"` | 源码无旧 DataDeal/AFE 散变量 |
| 启动零点校准 | 首次启动、sleep 唤醒后 AFE 初始化 | `AfeCurrent_GetSeq() == 0U` 时仍执行启动零点准备和校准 |
| SOC 新样本触发 | 200ms AFE 采样周期 | `App_SOC()` 只在 AFE seq 变化后更新样本 |
| AFE 通信异常恢复 | 模拟 AFE1/AFE2 IIC 异常 | fault/wake 计数、恢复触发和错误上报/清除行为不变 |
| 通信异常 sleep | AFE/EEPROM 通信错误持续超过延时 | 仍按原阈值请求 `NORMAL_MODE` sleep |

## ADC 状态结构体化第 2 阶段测试入口

专项变更：ADC DMA raw、滤波缓存、结果数组、Vbat、Type-C 电流和内部平滑计数已收口到 `s_adc`；外部模块通过 ADC getter 读取。

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 编译 | Keil `FD_Release` | 编译通过，无新增未解析符号 |
| 静态门禁 | `py -3.9 tools/project_check.py --quiet` | ADC 结构体化门禁通过 |
| 旧符号检查 | `rg "g_u16ADCValFilter|g_i32ADCResult|g_u32Vbat_mV|g_u16TypeCOutCurrent_mA|g_u32ADCValFilter2|s_u32AnlogCalLast10msTick" "103 + 309/Project/Source"` | 源码无旧 ADC 散变量 |
| DMA raw | Keil Watch `s_adc.raw[]` 或 `g_dbg.adc.raw_*` | TIM2 触发后 raw 值正常更新 |
| VBC 总压 | `ADC_GetVbatMilliVolt()`、`g_dbg.adc.vbat_mv` | 与原 ADC 分压换算结果一致 |
| Type-C 电流 | `ADC_GetTypeCOutCurrentMilliAmp()`、SOC Type-C 等效电流 | 零点、滤波、换算、SOC 输入路径不变 |
| 温度结果 | `ADC_GetResult(ADC_TEMP_MOS1)`、`g_dbg.adc.mos_temp` | MOS 温度显示与原逻辑一致 |

## 全局状态结构体化第 1 阶段测试入口

专项变更：`SystemDebug`、`Runtime`、`Flash`、`SleepDeal`、`RTC` 的低风险散变量已收口到模块 runtime 结构体；外部通信计数和 RTC STOP 唤醒标志改为通过模块函数访问。

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 编译 | Keil `FD_Release` | 编译通过，无新增未解析符号 |
| 静态门禁 | `py -3.9 tools/project_check.py --quiet` | `check_global_state_phase1()` 通过 |
| 旧符号检查 | `rg "RTC_ExtComCnt|is_rtc_wakekup|s_dbg_events|s_dbg_print_tick|s_u8StorageFlashBusy|TimeDisplay" "103 + 309/Project/Source"` | 源码无本阶段已收口的旧散变量名 |
| USART 外部通信 | 串口收包触发 `Sci_PortIRQHandler()` | `SleepDeal_RecordExternalComm()` 递增，低功耗 `LP_BLOCK_EXT_COMM` 可被置位 |
| RTC STOP 唤醒 | RTC Alarm 唤醒 | `RTC_IsStopWakeup()` 返回 1，恢复后按原流程清除 |
| reset sleep 唤醒 | HICCUP/NORMAL/DEEP reset sleep 后按键或充电唤醒 | `SleepDeal_IsBootFromSleepStartup()`、`SleepDeal_IsBootFromSleepChargerWakeup()` 行为不变 |
| Keil 观察 | Watch `s_dbgRt`、`s_rt`、`s_flash`、`s_sleep`、`s_rtc`、`g_dbg` | 模块状态可观察，`g_dbg` 仍作为统一调试快照入口 |

## 低功耗状态收口专项测试入口

专项变更：`g_stLowPowerRtcStatus` 统一保存低功耗模式、阻塞位图、空闲计时、低压计时、RTC STOP 累计秒数和唤醒次数；`LP_GetBlockReason()` 是唯一阻塞判断入口。

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 编译 | Keil `FD_Release` | 编译通过，无新增未解析符号 |
| 静态门禁 | `py -3.9 tools/project_check.py --quiet` | 新增低功耗状态收口门禁通过 |
| 文本检查 | `rg "LOW_POWER_RTC_BLOCK|s_u16IdleDelaySeconds|s_u32RtcSleepElapsedSeconds|s_u32RtcWakeCycles|s_u32LastSleepSeconds" "103 + 309/Project/Source/rtc_sleep.*"` | 源码无旧粗粒度阻塞枚举和独立计数变量 |
| 空闲 HICCUP | 无充放电、无通信、无 LED/Flash/fault/老化 | `g_stLowPowerRtcStatus.idle` 累计到 `idleMax` 后进入 `HICCUP_MODE` |
| 低压 deep | 单体低于强制/参数低压阈值且充电电流小于限制 | 低压计时优先于普通 block，达到阈值后请求 `DEEP_MODE` |
| 阻塞位图 | 分别制造电流、通信、按键、Flash、fault、LED、老化 | `g_stLowPowerRtcStatus.block` 对应 `LP_BLOCK_*` 位被置位，`idle` 清零 |
| 外部通信边沿 | 改变 `RTC_ExtComCnt` | 只通过 `LP_GetBlockReason()` 消费通信变化，`SystemDebug` 快照不提前清边沿 |
| 工厂老化 | 老化 running | 只阻塞 HICCUP RTC STOP；低压 deep 和外部 `DEEP_MODE/NORMAL_MODE` reset sleep 仍可执行 |
| ST-Link 监控 | `tools/stlink_bms_monitor.ps1 -Count 1` | 能按新 8 word 布局解析 `RtcMode/RtcBlock/RtcElapsedSeconds/RtcCycles` |

## 中断计数专项测试入口

专项方案文档：`docs/review/interrupt_counter_plan_2026-06-03.md`

当前阶段已实现主固件中断计数，至少覆盖：

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 编译 | Keil `FD_Release` | 编译通过，无新增未解析符号 |
| ISR 覆盖 | 对照 `docs/review/interrupt_counter_plan_2026-06-03.md` 中断清单 | 已实现/已启用 ISR 都有轻量计数插点 |
| RTC STOP | Keil Watch 观察 `g_stIrqDebug.phase` | `STOP_WAIT` 阶段只出现 RTC alarm 或合法 EXTI 唤醒 |
| 运行态高频中断 | TIM3/TIM4 计数和主循环周期 | 计数递增正常，灯板扫描和 10ms 节拍无明显异常 |
| 通信干扰 | USART/CAN 在休眠前后分别注入 | 可区分 RUN、SLEEP_PREPARE、STOP_WAIT、STOP_RESTORE 阶段进入情况 |
| 未实现向量兜底 | 启动汇编默认处理器路径 | 误入未实现中断时可看到 `last_vectactive`，并保持原停住行为 |

## 状态变量净删减专项测试入口

专项审计文档：`docs/review/state_variable_audit.md`

## SOC 文档合并与源码简化测试入口

专项文档：

- `docs/design/soc_design.md`
- `docs/review/soc_rest_fast_drop_analysis_2026-06-03.md`
- `docs/review/soc_simplification_candidates_2026-06-02.md`

后续 SOC 源码简化必须先确认“不改功能”边界，并至少覆盖：

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 当前逻辑文档一致性 | `docs/design/soc_design.md` | 校准策略、时间参数、正常自耗、RTC 不扣自耗、mid-tail 已删除、low-tail 当前表和显示边界均有源码证据 |
| 源码简化记录 | `docs/review/soc_simplification_candidates_2026-06-02.md` | 只记录净删减和当前执行结果，不再保留已撤销 helper 方案 |
| SOC 回放 | `python3 tools/soc_replay_test.py` | Python 表解析与 C 活动表一致，覆盖 low-tail、长静置慢下修和已删除路径边界 |
| SOC host C | `python3 tools/run_soc_host_c_test.py` | `30mA/0mA/1000mA` 和 debug-watch 组合通过 |
| SOC visual trace | `python3 tools/soc_visual_report.py --html build/host_tests/soc_visual_report_check.html --csv build/host_tests/soc_visual_trace_check.csv` | 5 个场景通过 |
| 发布口径 | `docs/review/test_plan.md#6-soc-测试` | `g_stCellInfoReport.SocElement.u16Soc` 保持 display SOC |
| RTC 休眠补偿 | `docs/review/test_plan.md#6-soc-测试` | HICCUP STOP 周期只推进长静置 OCV 慢速下修，不额外扣 RTC 自耗，不锁存短静置 deferred target |

SOC 源码净删减后，必须至少覆盖：

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 文档一致性 | `rg "Q-SV-|REQ-SV-|SV-CLEAN" docs/review` | 审计、确认、风险、计划、测试文档都有入口 |
| 产品信息初始化收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | `0xC002` 48 个寄存器读取不变 |
| FactoryAging 结构体收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | 旧 `s_u*FactoryAging*` 符号无残留；老化状态、剩余时间、BKP/Flash 保存和 `0x14F80208` 行为不变 |
| LogRecord 结构体收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | 旧日志私有状态符号无残留；startup/sleep/fault 日志和事件读取不变 |
| AFE current zero 结构体收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | 旧 AFE current zero 私有状态符号无残留；零点、充放电方向和 SOC sample seq 行为不变 |
| LedBar 初始化收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | 启动阶段显式初始化；`APP_LedBar()` 不再懒初始化；上电显示、按键显示、TIM4 扫描、STOP 前 GPIO 和 RTC 唤醒恢复行为不变 |
| `readyToSleep` 收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | 源码无全局 ready 字段/API；HICCUP/NORMAL/DEEP、sleep SOC、`BMS_SLEEP` 日志和 ST-Link 派生 `RtcReady` 行为不变 |
| 基础静态检查 | `git diff --check`、仓库脚本、可用编译 | 结果与基线对比清楚，不把旧失败当成本次失败 |

硬件实测项仍以 `docs/review/test_plan.md` 为准。

## RTC 唤醒后 ADC 采样简化测试入口

专项文档：`docs/review/adc_rtc_wakeup_simplification_2026-06-04.md`

当前阶段：源码已修改，已删除 ADC 软件滤波并改为直接采样计算；硬件实测待执行。

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 旧滤波符号检查 | `rg "ADC_Current_Smooth|ADC_TTC|ADC_Vbc|AD_CalNum|AD_CalNum_Cur|TYPEC_CUR_ZERO_CONFIRM_CNT|filt\\[" "103 + 309/Project/Source"` | 源码中无旧 ADC 软件滤波函数、宏和缓存引用 |
| 静态源码证据 | `rg "ADC_UpdateVbc|ADC_UpdateMosTemp|ADC_UpdateTypeCCurrent|ADC_STARTUP_DISCARD_TICKS|App_AnlogCal" "103 + 309/Project/Source/ADC.c"` | 能定位直接计算、首样本丢弃和 latest-sample 调用链 |
| 冷启动 ADC raw | Keil Watch 或 debug 快照读取 `s_adc.raw[]` / `g_dbg.adc.raw_*` | TIM2 触发后 raw 快速更新，不长时间为 0 |
| 冷启动 ADC 最终值 | `ADC_GetVbatMilliVolt()`、`ADC_GetTypeCOutCurrentMilliAmp()`、`ADC_GetResult(ADC_TEMP_MOS1)` | 丢弃 1 个 tick 后直接进入合理范围，不再长时间从 0 慢收敛 |
| RTC STOP 唤醒 ADC | 进入 HICCUP RTC STOP 后由 RTC 唤醒 | 唤醒后 ADC raw 恢复；约 20ms 起出现直接计算结果，200ms 内稳定 |
| Type-C 插拔 | 接入/断开 Type-C 负载 | 接入不再等待 32 点平均；断开后死区内清零 |
| 主总压隔离 | 比较 `g_stCellInfoReport.u16VCellTotle` 和 ADC VBC 辅助值 | 主总压仍来自 AFE 单体累加，ADC VBC 不覆盖主路径 |
| SOC 路径 | 观察 AFE sample seq、主回路电流、Type-C 等效电流 | AFE sample seq 仍驱动主 SOC；Type-C 只作为附加等效放电 |
| 低功耗电流 | STOP 前后测量板端电流 | ADC 简化后仍关闭 TIM2/ADC/DMA，不抬高 STOP 功耗 |

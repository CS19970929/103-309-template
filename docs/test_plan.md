# 测试计划

文档状态：部分验证
最后更新时间：2026-06-03
说明：完整 review 后测试计划见 `docs/review/test_plan.md`；本文件按仓库协作规则保留为顶层入口。

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
- `docs/review/soc_current_logic_2026-06-02.md`
- `docs/review/soc_simplification_candidates_2026-06-02.md`

后续 SOC 源码简化必须先确认“不改功能”边界，并至少覆盖：

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 当前逻辑文档一致性 | `docs/review/soc_current_logic_2026-06-02.md` | 校准策略、时间参数、RTC/显示边界均有源码证据 |
| 源码简化候选 | `docs/review/soc_simplification_candidates_2026-06-02.md` | 候选只涉及写法/状态所有权/重复发布，不改阈值和协议 |
| SOC 回放 | `python3 tools/soc_replay_test.py` | 结果与基线一致 |
| 发布口径 | `docs/review/test_plan.md#6-soc-测试` | `g_stCellInfoReport.SocElement.u16Soc` 保持 display SOC |
| RTC 休眠补偿 | `docs/review/test_plan.md#6-soc-测试` | HICCUP STOP 周期先补偿，最终按键显示不出现校准跳变 |

当前阶段未修改源码，因此只执行文档和静态一致性检查。进入源码净删减后，必须至少覆盖：

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

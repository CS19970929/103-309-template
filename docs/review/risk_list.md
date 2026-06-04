# 103-309 BMS 风险清单

状态：部分验证

本文以当前源码为第一可信来源，记录本轮 BMS App IO 与 RTC 低功耗配置审查发现的风险。部分专项已按用户确认进入源码修改；仍未做上板实测。2026-06-02 追加低功耗需求对齐风险，详见 `docs/review/low_power_requirement_alignment_2026-06-02.md`。
2026-06-03 追加 SOC 源码复审、文档合并与源码简化风险，详见 `docs/design/soc_design.md`、`docs/review/soc_rest_fast_drop_analysis_2026-06-03.md` 和 `docs/review/soc_simplification_candidates_2026-06-02.md`。
2026-06-03 追加中断计数实现风险，详见 `docs/review/interrupt_counter_plan_2026-06-03.md`。

## 参考源码

- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/System_Init.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/IrqDebug.c`
- `103 + 309/Project/Source/IrqDebug.h`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/DataDeal.h`

## SOC 文档合并与源码简化风险

状态：已按源码和 host 测试部分验证，2026-06-03 更新

| 风险 ID | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 |
|---|---|---|---|---|---|
| RISK-SOC-DOC-001 | 旧 SOC 文档和当前源码事实混用 | `docs/design/soc_design.md`, `docs/review/soc_current_logic_2026-06-02.md`, `SocEnhance.c` | 后续优化可能基于旧结论误改自耗、RTC 或 tail | 已处理 | `docs/design/soc_design.md` 为唯一权威入口；旧逻辑文档已归档 |
| RISK-SOC-UX-001 | 把内部 `s_soc.soc` 和对外 `display_soc` 混为一谈 | `soc_publish()`, `SOC_PublishReportData()` | 自动校准和用户显示节奏被误改 | MUST_KEEP | 后续源码简化不得改变 `display_soc` 发布口径；调试看 internal/display 双值 |
| RISK-SOC-RTC-001 | reset sleep 与 HICCUP RTC STOP 的 SOC 补偿路径不同 | `rtc_sleep_port.c`, `LowPowerSleep.c`, `LedBar.c` | 若误认为两条路径都有 RTC 秒数补偿，可能错误判断休眠后 SOC 准确性 | 已确认不补 reset sleep | reset sleep 只验证睡前 snapshot/快显 SOC；HICCUP RTC STOP 才推进长静置补偿 |
| RISK-SOC-SELF-001 | 混淆正常自耗和 RTC 自耗 | `soc_integrate_current_ma()`, `soc_apply_rtc_rest_ocv()` | 可能重复扣自耗或误删正常自耗 | MUST_KEEP | 保持正常运行自耗积分；RTC STOP 不额外扣自耗；host C 自耗矩阵持续覆盖 |
| RISK-SOC-TAIL-001 | 当前 low-tail 测试表速度较快 | `s_empty_tail_table`, `DELAY_SOC_TEST` | RELAX/低压场景可能出现快降体验；mid-tail 已删除 | KEEP_BUT_REFACTOR | 本轮不改 low-tail 表值；上板用 low-tail active、last calib source 和 internal/display SOC 确认体验 |
| RISK-SOC-SIM-001 | “只改写法”时改变 SOC 状态机顺序 | `SOC_IntEnhance_Ctrl()` | 满电、低压 low-tail、长静置慢下修、显示计时优先级变化 | MUST_KEEP | 保持当前直线顺序，用 SOC replay 和 host C 守住；mid-tail/deferred OCV 已按确认关闭 |
| RISK-SOC-CMD-001 | 收口命令 shadow 时影响上位机写 SOC/容量行为 | `SOC_Request*()`, `Sci_Upper.c` | `SetSocOnce`、容量重算 ACK 或显示行为改变；手动 OCV 已删除 | KEEP_BUT_REFACTOR | 请求 API 保留 capacity reset/set once；后续若私有化字段必须单独验证 Modbus 写命令 |

## 中断计数专项风险

状态：已实现，Keil FD_Release 已编译通过，待上板验证，2026-06-03 按当前源码新增

专项文档：`docs/review/interrupt_counter_plan_2026-06-03.md`

| 风险 ID | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 |
|---|---|---|---|---|---|
| RISK-IRQ-001 | 高速 ISR 计数插点增加中断开销 | `System_Init.c:TIM3_IRQHandler`, `LedBar.c:TIM4_IRQHandler`, `Can_HDX.c:USB_LP_CAN1_RX0_IRQHandler` | 可能影响 10ms 系统节拍、1ms 灯板扫描或 CAN 接收时序 | 已控制，Keil 已编译，需上板验证 | TIM3/TIM4/CAN 仅调用 `IrqDebug_CountFast()`，不写事件环、不打印、不访问 Flash |
| RISK-IRQ-002 | 启动汇编默认 handler 调 C 函数 | `startup_stm32f10x_hd.s`, `IrqDebug.c` | 若链接或调用约定错误，会影响未实现向量兜底路径 | Keil 已编译通过，仍需未实现向量实测 | 已保留记录后 `B .` 停住语义；上板或调试时确认 `last_vectactive` |
| RISK-IRQ-003 | 阶段标记未直接写入 `conf.c` | `rtc_sleep_port.c`, `SleepDeal.c`, `conf/conf.c` | `Sys_StopMode()` 若被新增其他直接调用，可能缺少 STOP 阶段标记 | 部分验证 | 因 `conf.c` 含历史非 UTF-8 字节，本轮避免重编码；后续新增直接调用 `Sys_StopMode()` 时必须在调用层标记阶段 |
| RISK-IRQ-004 | Release 默认保留中断计数 | `Project_Config.h`, `IrqDebug.h` | 增加少量 RAM 和 ISR 指令开销 | 已按需求实现，需实测确认 | 默认打开轻量计数；如量产功耗或时序评估不接受，可关闭 `PROJECT_CFG_IRQ_DEBUG_ENABLE` |

## BMS App IO 与 RTC 低功耗风险

| 风险 ID | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 |
|---|---|---|---|---|---|
| RISK-RTC-IO-001 | `PB0 / AFE1_PRO_EN` 在 `InitIO()` 中配置，但 RTC 唤醒恢复的 `InitIO_rtc()` 未显式恢复 | `conf.c:InitIO()`, `conf.c:InitIO_rtc()` | 如果 PB0 控制 AFE 保护或供电，唤醒后可能状态不确定 | 旧 commit 同样存在，需硬件确认 | 核对原理图并上板测 PB0 唤醒前后状态 |
| RISK-RTC-IO-002 | `PA3 / 2737_EN` 在 RTC 模式排除模拟输入 | `conf.c:IOstatus_RTCMode()` | 若该脚应关闭，可能增加休眠电流 | UNKNOWN | 测 STOP 电流，并确认 PA3 休眠期硬件要求 |
| RISK-RTC-IO-003 | `PB14 / AFE1_CTL` 在 RTC 模式排除模拟输入 | `conf.c:IOstatus_RTCMode()` | 可能影响 AFE 控制或低功耗漏电 | UNKNOWN | 核对 AFE 控制脚原理图，测 STOP 前后电平 |
| RISK-RTC-CAN-001 | RTC 周期唤醒后不再主动广播 CAN | `Can_HDX.c`, `rtc_sleep.c`, `RTC.c` | 休眠中 CAN 不再周期可见；换取更低功耗和更简单低功耗链路 | 用户已确认 | 上板验证 CMNT 睡前关闭、唤醒恢复后打开，确认外部唤醒后 CAN 正常恢复 |
| RISK-RTC-IWDG-001 | IWDG 开启时 RTC 唤醒周期最大 10 秒 | `RTC.c` | 与极低功耗目标可能冲突 | CONFLICT | 结合整机功耗目标确认 IWDG 与 RTC 周期策略 |
| RISK-RTC-IWDG-002 | `PROJECT_CFG_WDOG_ENABLE` 与 IWDG 实际行为必须一致 | `Project_Config.h`, `AppInit.c`, `System_Init.c` | 若宏和实际硬件状态漂移，RTC wake 安全窗口会误判 | 已处理，需构建/长稳验证 | 当前默认 `1`；`Init_IWDG()` 和 `IWDG_Feed()` 已按宏门控 |
| RISK-RTC-CAN-002 | CAN busy 查询不能被 debug 消费 | `Can_HDX.c`, `SystemDebug.c`, `Runtime.c`, `rtc_sleep.c` | 若 debug 先更新 CAN 接收计数，低功耗可能误判通信空闲 | 已处理，需通信回归 | 低功耗使用 `Can_IsBusy()`；debug/heartbeat 使用无副作用 `Can_PeekBusy()` |
| RISK-RTC-DBG-001 | DBGMCU 低功耗调试保持只能显式打开 | `conf.h`, `System_Init.c` | 若调试宏误带入 Release，STOP/SLEEP/STANDBY 调试保持会抬高功耗 | 已处理，仍需实测确认 | Release 默认关闭 DBGMCU 低功耗调试位，仅 Debug/显式宏打开；功耗实测读 `DBGMCU->CR` |
| RISK-RTC-AGING-001 | 工厂老化 active 只阻塞 HICCUP RTC STOP | `rtc_sleep.c`, `FactoryAging.c` | 若误扩展为阻塞 deep，会影响低压/关机；若失效则老化计时可能被 RTC STOP 打断 | 已处理，需上板验证 | 保持当前窄范围实现：老化 running 不进 HICCUP RTC STOP，但不阻塞 `DEEP_MODE/NORMAL_MODE` reset sleep |
| RISK-RTC-AFE-001 | AFE sleep block 主判断未接入 | `rtc_sleep.c`, `rtc_sleep_afe_sh367309.c` | AFE 异常、PCHG、保护状态下可能进入 STOP | UNKNOWN | 确认 SH367309 状态和 HICCUP sleep 关系，再决定接入 RTC block 或删除保留 reason |
| RISK-RTC-PARAM-001 | 上位机可写的普通休眠/RTC 参数当前未进入主策略 | `DataDeal.h`, `Sci_Upper.c`, `rtc_sleep.c`, `RTC.c` | 参数读写与真实行为不一致，维护和调试误导 | CHANGE_NEEDED | 确认保留/接入/删除，不要继续保留“看似有效”的参数 |
| RISK-RTC-WRAPPER-001 | `app_lowpower.c` 曾暴露多组非主路径 wrapper | `Runtime.c`, `rtc_sleep.c`, `rtc_sleep.h` | 增加低功耗入口数量和阅读成本 | 已处理 | 已删除未使用 wrapper 和 `app_lowpower.c/h`，只保留真实 `Runtime_RunOnce()->rtc_sleep()` 主路径 |

## 状态变量净删减专项风险

状态：部分验证，2026-06-02 按当前源码新增

专项文档：`docs/review/state_variable_audit.md`

| 风险 ID | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 |
|---|---|---|---|---|---|
| RISK-SV-LED-001 | 直接删除 `s_ledbar.initialized` 会重现 LED 初始化回归风险 | `AppInit.c:36-45`, `LedBar.c:171-177`, `LedBar.c:1034-1067`, `LedBar.c:1299-1368`, `conf/conf.c:273-349` | LED 每轮重置、启动显示窗口异常、低功耗阻塞、TIM4 ISR 未初始化访问；RTC STOP 唤醒后若完整重置会丢显示/防抖状态 | 已处理，需 LED/低功耗回归 | 已显式 `LedBar_Init()` 并移除 `APP_LedBar()` 懒初始化；保留 `initialized`、外部 API/ISR 防护和 STOP 前/唤醒后恢复链，后续再评估是否继续减少 `LedBar_EnsureInit()` |
| RISK-SV-LP-001 | `readyToSleep` 同时被低功耗、LED、日志、debug 使用，直接删除会丢提交前动作 | `rtc_sleep.c`, `rtc_sleep.h`, `LedBar.c`, `LogRecord.c`, `Runtime.c`, `SystemDebug.c`, `tools/stlink_bms_monitor.ps1` | 已删除全局阶段变量；仍需实测确认 HICCUP/NORMAL/DEEP、sleep SOC、`BMS_SLEEP` 日志和 ST-Link `RtcReady` 派生输出 | 已处理，需低功耗回归 | `rtc_sleep()` 已改为本地 `sleep_mode` 提交；LED/日志不再读写 ready；`SleepDeal_Continue()` 和 `LowPowerSleep_SaveResetState()` 保留 reset sleep 收尾；ST-Link 监控按新结构解析 |
| RISK-SV-DBG-001 | 控制状态和 debug mirror 混在 `g_stLowPowerRtcStatus` 中 | `rtc_sleep.h:50-58`, `rtc_sleep.c:86-92`, `SystemDebug.c:536-541` | 后续维护者可能把展示字段误认为控制字段，增加低功耗理解成本 | KEEP_BUT_REFACTOR | 文档标记后，单独批次迁移纯展示字段 |
| RISK-SV-PROD-001 | `ProductionID.c` 曾依赖主循环一次性 flag 初始化产品信息 | `ProductionID.c`, `ProductionID.h`, `AppInit.c`, `Runtime.c:57` | 已减少主循环一次性状态；仍需确认 `0xC002` 默认信息读取 | 已处理 | `InitProID()` 已收口到启动运行态初始化；`App_ProID_Deal()` 保留为空 hook 维持 PROID heartbeat |
| RISK-SV-AGING-001 | `FactoryAging.c` 的多个私有运行态变量已收口为结构体字段 | `FactoryAging.c` | 若字段初值或替换错误，会影响老化状态、剩余时间、BKP/Flash 保存节流、完成重试和 MOS 模式缓存 | 已处理，需老化回归 | 本批次只改变变量组织方式，不改状态机、BKP/Flash 存储格式、CAN/Modbus 可见接口；后续用老化 start/stop/reset/set hours 和 `0x14F80208` 广播回归 |
| RISK-SV-LOG-001 | `LogRecord.c` 私有运行态已收口为结构体字段，但 `su32_Interval_S_Tcnt` 仍是跨模块符号 | `LogRecord.c`, `LogRecord.h`, `rtc_sleep_port.c` | 如果误搬 `su32_Interval_S_Tcnt`，RTC 睡眠补偿会丢；如果误清事件 latch，日志去重会改变 | 已处理，需日志回归 | 本批次只收口私有状态，保留外部时间累计符号；后续用 startup/sleep/fault 日志、事件读取和 reset event record 回归 |
| RISK-SV-AFE-CUR-001 | `DataDeal.c` AFE current zero 私有运行态已收口为结构体字段 | `DataDeal.c`, `SOC.c` | 若字段替换错误，会影响启动零点、自动零点学习、电流方向、deadband 输出和 SOC sample seq 驱动 | 已处理，需电流/SOC 回归 | 本批次不改 CADC 读取、换算公式、deadband 和 `AfeCurrent_GetSeq()` 口径；后续用真实充/放电方向、零点、`0xD000` 电流和 SOC 回归 |
| RISK-SV-DATA-001 | `DataDeal.c` 中多个静态状态混合客户逻辑、保护逻辑和认证逻辑 | `DataDeal.c:51-95`, `DataDeal.c:930-1055` | 变量看似可删，但可能影响 MOS、RF_EN、过温、拔 5V 行为和认证 | UNKNOWN | 未确认产品/认证背景前只做文档归类，不改源码 |
| RISK-SV-KEEP-001 | 把真实历史状态误判为“不必要变量” | `LedBar.c:890-1009`, `SOC.c:116-142`, `DataDeal.c:825-917` | 会导致误唤醒、重复积分、故障恢复失败、通信状态丢失 | MUST_KEEP | 明确边界：防抖、边沿、累计延时、ISR 队列、SOC sample seq 第一批不删 |

## RTC 唤醒后 ADC 采样收敛与简化风险

状态：已确认并已执行，源码已修改，硬件待验证，2026-06-04 更新

专项文档：`docs/review/adc_rtc_wakeup_simplification_2026-06-04.md`

| 风险 ID | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 |
|---|---|---|---|---|---|
| RISK-ADC-WAKE-001 | VBC/MOS/Type-C 软件滤波已删除，最终值会更快反映 raw 抖动 | `ADC_UpdateVbc()`, `ADC_UpdateMosTemp()`, `ADC_UpdateTypeCCurrent()` | 诊断值和 Type-C 辅助电流可能比旧版本更敏感 | 已执行，需上板验证 | 真板观察 raw、VBC、MOS 温度、Type-C 电流抖动；必要时只对具体问题补最小保护 |
| RISK-ADC-WAKE-002 | ADC 重新初始化后只丢弃 1 个 10ms tick | `ADC_ResetAnlogCalSchedule()`, `App_AnlogCal()` | 若硬件上电/分压/参考电压稳定时间更长，可能首个结果短时异常 | KEEP_BUT_REFACTOR | RTC STOP 唤醒后用 Keil Watch 或 debug 快照确认 20ms/50ms/200ms 内结果稳定性 |
| RISK-ADC-WAKE-003 | Type-C 电流不再做 32 点平均和连续零点确认 | `ADC_UpdateTypeCCurrent()`, `AD_CurZeroDeadband` | 小电流边界和插拔瞬态可能更直接进入 SOC 附加放电 | 已执行，需上板验证 | 保留死区/限幅；实测 0A、小负载、阶跃负载和断开清零 |
| RISK-ADC-WAKE-004 | `App_AnlogCal()` 已改 latest-sample，结果刷新仍受调用频率限制 | `App_AnlogCal()`, `Runtime.c` | 若未来把调用周期改成 1s，结果不会积压历史滤波，但刷新率仍为 1s | 已执行 | 后续修改调用频率时同步评估显示/诊断刷新需求，不再用滤波补跑掩盖调度问题 |
| RISK-ADC-WAKE-005 | 简化 ADC 时误把 ADC VBC 当成主总压 | `DataDeal.c:201-224`, `DataDeal.c:980-986` | 可能改变保护、CAN/Modbus 上报和校准口径 | MUST_KEEP | 已保持 AFE 单体累加作为主总压；ADC VBC 仅作诊断和 Type-C 折算辅助 |

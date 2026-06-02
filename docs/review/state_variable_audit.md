# 状态变量净删减专项审计

文档状态：部分验证
源码验证：已按当前源码只读核对
最后更新时间：2026-06-02
修改范围：本文记录审计和已执行的低风险小批次；除已标记“已处理/已执行”的项外，不修改 `.c/.h`、Keil 工程、编译宏、协议和烧录脚本
未确认事项：所有 `REMOVE_CANDIDATE` 和 `KEEP_BUT_REFACTOR` 项必须由用户确认后才能进入源码修改

## 1. 审计目标

本专项目标不是单纯减少变量数量，而是在不影响功能、保护逻辑、硬件时序和协议兼容的前提下，删除或收口不必要的状态缓存，让主调度关系直接体现在代码里。

判断原则：

| 类型 | 判断标准 | 默认处理 |
|---|---|---|
| 当前输入可直接推导 | 每次调用都能从 GPIO、寄存器、全局报告或函数返回值即时得到 | 优先删除 |
| 固定启动顺序兜底 | 只是因为初始化入口不清晰而设置的 `initialized` 类标志 | 先显式初始化，再删除或收口 |
| 单函数阶段残留 | 在同一次函数调用内置位后立即消费 | 优先改成本地变量或直接流程 |
| 跨模块握手状态 | 一个模块置位，另一个模块消费 | 先收口所有权，再决定是否删除 |
| 历史采样/边沿/累计延时 | 防抖、边沿检测、故障持续时间、RTC 累计秒数、扫描索引 | 保留，必要时改名 |
| ISR 与主循环共享 | 中断写、主循环读，或硬件异步队列 | 保留，必要时缩小可见范围 |
| 协议/调试快照 | 对外寄存器或 debug watch 需要稳定读取 | 保留或改为只读快照，不参与控制 |

## 2. 当前真实调度链

当前工程是裸机 cooperative main loop，没有 RTOS。

```text
main()
  AppInit_Boot()
    AppInit_InitDevice()
    AppInit_InitRuntimeState()
    Init_RTC()
  while (1)
    Runtime_RunOnce()
      Runtime_RunFrontTasks()
        SysTime_LatchTaskFlags()
        FactoryAging_Task()
        APP_LedBar()
        App_AFEGet()
        SystemDebug_Snapshot()
      Runtime_RunIoAndPowerTasks()
        AppInit_ServiceSci()
        App_AnlogCal()
        rtc_sleep()
        App_Can()
      Runtime_RunBackgroundTasks()
        App_FlashUpdate()
        App_LogRecord()
        App_ProID_Deal()
        Feed_IWatchDog
```

关键源码证据：

| 事实 | 源码证据 |
|---|---|
| 主入口只有 `AppInit_Boot()` 和 `Runtime_RunOnce()` | `main.c:5-12` |
| 启动初始化没有显式 `LedBar_Init()` | `AppInit.c:44-49` |
| LED 在前台任务中先于低功耗运行 | `Runtime.c:7-26` |
| `rtc_sleep()` 在 IO/电源段、`App_LogRecord()` 在后台段 | `Runtime.c:28-61` |
| AFE/SOC 200ms 主链路是 `App_AFEGet()` 内部执行 | `DataDeal.c:1063-1085` |

## 3. 状态变量审计表

| ID | 变量/字段 | 文件 | 当前作用 | 分类 | 初步处理建议 |
|---|---|---|---|---|---|
| SV-LED-001 | `s_ledbar.initialized` | `LedBar.c:79`, `LedBar.c:171-177`, `LedBar.c:1034-1067` | LedBar 懒初始化和 TIM4 ISR 早到保护 | KEEP_BUT_REFACTOR | 候选第一批。先把 `LedBar_Init()` 显式放入 `AppInit_Boot()` 合适位置，确认 TIM4 只在初始化后使能，再删除分散 `LedBar_EnsureInit()` 或只保留 ISR 保护 |
| SV-LED-002 | `s_ledbar.scan_timer_initialized` | `LedBar.c:87`, `LedBar.c:356-412` | 避免重复初始化 TIM4，记录扫描定时器硬件配置状态 | MUST_KEEP | 与硬件定时器启停相关，不能直接删；可在 LED 初始化收口后复核是否能和 `scan_timer_enabled` 合并 |
| SV-LED-003 | `s_ledbar.scan_timer_enabled` | `LedBar.c:88`, `LedBar.c:387-412`, `LedBar.c:1241-1258` | 记录 TIM4 当前是否运行，并作为低功耗阻塞条件 | MUST_KEEP | 当前是硬件运行状态缓存，影响 STOP 前判断，先保留 |
| SV-LED-004 | `s_ledbar.key_filter_initialized`, `key_active`, `key_on_10ms`, `key_off_10ms` | `LedBar.c:95-98`, `LedBar.c:946-1009` | 按键首次采样预置、防抖、边沿和长按判断 | MUST_KEEP | 真实历史状态，不能因为主循环固定而删除 |
| SV-LED-005 | `s_ledbar.mcu_wk_filter_initialized`, `mcu_wk_active`, `mcu_wk_on_10ms`, `mcu_wk_off_10ms` | `LedBar.c:99-102`, `LedBar.c:890-928` | `MCU_WK` 首次采样预置、防抖和唤醒显示触发 | MUST_KEEP | 真实历史状态，先保留 |
| SV-LED-006 | `s_ledbar.sleep`, `blank`, `number`, `indicator_mask`, `frame`, `scan_index` | `LedBar.c:80-86`, `LedBar.c:1263-1288`, `LedBar.c:1299-1368` | LED 当前显示模型和 Charlieplexing 扫描位置 | MUST_KEEP | 显示输出与 1ms ISR 共享状态，不能直接删；可后续整理命名 |
| SV-LED-007 | `s_ledbar.soc_display_10ms`, `startup_display_armed` | `LedBar.c:89-90`, `LedBar.c:820-845`, `LedBar.c:1322-1333` | 启动/按键显示窗口计时和只触发一次控制 | MUST_KEEP | 用户可见交互状态，不能直接删；后续可把窗口逻辑命名得更直观 |
| SV-LP-001 | `g_stLowPowerRtcStatus.readyToSleep` | `rtc_sleep.h:52`, `rtc_sleep.c:107-134`, `rtc_sleep.c:304-353` | 低功耗 pending/ready 标志；当前在 `rtc_sleep()` 内置位后同次消费，同时被 LED、Runtime、LogRecord 读取 | REMOVE_CANDIDATE | 高价值候选。先收口为 `sleep_mode` 本地决策或明确的 `LowPower_CommitPending`，再处理 LED 保存 SOC 和日志清除副作用 |
| SV-LP-002 | `g_stLowPowerRtcStatus.mode` | `rtc_sleep.h:51`, `rtc_sleep.c:107-123`, `rtc_sleep.c:337-353` | 当前低功耗请求模式，来自低压、空闲、按键、AFE fault、上位机等路径 | MUST_KEEP | 跨模块请求状态，保留；可改名为 `requestMode` 或收窄写入口 |
| SV-LP-003 | `g_stLowPowerRtcStatus.blockReason` | `rtc_sleep.h:53`, `rtc_sleep.c:173-215`, `SystemDebug.c` | 当前低功耗阻塞原因，用于 debug/上位机观察 | KEEP_BUT_REFACTOR | 保留观察价值，但不要参与太多控制；后续统一 `LP_BLOCK_*` 和 `LOW_POWER_RTC_BLOCK_*` 口径 |
| SV-LP-004 | `g_stLowPowerRtcStatus.rtcWake/delaySeconds/delayTargetSeconds/elapsedSeconds` | `rtc_sleep.h:54-57`, `rtc_sleep.c:86-92` | 调试/状态快照镜像 | KEEP_BUT_REFACTOR | 这些字段多为展示镜像，可考虑只在 `SystemDebug` 中生成，不放进控制状态结构 |
| SV-LP-005 | `s_u16IdleDelaySeconds` | `rtc_sleep.c:23`, `rtc_sleep.c:207-220` | 无阻塞空闲累计时间，达到 `sys_time.time_enter_rtc` 后进 HICCUP | MUST_KEEP | 真实累计时长，保留 |
| SV-LP-006 | `deep_sleep_delay_seconds`, `force_deep_delay_seconds` | `rtc_sleep.c:138-164` | 低压/强制低压 deep sleep 持续时间计数 | MUST_KEEP | 保护相关累计状态，保留 |
| SV-LP-007 | `last_ext_comm_count` | `rtc_sleep.c:138`, `rtc_sleep.c:187-192` | 通过外部通信计数变化判断通信活动 | MUST_KEEP | 边沿/变化检测状态，保留；可改名为 `s_last_ext_comm_count` |
| SV-LP-008 | `s_u32RtcSleepElapsedSeconds`, `s_u32RtcWakeCycles` | `rtc_sleep.c:24-25`, `rtc_sleep.c:249-301` | RTC STOP hiccup 多周期累计休眠秒数和周期数 | MUST_KEEP | SOC 休眠补偿和运行时间恢复依赖，保留 |
| SV-LP-009 | `g_irq_t` | `rtc_sleep.c:13`, `rtc_sleep.c:225-295` | STOP 唤醒源记录，跨异常判断和唤醒后动作 | MUST_KEEP | 硬件异步结果，保留；后续可收口为局部返回值，但需实测 |
| SV-LOG-001 | `BMS_LOG_POINT`、`BMS_LOG_RECORD`、`s_log_record_flag`、`s_u32_LogRecord_UptimeSeconds`、`s_u32_LogRecord_LastSaveSeconds[]`、`s_u8_LogRecord_LastSaveValid[]`、`su8_Event[]`、`su8_CBC_Temp` | `LogRecord.c` | 日志记录点、记录数组、startup/sleep 请求、运行秒计数、重复保存抑制和故障边沿去重 | 已处理 | 已在 SV-STRUCT-02 中收口为单个 `LogRecordRuntime s_log_record`；保留外部引用的 `su32_Interval_S_Tcnt`，不改变日志格式和 Flash 保存接口 |
| SV-LOG-002 | `su32_Interval_S_Tcnt` | `LogRecord.c`, `LogRecord.h`, `rtc_sleep_port.c` | 日志事件时间间隔累计，同时被低功耗端口按 RTC 睡眠秒数补偿 | MUST_KEEP | 跨模块外部符号，先保留；若后续收口，需先改低功耗端口接口 |
| SV-SYS-001 | `s_system_status` | `System_Monitor.c:5`, `System_Monitor.c:115-164` | 系统运行状态快照，供 `0xD000`、CAN、debug 和 SOC feature 读取 | KEEP_BUT_REFACTOR | 保留对外快照，但检查是否有字段只是重复事实；不能直接删协议可见状态 |
| SV-SYS-002 | `s_system_onoff_func` | `System_Monitor.c:4`, `System_Monitor.c:166-201`, `Sci_Upper.c:2020/2043` | 上位机可写功能开关，SOC fixed/zero 依赖 | KEEP_BUT_REFACTOR | 需要确认 `SystemFeature_SetById()` 是否仍是客户需求；未确认前不能删 |
| SV-RUN-001 | `s_last_fault`, `s_last_lp_mode` | `Runtime.c:88-108` | debug event 边沿检测 | MUST_KEEP | 调试边沿状态，保留；可后续移入 `SystemDebug` 统一管理 |
| SV-PROD-001 | `su8_StartUpFlag` | `ProductionID.c:32-40` | 让 `App_ProID_Deal()` 只初始化一次产品信息 | 已处理 | 已在 SV-CLEAN-01 中把 `InitProID()` 收口到 `AppInit_InitRuntimeState()`，`App_ProID_Deal()` 只保留 runtime heartbeat hook |
| SV-AGING-001 | `s_u8FactoryAgingState`、`s_u32FactoryAgingElapsed10ms`、`s_u32FactoryAgingLastTick`、`s_u32FactoryAgingLastBkpSave10ms`、`s_u32FactoryAgingLastFlashSave10ms`、`s_u32FactoryAgingNextFinishRetry10ms`、`s_u16FactoryAgingDurationHours`、`s_u8FactoryAgingBkpSaveValid`、`s_u8FactoryAgingFlashSaveValid`、`s_u8FactoryAgingMosMode` | `FactoryAging.c:28-37` | 老化模块运行态、进度、保存节流、完成重试和 MOS 模式缓存 | 已处理 | 已在 SV-STRUCT-01 中收口为单个 `FactoryAgingRuntime s_factory_aging`；不改变老化状态机、BKP/Flash 保存格式、CAN/Modbus 可见状态 |
| SV-DATA-001 | `charger_detect_and_keyLogi_200ms()` 内 `state` | `DataDeal.c:51-95` | 充电器插拔状态机，拔 5V 后请求 deep sleep | UNKNOWN | 产品交互未确认，不能删；先确认拔 5V 行为 |
| SV-DATA-002 | `u8IICFaultcnt*`, `u8WakeCnt*`, `su16_Sleep_DelayT*` | `DataDeal.c:3-6`, `DataDeal.c:825-917` | AFE 通信错误计数、恢复重试和持续故障后休眠 | MUST_KEEP | 保护/恢复相关历史状态，保留 |
| SV-DATA-003 | `new_todo_logi()` 内 `mos_state/state_fuse/rong_fuse/err_afe/delay_cnt` | `DataDeal.c:930-1055` | MOS 过温、UL 认证、RF_EN 熔断类客户逻辑状态 | UNKNOWN | 需求不清，不能删；先把客户逻辑归属确认后再拆模块 |
| SV-SOC-001 | `s_u32LastAfeCurrentSampleSeq` | `SOC.c:116-142` | 防止 SOC 在无新 AFE 电流样本时重复积分 | MUST_KEEP | 关键算法状态，保留 |
| SV-RTC-001 | `TimeDisplay` | `RTC.c:3`, `RTC.c:494-501`, `RTC.c:537-541` | RTC 秒中断置位、`App_RTC()` 消费后清零 | MUST_KEEP | ISR 到主循环的秒更新 pending 标志，保留；可后续改名为 `s_rtc_second_pending` |

## 4. 第一批建议候选

第一批只建议处理“低风险、单一职责、可快速验证”的候选，不碰 AFE 保护、SOC 算法、协议寄存器、CAN ID、Flash 地址、IAP、RTC 唤醒源。

| 批次 | 候选 | 目标 | 前置确认 | 验证 |
|---|---|---|---|---|
| SV-CLEAN-01 | `ProductionID.c` 的 `su8_StartUpFlag` | 产品信息初始化从主循环一次性 flag 收口到启动阶段 | 已按“先只做低风险”执行 | `rg InitProID/App_ProID_Deal`、Modbus `0xC002` 读取、编译 |
| SV-CLEAN-02 | `s_ledbar.initialized` 的分散懒初始化 | 明确 LedBar 初始化时序，减少 `LedBar_EnsureInit()` | 确认 `LedBar_Init()` 可放入 `AppInit_Boot()`，且 TIM4 不会早于初始化触发 | LED 启动显示、按键显示、TIM4 扫描、STOP 前 GPIO、编译 |
| SV-CLEAN-03 | `readyToSleep` 阶段变量 | 让低功耗提交流程变成局部决策，不再用全局 ready 标志绕 LED/日志 | 确认睡前 SOC 保存和 sleep 日志写入时序 | `rtc_sleep()` host 静态检查、LED sleep SOC、日志 `BMS_SLEEP`、HICCUP/DEEP/NORMAL 回归 |
| SV-CLEAN-04 | `g_stLowPowerRtcStatus` 中纯 debug 镜像字段 | 把 `rtcWake/delay/elapsed` 转为 debug 快照或只读状态，减少控制状态结构 | 确认上位机/调试工具是否直接依赖这些字段 | `SystemDebug`、Modbus `0xD000/0xD300`、ST-Link monitor |
| SV-STRUCT-01 | `FactoryAging.c` 模块私有运行态变量 | 把同生命周期的老化状态集中成 `s_factory_aging`，便于 Keil Watch，不改变业务行为 | 已按用户“开始”执行 | `rg` 旧符号、`git diff --check`、`clang -fsyntax-only`；老化 CAN/上位机仍需实测 |
| SV-STRUCT-02 | `LogRecord.c` 模块私有运行态变量 | 把日志记录点、记录数组、请求 flag、重复保存抑制和事件边沿状态集中成 `s_log_record` | 已按用户“1、2、3 都做”执行 | `rg` 旧符号、`git diff --check`、`clang -fsyntax-only`；日志读写和 sleep/startup 事件仍需实测 |

## 5. 明确不建议第一批处理

| 模块 | 不建议先动的变量 | 原因 |
|---|---|---|
| SOC | `s_soc`, `s_saved_soc`, `s_u32LastAfeCurrentSampleSeq`, OCV/满电/低压相关计数 | 算法用户体验和保护边界强，必须有主机回放和实测 |
| AFE/DataDeal | AFE fault/recover 计数、current zero 状态、MOS/UL 认证状态 | 保护和客户逻辑不清，删除风险高 |
| CAN/Sci | TX/RX 队列、busy、pending、read block 状态 | 中断/主循环异步通信状态，不能因调度固定删除 |
| Flash/Log | Flash busy、journal 状态、日志边沿状态 | 掉电安全和事件去重依赖 |
| RTC/SleepDeal | BKP flag、唤醒合法性、`g_irq_t`、RTC elapsed | 低功耗唤醒链路需要硬件验证 |

## 6. 需求确认表

| 字段 | 说明 |
|---|---|
| Requirement ID | 需求 ID |
| Requirement description | 需求描述 |
| Evidence from code | 代码证据 |
| Current behavior | 当前行为 |
| Risk | 风险 |
| Codex judgment | Codex 判断 |
| Question for user | 需要用户确认的问题 |
| Suggested decision | 建议决策 |
| User decision placeholder | 用户决策占位 |

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-SV-001 | LedBar 初始化应由固定启动顺序显式完成，而不是由每个 API 懒初始化兜底 | `AppInit.c:44-49`, `LedBar.c:171-177`, `Runtime.c:18` | `APP_LedBar()` 首次运行时自动 `LedBar_Init()`，多个外部 API 都调用 `LedBar_EnsureInit()` | 删除不当会重现 LED 重复初始化/闪烁，或 TIM4 ISR 访问未初始化状态 | KEEP_BUT_REFACTOR | 是否允许把 `LedBar_Init()` 显式加入启动流程，并逐步删除分散懒初始化？ | 同意先做小批次，保留 ISR 安全保护直到验证完成 | 待确认 |
| REQ-SV-002 | 低功耗提交流程应只保留一个清晰状态源，避免 `readyToSleep` 同时服务提交、LED、日志和 debug | `rtc_sleep.c:107-134`, `rtc_sleep.c:304-353`, `LedBar.c:1314-1318`, `LogRecord.c:135-142` | `readyToSleep` 在 `rtc_sleep()` 内置位后同次消费，又被 LED 和日志清除路径读取 | 删除不当会丢 sleep SOC 保存、BMS_SLEEP 日志或低功耗 heartbeat busy 状态 | REMOVE_CANDIDATE | 是否允许把 `readyToSleep` 改成本地提交决策，并把 LED/日志收尾放入明确的 sleep commit 流程？ | 同意作为第二批高价值净删减，先画调用链再改 | 待确认 |
| REQ-SV-003 | 纯 debug/status 镜像字段不应混入控制状态结构 | `rtc_sleep.h:50-58`, `rtc_sleep.c:86-92`, `SystemDebug.c:536-541` | `g_stLowPowerRtcStatus` 同时保存控制状态和展示字段 | 继续混用会让维护者误以为展示字段参与低功耗控制 | KEEP_BUT_REFACTOR | 是否允许把只读展示字段迁移到 `SystemDebug` 或明确标记为 debug mirror？ | 同意先文档标记，源码阶段单独处理 | 待确认 |
| REQ-SV-004 | 产品信息初始化不应依赖主循环内的一次性 `static flag` | `ProductionID.c:32-40`, `Runtime.c:57`, `AppInit.c:34-42` | `InitProID()` 已在启动运行态初始化中调用；`App_ProID_Deal()` 保留为空 hook，维持 `DBG_MODULE_PROID` heartbeat | 仍需上板/上位机读取 `0xC002` 确认默认信息 | 已处理 | 是否允许把 `InitProID()` 放到启动初始化，主循环只保留真实后台处理？ | 已按低风险批次执行 | 已执行 |
| REQ-SV-005 | 按键、`MCU_WK`、SOC sample seq、AFE fault 计数等真实历史状态必须保留 | `LedBar.c:890-1009`, `SOC.c:116-142`, `DataDeal.c:825-917` | 这些状态用于防抖、边沿、去重积分、持续故障判断 | 误删会造成误唤醒、重复积分、故障恢复失效 | MUST_KEEP | 是否接受“不是所有状态变量都删，只删重复事实/残留阶段”的边界？ | 保留这些历史状态，只做命名和职责整理 | 待确认 |
| REQ-SV-006 | `DataDeal.c` 中客户逻辑状态必须先确认需求归属，不能直接按“变量多”删除 | `DataDeal.c:51-95`, `DataDeal.c:930-1055` | 充电器插拔、MOS 过温、UL 认证、RF_EN 熔断类逻辑混在 200ms 链路 | 直接删除可能改变安全输出和客户认证行为 | UNKNOWN | `charger_detect_and_keyLogi_200ms()` 和 `new_todo_logi()` 内这些状态是当前产品需求、认证需求，还是历史残留？ | 先列入需求确认，不进第一批删除 | 待确认 |
| REQ-SV-007 | 同一模块、同一生命周期、同一调试视角的私有运行态变量应优先收口到模块 runtime 结构体 | `FactoryAging.c:28-37`, `FactoryAging.c:45-627` | 老化模块原有 10 个文件级静态变量分别保存 state、elapsed、last tick、保存状态、retry 和 MOS mode | 若结构体字段初值或替换错误，会影响老化进度和完成保存；但本批次不改持久化格式 | 已处理 | 是否允许对单文件私有运行态做结构体收口，提升 Keil Watch 可读性？ | 已按低风险结构体收口批次执行 | 已执行 |
| REQ-SV-008 | 日志模块私有运行态应集中管理，同时保留外部补偿时间符号 | `LogRecord.c`, `LogRecord.h`, `rtc_sleep_port.c` | 日志记录点、记录数组、请求 flag、重复记录抑制和事件 latch 已收口到 `LogRecordRuntime s_log_record`；`su32_Interval_S_Tcnt` 仍由低功耗端口引用 | 若误搬外部符号会影响 RTC 睡眠秒数补偿；若误清 latch 会影响事件去重 | 已处理 | 是否允许先收口私有状态，保留跨模块时间累计符号？ | 已执行；不改日志格式、Flash 保存格式和低功耗补偿接口 | 已执行 |

## 7. 下一步执行边界

进入源码修改前必须满足：

1. 用户确认 `REQ-SV-001` 到 `REQ-SV-006` 的方向。
2. 第一批只允许处理 `SV-CLEAN-01`、`SV-STRUCT-01` 或 `SV-CLEAN-02` 这种低风险小范围项。
3. 每批必须同步更新本文、`requirement_confirmation.md`、`risk_list.md`、`refactor_plan.md` 和 `test_plan.md`。
4. 每批必须至少跑 `git diff --check`；涉及源码时再跑仓库检查脚本和可用编译/静态检查。
5. 未经确认，不修改 `.c/.h` 源码。

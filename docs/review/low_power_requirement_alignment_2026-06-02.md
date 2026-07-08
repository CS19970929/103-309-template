# 低功耗需求与当前实现对齐

文档状态：部分验证
源码验证：已按源码核对，未上板实测
日期：2026-06-02
范围：先只读梳理低功耗需求、当前实现、风险和简化方向；后续按已确认需求执行第一批低风险源码净删减。

更新说明：2026-06-02 后续按官方/行业调研和已确认需求执行低风险优化：Release 默认关闭 DBGMCU 低功耗调试保持；删除未使用 STOP wrapper、`app_lowpower.c/h` 模块、无消费者状态缓存、无调用 RTC SOC 访问 API、无效 port 参数/返回值和 `readyToSleep` 全局阶段变量。`LP_GetBlockReason()` 现已收口到 `rtc_sleep.c/h`，低功耗提交由 `rtc_sleep()` 本地 `sleep_mode` 决策完成。调研见 `docs/review/low_power_official_industry_research_2026-06-02.md`。

## 参考源码

- `103 + 309/Project/Source/main.c`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/rtc_sleep.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/LowPowerSleep.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/Project_Config.h`
- `103 + 309/Project/Source/System_Init.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/DataDeal.h`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/SocEnhance.c`

## 结论摘要

当前低功耗实现不是单一路径，而是三类机制叠加：

1. 运行态每轮主循环直接调用 `rtc_sleep()`，每秒判断是否进入 `HICCUP_MODE`。
2. `HICCUP_MODE` 在当前运行态内反复进入 `PWR_EnterSTOPMode()`，RTC alarm 周期唤醒后恢复外设；如果没有异常唤醒，则继续 STOP。
3. `NORMAL_MODE` 和 `DEEP_MODE` 走 reset sleep：先确认 sleep mode，再保存状态、写 BKP sleep flag、让 AFE sleep、`MCU_RESET()`，再由启动早期 `SleepDeal_HandleBootSleepStartup()` 进入 STOP 等有效唤醒。

复杂度主要来自两套 sleep 执行路径都在处理保存状态、CAN 电源、AFE、LED、唤醒合法性和日志；此前还有一层 `app_lowpower.c` bitmask、一层 `rtc_sleep.c` 8 位 block reason、一个 `readyToSleep` 全局阶段变量，以及若干当前无真实调用的 wrapper。本轮已删除 `app_lowpower.c/h` 和 `readyToSleep` 控制字段，详细 bitmask 和粗粒度 block reason 都收口在 `rtc_sleep.c/h`。

从“BMS 越简单越好”的目标看，后续简化应优先做净删减和口径统一，不应新增复杂状态机。已先处理 Release DBGMCU 回归和无调用 wrapper；剩余涉及产品行为的项目仍需按表确认。

## 当前运行链路

### 启动链路

```text
main()
  AppInit_Boot()
    SystemInit()
    InitDelay()
    SleepDeal_HandleBootSleepStartup()
    InitIO / InitSCI / InitE2PROM / InitAFE1 / InitCan / InitADC
    InitData_SOC()
    InitTimer()
    EnableLowPowerDebug()
    Init_IWDG()
    Init_RTC()
```

关键点：

- `SleepDeal_HandleBootSleepStartup()` 在完整 IO/AFE/CAN 初始化之前执行，用 BKP_DR2/DR3 判断是否是 sleep reset 启动。
- `Init_RTC()` 使用 BKP_DR1 判断 RTC 是否已初始化；若首次初始化会 `BKP_DeInit()`，因此 BKP 寄存器分配必须固定，不能随意复用。
- `EnableLowPowerDebug()` 当前仍在启动阶段调用；`conf.h` 已不再无条件定义 `__EnableLowPowerDebug__`，Release 默认清除 DBGMCU 低功耗调试保持位，调试时可通过显式宏打开。
- `PROJECT_CFG_WDOG_ENABLE` 当前默认为 1；`Init_IWDG()` 和 `IWDG_Feed()` 已按该宏门控，RTC wake period 安全窗口与实际 IWDG 行为一致。

### 主循环链路

```text
Runtime_RunOnce()
  FactoryAging_Task()
  APP_LedBar()
  App_AFEGet()
  AppInit_ServiceSci()
  App_AnlogCal()
  rtc_sleep()
  App_Can()
  App_FlashUpdate()
  App_LogRecord()
  App_ProID_Deal()
  Feed_IWatchDog
```

关键点：

- `APP_LedBar()` 在 `rtc_sleep()` 前执行，因此 LED 显示窗口会影响本轮能否进入低功耗。
- 运行态低功耗入口已从一行 wrapper 收敛为直接调用 `rtc_sleep()`。
- `rtc_sleep()` 不再先设置全局 `readyToSleep` 再消费；本轮使用局部 `sleep_mode` 直接执行 HICCUP/NORMAL/DEEP。
- `App_Can()` 在 `rtc_sleep()` 后执行，低功耗检查看到的是上一轮或当前队列状态；CAN 接收中断会更新 `sys_time.can_rcv_cnt`。

## 当前低功耗模式

| 模式 | 当前触发 | 当前执行 | 判断 |
|---|---|---|---|
| `NO_SLEEP` | 默认运行态 | 不睡眠 | MUST_KEEP |
| `HICCUP_MODE` | 空闲计数达到 `sys_time.time_enter_rtc`，当前为 30 秒 | 运行态内进入 STOP，RTC alarm 周期唤醒，正常则继续 STOP | MUST_KEEP，但实现可简化 |
| `NORMAL_MODE` | AFE 异常持续约 5 分钟后 `LowPower_Request(NORMAL_MODE)` | 走 sleep flag + AFE sleep + reset；启动早期等待合法唤醒 | KEEP_BUT_REFACTOR，触发条件需确认 |
| `DEEP_MODE` | 低压/强制低压、充电器拔除、长按按键、上位机功能号 `0x0A` | 走 sleep flag + AFE sleep + reset；启动早期等待合法唤醒 | MUST_KEEP，但触发来源需确认 |

## 当前入睡条件

### HICCUP 空闲入睡

`rtc_sleep()` 每秒调用一次 `lp_update_sleep_request()`：

- 若 `VCellMin <= 2800mV` 且充电电流 `<= 5`，持续 60 秒后请求 `DEEP_MODE`。
- 若 `VCellMin <= OtherElement.u16Sleep_Vlow` 且充电电流 `<= 5`，持续 `OtherElement.u16Sleep_TimeVlow * 60` 秒后请求 `DEEP_MODE`。
- 否则检查阻塞：
  - 充电电流或放电电流 `> 10`。
  - `GPIO_MCU_WK` active。
  - `RTC_ExtComCnt` 变化，代表 RS485/USART 有接收。
  - `LP_GetBlockReason() != 0`，代表框架层有阻塞。
- 无阻塞时，`g_stLowPowerRtcStatus.idle` 达到 `sys_time.time_enter_rtc` 后请求 `HICCUP_MODE`。

### Sleep 提交口径

`rtc_sleep()` 当前不再维护独立 `readyToSleep` 阶段变量：

- `lp_update_sleep_request()` 只负责更新 `g_stLowPowerRtcStatus.mode` 和 `block`。
- 本轮 `rtc_sleep()` 读取局部 `sleep_mode = g_stLowPowerRtcStatus.mode`。
- `sleep_mode == HICCUP_MODE` 时直接进入 `rtc_sleep_run_hiccup_cycle()`。
- `sleep_mode == NORMAL_MODE/DEEP_MODE` 时直接进入 `RtcSleep_PortCommitResetSleep()`，同步完成 `BMS_SLEEP` 日志和 `SleepDeal_Continue()`。
- `SystemDebug.g_dbg.lp.ready` 和 ST-Link `RtcReady` 只是由 `mode != NO_SLEEP` 派生的观察值，不再是控制字段。

### 框架层阻塞

`LP_GetBlockReason()` 使用 bitmask 检查：

- 充电电流 `> 10`。
- 放电电流 `> 10`。
- `Sci_IsAnyPortBusy()` 或 `Can_IsBusy()`。低功耗路径使用 `Can_IsBusy()` 消费并确认 CAN 接收活动；debug/heartbeat 只使用 `Can_PeekBusy()`。2026-06-04 起，普通周期广播 TX pending 不再作为 RTC idle 阻塞条件，CAN App 请求/ACK/read-block 和 RX 活动仍阻塞。
- `GPIO_MCU_WK` active。
- `StorageFlash_IsBusy()` 或 `u8FlashUpdateE2PROM != 0`。
- `u8FlashUpdateFlag != 0`。
- `g_stCellInfoReport.unMdlFault_Third.all != 0`。
- `LedBar_IsActiveForLowPower() != 0`。

当前问题：

- `LP_BLOCK_AFE_BUSY` 已删除，避免保留“看似支持但永远不触发”的 block bit；AFE not idle 是否阻塞仍保留为需求确认项。
- `RtcSleep_PortIsFactoryAgingActive()` 和 `RtcSleep_PortIsAfeSleepBlocked()` 这类未使用 wrapper 已删除；FactoryAging active 已按确认只阻塞 HICCUP RTC STOP，AFE not idle 是否阻塞仍需需求确认。
- `LOW_POWER_RTC_BLOCK_FACTORY_AGING` 已接入 HICCUP idle 阻塞；只阻止进入 RTC 周期 STOP，不阻止低压或外部请求的 `DEEP_MODE/NORMAL_MODE` reset sleep。
- `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` 枚举保留，但当前主判断不会产生该原因。

## 当前唤醒与恢复

### HICCUP STOP 周期

```text
rtc_sleep_run_hiccup_cycle()
  RtcSleep_PortPrepareRtcStop()
    LowPowerSleep_SaveCoreState()
      Can_PrepareSleep()
      SOC_SaveSnapshotBeforeSleep()
      FactoryAging_SaveProgressBeforeSleep()
    Init_RTC()
    IOstatus_RTCMode()
    InitWakeUp_RTCMode()
  Sys_StopMode()
  RtcSleep_PortDisableStopWakeup()
  RtcSleep_PortRestoreAfterStop()
    InitRunAfterStopWakeup()
  rtc_sleep_has_wakeup_exception()
```

无异常时：

- 读取 AFE 电压/温度。
- 检查电流唤醒、AFE 唤醒、紧急低压。
- 若只是 RTC alarm 周期唤醒，则执行 `SOC_ApplyRtcRelaxationCompensation()` 并继续 STOP。

异常时：

- 退出 HICCUP，`LowPower_Request(NO_SLEEP)`。
- 猜测/记录唤醒源，按键唤醒会触发 LED SOC 显示窗口。
- 把累计 sleep 秒数加回运行时间。

### Reset sleep 启动早期唤醒

```text
SleepDeal_Continue()
  select boot flag
  LowPowerSleep_SaveResetState()
    LowPowerSleep_SaveCoreState()
    LedBar_SaveSleepSoc()
  BootFlag_Write(BKP_DR2/BKP_DR3)
  InitAFE1_Sleep(0)
  AFE_Sleep()
  MCU_RESET()

SleepDeal_HandleBootSleepStartup()
  BootFlag_Read()
  BootFlag_Clear()
  IOstatus_xxxMode()
  InitWakeUp_xxxMode()
  SleepDeal_WaitStopWakeup()
  IORecover_xxxMode()
```

`SleepDeal_IsWakeupValid()` 当前认为以下条件可结束睡眠：

- `CHG_IN` active，标记 `FLASH_SLEEP_CHARGER_WAKE_VALUE`。
- `SW/DI1` 持续按下达到 `DI1_LONG_PRESS_WAKE_10MS`，当前为 50 个 10ms，即约 500ms。

短按只显示 `LedBar_ShowSleepSocPreview()`，显示窗口超时后继续 STOP。

## 当前电源控制

| 对象 | 当前行为 | 判断 |
|---|---|---|
| CAN/CMNT | 运行态 `InitCan()` 打开 `GPIO_CMNT_EN`；睡前 `Can_PrepareSleep()` 取消 TX、清命令队列、停 read-block、关闭 CMNT | MUST_KEEP，功耗收益明确 |
| CAN 周期广播 | 运行态 1000ms/5000ms 广播；RTC STOP 周期唤醒不主动广播；周期广播 TX pending 不再清零 RTC idle，入睡前可由 `Can_PrepareSleep()` 丢弃 | 已确认策略 |
| ADC/TIM2/DMA | STOP 前 `ADC_StopForLowPower()`；唤醒后 `InitADC()` | MUST_KEEP |
| TIM3 | `Sys_StopMode()` 前关闭，唤醒恢复 `InitTimer()` | MUST_KEEP |
| TIM4/LED | LED display active 阻塞低功耗；睡前 `LedBar_PrepareForStop()` 拉低 Charlieplexing 引脚 | MUST_KEEP，但显示窗口策略需确认 |
| AFE | Reset sleep 前执行 `InitAFE1_Sleep(0)` 和 `AFE_Sleep()`；HICCUP STOP 前当前未直接调用 `AFE_Sleep()` | UNKNOWN，需要确认 HICCUP 下 AFE 是否也应进入 sleep |
| DBGMCU | Release 默认清除低功耗调试保持位；调试可显式定义 `__EnableLowPowerDebug__` | 已按确认修复 |
| IWDG | 当前配置宏默认为 1；`Init_IWDG()` 和 `IWDG_Feed()` 已按宏门控；IWDG 开启时 RTC wake period 限制为 10 秒 | 已处理，需构建/长稳验证 |

## BKP 寄存器分配

| 寄存器 | 当前用途 | 代码位置 |
|---|---|---|
| BKP_DR1 | RTC 初始化标记 `RTC_BKP_DATA` | `RTC.c` |
| BKP_DR2 | sleep boot flag | `SleepDeal.c` |
| BKP_DR3 | sleep boot flag 反码 | `SleepDeal.c` |
| BKP_DR4 | LED sleep SOC | `LedBar.c` |
| BKP_DR5 | LED sleep SOC 反码 | `LedBar.c` |
| BKP_DR6 | 工厂老化 magic | `FactoryAging.c` |
| BKP_DR7 | 工厂老化 magic 反码 | `FactoryAging.c` |
| BKP_DR8 | 工厂老化 elapsed 低 16 位 | `FactoryAging.c` |
| BKP_DR9 | 工厂老化 elapsed 高 16 位 | `FactoryAging.c` |
| BKP_DR10 | 工厂老化 CRC | `FactoryAging.c` |
| BKP_DR11 | fault reason | `FaultSnapshot.h` |
| BKP_DR12 | fault reason 反码 | `FaultSnapshot.h` |

2026-07-08 更新：STM32F103C8T6 当前只使用 BKP_DR1..BKP_DR10，并全部保留给 SOC runtime snapshot。Sleep boot flag 改用 `FLASH_ADDR_SLEEP_FLAG`，fault snapshot 改用 RAM runtime mirror。

要求：后续任何低功耗简化都不能复用这些寄存器，也不能让 `BKP_DeInit()` 在非首次 RTC 初始化中清掉这些状态。

## 文档与源码冲突

| 冲突 ID | 现有文档口径 | 当前源码事实 | 风险 | 建议 |
|---|---|---|---|---|
| LP-CONFLICT-001 | Release 应清除 DBGMCU 低功耗调试位 | 已删除 `conf.h` 中无条件 `__EnableLowPowerDebug__`；`EnableLowPowerDebug()` 在未显式定义宏时清除 DBGMCU mask | 调试时若需要 STOP 内 attach，必须显式打开宏，不能用于功耗实测 | 已按确认修复 |
| LP-CONFLICT-002 | `PROJECT_CFG_WDOG_ENABLE` 表示 IWDG 开关 | 当前默认 1，且 `Init_IWDG()` / `IWDG_Feed()` 已按宏门控 | IWDG 开启时 RTC wake 周期最多 10 秒，功耗和稳定性仍需实测取舍 | 已按量产稳定优先处理；后续若拉长 RTC 周期必须重新评估 IWDG |
| LP-CONFLICT-003 | `OtherElement` 中普通休眠/RTC 参数代表低功耗策略 | 当前 `rtc_sleep()` 只使用 `u16Sleep_Vlow/u16Sleep_TimeVlow`，未使用 `u16Sleep_VNormal/u16Sleep_TimeNormal/u16Sleep_RTC_WakeUpTime/u16Sleep_TimeRTC` | 上位机参数与真实行为不一致 | 明确这些参数是保留、删除还是重新接入 |
| LP-CONFLICT-004 | 工厂老化会阻塞 sleep | 已按确认接入：`FactoryAging_IsActive()` 只阻塞 HICCUP idle 进入 RTC STOP，不阻塞低压或外部请求的 `DEEP_MODE/NORMAL_MODE` reset sleep | 若误扩展为阻塞 deep，会影响保护/关机路径 | 保持当前窄范围实现，上板验证老化 running 时不进 HICCUP |
| LP-CONFLICT-005 | AFE not idle 会阻塞 sleep | 当前主 sleep 判断不检查 AFE not idle，框架层未触发的 `LP_BLOCK_AFE_BUSY` 和未使用 AFE block wrapper 已删除 | AFE 状态与低功耗安全边界不清 | 确认 SH367309 哪些状态必须阻塞 HICCUP，再决定接入或删除 RTC reason |

## 需求确认表

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
| LP-REQ-001 | 空闲、无电流、无通信、无 Flash/IAP、无故障、无 LED 显示时，30 秒后进入 HICCUP STOP | `conf.c:5-8`, `rtc_sleep.c:192-197`, `Runtime.c:37` | `sys_time.time_enter_rtc=30`，满足条件后请求 `HICCUP_MODE` | 30 秒是否符合功耗目标和用户体验未确认 | MUST_KEEP | 空闲 30 秒进入 STOP 是否作为当前量产默认？ | 保留 30 秒，后续用实测再调 | |
| LP-REQ-002 | RTC STOP 周期唤醒只做必要恢复和判断，不主动广播 CAN | `Can_HDX.c:820-826`, `RTC.c:420-430`, `rtc_sleep.c:271-276` | 睡前关闭 CMNT，周期唤醒不发 CAN | 休眠中 CAN 不可见 | MUST_KEEP | 是否继续保持休眠中不发 CAN？ | 保留，更省电更简单 | 已确认过，建议继续保留 |
| LP-REQ-003 | 睡前必须保存 SOC snapshot 和老化进度 | `LowPowerSleep.c:5-15`, `SocEnhance.c:1672-1675`, `FactoryAging.c:422-436` | Core state 保存 CAN/SOC/老化；reset sleep 额外保存 LED SOC | 若删除会导致 SOC/老化掉电恢复错误 | MUST_KEEP | SOC 和老化进度是否都必须在睡前保存？ | 保留，后续只统一入口 | |
| LP-REQ-004 | 低压必须能触发 DEEP_MODE，防止过放 | `rtc_sleep.c:139-160` | `VCellMin<=2800mV` 60 秒强制 deep；`VCellMin<=u16Sleep_Vlow` 按参数延时 deep | 低压分支优先于通信/电流阻塞，行为强 | MUST_KEEP | 低压 deep 是否应优先于通信和显示？ | 保留低压优先 | |
| LP-REQ-005 | Reset sleep 唤醒时，短按显示 SOC，长按或充电才真正唤醒运行 | `SleepDeal.c:22-80`, `LedBar.c:1209-1218` | 短按显示保存 SOC，超时继续 STOP；充电或约 500ms 长按唤醒 | 用户交互定义不清可能导致误判 | KEEP_BUT_REFACTOR | 睡眠中短按只看电量、长按开机是否是产品定义？ | 保留，但把时间和条件写入需求 | |
| LP-REQ-006 | 长按运行态按键进入 DEEP_MODE | `LedBar.c:987-999` | 长按约 500ms 后 `SleepDeal_Continue(DEEP_MODE)` | 长按时间短，可能误关机 | UNKNOWN | 运行态长按 500ms 关机是否符合需求？ | 确认产品定义后再改 | |
| LP-REQ-007 | 充电器拔除后进入 DEEP_MODE | `DataDeal.c:51-95` | `CHG_IN` 从 active 变 inactive 后请求 `DEEP_MODE` | 影响拔 5V 后继续运行/待机体验 | UNKNOWN | 拔 5V 后应关机、待机，还是继续运行？ | 需要你确认 | |
| LP-REQ-008 | 工厂老化 active 是否阻塞低功耗 | `FactoryAging.c`, `rtc_sleep.h`, `rtc_sleep.c` | 已接入：老化 running 只阻塞 HICCUP idle 进入 RTC STOP；不阻塞低压或外部请求的 `DEEP_MODE/NORMAL_MODE` reset sleep | 如果误阻塞 deep，会影响保护/关机；如果不阻塞 HICCUP，会打断老化计时 | MUST_KEEP | 是否保持“老化只阻塞 RTC，不影响 deep/reset sleep”？ | 保持当前窄范围实现 | 已确认：只不允许进入 RTC |
| LP-REQ-009 | AFE busy/not idle 是否阻塞低功耗 | `rtc_sleep.c`, `rtc_sleep_afe_sh367309.c`, `rtc_sleep.h` | 当前主判断不检查 AFE not idle；框架层未触发的 AFE busy bit 和未使用 wrapper 已删除 | AFE 异常状态可能进入 STOP | CONFLICT | SH367309 哪些状态必须禁止 HICCUP STOP？ | 确认后恢复最小 RTC block 或删除相关保留 reason | |
| LP-REQ-010 | HICCUP STOP 前是否也让 AFE 进入 sleep | `rtc_sleep_port.c:92-100`, `SleepDeal.c:109-114`, `SH367309_Func.c:65-70` | Reset sleep 调 `AFE_Sleep()`，HICCUP STOP 当前未直接调 | AFE 功耗可能偏高，但休眠周期读 AFE 更简单 | UNKNOWN | HICCUP 期间 AFE 应低功耗测量，还是保持当前可快速读取？ | 结合 SH367309 手册/实测确认 | |
| LP-REQ-011 | `OtherElement` 普通休眠和 RTC 参数是否仍有效 | `DataDeal.h:116-123`, `rtc_sleep.c:152-159`, `RTC.c:369-393` | 当前只用低压阈值/低压时间；RTC wake 固定默认 10 秒 | 上位机参数与真实行为不一致 | CHANGE_NEEDED | `u16Sleep_VNormal/TimeNormal/RTC_WakeUpTime/TimeRTC` 是保留、接入还是删除？ | 没有真实需求则文档化为保留占位，后续删除未使用逻辑 | |
| LP-REQ-012 | Release 是否必须关闭 DBGMCU 低功耗调试保持 | `System_Init.c:21-34`, `docs/review/rtc_sleep_low_power_requirement_confirmation_2026-05-27.md` | 当前 Release 未定义 `__EnableLowPowerDebug__` 时会清除 DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP | 调试 STOP 时需显式打开宏，不能用于功耗实测 | MUST_KEEP | 是否继续保持 Release 关闭低功耗调试保持？ | 保持当前修复 | 已确认 |
| LP-REQ-013 | `PROJECT_CFG_WDOG_ENABLE` 是否必须真实门控 IWDG | `Project_Config.h:57-59`, `AppInit.c:32`, `System_Init.c:37-52` | 当前默认 1；`Init_IWDG()` 和 `IWDG_Feed()` 已按宏门控；RTC wake 安全窗口与宏一致 | IWDG 开启会限制 RTC 周期，影响极低功耗目标 | KEEP_BUT_REFACTOR | 是否接受量产稳定优先、默认启用 IWDG？ | 接受当前处理；后续只在实测功耗不足时重新评估 | 本轮已处理 |
| LP-REQ-014 | sleep 阻塞原因是否需要保留 bitmask 可观测性 | `rtc_sleep.h`, `rtc_sleep.c`, `SystemDebug.c:537-542` | `g_dbg.lp.block_mask` 由 `LP_GetBlockReason()` 现算，`g_stLowPowerRtcStatus.blockReason` 保留粗粒度映射；老化使用粗粒度 `LOW_POWER_RTC_BLOCK_FACTORY_AGING`，不加入通用 bitmask，避免被误解为阻塞所有 sleep | 排查“为什么不睡”仍有详细 bitmask，老化原因看 `blockReason` | KEEP_BUT_REFACTOR | 后续是否允许进一步删除粗粒度未触发 reason？ | 保留当前 bitmask，未确认的 AFE reason 确认后再删或接入 | 本轮已合并到 `rtc_sleep.c/h` |
| LP-REQ-017 | 周期 CAN 广播不能在无 ACK 或总线断开时长期阻止 RTC idle 累计 | `Can_HDX.c`, `CanFeidaoFrames.c`, `rtc_sleep.c` | TX 队列已标记来源；周期广播走 `Can_HDX_TransmitPeriodic()`，`Can_IsBusy()` 只让请求类 TX、命令队列、read-block、未归属硬件发送和 RX 活动阻塞 | 若错误忽略请求类 TX，会丢 ACK；若继续让周期帧阻塞，RTC 难进入 STOP | KEEP_BUT_REFACTOR | 是否保持“周期广播可睡前丢弃，请求/ACK/read-block 必须阻塞”的边界？ | 已按本轮问题执行；需上板验证无设备时可重新进入 RTC | 已按用户“开始”执行 |
| LP-REQ-015 | `LP_EnterStop/LP_BeforeSleep/LP_AfterWakeup/LP_Task` 这组 wrapper 是否保留 | `rg` 仅见旧文档引用，源码主路径不调用 | 已删除这些 wrapper，只保留真实 `Runtime_RunOnce()->rtc_sleep()` 路径 | 主路径更直接；若外部工具依赖旧 API 需同步改工具 | 已执行净删减 | 是否接受后续继续按“无调用、无协议、无硬件行为影响”净删减？ | 接受当前处理，后续继续白名单小批次 | 本轮已处理 |
| LP-REQ-016 | RTC SOC 临时缓存和访问 API 是否保留 | `rg` 仅见旧文档引用，源码无 `get_rtc_soc()/set_rtc_soc()` 消费者 | 已删除 `s_u8RtcSoc`、`get_rtc_soc()`、`set_rtc_soc()`；保留 `SOC_ApplyRtcRelaxationCompensation()` 调用 | 不再保存无消费者返回值，SOC 休眠补偿行为不变 | 已执行净删减 | 是否接受后续删除“只保存返回值但没有消费者”的变量？ | 接受，前提是保留真实副作用 | 本轮已处理 |

## 简化方向

### 第一批：文档和可观测性对齐

1. 修正文档中的 DBGMCU/IWDG/参数口径，让文档明确当前源码事实。
2. 把 BKP 分配表、当前低功耗三路径和待确认项放到权威文档。
3. 在测试计划中增加 DBGMCU、IWDG、老化阻塞、AFE block、sleep 参数有效性测试。
4. 已按已确认需求关闭 Release 低功耗调试保持回归。

### 第二批：低风险净删减

1. 已删除当前无主路径调用的 `LP_EnterStop/LP_BeforeSleep/LP_AfterWakeup/LP_SetWakeupPeriod/LP_Task`。
2. 已删除 `low_power_cancel_rtc()`、`low_power_is_idle_rtc_request()` 这类未调用 helper。
3. 已删除 `app_lowpower.c/h`、框架层未触发的 `LP_BLOCK_AFE_BUSY` 和未使用 AFE/FactoryAging port wrapper；`LOW_POWER_RTC_BLOCK_FACTORY_AGING` 已按确认接入 HICCUP idle 阻塞，`LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` 仍需确认：要么接入真实触发点，要么删除。
4. 已删除无消费者 `s_u8RtcSoc/get_rtc_soc/set_rtc_soc`，把 RTC SOC rest 保留为直接副作用调用。

### 第三批：需求确认后的小步行为修复

1. 已让 DBGMCU 低功耗调试保持只在显式宏下打开，Release 默认关闭。
2. 已按量产稳定优先统一 `PROJECT_CFG_WDOG_ENABLE` 默认值、`Init_IWDG()`、`IWDG_Feed()` 和 `RTC_IsWakeupPeriodSafe()` 语义。
3. 老化期间已按确认阻塞 HICCUP RTC STOP，但不加入 `LP_GetBlockReason()` 通用 bitmask，避免影响 deep/reset sleep 语义。
4. 如果确认 AFE not idle 不能 STOP，则恢复最小 AFE block，但不能在睡前做重型 MTP 操作。
5. 如果确认 `OtherElement` sleep 参数需要有效，则只接入真实用到的参数；若不需要，文档化并逐步删除上位机可写误导。

## 验证边界

本轮做了源码净删减和文档同步，未做以下验证：

- 未编译 Keil `FD_Release`。
- 未烧录 App。
- 未测 STOP 电流。
- 未测 `GPIO_CMNT_EN`、PA3、PB14、PB0 等睡前/唤醒电平。
- 未用 COM4/19200 或 CAN 抓包验证唤醒后通信。

已执行的本机检查：

- `tools/project_check.py --quiet` 上一轮基线为 87 OK / 1 warning / 41 errors。IWDG 默认值已修正，剩余错误需以本轮检查结果为准。

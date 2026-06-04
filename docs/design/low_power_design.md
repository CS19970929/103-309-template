# RTC / 低功耗 / IWDG 设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`rtc_sleep.c`, `rtc_sleep.h`, `rtc_sleep_port.c`, `RTC.c`, `SleepDeal.c`, `LowPowerSleep.c`, `conf.c`, `Can_HDX.c`, `SocEnhance.c`, `LedBar.c`
最后更新时间：2026-06-04
未确认事项：factory aging / AFE not idle 是否阻塞 sleep、`OtherElement` 普通休眠和 RTC 参数是否仍为有效需求。

## 2026-06-02 源码复核补充

- 2026-06-04 简化补充：删除无源码调用的 `LP_GetLastSleepSeconds()` / `LP_RecordLastSleepSeconds()`，HICCUP 退出时直接写 `g_stLowPowerRtcStatus.last`；`lp_idle()` 已合并回低功耗请求更新流程；`SleepDeal_Continue()` 改为先选择 `boot_flag` 再统一保存状态、写 BKP、AFE sleep 和 reset。
- 2026-06-04 命名补充：`IsSleepStartUp()` 改为 `SleepDeal_HandleBootSleepStartup()`；`lp_sync()`、`lp_deep()`、`lp_select()` 分别改为 `lp_refresh_status()`、`lp_select_deep_if_low_voltage()`、`lp_update_sleep_request()`，只改可读性口径，不改变主循环调度位置。
- 2026-06-04 补充：`Can_IsBusy()` 的低功耗语义已从“所有 CAN TX pending 都阻塞”收窄为“CAN App 请求/ACK/read-block、未归属硬件发送、RX 活动阻塞”；普通 1000ms/5000ms 周期广播 pending 不再清零 RTC idle 计数，真正入睡前仍由 `Can_PrepareSleep()` 取消 TX、清队列并关闭 CMNT。
- 当前源码已删除 `conf.h` 中无条件 `__EnableLowPowerDebug__`；`EnableLowPowerDebug()` 在未显式定义该宏时会清除 `DBGMCU_CR_DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP`，符合 Release 功耗实测边界。
- 当前 `PROJECT_CFG_WDOG_ENABLE` 默认为 `1`；`Init_IWDG()` 和 `IWDG_Feed()` 已按该宏门控，RTC wake period 安全窗口与实际 IWDG 行为一致。
- `rtc_sleep()` 只实际使用 `OtherElement.u16Sleep_Vlow` 和 `OtherElement.u16Sleep_TimeVlow`；`u16Sleep_VNormal`、`u16Sleep_TimeNormal`、`u16Sleep_RTC_WakeUpTime`、`u16Sleep_TimeRTC` 当前未进入主判断。
- `RtcSleep_PortIsFactoryAgingActive()`、`RtcSleep_PortIsAfeSleepBlocked()` 这类未使用 wrapper 已删除；FactoryAging active 已按确认只阻塞 HICCUP RTC STOP，不影响 `DEEP_MODE/NORMAL_MODE` reset sleep；AFE not idle 是否阻塞 STOP 仍需确认。
- 新的需求对齐文档见 `docs/review/low_power_requirement_alignment_2026-06-02.md`；官方/行业调研见 `docs/review/low_power_official_industry_research_2026-06-02.md`。

## 2026-05-27 RTC/STOP 修复补充

- `LedBar_IsActiveForLowPower()` 只允许真实显示活动阻塞低功耗：`soc_display_10ms`、`frame_mask`、`scan_timer_enabled`。
- `startup_display_armed` 只表示启动显示窗口已经触发，不能作为持续 active 条件，否则窗口结束后会永久 `LP_BLOCK_LED_ACTIVE`，导致 `rtc_sleep` 无法进入 STOP。
- Release 构建启动时必须清除 `DBGMCU_CR_DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP`，避免调试器残留状态抬高功耗。
- Debug 或临时 ST-Link 长期观察可以打开 DBG_STOP，但该状态只用于定位，不能用于功耗实测。

## 1. 当前低功耗入口

运行态入口：

```text
Runtime_RunIoAndPowerTasks()
  rtc_sleep()
    LP_GetBlockReason()
```

Reset 式 sleep 入口：

```text
SleepDeal_Continue()
  select boot_flag
  LowPowerSleep_SaveResetState()
  BootFlag_Write()
  InitAFE1_Sleep()
  AFE_Sleep()
  MCU_RESET()
```

Reset 后启动早期入口：

```text
AppInit_InitDevice()
  SleepDeal_HandleBootSleepStartup()
    BootFlag_Read()
    IOstatus_xxxMode()
    InitWakeUp_xxxMode()
    SleepDeal_WaitStopWakeup()
    IORecover_xxxMode()
```

## 2. 模式

| 模式 | 行为 |
|---|---|
| `HICCUP_MODE` | RTC Alarm 周期 STOP，醒来后恢复并判断是否继续睡 |
| `NORMAL_MODE` | 写 sleep flag，AFE sleep，MCU reset 后等待合法唤醒 |
| `DEEP_MODE` | 深睡 reset 路径，接充电/长按等唤醒 |
| `NO_SLEEP` | 运行态 |

## 3. 阻塞原因

当前 `LP_GetBlockReason()` 在 `rtc_sleep.c` 内现算，会阻塞 sleep 的条件：

- 充/放电电流 > 10mA。
- SCI/CAN busy。低功耗判断使用会确认 CAN 接收活动的 `Can_IsBusy()`，debug/heartbeat 使用无副作用的 `Can_PeekBusy()`；普通周期广播 TX pending 不作为 RTC idle 阻塞条件，CAN App 请求/ACK/read-block 和 RX 活动仍阻塞。
- MCU_WK/key active。
- Flash busy 或待写参数。
- IAP pending。
- fault active。
- LedBar active。
- 工厂老化 running 只阻塞 HICCUP RTC STOP；低压 deep 和外部 deep/reset sleep 请求不受此条件阻塞。

## 4. RTC 使用

RTC 优先 LSE，失败后 LSI fallback。F1 使用 RTC counter + Alarm 唤醒 STOP。RTC 秒中断更新 `RTC_time`，Alarm 唤醒设置 `is_rtc_wakekup`。

IWDG 开启时，RTC wake period 最大被限制为 10s。

RTC 默认 wake period 为 10s，IWDG 开启时仍限制最大 10s。CAN 不再使用 active/probe 状态参与 RTC 周期判断；RTC STOP 前关闭 CMNT，周期唤醒后不主动广播 CAN，真正唤醒恢复后再由主循环通信。

## 5. IWDG 使用

当前源码事实：

- `Project_Config.h` 中 `PROJECT_CFG_WDOG_ENABLE` 默认为 `1`，符合量产安全门禁。
- `conf.h` 只有在 `PROJECT_CFG_WDOG_ENABLE` 为 1 时才定义 `wdog_enable`，因此 `RTC_GetWakeupPeriodSeconds()` 的 10 秒限制只在 IWDG 启用时生效。
- `Init_IWDG()` 和 `IWDG_Feed()` 均受 `PROJECT_CFG_WDOG_ENABLE` 门控；若调试阶段关闭该宏，IWDG 不启动、不喂狗，RTC wake 安全判断也同步放开。

## 6. 唤醒流程

HICCUP STOP 醒来后：

1. 判断是否 RTC wake。
2. 恢复时钟、IO、ADC、USART、CAN、TIM3、AFE I2C。
3. 如果没有异常唤醒，进行 SOC RTC rest 补偿。
4. 刷新低功耗状态，不主动运行 CAN 周期广播。
5. 如果出现电流/AFE/按键/充电等异常唤醒，则退出 RTC sleep loop，恢复后由运行态 CAN 通信。

## 7. 运行态 idle sleep

`Runtime_RunNormalOnce()` 预留 `PROJECT_CFG_IDLE_SLEEP_ENABLE`。默认 `0`，不改变当前量产运行行为。

打开后，主循环在完成本轮任务后检查：

- 无待处理系统 tick。
- SCI 不 busy。
- CAN 不 busy。
- Flash 不 busy。
- 无 IAP / 参数写入 pending。

条件满足时仅进入 STM32 Sleep (`WFI`)，不进入 STOP，不关闭 CAN/USART/TIM3/EXTI。该模式主要降低运行态 CPU 空转功耗，不能替代 RTC STOP、CAN 收发器 standby、DC/DC/AFE/LED 电源控制。

## 8. 风险和建议

| 风险 | 建议 |
|---|---|
| CAN busy 被打断导致协议半包 | `Can_IsBusy()` 只让请求/ACK/read-block/RX 活动阻塞 STOP；普通周期广播 pending 可在睡前丢弃，debug 和 heartbeat 使用 `Can_PeekBusy()`，不能消费 CAN 接收活动 |
| 周期广播无 ACK 导致难进 RTC | 周期帧通过 `Can_HDX_TransmitPeriodic()` 入队，低功耗 idle 不再被普通周期队列反复清零；入睡时 `Can_PrepareSleep()` 清队列 |
| IWDG 开启后 RTC 周期最多 10 秒 | 当前以稳定优先，量产默认启用 IWDG；若后续为极低功耗拉长 RTC 周期，必须同步评估 IWDG 策略 |
| DBGMCU 低功耗调试保持只能显式打开 | 量产功耗实测必须确认 DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP 为 0；调试 STOP 时再临时打开 |
| fault 全部阻塞可能与过放 deep sleep 冲突 | 按 fault 类型分级确认 |
| LedBar active 阻塞 sleep 影响用户显示和功耗 | 确认显示窗口时长 |
| STM32 idle sleep 打开后可能影响现场调试节奏 | 默认关闭，硬件回归后再决定是否量产打开 |

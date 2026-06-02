# RTC / 低功耗 / IWDG 设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`rtc_sleep.c`, `rtc_sleep.h`, `rtc_sleep_port.c`, `RTC.c`, `SleepDeal.c`, `LowPowerSleep.c`, `conf.c`, `Can_HDX.c`, `SocEnhance.c`, `LedBar.c`
最后更新时间：2026-06-02
未确认事项：IWDG 宏与实际启用是否一致、factory aging / AFE not idle 是否阻塞 sleep、`OtherElement` 普通休眠和 RTC 参数是否仍为有效需求。

## 2026-06-02 源码复核补充

- 当前源码已删除 `conf.h` 中无条件 `__EnableLowPowerDebug__`；`EnableLowPowerDebug()` 在未显式定义该宏时会清除 `DBGMCU_CR_DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP`，符合 Release 功耗实测边界。
- 当前 `PROJECT_CFG_WDOG_ENABLE` 为 `0`，但 `AppInit_InitDevice()` 仍无条件调用 `Init_IWDG()`，`Init_IWDG()` 内部没有用该宏直接门控返回；IWDG 需求与 RTC wake period 安全窗口需要重新对齐。
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
  LowPowerSleep_SaveResetState()
  BootFlag_Write()
  InitAFE1_Sleep()
  AFE_Sleep()
  MCU_RESET()
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
- SCI/CAN busy。
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

- `Project_Config.h` 中 `PROJECT_CFG_WDOG_ENABLE` 当前为 `0`。
- `conf.h` 只有在 `PROJECT_CFG_WDOG_ENABLE` 为 1 时才定义 `wdog_enable`，因此 `RTC_GetWakeupPeriodSeconds()` 的 10 秒限制只在该宏有效时启用。
- `AppInit_InitDevice()` 仍无条件调用 `Init_IWDG()`，`Init_IWDG()` 内部当前没有按 `PROJECT_CFG_WDOG_ENABLE` 直接返回。

待确认需求：

- 如果量产必须启用 IWDG，则 `PROJECT_CFG_WDOG_ENABLE`、`Init_IWDG()`、`RTC_IsWakeupPeriodSafe()` 必须统一。
- 如果调试阶段允许关闭 IWDG，则必须明确写入构建 profile，不允许宏显示关闭但实际仍开启。

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
| CAN busy 被打断导致协议半包 | 保留 `Can_IsBusy()` 阻塞 STOP，确认 TX queue、App 命令和 read-block stream 结束后再睡 |
| IWDG 宏和实际启用不一致 | 先确认量产 IWDG 策略，再统一 `PROJECT_CFG_WDOG_ENABLE`、`Init_IWDG()` 和 RTC wake 安全窗口 |
| DBGMCU 低功耗调试保持只能显式打开 | 量产功耗实测必须确认 DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP 为 0；调试 STOP 时再临时打开 |
| fault 全部阻塞可能与过放 deep sleep 冲突 | 按 fault 类型分级确认 |
| LedBar active 阻塞 sleep 影响用户显示和功耗 | 确认显示窗口时长 |
| STM32 idle sleep 打开后可能影响现场调试节奏 | 默认关闭，硬件回归后再决定是否量产打开 |

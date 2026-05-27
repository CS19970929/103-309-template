# RTC / 低功耗 / IWDG 设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`app_lowpower.c`, `rtc_sleep.c`, `rtc_sleep_port.c`, `RTC.c`, `SleepDeal.c`, `LowPowerSleep.c`, `conf.c`, `Can_HDX.c`, `SocEnhance.c`, `LedBar.c`
最后更新时间：2026-05-27
未确认事项：IWDG 10s 唤醒周期是否满足功耗目标、fault 是否全部阻塞 sleep、`PROJECT_CFG_IDLE_SLEEP_ENABLE` 打开后是否通过硬件实测。

## 2026-05-27 RTC/STOP 修复补充

- `LedBar_IsActiveForLowPower()` 只允许真实显示活动阻塞低功耗：`soc_display_10ms`、`frame_mask`、`scan_timer_enabled`。
- `startup_display_armed` 只表示启动显示窗口已经触发，不能作为持续 active 条件，否则窗口结束后会永久 `LP_BLOCK_LED_ACTIVE`，导致 `rtc_sleep` 无法进入 STOP。
- Release 构建启动时必须清除 `DBGMCU_CR_DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP`，避免调试器残留状态抬高功耗。
- Debug 或临时 ST-Link 长期观察可以打开 DBG_STOP，但该状态只用于定位，不能用于功耗实测。

## 1. 当前低功耗入口

运行态入口：

```text
Runtime_RunIoAndPowerTasks()
  LP_Task()
    LP_BuildBlockReason()
    rtc_sleep()
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

当前 `LP_BuildBlockReason()` 会阻塞 sleep 的条件：

- 充/放电电流 > 10mA。
- SCI/CAN busy 或 CAN bus active。
- MCU_WK/key active。
- AFE 不允许 sleep。
- Flash busy 或待写参数。
- IAP pending。
- fault active。
- LedBar active。
- RTC 周期不满足 IWDG 安全窗口。

## 4. RTC 使用

RTC 优先 LSE，失败后 LSI fallback。F1 使用 RTC counter + Alarm 唤醒 STOP。RTC 秒中断更新 `RTC_time`，Alarm 唤醒设置 `is_rtc_wakekup`。

IWDG 开启时，RTC wake period 最大被限制为 10s。

CAN RTC 唤醒广播周期由 `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS` 配置，默认 `1s`，用于保留当前客户可见的周期广播行为。CAN active 状态由最后一次 TX ACK 或 RX 帧刷新，`PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS` 默认 `10s`；超时后允许低功耗判断不再被历史 CAN active 状态永久阻塞。

## 5. IWDG 使用

IWDG 默认开启：

- 主循环末尾喂狗。
- `__delay_ms()` 中喂狗。
- CAN RTC wake service 等待期间喂狗。
- STOP 前后喂狗。

## 6. 唤醒流程

HICCUP STOP 醒来后：

1. 判断是否 RTC wake。
2. 恢复时钟、IO、ADC、USART、CAN、TIM3、AFE I2C。
3. 如果没有异常唤醒，进行 SOC RTC rest 补偿。
4. 运行 CAN RTC wake service。
5. 如果出现电流/AFE/按键/充电等异常唤醒，则退出 RTC sleep loop。

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
| CAN bus active 可能长期阻塞 sleep | 已改为配置化保持时间，默认 10s，需实测 CAN 在线/离线切换 |
| IWDG 10s 周期导致功耗偏高 | 实测后决定是否调整 IWDG/RTC 策略 |
| fault 全部阻塞可能与过放 deep sleep 冲突 | 按 fault 类型分级确认 |
| LedBar active 阻塞 sleep 影响用户显示和功耗 | 确认显示窗口时长 |
| STM32 idle sleep 打开后可能影响现场调试节奏 | 默认关闭，硬件回归后再决定是否量产打开 |

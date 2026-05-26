# RTC / 低功耗 / IWDG 设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`app_lowpower.c`, `rtc_sleep.c`, `rtc_sleep_port.c`, `RTC.c`, `SleepDeal.c`, `LowPowerSleep.c`, `conf.c`, `Can_HDX.c`, `SocEnhance.c`, `LedBar.c`
最后更新时间：2026-05-26
未确认事项：RTC 休眠中是否必须周期 CAN 广播、IWDG 10s 唤醒周期是否满足功耗目标、fault 是否全部阻塞 sleep。

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

## 7. 风险和建议

| 风险 | 建议 |
|---|---|
| CAN bus active 可能长期阻塞 sleep | 确认低功耗时是否要求 CAN 在线 |
| IWDG 10s 周期导致功耗偏高 | 实测后决定是否调整 IWDG/RTC 策略 |
| fault 全部阻塞可能与过放 deep sleep 冲突 | 按 fault 类型分级确认 |
| LedBar active 阻塞 sleep 影响用户显示和功耗 | 确认显示窗口时长 |

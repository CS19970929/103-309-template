# RTC / 低功耗 / IWDG 设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`app_lowpower.c`, `rtc_sleep.c`, `rtc_sleep_port.c`, `RTC.c`, `SleepDeal.c`, `LowPowerSleep.c`, `conf.c`, `Can_HDX.c`, `SocEnhance.c`, `LedBar.c`
最后更新时间：2026-05-31
未确认事项：reset-sleep 下 UART1/CMNT/MCU_WK 是否应进入正常运行、IWDG 10s 唤醒周期是否满足功耗目标、fault 是否全部阻塞 sleep、CAN RTC 服务是否满足功耗和通信在线目标、`PROJECT_CFG_IDLE_SLEEP_ENABLE` 打开后是否通过硬件实测。

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
- SCI/CAN busy。
- CAN bus active 当前不作为永久 STOP block；它主要影响 RTC wake period 和 CAN RTC wake service 策略。
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

CAN RTC 唤醒广播周期由 `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS` 配置，默认 `1s`，用于保留当前客户可见的周期广播行为。CAN active 状态由最后一次 TX ACK 或 RX 帧刷新，`PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS` 默认 `10s`；当前源码中 CAN busy 会阻塞进入 STOP，CAN active 主要决定 RTC wake 采用 active 周期还是 idle probe 周期，不作为永久 STOP block。

## 4.1 reset-sleep 唤醒源补充

当前 reset-sleep 启动路径中，`conf.c` 会把 UART1 RX、CHG_IN、INT_WK_CMNT、MCU_WK 等配置为唤醒 EXTI，但 `SleepDeal.c` 的合法唤醒判断只接受 charger active 和 key hold。也就是说，UART1/CMNT/MCU_WK 可以唤醒 STOP，但如果没有 charger/key 条件，当前会再次进入 STOP。

该行为必须由产品需求确认后才能修改。当前详细确认表和执行门禁见 `docs/review/low_power_comm_wake_gate_plan_2026-05-31.md`。

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
| CAN bus active 口径容易被误读 | 当前 CAN busy 阻塞 STOP，CAN active 调整 RTC 服务周期；需实测 CAN 在线/离线切换 |
| IWDG 10s 周期导致功耗偏高 | 实测后决定是否调整 IWDG/RTC 策略 |
| fault 全部阻塞可能与过放 deep sleep 冲突 | 按 fault 类型分级确认 |
| LedBar active 阻塞 sleep 影响用户显示和功耗 | 确认显示窗口时长 |
| STM32 idle sleep 打开后可能影响现场调试节奏 | 默认关闭，硬件回归后再决定是否量产打开 |

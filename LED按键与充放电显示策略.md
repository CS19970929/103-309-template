# LED 按键与充放电显示策略

## 行为定义

本说明记录 D010 四灯 SOC LED 在运行态、休眠态和充放电状态下的交互规则，避免后续依赖单次对话记忆。

| 场景 | 行为 | 时序 |
| --- | --- | --- |
| 总开关断开 | 进入深度休眠 | PA9 高电平，LED 开关不允许唤醒 |
| 总开关闭合 | 退出休眠 | PA9 低电平，通过 EXTI9 下降沿记录唤醒源 |
| 开机显示 | 显示当前 SOC | 开机后保持 10 秒 |
| 开机时 socKey 已按住 | 显示当前 SOC，但不允许直接进入休眠 | 必须先松手，再重新长按 3 秒才能休眠 |
| 运行态静置 | LED 长灭 | 总开关闭合、未按键、未充电时不显示 |
| 运行态短按 socKey | 显示当前 SOC | 松手后保持 5 秒 |
| 运行态长按 socKey | 进入深度休眠 | 总开关闭合时，长按 3 秒触发 |
| 休眠态短按 socKey | 显示休眠前保存的 SOC | 总开关闭合时，松手后保持 3 秒，然后回到 STOP |
| 休眠态长按 socKey | 退出休眠并进入正常开机流程 | 总开关闭合时，长按 3 秒触发 |
| 充电态 | 当前 SOC 档位常亮，即将充满的下一档闪烁 | 检测到充电电流 `u16Ichg != 0` 时生效，闪烁周期 200ms |
| 低电量/过放告警 | 第一个 SOC 灯闪烁，其余灯灭 | `SOC < 10` 或单体/总压低压保护位有效时生效，闪烁周期 500ms |
| 放电态 | 默认长灭 | 不做放电动画；短按仍按运行态短按规则显示 SOC |

## 代码入口

- `103 + 309/Project/Source/LedBar.c`
  - `LedBar_ServiceSwitch()`：运行态总开关断开休眠、socKey 短按显示窗口、3 秒长按休眠，以及开机按住需先松手的保护。
  - `LedBar_IsChargeActive()`：充电态判定，仅 `u16Ichg != 0` 时认为充电有效。
  - `LedBar_RunChargeAnimation()`：充电显示当前 SOC 常亮、下一档闪烁，周期由 `LEDBAR_CHG_ANIMATION_PERIOD_10MS` 控制，当前为 20 个 10ms tick。
  - `LedBar_RunLowSocAlarm()`：低 SOC 或过放告警时闪第一个 SOC 灯，周期由 `LEDBAR_LOW_SOC_ALARM_PERIOD_10MS` 控制，当前为 50 个 10ms tick。
  - `APP_LedBar()`：运行态显示仲裁。充电动画优先；非充电且低 SOC/过放时告警闪灯；无显示请求则熄灭。
  - `LedBar_ConfigureLedsOutput()`：只在首次显示或退出低功耗后配置 SOC LED 输出，避免常亮时每帧先灭灯再点亮造成高频闪烁。
- `103 + 309/Project/Source/SleepDeal.c`
  - `IsSleepWakeupValid()`：休眠启动后的 STOP 循环唤醒判定。总开关断开时拒绝 LED 开关唤醒；总开关闭合或 socKey 长按 3 秒才放行开机。
- `103 + 309/Project/Source/conf/conf_gpio.h`
  - `PIN_MAIN_SW = GPIO_Pin_9`：总开关输入，闭合为低电平，使用 EXTI9 下降沿退出休眠。
- `103 + 309/Project/Source/conf/Project_Config.h`
  - `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS = 500`：运行态短按 SOC 显示 5 秒。
  - `PROJECT_CFG_LEDBAR_SLEEP_SOC_DISPLAY_10MS = 300`：休眠态短按 SOC 预览 3 秒。

## 验证要点

1. 未充电、未放电、未按键时，四个 SOC LED 均保持熄灭。
2. 开机后显示当前 SOC 约 10 秒；如果开机时 socKey 一直按住，保持显示但不触发休眠。
3. 开机时 socKey 按住未放开，松手后再重新长按 3 秒才进入休眠。
4. 总开关断开后进入休眠；休眠中只按 socKey 不能开机。
5. 总开关闭合后通过 PA9 下降沿退出休眠。
6. 总开关闭合时，运行态短按 socKey 后显示 SOC，松手后约 5 秒熄灭。
7. 总开关闭合时，运行态长按 socKey 超过 3 秒后进入休眠。
8. 总开关闭合时，休眠态短按 socKey 后显示保存的休眠前 SOC，松手后约 3 秒回到 STOP。
9. 总开关闭合时，休眠态长按 socKey 超过 3 秒后正常开机。
10. 检测到 `u16Ichg != 0` 时，已达到的 SOC 档位常亮，下一档以 200ms 周期闪烁；100% 时四灯常亮。
11. SOC 小于 10 或单体/总压低压保护位有效时，第一个 SOC 灯以约 500ms 周期闪烁报警。
12. SOC 灯常亮时，GPIO 不应出现每帧先关断再点亮的高频窄脉冲。
13. 只有放电电流、无按键请求、无低电量/过放告警时，LED 保持熄灭。

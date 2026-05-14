# LED 按键与充放电显示策略

## 行为定义

本说明记录 D010 四灯 SOC LED 在运行态、休眠态和充放电状态下的交互规则，避免后续依赖单次对话记忆。

| 场景 | 行为 | 时序 |
| --- | --- | --- |
| 总开关断开，未充电 | 进入深度休眠 | PA9 高电平，灯板开关不控制开机 |
| 总开关断开，正在充电 | 保持正常运行，不进入休眠 | `GPIO_CHG_IN` 低电平时生效，不要求 PA9 闭合 |
| 总开关闭合 | 可退出休眠 | PA9 低电平，通过 EXTI9 下降沿记录唤醒源 |
| 开机显示 | 显示当前 SOC | 开机后保持 10 秒 |
| 开机时 socKey 已按住 | 显示当前 SOC | 按住期间保持显示；socKey 不触发休眠 |
| 运行态静置 | LED 长灭 | 总开关闭合、未按键、未充电时不显示 |
| 运行态按下 socKey | 显示当前 SOC | socKey 只作为 SOC 查看键，松手后保持 5 秒 |
| 休眠态短按 socKey | 显示休眠前保存的 SOC | 松手后保持 3 秒，然后回到 STOP |
| 休眠态长按 socKey | 仍只显示 SOC，不开机 | 按住期间持续显示，松手后保持 3 秒并回到 STOP |
| 充电态 | 当前 SOC 档位常亮，即将充满的下一档闪烁 | 仅以 `GPIO_CHG_IN` 低电平判定充电，闪烁周期 200ms |
| 低电量/过放告警 | 第一个 SOC 灯闪烁，其余灯灭 | `SOC < 10` 或单体/总压低压保护位有效时生效，闪烁周期 500ms |
| 放电态 | 默认长灭 | 不做放电动画；短按仍按运行态短按规则显示 SOC |

## 代码入口

- `103 + 309/Project/Source/LedBar.c`
  - `LedBar_ServiceSwitch()`：运行态处理总开关断开休眠、充电保持唤醒、socKey SOC 显示窗口，以及开机按住 socKey 时持续显示。
  - `LedBar_IsChargeActive()`：充电态判定，仅 `GPIO_CHG_IN == Bit_RESET` 时认为充电有效。
  - `LedBar_RunChargeAnimation()`：充电显示当前 SOC 常亮、下一档闪烁，周期由 `LEDBAR_CHG_ANIMATION_PERIOD_10MS` 控制，当前为 20 个 10ms tick。
  - `LedBar_RunLowSocAlarm()`：低 SOC 或过放告警时闪第一个 SOC 灯，周期由 `LEDBAR_LOW_SOC_ALARM_PERIOD_10MS` 控制，当前为 50 个 10ms tick。
  - `APP_LedBar()`：运行态显示仲裁。充电动画优先；非充电且低 SOC/过放时告警闪灯；无显示请求则熄灭。
  - `LedBar_ConfigureLedsOutput()`：只在首次显示或退出低功耗后配置 SOC LED 输出，避免常亮时每帧先灭灯再点亮造成高频窄脉冲。
- `103 + 309/Project/Source/SleepDeal.c`
  - `IsSleepWakeupValid()`：休眠启动后的 STOP 循环唤醒判定。`GPIO_CHG_IN` 有效可直接唤醒；总开关只在 PA9 下降沿唤醒源确认为 `bms_keyirq` 时直接放行；socKey 只显示 SOC 并回到 STOP。
  - `App_SleepDeal()`：`GPIO_CHG_IN` 有效时清除普通和强制休眠请求，避免充电状态下进入休眠。
- `103 + 309/Project/Source/rtc_sleep.c`
  - `low_power_is_charger_input_active()`：低功耗模块统一读取 `GPIO_CHG_IN` 判断充电输入。
  - `LowPower_Request()` / `BQ769x0_SleepMode_Ctrl()` / `isErr_enterRTC()`：`GPIO_CHG_IN` 有效时阻止低功耗休眠请求或 RTC 休眠入口。
- `103 + 309/Project/Source/conf/conf_gpio.h`
  - `PIN_MAIN_SW = GPIO_Pin_9`：总开关输入，闭合为低电平，使用 EXTI9 下降沿退出休眠。
  - `PIN_CHG_IN = GPIO_Pin_0`：充电输入，低电平表示充电插入，可直接唤醒。
- `103 + 309/Project/Source/conf/Project_Config.h`
  - `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS = 500`：运行态短按 SOC 显示 5 秒。
  - `PROJECT_CFG_LEDBAR_SLEEP_SOC_DISPLAY_10MS = 300`：休眠态短按 SOC 预览 3 秒。

## 验证要点

1. 未充电、未放电、未按键时，四个 SOC LED 均保持熄灭。
2. 开机后显示当前 SOC 约 10 秒；如果开机时 socKey 一直按住，按住期间保持显示且不触发休眠。
3. 总开关断开且 `GPIO_CHG_IN` 无效时进入深度休眠。
4. 总开关断开但 `GPIO_CHG_IN` 有效时，保持正常运行，不进入休眠。
5. 休眠中 `GPIO_CHG_IN` 变为低电平时可直接唤醒，不要求总开关闭合。
6. 休眠中只按 socKey 不能开机，但可以显示保存的休眠前 SOC。
7. 休眠态按下 socKey 后显示 SOC，松手后约 3 秒回到 STOP，不退出休眠。
8. 总开关闭合后通过 PA9 下降沿退出休眠。
9. 总开关闭合时，运行态短按或按住 socKey 只显示 SOC，松手后约 5 秒熄灭。
10. `GPIO_CHG_IN` 低电平时，已达到的 SOC 档位常亮，下一档以 200ms 周期闪烁；100% 时四灯常亮。
11. SOC 小于 10 或单体/总压低压保护位有效时，第一个 SOC 灯以约 500ms 周期闪烁报警。
12. SOC 灯常亮时，GPIO 不应出现每帧先关断再点亮的高频窄脉冲。
13. 只有放电电流、无按键请求、无低电量/过放告警时，LED 保持熄灭。

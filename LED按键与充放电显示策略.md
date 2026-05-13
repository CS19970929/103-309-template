# LED 按键与充放电显示策略

## 行为定义

本说明记录 D010 四灯 SOC LED 在运行态、休眠态和充放电状态下的交互规则，避免后续依赖单次对话记忆。

| 场景 | 行为 | 时序 |
| --- | --- | --- |
| 运行态静置 | LED 长灭 | 未按键、未充电时不显示 |
| 运行态短按 socKey | 显示当前 SOC | 松手后保持 5 秒 |
| 运行态长按 socKey | 进入深度休眠 | 长按 3 秒触发 |
| 休眠态短按 socKey | 显示休眠前保存的 SOC | 松手后保持 3 秒，然后回到 STOP |
| 休眠态长按 socKey | 退出休眠并进入正常开机流程 | 长按 3 秒触发 |
| 充电态 | 进入四灯跑马动画 | 检测到充电电流时，25/50/75/100 四灯每 200ms 顺序点亮 |
| 放电态 | 默认长灭 | 不做放电动画；短按仍按运行态短按规则显示 SOC |

## 代码入口

- `103 + 309/Project/Source/LedBar.c`
  - `LedBar_ServiceSwitch()`：运行态 socKey 短按显示窗口和 3 秒长按休眠。
  - `LedBar_IsChargeActive()`：充电态判定，仅 `u16Ichg != 0` 时认为充电有效。
  - `LedBar_RunChargeAnimation()`：充电跑马动画，周期由 `LEDBAR_CHG_ANIMATION_PERIOD_10MS` 控制，当前为 20 个 10ms tick。
  - `APP_LedBar()`：运行态显示仲裁。充电动画优先；非充电时无短按显示请求则熄灭。
- `103 + 309/Project/Source/SleepDeal.c`
  - `IsSleepWakeupValid()`：休眠启动后的 STOP 循环唤醒判定。短按只预览 SOC，长按 3 秒才放行开机。
- `103 + 309/Project/Source/conf/Project_Config.h`
  - `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS = 500`：运行态短按 SOC 显示 5 秒。
  - `PROJECT_CFG_LEDBAR_SLEEP_SOC_DISPLAY_10MS = 300`：休眠态短按 SOC 预览 3 秒。

## 验证要点

1. 未充电、未放电、未按键时，四个 SOC LED 均保持熄灭。
2. 运行态短按 socKey 后显示 SOC，松手后约 5 秒熄灭。
3. 运行态长按 socKey 超过 3 秒后进入休眠。
4. 休眠态短按 socKey 后显示保存的休眠前 SOC，松手后约 3 秒回到 STOP。
5. 休眠态长按 socKey 超过 3 秒后正常开机。
6. 检测到 `u16Ichg != 0` 时，四灯按 25 -> 50 -> 75 -> 100 顺序跑马。
7. 只有放电电流、无按键请求时，LED 保持熄灭。

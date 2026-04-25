# LedBar 开关触发 SOC 显示说明

## 目标行为

- 正常运行时数码管默认熄灭。
- 用户按下开关后立即显示当前 SOC，松开后保持显示 5 秒，然后自动熄灭。
- 正常运行时长按开关 3 秒进入深度休眠流程。
- 休眠保持阶段被开关唤醒后立即显示休眠前缓存的 SOC。
- 休眠唤醒后持续长按 3 秒才放行开机；未达到长按时间则显示 5 秒后关闭数码管并继续 STOP 休眠。

## 实现位置

- `103 + 309/Project/Source/LedBar.c`
  - `APP_LedBar()` 统一处理 DI1 开关和 SOC 显示窗口。
  - `LedBar_SaveSleepSoc()` 将进入休眠前的 SOC 写入 `BKP_DR4/BKP_DR5`。
  - `LedBar_ShowSleepSocPreview()` 在休眠唤醒检查阶段重新打开显示 GPIO 和 TIM4 扫描。
  - `LedBar_PrepareForStop()` 熄灭显示、关闭 TIM4，并把显示相关 GPIO 收回低功耗状态。
- `103 + 309/Project/Source/SleepDeal.c`
  - `IsSleepWakeupValid()` 从单纯长按判断改为“按下先显示，长按才开机，短按回睡”。
  - `SleepDeal_Continue()` 在写睡眠标志前缓存当前 SOC，保证复位后的休眠保持阶段还能立即显示。
- `103 + 309/Project/Source/IO_Control.c`
  - `App_DI1_Switch()` 保留为空兼容入口，避免旧调用点继续处理 DI1。

## 时序

1. 正常运行中 `APP_LedBar()` 每轮读取 DI1 原始电平。
2. 检测到按下沿时，立刻打开 5 秒 SOC 显示窗口。
3. 若持续按下累计到 `LEDBAR_KEY_LONG_PRESS_10MS`，缓存 SOC 并进入深度休眠提交流程。
4. 进入休眠前 `SleepDeal_Continue()` 再次写入 SOC 备份寄存器。
5. 带睡眠标志复位启动时，`IsSleepStartUp()` 进入 STOP 循环。
6. DI1 唤醒 STOP 后，`IsSleepWakeupValid()` 立即调用 `LedBar_ShowSleepSocPreview()` 显示缓存 SOC。
7. 若 3 秒内松开，继续显示到 5 秒窗口结束，然后 `LedBar_PrepareForStop()` 熄灭并回到 STOP。
8. 若保持 3 秒，函数返回有效唤醒，后续 `IORecover_*()` 复位进入正常开机流程。

## 上板确认点

- 正常运行静置时 TIM4 关闭，`PIN_SEG_EN` 关闭，数码管不亮。
- 单击 DI1 后 SOC 立即显示，松开后约 5 秒熄灭。
- 长按 DI1 约 3 秒后进入深度休眠。
- 休眠中短按 DI1 后显示休眠前 SOC，约 5 秒后回睡。
- 休眠中长按 DI1 超过 3 秒后进入正常开机流程。

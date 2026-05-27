# RTC GPIO_SW 唤醒数码管显示

状态：部分验证

日期：2026-05-27

## 需求

RTC STOP 休眠后，如果由 `GPIO_SW / PA9` 中断唤醒，数码管需要立即唤醒并显示 SOC。

## 实现

- `EXTI9_5_IRQHandler()` 在 `EXTI_Line9` 中断中锁存 `soc_key` 唤醒源。
- `rtc_sleep_run_hiccup_cycle()` 在 STOP 退出并恢复外设后，将最终唤醒源交给 port 层处理。
- `RtcSleep_PortOnWakeupSource()` 在唤醒源为 `soc_key` 时调用 `LedBar_RequestSocDisplay()`。
- `LedBar_RequestSocDisplay()` 复用正常模式单击按键显示的同一个 SOC 显示窗口；随后立即运行一次 `APP_LedBar()`，按正常路径退出 LedBar sleep、刷新 SOC、图标和显示超时。

## 参考源码

- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/rtc_sleep_port.h`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/LedBar.h`

## 验证

1. 进入 RTC STOP 休眠。
2. 短按 `GPIO_SW / PA9`，确认能从 STOP 唤醒。
3. 松手较快时，数码管仍应显示 SOC，而不是因为恢复后 GPIO 已释放导致无显示。
4. 显示窗口结束后，确认低功耗策略允许再次进入 RTC STOP。

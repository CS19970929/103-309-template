# D009 CAN/RTC 休眠迁移审查

状态：部分验证

参考源码：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/Can_HDX.h`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/LedBar.h`
- 对比参考分支：`codex/can-upgrade-host`
- 目标分支：`fd_T3Max_D009`

## 审查结论

D009 目标分支已经具备 RTC 休眠后 CAN 短窗口通信的基础接口：`Can_GetIdleRtcPeriodSeconds()`、`Can_RtcWakeService()`、`RTC_GetWakeupPeriodSeconds()`、`RTC_WKTimeConfig()` 和 `rtc_sleep_run_hiccup_cycle()`。因此本次不应整文件覆盖当前分支代码，而应同步当前分支中更稳定的调度策略、可配置参数、发送队列/忙闲判断语义，以及 RTC STOP 前后清 pending、恢复运行态中断的边界处理。

D009 与当前参考分支硬件差异明显。D009 当前 `LedBar` 是 4 路独立 SOC LED，GPIO 为 `PA3/PA2/PA4/PA7`，按键为 `PA6 socKey`；当前参考分支仍包含 5 脚 Charlieplexing/数码管相关逻辑和 `GPIO_MCU_WK` 依赖。这部分不能照搬。

## 已识别当前行为

- D009 CAN 收发器供电脚为 `GPIO_CMNT_EN/PIN_CMNT_EN = GPIOB/GPIO_Pin_4`，低功耗前会置为断电电平。
- D009 `InitCan_GPIO()` 会在 RTC 唤醒后重新配置 `GPIO_CMNT_EN`、CAN RX/TX，因此 CAN 低功耗窗口有恢复 IO 的基础。
- D009 RTC 周期来自 `Can_GetIdleRtcPeriodSeconds()`，当前为有设备 1s、无设备 10s，并受 IWDG 安全窗口裁剪。
- D009 `Can_IsBusy()` 会把周期 pending mask 也视为 busy；参考分支已经改为队列化发送，避免“刚到 1s 周期就被 CAN pending 永久阻塞 RTC 入睡”。
- D009 `RTC.c` 缺少参考分支中的 `RTC_DisableStopWakeup()`、`RTC_RestoreRunInterrupts()` 等集中清理接口，相关清理分散在 `rtc_sleep.c`。
- D009 `CAN_ABOM` 当前为 `DISABLE`，参考分支为 `ENABLE`，需要确认是否同步自动 bus-off 恢复策略。

## 迁移原则

1. 保留 D009 的硬件 IO、普通 SOC LED、socKey、BAT_MASTER 配置和现有 CAN App/老化/IAP 协议。
2. 只迁移 CAN 低功耗调度、RTC wake service、忙闲判断、ACK/No-ACK 活跃判定、bus-off 恢复和 RTC STOP 边界清理。
3. 不迁移当前参考分支的数码管/Charlieplexing/`GPIO_MCU_WK` 显示逻辑。
4. 不修改 CAN ID、帧格式、寄存器地址、IAP 地址、App 烧录地址。

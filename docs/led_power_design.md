# LED/按键/电源重构设计

日期：2026-05-27

## 设计目标

- 不沿用旧 `LedBar_Command` 的业务分支实现。
- LED GPIO 只输出灯态，不直接处理 MOS、AFE_Sleep、SOC 计算。
- 休眠唤醒快显不依赖完整初始化，使用备份域快照。
- 关机动作不在 TIM3 LED 中断里执行 I2C 写 AFE，而是由主循环处理电源请求。

## 模块划分

- `LedBar.c/.h`：LED GPIO 初始化、灯态帧输出、运行态显示状态机。
- `LedSnapshot.c/.h`：备份域 SOC/报警/电源状态快照，使用 `BKP_DR2` 到 `BKP_DR5`。
- `SleepWakeFastUi.c/.h`：STOP 唤醒后的快显服务窗口，处理 8 秒超时、3 秒确认和充电立即唤醒。
- `PowerUi.c/.h`：电源请求层，统一释放 MOS force、关 CHG/DSG、进入 `SleepDeal_Continue()`。
- `SleepDeal.c`：睡眠前保存 LED 快照；STOP 唤醒后先进入快显服务，再决定继续初始化或重新休眠。

## 状态机

运行态 LED 状态：

- `OFF_IDLE`：灯灭，未上电。
- `BOOT_PREVIEW`：按下立即显示 SOC，8 秒超时灭灯。
- `BOOT_ANIM`：L1 到 L5 依次点亮，结束后请求开机。
- `WORK`：按 SOC/报警显示。
- `CHARGE`：已达档位常亮，下一档闪烁。
- `SHUTDOWN_CONFIRM`：L1 灭，L2-L5 闪，8 秒超时恢复工作。
- `SHUTDOWN_ANIM`：L5 到 L2 依次熄灭，结束后请求关机。

休眠快显状态：

- PA9 唤醒后立即初始化 LED GPIO。
- 读取备份域快照，直接显示 SOC。
- 8 秒超时则关灯并重新进入 STOP。
- 长按满 3 秒则播放开机动画并返回完整初始化。
- PA0 充电唤醒直接返回完整初始化。

## 备份域策略

- `BKP_DR1` 保留给原 RTC 初始化标志。
- `BKP_DR2` 保存 LED 快照 magic。
- `BKP_DR3` 保存 SOC 和报警 flags。
- `BKP_DR4` 保存版本和电源状态。
- `BKP_DR5` 保存异或校验。
- `RTC_ClockConfig()` 不再无条件 `BKP_DeInit()`，避免清掉 LED 快照。

## 电源动作

- 开机确认或充电唤醒只设置 `PowerUi_ConfirmPowerOn()`。
- 主循环中的 `PowerUi_ProcessRequests()` 释放 CHG/DSG MOS force。
- 关机确认动画结束后只设置 `PowerUi_RequestShutdown()`。
- 主循环处理关机请求：保存快照、强制关 CHG/DSG、直接写 AFE MOS 关闭、调用 `entersleep(DEEP_MODE)` 和 `SleepDeal_Continue()`，最终进入 AFE_Sleep 并复位。

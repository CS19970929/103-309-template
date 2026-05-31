# App_Can 低功耗优化说明

文档状态：已按源码验证
源码参考：`103 + 309/Project/Source/Can_HDX.c`, `103 + 309/Project/Source/CanFeidaoFrames.h`, `103 + 309/Project/Source/app_lowpower.c`, `103 + 309/Project/Source/rtc_sleep.c`, `103 + 309/Project/Source/RTC.c`
最后更新：2026-05-27

## 目标

打开 `App_Can()` 后，在不修改 CAN ID、payload、App 命令、Modbus 桥接和 IAP 入口的前提下，降低无 CAN 对端或低功耗 RTC 周期中的 CAN 功耗。

## 当前策略

- CAN 收发器电源由 `GPIO_CMNT_EN/PIN_CMNT_EN` 控制，`Bit_RESET` 为上电，`Bit_SET` 为断电。
- `CAN_NART = ENABLE`，关闭 bxCAN 自动重发。无 ACK 时单次发送快速失败，避免硬件持续重发导致功耗升高。
- 有 TX ACK 或 RX 报文时，CAN 进入 active 状态，保持正常 1000 ms / 5000 ms 飞道周期广播。
- 连续发送失败或超时达到 `PROJECT_CFG_CAN_NO_ACK_BACKOFF_THRESHOLD` 后，进入 idle probe 状态，不再发送完整业务广播。
- idle probe 状态只发送 `CAN_FEIDAO_RTC_PROBE_MSG_MASK`，当前为 `CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS` 单帧探测。
- 运行态 idle probe 周期由 `PROJECT_CFG_CAN_PROBE_PERIOD_SECONDS` 控制，默认 10 s。
- RTC 低功耗下，active 总线使用 `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS`，默认 1 s；idle 总线使用 `PROJECT_CFG_CAN_RTC_IDLE_PERIOD_SECONDS`，默认 10 s。
- 发送前先打开 CAN 收发器电源，并等待 `PROJECT_CFG_CAN_POWER_STABLE_TICKS` 个 10 ms tick，默认 2 tick，即 20 ms。
- 无 active 总线且 TX 队列、mailbox、read block 均空闲时，自动关闭 CAN 收发器电源。

## 兼容性边界

本次没有修改：

- 飞道扩展帧 ID：`0x14F80200 | chd_index`
- 周期业务帧 payload 含义
- CAN App 命令帧 `0x60` 和 ACK 帧 `0x61`
- `READ_REG` / `WRITE_COMMIT` 到 Modbus 寄存器的桥接逻辑
- CAN 进入 IAP 的命令语义
- 老化剩余时间广播 `0x14F80208`

## 风险和现场验证

- 无 CAN 对端时，BMS 不再持续发送完整业务广播，只保留 10 s 轻量探测。
- 对端重新接入后，任一探测帧 TX ACK 或 RX 报文会恢复 active 状态和完整周期广播。
- 由于关闭了自动重发，弱总线环境下单帧可能失败；周期广播会在后续周期补发，App 命令建议由上位机保持超时重试。
- 低功耗 active 总线下，系统允许进入 RTC HICCUP STOP；CAN 由 RTC 1 s 唤醒窗口恢复广播。

## 必测项

- 不接 CAN 设备：确认 `GPIO_CMNT_EN` 只在探测窗口上电，功耗下降，且没有持续高频 CAN 重发。
- 接 CAN 设备：确认 `0x14F80200/01/02/03/04/05/08` 周期广播恢复。
- 运行中断开 CAN 设备：确认连续无 ACK 后切换到 10 s idle probe。
- idle probe 中重新接入 CAN 设备：确认探测 ACK 后恢复完整周期广播。
- RTC 低功耗：active 总线 1 s 唤醒广播，idle 总线 10 s 探测。
- CAN App：回归 `GET_STATUS`, `READ_REG`, `READ_BLOCK`, `WRITE_PREP/COMMIT`, `ENTER_IAP`, 老化控制。

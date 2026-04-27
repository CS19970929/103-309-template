# RTC CAN 自适应休眠说明

## 目标

空闲进入 HICCUP RTC 休眠后，根据 CAN 总线是否存在其他设备自动调整 RTC 唤醒周期：

- 检测到 CAN 总线上有其他设备：RTC 休眠 1 秒，唤醒后继续按原 1s / 5s CAN 报文节奏发送。
- 未检测到其他设备：RTC 休眠 10 秒，RTC 周期内不发送 CAN，降低无 ACK 场景下的功耗。

## 总线设备判定

当前没有专用硬件检测“是否接设备”，软件使用 CAN 行为判断：

- 发送报文返回 `CAN_TxStatus_Ok`，说明总线上至少有节点 ACK，认为总线有其他设备。
- CAN RX0 中断收到报文，也认为总线有其他设备。
- 连续出现 ACK 失败或发送超时达到阈值后，认为总线无其他设备。

这个判断不会在无设备状态下主动探测，因此如果总线设备是在 MCU 已进入 10 秒静默 RTC 周期后才接入，需要依赖外部唤醒或重新进入正常运行阶段后重新检测。

## 时序处理

CAN 发送调度保留原来的 10ms tick 状态机，并新增 CAN 内部逻辑 tick：

- 正常运行时，逻辑 tick 跟随 `SysTime_Get10msTickCount()` 推进。
- RTC Stop 期间 TIM3 停止，RTC 唤醒后通过实际 RTC 休眠秒数补偿逻辑 tick。
- 有设备时，RTC 唤醒后调用 CAN RTC 服务窗口，复用原 `feidao_can_send()` 状态机完成上电、稳定等待、发送、发送完成/超时处理。

这样 1000ms 报文和 5000ms 报文不会因为 Stop 期间 TIM3 停止而长期漂移。

## 关键文件

- `103 + 309/Project/Source/Can_HDX.c`
  - 维护 CAN 总线活跃状态。
  - 提供 `Can_GetIdleRtcPeriodSeconds()`，输出 1 秒或 10 秒 RTC 周期。
  - 提供 `Can_RtcWakeService()`，RTC 唤醒后按补偿时间执行 CAN 发送窗口。
- `103 + 309/Project/Source/RTC.c`
  - `RTC_GetWakeupPeriodSeconds()` 改为读取 CAN 给出的自适应周期。
  - `RTC_WKTimeConfig()` 记录本次实际配置的 RTC 周期，供休眠累计使用。
- `103 + 309/Project/Source/rtc_sleep.c`
  - 按每次 RTC Alarm 的实际周期累计休眠秒数。
  - RTC 周期内有设备时执行 CAN 发送，无设备时保持静默。

## 验证建议

1. 不接 CAN 对端：进入 RTC 后应为约 10 秒唤醒一次，`GPIO_CMNT_EN` 不应出现周期性 CAN 发送上电脉冲。
2. 接 CAN 对端并能 ACK：进入 RTC 后应约 1 秒唤醒一次，`GPIO_CMNT_EN` 在发送窗口短时上电，1s / 5s 报文周期保持。
3. 运行中拔掉对端：连续 ACK 失败后切到 10 秒静默周期。
4. 用示波器同时看 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL，确认收发器上电稳定时间和发送窗口满足芯片要求。

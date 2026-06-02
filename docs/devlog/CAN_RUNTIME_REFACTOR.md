# CAN 运行时状态收口说明

文档状态：已按源码验证
最后更新时间：2026-06-02

## 当前模块边界

- `CanFeidaoFrames.c/.h`：只负责飞道扩展帧字段组包、发送顺序和周期报文 mask。
- `Can_HDX.c`：负责 CAN 初始化、TX queue、周期调度、no-ACK 退避、App 命令、read-block stream、IAP 延迟入口和睡前 CMNT 关闭。
- `Can_HDX_Transmit()`：协议帧模块唯一发送出口；返回 `0` 只表示入队成功，硬件 ACK 在后续 `feidao_can_service_tx()` 中判断。

## 当前运行态字段

`Can_HDX.c` 现在按职责保留三组文件级 runtime：

- `s_tx`：TX 环形队列、当前 mailbox、发送起始 tick。
- `s_runtime`：周期调度 tick、bus active、last bus activity、no-ACK、probe 状态。
- `s_app`：CAN App 命令队列、两阶段写寄存器、read-block stream、IAP 延迟。

已删除旧的 RTC 周期 CAN 服务字段和软件 bus-off 状态字段：

- RTC 周期 CAN 服务已删除：不再保留 `rtc_service_active`、RTC wake timeout、RTC wake ACK 和 RTC service 计数。
- bus-off 软件状态机已删除：不再保留 `bus_off`、bus-off enter/recover 计数。
- 收发器电源不再用运行态缓存字段判断，debug 需要时直接读 `GPIO_CMNT_EN` 输出电平。

## 低功耗边界

- `Can_PrepareSleep()`：进入 RTC STOP 或 reset sleep 前清理 CAN 发送/命令状态，并关闭 `GPIO_CMNT_EN`。
- RTC HICCUP 周期唤醒：只做硬件恢复、SOC 休眠补偿和状态刷新，不再主动发送 CAN。
- 正常唤醒后：`InitRunAfterStopWakeup()` 调 `InitCan()`，CAN 重新初始化并打开 CMNT，通信回到主循环运行态。

## bus-off 边界

- `CAN_ABOM = ENABLE` 保留，由 bxCAN 自动完成 bus-off 恢复。
- 软件不再因 BOFF 清队列或统计恢复次数。
- debug snapshot 仍可通过 `CAN1->ESR & CAN_ESR_BOFF` 观察当前硬件 bus-off 位。

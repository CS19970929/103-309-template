# CAN 低功耗发送调度说明

## 背景

`GPIO_CMNT_EN` 用来控制 CAN 收发器供电，当前硬件逻辑为：

- `Bit_RESET`：CAN 收发器上电
- `Bit_SET`：CAN 收发器断电

为了降低 CAN 待机功耗，`App_Can()` 不再使用 `__delay_ms()` 阻塞等待，而是改为 10ms 系统 tick 驱动的非阻塞状态机：有报文到期时上电，等待收发器稳定，发送并等待 mailbox 完成，完成后立即断电。

## 关键策略

1. 上电等待时间由 `FEIDAO_CAN_POWER_STABLE_TICKS` 控制，当前为 `1` 个 10ms tick，即 10ms。
2. 发送完成等待由 `FEIDAO_CAN_TX_DONE_TIMEOUT_TICKS` 控制，当前为 `2` 个 10ms tick，即 20ms 兜底超时。
3. `CAN_NART` 已启用，表示关闭硬件自动重发。这样未接通信设备、没有 ACK 时，bxCAN 不会持续重发同一帧，避免总线无人应答时功耗明显升高。
4. 初始化完成后立即关闭 `GPIO_CMNT_EN`，后续只在发送窗口短时上电。

## 发送周期

发送调度按函数名中的周期执行：

- `feidao_send_volage_current_1000ms()`：1000ms
- `feidao_send_soc_1000ms()`：1000ms
- `feidao_send_cap_5000ms()`：5000ms
- `feidao_send_soh_5000ms()`：5000ms
- `feidao_send_version_5000ms()`：5000ms
- `feidao_send_status_5000ms()`：5000ms
- `feidao_send_factory_time_5000ms()`：5000ms

当多个报文同时到期时，只进行一次 CAN 收发器上电，依次发送到期帧，全部完成后断电。

## 上电延时确认建议

代码无法从软件侧判断具体 CAN 收发器芯片的电源稳定时间，应按实际芯片数据手册确认 `VCC` 上电到 TXD/RXD 可正常工作的时间。如果暂时无法确认，当前 10ms 属于保守默认值。后续实测可按以下方式收敛：

1. 用示波器同时看 `GPIO_CMNT_EN`、收发器 `VCC`、`CAN_TX`、`CANH/CANL`。
2. 从 10ms 开始，逐步调小 `FEIDAO_CAN_POWER_STABLE_TICKS` 对应的等待时间。
3. 以冷启动、低温、低电压条件下第一帧仍能被对端稳定接收为准。

## 注意事项

启用 `CAN_NART` 后，无 ACK 的帧会失败并被丢弃，不会无限重试。这个行为适合低功耗定时报文场景；如果某些业务帧必须保证送达，需要在上层增加重发策略，而不是依赖 bxCAN 硬件无限自动重发。

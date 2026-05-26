# CAN 协议当前实现

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Can_HDX.c`, `103 + 309/Project/Source/Can_HDX.h`, `103 + 309/Project/Source/CanFeidaoFrames.c`, `103 + 309/Project/Source/CanFeidaoFrames.h`, `tools/can_bms_host.py`
最后更新时间：2026-05-26
未确认事项：飞道协议字段单位和客户最终版本仍需用户确认；本文以当前源码发送内容和 App 命令为准。

## 1. CAN 初始化

当前 CAN 入口为 `InitCan()` 和 `App_Can()`。

- 使用 CAN1，PA11/PA12。
- `CAN_ABOM = ENABLE`，允许自动 bus-off 恢复。
- FIFO0 接收中断 `USB_LP_CAN1_RX0_IRQn`。
- 发送侧使用 `FEIDAO_CAN_TX_QUEUE_SIZE = 32` 的软件队列。
- CAN 收发和低功耗状态会更新 debug watch 结构。

## 2. 周期广播

周期广播由 `CanFeidaoFrames.c` 实现，扩展帧基地址为 `0x14F80200`，发送时 `ExtId = 0x14F80200 | chd_index`。

| 周期 | chd_index | 内容 | 源码函数 |
|---|---:|---|---|
| 1000 ms | 0 | 总压、电流 | `CanFeidao_SendVoltageCurrent1000ms()` |
| 1000 ms | 2 | 充电状态、SOC、温度、电池类型 | `CanFeidao_SendSoc1000ms()` |
| 5000 ms | 1 | 当前容量能量、设计容量能量 | `CanFeidao_SendCap5000ms()` |
| 5000 ms | 3 | SOH、循环次数 | `CanFeidao_SendSoh5000ms()` |
| 5000 ms | 4 | 协议版本、软件版本 | `CanFeidao_SendVersion5000ms()` |
| 5000 ms | 5 | MOS/充放电/故障状态和容量 | `CanFeidao_SendStatus5000ms()` |
| 5000 ms | 8 | 出厂老化剩余时间和日期 | `CanFeidao_SendFactoryTime5000ms()` |

`0x14F80208` 对应 `chd_index = 8`，当前用于老化状态广播，包含剩余分钟数。

## 3. App 命令通道

当前 App 命令通道使用标准帧：

| 项 | 当前值 |
|---|---|
| 命令 ID | `0x60` |
| ACK ID | `0x61` |
| 命令 magic | `0xA5 0x5A` |
| ACK magic | `0x5A 0xA5` |
| CRC | `Sci_CRC16RTU(data, 6)`，写入 data[6..7] |

支持命令：

| 命令 | 值 | 当前行为 |
|---|---:|---|
| `GET_STATUS` | `0x01` | 返回 SOC/SOH 百分比 |
| `ENTER_IAP` | `0x02` | 检查 `0xC3 0x3C` 和节点后写 SRAM mailbox，延迟触发 `u8FlashUpdateFlag` |
| `READ_REG` | `0x03` | 通过 `Sci_HostReadWords()` 读取 1 个 Modbus word |
| `WRITE_PREP` | `0x04` | 暂存寄存器地址和值高字节 |
| `WRITE_COMMIT` | `0x05` | 与 prep 地址匹配后通过 `Sci_HostWriteWords()` 写 1 个 word |
| `READ_BLOCK` | `0x06` | 读取最多 120 words，并用 `0x86` 数据帧分包返回 |
| `AGING_START` | `0x07` | guard 通过后调用 `FactoryAging_StartByHost()` |
| `AGING_STOP` | `0x08` | guard 通过后调用 `FactoryAging_StopByHost()` |
| `AGING_RESET_TIME` | `0x09` | guard 通过后调用 `FactoryAging_ResetTimeByHost()` |
| `AGING_SET_HOURS` | `0x0A` | guard 通过并检查小时范围后写老化时长 |

## 4. 低功耗关系

- `Can_PrepareSleep()` 会取消当前发送、清空发送队列、清空 App 命令队列、停止 block stream，并关闭 CAN 收发器电源。
- `Can_RtcWakeService()` 会短时打开 CAN 电源并发送 1000ms 探测帧，过程中持续喂 IWDG。
- 当前低功耗是否保留 CAN 周期广播属于需求确认项，不能只按旧文档判断。

## 5. 兼容风险

1. 周期帧 ID、字段单位和字节序属于客户协议兼容面。
2. App 命令 `READ_REG`/`WRITE_COMMIT` 直接桥接 Modbus 寄存器，任何寄存器权限变更都会影响 CAN 上位机。
3. 老化、写 SOC、IAP 都是外部可见命令，后续必须先回归 `tools/can_bms_host.py` 和 `tools/comm_tool_upgrade_ui.py`。

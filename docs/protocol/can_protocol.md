# CAN 协议当前实现

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Can_HDX.c`, `103 + 309/Project/Source/Can_HDX.h`, `103 + 309/Project/Source/CanFeidaoFrames.c`, `103 + 309/Project/Source/CanFeidaoFrames.h`, `tools/can_bms_host.py`
最后更新时间：2026-06-04
未确认事项：飞道协议字段单位和客户最终版本仍需用户确认；本文以当前源码发送内容和 App 命令为准。

## 1. CAN 初始化

当前 CAN 入口为 `InitCan()` 和 `App_Can()`。

- 使用 CAN1，PA11/PA12。
- `CAN_ABOM = ENABLE`，允许自动 bus-off 恢复。
- FIFO0 接收中断 `USB_LP_CAN1_RX0_IRQn`。
- 发送侧使用 `FEIDAO_CAN_TX_QUEUE_SIZE = 32` 的软件队列；队列项标记来源，周期广播走 `Can_HDX_TransmitPeriodic()`，App ACK/read-block 等请求类帧走 `Can_HDX_Transmit()`。
- CAN 诊断通过 `Can_GetDebugSnapshot()` 填充 debug 结构；bus-off 位从 `CAN1->ESR` 只读获取。
- `Can_HDX.c` 内部状态按职责收口为 `s_tx`、`s_runtime`、`s_app` 三类文件级 `static` runtime。
- App 普通 ACK 和 `READ_BLOCK_DATA` 共用内部 `feidao_can_app_send_frame()` 生成 `0x61` 标准返回帧，保持 `5A A5`、`Data[2..5]` 和 CRC16 格式一致。

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
| `AGING_START` | `0x07` | guard 通过后调用 `FactoryAging_StartByHost()`；完成态下清零累计时间并开启新一轮 |
| `AGING_STOP` | `0x08` | guard 通过后调用 `FactoryAging_StopByHost()`，提前结束本轮老化时间并清零剩余时间 |
| `AGING_RESET_TIME` | `0x09` | guard 通过后调用 `FactoryAging_ResetTimeByHost()` |
| `AGING_SET_HOURS` | `0x0A` | guard 通过并检查小时范围后写老化时长 |

## 4. 低功耗关系

- `InitCan()` / STOP 唤醒恢复会打开 `GPIO_CMNT_EN`，运行态 CAN 供电保持打开。
- `Can_PrepareSleep()` 会取消当前发送、清空发送队列、清空 App 命令队列、停止 block stream，并关闭 CAN 收发器电源。
- RTC HICCUP 周期唤醒后不再主动广播 CAN；唤醒恢复后回到主循环，再按运行态调度通信。
- 运行态不再维护 CAN active/probe/no-ACK 退避状态，固定按 1000ms/5000ms 周期调度飞道广播。
- 低功耗 `Can_IsBusy()` 不再把普通周期广播 TX pending 当作 RTC idle 阻塞条件；CAN App 请求/ACK/read-block、未归属硬件发送和 RX 活动仍会阻塞 STOP。
- `CAN_NART = ENABLE`，无 ACK 时不做硬件自动重发，避免无对端时持续重发导致功耗升高。
- `CAN_ABOM = ENABLE`，bus-off 恢复交给 bxCAN 自动处理，软件不再维护 bus-off 状态机。

当前策略：运行态保持完整周期广播；RTC STOP 中关闭 CMNT，不做周期 CAN 服务；真正唤醒恢复后再通信。

## 5. 兼容风险

1. 周期帧 ID、字段单位和字节序属于客户协议兼容面。
2. App 命令 `READ_REG`/`WRITE_COMMIT` 直接桥接 Modbus 寄存器，任何寄存器权限变更都会影响 CAN 上位机。
3. 老化、写 SOC、IAP 都是外部可见命令，后续必须先回归 `tools/can_bms_host.py` 和 `tools/comm_tool_upgrade_ui.py`。
4. 低功耗优化不得改变外部可见 CAN ID、payload 或 App 命令语义；RTC STOP 中已确认不再周期广播 CAN。

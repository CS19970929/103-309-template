# CAN 协议当前实现

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Can_HDX.c`, `103 + 309/Project/Source/Can_HDX.h`, `103 + 309/Project/Source/CanFeidaoFrames.c`, `103 + 309/Project/Source/CanFeidaoFrames.h`, `tools/can_bms_host.py`
最后更新时间：2026-05-27
未确认事项：飞道协议字段单位和客户最终版本仍需用户确认；本文以当前源码发送内容和 App 命令为准。

## 1. CAN 初始化

当前 CAN 入口为 `InitCan()` 和 `App_Can()`。

- 使用 CAN1，PA11/PA12。
- `CAN_ABOM = ENABLE`，允许自动 bus-off 恢复。
- FIFO0 接收中断 `USB_LP_CAN1_RX0_IRQn`。
- 发送侧使用 `FEIDAO_CAN_TX_QUEUE_SIZE = 32` 的软件队列。
- CAN 收发和低功耗状态会更新 debug watch 结构。
- `Can_HDX.c` 内部状态按职责收口为 `s_tx`、`s_runtime`、`s_app` 三类文件级 `static` runtime；`g_stCanLowPowerStatus` 只作为诊断快照，由运行态刷新，不作为控制真相源。

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

- `Can_PrepareSleep()` 会取消当前发送、清空发送队列、清空 App 命令队列、停止 block stream，并关闭 CAN 收发器电源。
- `Can_RtcWakeService()` 会短时打开 CAN 电源；active 总线发送到期的 1000ms/5000ms 业务帧，idle 总线只发送轻量探测帧，过程中持续喂 IWDG。
- `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS` 配置 active 总线 RTC 唤醒 CAN 广播周期，默认 `1s`，保留客户可见周期广播。
- `PROJECT_CFG_CAN_RTC_IDLE_PERIOD_SECONDS` 配置 idle 总线 RTC 探测周期，默认 `10s`。
- `PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS` 配置 CAN active 保持时间，默认 `10s`。最后一次 TX ACK 或 RX 帧后保持 active，超时后允许低功耗判断继续，不再永久阻塞 STOP。
- `CAN_NART = ENABLE`，无 ACK 时不做硬件自动重发，避免无对端时持续重发导致功耗升高。
- `GPIO_CMNT_EN` 在发送前上电，等待 `PROJECT_CFG_CAN_POWER_STABLE_TICKS` 个 10ms tick；无 active 总线且 TX/read-block 空闲后断电。

当前策略：有 CAN 对端时保持完整周期广播；无对端时切换为 10s 轻量探测，探测 ACK 或 RX 报文会恢复完整广播。

## 5. 兼容风险

1. 周期帧 ID、字段单位和字节序属于客户协议兼容面。
2. App 命令 `READ_REG`/`WRITE_COMMIT` 直接桥接 Modbus 寄存器，任何寄存器权限变更都会影响 CAN 上位机。
3. 老化、写 SOC、IAP 都是外部可见命令，后续必须先回归 `tools/can_bms_host.py` 和 `tools/comm_tool_upgrade_ui.py`。
4. 低功耗优化不得改变默认 `1s` RTC CAN 广播，也不得改变外部可见 CAN ID、payload 或 App 命令语义。

# 通信协议设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`Sci_Upper.c/.h`, `Can_HDX.c/.h`, `CanFeidaoFrames.c/.h`, `ProductionID.c`, `Flash.c`, `FactoryAging.c`
最后更新时间：2026-05-31
未确认事项：Host 写权限、CAN 版本字段来源、老化和 SOC 控制是否长期保留；校准、铜损、RTC、SOC 注入测试写入口是否废弃、占位或恢复。

## 1. 通信架构

当前 BMS App 有两类外部通信：

1. USART1 Modbus RTU 上位机协议。
2. CAN 飞道周期广播 + CAN App 服务协议。

CAN App 服务会复用 Modbus 寄存器读写函数，因此 Modbus 寄存器表仍是核心协议真相源。

## 2. Modbus / UART

| 项目 | 当前行为 |
|---|---|
| 串口 | USART1 PB6/PB7 remap |
| 波特率 | 19200 8N1 |
| Slave | `0x01` |
| 命令 | `0x03`, `0x06`, `0x10` |
| 判帧 | 长度 + IDLE/中断 |
| CRC | Modbus RTU CRC16 |

重要地址：

- `0xD000/0xD100/0xD200/0xD300`：只读状态窗口。
- `0xC002`：SN/HW/SW，各 16 bytes，共 48 寄存器数据来源。
- `0x2100`：保护参数。
- `0x2200`：SOC 表/铜损/RTC，其中 SOC table 写入口当前显式拒绝，铜损/RTC 写入口为空实现。
- `0x2300`：OtherElement、均衡、睡眠、SOC、系统参数。
- `0xFFFD`：进入 IAP / Flash connect。

## 3. CAN 周期广播

`CanFeidaoFrames.c` 发送扩展帧：

| ch | 内容 | 周期 |
|---|---|---|
| 0 | 总压/电流 | 1000ms |
| 1 | 容量 | 5000ms |
| 2 | SOC/温度/电池类型 | 1000ms |
| 3 | SOH/循环次数 | 5000ms |
| 4 | 版本 | 5000ms |
| 5 | 状态/异常/容量 | 5000ms |
| 8 | 工厂老化状态/剩余分钟/日期 | 5000ms |

## 4. CAN App 服务

标准帧命令：

- 命令 ID：`0x60`
- ACK ID：`0x61`
- 请求头：`A5 5A`
- 响应头：`5A A5`
- CRC：前 6 字节 CRC16，高字节在 byte6。

已实现命令：

| 命令 | 功能 |
|---|---|
| `0x01` | GET_STATUS，返回 SOC/SOH |
| `0x02` | ENTER_IAP，设置 SRAM mailbox，延迟复位 |
| `0x03` | READ_REG，桥接 Modbus 单寄存器读 |
| `0x04/0x05` | WRITE_PREP/WRITE_COMMIT，桥接 Modbus 单寄存器写 |
| `0x06` | READ_BLOCK，连续读寄存器并以 `0x86` 分帧返回 |
| `0x07-0x0A` | 老化 start/stop/reset/set hours |

## 5. 协议和业务耦合风险

1. CAN 写寄存器会调用 `Sci_HostWriteWords()`，可能触发 Flash、SOC、AFE 和 IAP 副作用。
2. `PROJECT_CFG_HOST_WRITE_ENABLE 1` 表示量产当前允许写关键参数。
3. `0xC002` 是上位机实时监控底栏依赖，不能随意改字段长度。
4. `0xD300 supported=0` 是量产 SOC 测试隔离约定。
5. 老化剩余时间 UI 依赖 `0x14F80208`。
6. `Sci_ModbusProcessFrame()` 默认正 ACK，空 handler 或 `#if 0` handler 若不显式设置 NEG，会让上位机或 CAN App 误判写入成功；详细门禁见 `docs/review/protocol_write_ack_gate_plan_2026-05-31.md`。

## 6. 后续建议

1. 从 `Sci_Upper.h` 生成 `docs/protocol/modbus_register_map.md`。
2. 从 `Can_HDX.c` 和 `CanFeidaoFrames.c` 生成 `docs/protocol/can_protocol.md`。
3. 给写寄存器建立“副作用表”：Flash、AFE、SOC、IAP、老化。
4. 写权限策略必须先确认再改源码。

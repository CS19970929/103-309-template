# PC 与 Comm Tool 串口协议

日期：2026-05-21

## 1. 物理层

| 项目 | 默认值 |
|---|---|
| 串口 | PC 侧选择 |
| 波特率 | `115200` |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | None |
| 流控 | None |

## 2. 帧格式

所有多字节字段使用小端。

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 1 | SOF0 | 固定 `0xA5` |
| 1 | 1 | SOF1 | 固定 `0x5A` |
| 2 | 1 | version | 第一版固定 `0x01` |
| 3 | 1 | cmd | 命令码 |
| 4 | 2 | seq | PC 递增序号 |
| 6 | 2 | len | payload 字节数 |
| 8 | len | payload | 命令数据 |
| 8 + len | 2 | crc16 | Modbus CRC16，计算范围为 SOF 到 payload |

最大 payload 第一版限制为 512 字节。固件下载使用多帧分包。

## 3. 应答规则

Comm Tool 应答命令码为 `cmd | 0x80`。应答 payload 第一个字节固定为状态码：

| 状态 | 含义 |
|---:|---|
| `0x00` | 成功 |
| `0x01` | CRC 错误 |
| `0x02` | 长度错误 |
| `0x03` | 参数错误 |
| `0x04` | 忙 |
| `0x05` | 超时 |
| `0x06` | Flash 错误 |
| `0x07` | CAN 错误 |
| `0x08` | 固件无效 |
| `0x7F` | 命令不支持 |

PC 必须按 seq 匹配应答。Comm Tool 收到非法帧时可以静默丢弃，但 CRC 错误且长度可解析时建议返回错误应答。

## 4. 命令表

| 命令 | 名称 | 方向 | 说明 |
|---:|---|---|---|
| `0x01` | `GET_INFO` | PC -> Comm Tool | 读取 Comm Tool 版本、Flash 分区、协议版本 |
| `0x02` | `SET_CONFIG` | PC -> Comm Tool | 设置 CAN 波特率、BMS node id |
| `0x10` | `BMS_READ_REGS` | PC -> Comm Tool | 通过 CAN 读取 BMS 寄存器 |
| `0x11` | `BMS_WRITE_REG` | PC -> Comm Tool | 通过 CAN 写 BMS 单寄存器 |
| `0x12` | `BMS_WRITE_REGS` | PC -> Comm Tool | 通过 CAN 写 BMS 多寄存器 |
| `0x20` | `FW_BEGIN` | PC -> Comm Tool | 开始下载 BMS 固件到 Comm Tool |
| `0x21` | `FW_DATA` | PC -> Comm Tool | 固件数据分包 |
| `0x22` | `FW_END_VERIFY` | PC -> Comm Tool | 下载结束并校验 |
| `0x23` | `FW_INFO` | PC -> Comm Tool | 查询 Comm Tool 缓存固件信息 |
| `0x30` | `BMS_UPGRADE_START` | PC -> Comm Tool | 触发 Comm Tool 给 BMS 升级 |
| `0x31` | `BMS_UPGRADE_STATUS` | PC -> Comm Tool | 查询升级进度 |
| `0x32` | `BMS_UPGRADE_ABORT` | PC -> Comm Tool | 中止升级 |

## 5. 固件下载流程

```text
GET_INFO
FW_BEGIN(size, crc16, image_type)
FW_DATA(offset, data)
FW_DATA(offset, data)
...
FW_END_VERIFY(size, crc16)
FW_INFO
```

规则：

1. `FW_BEGIN` 先擦除 Comm Tool 固件缓存区和索引区有效标志。
2. `FW_DATA` 必须按 offset 写入，Comm Tool 需要校验 offset 不能越界。
3. `FW_END_VERIFY` 对缓存区实际长度计算 CRC，通过后写入有效标志。
4. 缓存固件无效时，`BMS_UPGRADE_START` 必须拒绝执行。

## 6. BMS 读写转发

BMS 读写仍使用原串口协议中的寄存器语义：

| 原功能 | 新路径 |
|---|---|
| `0x03` 读寄存器 | PC 串口命令 `BMS_READ_REGS` -> Comm Tool CAN -> BMS |
| `0x06` 写单寄存器 | PC 串口命令 `BMS_WRITE_REG` -> Comm Tool CAN -> BMS |
| `0x10` 写多寄存器 | PC 串口命令 `BMS_WRITE_REGS` -> Comm Tool CAN -> BMS |

PC 工具不重新定义保护参数含义，只复用 BMS 寄存器地址表。

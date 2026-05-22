# BMS App CAN 服务协议

## 作用

BMS App CAN 服务用于 comm tool 在正常 App 运行时读取状态、写保护参数，以及让 App 进入 IAP。它不是升级数据传输协议，真正固件写入由 BMS IAP 完成。

## CAN 帧

第一阶段使用标准帧，保持与当前 App 命令骨架兼容。

| 方向 | 标准帧 ID | 说明 |
| --- | --- | --- |
| comm tool -> BMS App | `(CAN_ADRESS_STD_ID << 7) | 0x60` | App 服务请求 |
| BMS App -> comm tool | `(CAN_ADRESS_STD_ID << 7) | 0x61` | App 服务响应 |

请求固定 8 字节：

| 字节 | 含义 |
| --- | --- |
| 0 | `0xA5` |
| 1 | `0x5A` |
| 2 | 命令 |
| 3~5 | 参数 |
| 6~7 | 前 6 字节 CRC16-Modbus，高字节在前 |

响应固定 8 字节：

| 字节 | 含义 |
| --- | --- |
| 0 | `0x5A` |
| 1 | `0xA5` |
| 2 | 原命令 |
| 3 | 状态码，`0x00` 成功 |
| 4~5 | 返回值 |
| 6~7 | 前 6 字节 CRC16-Modbus，高字节在前 |

## 已实现命令

| 命令 | 参数 | 返回 | 说明 |
| --- | --- | --- | --- |
| `0x01 GET_STATUS` | `00 00 00` | `SOC, SOH` | 读取基础状态 |
| `0x02 ENTER_IAP` | `C3 3C can_addr` | `08 48` | 写升级标志并延时复位进入 IAP |

## 后续扩展命令

寄存器读写扩展不得重新定义保护参数含义，必须复用 `COMMUNICATION_ADDRESS_INDEX.md` 和 `Sci_Upper.c` 的地址、范围检查、副作用处理。

| 命令 | payload 规划 | 说明 |
| --- | --- | --- |
| `0x10 READ_REGS` | `addr:u16 count:u8` | 读取 BMS 寄存器 |
| `0x11 WRITE_REGS` | `addr:u16 count:u8 value0:u16...` | 写 BMS 寄存器 |

## 接收窗口

现有 BMS CAN 会对收发器做低功耗开关。App 服务必须在 BMS 周期广播后的短窗口内接收命令；第一阶段保留短接收窗口，避免长期打开 CAN 收发器。


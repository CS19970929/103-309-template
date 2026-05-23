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

块读数据帧也使用 BMS App 响应 ID，但字节 2 固定为 `0x86`，字节 3 为序号，字节 4~5 为寄存器值：

| 字节 | 含义 |
| --- | --- |
| 0 | `0x5A` |
| 1 | `0xA5` |
| 2 | `0x86` |
| 3 | 从 `0` 开始的寄存器序号 |
| 4~5 | 寄存器值，高字节在前 |
| 6~7 | 前 6 字节 CRC16-Modbus，高字节在前 |

## 已实现命令

| 命令 | 参数 | 返回 | 说明 |
| --- | --- | --- | --- |
| `0x01 GET_STATUS` | `00 00 00` | `SOC, SOH` | 读取基础状态，兼容早期 comm tool |
| `0x02 ENTER_IAP` | `C3 3C can_addr` | `08 48` | 写升级标志并延时复位进入 IAP |
| `0x03 READ_REG` | `addr_hi addr_lo 00` | `value_hi value_lo` | 读取一个 BMS 串口寄存器 |
| `0x04 WRITE_PREP` | `addr_hi addr_lo value_hi` | `addr_hi addr_lo` | 写单个寄存器第一帧，暂存地址和高字节 |
| `0x05 WRITE_COMMIT` | `addr_hi addr_lo value_lo` | `00 00` | 写单个寄存器第二帧，校验地址一致后提交 |
| `0x06 READ_BLOCK` | `addr_hi addr_lo count` | `count, 00` 后续 `0x86` 数据帧 | 一次读取 `1..120` 个连续寄存器，供 UI 状态页和实时监控使用 |

## 状态码

| 状态码 | 含义 |
| --- | --- |
| `0x00` | 成功 |
| `0x01` | 命令不支持 |
| `0x02` | 参数错误或寄存器地址非法 |
| `0x05` | App 进入 IAP 请求失败 |
| `0x07` | 无写权限，通常是量产固件 `PROJECT_CFG_HOST_WRITE_ENABLE=0` |
| `0x08` | BMS 寄存器处理失败 |

## 寄存器读写规则

- CAN App 服务不重新定义寄存器地址，统一复用 `Sci_Upper.c` 原有串口寄存器表。
- 读寄存器调用 `Sci_HostReadWords()`，使用原 `0x03` 读寄存器校验和返回数据生成逻辑。
- comm tool 读取多个寄存器时优先使用 `READ_BLOCK`。BMS App 收到块读后只调用一次 `Sci_HostReadWords()`，再每 10ms 发送一个 `0x86` 数据帧，避免升级后 App 刚恢复时被大量单寄存器 CAN 请求打满。
- 写寄存器调用 `Sci_HostWriteWords()`，使用原 `0x06/0x10` 写寄存器权限、范围、副作用处理。
- CAN 接收中断只缓存请求帧；真正寄存器处理在 `App_Can()` 主循环中执行，避免在中断里操作 Flash/EEPROM 或串口缓存。
- 如果任一串口通道正在收发，`Sci_HostReadWords()` / `Sci_HostWriteWords()` 会拒绝本次 CAN 寄存器访问，避免和原串口响应缓存冲突。

## 接收窗口

现有 BMS CAN 会对收发器做低功耗开关。App 服务必须在 BMS 周期广播后的短窗口内接收命令；第一阶段保留短接收窗口，避免长期打开 CAN 收发器。

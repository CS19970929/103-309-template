# PC 与 comm tool 串口协议

## 串口参数

默认参数：`115200 8N1`，无流控。PC 端固定使用 Windows Python Launcher：

```powershell
py -3.9 tools\comm_tool_host.py ...
```

## 帧格式

所有多字节字段为小端。

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| magic | 2 | 固定 `55 AA` |
| version | 1 | 当前 `01` |
| flags | 1 | bit0 为 ACK 帧 |
| seq | 2 | PC 发起递增序号，响应沿用请求序号 |
| cmd | 1 | 命令号 |
| status | 1 | 请求固定 `00`，响应为状态码 |
| length | 2 | payload 字节数 |
| payload | N | 命令数据 |
| crc16 | 2 | CRC16-Modbus，覆盖 crc 前所有字段 |

最大 payload 第一阶段限制为 `512` 字节。固件下载分块推荐 `256` 字节。

## 状态码

| 状态码 | 含义 |
| --- | --- |
| `0x00` | 成功 |
| `0x01` | CRC 错 |
| `0x02` | 命令不支持 |
| `0x03` | 参数错误 |
| `0x04` | 状态不允许 |
| `0x05` | Flash 错误 |
| `0x06` | CAN 超时 |
| `0x07` | BMS 返回错误，常见原因是 BMS App 未响应、地址非法、写权限关闭或参数越界 |

`UPGRADE_STATUS.last_error` 为 comm tool 内部阶段码，常用值：

| 阶段码 | 含义 |
| --- | --- |
| `0x02` | IAP `HELLO` 未收到 ACK |
| `0x03` | IAP `START` 未收到 ACK 或被拒绝 |
| `0x06` | IAP `COMMIT` 未收到 ACK 或被拒绝 |
| `0x07` | IAP `END` 未收到 ACK 或被拒绝 |
| `0x21` | 一键升级自动发送 App `ENTER_IAP` 未收到 ACK |

## 命令

| 命令 | 方向 | payload | 说明 |
| --- | --- | --- | --- |
| `0x01 GET_INFO` | PC -> comm | 空 | 读取 comm tool 版本、Flash 缓存能力、CAN 波特率 |
| `0x02 SET_CAN` | PC -> comm | `bitrate:u32 node:u8 reserved[3]` | 设置 CAN 参数 |
| `0x10 BMS_READ` | PC -> comm | `addr:u16 count:u16` | 通过 CAN 读取 BMS 寄存器 |
| `0x11 BMS_WRITE` | PC -> comm | `addr:u16 count:u16 words[count]` | 通过 CAN 写 BMS 寄存器 |
| `0x20 FW_BEGIN` | PC -> comm | `app_addr:u32 size:u32 crc16:u16 crc32:u32` | 开始下载 BMS App 到 comm tool |
| `0x21 FW_DATA` | PC -> comm | `offset:u32 data[n]` | 写入固件缓存 |
| `0x22 FW_END` | PC -> comm | `size:u32 crc16:u16 crc32:u32` | 结束下载并校验缓存 |
| `0x23 FW_INFO` | PC -> comm | 空 | 查询缓存固件信息 |
| `0x30 ENTER_IAP` | PC -> comm | 空 | 让 BMS App 写标志并复位进入 IAP |
| `0x31 UPGRADE` | PC -> comm | 空 | 使用缓存固件给 BMS 一键升级 |
| `0x32 UPGRADE_STATUS` | PC -> comm | 空 | 查询升级状态 |
| `0x33 UPGRADE_ABORT` | PC -> comm | 空 | 终止当前升级 |
| `0x40 RAW_CAN_TX` | PC -> comm | `id:u32 ide:u8 dlc:u8 data[8]` | 调试用原始 CAN 发送 |
| `0x41 CAN_DIAG` | PC -> comm | `clear:u8` | 读取 comm tool CAN 发送、接收和错误寄存器诊断；`clear!=0` 表示读取前清零 |

### BMS_READ / BMS_WRITE 细节

- `BMS_READ` 读取的是 BMS 原串口寄存器地址空间，不重新定义参数含义。
- `BMS_READ` 响应 payload 为 `words[count]`，每个寄存器 16 位小端。
- `BMS_WRITE` 请求 payload 为 `addr:u16 count:u16 words[count]`，每个寄存器 16 位小端。
- comm tool 当前一次最多转发 `120` 个寄存器；内部通过 CAN App 服务逐字读写。
- BMS App 写入仍走 `Sci_Upper.c` 的地址、范围、副作用和权限检查。量产默认 `PROJECT_CFG_HOST_WRITE_ENABLE=0` 时会拒绝写入，这是预期保护。
- UI 的 `读取BMS信息` 使用 `0xD000` 起连续 `63` 个只读寄存器；`读取BMS状态` 使用 `0xD034/0xD035` 读取 SOC/SOH。

## 安全规则

- `FW_BEGIN.app_addr` 第一阶段必须等于 `0x08004800`。
- PC 工具真实下载必须显式传入 `-ConfirmAppAddress 0x08004800`。
- comm tool 写缓存前必须擦除缓存页，不能覆盖自身程序区。
- `FW_END` 校验失败时缓存固件无效，禁止执行 `UPGRADE`。

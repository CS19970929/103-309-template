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

最大 payload 第一阶段限制为 `512` 字节。固件下载 `FW_DATA` 默认数据分块为 `496` 字节，实际 payload 为 `offset:u32 + data[496]`，低于协议上限。

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
| `0x07` | BMS 返回错误，常见原因是 BMS App 未响应、地址非法、参数越界或写权限关闭 |

## 设备目标

comm tool 默认目标为：CAN 波特率 `250000`、BMS App CAN 地址 `0`、IAP 节点 `1`。comm tool 自身 App 的 CAN 服务地址固定为 `14`，用于另一台 comm tool 触发它进入自身 IAP。

多设备总线中，读写和进入 IAP 先按 BMS App CAN 地址选择目标；只有被选中的 BMS App 会响应 `ENTER_IAP` 并复位进入 IAP。BMS 进入 IAP 后使用 IAP 节点帧升级。当前 IAP 默认节点为 `1`，因此如果总线上已有多个 BMS 同时停留在 IAP 且节点相同，comm tool 无法区分它们，禁止直接升级。

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
| `0x01 GET_INFO` | PC -> comm | 空 | 读取 comm tool 版本、Flash 缓存能力、CAN 波特率、目标节点 |
| `0x02 SET_CAN` | PC -> comm | `bitrate:u32 node:u8 app_can_addr:u8 reserved:u16` | 设置 CAN 参数、IAP 节点和 BMS App CAN 地址 |
| `0x10 BMS_READ` | PC -> comm | `addr:u16 count:u16` | 通过 CAN 读取 BMS 寄存器 |
| `0x11 BMS_WRITE` | PC -> comm | `addr:u16 count:u16 words[count]` | 通过 CAN 写 BMS 寄存器 |
| `0x20 FW_BEGIN` | PC -> comm | `app_addr:u32 size:u32 crc16:u16 crc32:u32` | 开始下载 BMS App 或 comm tool App 到 comm tool 缓存 |
| `0x21 FW_DATA` | PC -> comm | `offset:u32 data[n]` | 写入固件缓存 |
| `0x22 FW_END` | PC -> comm | `size:u32 crc16:u16 crc32:u32` | 结束下载并校验缓存 |
| `0x23 FW_INFO` | PC -> comm | 空 | 查询缓存固件信息 |
| `0x30 ENTER_IAP` | PC -> comm | 空 | 让 BMS App 写标志并复位进入 IAP |
| `0x31 UPGRADE` | PC -> comm | 空 | 启动使用缓存固件给 BMS 一键升级，命令返回后通过 `UPGRADE_STATUS` 查询进度 |
| `0x32 UPGRADE_STATUS` | PC -> comm | 空 | 查询升级状态 |
| `0x33 UPGRADE_ABORT` | PC -> comm | 空 | 终止当前升级 |
| `0x40 RAW_CAN_TX` | PC -> comm | `id:u32 ide:u8 dlc:u8 data[8]` | 调试用原始 CAN 发送 |
| `0x41 CAN_DIAG` | PC -> comm | `clear:u8` | 读取 comm tool CAN 发送、接收和错误寄存器诊断；`clear!=0` 表示读取前清零 |

### GET_INFO / CAN_DIAG 扩展字段

`GET_INFO` 前 20 字节保持兼容：

| offset | 长度 | 含义 |
| --- | --- | --- |
| 0 | 4 | `proto, major, minor, patch` |
| 4 | 4 | CAN 波特率 |
| 8 | 4 | 固件缓存起始地址 |
| 12 | 4 | 固件缓存大小 |
| 16 | 4 | flags |
| 20 | 1 | 当前 IAP 节点 |
| 21 | 1 | 当前 BMS App CAN 地址 |

`CAN_DIAG` 第 61 字节为 IAP 节点，第 62 字节为 BMS App CAN 地址。旧工具只解析前 62 字节时仍可工作。

### BMS_READ / BMS_WRITE 细节

- `BMS_READ` 读取的是 BMS 原串口寄存器地址空间，不重新定义参数含义。
- `BMS_READ` 响应 payload 为 `words[count]`，每个寄存器 16 位小端。
- `BMS_WRITE` 请求 payload 为 `addr:u16 count:u16 words[count]`，每个寄存器 16 位小端。
- comm tool 当前一次最多转发 `120` 个寄存器；读多个寄存器时内部使用 CAN App `READ_BLOCK` 块读，BMS App 每 10ms 返回一个寄存器数据帧。
- BMS App 写入仍走 `Sci_Upper.c` 的地址、范围、副作用和权限检查。当前 Release 默认 `PROJECT_CFG_HOST_WRITE_ENABLE=1`，允许 UI 写保护参数和其它参数。
- UI 写 SH309 AFE 保护参数时不会逐个地址直接覆盖。`0x2400..0x2417` 会先读取整块、合并修改项、一次写回 24 个寄存器并回读校验；`0x2132..0x2136` MOS 过温参数同理整块写入。二级过流和短路电流列表由 BMS 当前 `0x231D` 采样电阻、`0x231E` 采样电阻数换算生成。
- UI 的 `读取BMS信息` 和 `实时监控` 使用 `0xD000` 起连续 `63` 个只读寄存器；`读取BMS状态` 使用 `0xD034/0xD035` 读取 SOC/SOH；`读取BMS日志` 使用 `0xC008` 完整事件记录窗口，数量固定为 `100` 个寄存器。`0xC008` 是旧串口协议特殊窗口，不能按 `0xC008 + offset` 拆分读取；UI 对完整窗口读取最多重试 3 次。

## 安全规则

- 下载 BMS App 时 `FW_BEGIN.app_addr` 必须等于 `0x08004800`，PC 工具真实下载必须显式传入 `-ConfirmAppAddress 0x08004800`。
- 下载 comm tool App 给另一台 comm tool 自升级时，`FW_BEGIN.app_addr` 必须等于 `0x08008000`，PC 工具真实下载必须显式传入 `-ConfirmAppAddress 0x08008000`。
- comm tool App 不能裸写到 `0x08000000`；该地址是 comm tool 自身 IAP。
- comm tool 写缓存前必须擦除缓存页，不能覆盖自身程序区。
- `FW_END` 校验失败时缓存固件无效，禁止执行 `UPGRADE`。
- 多个 BMS 同时挂在 CAN 总线时，必须保证目标 BMS App CAN 地址唯一；多个相同地址或多个相同 IAP 节点同时在线会造成响应串扰。

## comm tool 自升级兼容协议

comm tool IAP 同时支持旧 BMS 串口升级协议和当前 CAN-IAP 协议：

- 串口：USART3 `115200 8N1`，旧 BMS `0x10` 写多个寄存器命令，地址仍为 `0xFFFD` 连接、`0xFFFE` 数据块、`0xFFFF` 完成。App 运行时收到 `0xFFFD` 后会用 SRAM mailbox 请求复位进入 IAP，并先 ACK 再复位，旧串口上位机可复用。
- CAN：目标 comm tool App 地址固定 `14`，收到 App 服务 `ENTER_IAP` 后复位进入 IAP；IAP 使用扩展帧 `0x14F8F000/0x14F8F100/0x14000000` 的 CAN-IAP 协议，节点默认 `1`。

另一台 comm tool 升级当前 comm tool 时，PC 先把 `COMM_TOOL_Release.bin` 以 `app_addr=0x08008000` 下载到主控 comm tool 缓存，然后设置 `app-can-addr=14`、`node-id=1`，执行 `enter-iap` 和 `upgrade`。目标 IAP 收到完整镜像并校验后才写入 App 首页向量，升级中断会停留在 IAP。

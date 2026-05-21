# Comm Tool 与 BMS App CAN 网关协议

日期：2026-05-21

## 1. 目标

CAN 网关协议只服务 BMS App 正常运行状态下的读写操作，不承担 Bootloader 固件传输。Bootloader 升级使用独立 CAN-IAP 协议。

## 2. 默认参数

| 项目 | 默认值 |
|---|---|
| CAN | CAN1 |
| 波特率 | `250 kbit/s` |
| 帧格式 | 扩展帧 |
| 默认 BMS node id | `1` |

## 3. CAN ID

| 方向 | 扩展帧 ID | 用途 |
|---|---:|---|
| Comm Tool -> BMS App | `0x18DA0000 | node_id` | 请求帧 |
| BMS App -> Comm Tool | `0x18DB0000 | node_id` | 应答帧 |
| BMS App -> Comm Tool | `0x18DC0000 | node_id` | 周期状态广播，可选 |

## 4. 请求帧

单个 CAN payload 为 8 字节。

| 字节 | 含义 |
|---:|---|
| 0 | op：`0x03` 读，`0x06` 写单寄存器，`0x10` 写多寄存器，`0x55` 进入 Bootloader |
| 1 | seq |
| 2 | address high |
| 3 | address low |
| 4 | count/value high |
| 5 | count/value low |
| 6 | data 或保留 |
| 7 | CRC8 或保留，第一版保留 |

多寄存器写入数据超过一帧时，使用连续数据帧：

| 字节 | 含义 |
|---:|---|
| 0 | `0x11` |
| 1 | seq |
| 2 | chunk_index |
| 3 | chunk_len |
| 4..7 | data |

## 5. 应答帧

| 字节 | 含义 |
|---:|---|
| 0 | op echo |
| 1 | seq echo |
| 2 | status |
| 3 | data_len |
| 4..7 | data 或错误码 |

状态码：

| 状态 | 含义 |
|---:|---|
| `0x00` | 成功 |
| `0x01` | 地址非法 |
| `0x02` | 长度非法 |
| `0x03` | 写入被拒绝 |
| `0x04` | 忙 |
| `0x05` | 内部错误 |
| `0x06` | 即将复位进入 Bootloader |

## 6. 寄存器语义

BMS App 的寄存器地址和读写含义必须继承旧串口协议。PC 工具、Comm Tool 和 BMS App 都不能各自维护一套不同参数定义。

第一版优先覆盖：

1. `0xD000` 主状态区。
2. `0xD100` 系统状态区。
3. `0xD300` 新版能力查询区。
4. `0x1000 - 0x11FF` 控制命令。
5. `0x2000` 起保护参数写入区。

## 7. 进入 Bootloader

Comm Tool 向 BMS App 发送 `op=0x55`。BMS App 校验 magic、node id、当前运行状态后：

1. 关闭充放电 MOS 或进入安全状态。
2. 写入 Bootloader 请求标志。
3. 应答 `status=0x06`。
4. 延时 100 ms 到 300 ms 后复位。

Comm Tool 收到应答后切换到 CAN-IAP 握手流程。

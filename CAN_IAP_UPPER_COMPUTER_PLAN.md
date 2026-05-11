# CAN-IAP 升级与电池上位机实施方案

## 1. 当前结论

当前可见 Keil Target 的 scatter 文件为：

`103 + 309/Project/Users/Objects/FD_Release.sct`

其中 IROM 起始地址是 `0x08004800`，因此该 Target 是 App 工程，不是从 `0x08000000` 启动的纯 IAP/Bootloader 工程。当前 App 已具备：

- CAN 周期广播电池状态，扩展帧 ID 形如 `0x14F80200 | chd_index`。
- RS485/Modbus 读取电池信息、修改保护参数、写升级标志。
- 写入 `FLASH_ADDR_UPDATE_FLAG = 0x0801F800` 后复位并跳转 IAP 的路径。

CAN 固件升级要可靠落地，最终必须在真正 IAP/Bootloader 中实现 CAN 接收、擦除 App 区、写入 App 区和校验跳转。当前 App 工程只能负责“收到升级请求后进入 IAP”，不能替代 IAP 完成升级。

## 2. 是否必须有 CAN 盒

PC 上位机真实连接电池板时，必须有一个 CAN 物理接口。这个接口可以是：

1. USB-CAN 盒，例如 PCAN、Kvaser、周立功、CANable。
2. 自研专用 MCU 升级器，例如 USB/串口转 CAN，内部由 MCU 发送 CAN 帧。
3. 其他带 CAN 的工装或网关。

开发阶段推荐 USB-CAN 盒。原因是它能直接配合 Python `python-can`、CAN 调试工具和抓包软件，协议问题容易定位。专用 MCU 升级器适合后期售后或量产一键升级，等 PC 方案和 CAN-IAP 协议稳定后再做。

## 3. 推荐分阶段路线

### 第一阶段：PC + USB-CAN 盒上位机

目标：

- 监听并解析现有 App 的 CAN 电池广播。
- 对待升级 `.bin` 做安全检查和分包 dry-run。
- 固化 CAN-IAP 协议格式，方便后续 IAP 固件按同一协议实现。

本阶段不擦写板端 Flash，不修改 IAP 代码。

### 第二阶段：App 进入 IAP 命令

目标：

- 在 App CAN 协议中增加“进入 IAP”命令。
- App 收到合法命令后关闭充放电 MOS，写 `FLASH_TO_IAP_VALUE` 到 `0x0801F800`，复位。
- 保留当前 RS485 `0xFFFD` 进入 IAP 路径。

这一步仍然只负责跳转，不负责升级写 Flash。

当前分支已增加 App 侧标准帧命令骨架：

| 方向 | 标准帧 ID | 说明 |
| --- | --- | --- |
| 上位机 -> App | `(CAN_ADRESS_STD_ID << 7) | 0x60`，当前为 `0x060` | App 命令 |
| App -> 上位机 | `(CAN_ADRESS_STD_ID << 7) | 0x61`，当前为 `0x061` | App ACK |

App 命令 8 字节固定格式：

| 字节 | 含义 |
| --- | --- |
| 0 | `0xA5` |
| 1 | `0x5A` |
| 2 | 命令 |
| 3~5 | 参数 |
| 6~7 | 前 6 字节的 Modbus CRC16，高字节在前 |

App ACK 8 字节固定格式：

| 字节 | 含义 |
| --- | --- |
| 0 | `0x5A` |
| 1 | `0xA5` |
| 2 | 原命令 |
| 3 | 状态码，`0x00` 为成功 |
| 4~5 | 返回值 |
| 6~7 | 前 6 字节的 Modbus CRC16，高字节在前 |

已定义命令：

| 命令 | 参数 | 返回值 | 说明 |
| --- | --- | --- | --- |
| `0x01` | `00 00 00` | `SOC, SOH` | 读取基础状态 |
| `0x02` | `C3 3C CAN_ADRESS_STD_ID` | `08 48` | 写升级标志并延迟约 200ms 复位进入 IAP |

### 第三阶段：IAP 实现 CAN 升级

目标：

- IAP 上电后判断升级标志或等待 CAN 握手。
- CAN 收到升级开始命令后，只擦除 `0x08004800` 之后的 App 区。
- 禁止擦写 `0x08000000` 到 `0x080047FF` 的 IAP 区。
- 收完整镜像后做 CRC 校验。
- 校验成功后清升级标志并跳转 `0x08004800`。
- 校验失败或掉电中断后继续停留在 IAP，等待重新升级。

### 第四阶段：GUI 上位机

目标：

- 做成图形界面，包含连接、实时信息、保护参数、升级、日志页。
- 后端继续复用第一阶段的 Python CAN 协议代码。
- 后续如需要一键售后设备，再把同一协议移植到专用 MCU 升级器。

## 4. CAN-IAP 协议草案

为避免和当前 App 的电池广播 `0x14F802xx` 冲突，IAP 升级使用新的扩展帧 ID。

| 方向 | 扩展帧 ID | 用途 |
| --- | --- | --- |
| 上位机 -> IAP 控制帧 | `0x14F8F000 | node_id` | 握手、开始、结束、终止 |
| IAP -> 上位机应答帧 | `0x14F8F100 | node_id` | ACK/NACK、错误码、期望序号 |
| 上位机 -> IAP 数据帧 | `0x14F90000 | (seq << 8) | node_id` | App 固件数据，8 字节 payload |

固定参数：

- `node_id` 默认 `1`。
- 当前 App 固件 CAN 位时序为 `250 kbit/s`，上位机默认 `250000`。
- `APP_BASE_ADDR` 固定 `0x08004800`。
- `IAP_BASE_ADDR` 固定 `0x08000000`。
- 固件数据最后一帧不足 8 字节时用 `0xFF` 填充，但 CRC 只计算原始 `.bin` 实际长度。

控制命令：

| 命令 | payload 格式 | 说明 |
| --- | --- | --- |
| `0x01 HELLO` | `01 version node FF FF FF FF FF` | 握手 |
| `0x02 START` | `02 version size32 crc16` | 开始升级，`size32` 为 App 镜像字节数 |
| `0x04 END` | `04 frame_count16 crc16 FF FF FF` | 数据发送结束 |
| `0x05 ABORT` | `05 reason FF FF FF FF FF FF` | 中止升级 |
| `0x79 ACK` | `79 cmd status expect_seq16 code FF FF` | IAP 应答成功 |
| `0x1F NACK` | `1F cmd status expect_seq16 code FF FF` | IAP 应答失败 |

CRC 使用 Modbus CRC16，和当前工程 `Sci_CRC16RTU()` 路径保持一致。

## 5. 上位机怎么做

本仓库新增起步工具：

`tools/can_bms_host.py`

启动脚本：

`tools/start_can_bms_host.ps1`

典型命令：

```powershell
.\tools\start_can_bms_host.ps1 -Mode detect

.\tools\start_can_bms_host.ps1 -Mode listen -Interface pcan -Channel PCAN_USBBUS1 -Bitrate 250000 -Duration 10

.\tools\start_can_bms_host.ps1 -Mode app-read-status -Interface pcan -Channel PCAN_USBBUS1 -Bitrate 250000 -CanAddress 0

.\tools\start_can_bms_host.ps1 -Mode app-enter-iap -Interface pcan -Channel PCAN_USBBUS1 -Bitrate 250000 -CanAddress 0 -ConfirmEnterIap

.\tools\start_can_bms_host.ps1 -Mode upgrade-dry-run -Bin "103 + 309\Project\Users\Objects\FD_Release.bin"
```

如果没有 CAN 盒，也可以先用 dry-run 检查固件分包：

```powershell
.\tools\start_can_bms_host.ps1 -Mode upgrade-dry-run -Bin "103 + 309\Project\Users\Objects\FD_Release.bin"
```

真实发升级帧必须等 IAP 固件支持该协议后再执行，并且必须显式确认 App 地址：

```powershell
.\tools\start_can_bms_host.ps1 -Mode upgrade -Interface pcan -Channel PCAN_USBBUS1 -Bitrate 250000 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -ConfirmAppAddress 0x08004800
```

## 6. 固件侧待办

IAP 工程需要新增或确认：

- CAN 引脚、波特率、过滤器和收发器电源控制。
- IAP 启动时保留足够等待窗口，能收到上位机 `HELLO`。
- App 区擦除边界从 `0x08004800` 开始，严禁擦除 `0x08000000` 到 `0x080047FF`。
- 数据帧按 `seq` 顺序写入 Flash。
- 写后回读校验。
- `START` 和 `END` 的 size/CRC 校验。
- 升级失败保持 IAP 模式，不跳转损坏 App。
- 成功后清 `FLASH_ADDR_UPDATE_FLAG`，跳转 App。

App 工程需要新增或确认：

- CAN 命令进入 IAP。
- 电池信息读取和保护参数修改的 CAN 协议。
- 参数写入复用现有 RS485 地址表和范围校验，不要另起一套参数含义。

## 7. 安全规则

- IAP 地址固定 `0x08000000`。
- App 地址固定 `0x08004800`。
- CAN-IAP 升级只能写 App 区，禁止写 IAP 区。
- 上位机必须默认 dry-run，真实发送升级帧必须显式确认 `0x08004800`。
- 新增烧录或升级脚本必须保留地址检查和 dry-run 输出。

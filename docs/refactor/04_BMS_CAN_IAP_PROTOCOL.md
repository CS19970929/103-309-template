# BMS CAN-IAP 升级协议

日期：2026-05-21

## 1. 设计目标

1. 升级失败后 BMS 必须停留在 Bootloader。
2. Bootloader 不擦写自身区域。
3. App valid 标志只在完整镜像校验通过后写入。
4. Comm Tool 是升级主控，BMS Bootloader 是被动接收方。
5. 支持超时重试、断电后重来、坏包拒绝。

## 2. CAN 参数

| 项目 | 默认值 |
|---|---|
| CAN 波特率 | `250 kbit/s` |
| 帧格式 | 扩展帧 |
| node id | 默认 `1` |
| 数据帧 payload | 8 字节 |

## 3. CAN ID

| 方向 | 扩展帧 ID | 用途 |
|---|---:|---|
| Comm Tool -> BMS Boot | `0x18E00000 | node_id` | 控制帧 |
| BMS Boot -> Comm Tool | `0x18E10000 | node_id` | ACK/NACK |
| Comm Tool -> BMS Boot | `0x18E20000 | node_id` | 数据帧 |

## 4. 控制命令

| 命令 | 名称 | payload |
|---:|---|---|
| `0x01` | `HELLO` | `cmd, proto_ver, node_id, 0, 0, 0, 0, 0` |
| `0x02` | `START` | `cmd, image_type, size32, crc16` |
| `0x03` | `ERASE` | `cmd, area, start_page16, page_count16, 0` |
| `0x04` | `END` | `cmd, frame_count16, crc16, 0, 0, 0` |
| `0x05` | `VERIFY` | `cmd, size32, crc16` |
| `0x06` | `RUN` | `cmd, 0, 0, 0, 0, 0, 0, 0` |
| `0x07` | `ABORT` | `cmd, reason, 0, 0, 0, 0, 0, 0` |

`START` 之后 Bootloader 清除 App valid 标志。`VERIFY` 成功后 Bootloader 写入 App valid 标志。

## 5. 数据帧

数据帧固定 8 字节：

| 字节 | 含义 |
|---:|---|
| 0..1 | seq16 |
| 2..7 | 固件数据 6 字节 |

最后一帧不足 6 字节时使用 `0xFF` 填充，CRC 只计算原始固件长度。

## 6. ACK/NACK

| 字节 | 含义 |
|---:|---|
| 0 | `0x79` ACK 或 `0x1F` NACK |
| 1 | 原命令 |
| 2 | status |
| 3..4 | expect_seq16 |
| 5..6 | error_code16 |
| 7 | 保留 |

错误码：

| 错误码 | 含义 |
|---:|---|
| `0x0000` | 无错误 |
| `0x0001` | 命令顺序错误 |
| `0x0002` | 镜像过大 |
| `0x0003` | 擦除失败 |
| `0x0004` | 写入失败 |
| `0x0005` | seq 错误 |
| `0x0006` | CRC 错误 |
| `0x0007` | App 向量非法 |
| `0x0008` | App 地址越界 |
| `0x0009` | 忙 |

## 7. Bootloader 状态机

```text
BOOT
  -> CHECK_APP
  -> WAIT_HELLO
  -> ERASE_APP
  -> RECEIVE_IMAGE
  -> VERIFY_IMAGE
  -> MARK_VALID
  -> RUN_APP
```

异常路径：

| 场景 | 处理 |
|---|---|
| 空 App | 停留 `WAIT_HELLO` |
| App MSP 不在 SRAM | 停留 `WAIT_HELLO` |
| App ResetHandler 不在 App 区 | 停留 `WAIT_HELLO` |
| START 后断电 | 下次上电停留 `WAIT_HELLO` |
| 数据帧 seq 错误 | NACK 并返回期望 seq |
| END/VERIFY CRC 错误 | 清 App valid，停留 `WAIT_HELLO` |
| RUN 前校验失败 | NACK，停留 `WAIT_HELLO` |

## 8. 跳转 App 要求

Bootloader 跳转前必须：

1. 关闭 SysTick。
2. 关闭并清理外设中断。
3. 清 NVIC pending。
4. 设置 `SCB->VTOR = BMS_APP_BASE`。
5. 设置 MSP 为 App 向量表第一个 word。
6. 跳转 ResetHandler，跳转函数不返回。

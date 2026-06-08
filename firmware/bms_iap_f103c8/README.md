# BMS CAN-IAP F103C8

这是当前 `103 + 309` 主板配套的 BMS 端 IAP 工程。

## 地址规划

| 区域 | 地址 |
| --- | --- |
| IAP | `0x08000000..0x080047FF` |
| App | `0x08004800..0x0801F7FF` |
| 运行标志页 | `0x0801F800..0x0801FFFF` |
| SRAM mailbox | `0x20004FE0` |

App 通过 `AppUpgrade_RequestIap()` 写入 SRAM mailbox，复位后 IAP 校验 `magic`、反码、`request`、反码和 CRC；校验通过后停留在 IAP，否则在 App 向量有效时跳转 App。

## CAN-IAP

- CAN：`250 kbit/s`
- 默认节点：`1`
- 控制帧：`0x14F8F000 | node`
- ACK 帧：`0x14F8F100 | node`
- 数据帧：`0x14000000 | (seq << 8) | node`

升级流程按 `docs/protocol/BMS_CAN_IAP_PROTOCOL.md`：

1. `HELLO`
2. `START(size, crc16)`
3. 连续数据帧，每 256B `COMMIT`
4. `END(frame_count, crc16)`

IAP 在 `START` 时擦除 App 首页使旧 App 立即失效；首个 App 页缓存到最后，在整包 CRC 和向量表校验通过后才写入 MSP/ResetHandler。

源码同时保留旧串口 IAP 解析作为兜底路径，默认 USART1 重映射 `PB6/TX`、`PB7/RX`，`115200 8N1`。

## Keil 工程

工程文件：`firmware/bms_iap_f103c8/keil/BMS_CAN_IAP_F103C8.uvprojx`

构建后烧录 IAP 到 `0x08000000`，App bin 必须是链接到 `0x08004800` 的产物。

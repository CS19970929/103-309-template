# BMS CAN-IAP 升级协议

## CAN ID

使用扩展帧，避开现有 BMS 周期广播 `0x14F802xx`。

| 方向 | 扩展帧 ID | 说明 |
| --- | --- | --- |
| comm tool -> BMS IAP | `0x14F8F000 | node` | 控制帧 |
| BMS IAP -> comm tool | `0x14F8F100 | node` | ACK/NACK |
| comm tool -> BMS IAP | `0x14F90000 | (seq << 8) | node` | 数据帧 |

默认 `node=1`，CAN 波特率第一阶段为 `250 kbit/s`。

## 控制命令

| 命令 | payload | 说明 |
| --- | --- | --- |
| `0x01 HELLO` | `01 version node FF FF FF FF FF` | 握手 |
| `0x02 START` | `02 version size:u32 crc16:u16` | 开始升级 |
| `0x03 COMMIT` | `03 block_seq:u16 block_len:u16 block_crc:u16 FF` | 提交一个块 |
| `0x04 END` | `04 frame_count:u16 crc16:u16 FF FF FF` | 结束升级 |
| `0x05 ABORT` | `05 reason FF FF FF FF FF FF` | 终止升级 |
| `0x79 ACK` | `79 cmd status expect_seq:u16 code FF FF` | 成功响应 |
| `0x1F NACK` | `1F cmd status expect_seq:u16 code FF FF` | 失败响应 |

## 数据与提交

- 每个数据帧 8 字节。
- `seq` 从 0 开始递增。
- 推荐 32 个数据帧组成一个 256B 块。
- BMS IAP 只有收到 `COMMIT` 且块 CRC 正确后，才擦写或写入 Flash。
- 每个块写入后必须回读校验。
- 整包 `END` 后再做总 CRC 和 App 向量校验。

## App 有效性检查

写入完成后必须检查：

- 初始 MSP 在 SRAM 范围内。
- ResetHandler 在 App Flash 范围内。
- 固件长度不越过 App 区上限和参数区。
- 整包 CRC16 与 `START/END` 一致。

## 断电恢复

- `START` 后立即清除 App 有效标志。
- 任意升级中断后，下次启动停留在 IAP。
- 未通过 `END` 总校验时禁止跳 App。
- 升级成功后再写 App 有效标志并跳转。


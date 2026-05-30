# 固件发布产物报告

- version: `unknown`
- generated_at: `2026-05-30T11:04:30`

## 产物清单

| 文件 | 类型 | 大小(bytes) | CRC32 | SHA256(前16) | 构建时间 |
|---|---|---|---|---|---|
| data/examples/firmware_demo.bin | .bin | 37 | 0xCEA34B53 | 8a4f3c9fadfb9b97 | 2026-05-30T11:02:15 |
| data/examples/firmware_demo.hex | .hex | 72 | 0x18FC4BF9 | 9bd2bb48895b6f59 | 2026-05-30T11:02:15 |
| data/examples/example_keil.map | .map | 457 | 0x535702A0 | d013870b7d2d7b41 | 2026-05-30T10:53:30 |

## Flash/RAM 估算

| 来源 | Code | RO | RW | ZI | Flash估算 | RAM估算 |
|---|---|---|---|---|---|---|
| data/examples/example_keil.map | 4096 | 512 | 128 | 2048 | 4736 | 2176 |

## 安全说明

- 本工具只读取本地产物并计算校验值，不烧录、不擦除、不连接设备。

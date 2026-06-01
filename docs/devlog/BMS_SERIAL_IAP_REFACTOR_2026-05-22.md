# BMS 串口 IAP 重构记录

## 兼容范围

本次重构不改变 PC 和板端之间的串口升级命令入口：

| 地址 | 命令 | 说明 |
| --- | --- | --- |
| `0xFFFD` | `0x10` | App 写 SRAM mailbox 后复位进入 IAP；IAP 侧初始化升级会话 |
| `0xFFFE` | `0x10` | IAP 写入 App 数据块 |
| `0xFFFF` | `0x10` | IAP 完成升级 |

旧上位机如果按原方式发送 1024B 数据块，协议入口仍兼容。

## IAP 侧新逻辑

- 新增 `iap_upgrade.c/.h`，串口升级由独立状态机处理。
- `Sci.c` 不再直接擦写 App Flash，只负责协议解析、ACK/NACK。
- 单块最大 1024B，写入范围限制在 `0x08004800` 到 `0x0801F800`。
- 每块写入后逐半字回读校验。
- 完成命令必须通过 App 向量校验后才写 `FLASH_TO_APP_VALUE`。
- 超时或错误后保持 IAP 标志，避免跳入坏 App。

## App 侧新逻辑

- 新增 `AppUpgrade_RequestIap()`，串口和 CAN 进入 IAP 请求统一写入 `0x20004FE0` 的 SRAM mailbox，并立即读回 `magic/request/crc` 校验。
- 串口 ACK 发送完成后才置复位标志，避免上位机收不到 ACK。
- CAN 进入 IAP 保持原握手参数 `C3 3C can_addr`，但也复用同一个 mailbox 校验函数。
- 不再使用 `0x0801F800` 的 Flash 半字作为 App 跳 IAP 请求；该页保留给 IAP 断电安全和 App 有效性边界，避免每次升级入口擦写 Flash。

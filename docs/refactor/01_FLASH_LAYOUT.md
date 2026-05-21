# Flash 分区规划

日期：2026-05-21

## 1. Comm Tool STM32F103RET6

目标芯片：`STM32F103RET6`

资源假设：

| 资源 | 大小 |
|---|---:|
| Flash | 512 KB |
| SRAM | 64 KB |
| Flash 页大小 | 2 KB |

Comm Tool 不使用 Bootloader。第一版从 `0x08000000` 直接启动，应用区最大限制为 128 KB，剩余 Flash 用于缓存 BMS 固件和升级状态。

| 区域 | 起始地址 | 结束地址 | 大小 | 用途 |
|---|---:|---:|---:|---|
| Comm Tool 程序区 | `0x08000000` | `0x0801FFFF` | 128 KB | Keil scatter 限制在此区域 |
| BMS 固件缓存区 | `0x08020000` | `0x0806FFFF` | 320 KB | 存储 PC 下载的 BMS App 镜像 |
| 升级索引区 | `0x08070000` | `0x08077FFF` | 32 KB | 固件长度、CRC、有效标志、升级状态 |
| Comm Tool 参数区 | `0x08078000` | `0x0807FFFF` | 32 KB | CAN 波特率、节点号、串口配置 |

Comm Tool 写 Flash 的硬规则：

1. 禁止擦写 `0x08000000 - 0x0801FFFF`。
2. 下载 BMS 固件只能写入 `0x08020000 - 0x0806FFFF`。
3. 固件缓存写入完成并 CRC 通过后，才允许写入有效标志。
4. 升级过程中如果复位，Comm Tool 必须能根据索引区判断缓存固件是否有效。

## 2. BMS Next 建议分区

BMS Next 的最终地址要以实际 BMS MCU 型号和 Flash 容量确认后定稿。重构版不强制继承旧地址。

建议优先方案：

| 区域 | 起始地址 | 结束地址 | 用途 |
|---|---:|---:|---|
| BMS Bootloader | `0x08000000` | `0x08007FFF` | CAN-IAP 和 App 校验跳转 |
| BMS App | `0x08008000` | 参数区前 | 业务程序 |
| BMS 参数区 | Flash 尾部预留 | Flash 尾部 | 保护参数、SOC 快照、运行参数 |
| BMS 升级标志区 | Flash 尾部预留 | Flash 尾部 | App 有效标志、版本、CRC |

如果实际 BMS MCU Flash 容量无法支持 32 KB Bootloader，再评估压缩到 `0x08004800` 旧 App 地址方案。地址调整必须同步更新：

1. BMS Bootloader scatter。
2. BMS App scatter。
3. Comm Tool CAN-IAP 协议常量。
4. PC 工具固件检查规则。
5. 烧录脚本 dry-run 输出和地址保护。

## 3. BMS Bootloader 安全规则

1. Bootloader 永远不擦写自身区域。
2. 擦 App 区之前先清除 App valid 标志。
3. 只在完整镜像 CRC 通过后写 App valid 标志。
4. 跳 App 前必须校验 MSP、ResetHandler、长度和 CRC。
5. 升级中断、坏 CRC、坏向量、空 App 都必须停留在 Bootloader。
6. 看门狗打开时，擦写 Flash 和等待 CAN 包过程中必须有明确喂狗点。

## 4. 旧工程地址说明

旧工程当前 App scatter 地址为 `0x08004800`。该地址只对旧 `103 + 309` 工程和旧安全烧录脚本有效。BMS Next 重构完成前，禁止把新地址假设反向写入旧脚本。

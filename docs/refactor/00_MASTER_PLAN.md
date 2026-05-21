# BMS Next 与 Comm Tool 重构总规划

日期：2026-05-21

## 1. 目标

本轮重构分为三端：

| 端 | 目标 |
|---|---|
| PC 上位机 | 通过串口连接 Comm Tool，读写 BMS 信息和保护参数，下载待升级 BMS 固件到 Comm Tool，触发一键升级 |
| Comm Tool | 使用 STM32F103RET6，PC 侧串口，BMS 侧 CAN，内部 Flash 缓存 BMS 固件，负责 CAN 升级主控 |
| BMS Next | 保留必要 BMS 功能，重构 App 和 Bootloader，支持 CAN 升级，串口通信协议语义保持不变 |

本轮不继续修补旧 IAP。BMS Bootloader 重新设计，升级失败时必须停留在可升级状态，不能跳转半个 App。

## 2. 基本架构

```text
PC Tool
  |
  | UART binary protocol
  |
Comm Tool STM32F103RET6
  |
  | CAN gateway / CAN-IAP
  |
BMS Next App / BMS Next Bootloader
```

PC 不直接连接 BMS CAN。PC 对 BMS 的读写全部通过 Comm Tool 转发。Comm Tool 是升级流程主控，BMS Bootloader 只负责安全接收、写 Flash、校验、跳转。

## 3. 仓库目录

```text
firmware/
  comm_tool_f103ret6/
    keil/
    source/
      app/
      bsp/
      protocol/
      storage/
  bms_next/
    bootloader_can/
    app/
shared/
  protocol/
pc_tool/
docs/refactor/
tools/
```

旧 `103 + 309` 工程暂时只作为功能、寄存器地址、AFE 驱动和参数策略参考。新工程不要直接依赖旧主循环和旧 IAP 状态机。

## 4. 第一阶段交付

1. `docs/refactor` 下生成重构设计文档。
2. 生成 `firmware/comm_tool_f103ret6` Keil MDK 工程骨架。
3. Comm Tool 工程使用标准外设库，目标芯片为 `STM32F103RET6`。
4. 固化 Comm Tool Flash 分区、串口协议、CAN 网关协议和 CAN-IAP 协议。
5. 新增 Comm Tool 命令行构建脚本。

## 5. 第二阶段交付

1. 完成 Comm Tool 串口收发、CAN 收发、固件缓存 Flash 写入。
2. PC 命令行工具能下载 BMS `.bin` 到 Comm Tool 并校验。
3. Comm Tool 可查询缓存固件版本、长度、CRC 和有效标志。

## 6. 第三阶段交付

1. 新建 BMS CAN Bootloader 工程。
2. 实现 CAN-IAP 接收、写 App 区、CRC 校验、有效标志、跳转 App。
3. 测试空 App、坏向量、坏 CRC、升级中断、重复升级。

## 7. 第四阶段交付

1. 新建 BMS Next App 工程。
2. 保留 AFE、保护、MOS、SOC、参数、串口协议。
3. 保留数码管、出厂老化、低功耗逻辑，但重构为边界清晰、任务入口统一、可配置的简化实现。
4. 新增 CAN 网关读写和进入 Bootloader 命令。
5. 删除旧 IAP 和无关调试功能，复杂日志默认精简为必要故障快照。

## 8. 第五阶段交付

1. PC 上位机 GUI。
2. 参数读写界面。
3. 固件下载到 Comm Tool。
4. 一键升级 BMS。
5. 升级进度和错误码显示。

## 9. 提交规则

每个大阶段必须同步更新文档，并自动创建描述清楚的 git 提交。涉及烧录地址、测试模式、量产隔离、上位机启动方式的规则必须写入仓库脚本或文档。

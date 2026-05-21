# Comm Tool Keil 工程说明

日期：2026-05-21

## 1. 工程定位

Comm Tool 工程目录：

```text
firmware/comm_tool_f103ret6/
```

Keil 工程：

```text
firmware/comm_tool_f103ret6/keil/COMM_TOOL_F103RET6.uvprojx
```

目标 MCU：

```text
STM32F103RET6
```

使用库：

```text
STM32F10x_StdPeriph_Lib_V3.5.0
```

## 2. Target

| Target | 用途 |
|---|---|
| `COMM_TOOL_Debug` | 调试构建，保留调试信息 |
| `COMM_TOOL_Release` | 正式构建，输出 bin |

两个 Target 都使用 `STM32F10X_HD` 和 `USE_STDPERIPH_DRIVER`。

## 3. scatter

scatter 文件：

```text
firmware/comm_tool_f103ret6/keil/COMM_TOOL_F103RET6.sct
```

程序区限制：

```text
0x08000000 - 0x0801FFFF
```

这样可以防止 Comm Tool 程序链接到 BMS 固件缓存区。

## 4. 模块

| 目录 | 模块 |
|---|---|
| `source` | main、配置 |
| `source/bsp` | UART、CAN、板级初始化、中断 |
| `source/protocol` | PC 串口协议、CAN 网关、升级管理、CRC |
| `source/storage` | 内部 Flash 固件缓存 |

## 5. 默认外设

| 外设 | 引脚 | 用途 |
|---|---|---|
| USART1 TX | PA9 | PC 串口 |
| USART1 RX | PA10 | PC 串口 |
| CAN1 RX | PA11 | BMS CAN |
| CAN1 TX | PA12 | BMS CAN |

后续如果硬件原理图不同，必须同步修改 `comm_tool_config.h` 和本文档。

## 6. 构建脚本

脚本：

```powershell
.\tools\build_comm_tool_keil.ps1 -Target COMM_TOOL_Release
```

脚本职责：

1. 定位 `UV4.exe`。
2. 检查 `.uvprojx` 存在。
3. 检查 Target 名称存在。
4. 调用 Keil 命令行构建。
5. 输出日志路径。

## 7. 第一版限制

当前工程骨架先固化模块边界、Flash 分区、协议常量和 Keil 配置。升级传输完整业务会分阶段实现并逐步验证。

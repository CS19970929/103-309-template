# Comm Tool STM32F103RET6

本工程是 PC 串口到 BMS CAN 的 Comm Tool 固件。

目标：

1. PC 通过串口读写 Comm Tool。
2. Comm Tool 通过 CAN 读写 BMS。
3. PC 下载 BMS 固件到 Comm Tool 内部 Flash。
4. Comm Tool 通过 CAN-IAP 给 BMS 一键升级。

Keil 工程：

```text
keil/COMM_TOOL_F103RET6.uvprojx
```

构建脚本：

```powershell
.\tools\build_comm_tool_keil.ps1 -Target COMM_TOOL_Release
```

第一版工程骨架已经固定 Flash 分区、串口协议入口、CAN 网关入口、固件缓存入口和升级管理入口。完整升级传输会在后续阶段继续补齐。

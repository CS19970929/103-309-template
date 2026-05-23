# comm tool F103RET6 Keil 工程说明

## 工程入口

- Keil 工程：`firmware/comm_tool_f103ret6/keil/COMM_TOOL_F103RET6.uvprojx`
- Target：`COMM_TOOL_Release`
- MCU：`STM32F103RET6`，Keil 设备选择 `STM32F103RE`
- 代码区：`0x08000000` 起，IROM 限制 `0x00010000`
- BMS App 缓存区：`0x08010000` 起
- 缓存元数据页：`0x0807F800`

IROM 故意限制为 64KB，目的是让 comm tool 自身程序不能覆盖从 `0x08010000` 开始的 BMS 固件缓存区。

## 板级引脚

参考 `E:\TODO\code\c058 from c030`，只移植串口、CAN、供电和 debug LED：

| 功能 | 引脚 | 配置 |
| --- | --- | --- |
| PC 串口 TX | `PC10` | `USART3_TX`，`GPIO_PartialRemap_USART3` |
| PC 串口 RX | `PC11` | `USART3_RX`，默认 `115200 8N1` |
| CAN RX | `PA11` | `CAN1_RX` |
| CAN TX | `PA12` | `CAN1_TX` |
| CAN/通信供电 | `PC12` | 上电置 1 |
| PWSV 控制 | `PC13` | 上电置 1 |
| PWSV 待机 | `PD2` | 上电置 0 |
| Debug LED | `PB15` | 500ms 翻转 |

CAN 默认波特率为 `250 kbit/s`，同时保留 `125 kbit/s` 和 `500 kbit/s` 的运行时切换。

## 源码范围

- `source/app/`：comm tool 协议、Flash 缓存、CAN-IAP 业务逻辑。
- `source/bsp/`：本次新增的 RET6 板级适配。
- 标准库复用仓库已有 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0`，没有拷贝无关业务代码。

## 构建命令

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -b "firmware\comm_tool_f103ret6\keil\COMM_TOOL_F103RET6.uvprojx" -t "COMM_TOOL_Release" -o "firmware\comm_tool_f103ret6\keil\build_logs\COMM_TOOL_Release.log"
```

构建产物：

- `firmware/comm_tool_f103ret6/keil/Objects/Release/COMM_TOOL_Release.axf`
- `firmware/comm_tool_f103ret6/keil/Objects/Release/COMM_TOOL_Release.hex`
- `firmware/comm_tool_f103ret6/keil/Objects/Release/COMM_TOOL_Release.bin`

## PC 联调

```powershell
.\tools\start_comm_tool_host.ps1 -Mode info -Port COM4
.\tools\start_comm_tool_host.ps1 -Mode fw-download -Port COM4 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -ConfirmAppAddress 0x08004800
.\tools\start_comm_tool_host.ps1 -Mode upgrade -Port COM4 -ConfirmUpgrade
```

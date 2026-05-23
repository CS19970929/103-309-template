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
| CAN/通信驱动 | `PC12` | 正常运行置 0，对齐 `MCUO_DRV_CMNT=0` |
| PWSV 控制 | `PC13` | 正常运行置 0，对齐 `MCUO_PWSV_CTR=0` |
| PWSV 待机 | `PD2` | 正常运行置 1，对齐 `MCUO_PWSV_STB=1` |
| Debug LED | `PB15` | 500ms 翻转 |

comm tool 当前复用主仓库 `system_stm32f10x.c`，运行时 HSE 直连，`PCLK1=8 MHz`。CAN 位时序必须按该时钟计算：

| CAN 波特率 | BS1 | BS2 | Prescaler |
| --- | --- | --- | --- |
| `125 kbit/s` | `5tq` | `2tq` | `8` |
| `250 kbit/s` | `5tq` | `2tq` | `4` |
| `500 kbit/s` | `5tq` | `2tq` | `2` |

默认使用 `250 kbit/s`，和 BMS IAP 的 CAN1 位时序一致。不要按 36MHz PCLK1 计算 comm tool CAN 预分频，否则工具界面显示 250k，但实际总线速率会错误。

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

# comm tool firmware source

目标芯片为 `STM32F103RET6`，使用 STM32F10x StdPeriph。

业务层源码：

- `ct_protocol.*`：PC 串口帧解析与编码。
- `ct_flash_store.*`：BMS App 固件缓存区管理。
- `ct_can_gateway.*`：BMS App 服务帧和 CAN-IAP 帧封装。
- `ct_upgrade_manager.*`：从本机 Flash 缓存向 BMS IAP 发送升级流程。
- `ct_app.*`：串口命令到 Flash/CAN/升级管理的分发。
- `ct_board_port.h`：板级 UART/CAN/tick/reset 适配接口。

板级适配和 Keil 工程已补齐：

- BSP：`firmware/comm_tool_f103ret6/source/bsp/`
- Keil 工程：`firmware/comm_tool_f103ret6/keil/COMM_TOOL_F103RET6.uvprojx`
- 硬件说明：`docs/COMM_TOOL_F103RET6_KEIL_PORT_2026-05-23.md`

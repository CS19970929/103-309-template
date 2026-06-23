# comm tool firmware source

目标芯片为 `STM32F103RET6`，使用 STM32F10x StdPeriph。

业务层源码：

- `ct_protocol.*`：PC 串口帧解析与编码。
- `ct_modbus_bridge.*`：PC 侧原始 Modbus RTU 透传到 USART2，支持 BMS 直连读写和串口 IAP 经 comm tool 转发。
- `ct_flash_store.*`：BMS App 固件缓存区管理。
- `ct_can_gateway.*`：BMS App 服务帧和 CAN-IAP 帧封装。
- `ct_upgrade_manager.*`：从本机 Flash 缓存向 BMS IAP 发送升级流程。
- `ct_boot_control.*`：comm tool App 进入自身 IAP 的 SRAM mailbox。
- `ct_self_iap.*`：保留 CAN App `ENTER_IAP` 入口，让 comm tool 自身可进入 IAP；App 运行态下 PC 原始 Modbus `0xFFFD` 优先透传给 BMS，避免 BMS 直连 IAP 误触发 comm tool 自身复位。
- `ct_app.*`：串口命令到 Flash/CAN/升级管理的分发。
- `ct_board_port.h`：板级 UART/CAN/tick/reset 适配接口。

板级适配和 Keil 工程已补齐：

- BSP：`firmware/comm_tool_f103ret6/source/bsp/`
- IAP：`firmware/comm_tool_f103ret6/source/iap/`
- PC 侧 UART：串口1，默认 `115200 8N1`；BMS 透传 UART：USART2，`PA2=TX`、`PA3=RX`、`19200 8N1`。
- PA6：离线升级按键，上拉输入，低电平稳定 60 ms 后从有效 BMS App 缓存启动 CAN-IAP。
- App Keil 工程：`firmware/comm_tool_f103ret6/keil/COMM_TOOL_F103RET6.uvprojx`
- IAP Keil 工程：`firmware/comm_tool_f103ret6/keil/COMM_TOOL_IAP.uvprojx`
- 硬件说明：`docs/COMM_TOOL_F103RET6_KEIL_PORT_2026-05-23.md`

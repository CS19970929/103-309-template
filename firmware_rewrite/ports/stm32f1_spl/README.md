# STM32F1 SPL Port Boundary

本目录是 `firmware_rewrite` 接入 STM32F103 + SPL 的硬件适配边界。

`firmware_rewrite/src` 内的业务代码不直接包含 `stm32f10x_*.h`，也不直接操作 GPIO、CAN、USART、RTC、Flash、AFE。真实硬件接入时，只在本 port 层完成这些动作，然后通过 `bms_platform_ops_t` 喂给 clean-room core。

## 需要接线的函数

- `read_sample`：读取 AFE/ADC/DI1/MCU_WK/通信活动，填充 `bms_sample_t`。
- `save_snapshot` / `load_snapshot`：映射到 `0x0801E000` / `0x0801E800` SOC 双槽。
- `can_send_probe` / `can_send_status`：控制 `GPIO_CMNT_EN`，等待收发器稳定后发送探测帧或状态帧。
- `set_charge_mos` / `set_discharge_mos`：输出保护后的 MOS 控制状态。
- `set_display`：按短按 5s、长按 3s、`GPIO_MCU_WK` 持续显示的策略刷新 LED/数码管。
- `enter_rtc_stop`：按 core 给出的 1s/10s 周期配置 `RTC Alarm + EXTI17` 并进入 STOP。
- `request_iap_reset`：写 IAP 标志后复位，App 地址仍为 `0x08004800`。

## 当前状态

当前目录提供可编译的适配边界、`bms_main_stm32f1_spl.c` 入口、rewrite 专用 `system/interrupt` 文件，以及 `bms_board_stm32f1_spl.c` 默认板级实现。

Keil 工程 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` 已经引用本目录和 `firmware_rewrite/src`，并把 App 起点改为 `0x08004800`。

## 已接入内容

- `bms_system_stm32f1_spl.c`：最小 `SystemInit()`，Vector Table 固定偏移 `0x4800`。
- `bms_it_stm32f1_spl.c`：不再引用旧 `main.h`，转发 SysTick/CAN/USART，并处理 `RTCAlarm_IRQHandler`。
- `bms_board_stm32f1_spl.c`：GPIO/ADC/USART1 Modbus/CAN/Flash/RTC STOP/IAP 基础接线。
- `tools/build_rewrite_arm_gcc.py`：用 `arm-none-eabi-gcc` 对 core + port 做 Cortex-M3 只编译检查。

## 上板必须确认

- AFE/ADC 分压和电流零点比例是否与实物一致。
- `GPIO_AFE_CTL`、`GPIO_AFE_PRO` 对 MOS/AFE 的真实控制极性。
- LED Charlieplexing 具体段码和刷新时序。
- CAN transceiver `PB4` 断电/上电极性及 ACK 行为。
- RTC STOP 唤醒周期、唤醒电流和唤醒后时钟恢复。
- RS485 是否使用 USART1 remap `PB6/PB7`，以及是否需要独立 DE/RE 方向脚。

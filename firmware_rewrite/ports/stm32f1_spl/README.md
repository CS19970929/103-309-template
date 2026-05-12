# STM32F1 SPL Port Boundary

本目录是 `firmware_rewrite` 接入 STM32F103 + SPL 的硬件适配边界。

`firmware_rewrite/src` 内的业务代码不直接包含 `stm32f10x_*.h`，也不直接操作 GPIO、CAN、USART、RTC、Flash、AFE。真实硬件接入时，只在本 port 层完成这些动作，然后通过 `bms_platform_ops_t` 喂给 clean-room core。

## 需要接线的函数

- `read_sample`：读取 AFE/ADC/DI1/MCU_WK/通信活动，填充 `bms_sample_t`。
- `save_snapshot` / `load_snapshot`：映射到 `0x0801E000` / `0x0801E800` SOC 双槽。
- `can_send_probe` / `can_send_status`：控制 `GPIO_CMNT_EN`，等待收发器稳定后发送探测帧或状态帧。
- `set_charge_mos` / `set_discharge_mos`：输出保护后的 MOS 控制状态。
- `set_display`：按短按 5s、长按 3s、`GPIO_MCU_WK` 持续显示的策略刷新 LED/数码管。
- `enter_rtc_stop`：按 core 给出的 1s/10s 周期进入 RTC STOP。
- `request_iap_reset`：写 IAP 标志后复位，App 地址仍为 `0x08004800`。

## 当前状态

当前文件提供可编译的适配壳和 `bms_main_stm32f1_spl.c` 入口。默认弱符号函数不会操作真实硬件，真正上板时由板级文件覆盖 `bms_stm32f1_board_*` 函数，不改 `firmware_rewrite/src` 的业务逻辑。

Keil 工程 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` 已经引用本目录和 `firmware_rewrite/src`，并把 App 起点改为 `0x08004800`。

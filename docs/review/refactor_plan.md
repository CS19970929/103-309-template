# D009 CAN/RTC 休眠迁移实施计划

状态：部分验证

参考源码：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/Can_HDX.h`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/conf/conf.c`

## 计划

1. 已完成：保留 D009 `LedBar.c/.h`、`conf_gpio.h` 中普通 SOC LED 和 socKey 定义，未迁移当前参考分支数码管/Charlieplexing 显示代码。
2. 已完成：在 D009 `Can_HDX.c` 中同步 CAN 低功耗运行态参数：可配置 active/idle RTC 周期、ACK 维持 active、连续 no-ACK 回退 idle、RTC service 超时保护。
3. 已完成：调整 CAN busy/sleep-blocked 语义，未开始发送的周期 pending 不阻止 RTC 入睡；正在发送、邮箱未完成、App 命令、read-block 流仍阻止入睡。
4. 已完成：在 D009 `RTC.c` 中同步 STOP alarm 清理和运行态恢复接口，避免 RTC alarm/EXTI17/NVIC pending 残留。
5. 已完成：按当前分支拆分 RTC 低功耗模块，并通过 `rtc_sleep_port.c` 适配 D009 普通 LED、socKey、MAIN_SW、AFE 访问和 `SleepDeal_Continue()` 无参接口。
6. 已完成：更新 Keil 工程文件加入拆分模块，工程文件保持无 BOM UTF-8。
7. 已完成：编译 D009 `FD_Release`，0 error / 8 warning。
8. 待上板验证：有 CAN ACK 对端时 RTC 休眠中 1s 通信；无 ACK 时低频探测；示波器观察 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL。

## 不做事项

- 不改变 CAN ID、CAN App 命令号、payload、CRC、寄存器映射。
- 不修改 IAP/Bootloader 地址和 App 起始地址。
- 不迁移数码管、74HC595、Charlieplexing 或 `GPIO_MCU_WK` 显示逻辑。
- 不引入 HAL、RTOS 或 malloc。

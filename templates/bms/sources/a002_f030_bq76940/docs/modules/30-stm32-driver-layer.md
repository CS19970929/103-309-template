# STM32 驱动库与中断层

## 相关文件

- [system_stm32f0xx.c](../../Code/Drivers/system_stm32f0xx.c)
- [stm32f0xx_it.c](../../Code/Drivers/stm32f0xx_it.c)
- [startup_stm32f0xx.s](../../Code/Drivers/startup_stm32f0xx.s)
- `Code/STM32F0xx_StdPeriph_Driver`

## 模块职责

该层提供 MCU 启动、系统时钟、中断入口和 STM32F0 标准外设库。业务模块不直接依赖寄存器裸写，而是大量使用 StdPeriph API。

## 系统启动

`startup_stm32f030.s` 提供复位向量和中断向量。复位后进入 `SystemInit()`，再进入 C 运行时和 `main()`。

由于 `_IAP` 启用，应用启动后还会由 `Flash.c` 将应用向量表拷贝到 SRAM 并重映射。

## 系统时钟

`system_stm32f0xx.c` 中配置 HSE + PLL。当前宏路径下目标系统频率为 48MHz。若 HSE 启动失败，代码保留 HSI 回退路径。

## 中断层边界

| 中断 | 职责 |
| --- | --- |
| TIM17 | 产生系统节拍和任务标志。 |
| USART1/USART2 | 接收串口字节，更新通信活动状态。 |
| EXTI0_1 | PA0 充电器/全串唤醒。 |
| EXTI4_15 | PB6、PA10、PC13 等唤醒输入。 |
| RTC | Alarm A 周期唤醒。 |
| SysTick | 当前不承担调度，handler 为空。 |

## 维护建议

- 中断中保持短逻辑，只设置标志，不执行 EEPROM、I2C、复杂保护判断等耗时操作。
- 修改系统频率时必须同步 TIM17、TIM15、USART、ADC、I2C bit-bang 延时和看门狗相关假设。
- 升级 MCU 型号时要同步启动文件、向量表拷贝数量、Flash/SRAM 地址和 StdPeriph 设备宏。

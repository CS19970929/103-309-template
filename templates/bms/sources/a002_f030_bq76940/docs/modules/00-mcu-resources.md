# MCU 资源与引脚分配

本页描述当前工程实际使用的 MCU 资源。判断依据包括 Keil 工程配置、`main.h` 条件宏、初始化函数和中断服务函数。

## MCU 与存储资源

| 项目 | 配置 |
| --- | --- |
| MCU | `STM32F030C8` |
| 内核 | Cortex-M0 |
| Flash | `0x08000000` 起始，`0x10000` 字节，64KB |
| SRAM | `0x20000000` 起始，`0x2000` 字节，8KB |
| 应用入口 | `APPLICATION_ADDRESS = 0x08001C00` |
| IAP 区 | `0x08000000` 至应用起始前 |
| App 链接区 | `0x08001C00..0x0800DFFF` |
| Internal Flash Storage | `0x0800E000..0x0800FFFF` |
| Flash 标志 | `0x0800FFF0`、`0x0800FFF2`、`0x0800FFF4` |

## 时钟资源

| 资源 | 用途 | 说明 |
| --- | --- | --- |
| HSE | 系统主时钟来源 | `system_stm32f0xx.c` 中默认使用 HSE + PLL。 |
| PLL | 系统倍频 | `_HSE_12M_PLL_48M` 路径下 12MHz HSE 倍频到 48MHz。 |
| LSI | IWDG、RTC 备选 | IWDG 依赖 LSI；RTC 在 LSE 失败时回退到 LSI。 |
| LSE | RTC 首选 | `RTC.c` 中优先启用 LSE。 |
| SysTick | 阻塞延时 | `SysTick_Handler()` 为空，系统节拍不依赖 SysTick 中断。 |

## 定时器资源

| 定时器 | 用途 | 配置 |
| --- | --- | --- |
| `TIM17` | 系统节拍 | `InitTimer()` 配置 500us 中断，两次累加形成 1ms，并派生 10ms、200ms 等任务标志。 |
| `TIM15` | ADC 外部触发 | `ADC.c` 中配置 TRGO，约 10ms 触发一次 ADC 扫描。 |
| `TIM3` | PWM 预留 | `PWM.c` 使用 CH1/CH2 输出，但默认未在主流程初始化。 |

## ADC 资源

| ADC 通道 | GPIO | 业务含义 | 说明 |
| --- | --- | --- | --- |
| `ADC_Channel_4` | PA4 | 外部温度 EV1 | ADC + DMA 循环采样。 |
| `ADC_Channel_5` | PA5 | 外部温度 EV2 | 与 `LedBar` 的 RUN LED 条件功能冲突。 |
| `ADC_Channel_8` | PB0 | MOS 温度 | 用于 MOS 温度保护。 |

ADC 配置为 12bit、向上扫描、DMA circular 模式，DMA 缓冲区为 `g_u16ADCValFilter[ADC_NUM]`。

## USART 资源

| USART | GPIO | 默认协议 | 波特率 | 中断 |
| --- | --- | --- | --- | --- |
| `USART1` | PA9 TX / PA10 RX | `Sci_Upper` | 19200 | `USART1_IRQHandler()` |
| `USART2` | PA2 TX / PA3 RX | `Sci_Upper` | 19200 | `USART2_IRQHandler()` |

`Uart_Client` 也使用相同 USART 引脚，但 `_CLIENT_SCI1/_CLIENT_SCI2` 默认未启用。

## I2C 与软件总线资源

| 总线 | GPIO | 用途 | 实现 |
| --- | --- | --- | --- |
| AFE I2C | PB10 SCL / PB11 SDA | 访问 `BQ769x0` | `bsp_i2c_gpio1.c` 软件 I2C |
| Generic BSP I2C | PB8 SCL / PB7 SDA | 历史/预留 | `bsp_i2c_gpio.c` |

注意：`I2C_AFE1.h` 中仍保留 PF6/PF7 的历史 bit-bang 宏，但当前 `InitAFE1_IIC()` 实际调用 PB10/PB11 软件 I2C。

## GPIO 资源表

| GPIO | 当前用途 | 所属模块 | 备注 |
| --- | --- | --- | --- |
| PA0 | `INT_WK_MCU` / 充电器唤醒 | `ChargerLoadFunc`、EXTI | EXTI0，部分历史模块也用作紧急按键。 |
| PA1 | `MCUO_AFE_ALARM` / `M_AD_PWR` | `System_Init` | 输出，默认低。 |
| PA2 | USART2 TX | `Sci_Upper` | AF1。 |
| PA3 | USART2 RX | `Sci_Upper` | AF1。 |
| PA4 | ADC EV1 | `ADC` | 模拟输入。 |
| PA5 | ADC EV2 | `ADC` | 与 `LedBar` RUN LED 冲突。 |
| PA6 | TIM3 CH1 / LED ALARM | `PWM` / `LedBar` | 条件功能冲突。 |
| PA7 | TIM3 CH2 | `PWM` | 默认不启用。 |
| PA8 | MOS 预充控制 | `IO_Control` | `_MOS_SAME_DOOR_NO_PRECHG` 下仍初始化。 |
| PA9 | USART1 TX | `Sci_Upper` | AF1。 |
| PA10 | USART1 RX / EXTI wake | `Sci_Upper`、`SleepDeal` | `UART1_WAKEUP_ENABLE` 时可作为唤醒源。 |
| PA11 | `SD_DRV_CHG` | `System_Init` | 驱动相关控制。 |
| PA12 | 加热/冷却 Relay | `Heat_Cool` | 历史模块中也作为蜂鸣器。 |
| PA15 | 未使用 | 可复用 | 旧 EEPROM WP 已废除。 |
| PB0 | ADC MOS 温度 | `ADC` | 模拟输入。 |
| PB1 | `M_CMNT_EN` / `PWSV_STB` | `System_Init` | 默认输出高。 |
| PB2 | Debug LED | `System_Init` | 预留调试。 |
| PB3 | 未使用 | 可复用 | 旧 EEPROM SCL 已废除。 |
| PB4 | 未使用 | 可复用 | 旧 EEPROM SDA 已废除。 |
| PB5 | `PWSV_LDO` / LED Bar | `System_Init` / `LedBar` | 条件功能复用。 |
| PB6 | `PIN_LOAD_RM` | `SleepDeal`、EXTI | 负载移除/唤醒输入。 |
| PB7 | RS485 enable / CB / LED Bar / BSP I2C SDA | 多模块 | 高风险复用点。 |
| PB8 | 可选负载检测驱动 / BSP I2C SCL / LED Bar | `ChargerLoadFunc` / `LedBar` | 条件功能复用。 |
| PB10 | AFE SCL | `I2C_AFE1` | 软件 I2C。 |
| PB11 | AFE SDA | `I2C_AFE1` | 软件 I2C。 |
| PB12 | LED Bar | `LedBar` | 条件功能。 |
| PB13 | CHG MOS/Relay / LED Bar | `IO_Control` / `LedBar` | 高风险复用点。 |
| PB14 | DSG MOS/Relay / LED Key | `IO_Control` / `LedBar` | 高风险复用点。 |
| PB15 | BLE enable / power control | `System_Init` | 默认输出高。 |
| PC13 | DI1 / Key / Wake | `IO_Control`、`SleepDeal` | EXTI13 可唤醒。 |
| PF7 | AFE wake | `System_Init` | 历史 AFE I2C 宏也曾使用。 |

## 中断资源

| 中断 | 处理函数 | 用途 |
| --- | --- | --- |
| `TIM17_IRQn` | `TIM17_IRQHandler()` | 系统节拍、任务标志、延时计数。 |
| `USART1_IRQn` | `USART1_IRQHandler()` | 上位机协议接收。 |
| `USART2_IRQn` | `USART2_IRQHandler()` | 上位机协议接收。 |
| `EXTI0_1_IRQn` | `EXTI0_1_IRQHandler()` | PA0 充电器/全串唤醒。 |
| `EXTI2_3_IRQn` | `EXTI2_3_IRQHandler()` | 预留外部中断。 |
| `EXTI4_15_IRQn` | `EXTI4_15_IRQHandler()` | PB6、PA10、PC13 等唤醒源。 |
| `RTC_IRQn` | `RTC_IRQHandler()` | RTC Alarm A 周期唤醒。 |
| `DMA1_Channel1_IRQn` | 可配置 | ADC DMA 传输，当前主要使用 circular buffer。 |

## 资源冲突与版本判断

| 冲突点 | 涉及模块 | 判断建议 |
| --- | --- | --- |
| PA5 | ADC EV2、`LedBar` RUN LED | 默认 ADC 有效，`LedBar` 未启用。 |
| PA6 | PWM CH1、`LedBar` ALARM LED | 默认均未主流程启用，启用前必须二选一。 |
| PB13/PB14 | CHG/DSG MOS、`LedBar` 输出/按键 | 默认 MOS/Relay 控制有效，`LedBar` 未启用。 |
| PB7 | RS485 enable、CB、LED Bar、BSP I2C SDA | 典型客户版本复用点，不能直接同时启用。 |
| PA12 | Heat/Cool Relay、历史 Buzzer | 默认 Heat 未启用，`LED_Buzzer` 排除构建。 |
| PF7 | AFE wake、历史 AFE SDA 宏 | 当前实际 AFE I2C 使用 PB10/PB11。 |

维护时不要只看头文件宏名判断资源占用，应同时确认初始化函数是否被调用、条件宏是否启用、Keil 工程是否参与构建。

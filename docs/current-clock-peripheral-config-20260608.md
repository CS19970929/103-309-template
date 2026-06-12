# 当前 BMS 项目时钟与外设配置梳理

日期：2026-06-08  
分支：`codex/d009-can-host-comm`  
范围：`103 + 309/Project` 主 BMS App 工程，附带列出当前配套 CAN IAP 工程中会影响升级的一致性配置。

## 1. 结论摘要

当前 App 工程不是 72MHz PLL 运行口径。按 Keil target define `STM32F10X_MD,USE_STDPERIPH_DRIVER` 和 `system_stm32f10x.c` 当前源码，系统时钟选择为 `HSE_VALUE`，而 `STM32F10X_MD` 下 `HSE_VALUE = 8MHz`。`HCLK/PCLK1/PCLK2` 都是 8MHz，未配置 PLL 倍频。

启动地址已经按 IAP 方式偏移：App IROM 为 `0x08004800..0x0801F7FF`，向量表 `SCB->VTOR = 0x08004800`。RAM 配置为 `0x20000000..0x20004FDF`，`0x20004FE0` 留作 App 请求 IAP 的 SRAM mailbox。

当前主路径启用的核心外设是：`GPIOA/B/C/D/E`、`USART1`、`USART2`、`CAN1`、软件 SPI 访问 SH36735/AFE、EEPROM 软件 I2C、`IWDG`、`TIM3` 系统节拍。`ADC1/TIM2/DMA1` 代码存在但 `InitDevice()` 里 `InitADC()` 当前注释掉；`USART3`、`Heat/Cool`、LED bar 主循环也未按当前宏启用。

需要优先关注的配置不一致是 CAN 位率：当前 App CAN 在 8MHz PCLK1 下按 `Prescaler=4, BS1=2tq, BS2=1tq` 计算是 500kbit/s；配套 IAP CAN 是 250kbit/s。若上位机和现场总线按 250k 配置，App 正常通信会不一致。

## 2. 当前编译宏与工程边界

| 来源 | 当前配置 | 影响 |
|---|---|---|
| `CommomSH367309_16series_103RCT6_C.uvprojx` | `STM32F10X_MD,USE_STDPERIPH_DRIVER` | STM32F103 medium-density，标准外设库，`HSE_VALUE=8MHz` |
| `main.h` | `_IAP` | App 向量表搬到 `0x08004800` |
| `main.h` | `_COMMOM_UPPER_SCI1`、`_COMMOM_UPPER_SCI2` | 启用 USART1/USART2 上位机协议 |
| `main.h` | `_COMMOM_UPPER_SCI3` 注释 | USART3 代码存在但当前未启用 |
| `conf.h` | `wdog_enable` | App 上电初始化 IWDG，主循环和 delay 中喂狗 |
| `conf.h` | `__FUNC__CAN__` | 启用 CAN 初始化和 `App_Can()` |
| `conf.h` | `__FUNC_RTC__` 注释 | IWDG 使用非 RTC 分支，RTC 功能不由该宏启用 |
| `conf.h` | `__FUNC__HEAT__` 注释 | PC6 heat/cool 初始化和控制不在当前主路径 |
| `elog_cfg.h` | `ELOG_OUTPUT_ENABLE` 注释 | `InitDevice()` 中 easylogger 串口输出分支不启用 |

## 3. 启动、Flash、RAM 与向量表

| 项 | 当前值 | 来源与说明 |
|---|---:|---|
| IAP 起始地址 | `0x08000000` | `Flash.h` |
| App 起始地址 | `0x08004800` | `Flash.h`、Keil IROM、`VECT_TAB_OFFSET` |
| App 大小 | `0x1B000` | Keil IROM，覆盖到 `0x0801F7FF` |
| 升级标志页 | `0x0801F800` | 旧半字升级标志仍保留 |
| 睡眠标志页 | `0x0801FC00` | reset-sleep 使用 |
| App RAM | `0x20000000, size 0x4FE0` | Keil IRAM |
| IAP mailbox | `0x20004FE0` | App 写入、IAP 读取并清除 |
| App VTOR | `FLASH_BASE | 0x4800` | `_IAP` 定义后 `SystemInit()` 设置 |

App 请求 IAP 的当前方式不是只写 Flash 半字，而是：

1. `App_FlashUpdate()` 检测 `u8FlashUpdateFlag == 1`。
2. 关闭 CHG/DSG MOS，延时 10ms。
3. `AppUpgrade_RequestIap()` 写 `0x20004FE0` mailbox：`magic/magic_inv/request/request_inv/crc`。
4. 写入校验成功后 `MCU_RESET()`。
5. IAP 上电读取并清除 mailbox，若有效则停留 IAP，否则 App 向量有效时跳 App。

## 4. 系统时钟树

### 4.1 App 主工程

`SystemInit()` 执行顺序：

1. 打开 HSI，复位 RCC 配置。
2. 关闭 HSE/CSS/PLL，清 PLL 相关位。
3. 调 `SetSysClock()`。
4. 当前只定义 `SYSCLK_FREQ_HSE`，进入 `SetSysClockToHSE()`。
5. HSE ready 后设置：
   - `FLASH_ACR_PRFTBE`
   - Flash latency 0 wait state
   - `HCLK = SYSCLK`
   - `PCLK2 = HCLK`
   - `PCLK1 = HCLK`
   - `SYSCLK = HSE`
6. `_IAP` 下设置 `SCB->VTOR = 0x08004800`。

当前时钟计算：

| 时钟 | 当前值 | 说明 |
|---|---:|---|
| HSE | 8MHz | `STM32F10X_MD` 下标准库默认 |
| HSI | 8MHz | reset 后打开，HSE 失败时仍可能留作 SYSCLK |
| SYSCLK | 8MHz | HSE 直通，无 PLL |
| HCLK | 8MHz | AHB div1 |
| PCLK1 | 8MHz | APB1 div1 |
| PCLK2 | 8MHz | APB2 div1 |
| SysTick clock | HCLK/8 = 1MHz | `InitDelay()` 清 `CLKSOURCE` |

注意：Keil `.uvprojx` 中 `<Cpu>...CLOCK(12000000)...</Cpu>` 是工程元数据，不是当前固件运行时 RCC 配置。

### 4.2 HSE 失败行为

当前 `SetSysClockToHSE()` 的 HSE 失败分支没有显式调用 `System_ERROR_UserCallback(ERROR_HSE)`，也没有主动切换到其它配置；由于 reset 默认 HSI 已打开，实际会保持 HSI 8MHz。`SystemCoreClock` 初始值同样是 8MHz，因此很多基于 `SystemCoreClock` 的 delay 不会因 HSE/HSI 都是 8MHz 而明显错位。

### 4.3 RTC/STOP 相关时钟

`Sys_StopMode()` 进入：

```c
PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
```

函数里只有 `_HSE_8M_PLL_48M` 或 `_HSE_12M_PLL_48M` 定义时才会尝试恢复 PLL；当前这两个宏未定义。`rtc_sleep.c` 中还存在 `cpu_frequency_conf()`，名字像高频恢复，但当前实现没有完整配置 PLL source/multiplier，不能按 72MHz 恢复口径理解。

## 5. 主初始化顺序

当前 `main()`：

1. `InitDevice()`
2. `InitVar()`
3. `InitAFE3520_Registers(0, 0)`
4. 进入主循环：
   - `App_SysTime()`
   - `App_AFEGet()`
   - `App_WarnCtrl()`
   - `App_Sci()`
   - `App_E2promDeal()`
   - `App_CellBalance()`
   - `App_Can()`
   - `App_SleepDeal()`
   - `App_SOC()`
   - `App_FlashUpdate()`
   - `App_LogRecord()`
   - `App_ProID_Deal()`
   - `Feed_IWatchDog`

`InitDevice()` 当前顺序：

1. `SystemInit()`
2. `InitDelay()`
3. `IsSleepStartUp()`
4. `jtag_disableAndConfIO()`
5. `InitNVIC()`
6. `InitIO()`
7. `InitSystemWakeUp()`，当前空函数
8. `InitE2PROM()`
9. `InitCan()`
10. `InitSci()`
11. `InitMosRelay_DOx()`
12. `InitData_SOC()`
13. `bsp_InitSPIBus()`
14. `sh36735_spi_sw_init()`
15. `Init_IWDG()`
16. `InitTimer()`

当前明确未在正常上电路径执行：

| 函数 | 状态 | 说明 |
|---|---|---|
| `InitADC()` | 注释掉 | ADC/TIM2/DMA 配置保留但不启用 |
| `App_AnlogCal()` | 主循环注释掉 | ADC 计算不跑 |
| `InitHeat_Cool()` | 宏关闭 | `__FUNC__HEAT__` 未定义 |
| `App_Heat_Cool_Ctrl()` | 宏关闭 | 同上 |
| `APP_LedBar()` | 主循环未调用 | `LedBar.c` 存在，但当前主路径不驱动 |
| `sleep()`/`rtc_sleep()` | 主循环注释掉 | 当前主循环走 `App_SleepDeal()` reset-sleep 逻辑 |
| `USART3` | 宏关闭 | `_COMMOM_UPPER_SCI3` 未定义 |

## 6. GPIO 正常态配置

### 6.1 全局与 SWJ

`jtag_disableAndConfIO()`：

| 引脚 | 配置 | 初始电平 | 说明 |
|---|---|---:|---|
| PA15/PB3/PB4 JTAG | `GPIO_Remap_SWJ_JTAGDisable` | - | 关闭 JTAG，保留 SWD |
| PB3/PB4 | Out_PP 50MHz | PB4 先拉低 | 后续 `InitIO()` 会重配 PB3/PB4 |

`InitIO()` 先开启 `AFIO + GPIOA/B/C/D/E`。

### 6.2 `InitIO()` 直接配置

| 引脚 | 宏/用途 | 模式 | 速度 | 初始电平 | 备注 |
|---|---|---|---:|---:|---|
| PB15 | `PIN_DBG_LED` | Out_PP | 2MHz | 未显式设置 | 调试 LED |
| PB5 | `PIN_KEY1` | IN_FLOATING | - | - | 与 `MCUI_ENI_DI1`、LED `SOC_100` 定义复用，当前按输入键处理 |
| PA15 | `PIN_M_STB` | Out_PP | 2MHz | 1 | 电源/standby 控制 |
| PB3 | `PIN_AD_EN` | Out_PP | 2MHz | 1 | AD/2737 enable，先由 JTAG disable 配过 |
| PB4 | `PIN_CMNT_EN` | Out_PP | 2MHz | 1 | 与 `BLE_EN/ENO_DO5` 定义复用，当前按 CMNT enable |
| PA4 | `PIN_CS_SPI` | Out_PP | 2MHz | 未显式设置 | 后续 `sh36735_spi_sw_init()` 拉高 CS |
| PA9 | `PIN_CHG_DET` | IN_FLOATING | - | - | 充电检测输入；USART1 已 remap 到 PB6/PB7 |
| PA8 | `PIN_DSG_DET` | IN_FLOATING | - | - | 放电/负载检测输入 |
| PB14 | `PIN_M_CCC` | Out_PP | 2MHz | 1 | 与 `AFE1_CTL` 定义复用 |

### 6.3 后续模块追加 GPIO

| 模块 | 引脚 | 模式 | 当前主路径 |
|---|---|---|---|
| EEPROM 软件 I2C | PB10/PB11 | Out_PP 2MHz，高 | 启用 |
| EEPROM WP | PB13 | Out_PP 2MHz，高 | 启用 |
| USART1 remap | PB6 TX / PB7 RX | AF_PP 10MHz / IN_FLOATING | 启用 |
| USART2 | PA2 TX / PA3 RX | AF_PP 10MHz / IN_FLOATING | 启用 |
| CAN1 | PA11 RX / PA12 TX | IPU / AF_PP 2MHz | 启用 |
| SH36735 软件 SPI | PA5 SCK / PA7 MOSI / PA6 MISO | Out_PP 2MHz / Out_PP 2MHz / IN_FLOATING | 启用 |
| SH36735 CS | PA4 | Out_PP 2MHz，高 | 启用 |
| Heat/Cool | PC6 | Out_PP 2MHz | 当前宏关闭 |
| LED bar | PA5/PA6/PA7/PB1/PC13 | 代码存在 | 主路径未调用，且 PA5/6/7 与 SPI 复用冲突 |
| AFE IIC legacy | PB8/PB9、PC12 | Out_PP 2MHz | `Init()` 低功耗恢复路径使用，正常 `InitDevice()` 不调用 |

## 7. 定时与系统节拍

### 7.1 SysTick

`InitDelay()` 使用 SysTick 做阻塞延时：

| 项 | 当前配置 |
|---|---|
| Clock source | HCLK/8 |
| 当前 tick clock | 1MHz |
| `fac_us` | `SystemCoreClock / 8000000 = 1` |
| 用途 | `__delay_us()`、`__delay_ms()` |
| 中断 | `SysTick_Handler()` 空，未作为系统节拍 |

`__delay_ms()` 内部会调用 `Feed_IWatchDog`，避免长 delay 饿狗。

### 7.2 TIM3 主节拍

`InitTimer()`：

| 项 | 当前值 |
|---|---:|
| 外设 | TIM3 |
| 时钟 | PCLK1 = 8MHz，APB1 div1，所以 TIM3 clock = 8MHz |
| Prescaler | `80 - 1`，前提 `System_ErrFlag.u8ErrFlag_HSE == 0` |
| Period | `99` |
| 更新周期 | `8MHz / 80 / 100 = 1kHz`，即 1ms |
| NVIC | `TIM3_IRQn`，preemption 0，sub 3 |

`TIM3_IRQHandler()` 每 1ms：

| 变量/标志 | 周期 | 用途 |
|---|---:|---|
| `g_u81msClockCnt` | 1ms 累加，达到 2 清零 | `App_SysTime()` 生成 1ms flag |
| `g_u810msClockCnt` | 每 2ms 加 1，达到 5 清零 | `App_SysTime()` 生成 10ms 分片 flag |
| `gu8_200msCnt` | 200ms | `gu8_200msAccClock_Flag` |
| `cnt_1000ms` | 1000ms | `gu8_1000msAccClock_Flag` |

### 7.3 TIM2/ADC 保留链

`InitADC()` 当前未调用。若后续重新启用，需要注意：

| 项 | 当前代码值 | 在 8MHz 下的实际效果 |
|---|---|---|
| TIM2 Prescaler | `72 - 1`，HSE flag 为 0 时 | TIM2 tick 约 111.1kHz |
| TIM2 Period | `999` | 触发约 111.1Hz，不是旧注释里的 1kHz |
| ADC clock | `PCLK2 / 8` | 1MHz，不是注释中的 9MHz |
| ADC channel | ADC1 Channel 1 only | `AD_Used_amount = 1` |

## 8. USART/上位机串口

当前启用 `SCI1` 和 `SCI2`，协议层是类 Modbus RTU：`0x03` 读寄存器、`0x06` 单寄存器写、`0x10` 多寄存器写。`Sci_Upper.h` 中 `__BAUD_RATE__ 115200` 没有被初始化使用；实际硬编码为 19200。

| 通道 | 当前状态 | 引脚 | 重映射 | 波特率 | 格式 | 中断 |
|---|---|---|---|---:|---|---|
| USART1 | 启用 | PB6 TX / PB7 RX | `GPIO_Remap_USART1` | 19200 | 8N1，无流控 | RXNE，NVIC 3/3 |
| USART2 | 启用 | PA2 TX / PA3 RX | 默认 | 19200 | 8N1，无流控 | RXNE，NVIC 3/3 |
| USART3 | 未启用 | PD8/PD9 full remap 代码存在 | `GPIO_FullRemap_USART3` | 19200 | 8N1 | 宏关闭 |

ISR 行为：

| ISR | 行为 |
|---|---|
| `USART1_IRQHandler()` | 错误检查，RXNE 时 `RTC_ExtComCnt++`，调用 SCI1 RX 解析 |
| `USART2_IRQHandler()` | 先处理 ORE/FE/NE/PE，RXNE 时 `RTC_ExtComCnt++`，调用 SCI2 RX 解析，另处理 IDLE 标志 |
| `USART3_IRQHandler()` | 代码存在，宏关闭时只保留空壳逻辑 |

发送不是 TXE ISR 全自动发送，主要由 `App_CommonUpperSCIx()` 在主循环状态机中轮询发送字节。

## 9. CAN1

### 9.1 App CAN

当前 `__FUNC__CAN__` 已定义，`InitCan()` 和 `App_Can()` 都启用。

| 项 | 当前配置 |
|---|---|
| 引脚 | PA11 RX，PA12 TX |
| RX 模式 | `GPIO_Mode_IPU` |
| TX 模式 | `GPIO_Mode_AF_PP`，2MHz |
| CAN remap | 未启用，使用默认 PA11/PA12 |
| CAN mode | Normal |
| ABOM | ENABLE，BusOff 自动恢复 |
| AWUM | DISABLE |
| NART | ENABLE，不自动重发 |
| RFLM/TXFP | DISABLE |
| SJW | 1tq |
| BS1 | 2tq |
| BS2 | 1tq |
| Prescaler | 4 |
| RX interrupt | NVIC 配了 RX0，但 `CAN_IT_FMP0` 被 DISABLE |
| RX 方式 | `App_Can()` 每 10ms 分片轮询 FIFO0 |

位率按当前 8MHz PCLK1 计算：

```text
bitrate = 8MHz / (4 * (1 + 2 + 1)) = 500kbit/s
sample point = (1 + 2) / (1 + 2 + 1) = 75%
```

过滤器：

| 项 | 当前值 |
|---|---|
| Filter number | 0 |
| Mode | IdMask |
| Scale | 16bit |
| FIFO | FIFO0 |
| `CAN_ADRESS_STD_ID` | `0x00` |
| Mask | `0x0780` |
| Filter | `0x0000` |

含义：当前按标准帧 ID 的高位地址段过滤，`CAN_ADRESS_STD_ID=0` 时匹配 bits `10:7` 为 0 的帧。

### 9.2 App CAN 调度

`App_Can()` 只在 `b1Sys10msFlag2` 时运行：

1. `Can_EnsureNormalMode()` 确保 CAN1 时钟打开、退出 sleep/init。
2. `Can_PollReceive()` 关闭 CAN RX 中断并 drain FIFO。
3. `Can_AppService()` 处理上位机桥接命令。
4. `Can_TransmitDeal()` 处理待发标志。
5. 周期发送 `0x02`，周期由 `CAN_0X02_SEND_PERIOD_TICKS` 决定。

### 9.3 IAP CAN

配套 IAP 工程 `firmware/bms_iap_f103c8`：

| 项 | 当前配置 |
|---|---|
| 系统时钟 | HSE 8MHz 优先，失败回 HSI 8MHz |
| 引脚 | PA11/PA12 |
| CAN mode | Normal |
| ABOM | ENABLE |
| NART | DISABLE，允许自动重发 |
| BS1/BS2 | 5tq / 2tq |
| Prescaler | 4 |
| 位率 | `8MHz / (4 * (1 + 5 + 2)) = 250kbit/s` |
| 帧格式 | 扩展帧 |
| Filter | 32bit mask 全 0，接收所有扩展/数据过滤后软件判断 |

结论：App CAN 和 IAP CAN 当前位率不一致。若目标是同一上位机同一 CAN 速率无切换，需统一为 250k 或明确工具在 App/IAP 阶段切速。

## 10. SPI、AFE 与 EEPROM

### 10.1 SH36735/AFE 软件 SPI

当前主路径：

1. `bsp_InitSPIBus()` 配置 PA5/PA7/PA6。
2. `sh36735_spi_sw_init()` 拉高 CS/SCK/MOSI。
3. `InitAFE3520_Registers(0, 0)` 通过 `sh36735_write_reg_u8()`/`sh36735_read_regs()` 初始化 AFE 寄存器并读回校验。

| 信号 | 引脚 | 模式 | 初始态 |
|---|---|---|---|
| CS | PA4 | Out_PP 2MHz | `sh_cs_high()` |
| SCK | PA5 | Out_PP 2MHz | high |
| MOSI | PA7 | Out_PP 2MHz | high |
| MISO | PA6 | IN_FLOATING | - |

软件 SPI 时序：

| 项 | 当前实现 |
|---|---|
| 每 bit | 根据 MSB 设置 MOSI，SCK low delay 1us，SCK high delay 1us，读 MISO |
| 空闲 | SCK high |
| 近似速率 | 单 bit 至少 2us，约 500kbit/s 级别，另有函数调用开销 |

`sh36735_spi_hw.c` 也在源码中，使用 SPI1 mode 3、PA5/6/7、PA4 CS，但主路径没有调用 `sh36735_spi_hw_init()`。当前应按软件 SPI 理解。

### 10.2 AFE 寄存器初始化

`InitAFE3520_Registers(0, 0)` 当前做的关键事：

| 寄存器/动作 | 当前配置 |
|---|---|
| `AFE_SCONF1` | 0 |
| `AFE_SCONF2` | `0x80` 置位，`PD_EN=0`，`CHGMOS=0`，`DSGMOS=0`，`PUMP_EN=0` |
| `AFE_SCONF4` | `SeriesNum` |
| `AFE_SCONF3.CRLD_EN` | 0 |
| `AFE_SCONF6` | `0x7f`，保护使能掩码 |
| OV/UV | OVP 4250mV/5，UVP 2650mV/5 |
| OCD/OCC | `AFE_OCD2V_OCD2T=3`，`AFE_OCCV_OCCT=7` |
| 温度阈值 | 按 NTC 电阻公式写 OTC/UTC/OTD/UTD |
| 校验 | 每次写后读回，不一致报 `ERROR_SPI` |
| 清标志 | 清 OV/UV/OCD/SC/OCC/OTC/UTC/OTD/UTD |

### 10.3 EEPROM 软件 I2C

`InitE2PROM()`：

| 信号 | 引脚 | 模式 | 初始态 |
|---|---|---|---|
| EEPROM SCL | PB10 | Out_PP 2MHz | high |
| EEPROM SDA | PB11 | Out_PP 2MHz | high |
| EEPROM WP | PB13 | Out_PP 2MHz | high |

当前主路径使用 `InitE2PROM()`；低功耗恢复 `Init()` 中使用 `InitE2PROM_i2c()`，配置等价。

### 10.4 AFE legacy IIC

`initAFE1_IIC()` 配置：

| 信号 | 引脚 | 模式 | 初始态 | 当前状态 |
|---|---|---|---|---|
| AFE IIC/Sleep pins | PB8/PB9 | Out_PP 2MHz | high | 正常 `InitDevice()` 不调用 |
| PRE MOS/CMNT | PC12 | Out_PP 2MHz | 未显式写 | `Init()` 低功耗恢复路径调用 |

当前 AFE 数据读取已主要走 `sh36735_read_regs()` 的 SPI 协议，不应再把 PB8/PB9 当作正常主路径必配外设。

## 11. ADC、DMA 与模拟量

当前 `InitDevice()` 中 `InitADC()` 被注释，主循环 `App_AnlogCal()` 也被注释，因此 ADC1/TIM2/DMA1 当前不是正常上电运行外设。

若恢复启用，当前代码配置如下：

| 模块 | 配置 |
|---|---|
| GPIO | PA1 `GPIO_Mode_AIN` |
| TIM2 | APB1 TIM2 enable，PWM1 on CH2，Period 999 |
| ADC1 | Independent，scan enable，continuous disable，external trigger `TIM2_CC2` |
| ADC clock | `RCC_PCLK2_Div8`，当前为 1MHz |
| DMA | DMA1 Channel1，peripheral `ADC1->DR` to `g_u16ADCValFilter[0]`，HalfWord，circular，high priority |
| Channel count | `AD_Used_amount = 1` |
| ADC channel | ADC_Channel_1，rank 1，sample 55.5 cycles |
| ADC interrupt | DMA interrupt配置代码在注释块内，当前不启用 |

风险点：ADC 代码里的注释仍按 72MHz/9MHz 口径写，但当前时钟是 8MHz。如果后续重新启用 ADC，需要重新确定 TIM2 触发频率和 ADC clock。

## 12. RTC、BKP、PWR 与低功耗

### 12.1 RTC 配置

`Init_RTC()` 当前不在正常 `InitDevice()` 中调用；它由 `rtc_sleep()` 或 `IsSleepStartUp()` 相关路径触发。

`RTC_ClockConfig()`：

| 项 | 当前配置 |
|---|---|
| Backup access | `PWR_BackupAccessCmd(ENABLE)` |
| Backup domain | `BKP_DeInit()` |
| 首选 RTC clock | LSE |
| LSE timeout | `0x5000` loops |
| LSE 成功 | `RCC_RTCCLKSource_LSE`，prescaler `32767`，1Hz |
| LSE 失败 | 打开 LSI，`RCC_RTCCLKSource_LSI`，prescaler `40000-1`，约 1Hz |
| RTC sec interrupt | ENABLE |
| RTC NVIC | `RTC_IRQn` preemption 1，sub 0 |
| RTC Alarm EXTI | EXTI17 rising，`RTCAlarm_IRQn` preemption 0，sub 0 |
| RTC wake alarm | `RTC_GetCounter() + 3` 秒 |

`RTC_IRQHandler()`：

| 条件 | 行为 |
|---|---|
| `RTC_IT_SEC` | 清秒中断，`TimeDisplay=1` |
| `RTC_FLAG_ALR` | 清 alarm，`is_rtc_wakekup=true`，`rtc_cnt++` |

### 12.2 当前主循环低功耗路径

当前 `main()` 调用 `App_SleepDeal()`，没有调用 `sleep()`/`rtc_sleep()`。因此当前主路径更接近 reset-sleep：

1. `App_SleepDeal()` 每 1s 或强制 sleep 标志处理休眠状态。
2. 需要 sleep 时 `SleepDeal_Continue()` 写 `FLASH_ADDR_SLEEP_FLAG`。
3. 调 `AFE_Sleep()`、记录日志，然后 `MCU_RESET()`。
4. 下次上电 `InitDevice()` 早期 `IsSleepStartUp()` 读取 sleep flag。
5. 根据 flag 配 GPIO/EXTI，进入 STOP。
6. 唤醒后调用 `IORecover_*()`，当前实现都是 `MCU_RESET()`。

`IsSleepStartUp()` 三类 flag：

| Flag | 睡前 IO | 唤醒源 | STOP 后恢复 |
|---|---|---|---|
| `FLASH_HICCUP_SLEEP_VALUE` | `IOstatus_RTCMode()` | `InitWakeUp_RTCMode()` | `IORecover_RTCMode()` reset |
| `FLASH_NORMAL_SLEEP_VALUE` | `IOstatus_NormalMode()` | `InitWakeUp_NormalMode()` | `IORecover_NormalMode()` reset |
| `FLASH_DEEP_SLEEP_VALUE` | `IOstatus_DeepMode()` | `InitWakeUp_DeepMode()` | `IORecover_DeepMode()` reset |

### 12.3 `rtc_sleep()` 保留路径

`rtc_sleep()` 代码存在，但当前主循环注释了 `sleep()` 调用。若未来启用，HICCUP 路径大致为：

1. `Init_RTC()`
2. `IOstatus_RTCMode()`
3. `InitWakeUp_RTCMode()`
4. `Sys_StopMode()`
5. STOP 唤醒后关闭部分 EXTI/RTC alarm
6. 调 `Init()` 重新配置串口、CAN、TIM3、AFE IIC、EEPROM I2C
7. 检查 AFE/电压/电流/保护，决定继续 RTC sleep 或退出

当前源码没有 `RtcSleep_PortRestoreAfterStop()` 这类封装，STOP 后恢复是直接调用 `Init()`。

### 12.4 低功耗 IO 状态

`IOstatus_RTCMode()`：

| 端口 | 配置 |
|---|---|
| GPIOA | 全部 AIN |
| GPIOB | 全部 AIN，但排除 PB14 |
| GPIOC | 全部 AIN |
| GPIOD | 全部 AIN |
| GPIOE | 全部 AIN |
| PC12 | Out_PP 2MHz，`MCUO_DRV_CMNT=1` |
| PC13 | Out_PP 2MHz，`MCUO_PWSV_CTR=1` |
| PD2 | Out_PP 2MHz，`MCUO_PWSV_STB=0` |
| ADC1 | `ADC_DeInit(ADC1)` |
| Delay | `__delay_ms(100)` |

`InitWakeUp_Base()` 配置：

| 唤醒源 | 引脚 | EXTI | 触发 | NVIC |
|---|---|---|---|---|
| MCU wake | PA0 | EXTI0 | rising | 1/1 |
| KEY1 | PB5 | EXTI5 | falling | EXTI9_5 1/1 |

`InitWakeUp_NormalMode()` 追加：

| 唤醒源 | 引脚 | EXTI | 触发 | NVIC |
|---|---|---|---|---|
| CMNT/RS485 wake | PB12 | EXTI12 | rising | EXTI15_10 1/1 |

`InitWakeUp_RTCMode()` 再追加 RTC alarm：当前 3 秒后 alarm。

## 13. IWDG

当前 `wdog_enable` 已定义，`InitDevice()` 会调用 `Init_IWDG()`。

| 项 | 当前配置 |
|---|---|
| Clock | LSI，名义 40kHz |
| PWR clock | `RCC_APB1Periph_PWR` enable |
| Prescaler | `IWDG_Prescaler_64`，因为 `__FUNC_RTC__` 未定义 |
| Reload | `800` |
| 估算 timeout | 约 1.28s |
| 首次 feed | 初始化中 feed 后 enable |
| Debug | 设置 DBGMCU IWDG/WWDG stop bits |
| 喂狗位置 | `__delay_ms()` 循环、`rtc_sleep()` STOP 前后、主循环末尾 |

如果未来定义 `__FUNC_RTC__`，IWDG 分支会变为 prescaler 256、reload `0x0FFF`，timeout 会显著变长。

## 14. 中断与 NVIC 汇总

`InitNVIC()` 设置 `NVIC_PriorityGroup_1`。

| 中断 | 当前是否启用 | 优先级 | 触发源 | 处理 |
|---|---|---|---|---|
| TIM3 | 启用 | 0/3 | TIM3 update | 1ms tick |
| USART1 | 启用 | 3/3 | RXNE、EIE相关错误 | SCI1 接收解析 |
| USART2 | 启用 | 3/3 | RXNE、EIE相关错误 | SCI2 接收解析 |
| USART3 | 宏关闭 | 3/3 代码存在 | RXNE | 当前未启用 |
| CAN1 RX0 | NVIC 启用但 CAN RX interrupt disabled | 1/1 | FIFO0 message pending | 正常靠轮询，ISR存在 |
| RTC | 仅 RTC 初始化后启用 | 1/0 | second/alarm flag | 秒标志、alarm wake |
| RTCAlarm | 首次 RTC init 配置 | 0/0 | EXTI17 rising | 清 EXTI17 |
| EXTI0 | 睡眠/充电检测路径启用 | 1/1 | PA0 rising 或 falling，取决于调用路径 | 清 pending，部分路径置 charger flag |
| EXTI9_5 | 睡眠路径启用 | 1/1 | PB5 falling 等 | 清 pending |
| EXTI15_10 | 睡眠 Normal/RTC 路径启用 | 1/1 | PB12 rising | 清 pending |
| DMA1_Channel1 | 当前未启用 | 注释内 | ADC DMA | 未启用 |

## 15. 当前外设状态总表

| 外设 | 当前主路径状态 | 主要配置 | 备注 |
|---|---|---|---|
| RCC/SystemClock | 启用 | HSE 8MHz，AHB/APB div1，无 PLL | 旧注释里的 72MHz 不成立 |
| Flash/IAP | 启用 | App `0x08004800`，mailbox `0x20004FE0` | App 请求 IAP 走 SRAM mailbox |
| GPIOA-E | 启用 | AFIO + GPIOA/B/C/D/E clock | 多个引脚有复用别名 |
| USART1 | 启用 | PB6/PB7，19200 8N1 | 上位机协议 |
| USART2 | 启用 | PA2/PA3，19200 8N1 | 上位机协议 |
| USART3 | 未启用 | full remap PD8/PD9 代码存在 | 宏关闭 |
| CAN1 App | 启用 | PA11/PA12，500kbit/s | 与 IAP 250k 不一致 |
| CAN1 IAP | IAP 工程启用 | PA11/PA12，250kbit/s，扩展帧 | 上位机升级使用 |
| TIM3 | 启用 | 1ms update | 系统调度节拍 |
| SysTick | 启用 | HCLK/8，阻塞 delay | ISR 空 |
| IWDG | 启用 | prescaler 64，reload 800 | 主循环喂狗 |
| ADC1 | 未启用 | 保留：PA1，TIM2_CC2，DMA1_CH1 | 重启用前需重算时钟 |
| TIM2 | 未启用 | ADC trigger 保留 | `InitADC()` 未调用 |
| DMA1_CH1 | 未启用 | ADC circular DMA 保留 | 中断未启用 |
| RTC | 条件启用 | LSE优先/LSI兜底，1Hz，alarm+3s | sleep 路径使用 |
| PWR/STOP | 条件启用 | STOP low-power regulator WFI | reset-sleep 和 rtc_sleep 使用 |
| BKP | 条件启用 | DR1 标志 | RTC init 使用 |
| EEPROM I2C | 启用 | PB10/PB11/PB13 软件 I2C/WP | 正常初始化 |
| SH36735 SPI | 启用 | PA4/5/6/7 软件 SPI | 主 AFE 通信 |
| SH36735 HW SPI | 未启用 | SPI1 mode 3 代码存在 | 未调用 |
| Heat/Cool | 未启用 | PC6 代码存在 | `__FUNC__HEAT__` 关闭 |
| LED bar | 未启用 | PA5/6/7/PB1/PC13 代码存在 | 与 SPI 引脚冲突，主循环未调用 |

## 16. 关键风险与建议确认项

1. App CAN 与 IAP CAN 位率不一致。当前 App 是 500kbit/s，IAP 是 250kbit/s。若上位机“参数读写”和“升级”要无感切换，需要统一设计。
2. 当前 App 工程注释大量保留 72MHz/9MHz 口径，但源码实际是 8MHz。后续不要按注释推导外设频率。
3. ADC 链路未启用；如果未来重新打开，TIM2 触发频率和 ADC clock 必须按 8MHz 重新确认。
4. LED bar 引脚 PA5/PA6/PA7 与当前 SH36735 软件 SPI 冲突。当前 LED bar 未启用，所以没有运行冲突；如果要恢复 LED，必须重新分配或仲裁。
5. PB4、PB5、PB14 在头文件中有多个语义别名，当前配置以实际调用路径为准：PB4 是 CMNT_EN，PB5 是 KEY/DI 输入，PB14 是 M_CCC/AFE1_CTL。
6. `RTC_ClockConfig()` 内部 `BKP_DeInit()` 会重置 backup domain；如果后续依赖 BKP 保存更多状态，需要重新审查。
7. `Sys_StopMode()` 依赖 PWR 时钟，但函数自身打开 PWR 时钟的语句被注释；当前因 `Init_IWDG()` 先打开 PWR 且 watchdog 启用而能覆盖。若关闭 watchdog，要重新确认 STOP 前 PWR clock。

## 17. 本次核对的主要源码入口

| 模块 | 文件 |
|---|---|
| 时钟/向量表 | `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c` |
| Keil target | `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` |
| 主流程 | `103 + 309/Project/Source/main.c` |
| 宏配置 | `103 + 309/Project/Source/main.h`、`103 + 309/Project/Source/conf/conf.h` |
| GPIO/STOP | `103 + 309/Project/Source/conf/conf.c`、`conf_gpio.h` |
| 定时/IWDG | `103 + 309/Project/Source/System_Init.c`、`System_Init.h` |
| 串口 | `103 + 309/Project/Source/Sci_Upper.c`、`Sci_Upper.h` |
| CAN | `103 + 309/Project/Source/Can_HDX.c`、`Can_HDX.h` |
| ADC | `103 + 309/Project/Source/ADC.c`、`ADC.h` |
| RTC/睡眠 | `103 + 309/Project/Source/RTC.c`、`SleepDeal.c`、`rtc_sleep.c` |
| SPI/AFE | `bsp_spi_bus.c`、`sh36735_spi_sw.c`、`sh36735_spi_hw.c`、`I2C_AFE1.c` |
| EEPROM | `103 + 309/Project/Source/EEPROM.c` |
| IAP | `firmware/bms_iap_f103c8/source/iap/*.c`、`bms_iap_config.h` |

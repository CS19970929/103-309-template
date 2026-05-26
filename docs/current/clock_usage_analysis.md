# 当前项目系统时钟用法分析

> ClockAgent 第一阶段输出。范围：只读源码；未修改源码。本文只覆盖系统时钟初始化、STOP 唤醒后的时钟恢复链路，以及和时钟强相关的外设初始化点。

## 1. MCU 与工程配置

- 当前主工程是 STM32F1 标准外设库工程，不是 HAL 工程。Keil 工程 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` 的 `FD_Release` 目标声明 `<Device>STM32F103C8</Device>`，编译宏为 `STM32F10X_MD,USE_STDPERIPH_DRIVER`，见该文件第 17 行、第 340 行；`FD_Debug` 同样声明 `STM32F103C8`，调试目标额外定义 `PROJECT_CFG_BUILD_PROFILE=1,PROJECT_CFG_DEBUG_WATCH_ENABLE=1,_DEBUG_`，见第 954 行、第 1277 行。
- 启动文件使用 `startup_stm32f10x_hd.s`，但 C 编译宏是 `STM32F10X_MD`。Keil 文件列表见 `CommomSH367309_16series_103RCT6_C.uvprojx` 第 613-615 行、第 1550-1552 行；启动文件 `Reset_Handler` 在进入 `__main` 前调用 `SystemInit()`，见 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s` 第 147-154 行。
- `PROJECT_CFG_IAP_ENABLE` 在 `103 + 309/Project/Source/conf/Project_Config.h` 第 159 行为 `1`；`103 + 309/Project/Source/conf/conf.h` 第 131-133 行据此定义 `_IAP`。`system_stm32f10x.c` 通过 `main.h` 引入 `conf.h`，因此 `_IAP` 会影响向量表偏移选择。

## 2. 上电系统时钟初始化

- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c` 第 212-262 行的 `SystemInit()` 会先把 RCC 复位到默认状态，打开 HSI，清除 HSE/CSS/PLL，再调用 `SetSysClock()`。
- 同一文件第 106-116 行当前启用的是 `#define SYSCLK_FREQ_HSE HSE_VALUE`，`SYSCLK_FREQ_72MHz` 被注释掉。因此当前目标不是 PLL 72 MHz，而是 HSE 直接作为 SYSCLK。
- `HSE_VALUE` 没有在 Keil `<Define>` 中覆盖；默认值来自 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h` 第 115-120 行，非 Connectivity Line 为 `8000000`。因此源码层面的 `SystemCoreClock` 目标是 8 MHz。
- `SetSysClock()` 在 `system_stm32f10x.c` 第 423-437 行根据 `SYSCLK_FREQ_HSE` 调用 `SetSysClockToHSE()`；`SetSysClockToHSE()` 第 504-568 行启用 HSE、等待 `HSERDY`，然后配置：
  - `HCLK = SYSCLK`，见第 552-553 行。
  - `PCLK2 = HCLK`，见第 555-556 行。
  - `PCLK1 = HCLK`，见第 558-559 行。
  - `SYSCLK = HSE`，见第 561-568 行。
- `SystemCoreClock` 在 `system_stm32f10x.c` 第 151-164 行根据编译宏初始化；当前路径是 `SystemCoreClock = SYSCLK_FREQ_HSE`，即 `HSE_VALUE`。
- `SystemInit()` 除了配置时钟，还会设置向量表。当前 `_IAP` 开启时，`SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET`，`VECT_TAB_OFFSET` 为 `0x4800`，见 `system_stm32f10x.c` 第 128 行、第 264-272 行。
- 应用层启动又手动调用一次 `SystemInit()`：`103 + 309/Project/Source/AppInit.c` 第 7-16 行中 `AppInit_InitDevice()` 第 9 行调用 `SystemInit()`，随后第 15 行调用 `InitDelay()`。这和启动文件中的 `SystemInit()` 重复，但当前未发现直接破坏行为。

## 3. 是否已有 SystemClock_Config / Clock_ReConfigAfterStop

- 未找到 HAL 风格 `SystemClock_Config()`。
- 未找到独立命名的 `Clock_ReConfigAfterStop()`、`Clock_RestoreAfterStop()` 或 `BSP_Clock*` 模块。
- 当前自定义 Stop 后时钟恢复入口是 `cpu_frequency_conf()`：`103 + 309/Project/Source/rtc_sleep_port.c` 第 207-212 行调用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`。
- `cpu_frequency_conf()` 在 `103 + 309/Project/Source/rtc_sleep.h` 第 67 行和 `103 + 309/Project/Source/rtc_sleep_port.h` 第 47 行导出，说明它已被作为低功耗端口层能力使用。

## 4. STOP 进入与唤醒后的当前恢复链路

当前 HICCUP RTC STOP 路径如下：

1. `103 + 309/Project/Source/rtc_sleep.c` 第 303-321 行的 `rtc_sleep_run_hiccup_cycle()` 调用 `rtc_sleep_prepare_rtc()`，再调用 `RtcSleep_PortEnterStop()`，STOP 返回后调用 `RtcSleep_PortDisableStopWakeup()` 和 `RtcSleep_PortRestoreAfterStop()`。
2. `103 + 309/Project/Source/rtc_sleep_port.c` 第 108-123 行中，`RtcSleep_PortPrepareRtcStop()` 调用 `Init_RTC()`、`IOstatus_RTCMode()`、`InitWakeUp_RTCMode()`；`RtcSleep_PortEnterStop()` 在 `Sys_StopMode()` 前后喂 IWDG。
3. `103 + 309/Project/Source/conf/conf.c` 第 374-385 行的 `Sys_StopMode()`：
   - 使能 PWR 时钟，见第 376 行。
   - 临时使能 TIM3，停止 TIM3 并清 pending，再关闭 TIM3 时钟，见第 377-380 行。
   - 清 EXTI/NVIC 唤醒 pending，见第 381 行。
   - 调用 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`，见第 382 行。
   - 从 WFI 返回后立即调用 `cpu_frequency_conf()`，见第 384 行。
4. 标准外设库 `PWR_EnterSTOPMode()` 会设置 `SLEEPDEEP`、执行 `__WFI()`、返回后清 `SLEEPDEEP`，见 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_pwr.c` 第 197-229 行。
5. `RtcSleep_PortRestoreAfterStop()` 在 `103 + 309/Project/Source/rtc_sleep_port.c` 第 131-134 行调用 `InitRunAfterStopWakeup()`；该函数位于 `conf.c` 第 392-421 行，依次恢复 `InitDelay()`、`RTC_RestoreRunInterrupts()`、`InitIO_rtc()`、`ADC_StopForLowPower()`、`InitADC()`、串口、CAN、TIM3 和 AFE I2C。

旧的复位式睡眠启动路径也会经过 `Sys_StopMode()`：`103 + 309/Project/Source/SleepDeal.c` 第 186-230 行在 `FLASH_HICCUP_SLEEP_VALUE`、`FLASH_NORMAL_SLEEP_VALUE`、`FLASH_DEEP_SLEEP_VALUE` 分支中循环调用 `Sys_StopMode()`，有效唤醒后调用 `IORecover_*()`；而 `IORecover_RTCMode()`、`IORecover_NormalMode()`、`IORecover_DeepMode()` 都执行 `MCU_RESET()`，见 `conf.c` 第 359-371 行。

## 5. 和时钟强相关的外设依赖点

- `InitDelay()` 使用 `SystemCoreClock / 8000000` 计算 SysTick 延时系数，见 `103 + 309/Project/Source/System_Init.c` 第 132-137 行。因此 STOP 返回后必须先更新 `SystemCoreClock`，再使用阻塞延时。
- `InitTimer()` 使用 `Timer_GetPrescalerFor100kHz()`，后者按 `SystemCoreClock / 100000` 计算 TIM3 预分频，见 `System_Init.c` 第 59-74 行、第 100-127 行；TIM3 中断在第 293-299 行生成 10 ms tick。
- `InitADC_TIMER()` 使用 `SystemCoreClock / 100000` 计算 TIM2 触发时基，见 `103 + 309/Project/Source/ADC.c` 第 149-160 行；`InitADC_ADC1()` 使用 `RCC_ADCCLKConfig(RCC_PCLK2_Div8)`，见第 215-232 行。
- `InitCan_CAN1()` 使能 `RCC_APB1Periph_CAN1` 并配置 `CAN_BS1_5tq`、`CAN_BS2_2tq`、`CAN_Prescaler = 4`，见 `103 + 309/Project/Source/Can_HDX.c` 第 825-843 行。按当前源码时钟 `PCLK1 = 8 MHz` 计算，CAN bit rate 为 `8 MHz / 4 / (1 + 5 + 2) = 250 kbit/s`。
- `Sci_Upper.c` 第 1608-1652 行的串口初始化根据 APB1/APB2 时钟使能 USART，并以 `USART_BaudRate = 19200` 调用 `USART_Init()`。STOP 返回后若 APB 时钟树没有恢复，Modbus/RS485 波特率会受影响。
- LED 扫描使用 TIM4，`LedBar_ScanTimerInit()` 使能 `RCC_APB1Periph_TIM4` 并按 `LedBar_GetTimerPrescalerFor100kHz()` 设置预分频，见 `103 + 309/Project/Source/LedBar.c` 第 355-371 行；休眠前 `LedBar_StopScanTimer()` 关闭 TIM4 时钟，见第 392-405 行。

## 6. Stop 唤醒后是否需要恢复 HSE / PLL / SYSCLK / AHB / APB

需要恢复。依据分为官方规则和当前项目规则：

- 官方规则：ST RM0008 对 STM32F101/F102/F103/F105/F107 的 STOP 模式说明指出，STOP 模式中 1.8 V 域时钟停止，PLL、HSI、HSE 振荡器关闭；通过中断或唤醒事件退出 STOP 时，HSI RC 被选择为系统时钟。RM0008 的 RCC `SW[1:0]` 位说明也指出，离开 Stop/Standby 时硬件强制选择 HSI。官方资料入口：ST RM0008《STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx reference manual》，https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf 。
- 当前项目运行目标不是 HSI，而是 `SYSCLK_FREQ_HSE`。`system_stm32f10x.c` 第 106-116 行选择 HSE，`SetSysClockToHSE()` 第 552-568 行配置 HCLK/PCLK1/PCLK2 都等于 HSE。因此 STOP 返回后必须恢复到 HSE 直驱 SYSCLK，并恢复 AHB/APB 分频。
- 当前项目没有启用 PLL，`SYSCLK_FREQ_72MHz` 在 `system_stm32f10x.c` 第 115 行被注释，所以现阶段不需要恢复 PLL 到运行态；但如果后续改回 72 MHz，必须在 Stop 返回后恢复 PLL，并重新确认 APB1 不超过 36 MHz、CAN 位时序和 ADC 分频。

## 7. 当前实现的优点与缺口

### 已有优点

- `Sys_StopMode()` 在 `PWR_EnterSTOPMode()` 返回后立即调用 `cpu_frequency_conf()`，时钟恢复点位于外设恢复之前，顺序正确，见 `conf.c` 第 382-385 行和 `rtc_sleep.c` 第 310-321 行。
- `cpu_frequency_conf()` 会调用 `SystemCoreClockUpdate()` 和 `InitDelay()`，避免 SysTick 延时系数长期停留在旧值，见 `rtc_sleep_port.c` 第 207-212 行。
- `InitRunAfterStopWakeup()` 在时钟恢复后统一恢复 ADC、USART、CAN、TIM3、AFE I2C，见 `conf.c` 第 392-421 行；这符合先恢复系统时钟、再恢复依赖 APB/AHB 外设的顺序。

### 主要缺口

- 当前 Stop 后恢复函数直接复用 `SystemInit()`，不是专用 `Clock_ReConfigAfterStop()`。`SystemInit()` 同时包含 RCC 复位、HSE 选择和 `SCB->VTOR` 设置，见 `system_stm32f10x.c` 第 212-272 行。低功耗框架后续移植到其他 F0/F1 项目时，建议把“运行时钟配置”和“启动向量表配置”拆开。
- `SetSysClockToHSE()` 的 HSE 启动失败分支为空，见 `system_stm32f10x.c` 第 570-573 行。若 STOP 后 HSE 未起振，系统会继续执行；`SystemCoreClockUpdate()` 可根据实际 `SWS` 更新为 HSI，但当前没有错误标记，也没有阻止 CAN/USART 恢复。
- Keil 工程 `<Cpu>` 字段里有 `CLOCK(12000000)`，见 `CommomSH367309_16series_103RCT6_C.uvprojx` 第 21 行、第 958 行；源码 `HSE_VALUE` 默认为 8 MHz。该字段不等同于编译宏，但提示需要在硬件资料中确认晶振实际频率。若实际 HSE 不是 8 MHz，`SystemCoreClock`、USART 波特率、TIM2/TIM3/TIM4、ADC 触发、CAN 位时序都会偏离。
- `System_Init.c` 第 59 行注释仍写“所以为 72MHz”，`ADC.c` 第 232 行注释仍写 PCLK2/8 为 9 MHz；按当前 `SYSCLK_FREQ_HSE = 8 MHz`，这些注释已经不是当前源码事实。

## 8. 第一阶段结论

- 当前项目已有可工作的 Stop 后时钟恢复链路：`Sys_StopMode()` 返回后调用 `cpu_frequency_conf()`，恢复 `SystemInit()` 目标时钟并更新 `SystemCoreClock`。
- 当前项目没有 `SystemClock_Config()`，也没有独立 `Clock_ReConfigAfterStop()`；后续低功耗框架应新增专用 `bsp_clock` 或同等模块，而不是继续让业务低功耗层直接调用 `SystemInit()`。
- 当前源码运行时钟目标是 HSE 直驱 8 MHz、HCLK/PCLK1/PCLK2 全 1 分频、无 PLL。Stop 唤醒后必须恢复 HSE/SYSCLK/AHB/APB；现阶段无需恢复 PLL，但要保留未来 72 MHz/PLL 配置的扩展点。
- 后续实现前必须确认硬件 HSE 实际频率，并同步修正 `HSE_VALUE` 或 Keil 编译宏；否则 CAN、USART、TIM 和 ADC 的时间参数没有可靠依据。

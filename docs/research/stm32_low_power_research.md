# STM32F0/F1 RTC 低功耗官方资料调研

更新时间：2026-05-26  
阶段：第一阶段，只读资料调研  
适用范围：当前 BMS 项目后续低功耗框架设计，目标 MCU 覆盖 STM32F0/F1，优先标准外设库或寄存器实现，不依赖 HAL。

## 资料来源

本文件只整理和 BMS 保护板低功耗框架直接相关的官方规则。CurrentProjectAgent 后续再把这些规则和当前源码逐项对照。

| 类别 | 官方资料 | 章节线索 | 本项目需要关注的点 |
|---|---|---|---|
| F1 Reference Manual | ST RM0008：STM32F101xx/102xx/103xx/105xx/107xx advanced Arm-based 32-bit MCUs，https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf | Power control (PWR) / Low-power modes；Reset and clock control (RCC)；Backup domain；RTC；EXTI；IWDG | F1 Stop/Standby 进入退出条件、RTC Alarm 通过 EXTI17 唤醒、Stop 唤醒后系统时钟变为 HSI、IWDG 在 Stop/Standby 中仍工作 |
| F0 Reference Manual | ST RM0091：STM32F0x1/STM32F0x2/STM32F0x8 advanced Arm-based 32-bit MCUs，https://www.st.com/resource/en/reference_manual/rm0091-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf | PWR；RCC；RTC/TAMP；EXTI；IWDG | F0 Stop/Standby、RTC Alarm/Wakeup、Stop 后 HSI 作为系统时钟、IWDG 使用 LSI |
| F030/F070 Reference Manual | ST RM0360：STM32F030x4/x6/x8/xC and STM32F070x6/xB advanced Arm-based 32-bit MCUs，https://www.st.com/resource/en/reference_manual/rm0360-stm32f030x4x6x8xc-and-stm32f070x6xb-advanced-armbased-32bit-mcus-stmicroelectronics.pdf | PWR 6.3；RCC 7.2；RTC；EXTI；IWDG | F030/F070 的 RTC periodic wakeup 支持范围不同，不能把所有 F0 都当成有 Wakeup Timer |
| Cortex-M3 编程手册 | ST PM0056：STM32F10xxx/20xxx/21xxx/L1xxxx Cortex-M3 programming manual，https://www.st.com/resource/en/programming_manual/pm0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf | Power management；System control register SCR；SysTick | WFI/WFE、SLEEPDEEP、Sleep 与 Deep-sleep 的核心侧语义，SysTick 属于 Cortex 核心外设 |
| Cortex-M0 编程手册 | ST PM0215：STM32F0 series Cortex-M0 programming manual，https://www.st.com/resource/en/programming_manual/pm0215-stm32f0-series-cortexm0-programming-manual-stmicroelectronics.pdf | 2.5 Power management；4.3.4 SCR；SysTick | Sleep 停 CPU 时钟，Deep-sleep 对应产品级 Stop/Standby；WFI/WFE 会被中断/事件唤醒，也可能有调试造成的伪唤醒 |
| RTC 应用笔记 | ST AN4759：Introduction to using the hardware real-time clock (RTC) and the tamper management unit (TAMP) for STM32 MCUs，https://www.st.com/resource/en/application_note/an4759-introduction-to-using-the-hardware-realtime-clock-rtc-and-the-tamper-management-unit-tamp-with-stm32-mcus-stmicroelectronics.pdf | 2.4 Wakeup unit；2.4.1 Program the auto-wakeup unit；Alarm 章节 | F0 等新 RTC 的 Wakeup Timer 配置顺序：关写保护、关 WUTE、等 WUTWF、写 WUTR/WUCKSEL、清标志、使能中断/计数 |
| F0 RTC 应用笔记 | ST AN3371：Using the hardware real-time clock (RTC) in STM32 F0/F2/F3/F4 and L1 series of MCUs，https://www.st.com/resource/en/application_note/an3371-using-the-hardware-realtime-clock-rtc-in-stm32-f0-f2-f3-f4-and-l1-series-of-mcus-stmicroelectronics.pdf | RTC clock/calendar；Alarm；Wakeup timer；SPL API 分组 | F0 RTC 是 calendar RTC，Alarm 和 Wakeup Timer 是不同路径；移植接口不能照搬 F1 32-bit counter 模型 |
| F1 RTC 校准 | ST AN2604：STM32F101xx and STM32F103xx RTC calibration，https://www.st.com/resource/en/application_note/an2604-stm32f101xx-and-stm32f103xx-rtc-calibration-stmicroelectronics.pdf | RTC calibration basics；Crystal accuracy；Methodology | F1 RTC 精度和 LSE 晶体误差相关；第一版可不做校准，但休眠时间统计和 SOC 静置判断不能假设 LSI 精确 |
| 晶振设计 | ST AN2867：Guidelines for oscillator design on STM8AF/AL/S and STM32 MCUs/MPUs，https://www.st.com/resource/en/application_note/an2867-guidelines-for-oscillator-design-on-stm8afals-and-stm32-mcusmpus-stmicroelectronics.pdf | LSE/HSE Pierce oscillator；Recommended low-speed resonators；PCB design guidelines；LSE sensitivity to PC13 activity | LSE 可靠性是 RTC 唤醒可靠性的硬件前提；等待 LSERDY 必须有超时，PC13/32 kHz 区域布局和负载电容会影响量产稳定性 |
| F1 Errata | ST ES0340/ES0348：STM32F103 对应封装容量 errata，示例：https://www.st.com/resource/en/errata_sheet/es0348-stm32f101x46-stm32f102x46-stm32f103x46-device-errata-stmicroelectronics.pdf | System：Debugging Stop mode and SysTick timer；Wakeup sequence from Standby mode when using more than one wakeup source；LSE startup in harsh environments | 调试态和量产态的 Stop/SysTick 表现可能不同；多唤醒源 Standby 要按 errata 规避；LSE 恶劣环境启动失败要作为风险处理 |
| F030 Errata | ST ES0219：STM32F030x4/x6/x8/xC device errata，https://www.st.com/resource/en/errata_sheet/es0219-stm32f030x4x6x8xc-device-errata-stmicroelectronics.pdf | System wake-up sequence from Standby；IWDG：RVU/PVU flag not reset in Stop | F030 低功耗前后不能无条件等待 IWDG PVU/RVU 清零；Standby 多唤醒源要规避 |

## 官方规则提炼

### 1. Sleep、Stop、Standby 的用途边界

PM0056/PM0215 定义了 Cortex 侧的 Sleep 和 Deep-sleep 入口：Sleep 停止处理器执行，Deep-sleep 由芯片产品级 PWR 模块解释为 Stop 或 Standby。对 BMS 项目应这样落地：

- Sleep：适合短暂空闲降功耗，CPU 停止，部分外设时钟可继续运行；任意已使能中断都可能唤醒。它不能替代 RTC 周期低功耗。
- Stop：第一阶段推荐目标。SRAM 和大部分寄存器保持，主时钟域停止，RTC/EXTI/IWDG 等低速域可工作。唤醒后从 WFI/WFE 后继续运行，但必须恢复系统时钟和时基。
- Standby：更接近复位式低功耗。SRAM 和普通外设寄存器丢失，唤醒后走 reset 启动流程，仅保留备份域/状态标志。BMS 首版不应把普通空闲休眠做成 Standby；Standby 更适合过放深度休眠或明确复位式策略。

### 2. Stop 唤醒后必须恢复系统时钟

RM0008、RM0091、RM0360 均给出相同方向的规则：Stop/Deep-sleep 唤醒后，HSI 会作为系统时钟源，HSE/PLL 不保持原运行态。

对当前 BMS 框架的约束：

- Stop 返回后不能直接继续使用基于 72 MHz 或 48 MHz 的 SysTick/TIM/CAN/UART 波特率假设。
- 必须设计 `Clock_ReConfigAfterStop` 或等价接口，恢复 HSE/PLL/SYSCLK/AHB/APB 分频，再恢复 SysTick、TIM、ADC、UART、CAN。
- 第一版不要做 CAN/USART Stop 唤醒，因为通信波特率和时钟恢复窗口会增加协议乱序风险。通信活跃时禁止进入 Stop 更稳。

### 3. RTC 时钟源必须为 Stop/Standby 可用时钟

F1/F0 RTC 时钟通常可选 LSE、LSI 或 HSE 分频；但 Stop/Standby 中 HSE/PLL 会停止。因此用于低功耗唤醒的 RTC 时钟源应优先：

1. LSE 32.768 kHz：精度和长期休眠时间统计最好，但依赖晶体、负载电容、PCB 布局和启动裕量。
2. LSI：不依赖外部晶体，适合保底唤醒；但频率误差大，F1 数据手册中 LSI 典型 40 kHz、范围可到 30-60 kHz，休眠时间和 IWDG 超时都必须按最坏情况计算。

不建议把 HSE 分频作为 Stop/Standby 周期唤醒源。即使运行态 RTC 可以走 HSE 分频，Stop 中 HSE 关闭后无法提供稳定 RTC 唤醒。

### 4. IWDG 在 Stop/Standby 中仍然有效

RM0008 的 IWDG 章节说明 IWDG 使用 LSI，并且位于 Stop/Standby 仍工作的电源域；F0 RM0091/RM0360 也给出 IWDG clock always LSI 的规则。

对 BMS 的直接约束：

- 一旦 IWDG 启动，低功耗期间不能假设它暂停。
- RTC 周期唤醒时间必须小于 IWDG 最短超时时间；若使用未校准 LSI，必须按 LSI 最高频率计算最短超时。
- F1 以 40 kHz 标称、prescaler 256、reload 4095 时理论最大约 26.2 s；若 LSI 为 60 kHz，最短只有约 17.5 s。设计默认唤醒周期建议小于最短超时的 50%-70%，并在睡前立即喂狗。
- 若需要超过 IWDG 超时的深度休眠，只能设计成 Standby/复位式策略，并在文档中明确启动恢复路径。

### 5. SysTick 不能作为 Stop 周期唤醒源

PM0056/PM0215 把 SysTick 归入核心侧定时器。Sleep 模式下具体行为取决于处理器时钟和实现，Stop/Deep-sleep 中大部分系统和外设时钟停止，不能指望 SysTick 继续计时。

对 BMS 的直接约束：

- 10 ms/100 ms/1 s 软件任务节拍在 Stop 中会暂停。
- 唤醒后必须用 RTC 休眠秒数补偿业务时间，例如 SOC 静置时间、保护延时、通信超时窗口，而不是让 SysTick 在 Stop 中自然累加。
- 进入 Stop 前建议暂停或屏蔽周期 SysTick 唤醒，避免刚进入低功耗就被普通节拍打醒。

### 6. F1 RTC 是旧式 counter + Alarm，不是 F0 calendar RTC

STM32F1 的 RTC 主要是 32-bit counter、prescaler 和 Alarm。低功耗周期唤醒应基于 RTC counter 加 Alarm：

- Stop 唤醒：RTC Alarm 事件需要走 EXTI Line 17，上升沿触发，配合 RTC Alarm 中断/事件。
- Standby 唤醒：RTC Alarm 可唤醒，但不需要配置 EXTI Line 17；唤醒后是 reset 流程。
- 配置 RTC/备份域前必须开启 PWR/BKP 时钟，并允许访问备份域。
- `RTC_WaitForSynchro`、`RTC_WaitForLastTask`、等待 LSERDY/LSIRDY 都必须加超时；BMS 量产代码不能因为 LSE 未起振而死等。

### 7. F0 RTC 是 calendar RTC，Alarm 和 Wakeup Timer 要分型号处理

F0 系列不应只按一个“F0 RTC”实现。RM0091 覆盖 F0x1/F0x2/F0x8，RM0360 覆盖 F030/F070 值线。关键差异：

- RTC Alarm 通常通过 EXTI Line 17 唤醒 Stop。
- RTC Wakeup event 在部分 F0 中通过 EXTI Line 20；RM0360 明确 STM32F070xB、STM32F030xC 支持 RTC periodic wakeup，STM32F030x4/x6/x8 不支持该 periodic wakeup 功能。
- 因此可复用框架必须有编译期能力宏：若目标芯片支持 RTC Wakeup Timer，优先用 Wakeup Timer；否则回退到 Alarm。

### 8. 进入 Stop 前必须清 pending 状态

RM0091/RM0360 对 Stop 入口有明确提示：进入 Stop 前要处理 EXTI pending、外设中断 pending、RTC Alarm flag 等，否则会立即唤醒或根本进不去。

对 BMS 的直接约束：

- RTC 唤醒源配置完成后，清 RTC 对应 flag，再清 EXTI_PR 对应 line。
- 普通通信、按键、LED 扫描、TIM、ADC、AFE 中断如果仍 pending，应先处理或作为禁止休眠原因。
- 低功耗入口函数不要只调用 `PWR_EnterSTOPMode`；必须有统一的 BeforeSleep 检查和清标志顺序。

### 9. LSE 可靠性要按硬件风险处理

AN2867 和 F1/F0 errata 都提示 LSE 启动和稳定性不是纯软件问题。对 BMS 项目：

- 如果板子没有可靠 32.768 kHz LSE，软件应允许 LSI fallback。
- 等待 LSE ready 必须有超时，并记录降级原因。
- 若 PC13 或 32 kHz 晶体附近有 LED/按键/Charlieplexing 高翻转信号，后续硬件审查要对照 AN2867 的 LSE PCB 和 PC13 敏感性建议。

## 对 BMS 保护板的第一阶段结论

1. 第一版低功耗框架应选择 Stop + RTC 周期唤醒，不应直接追求 Standby 最低电流。
2. F1 推荐以 RTC Alarm + EXTI17 做周期唤醒；F0 需按芯片能力选择 Wakeup Timer 或 Alarm。
3. RTC 初始化和低功耗入口的所有等待点必须有超时，尤其是 LSERDY/LSIRDY、RTC 同步、RTC 写完成、F0 WUTWF。
4. IWDG 是硬约束：RTC 周期必须小于 IWDG 最短超时，睡前喂狗，醒后先恢复时钟并尽快回到主循环喂狗路径。
5. Stop 唤醒后系统时钟不是原来的 PLL/HSE 配置，必须恢复时钟后再恢复 SysTick、TIM、ADC、UART、CAN 和 LED 扫描。
6. 通信活跃、Flash 擦写/参数保存、AFE 忙、故障处理、升级流程、LED 显示窗口都应作为禁止休眠原因，而不是在 Stop 中尝试复杂唤醒。
7. Standby 应作为单独策略，主要服务过放深度休眠；它会 reset 式启动，必须提前持久化 SOC/保护/日志/唤醒原因。


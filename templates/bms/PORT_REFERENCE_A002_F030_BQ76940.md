# A002 F030 + BQ76940 Port 参考清单

## 使用边界

A002 旧项目只作为 port 参考源，不作为应用层模板。后续生成 `STM32F030 + BQ76940` 工程时，业务逻辑仍来自当前 `103 + 309` 应用层模板，只替换 MCU port、AFE port、Keil/scatter 和配置头。

## 可参考内容

| 类别 | 参考路径 | 用途 |
| --- | --- | --- |
| F0 startup/system | `templates/bms/sources/a002_f030_bq76940/Code/Drivers/` | `startup_stm32f0xx.s`、`system_stm32f0xx.c`、中断文件和 F0 CMSIS 头文件。 |
| F0 StdPeriph | `templates/bms/sources/a002_f030_bq76940/Code/STM32F0xx_StdPeriph_Driver/` | F0 标准外设库源码和头文件。 |
| F0 Flash/IAP | `Code/Source/Flash.c`、`Code/Include/Flash.h` | F0 内部 Flash 写入、IAP App 起始地址、scatter 地址参考。 |
| F0 RTC/低功耗寄存器 | `Code/Source/RTC.c`、`Code/Source/SleepDeal.c` | 只参考 F0 RTC、PWR、EXTI、Stop/Standby 寄存器用法，不继承旧状态机。 |
| BQ76940 AFE | `Code/Source/I2C_AFE1.c`、`Code/Source/BQ769X0_Func.c` | BQ76940 I2C、采样、MOS 控制、SCD/OCD latch 状态参考。 |
| Keil/F0 地址 | `CommomBQ769x0_16series_030C8T6_C.uvprojx`、`Objects/*.sct` | F0 Keil Target、scatter、App/Storage 地址参考。 |
| F0 profile 配置 | `Code/Include/Project_Template_Config.h` | F0/BQ76940 地址和 feature/profile 字段参考。 |

## 禁止继承内容

| 旧模块 | 原因 |
| --- | --- |
| A002 `main.c` 调度 | 通用模板主循环已经由当前项目 `Runtime.c` 负责。 |
| A002 `SOC.c` / `SocEnhance.c` 应用算法 | SOC 体验、低压末端、静置/RTC 策略以当前项目为准。 |
| A002 `SleepDeal.c` 状态机 | 只参考 F0 寄存器，低功耗流程以当前项目 RTC/CAN/LowPower 闭环为准。 |
| A002 `Fault.c` 应用保护流程 | 只参考 BQ76940 软件保护 owner 需要的数据，不复制旧流程。 |
| A002 `Sci_Upper.c` 协议应用流程 | 通信协议以当前项目为基线，后续只按项目需求生成接口。 |

## 生成器处理规则

1. 选择 `a002_f030_bq76940` profile 时，生成器应从当前应用层模板复制业务模块。
2. MCU port 使用 F0 startup、system、StdPeriph、Flash/IAP、RTC/EXTI/Watchdog 参考。
3. AFE port 使用 BQ76940 I2C/寄存器访问参考，并适配到统一 AFE port API。
4. 生成报告必须列出 F0 地址：IAP `0x08000000`、App `0x08001C00`、Storage `0x0800E000..0x0800FFFF`。
5. 外部 EEPROM 后端不生成、不保留。

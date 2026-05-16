# 构建配置与条件编译

## 工程入口

| 项目 | 说明 |
| --- | --- |
| Keil 工程 | `CommomBQ769x0_16series_030C8T6_C.uvprojx` |
| 目标名 | `Target 1` |
| MCU | `STM32F030C8` |
| 库 | STM32F0 Standard Peripheral Library |
| Scatter | `Objects/CommomBQ769x0_16series_030C8T6_C.sct` |
| 应用入口 | [main.c](../../Code/Source/main.c) |

工程使用 Keil uVision 工程文件组织源码，源码目录主要分为 `Code/Source`、`Code/Drivers`、`Code/BSP`、`Code/STM32F0xx_StdPeriph_Driver`。

## 关键条件宏

| 宏 | 默认状态 | 影响 |
| --- | --- | --- |
| `_IAP` | 启用 | 应用从 `0x08001C00` 启动，启动时重映射向量表。 |
| `_COMMOM_UPPER_SCI1` | 启用 | 启用 USART1 公共上位机协议。 |
| `_COMMOM_UPPER_SCI2` | 启用 | 启用 USART2 公共上位机协议。 |
| `_CLIENT_SCI1` | 未启用 | USART1 客户协议不参与运行。 |
| `_CLIENT_SCI2` | 未启用 | USART2 客户协议不参与运行。 |
| `__FUNC__HEAT__` | 未启用 | 加热/冷却模块默认不初始化、不调度。 |
| `__FUNC__LED__` | 未启用 | SOC LED Bar 默认不初始化、不调度。 |
| `__FUNC_RTC__` | 未启用 | 部分 RTC/SOC 低功耗修正路径不参与编译。 |
| `UART1_WAKEUP_ENABLE` | 启用 | PA10 可作为 USART1 RX 唤醒源。 |
| `PROJECT_CFG_FEATURE_LOW_POWER` | 启用 | 通过 `PROJECT_FEATURE_LOW_POWER` 门控 `App_SleepDeal()`。 |
| `PROJECT_CFG_FEATURE_RTC` | 启用 | 通过 `PROJECT_FEATURE_RTC` 门控 `Init_RTC()`。 |
| `LIFEPO` | 启用 | 默认使用磷酸铁锂 OCV/SOC 参数。 |

## 模板配置头

模板副本新增 [Project_Template_Config.h](../../Code/Include/Project_Template_Config.h)，用于集中描述 profile 级配置，后续项目配置生成器只能从该文件和 `templates/bms/target_profiles.json` 派生工程参数。

配套边界头文件：

| 文件 | 职责 |
| --- | --- |
| [Project_Target.h](../../Code/Include/Project_Target.h) | 固定当前模板的 MCU、AFE、board profile。 |
| [Project_Protection.h](../../Code/Include/Project_Protection.h) | 将软/硬件保护开关收口为 `PROJECT_CFG_PROTECTION_MODE`。 |
| [Project_Features.h](../../Code/Include/Project_Features.h) | 将 SOC、低功耗、通信、显示、加热、存储等功能映射为统一 feature gate。 |

| 配置 | 当前值 | 说明 |
| --- | --- | --- |
| `PROJECT_CFG_MCU_STM32F030_STD` | `1` | 当前模板 MCU 为 `STM32F030` StdPeriph。 |
| `PROJECT_CFG_AFE_BQ76940` | `1` | 当前模板 AFE 为 `BQ76940`。 |
| `PROJECT_CFG_PROTECTION_MCU_SOFTWARE` | `1` | 软件保护是 MOS/Relay 决策主线。 |
| `PROJECT_CFG_PROTECTION_AFE_HARDWARE` | `0` | AFE 硬件保护不是当前主保护口径。 |
| `PROJECT_CFG_STORAGE_INTERNAL_FLASH` | `1` | 参数、日志、SOC 和生产信息固定保存到 MCU 内部 Flash。 |

## Flash 地址边界

| 区域 | 地址/长度 | 说明 |
| --- | --- | --- |
| IAP | `0x08000000` | Boot/IAP 起始地址。 |
| App | `0x08001C00` | 应用起始地址，必须同步 IAP、向量重映射和 scatter。 |
| App Size | `0x0000C400` | App 链接范围为 `0x08001C00..0x0800DFFF`。 |
| Storage | `0x0800E000..0x0800FFFF` | 内部 Flash 参数与标志区，App 不能链接到这里。 |

## 默认产品参数

| 参数 | 值 | 说明 |
| --- | --- | --- |
| `SNum` | `4` | 默认电芯串数。 |
| `LEVEL_CURR` | `CURR_DEFAULT` | 默认电流等级。 |
| `APPLICATION_ADDRESS` | `0x08001C00` | IAP 应用入口。 |
| 默认硬件版本 | `hanstar` | `ProductionID` 默认值。 |
| 默认软件版本 | `a002-c063v1p0` | `ProductionID` 默认值。 |
| 默认序列号 | `hanstar` | `ProductionID` 默认值。 |

## 源码分层

| 层级 | 目录/文件 | 职责 |
| --- | --- | --- |
| 应用层 | `Code/Source` | BMS 业务逻辑、保护、SOC、通信、存储、低功耗。 |
| BSP 层 | `Code/BSP` | AFE 软件 I2C、通用板级初始化骨架。 |
| MCU 驱动层 | `Code/Drivers` | `system_stm32f0xx.c`、中断向量、启动文件等。 |
| 标准库 | `Code/STM32F0xx_StdPeriph_Driver` | STM32F0 StdPeriph 外设库。 |

## 参与构建但默认不一定运行的模块

| 模块 | 状态 | 说明 |
| --- | --- | --- |
| `PWM.c` | 参与构建 | 主流程默认未初始化/调用。 |
| `LedBar.c` | 参与构建 | 受 `PROJECT_FEATURE_LEDBAR` 控制，默认不运行。 |
| `Heat_Cool.c` | 参与构建 | 受 `PROJECT_FEATURE_HEAT` 控制，默认不运行。 |
| `I2C_Slave.c` | 参与构建 | 初始化与处理函数为空，属于预留模块。 |
| `Uart_Client.c` | 参与构建 | 受 `_CLIENT_SCI1/_CLIENT_SCI2` 控制，默认不运行。 |
| `LED_Buzzer.c` | 排除构建 | Keil 工程中 `IncludeInBuild=0`。 |

## 编码注意事项

当前仓库内的模板副本源码和文档已统一转为 UTF-8，外部旧项目原目录不修改。后续修改应继续保持 UTF-8，避免 VSCode/Codex 与 Keil 之间出现注释乱码或批量处理失败。

## 修改建议

- 新增功能优先通过 `Project_Template_Config.h` 或明确的模块配置头增加清晰功能宏，并在文档中说明默认状态。
- 修改 GPIO 复用前先更新 [MCU 资源与引脚分配](00-mcu-resources.md)。
- 对参与构建但默认不运行的模块，应明确它们是客户版本功能、历史功能还是预留骨架。
- 新增模块应同时补充初始化入口、主循环调度入口、参数存储位置和通信协议映射。

# 与低功耗相关的 MCU 资源清单

> 范围：当前 STM32F103C8 BMS App 的 MCU 资源与低功耗关系。本文用于后续设计 `bsp_rtc.c/.h`、`bsp_power.c/.h`、`bsp_clock.c/.h`、`app_lowpower.c/.h` 时对照，第一阶段不修改源码。

## 1. MCU / 时钟 / Flash 映射

| 资源 | 当前项目依据 | 低功耗关系 | 当前结论 |
|---|---|---|---|
| MCU | `CommomSH367309_16series_103RCT6_C.uvprojx:17` 为 `STM32F103C8` | 使用 Cortex-M3、STM32F1 PWR/RTC/BKP/EXTI17 机制 | 按 F1 方案落地，F0 作为移植分支 |
| 编译宏 | `CommomSH367309_16series_103RCT6_C.uvprojx:340` 为 `STM32F10X_MD,USE_STDPERIPH_DRIVER` | 标准外设库 API 可用 | 不使用 HAL |
| App VTOR | `system_stm32f10x.c:128` 配置 `VECT_TAB_OFFSET 0x4800` | 符合 App 地址 `0x08004800` 的中断向量偏移 | 任何时钟恢复都不能破坏 VTOR |
| SYSCLK | `system_stm32f10x.c:110` 使用 `SYSCLK_FREQ_HSE` | Stop 唤醒后硬件回 HSI，运行态要恢复 HSE | 当前通过 `cpu_frequency_conf()` 调 `SystemInit()` 恢复 |
| HSE 频率 | `stm32f10x.h:115-121` 默认 8MHz；Keil `CLOCK(12000000)` 在工程 `:21/:958` | 影响 USART/CAN/TIM/ADC | 需要实物确认，属于 P1 风险 |
| IAP/App 地址 | `system_stm32f10x.c:128`、AGENTS 规则 | 低功耗新增代码不得改变烧录安全规则 | 第一阶段只读，不涉及烧录 |

## 2. RTC / BKP / EXTI / PWR

| 资源 | 当前项目依据 | 低功耗关系 | 当前结论 |
|---|---|---|---|
| RTC 功能开关 | `Project_Config.h:84-86` `PROJECT_CFG_RTC_ENABLE=1` | 允许低功耗计时、休眠唤醒、SOC 休眠修正 | 已启用 |
| RTC 初始化 | `RTC.c:437-480` | 启动后初始化 RTC，配置 Alarm/NVIC | 已有完整入口 |
| LSE/LSI | `RTC.c:235-278` | LSE 启动失败时回退 LSI | 已有 fallback，但 LSI 精度影响休眠时间和 IWDG估算 |
| RTC wait 超时 | `RTC.c:26-55` | 防止 RTC 同步/写完成死等 | 已有 safe wait，后续需检查返回值 |
| RTC Alarm | `RTC.c:407-418` | HICCUP Stop 周期唤醒源 | 使用 F1 RTC Alarm |
| EXTI17 pending | `RTC.c:281-287` | Alarm 唤醒前后必须清 pending | 已清 RTC ALR、EXTI17、NVIC pending |
| 运行态秒中断 | `RTC.c:426-435` | Stop 后恢复秒中断 | 已有 `RTC_RestoreRunInterrupts()` |
| BKP_DR1 | `RTC.c:444-461` | RTC 初始化标志 | 与 `BKP_DeInit()` 相关 |
| BKP_DR2/DR3 | `SleepDeal.c:123-167` | 睡眠启动标志及反码 | 后续需形成 BKP 分配表 |
| PWR Stop | `conf.c:374-385` | 核心 Stop 入口 | 已使用 `PWR_EnterSTOPMode(...WFI)` |

## 3. 时基资源

| 资源 | 当前项目依据 | 低功耗关系 | 当前结论 |
|---|---|---|---|
| TIM3 | `System_Init.c:100-127` | 10ms 主任务时基 | Stop 前关闭，唤醒后需恢复 |
| TIM3 IRQ | `System_Init.c:293-299` | 产生 10ms tick | Stop 后若 TIM3 未恢复，主循环时间基准停滞 |
| 50/100/200/1000ms flags | `System_Init.c:258-286` | 任务调度、低功耗每秒检查、LED/老化/SOC | 唤醒后需重新启动 TIM3 |
| SysTick | `System_Init.c:132-150` | 阻塞延时，不作为周期调度 | Stop 后必须根据恢复后的 `SystemCoreClock` 重新 `InitDelay()` |
| `SysTime_LatchTaskFlags()` | `Runtime.c:14-18` | 主循环锁存 tick flag | 低功耗状态机应在锁存后运行 |

## 4. IWDG

| 资源 | 当前项目依据 | 低功耗关系 | 当前结论 |
|---|---|---|---|
| IWDG 开关 | `Project_Config.h:80-82` | 量产启用 | 已启用 |
| 初始化 | `System_Init.c:33-48` | `__FUNC_RTC__` 下为最大窗口 | 当前约 26.2s 标称，保守最短约 17.5s |
| 主循环喂狗 | `Runtime.c:39-41` | 正常运行安全 | 已有 |
| Stop 前后喂狗 | `rtc_sleep_port.c:118-123` | 避免刚进/刚出 Stop 误复位 | 已有 |
| CAN RTC 服务喂狗 | `Can_HDX.c:917-925` | 唤醒服务最长 1.5s | 已有 |
| RTC 周期裁剪 | `RTC.c:375-397` | 应限制 RTC 周期小于 IWDG 超时 | 当前被注释，需后续恢复 |

## 5. GPIO / 唤醒源 / 电源控制

| 资源 | 当前项目依据 | 低功耗关系 | 当前结论 |
|---|---|---|---|
| CHG_IN | `conf_gpio.h:30-31`，`SleepDeal.c:11-14` | 充电唤醒/禁止休眠 | 当前复位式睡眠唤醒判断已用 |
| INT_WK_CMNT | `conf_gpio.h:33-34`，`conf.c:252-258` | 外部通信唤醒线 | 当前配置 EXTI12 |
| MCU_WK | `conf_gpio.h:39-40`，`rtc_sleep_port.c:46-49` | MCU 唤醒/禁止 RTC sleep | 当前作为 `LOW_POWER_RTC_BLOCK_MCU_WAKE` |
| SW | `conf_gpio.h:42-43`，`rtc_sleep_port.c:199-202` | 按键唤醒/显示 SOC | 当前用于猜测唤醒源 |
| GPIO_CMNT_EN | `conf.c:69-80`，`Can_HDX.c:882-893` | CAN 收发器供电 | 睡眠前关闭，RTC 服务时打开 |
| GPIO_DC_EN | `conf.c:321-323` | DC 输出控制 | RTC 模式下拉低并保留输出 |
| AFE1_CTL | `conf_gpio.h:45-46` | AFE 供电/控制 | 睡眠前由 AFE sleep 路径管理 |

## 6. 通信资源

| 资源 | 当前项目依据 | 低功耗关系 | 当前结论 |
|---|---|---|---|
| CAN1 | `Can_HDX.c:825-844` 初始化 | Stop 后需要重新初始化或恢复 | 当前由 IO/系统恢复链路间接处理 |
| CAN RX IRQ | `Can_HDX.c:949-958` | 运行态接收，不作为第一版 Stop 唤醒 | 第一版不建议 CAN Stop 唤醒 |
| CAN busy | `Can_HDX.c:865-880` | 睡前阻塞条件 | 已有函数，未统一接入低功耗阻塞 |
| CAN RTC 服务 | `Can_HDX.c:906-933` | RTC 唤醒后短服务窗口 | 已有，最长 1.5s |
| USART1/2/3 | `Sci_Upper.c:1596-1653` | Stop 后需要恢复寄存器和波特率 | 当前恢复路径有 `USART_DeInit()`，后续应明确重初始化顺序 |
| SCI busy | `Sci_Upper.c:1576-1594`、`:1678-1690` | 通信活跃禁止休眠 | 已有函数，应接入 `LP_BLOCK_COMM` |
| Modbus frame service | `Sci_Upper.c:2243-2256` | 主循环服务，Stop 会中断响应 | 通信窗口内禁止 Stop |

## 7. 采样与业务外设

| 资源 | 当前项目依据 | 低功耗关系 | 当前结论 |
|---|---|---|---|
| ADC1 | `ADC.c:215-260` | 依赖 ADC 时钟、DMA、TIM2 触发 | Stop 前关闭，醒后重新 Init |
| DMA1 CH1 | `ADC.c:108-122` | ADC DMA buffer | Stop 前关闭，醒后重新配置 |
| TIM2 | `ADC.c:149-181` | ADC 触发源 | Stop 前关闭，醒后重启 |
| ADC Stop API | `ADC.c:268-285` | 外设休眠准备 | 已有 |
| AFE 200ms任务 | `DataDeal.c:1225-1249` | 保护/SOC/MOS 数据更新 | Stop 期间暂停，醒后需重新同步 |
| AFE sleep | `SleepDeal.c:109-113` | 复位式睡眠前让 AFE 进入低功耗 | 已有 |
| SOC 休眠补偿 | `SocEnhance.c:1739-1764` | RTC 休眠时间用于 OCV/静置修正 | 已有 |
| SOC 快照 | `SocEnhance.c:1678-1685` | 睡前保存 | 已有 |
| LED 扫描 | `LedBar.c:1018-1024` | 依赖 TIM4/扫描状态 | Stop 前应停扫描，醒后恢复显示 |
| LED 睡眠状态 | `LedBar.c:1039-1045` | 低功耗 pending 时保存 SOC 并睡眠 | 已有 |

## 8. Flash / BKP / 非易失数据

| 资源 | 当前项目依据 | 低功耗关系 | 当前结论 |
|---|---|---|---|
| 参数 Flash 保存 | `EEPROM.c:161-172` | 写入期间禁止休眠 | 缺少统一 busy 接口 |
| Storage slot 写入 | `Flash.c:260-278`、`:280-342` | 擦页/写页不可被低功耗打断 | 需要 `LP_BLOCK_FLASH_BUSY` |
| 单半字写 | `Flash.c:650-670` | 例如升级标志/策略标志 | 需要阻塞 Stop |
| 日志睡眠请求 | `LogRecord.c:66-74` | 睡前记录 sleep event | 已有 |
| 日志周期写 | `LogRecord.c:178-210` | 每秒运行 | Stop 周期会暂停，应明确累计时间策略 |
| 老化进度保存 | `FactoryAging.c:358-370` | 睡前保存进度 | 已有 |
| BKP 睡眠标志 | `SleepDeal.c:123-167` | 复位式睡眠启动态保存 | 需统一分配 |

## 9. 当前资源恢复顺序

当前 HICCUP Stop 恢复大致顺序：

1. `PWR_EnterSTOPMode()` 返回。
2. `cpu_frequency_conf()` 调 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`，见 `rtc_sleep_port.c:207-212`。
3. `InitRunAfterStopWakeup()` 调 `InitDelay()`、`RTC_RestoreRunInterrupts()`、`InitIO_rtc()`，见 `conf.c:392-400`。
4. 停止并重启 ADC，见 `conf.c:401-402`。
5. LED 恢复显示窗口，见 `conf.c:404-407`。
6. USART DeInit，后续恢复逻辑在当前片段中需要继续审查，见 `conf.c:409-410`。
7. `rtc_sleep_run_hiccup_cycle()` 若是 RTC 唤醒，则执行 SOC 休眠补偿和 CAN RTC 服务，见 `rtc_sleep.c:321-328`。

建议后续框架把这个顺序固化为：

`BspClock_RestoreAfterStop()` -> `BspRtc_AfterWakeup()` -> `BspPower_RestoreGpio()` -> `ADC/UART/CAN/LED restore` -> `AFE/SOC/保护同步` -> `LP_STATE_RUN`。

## 10. 资源风险重点

- `RTC_GetWakeupPeriodSeconds()` 的 IWDG 安全窗口被注释，见 `RTC.c:375-397`。
- `HSE_VALUE=8MHz` 与 Keil `CLOCK(12000000)` 不一致，需要实测确认。
- `SystemInit()` 在 Stop 恢复中会重新设置 VTOR/RCC，虽然当前 App 偏移已有配置，但不建议长期把它作为唯一恢复 API。
- Flash 写入没有 busy API，参数保存、日志写入、老化保存、升级标志写入期间应禁止 Stop。
- CAN/SCI busy 已有接口，但未统一纳入低功耗阻塞位。
- AFE 与保护依赖 200ms 任务，Stop 后必须重新同步 AFE 数据和 MOS/保护状态。
- LED 显示窗口当前会因低功耗 pending 而睡眠，但没有作为统一阻塞原因可观测。

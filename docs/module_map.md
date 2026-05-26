# 103-309 BMS 模块地图

文档状态：CURRENT
源码验证：VERIFIED_FROM_REVIEW
主要参考源码：`docs/review/module_map.md` 与当前主工程源码
最后更新时间：2026-05-26
未确认事项：均衡主动控制、真实电流路径、实际 Flash 容量。

## 1. 模块到文件映射

| 模块 | 主要源码文件 | 职责 |
|---|---|---|
| 启动与调度 | `main.c`, `AppInit.c`, `Runtime.c`, `System_Init.c` | 初始化、主循环、TIM3 tick、IWDG |
| 配置与 IO | `conf/Project_Config.h`, `conf/conf.h`, `conf_gpio.h`, `conf.c` | 产品宏、GPIO、电源轨、唤醒源、STOP IO |
| ADC | `ADC.c/.h` | PA1 VBC、PA2 Type-C 电流、PB1 MOS 温度 |
| AFE | `I2C_AFE1.c/.h`, `SH367309_Func.c/.h`, `SH367309_DataDeal.c/.h` | SH367309 通信、保护参数、MOS、fault |
| 数据聚合 | `DataDeal.c/.h` | 电压/温度/电流加载、客户逻辑、SOC 输入 |
| SOC | `SOC.c/.h`, `SocEnhance.c/.h` | SOC 算法、snapshot、RTC 补偿、发布 |
| 存储 | `EEPROM.c/.h`, `Flash.c/.h`, `UpgradeParamPolicy.h` | 内部 Flash 参数、日志、SOC、老化、IAP mailbox |
| Modbus | `Sci_Upper.c/.h` | USART1/2/3 上位机协议、寄存器读写 |
| CAN | `Can_HDX.c/.h`, `CanFeidaoFrames.c/.h` | 飞道周期帧、CAN App 服务、RTC wake service |
| 低功耗 | `app_lowpower.c`, `rtc_sleep.c`, `rtc_sleep_port.c`, `SleepDeal.c`, `LowPowerSleep.c`, `RTC.c` | 休眠判定、STOP、RTC、BKP、reset sleep |
| LED | `LedBar.c/.h` | 查理复用显示、按键、睡眠 SOC 预览 |
| 老化 | `FactoryAging.c/.h` | 运行态老化计时、BKP/Flash 保存、CAN 控制 |
| 日志/产品 | `LogRecord.c/.h`, `ProductionID.c/.h` | 事件记录、SN/HW/SW 信息 |

## 2. 关键依赖

| 消费者 | 依赖 |
|---|---|
| Modbus/CAN/LED/低功耗 | `g_stCellInfoReport` |
| SOC | AFE 电流/电压、ADC Type-C 电流、OtherElement、Flash snapshot |
| AFE 参数 | `PRT_E2ROMParas`, `OtherElement`, `g_u32CS_Res_AFE` |
| 低功耗 | 电流、通信 busy、Flash busy、fault、LED active、IWDG 周期 |
| IAP | `u8FlashUpdateFlag`, SRAM mailbox, safe app address |

## 3. 重构建议

1. `DataDeal.c` 先拆职责文档，再拆代码。
2. `Sci_Upper.c` 先生成协议表，不直接改地址。
3. `Flash.c` 先加验证门禁，不改布局。
4. AFE 抽象应从接口设计开始，不先改 SH367309 行为。
5. LED/低功耗/老化必须先确认用户体验和客户需求。

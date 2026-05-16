# 历史睡眠状态机

## 相关文件

- [SleepDeal.c](../../Code/Source/SleepDeal.c)
- [SleepDeal.h](../../Code/Include/SleepDeal.h)
- [Flash.c](../../Code/Source/Flash.c)
- [RTC.c](../../Code/Source/RTC.c)

## 模块职责

`SleepDeal` 是当前 A002 模板唯一低功耗主入口，负责根据系统条件进入低功耗模式、配置外部唤醒源、保存睡眠标志并在唤醒后恢复状态。

旧文档曾描述过 `IdleSleep` 空闲睡眠兼容层，但当前模板源码没有 `IdleSleep.c`、`IdleSleep_Init()` 或 `App_IdleSleep()`。后续移植不要恢复第二套低功耗状态机，除非先重新设计统一低功耗接口。

## 主调度

`App_SleepDeal()` 在主循环中执行。它通常需要满足：

- BMS 已完成启动。
- Sleep 功能开关允许。
- 当前无阻止睡眠的保护或运行状态。
- 睡眠状态机到达进入条件。

## 睡眠标志

模块通过 Flash 保存睡眠相关状态：

| 标志 | 说明 |
| --- | --- |
| `FLASH_ADDR_SLEEP_FLAG` | 睡眠模式或状态。 |
| `FLASH_ADDR_WAKE_TYPE` | 唤醒原因。 |

进入睡眠前写入 Flash 后可能触发系统复位，下一次启动时通过 `IsSleepStartUp()` 判断是否按睡眠恢复路径运行。

## 唤醒源

典型唤醒源包括：

- PA0 充电器/全串唤醒。
- PB6 负载移除/负载唤醒。
- PC13 按键/DI 唤醒。
- PA10 USART1 RX 唤醒。
- RTC 周期 Alarm。

## 与旧 `IdleSleep` 说明的关系

| 项目 | 当前结论 |
| --- | --- |
| `IdleSleep.c` | 当前模板不存在。 |
| `App_IdleSleep()` | 当前主循环未调用，源码不存在。 |
| 低功耗入口 | 统一为 `PROJECT_FEATURE_LOW_POWER` 门控下的 `App_SleepDeal()`。 |
| RTC 唤醒 | 由 `RTC.c` 与 `SleepDeal.c` 配合，不再依赖 IdleSleep 文档。 |

## 维护建议

- 修改低功耗逻辑前先保持单入口：`SleepDeal` 负责进入条件、唤醒源和恢复路径。
- Flash 写睡眠标志会消耗擦写寿命，频繁睡眠场景需要先设计 RAM/BKP/低频写入策略，不要直接新增第二套空闲睡眠逻辑。
- 唤醒源配置要与 EXTI 中断处理保持一致，否则会出现能唤醒但无法记录原因的问题。

# RTC 与周期唤醒

## 相关文件

- [RTC.c](../../Code/Source/RTC.c)
- [RTC.h](../../Code/Include/RTC.h)
- [stm32f0xx_it.c](../../Code/Drivers/stm32f0xx_it.c)

## 模块职责

RTC 模块负责实时时钟初始化、Alarm A 周期唤醒、低功耗唤醒原因记录，以及为休眠相关 SOC 修正提供时间基础。

## 时钟来源

初始化时优先使用 LSE。如果 LSE 启动失败，会复位 BDCR 并回退到 LSI。

| 时钟 | 用途 | 说明 |
| --- | --- | --- |
| LSE | RTC 首选 | 精度更好。 |
| LSI | RTC 备用 | 精度较低，但可保证可用性。 |

## Alarm A

`RTC_PeriodicWakeEnable(period_sec)` 用于配置周期唤醒。当前模板的低功耗路径由 `SleepDeal` 配合 RTC 使用；旧 `IdleSleep` 说明已废止。

RTC 中断处理逻辑：

1. 判断 Alarm A 标志。
2. 清除 RTC/EXTI 标志。
3. 写入 `FLASH_ADDR_WAKE_TYPE`，标记 RTC 唤醒。
4. 如果周期唤醒仍启用，则调度下一次 Alarm。

## 与低功耗模块关系

| 模块 | 关系 |
| --- | --- |
| `SleepDeal` | 使用 RTC 作为睡眠唤醒源之一。 |
| `SOC` | 部分 OCV 修正路径可依赖 RTC 时间。 |
| `Flash` | RTC 唤醒原因写入 Flash 标志。 |

## 维护建议

- 如果产品要求准确休眠时长，应优先保证 LSE 硬件存在且稳定。
- LSI 模式下不建议用于高精度计量，只适合周期唤醒和粗略计时。
- RTC 中断内不要做复杂业务逻辑，应只设置标志和安排下一次唤醒。

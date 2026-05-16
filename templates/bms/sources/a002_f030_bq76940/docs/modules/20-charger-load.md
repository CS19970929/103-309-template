# 充电器与负载检测

## 相关文件

- [ChargerLoadFunc.c](../../Code/Source/ChargerLoadFunc.c)
- [ChargerLoadFunc.h](../../Code/Include/ChargerLoadFunc.h)
- [stm32f0xx_it.c](../../Code/Drivers/stm32f0xx_it.c)

## 模块职责

该模块负责检测充电器插入、全串唤醒、负载相关信号，并在特定故障后根据充电器接入状态尝试恢复充放电路径。

## 默认硬件资源

| 信号 | GPIO | 说明 |
| --- | --- | --- |
| 充电器/全串唤醒 | PA0 | EXTI0，插入或唤醒触发。 |
| 负载移除/唤醒 | PB6 | EXTI6，低功耗唤醒路径使用。 |

可选 `_CHARGER_LOAD` 路径还涉及 PB8、PF5、PF4、PC6 等资源，但默认未启用。

## 主调度

`App_ChargerLoad_Det()` 在主循环中执行，读取中断设置的状态标志，并处理充电器/负载状态变化。

## 充电器恢复逻辑

`AllSeriesDeal_Charger_ON()` 可在检测到充电器插入后，对部分故障场景尝试恢复，例如：

- 欠压保护后允许充电恢复。
- 放电过流或 CBC 关闭后，满足条件时重新打开充电路径。
- 驱动关闭后根据充电器状态触发恢复流程。

## 与其他模块关系

| 模块 | 关系 |
| --- | --- |
| `Fault` | 判断故障是否允许通过充电器恢复。 |
| `IO_Control` | 恢复时影响 CHG/DSG 控制。 |
| `SleepDeal` | PA0/PB6 是重要唤醒源。 |
| `LogRecord` | 插入/恢复事件可记录。 |

## 维护建议

- 充电器插入恢复属于安全相关逻辑，新增恢复条件必须确认不会绕过欠压、过温、短路等严重保护。
- 可选 `_CHARGER_LOAD` 功能启用前必须重新检查 GPIO 资源冲突。
- EXTI 中断中只设置状态，实际恢复动作应保持在主循环中执行。

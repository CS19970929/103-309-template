# PWM 与短路/过流阈值辅助

## 相关文件

- [PWM.c](../../Code/Source/PWM.c)
- [PWM.h](../../Code/Include/PWM.h)
- [ShortFunc.c](../../Code/NewFunc/ShortFunc.c)
- [ShortFunc.h](../../Code/NewFunc/ShortFunc.h)
- [I2C_AFE1.c](../../Code/Source/I2C_AFE1.c)

## PWM 模块状态

`PWM.c` 参与构建，但默认未在 `main.c` 中初始化或调度。它主要配置 TIM3 PWM 输出，可能用于 CBC/OC 参考电压或客户硬件功能。

## PWM 硬件资源

| 定时器 | 通道 | GPIO | 说明 |
| --- | --- | --- | --- |
| TIM3 | CH1 | PA6 | 与 `LedBar` ALARM LED 冲突。 |
| TIM3 | CH2 | PA7 | PWM 输出。 |

## PWM 逻辑

PWM 占空比会根据 `OtherElement` 等参数计算 CBC/OC 参考值。由于默认主流程未调用，维护时应先确认目标硬件是否实际使用该参考输出。

## 短路/过流阈值辅助

`ShortFunc` 用于把业务侧短路/过流参数转换为 `BQ769x0` 可接受的寄存器配置。`InitAFE1()` 初始化 AFE 时会调用相关逻辑。

典型处理包括：

- 根据采样电阻和电流等级计算阈值。
- 从 `BQ769x0` 支持的 SCD/OCD delay 和 threshold 组合中选择接近值。
- 写入 `PROTECT1`、`PROTECT2` 等保护寄存器。

## 与保护模块关系

软件保护由 `Fault` 判断，硬件快速保护由 AFE SCD/OCD 实现。两者阈值应有清晰分层：

- AFE SCD/OCD 用于快速硬件级关断。
- 软件 OCP 用于带滤波和恢复策略的业务保护。

## 维护建议

- 修改电流采样电阻或电流量程时，必须同步校准参数、软件 OCP 阈值和 AFE SCD/OCD 寄存器值。
- 如果启用 PWM 输出，必须解决 PA6 与 LED Bar 的资源冲突。
- AFE 短路阈值是安全关键参数，不应只按通信参数直接写入，必须经过合法范围和最接近档位转换。

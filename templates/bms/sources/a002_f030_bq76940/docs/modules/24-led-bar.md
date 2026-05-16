# SOC LED Bar

## 相关文件

- [LedBar.c](../../Code/NewFunc/LedBar.c)
- [LedBar.h](../../Code/NewFunc/LedBar.h)

## 模块状态

`LedBar` 源码参与构建，但默认 `__FUNC__LED__` 未定义，因此：

- `Init_LedBar()` 默认不调用。
- `APP_LedBar()` 默认不进入主循环。

该模块属于条件客户功能。

## 硬件资源

| 信号 | GPIO | 说明 |
| --- | --- | --- |
| SOC LED | PB7 | 与 RS485 enable、CB、BSP I2C SDA 冲突。 |
| SOC LED | PB8 | 与可选负载检测、BSP I2C SCL 冲突。 |
| SOC LED | PB13 | 与 CHG MOS/Relay 冲突。 |
| SOC LED | PB12 | LED Bar 专用路径。 |
| SOC LED | PB5 | 与电源控制相关资源复用。 |
| RUN LED | PA5 | 与 ADC EV2 冲突。 |
| ALARM LED | PA6 | 与 TIM3 PWM CH1 冲突。 |
| Key | PB14 | 与 DSG MOS/Relay 冲突。 |

## 功能逻辑

模块根据 SOC、充放电状态、故障状态和按键状态显示 LED：

- 正常 SOC 档位显示。
- 充电动态显示。
- 放电状态显示。
- 故障/告警显示。
- 长按按键进入或退出相关状态。

## 与其他模块关系

| 模块 | 关系 |
| --- | --- |
| `SOC` | 提供 SOC 百分比。 |
| `Fault` | 提供告警/故障显示依据。 |
| `IO_Control` | 提供充放电状态。 |
| `SleepDeal` | 睡眠状态下显示策略不同。 |

## 维护建议

- 默认硬件资源与主 BMS 控制资源冲突明显，启用前必须确认目标 PCB 版本。
- 不建议在当前默认 MOS 拓扑上直接启用 LED Bar。
- 如果产品需要 LED Bar，应建立独立硬件版本宏，并在资源表中明确互斥关系。

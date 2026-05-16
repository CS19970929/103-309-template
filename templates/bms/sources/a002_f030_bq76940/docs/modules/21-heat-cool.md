# 加热与冷却控制

## 相关文件

- [Heat_Cool.c](../../Code/Source/Heat_Cool.c)
- [Heat_Cool.h](../../Code/Include/Heat_Cool.h)
- [IO_Control.c](../../Code/Source/IO_Control.c)

## 模块状态

源码参与构建，但默认 `__FUNC__HEAT__` 未定义，因此：

- `InitHeat()` 默认不调用。
- `App_Heat_Cool_Ctrl()` 默认不进入主循环。

该模块属于条件功能，启用前必须检查硬件资源和保护策略。

## 硬件资源

| 信号 | GPIO | 说明 |
| --- | --- | --- |
| Heat/Cool Relay | PA12 | 加热或冷却控制输出。 |

PA12 在历史 `LED_Buzzer` 模块中也被用作 Buzzer，因此启用前要确认旧模块未参与构建。

## 功能逻辑

模块根据温度、SOC、电流、保护状态和内部 Flash 参数决定加热/冷却状态。加热过程中可能通过 `Driver_Element.DriverForceExt` 强制影响 CHG/DSG 驱动状态。

冷却相关结构存在，但默认主控制路径中冷却调用较少，需要按具体产品硬件确认。

## 与其他模块关系

| 模块 | 关系 |
| --- | --- |
| `ADC` / `DataDeal` | 提供温度数据。 |
| `Storage` | 提供内部 Flash 中的加热/冷却阈值参数。 |
| `IO_Control` | 加热状态可强制驱动输出。 |
| `Fault` | 温度保护影响是否允许加热/冷却。 |
| `LogRecord` | 加热/冷却事件可记录。 |

## 维护建议

- 启用加热前必须定义清楚加热与充电 MOS 的联动策略。
- 加热继电器属于功率输出，建议增加最小开关间隔和故障降级路径。
- 如果冷却功能真实使用，应补充硬件输出、启停阈值、异常检测和通信配置项。

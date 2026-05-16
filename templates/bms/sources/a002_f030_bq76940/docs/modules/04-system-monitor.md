# 系统监控与启动门控

## 相关文件

- [System_Monitor.c](../../Code/Source/System_Monitor.c)
- [System_Monitor.h](../../Code/Include/System_Monitor.h)

## 模块职责

`System_Monitor` 是系统级状态与功能开关中心，主要负责：

- 管理各功能模块是否允许启动。
- 记录系统错误计数。
- 为保护、驱动、SOC、均衡、睡眠等模块提供启动门控。
- 从内部 Flash 默认参数初始化系统开关。

## 核心数据

| 数据 | 说明 |
| --- | --- |
| `System_OnOFF_Func` | 功能开关位，控制 Balance、BMS、MOS、AFE、Sleep 等功能。 |
| `SystemStatus` | 系统运行状态。 |
| `System_ErrFlag` | 系统错误标志与计数。 |

## 默认功能开关

`InitSystemMonitorData_EEPROM()` 中默认启用：

- `Balance`
- `BMS`
- `MOS`
- `AFE1`
- `Sleep`

加热功能受 `__FUNC__HEAT__` 控制，冷却默认关闭。

## 启动门控

`System_FUNC_StartUp()` 根据模块 ID 判断功能是否已经允许运行。主循环中多个模块会调用该函数，例如：

| 功能 | 典型调用模块 |
| --- | --- |
| ADC/Sample | `DataDeal`、`ADC` |
| Protect | `Fault` |
| MOS/Relay | `IO_Control` |
| Balance | `Cell_balance` |
| Sleep | `SleepDeal` |
| Heat/Cool | `Heat_Cool` |

## 错误处理接口

`System_ERROR_UserCallback()` 提供系统错误的统一操作接口，支持：

- 增加错误计数。
- 清除错误计数。
- 查询错误状态。

该接口被 AFE、内部 Flash 存储、CBC、温度断线等模块用于将局部故障提升为系统错误。

## 维护建议

- 新增模块如果会影响安全控制，应接入 `System_FUNC_StartUp()`，避免启动阶段未准备好时运行。
- 系统错误应优先通过 `System_ERROR_UserCallback()` 记录，而不是在多个模块中分散定义全局标志。
- 对于可恢复错误，应明确恢复条件和清除路径，避免错误计数只增不减导致系统长期锁定。

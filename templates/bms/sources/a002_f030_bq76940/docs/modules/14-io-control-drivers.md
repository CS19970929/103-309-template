# MOS/Relay 驱动控制

## 相关文件

- [IO_Control.c](../../Code/Source/IO_Control.c)
- [IO_Control.h](../../Code/Include/IO_Control.h)
- [IODrivers_030.c](../../Code/Source/IODrivers_030.c)
- [IODrivers_030.h](../../Code/Include/IODrivers_030.h)
- [BQ769X0_Func.c](../../Code/Source/BQ769X0_Func.c)

## 模块职责

该模块负责根据保护状态、电流方向、系统错误、外部强制命令和驱动拓扑，决定 CHG/DSG MOS 或 Relay 的开关状态。

## 默认驱动拓扑

当前选择的拓扑为 `_MOS_SAME_DOOR_NO_PRECHG`。相关 GPIO 包括：

| 信号 | GPIO | 说明 |
| --- | --- | --- |
| CHG MOS/Relay | PB13 | 充电通道。 |
| DSG MOS/Relay | PB14 | 放电通道。 |
| Precharge | PA8 | 预充控制，当前拓扑下不一定实际使用。 |
| DI1 | PC13 | 外部输入/唤醒。 |

当前 `u8_DriverCtrl_Right = 1` 表示 AFE 拥有 MOS 控制权，MCU GPIO 注册状态，但实际 CHG/DSG 主要通过 `BQ769X0_DriverMos_Ctrl()` 写 AFE `SYS_CTRL2`。

## 主调度

`App_MOS_Relay_Ctrl()` 在主循环中执行，通常以 10ms 节拍处理：

1. 检查系统启动门控。
2. 读取保护等级、系统错误和温度断线状态。
3. 更新 `Driver_Element`。
4. 调用底层驱动状态机。
5. 必要时写 AFE CHG/DSG 控制位。

## 输入条件

| 输入 | 来源 |
| --- | --- |
| 保护标志 | `Fault` |
| 当前电流 | `DataDeal` |
| AFE MOS 状态 | `BQ769X0_Func` |
| 系统错误 | `System_Monitor` |
| 加热/冷却强制控制 | `Heat_Cool` |
| 充电器/负载状态 | `ChargerLoadFunc` |

## 输出

| 输出 | 说明 |
| --- | --- |
| CHG 控制 | 允许或禁止充电。 |
| DSG 控制 | 允许或禁止放电。 |
| Relay/MOS 状态 | 对通信和日志输出。 |
| AFE `SYS_CTRL2` | 实际控制 AFE CHG/DSG 位。 |

## 维护建议

- 驱动拓扑变更必须同时检查 GPIO 初始化、驱动状态机、保护影响和 AFE 控制权。
- 不建议在保护模块中直接写 MOS GPIO，应保持保护判断与驱动执行分层。
- 若新增强制开关命令，应定义优先级，避免与故障保护互相覆盖。

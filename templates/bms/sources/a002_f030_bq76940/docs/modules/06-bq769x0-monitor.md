# BQ769x0 监控与 AFE 故障处理

## 相关文件

- [BQ769X0_Func.c](../../Code/Source/BQ769X0_Func.c)
- [BQ769X0_Func.h](../../Code/Include/BQ769X0_Func.h)
- [I2C_AFE1.c](../../Code/Source/I2C_AFE1.c)

## 模块职责

该模块在 AFE 驱动之上实现运行期监控，负责：

- 同步 AFE CHG/DSG MOS 实际状态。
- 周期读取 AFE `SYS_STAT`。
- 处理 AFE 内部 OV、UV、OCD、SCD、XREADY 等状态。
- 在需要时清除 AFE latch 状态。
- 为上层保护与驱动模块提供 AFE 运行状态。

## 主调度

`App_BQ769X0_Monitor()` 在主循环中被调用，内部通常按 1s 节拍执行。它依赖系统启动门控，避免在 AFE 未初始化完成时处理状态寄存器。

## MOS 控制接口

`BQ769X0_DriverMos_Ctrl(chg, dsg)` 通过写 `SYS_CTRL2` 控制 AFE 内部 CHG/DSG 输出位。

默认驱动拓扑中，`IODrivers_030` 的 `u8_DriverCtrl_Right = 1` 表示驱动权交给 AFE，MCU 侧 GPIO 不直接拉动 MOS 主控制，而是通过 AFE CHG/DSG 控制位实现。

## 状态处理

| AFE 状态 | 处理思路 |
| --- | --- |
| `OV` | 若外部保护判定已恢复，则清除 AFE OV 标志。 |
| `UV` | 若外部保护判定已恢复，则清除 AFE UV 标志。 |
| `OCD` | 进入放电过流处理路径，必要时关闭驱动并记录。 |
| `SCD` | 进入短路处理路径，必要时关闭驱动并记录。 |
| `DEVICE_XREADY` | 认为 AFE 设备异常，需要重新初始化或上报错误。 |
| `OVRD_ALERT` | 读取并清除相关状态。 |

## 与其他模块关系

| 上游/下游 | 关系 |
| --- | --- |
| `I2C_AFE1` | 提供寄存器读写和 AFE 初始化。 |
| `Fault` | 提供外部保护判断结果，用于确定是否清除 AFE latch。 |
| `IO_Control` | 通过本模块设置 CHG/DSG 状态。 |
| `LogRecord` | AFE 异常会触发事件记录。 |
| `System_Monitor` | AFE 故障会更新系统错误。 |

## 维护建议

- AFE 内部保护与软件保护同时存在，修改阈值时要同时检查 `Fault` 参数和 AFE 寄存器阈值。
- 清除 AFE latch 前必须确认恢复条件，避免故障条件仍存在时反复清除。
- 如果新增 AFE 型号，需要将寄存器地址、ADC gain/offset 计算和保护阈值转换逻辑隔离出来，避免污染业务模块。

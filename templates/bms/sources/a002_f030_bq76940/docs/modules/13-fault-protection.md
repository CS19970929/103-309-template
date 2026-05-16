# 故障保护与告警

## 相关文件

- [Fault.c](../../Code/Source/Fault.c)
- [Fault.h](../../Code/Include/Fault.h)
- [PubFunc.c](../../Code/Source/PubFunc.c)
- [DataDeal.c](../../Code/Source/DataDeal.c)

## 模块职责

`Fault` 模块负责 BMS 保护和告警判断，包括电压、电流、温度、压差、SOC、MOS 温度、温度断线等条件，并将结果输出给驱动控制、通信和日志模块。

当前 `a002_f030_bq76940` profile 使用 MCU 软件保护作为主保护口径：

| 配置 | 当前值 | 含义 |
| --- | --- | --- |
| `PROJECT_CFG_PROTECTION_MCU_SOFTWARE` | `1` | `App_WarnCtrl()` 执行软件保护判断。 |
| `PROJECT_CFG_PROTECTION_AFE_HARDWARE` | `0` | AFE 不作为主保护 owner。 |
| `PROJECT_PROTECTION_USES_MCU_SOFTWARE` | `1` | `Fault.c` 的保护判定参与驱动关闭决策。 |

`BQ76940` 的 OCD/SCD 等硬件状态仍可作为快速辅助或 latch 状态来源，但当前模板的可移植主线是软件保护。后续切换到其他 AFE 时，应该先改 profile 和 `Project_Protection.h`，不要在业务函数里散落 AFE 型号判断。

## 输入数据

| 输入 | 来源 |
| --- | --- |
| 单体电压、总压、电流、温度 | `g_stCellInfoReport` |
| 保护阈值与恢复阈值 | 内部 Flash 参数区中的 `PRT_E2ROMParas` |
| AFE 内部 OCD/SCD 状态 | `BQ769X0_Func` |
| 系统错误状态 | `System_Monitor` |

## 保护等级

代码中存在多级保护/告警概念，典型包括：

- 一级告警。
- 二级保护。
- 三级保护或严重故障。

驱动控制模块通常更关注会影响 CHG/DSG 关闭的高等级保护。

## 典型保护项

| 保护项 | 说明 |
| --- | --- |
| 单体过压/欠压 | 基于最大/最小单体电压。 |
| 电池包过压/欠压 | 基于 Pack 电压。 |
| 充电过流 | 基于当前电流方向与阈值。 |
| 放电过流 | 基于当前电流方向与阈值。 |
| 充电高温/低温 | 基于外部温度。 |
| 放电高温/低温 | 基于外部温度。 |
| MOS 高温 | 基于 MOS 温度 ADC。 |
| 单体压差过大 | 基于最大/最小单体压差。 |
| SOC 低 | 基于 SOC 估算结果。 |
| 温度断线 | 基于温度采样异常检测。 |

## 判定方式

模块大量使用 `App_PubOPUPChk()` 这类公共滞回/滤波判断函数，避免采样噪声导致保护频繁抖动。典型保护判断包含：

- 触发阈值。
- 恢复阈值。
- 触发滤波时间。
- 恢复滤波时间。
- 当前保护状态。

## 输出

| 输出 | 用途 |
| --- | --- |
| `Protect_Flag` | 驱动控制、通信输出。 |
| `Warn_Flag` | 通信输出、日志记录。 |
| 故障记录 | `LogRecord` 和内部 Flash 历史记录。 |
| 驱动关闭条件 | `IO_Control` 读取后关闭 CHG/DSG。 |

## 维护建议

- 新增保护项要同时定义触发、恢复、滤波、驱动影响和日志事件。
- 阈值修改应与 AFE 硬件保护阈值保持一致或明确分层，否则可能出现软件未保护但 AFE 已 latch 的情况。
- 保护恢复逻辑必须与 AFE latch 清除逻辑配合，避免反复恢复/触发。

# 保护恢复逻辑梳理

生成日期：2026-05-19

## 1. 模块边界

保护恢复由三条链路共同完成：

| 链路 | 主要文件 | 职责 |
| --- | --- | --- |
| 软件保护判断 | `Fault.c`、`Fault.h`、`PubFunc.c` | 按配置阈值、恢复阈值和滤波时间生成一二三级故障位 |
| AFE 硬件保护恢复 | `DataDeal.c`、`SH367309_Func.c`、`SH367309_Func.h` | 读取 AFE 标志，满足恢复条件后清 FLAG1/FLAG2 |
| MOS/继电器动作 | `IO_Control.c`、`IODrivers.c`、`IODrivers.h` | 根据故障位、AFE 标志和外部强制关闭条件决定 CHG/DSG 开关 |

主循环顺序上，`App_AFEGet()` 先刷新采样并处理 AFE 标志，`App_WarnCtrl()` 随后更新软件保护位，`App_MOS_Relay_Ctrl()` 在 AFE 采样函数末尾执行。

## 2. 软件保护判断

`App_WarnCtrl()` 每轮主循环依次检查：

- 单体过压/欠压二级、三级；
- 总压过压/欠压二级、三级；
- MOS 过温二级、三级；
- 单体压差二级、三级；
- 放电/充电过流二级、三级；
- SOC 低二级、三级；
- 充电/放电高低温二级、三级。

一级保护函数仍保留在文件中，但当前 `App_WarnCtrl()` 没有调一级检查。

所有软件保护参数集中在 `PRT_E2ROMParas`：

- 每类保护都有 `First/Second/Third/Rcv/Filter` 字段。
- 电压、电流、温度、压差、SOC 均复用同一套滞回判断函数。
- 故障位写入 `g_stCellInfoReport.unMdlFault_Second` 或 `unMdlFault_Third`，上报和 MOS 控制都读取这里。

## 3. 公共滞回函数

`App_PubOPUPChk()` 是软件保护的核心：

```text
未触发状态：
  当前值 >= 保护阈值 OPValB，计数达到 TimeCntB 后置故障
  当前值低于保护阈值，计数递减

已触发状态：
  当前值 <= 恢复阈值 OPValS，计数达到 TimeCntS 后清故障
  当前值高于恢复阈值，计数递减
```

`u8FlagLogic` 用于区分过压/过流/高温这类“大于触发”和欠压/低温这类“小于触发”的逻辑。每个保护函数会把旧故障位传入 `u8FlagBit`，函数返回后再写回对应故障位。

故障从 0 变 1 时会调用 `FaultWarnRecord()` 和 `FaultWarnRecord2()`。当前 `FaultWarnRecord()` 主体被 `#if 0` 关闭，实际记录主要走 `FaultWarnRecord2()`。

## 4. AFE 硬件保护标志

AFE 标志通过宏读取 `Registers_AFE1.flag1/flag2`：

| AFE 标志 | 软件映射 |
| --- | --- |
| `is_AFE_COV` | 单体过压三级 |
| `is_AFE_CUV` | 单体欠压三级 |
| `is_AFE_OCC` | 充电过流三级 |
| `is_AFE_ODC` | 放电过流三级 |
| `IS_AFE_SC` | 短路/CBC 放电错误 |
| `is_AFE_OTC/UTC/OTD/UTD` | 充放电高低温三级 |

`SH_AFE_GetProtectStatus()` 可以把 AFE FLAG 映射到 `g_stCellInfoReport.unMdlFault_Third`，但当前在 `I2C_AFE1.c` 中调用被注释。现有运行路径主要依赖 `Drivers_External_Ctrl()` 直接读取 AFE 宏并通过 `fault_report()` 记录第三方故障。

## 5. AFE 恢复与清标志

`App_AFEGet()` 在 `UpdateVoltageFromBqMaximo()` 成功后刷新电压、电流、温度，然后处理 AFE 恢复：

| 标志 | 恢复条件 | 动作 |
| --- | --- | --- |
| COV | `VCellMax < u16VcellOvp_Rcv` | `SH_AFE_ClearProtectFlag(AFE_FLAG_OV)` |
| CUV | `VCellMin > u16VcellUvp_Rcv` | `SH_AFE_ClearProtectFlag(AFE_FLAG_UV)` |
| OCC | 充电器不在线 | `SH_AFE_ClearProtectFlag(AFE_FLAG_OCC)` |
| OCD | 负载不在线 | 依次清 `AFE_FLAG_OCD1`、`AFE_FLAG_OCD2` |
| OTC | `TempMax < u16TChgOTp_Rcv` | 清 OTC |
| UTC | `TempMin > u16TchgUTp_Rcv` | 清 UTC |
| OTD | `TempMax < u16TdischgOTp_Rcv` | 清 OTD |
| UTD | `TempMin > u16TdischgUTp_Rcv` | 清 UTD |
| SC | `LOADOFF` 确认负载释放 | 清 SC 并关闭 `CRLD_EN` |

短路恢复是两状态流程：

1. 检测 `IS_AFE_SC` 后置 `System_ErrFlag.u8ErrFlag_CBC_DSG = 1`，写 `SCONF3.CRLD_EN = 2`。
2. 等待 `BSTATUS2.LOADOFF` 后清 SC 标志，并把 `CRLD_EN` 写回 0。
3. AFE SC 标志消失后清 `u8ErrFlag_CBC_DSG`。

`SH_AFE_ClearProtectFlag()` 先设置 `SCONF2.LTCLR`，再写 FLAG1 或 FLAG2。FLAG1 清除使用当前 FLAG1 按位清目标位；FLAG2 清除路径使用 `AFE_Protect | 0xFE` 参与掩码，后续维护时要结合 AFE 手册复核目标位是否可能被误清。

## 6. MOS/继电器保护动作

当前工程选择的是 `_MOS_SAME_DOOR_NO_PRECHG` 路径，`Drivers_Ctrl()` 实际调用 `MosCtrl_SameDoor_NoPreChg()`。

MOS 控制的输入来自 `RefreshData_Drivers()`：

- `Driver_Element.Fault_Flag.all = g_stCellInfoReport.unMdlFault_Third.all`。
- 若启用二级电流保护宏，会把二级充放电过流也并入驱动故障位。
- `Driver_Element.u16_CurChg/u16_CurDsg` 来自当前采样。
- `isforceClose()` 为真时置 `DriverForceExt.b2_DriverOFF_Flag = FORCE_CLOSE_MODE`。

`MosCtrl_SameDoor_NoPreChg()` 的主要动作：

- 任意强制关闭、功能关闭或部分故障组合时同时关闭 CHG/DSG。
- 压差过大同时关闭 CHG/DSG。
- 充电过流时关闭 CHG MOS、保持或打开 DSG MOS；若检测到放电电流大于恢复开管阈值，会允许 CHG MOS 打开。
- 放电过流时关闭 DSG MOS、保持或打开 CHG MOS；若检测到充电电流大于恢复开管阈值，会允许 DSG MOS 打开。
- 过压、充电高/低温时关闭 CHG MOS，允许 DSG MOS。
- 欠压、放电高/低温时关闭 DSG MOS，允许 CHG MOS。
- SOC 低保护的封管逻辑被 `#if 0` 关闭，当前 SOC 低不直接关闭 MOS。

过流还有重复触发处理：2 分钟内重复触发 3 次后，会设置对应 MOS 的强制关闭位，避免保护反复恢复。

## 7. 外部状态机恢复

`Drivers_External_Ctrl()` 是当前项目额外加入的在线状态机：

- 如果 AFE 有 COV/CUV/OCC/OCD/OTC/UTC/OTD/UTD/SC 任一标志，先通过 `fault_report()` 对部分 AFE 标志记录三级故障，然后直接返回，不执行后续 MOS 状态切换。
- 无 AFE 标志时按 `bms_status` 运行：
  - `S_STARTUP`/`S_IDLE`：默认关闭 CHG/DSG，根据充电器/负载在线切入充电或放电状态。
  - `S_DSG`：无负载回空闲，有充电器切充电。
  - `S_CHG`：无负载时关闭 DSG；连续 5 次无充电电流后关闭 CHG，再根据在线状态切回放电或空闲。
- 状态机输出与 `SystemStatus` 不一致且驱动控制权允许时，调用 `SH367309_DriverMos_Ctrl()` 写实际 MOS。

充电器在线和负载在线都按 GPIO 低电平判断：

- `GPIO_CHG_DET/PIN_CHG_DET == 0`：充电器在线。
- `GPIO_DSG_DET/PIN_DSG_DET == 0`：负载在线。

## 8. 强制关闭与错误状态

`isforceClose()` 会导致驱动强制关闭：

- 系统关闭输出、加热关闭输出、CBC 关闭输出；
- AFE1/AFE2 通信错误；
- EEPROM 通信或存储错误；
- CBC 充电/放电错误；
- 温感断线错误。

错误状态由 `System_ERROR_UserCallback()` 统一维护：

- `ERROR_xxx`：增加或置位对应错误。
- `ERROR_REMOVE_xxx`：清除对应错误。
- `ERROR_STATUS_xxx`：查询对应错误。

均衡模块写 AFE 均衡寄存器失败会置 `ERROR_SPI`，成功会清 `ERROR_SPI`。保护恢复文档中要注意：当前 `isforceClose()` 未把 `ERROR_STATUS_SPI` 作为强制关闭条件，但均衡允许条件会检查 SPI 错误。

## 9. 当前风险和维护建议

1. `SH_AFE_GetProtectStatus()` 当前未在主路径调用，AFE 硬件标志不会稳定同步到 `unMdlFault_Third`，MOS 状态机和上报记录存在两套来源。
2. `Drivers_External_Ctrl()` 的 AFE 标志分支包含 `IS_AFE_SC`，但没有对短路调用 `fault_report()`；短路只通过 `u8ErrFlag_CBC_DSG` 表达。
3. `SH_AFE_ClearProtectFlag()` 清 FLAG2 的掩码写法需要结合 SH36735XX 文档复核，避免清错温度保护标志。
4. `App_AFEGet()` 在 SCI 发送或 EEPROM 写参数时会跳过 AFE 采样和恢复处理，密集通信期间保护恢复会延后。
5. `Drivers_External_Ctrl()` 遇到 AFE 标志直接返回，实际开关动作依赖 AFE 内部保护和前一次软件 MOS 状态，后续若改为 MCU 主动恢复，需要重新验证时序。
6. 当前一级保护未在 `App_WarnCtrl()` 中调用，如果上位机或协议仍显示一级保护，需要确认产品需求。

# AFE 保护恢复修复记录

生成日期：2026-05-19

## 1. 修改目标

本次按前置规划先完成两项修复：

1. 修正 `SH_AFE_ClearProtectFlag()` 对 `FLAG1/FLAG2` 的清保护流程，避免温度类 `FLAG2` 掩码错误导致无法按 AFE 文档恢复。
2. 将 AFE 硬件保护标志稳定映射到软件三级故障位，但只做置位，不在硬件标志清除时反向清除软件故障，避免破坏原有软件恢复、滤波和滞回逻辑。

## 2. AFE 清标志修复

涉及文件：

- `103 + 309/Project/Source/SH367309_Func.c`
- `103 + 309/Project/Source/SH367309_Func.h`

修复前风险：

- 原逻辑用 `AFE_Protect & 0xFF00` 判断寄存器分组，对 `FLAG2` 使用 `~(AFE_Protect | 0xFE)` 生成写入值，实际会把低 8 位保护掩码污染成错误结果。
- 清标志后没有回读确认，通信失败或硬件条件未释放时，软件无法知道保护标志仍然存在。

修复后流程：

1. 通过 `AFE_REG_FLAG2` 标记判断目标寄存器是 `FLAG1` 还是 `FLAG2`。
2. 只提取保护枚举低 8 位作为目标 bit mask。
3. 先写 `SCONF2.LTCLR=1`，再写目标 `FLAG1/FLAG2`，清除指定保护位。
4. 清除后回读目标寄存器，并同步 `Registers_AFE1.flag1/flag2.all`。
5. 如果目标 bit 仍然为 1，返回失败并保留故障状态，由后续恢复周期继续判断。
6. 对 `FLAG2` 回读结果保留 `CADC_FLG/VADC_FLG` 缓存，避免清温度保护时误伤转换完成状态记录。

该流程与 AFE 文档要求一致：先置 `SCONF2.LTCLR=1`，再清 `FLAG1/FLAG2` 对应保护标志。

## 3. 硬件保护到软件故障映射

涉及文件：

- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/Fault.c`
- `103 + 309/Project/Source/SH367309_Func.c`

修复前风险：

- `SH_AFE_GetProtectStatus()` 直接把 AFE flag 赋值给软件三级故障位。
- 当 AFE flag 被清除或暂时未读到时，可能绕过 `Fault.c` 中已有的软件恢复阈值、滤波计数和滞回判断，直接把软件故障位清零。
- 短路保护只写 `System_ErrFlag.u8ErrFlag_CBC_DSG`，其他硬件保护与软件三级保护闭环同步不稳定。

修复后策略：

- AFE flag 为 1 时，只把对应软件三级故障位置 1。
- AFE flag 为 0 时，不主动清除对应软件三级故障位。
- 软件故障位仍由原有 `App_*_SecondCheck()`、`App_*_ThirdCheck()` 按恢复阈值和滤波计数清除。
- `App_AFEGet()` 在尝试清除 AFE 标志后同步一次硬件状态。
- `App_WarnCtrl()` 在软件保护检查完成后再同步一次硬件状态，确保仍存在的硬件保护不会被同一周期的软件恢复逻辑覆盖。

映射关系如下：

| AFE flag | 软件故障/错误位 |
| --- | --- |
| `FLAG1.OV_FLG` | `b1CellOvp` |
| `FLAG1.UV_FLG` | `b1CellUvp` |
| `FLAG1.OCC_FLG` | `b1IchgOcp` |
| `FLAG1.OCD1_FLG`、`FLAG1.OCD2_FLG` | `b1IdischgOcp` |
| `FLAG1.SC_FLG` | `u8ErrFlag_CBC_DSG` |
| `FLAG2.OTC_FLG` | `b1CellChgOtp` |
| `FLAG2.UTC_FLG` | `b1CellChgUtp` |
| `FLAG2.OTD_FLG` | `b1CellDischgOtp` |
| `FLAG2.UTD_FLG` | `b1CellDischgUtp` |

## 4. 未纳入本次修改的后续项

本次只执行规划中的第 1、2 项，以下内容保持原状：

- AFE WDT 保护：当前工程未开启 AFE WDT，本次不新增 WDT 保护恢复闭环。
- AFE 内部高温/TOTI：文档描述为进入 Powerdown，不属于普通 `FLAG1/FLAG2` 清除恢复路径，本次不新增恢复策略。
- 软件阈值与 AFE 阈值一致性：本次不调整 MTP 阈值配置和软件保护参数表。


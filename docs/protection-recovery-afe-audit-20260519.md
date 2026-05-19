# 保护恢复与 AFE 硬件保护核对报告

生成日期：2026-05-19  
依据文档：`E:/nas sync win/work/SH36735XX CV0.2C.pdf`

## 1. 总结论

当前工程的保护恢复不是“全部都有且全部正确”。结论分三类：

1. 软件保护：`App_WarnCtrl()` 当前调用的二级、三级保护均有恢复条件，恢复由 `App_PubOPUPChk()` 的滞回阈值和滤波计数完成。
2. AFE 硬件保护：OV、UV、OCC、OCD1、OCD2、SC、OTC、UTC、OTD、UTD 都有代码尝试恢复，恢复方向与 AFE 文档总体一致，即先满足业务释放条件，再置 `SCONF2.LTCLR=1`，再清 `FLAG1/FLAG2` 对应位。
3. 关键问题：`FLAG2` 温度类保护的清标志实现不正确；WDT、内部高温、复位/唤醒/断线等 AFE 状态未纳入当前保护恢复闭环；AFE 硬件保护标志未稳定同步到软件三级故障位。

## 2. AFE 文档确认结果

SH36735XX CV0.2C 文档中，硬件保护退出方式如下：

| AFE 保护 | AFE 标志 | 文档中的退出/解除方式 | 文档页码 |
| --- | --- | --- | --- |
| 过充电 OV | `FLAG1.OV_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `OV_FLG` 清 0 | p12、p39 |
| 过放电 UV | `FLAG1.UV_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `UV_FLG` 清 0 | p12、p39-p40 |
| 放电过流1 OCD1 | `FLAG1.OCD1_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `OCD1_FLG` 清 0 | p13、p39 |
| 放电过流2 OCD2 | `FLAG1.OCD2_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `OCD2_FLG` 清 0 | p13、p39 |
| 短路 SC | `FLAG1.SC_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `SC_FLG` 清 0 | p13、p39 |
| 充电过流 OCC | `FLAG1.OCC_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `OCC_FLG` 清 0 | p14、p39 |
| 充电低温 UTC | `FLAG2.UTC_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `UTC_FLG` 清 0 | p14、p40 |
| 充电高温 OTC | `FLAG2.OTC_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `OTC_FLG` 清 0 | p14、p40 |
| 放电低温 UTD | `FLAG2.UTD_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `UTD_FLG` 清 0 | p15、p40 |
| 放电高温 OTD | `FLAG2.OTD_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `OTD_FLG` 清 0 | p15、p40 |
| 看门狗 WDT | `FLAG2.WDT_FLG` | MCU 置 `SCONF2.LTCLR=1` 后，将 `WDT_FLG` 清 0；超时未清会进入 Powerdown | p17、p40 |
| 内部高温 TOTI | 无普通 FLAG 恢复闭环 | 文档描述为进入 Powerdown 并关闭 MOS | p15 |

`SCONF2.LTCLR` 文档确认点：

- `LTCLR=0` 时不允许清 `FLAG1/FLAG2`。
- MCU 置 `LTCLR=1` 后才允许清保护标志。
- MCU 清零 `FLAG1/FLAG2` 后，`LTCLR` 由 IC 自动清零。

负载释放检测文档确认点：

- `SCONF3.CRLD_EN[1:0]=10` 开启负载状态检测。
- 负载未连接时 `BSTATUS2.LOADOFF=1`。
- 当前代码用这个机制释放短路保护，方向与文档一致。

## 3. 软件保护恢复核对

`Fault.c` 中当前主循环实际调用的是二级和三级保护，一级保护函数存在但未在 `App_WarnCtrl()` 中调用。

| 软件保护 | 当前调用级别 | 恢复方式 | 结论 |
| --- | --- | --- | --- |
| 单体过压 | 二级、三级 | 当前值低于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| 单体欠压 | 二级、三级 | 当前值高于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| 总压过压 | 二级、三级 | 当前值低于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| 总压欠压 | 二级、三级 | 当前值高于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| 充电过流 | 二级、三级 | 当前值低于恢复阈值后，还要满足 `CurOverFaultDelay` 计数 | 有恢复，但恢复时间依赖主循环节拍 |
| 放电过流 | 二级、三级 | 当前值低于恢复阈值后，还要满足 `CurOverFaultDelay` 计数 | 有恢复，但恢复时间依赖主循环节拍 |
| 充电高温 | 二级、三级 | 温度低于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| 充电低温 | 二级、三级 | 温度高于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| 放电高温 | 二级、三级 | 温度低于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| 放电低温 | 二级、三级 | 温度高于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| MOS 高温 | 二级、三级 | 温度低于恢复阈值并满足滤波计数后清故障 | 有恢复 |
| 单体压差大 | 二级、三级 | 压差低于恢复阈值并满足滤波计数后清故障，同时清 `ERROR_VDEATLE_OVER` | 有恢复 |
| SOC 低 | 二级、三级 | SOC 高于恢复阈值并满足滤波计数后清故障 | 有恢复；当前 MOS 封管逻辑被关闭 |

软件保护的主要注意点：

- 一级保护函数存在，但当前不参与主循环。
- `CurOverFaultDelay` 注释按 10 ms 计数理解，但 `App_WarnCtrl()` 当前不是明确 10 ms 门控调用，实际恢复时间取决于主循环运行频率。
- SOC 低保护只形成故障位，`IODrivers.c` 中 SOC 低封管逻辑被 `#if 0` 关闭。

## 4. AFE 硬件保护恢复核对

当前 AFE 恢复逻辑主要在 `DataDeal.c:556-647` 和 `SH367309_Func.c:203-222`。

| AFE 保护 | 当前代码恢复条件 | 清标志调用 | 是否有恢复 | 是否正确 |
| --- | --- | --- | --- | --- |
| OV | `VCellMax < PRT_E2ROMParas.u16VcellOvp_Rcv` | `AFE_FLAG_OV` | 有 | 基本正确 |
| UV | `VCellMin > PRT_E2ROMParas.u16VcellUvp_Rcv` | `AFE_FLAG_UV` | 有 | 基本正确 |
| OCC | 充电器不在线 | `AFE_FLAG_OCC` | 有 | 清 FLAG1 正确；释放条件是项目策略，不是 AFE 文档硬要求 |
| OCD1/OCD2 | 负载不在线 | `AFE_FLAG_OCD1`、`AFE_FLAG_OCD2` | 有 | 清 FLAG1 正确；释放条件是项目策略 |
| SC | `CRLD_EN=2` 后检测 `BSTATUS2.LOADOFF=1` | `AFE_FLAG_SC` | 有 | 方向正确，但未检查写 `SCONF3` 和清标志返回值 |
| OTC | `TempMax < u16TChgOTp_Rcv` | `AFE_FLAG_OTC` | 有 | 不正确，FLAG2 清除掩码错误 |
| UTC | `TempMin > u16TchgUTp_Rcv` | `AFE_FLAG_UTC` | 有 | 不正确，FLAG2 清除掩码错误 |
| OTD | `TempMax < u16TdischgOTp_Rcv` | `AFE_FLAG_OTD` | 有 | 不正确，FLAG2 清除掩码错误 |
| UTD | `TempMin > u16TdischgUTp_Rcv` | `AFE_FLAG_UTD` | 有 | 不正确，FLAG2 清除掩码错误 |
| WDT | 当前未见启用和恢复路径 | 无 | 当前未启用 | 若启用则缺恢复 |
| 内部高温 TOTI | 当前未见软件恢复路径 | 无 | 无 | 文档定义为进入 Powerdown，不能按普通 FLAG 恢复 |

## 5. 关键问题

### P1：FLAG2 清除掩码错误

`SH_AFE_ClearProtectFlag()` 对 FLAG2 的处理如下：

```c
Temp = (Registers_AFE1.flag2.all) & (uint8_t)(~(AFE_Protect | 0xFE));
Result = sh36735_write_reg_u8(AFE_FLAG2, Temp);
```

`AFE_FLAG_OTC/UTC/OTD/UTD` 的枚举值带有 `AFE_REG_FLAG2 = 0x1000`，再与 `0xFE` 做或运算后，低 8 位几乎固定为 `0xFE`，取反后只剩 `0x01`。结果是清温度保护时写入值接近 `flag2 & 0x01`，会把 FLAG2 中除 bit0 外的大部分标志都写 0。

影响：

- 温度类硬件保护恢复不是精确清目标位。
- 可能误清 `WDT_FLG`、`RST2_FLG` 或其他温度标志。
- 与文档“写 0 清对应标志位”的要求不匹配。

建议改为只清目标低 8 位：

```c
UINT8 flag_mask = (UINT8)(AFE_Protect & 0xFFu);
Temp = Registers_AFE1.flag2.all & (UINT8)(~flag_mask);
```

同时建议清除后读回 `FLAG2` 验证目标位确实为 0。

### P1：WDT 硬件保护未纳入恢复闭环

PDF 明确有 `FLAG2.WDT_FLG`，解除方式同样是 `LTCLR=1` 后清 `WDT_FLG`。当前 `InitAFE3520_Registers()` 没有写 `SCONF5` 打开 WDT，按文档复位值 `WDT_EN=0` 推断当前未启用。

如果后续启用 WDT，则需要补：

- WDT 标志读取；
- `AFE_FLAG_WDT` 定义；
- 超时前清 `WDT_FLG`；
- WDT 导致 MOS 关闭/RESET 前后的系统恢复策略。

### P1：内部高温不是普通恢复路径

PDF 中内部高温保护会让系统进入 Powerdown 并关闭 MOS。当前代码没有内部高温状态识别和恢复策略。若产品需要覆盖内部高温，应设计为 Powerdown 后唤醒/重启恢复，而不是简单清 FLAG。

### P2：AFE 硬件保护标志未稳定同步到软件故障位

`SH_AFE_GetProtectStatus()` 可以把 AFE FLAG 映射到 `g_stCellInfoReport.unMdlFault_Third`，但当前在 `I2C_AFE1.c` 中调用被注释。

当前实际效果：

- `Drivers_External_Ctrl()` 直接读取 AFE 宏并通过 `fault_report()` 记录部分故障事件。
- `fault_report()` 不写 `g_stCellInfoReport.unMdlFault_Third`，只做记录去重。
- `IS_AFE_SC` 进入了 AFE 分支，但没有对应 `fault_report()`，只置 `System_ErrFlag.u8ErrFlag_CBC_DSG`。

风险：

- AFE 已触发硬件保护，但软件三级故障位可能不一致。
- 上报、日志、MOS 控制和均衡禁止条件可能看到不同的故障来源。

### P2：清标志缺少写入结果和读回确认

`SH_AFE_ClearProtectFlag()` 会先写 `SCONF2.LTCLR=1`，但没有检查该写是否成功，也没有确认 `LTCLR` 已允许清标志。清 `FLAG1/FLAG2` 后也没有读回确认。

建议：

- 写 `SCONF2` 失败时直接返回失败。
- 清标志后读回目标 FLAG，确认目标位清零。
- 失败时置 `ERROR_SPI` 或专门的 AFE 清保护失败错误。

### P2：硬件阈值与软件阈值不是同一套参数

`InitAFE3520_Registers()` 当前硬件阈值是硬编码：

- OV：4250 mV。
- UV：2500 mV。
- OCD2：寄存器值 3。
- OCC：寄存器值 7。
- 温度阈值使用固定 NTC 电阻值换算。

软件保护使用 `PRT_E2ROMParas`。恢复 AFE 标志时又使用 `PRT_E2ROMParas` 的恢复阈值做释放条件。

风险：

- AFE 触发阈值和软件三级阈值可能不一致。
- 上位机改保护参数后，软件恢复阈值变化，但 AFE 硬件阈值未必同步变化。
- 现场表现可能是 AFE 先保护、软件故障位未触发，或软件已恢复但 AFE 仍反复触发。

## 6. 逐项结论

| 项目 | 结论 |
| --- | --- |
| 软件二级/三级保护是否都有恢复 | 是，均有滞回恢复 |
| 软件一级保护是否在跑 | 否，函数存在但 `App_WarnCtrl()` 未调用 |
| AFE FLAG1 类保护恢复是否都有 | OV、UV、OCC、OCD1、OCD2、SC 都有 |
| AFE FLAG1 类清除方式是否正确 | 基本正确，但缺少 LTCLR 写入检查和清后读回 |
| AFE FLAG2 温度保护恢复是否都有 | OTC、UTC、OTD、UTD 都有恢复尝试 |
| AFE FLAG2 温度保护清除是否正确 | 不正确，掩码会误清其他 FLAG2 位 |
| WDT 硬件保护是否有恢复 | 当前未启用；若启用则缺恢复 |
| 内部高温硬件保护是否有恢复 | 无普通恢复；文档定义为 Powerdown 类处理 |
| AFE 硬件保护是否完整进入软件故障位 | 否，`SH_AFE_GetProtectStatus()` 未启用，SC 也未记录为普通三级故障 |

## 7. 建议修复顺序

1. 先修 `SH_AFE_ClearProtectFlag()` 的 FLAG2 掩码，并增加 SCONF2/FLAG 清除读回验证。
2. 建立 AFE 硬件保护到软件三级故障位的统一同步策略，至少覆盖 OV、UV、OCC、OCD1/2、SC、OTC、UTC、OTD、UTD。
3. 明确 WDT 是否启用；若启用，补 `WDT_FLG` 定义和恢复流程。
4. 明确内部高温 Powerdown 后的系统恢复策略。
5. 统一 AFE 硬件阈值和 `PRT_E2ROMParas` 参数来源，避免硬件保护和软件保护触发/恢复点不一致。
6. 若产品需要一级保护上报或动作，将一级保护加入 `App_WarnCtrl()` 或明确标注一级保护废弃。

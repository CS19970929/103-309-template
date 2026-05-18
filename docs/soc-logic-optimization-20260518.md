# SOC 逻辑与计算优化说明

日期：2026-05-18
主工程：`103 + 309`
核心文件：`103 + 309/Project/Source/SocEnhance.c`

## 优化目标

本次只收敛 SOC 模块内部逻辑，不改变外部接口：

- 保证 SOC 百分比始终限制在 0~100。
- 保证容量积分不再依赖 `UINT32` 无符号溢出兜底。
- 保证 EEPROM 空值、坏值不会直接污染 SOC、循环次数和剩余容量。
- 保证 AFE 电压无效或异常跳变时，不触发端点校准把 SOC 误拉到 0 或 100。
- 保证输出容量、SOH、循环次数换算统一，避免除 0 和 16 位溢出截断。

## 原问题

1. EEPROM 恢复缺少边界

   原逻辑直接把 `ReadEEPROM_Word_NoZone(E2P_ADDR_SOC)` 转成 `UINT8`。如果 EEPROM 为空值 `0xFFFF`，SOC 会变成 255，再参与 `CapNow = SOC * CapFactory / 100`，导致剩余容量异常放大。放电累积百分比和循环次数同样没有合法性判断。

2. 容量为 0 时存在除 0 风险

   SOC 标称容量来自 `OtherElement.u16Soc_Ah`，单位为 `Ah * 10`。如果参数未初始化或被写成 0，`u32CapFactory = u16_SOC_Ah * 3600` 会为 0，后续 `CapChange * 100 / CapFactory` 和 SOH 计算都会除 0。

3. 充放电积分依赖无符号溢出

   原放电逻辑先执行 `u32CapNow -= Idsg`，再用 `if (u32CapNow > u32CapFactory) u32CapNow = 0` 判断下溢。这种写法可读性差，也容易在后续改动中被误删。

4. 百分比和容量输出未统一限幅

   `SOC_Result_Pass()` 直接把内部容量除以 360 输出到 `UINT16`，大容量配置下可能截断。SOH 也直接除以 `u32CapFactory`，没有统一的容量有效性保护。

5. 电压滤波函数没有接入主流程

   `SOC_Data_Filter()` 原本用于过滤 `VCellMax` 和 `VCellMin` 之间超过 600 mV 的异常跳变，但 `SOC_IntEnhance_Ctrl()` 没有调用，导致端点校准直接使用原始电压。

6. 低压校准可能在非放电状态触发

   `soc_cali()` 中低压置 0 的逻辑不受 `else` 限制，充电或静置时只要最低单体低于 SOC 0% 电压，也可能计时后把 SOC 清零。

## 关键计算口径

- 输入容量参数：`u16_SOC_Ah`，单位 `Ah * 10`。
- 电流输入：`u16_Ichg/u16_Idsg`，单位 `A * 10`。
- 内部容量：`u32CapFactory/u32CapNow/u32CapChange`，单位等价于 `(A * 10) * s`。
- 标称容量计算：`u32CapFactory = u16_SOC_Ah * 3600`。
- 输出容量：`Ah * 100 = 内部容量 / 360`。
- SOC 变化百分比：`CapChange * 100 / CapFactory`，本次改为 `uint64_t` 中间值，避免大容量参数下乘 100 溢出。

## 本次修复

1. 新增 SOC 内部安全函数

   新增 `SOC_ClampPercent()`、`SOC_SetSocAndCapacity()`、`SOC_AddCapNow()`、`SOC_SubCapNow()`、`SOC_CapChangeToPercent()`、`SOC_CapChangeRemainder()`、`SOC_PassResultNow()` 等函数，统一处理限幅、积分、输出换算和除 0 保护。

2. EEPROM 恢复加合法性判断

   - SOC 只接受 `0~100`，否则优先使用有效单体电压做 OCV 估算；电压无效时回落到 60%。
   - 放电循环累积百分比只接受 `<80`，否则清 0。
   - 循环次数遇到 `0xFFFF` 时使用配置中的历史循环次数，并按循环寿命上限限幅。

3. 充放电积分改为饱和计算

   - 充电剩余容量使用 `SOC_AddCapNow()`，超过标称容量直接钳位到满。
   - 放电剩余容量使用 `SOC_SubCapNow()`，不足扣减时直接钳位到 0。
   - SOC 百分比增加/减少都显式判断边界，不再依赖 `UINT8` 或 `UINT32` 回绕。

4. 端点校准加电压有效性保护

   `CorrectionTerminal_CV()` 和 `soc_cali()` 在电压无效时直接退出，避免 AFE 未读回、通信异常、单体电压跳变时误校准。

5. 低压置 0 只在放电状态触发

   `soc_cali()` 中 SOC 置 0 改为 `else if (isDSG())`，充电状态只允许满电校准，放电状态才允许过放端校准。

6. 主流程接入电压滤波

   `SOC_IntEnhance_Ctrl()` 入口调用 `SOC_Data_Filter()`，先处理电压突变，再进入状态机、端点校准、EEPROM 维护和输出。

## 需要硬件验证的点

- EEPROM 空片或 SOC 区域为 `0xFFFF` 时，开机 SOC 应为 OCV 估算值；如果 AFE 电压尚未有效，应为 60%。
- 低压放电持续 10 秒后，SOC 应置 0；充电或静置低压不应直接置 0。
- 满电充电条件满足时，SOC 应置 100，且容量变化累积量清零。
- AFE 单体电压异常跳变超过 600 mV 时，SOC 端点校准不应误动作。
- 通讯上报的 `SOC/SOH/CapacityNow/CapacityFull/CapacityFactory/Cycle_times` 应稳定在合法范围内。

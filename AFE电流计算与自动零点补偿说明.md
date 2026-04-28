# AFE 电流计算与自动零点补偿说明

## 现有链路

1. `UpdateVoltageFromBqMaximo()` 读取 SH367309 的 `Cadc` 寄存器，保存到 `SH367309_Read_AFE1.u16Current`。
2. `App_AFEGet()` 每 200ms 调用一次 `DataLoad_Current()`。
3. `DataLoad_Current()` 统一完成原始码解释、零点补偿、mA 换算、K/B 修正和 A*10 输出。
4. 输出值写入 `g_stCellInfoReport.u16Ichg` / `g_stCellInfoReport.u16IDischg`，后续 CAN、SOC、保护、休眠唤醒都使用这两个值。

## 新计算流程

`DataLoad_Current()` 已拆成独立步骤：

1. 原始 16bit 码按二补码转换为有符号采样码。
2. 在原始采样码层做自动零点补偿，得到 `corrected_raw`。
3. 对 `corrected_raw` 取绝对值，按 `raw * 200mV * g_u32CS_Res_AFE / 21470` 换算为 mA。
4. 2A 以上继续应用当前 K/B 校准参数；2A 以内不叠加 B 值，避免校准 B 把零点重新抬高。
5. mA 四舍五入转 A*10，`<= 0.3A` 的输出保持为 0。

## 自动零点策略

自动补偿不要求人工校准，也不写 Flash：

- 初次建立零点：原始采样电流必须小于 `AFE_CURRENT_AUTO_ZERO_LIMIT_MA`，并连续稳定 `AFE_CURRENT_AUTO_ZERO_CONFIRM_CNT` 次。
- 稳定判定：相邻原始采样码变化不超过 `AFE_CURRENT_AUTO_ZERO_STABLE_RAW`。
- 建立后跟踪温漂：只有补偿后的电流仍处在输出死区内，才用 1/16 的慢速滤波更新零点。
- 正常充放电时：只应用已学习到的零点，不继续学习，避免把真实负载电流吸收到零点。

当前参数按 200ms 周期计算，约 3.2s 可确认一次稳定零点。

## 边界说明

纯软件无法在“真实小电流长期稳定”和“零点偏移长期稳定”之间做到绝对区分。因此策略采用保守窗口：

- 零点只在小电流、稳定、且后续处于死区时学习。
- 如果设备上电时就存在稳定小负载，且落在自动零点建立窗口内，软件仍可能误认为零点。
- 若现场零漂超过 `AFE_CURRENT_AUTO_ZERO_LIMIT_MA`，需要先确认采样电阻、AFE 参考、走线和寄存器配置；继续放大自动窗口会增加吞掉真实小电流的风险。

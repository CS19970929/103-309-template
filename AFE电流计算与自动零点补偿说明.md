# AFE 电流计算与自动零点补偿说明

## 现有链路

1. `UpdateVoltageFromBqMaximo()` 读取 SH367309 的 `Cadc` 寄存器，保存到 `SH367309_Read_AFE1.u16Current`。
2. `App_AFEGet()` 每 200ms 调用一次 `DataLoad_Current()`。
3. `DataLoad_Current()` 统一完成原始码解释、零点补偿、mA 换算、预留 K/B 修正和 A*10 输出。
4. 输出值写入 `g_stCellInfoReport.u16Ichg` / `g_stCellInfoReport.u16IDischg`，后续 CAN、SOC、保护、休眠唤醒都使用这两个值。

## 启动零点校准

开机初始化 AFE 时，`InitAFE1()` 只在第一次启动采样前执行快速零点校准：

1. `InitIO()` 先把 `MCUO_AFE_CTLC` 置 0，保证 CTLC 默认关闭。
2. `InitAFE1()` 调用 `AfeCurrent_PrepareStartupZero()`，保持 `MCUO_AFE_CTLC = 0`。
3. AFE 配置完成并打开 CADC 后，`AfeCurrent_StartupZeroCal()` 读取 `MTP_ADC2` 的 `Cadc` 原始值。
4. 默认等待 40ms 后最多采样 8 次，间隔 20ms，连续 4 次稳定即确认零点；正常情况下 CTLC 关闭时间约 100ms 级。
5. 校准成功、超时或 I2C 失败都会调用 `open_ctlc()`，避免启动体验被长时间阻塞。

`g_u8AfeCurrentZeroState` 可用于判断结果：`2` 成功，`3` 超时使用已有样本，`4` I2C 失败使用 0 偏移。

## 新计算流程

`DataLoad_Current()` 已拆成独立步骤：

1. 原始 16bit 码按二补码转换为有符号采样码。
2. 在原始采样码层做自动零点补偿，得到 `corrected_raw`。
3. 对 `corrected_raw` 取绝对值，按 `raw * 200mV * g_u32CS_Res_AFE / 21470` 换算为 mA。
4. K/B 校准路径保留在 `DataLoad_CurrentApplyCalib()`，但当前文件内常量 `s_u8AfeCurrentKbCalibEnable = 0U`，电流先直接按硬件采样结果输出。
5. mA 四舍五入转 A*10，`<= 0.3A` 的输出保持为 0。

## 自动零点策略

自动补偿不要求人工校准，也不写 Flash：

- 初次建立零点：原始采样电流必须小于 `AFE_CURRENT_AUTO_ZERO_LIMIT_MA`，并连续稳定 `AFE_CURRENT_AUTO_ZERO_CONFIRM_CNT` 次。
- 稳定判定：相邻原始采样码变化不超过 `AFE_CURRENT_AUTO_ZERO_STABLE_RAW`。
- 建立后跟踪温漂：只有补偿后的电流仍处在输出死区内，才用 1/16 的慢速滤波更新零点。
- 正常充放电时：只应用已学习到的零点，不继续学习，避免把真实负载电流吸收到零点。

启动后若需要跟踪温漂，当前参数按 200ms 周期计算，约 3.2s 可确认一次稳定零点。

## Keil 在线调试建议

重点观察这些全局变量：

- `g_stAfeCurrentObserve`：当前原始码、零点、修正后 raw、mA、A*10、方向、CTLC 软件状态。
- `g_i32AfeCurrentZeroOffsetRawQ4`：零点偏移内部滤波值。
- `g_u8AfeCurrentZeroReady`：零点是否可用。
- `g_u8AfeCurrentZeroState`：零点流程状态，正式逻辑使用它，不依赖观察快照。
- `g_u8AfeCurrentZeroStableCnt`：稳定计数。
- `g_u32AfeCurrentSampleSeq`：AFE 电流样本序号，SOC 依赖它判断是否有新样本。

## 边界说明

纯软件无法在“真实小电流长期稳定”和“零点偏移长期稳定”之间做到绝对区分。因此策略采用保守窗口：

- 零点只在小电流、稳定、且后续处于死区时学习。
- 如果设备上电时就存在稳定小负载，且落在自动零点建立窗口内，软件仍可能误认为零点。
- 若现场零漂超过 `AFE_CURRENT_AUTO_ZERO_LIMIT_MA`，需要先确认采样电阻、AFE 参考、走线和寄存器配置；继续放大自动窗口会增加吞掉真实小电流的风险。

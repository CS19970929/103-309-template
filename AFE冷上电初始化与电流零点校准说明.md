# AFE 冷上电初始化与电流零点校准说明

## 问题现象

板子从完全断电状态再上电时，电流零点校准可能没有生效，表现为主回路电流偏差明显；但普通休眠唤醒后电流读数正常。

这两条路径的关键差异不是 MCU RAM 是否保留。普通休眠流程会执行 `MCU_RESET()`，RAM 同样会重新初始化。真正差异是 AFE 状态：

- 冷上电：SH367309、参考源、CADC 和寄存器状态都处于完整冷启动恢复过程。
- 休眠唤醒：MCU 复位，但 AFE 是从 sleep/低功耗状态恢复，不是完整掉电重启。

因此冷上电时 MCU 可能先跑到 `InitAFE1()` 和启动零点校准，而 AFE CADC 还没有完全稳定。

## 本次修复

启动零点校准入口仍然在 `InitAFE1()`，但增加了启动来源区分和 CADC 稳定保护：

1. `IsSleepStartUp()` 读取 BKP 休眠启动标志后，记录本次是否来自休眠恢复。
2. `InitAFE1()` 在 `AfeCurrent_PrepareStartupZero()` 前调用 `AfeCurrent_SetStartupColdBoot()`。
3. 冷上电使用更保守的 CADC 稳定参数：
   - settle：800ms
   - 最大采样：32 次
   - 前 6 个成功样本丢弃
   - 采样间隔：25ms
4. 休眠复位恢复使用较短参数：
   - settle：120ms
   - 最大采样：16 次
   - 前 2 个成功样本丢弃
   - 采样间隔：20ms
5. 启动零点只接受落在零点窗口内的 CADC 样本，避免真实负载电流或冷启动异常大偏移被误吸收到零点。

## 状态变量

Keil 在线调试时重点看 `g_stAfeCurrentObserve`：

- `u8StartupColdBoot`：1 表示冷上电策略，0 表示休眠恢复策略。
- `u8StartupSampleCnt`：启动零点总采样次数。
- `u8StartupDiscardCnt`：启动阶段丢弃的成功 CADC 样本数。
- `u8StartupFailCnt`：启动阶段 I2C/CADC 读取失败次数。
- `u8StartupRangeFailCnt`：启动阶段样本超出零点窗口次数。
- `u8ZeroState`：2 成功，3 超时使用已有有效样本，4 I2C 失败，5 样本超范围。
- `u8ZeroReady`：1 表示启动零点已建立；0 表示运行期继续等待小电流稳定样本再学习零点。

## 后续规则

- 冷上电不要只靠固定 40ms 延时做 CADC 零点。
- AFE 初始化后必须先等待 CADC 稳定、丢弃早期样本，再建立电流零点。
- 启动样本超出零点窗口时不要强行校准，避免把真实负载电流当成零点。
- 如需调整启动速度，优先调 `s_stAfeCurrentColdStartupZeroParam` 和 `s_stAfeCurrentWarmStartupZeroParam`，不要绕过 `AfeCurrent_StartupZeroCal()`。

## 上板验证

1. 完全断电 10s 后上电，观察 `u8StartupColdBoot == 1`。
2. 无充放电电流时，确认 `u8ZeroState == 2` 或短时 `3`，`i32CorrectedRaw` 接近 0。
3. 休眠后唤醒，观察 `u8StartupColdBoot == 0`。
4. 对比 `g_stCellInfoReport.u16Ichg` / `g_stCellInfoReport.u16IDischg` 与钳表或电子负载读数。

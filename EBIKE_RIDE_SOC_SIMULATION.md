# Ebike 骑行 SOC 模拟说明

## 目的

用于在没有真实电池包和电子负载的情况下，把一组可重复的 ebike 骑行电压、电流输入注入到现有 SOC 算法前端，验证库仑积分、OCV 修正、满电确认和弱单体保护对 SOC 的影响。

默认固件不启用该功能。只有打开 `PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE` 后，`App_AFEGet()` 才会在真实 AFE/ADC 采样完成后覆盖 `g_stCellInfoReport` 中的单体电压、总压、充放电电流，再递增 `g_u32AfeCurrentSampleSeq`，因此 `App_SOC()` 会把模拟值当作正常采样值处理。

## 开关

配置文件：`103 + 309/Project/Source/conf/Project_Config.h`

- `PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE`
  - `0`：关闭，正常使用真实 AFE/ADC 采样。
  - `1`：开启模拟。Release 构建会被 `Project_BuildGuard.h` 拦截，测试时应切到 Debug 或 Factory/Test profile。
- `PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE`
  - `0`：自动骑行工况，包含静置、起步、巡航、爬坡、滑行回弹、短时冲刺和停车恢复，默认只模拟 ebike 放电骑行。
  - `1`：恒定 8A 放电，便于核对库仑积分速度。
  - `2`：手动调试器输入，通过 `EbikeRideSim_SetManualSample(cell_mv, ichg_a10, idsg_a10)` 注入单点。
- `PROJECT_CFG_EBIKE_RIDE_SIM_INITIAL_SOC_PERCENT`
  - 模拟器内部真实 SOC 初值。
- `PROJECT_CFG_EBIKE_RIDE_SIM_CELL_RES_MOHM`
  - 单体等效内阻，决定放电负载压降和轻载回弹幅度。
- `PROJECT_CFG_EBIKE_RIDE_SIM_CELL_IMBALANCE_MV`
  - 单体间最大模拟不一致电压。

## 使用步骤

1. 切到测试构建配置。
   - `PROJECT_CFG_BUILD_PROFILE` 改为 `1` 或 `2`。
   - `PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE` 改为 `1`。
   - Release 配置下打开模拟会被 `Project_BuildGuard.h` 拦截，这是为了避免测试输入进入正式固件。
2. 选择测试工况。
   - 自动骑行：`PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE = 0`
   - 恒定放电：`PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE = 1`
   - 手动注入：`PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE = 2`
3. 对齐初始 SOC。
   - 做精度测试时，让 `PROJECT_CFG_EBIKE_RIDE_SIM_INITIAL_SOC_PERCENT` 与 SOC 初始值一致。
   - 如果 Flash 中已有旧 SOC，先通过调试器调用 `SOC_ResetStoredSnapshotToDefault()`，或通过上位机执行一次性设置 SOC。
4. 编译下载后运行。
   - `App_AFEGet()` 每 200ms 采样一次。
   - 模拟开启后，`EbikeRideSim_Update(200U)` 会在真实采样后覆盖 `g_stCellInfoReport`，然后 `App_SOC()` 使用这些模拟值计算。
5. 对比结果。
   - 真实值看 `g_stEbikeRideSimObserve.u8TruthSoc`。
   - SOC 内核值看 `SOC_Calculate_Element.u8SOC_Now`。
   - 对外显示值看 `g_stCellInfoReport.SocElement.u16Soc`。
   - 显示值默认每 1s 只走 1%，所以短时间测试应以内核值为准。

## 手动注入示例

手动模式下，调试器里调用：

```c
EbikeRideSim_SetManualSample(3700, 0, 80);  // 单体 3700mV，8A 放电
EbikeRideSim_SetManualSample(4180, 10, 0);  // 单体 4180mV，1A 充电
EbikeRideSim_SetManualSample(3600, 0, 0);   // 单体 3600mV，静置
```

注意：`ichg_a10` 和 `idsg_a10` 不能同时非零。如果同时非零，模拟器会保留充电、清零放电。

## 单位

- 单体电压：`mV`
- 总压 `u16VCellTotle`：`10mV`
- 充电/放电电流：`A * 10`
  - `80` 表示 `8.0A`
  - `300` 表示 `30.0A`
- 模拟器真实容量：`A * 10 * s`，与 `SocEnhance.c` 内部容量单位一致。

## 观察点

调试器里查看全局变量：

- `g_stEbikeRideSimObserve.u8TruthSoc`：模拟器内部真实 SOC。
- `g_stEbikeRideSimObserve.u16CellOcv_mV`：按真实 SOC 生成的静态单体 OCV。
- `g_stEbikeRideSimObserve.u16CellLoad_mV`：叠加内阻压降/抬升后的负载单体电压。
- `g_stEbikeRideSimObserve.u16Ichg_A10` / `u16IDsg_A10`：当前模拟电流。
- `g_stCellInfoReport.SocElement.u16Soc`：现有 SOC 算法输出。

测试时重点比较：

```text
SOC 误差 = g_stCellInfoReport.SocElement.u16Soc - g_stEbikeRideSimObserve.u8TruthSoc
```

## 自动化加速测试

固件内已提供 `SocAutoTest_Task()`，用于在板端自动推进 SOC 测试。开启后，`App_SOC()` 不再等待真实 AFE 样本，而是在每次 200ms 调度中连续执行多帧虚拟样本：

```c
#define PROJECT_CFG_BUILD_PROFILE 2
#define PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE 1
#define PROJECT_CFG_SOC_AUTO_TEST_ENABLE 1
#define PROJECT_CFG_SOC_AUTO_TEST_TICKS_PER_CALL 100
```

`PROJECT_CFG_SOC_AUTO_TEST_TICKS_PER_CALL=100` 表示每次 `App_SOC()` 调用推进 100 个虚拟 200ms 样本，相当于约 100 倍时间加速。可以调到 `1` 做单步观察，也可以调到 `200` 或更高缩短台架等待时间；过高会增加单次调度耗时。

自动化测试包含一条真实 ebike 放电骑行曲线：静置、起步大电流、巡航波动、爬坡高负载、滑行回弹、短时冲刺和停车恢复。这里不是随意组合电压和电流，而是先生成电机负载电流，再用同一电流驱动容量和电压模型。每个虚拟 tick 都会重新计算：

- 放电电流：0A 到约 57A，包含加速脉冲和路况波动。
- 单体 OCV：由内部真实 SOC 查表得到。
- 内阻：基础内阻来自 `PROJECT_CFG_EBIKE_RIDE_SIM_CELL_RES_MOHM`，低 SOC 时等效内阻上升。
- 欧姆压降：`V_ohmic = I_dsg * R_eff`。
- 极化压降：一阶 RC 动态，负载持续时逐渐增大，滑行/停车时逐渐恢复。
- 负载电压：`V_cell = OCV(SOC_truth) - V_ohmic - V_polarization`。
- 单体差异：按 cell index 和运行时间生成轻微不一致。

下载后用调试器观察 `g_stSocAutoTestReport`：

- `u8Done=1`：自动测试已结束。
- `u8Passed=1`：全部自动用例通过。
- `u16CaseFailed=0` 且 `u16CasePassed == u16CaseTotal`：通过条件。
- `u16FailCode`：失败定位，百位以上是用例号，低两位是失败原因。
- `u8ActualSoc` / `u8TruthSoc`：当前 SOC 算法结果与模拟真实 SOC。
- `u16ObservedIDsgMin_A10` / `u16ObservedIDsgMax_A10`：确认骑行电流确实在变化。
- `u16ObservedVMin_mV` / `u16ObservedVMax_mV`：确认骑行电压确实在变化。

同时观察 `g_stEbikeRideSimObserve` 可以确认物理链路：

- `u16CellOcv_mV`：由真实 SOC 查表得到的开路电压。
- `u16EffectiveRes_mOhm`：当前等效内阻。
- `u16OhmicSag_mV`：由当前电流和等效内阻计算的瞬时压降。
- `i16Polarization_mV`：一阶极化压降。
- `u16CellLoad_mV`：最终送给 SOC/AFE 数据结构的负载端单体电压。

真实放电骑行用例通过条件是：电流变化幅度至少 8A，单体电压变化幅度至少 60mV，模拟真实 SOC 必须下降，并且 SOC 内核值与模拟真实 SOC 偏差不超过 3%。

## 完整测试用例

以下用例按当前 `SocEnhance.c` 的逻辑分支设计，覆盖启动、积分、端点、OCV 修正、低压保护、容量学习、循环次数和持久化。

| ID | 目标 | 配置/输入 | 运行时间 | 重点观察 | 通过标准 |
| --- | --- | --- | --- | --- | --- |
| SOC-T00 | 正常固件不受影响 | `PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE=0` | 编译运行 | AFE 实测值、SOC 输出 | Keil 构建 0 error；采样仍来自真实 AFE |
| SOC-T01 | 模拟链路冒烟 | `PROFILE=0`，初始 SOC 80% | 2min | `u8Enabled`、单体电压、电流、总压 | 模拟变量非零，`g_stCellInfoReport` 被覆盖，SOC 任务正常刷新 |
| SOC-T02 | 恒流放电库仑积分 | `PROFILE=1`，初始 SOC 与内核 SOC 均为 80%，8A 放电 | 至少 15min | `u8TruthSoc`、`u8SOC_Now`、`u32CapNow` | 27Ah 包约每 121.5s 下降 1%；内核 SOC 与 truth SOC 误差不超过 1% |
| SOC-T03 | 低于电流门限不积分 | 手动：`3700mV, ichg=0, idsg=3` | 2min | `u8Mode`、`u32CapNow` | 电流小于 0.4A 门限，模式进入/保持 RELAX，容量基本不变 |
| SOC-T04 | 电流方向切换 | 手动：8A 放电 2min，再 2A 充电 2min，再静置 | 5min | `u8IntegrateDirection`、`u32CapChange` | 方向切换时积分余量清零，不出现反向跳变 |
| SOC-T05 | 显示 SOC 缓变 | 初始内核 SOC 与显示 SOC 制造 5% 差值 | 10s | `u8SOC_Now`、`SocElement.u16Soc` | 对外显示每秒最多变化 1%，内核值不被显示策略拖慢 |
| SOC-T06 | 满电端点快修正 | 手动：`VcellMax >= V100 + 50mV`，小电流充电 | 5s | `u16ChgTerminalTicks`、`u8SOC_Now` | 满电高压持续约 2s 后 SOC 每次最多上调 1%，最高先到 99% |
| SOC-T07 | 满电确认到 100% | 手动：`Vmax >= V100`、`Vmin >= max(V100-80, 化学体系下限)`、压差 <= 120mV、小电流充电 | 65s | `u16FullConfirmTicks`、`u8SOC_Now`、学习锚点 | 满足 60s 后 SOC=100，容量设为满，形成 full anchor |
| SOC-T08 | 放电端点修正 | 手动：`Vmin <= V0 + 100mV`，持续放电 | 30s | `u16DsgTerminalTicks`、`u8SOC_Now` | 低压端点按 2/4/8/10s 档逐步向下修正 |
| SOC-T09 | 弱单体保护 | 手动：`Vmin <= V0 + 120mV`，无充电 | 10s | `u8SOC_Now`、`u8DSG_SOC_Int` | SOC 被限制到 8/6/4/2/0 档；`Vmin <= V0` 时触发 empty anchor |
| SOC-T10 | 静置 OCV 下修 | 放电后手动静置，电压稳定且 OCV 对应 SOC 低于当前 SOC | 10min/30min/60min/6h 桶 | `u32RestTicks`、`u8RestBucketApplied` | 到达 600/1800/3600/21600s 桶后按 1/1/2/3% 上限下修 |
| SOC-T11 | 高 SOC 静置上修 | 当前 SOC >= 90%，静置 OCV 高于当前 SOC | 60min/6h | `u8RestBucketApplied`、`u8SOC_Now` | 3600s 后最多上修 1%，21600s 后最多上修 2% |
| SOC-T12 | 在线 OCV 轻载修正 | 手动：0.4A 到 C/10 轻载，电压稳定，OCV 与当前 SOC 差值 >= 3% | 2min | `u16OnlineOcvStableTicks`、`u16OnlineOcvTicks` | 稳定 20s + 修正周期 30s 后，每周期向目标修正 1% |
| SOC-T13 | 重载后在线 OCV 禁止 | 先 >= C/3 放电，再切轻载 | 3min | `u16OnlineOcvRecoveryHoldoffTicks` | 重载后 180s 内不执行在线 OCV 修正 |
| SOC-T14 | OCV 目标边界过滤 | 手动给出 OCV 对应 <5% 或 >95% 的电压 | 1min | `u16OnlineOcvTicks` | 在线 OCV 不修正到不可信边界 |
| SOC-T15 | LFP 平台区过滤 | 切 LIFEPO 配置，目标 SOC 落在 20%-90% | 2min | `u16OnlineOcvTicks` | LFP 平台区在线 OCV 不修正 |
| SOC-T16 | 容量学习：满到空 | 满电确认形成 full anchor 后，连续放电到 empty anchor | 完整放电 | `u16LearnState`、`u32LearnPassedAs10`、`u32CapFull` | 首次跨度 >=90% 后更新满容量，单次容量变化不超过 5% |
| SOC-T17 | 容量学习：空到满 | empty anchor 后连续充电到满电确认 | 完整充电 | `u16LearnFlags`、`u16MaxErrorPercent` | 学习成功后误差标记降为 5%，形成 learned flag |
| SOC-T18 | 循环次数累计 | 连续放电累计 80% SOC | 直到累计 80% | `u8DSG_SOC_Int`、`u32Cycle_times` | 每累计 80% 放电，循环次数 +1 |
| SOC-T19 | 快照持久化 | SOC 变化后复位重启 | 重启一次 | `SOC_Calculate_Element_backup`、Flash SOC | 重启后恢复上次 SOC、容量、学习状态和循环计数 |
| SOC-T20 | 保护/系统故障阻断校准 | 手动置三层保护或系统故障，再给 OCV/满电条件 | 1min | `SOC_IsCalibrationAllowed()` 相关结果 | 被配置阻断时，不执行 OCV、满电确认和静置补偿 |

## 建议测试顺序

1. 先跑 `SOC-T00` 到 `SOC-T05`，确认模拟入口、积分和显示没有问题。
2. 再跑 `SOC-T06` 到 `SOC-T09`，验证满/空端点和弱单体低压安全策略。
3. 然后跑 `SOC-T10` 到 `SOC-T15`，验证静置与在线 OCV 修正边界。
4. 最后跑 `SOC-T16` 到 `SOC-T20`，验证长周期容量学习、循环次数和 Flash 快照。

## 关键计算基准

以当前 `BAT_SLAVE` 默认 `BMS_CAPCITY=270` 为例，`OtherElement.u16Soc_Ah = 270`，表示 `27Ah`：

```text
满容量 = 270 * 3600 = 972000 A10*s
1% 容量 = 9720 A10*s
8A 放电 = 80 A10
8A 放电下降 1% 时间 = 9720 / 80 = 121.5s
```

如果修改容量或电流，按同样公式换算预期 SOC 变化速度。

## 注意事项

1. 开启模拟后，真实 AFE 电压和电流会被覆盖，只能用于台架和固件算法测试。
2. 不要同时注入充电和放电电流，手动接口检测到两者同时非零时会保留充电、清零放电。
3. 测试前建议清除或重置 SOC 快照，否则 Flash 中旧 SOC 会影响初始误差。
4. 自动工况的 OCV 表没有直接复用 SOC 估算表，避免测试结果过度乐观。
5. 若要模拟新的路况，只需要调整 `EbikeRideSim.c` 中 `s_stAutoProfile` 的分段时长、电流和额外压降。

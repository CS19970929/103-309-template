# SOC 自动化测试与 ebike 放电骑行模拟交接文档

## 1. 背景

本次需求来自对现有 SOC 模块准确度的测试需求。目标是在没有真实 ebike 电池包、真实电机控制器和电子负载的情况下，在固件内部模拟一段可重复的 ebike 放电骑行过程，并用它自动化验证 SOC 计算逻辑。

对话过程中需求逐步收敛为：

1. 在程序中模拟 ebike 骑行，用于测试 SOC 准确度。
2. 能够模拟不同电压、电流。
3. 根据当前 SOC 模块逻辑给出完整测试用例。
4. 修复测试配置下的编译问题。
5. 能够自动化测试，并能判断测试是否成功、SOC 逻辑是否可靠。
6. 能够加速测试。
7. 加速时电压、电流也必须变化，模拟真实 ebike 放电骑行。
8. 电压、电流必须满足基本物理关系，不能是随意组合值。

当前实现以固件内测试为主：测试代码默认关闭，只在 Factory/Test 或 Debug 配置中显式开启。

## 2. 核心需求定义

### 2.1 功能需求

- 支持在固件内生成 ebike 放电骑行输入。
- 支持一键自动推进 SOC 测试，无需手动逐点注入。
- 支持加速运行，避免长时间等待真实 200ms 周期。
- 支持调试器观察自动测试报告。
- Release 构建必须防止测试输入进入正式固件。

### 2.2 物理约束

自动骑行曲线不能直接拼接任意电压和电流。当前约束为：

```text
I_dsg(t) = 骑行工况负载电流
SOC_truth(t) = SOC_truth(t-1) - I_dsg(t) * dt / 容量
OCV(t) = OCV_Table(SOC_truth)
R_eff(t) = R_base + low_soc_gain
V_ohmic(t) = I_dsg(t) * R_eff(t)
V_polarization(t) = 一阶动态极化压降
V_cell_load(t) = OCV(t) - V_ohmic(t) - V_polarization(t)
V_cell_i(t) = V_cell_load(t) + cell_imbalance_i(t)
```

也就是说，电流先由骑行负载产生；容量下降和端电压都由同一个电流驱动，保证输入之间有基本一致性。

## 3. 当前已实现内容

### 3.1 ebike 骑行模拟

文件：

- `103 + 309/Project/Source/EbikeRideSim.c`
- `103 + 309/Project/Source/EbikeRideSim.h`

当前支持：

- 自动放电骑行 profile。
- 恒流放电 profile。
- 手动调试器注入接口。
- 内部真实 SOC 和真实容量跟踪。
- 三元/LFP OCV 查表。
- 单体内阻压降。
- 低 SOC 下等效内阻上升。
- 一阶极化压降。
- 单体间轻微不一致。
- 输出覆盖 `g_stCellInfoReport`，让现有 `App_SOC()` 按真实采样路径处理。

关键观察量：

- `g_stEbikeRideSimObserve.u8TruthSoc`
- `g_stEbikeRideSimObserve.u16CellOcv_mV`
- `g_stEbikeRideSimObserve.u16EffectiveRes_mOhm`
- `g_stEbikeRideSimObserve.u16OhmicSag_mV`
- `g_stEbikeRideSimObserve.i16Polarization_mV`
- `g_stEbikeRideSimObserve.u16CellLoad_mV`
- `g_stEbikeRideSimObserve.u16IDsg_A10`

### 3.2 SOC 自动化测试入口

文件：

- `103 + 309/Project/Source/SocAutoTest.c`
- `103 + 309/Project/Source/SocAutoTest.h`

自动测试通过 `SocAutoTest_Task()` 运行。开启后 `App_SOC()` 会进入自动测试路径，不再等待真实 AFE 样本。

测试报告：

- `g_stSocAutoTestReport.u8Done`
- `g_stSocAutoTestReport.u8Passed`
- `g_stSocAutoTestReport.u16CaseTotal`
- `g_stSocAutoTestReport.u16CasePassed`
- `g_stSocAutoTestReport.u16CaseFailed`
- `g_stSocAutoTestReport.u16FailCode`
- `g_stSocAutoTestReport.u8ActualSoc`
- `g_stSocAutoTestReport.u8TruthSoc`
- `g_stSocAutoTestReport.u16ObservedVMin_mV`
- `g_stSocAutoTestReport.u16ObservedVMax_mV`
- `g_stSocAutoTestReport.u16ObservedIDsgMin_A10`
- `g_stSocAutoTestReport.u16ObservedIDsgMax_A10`

当前自动测试用例：

| 用例 | 目的 |
| --- | --- |
| `GATE_IDLE` | 低于电流门限时 SOC 不应积分 |
| `REAL_RIDE_DSG` | 真实 ebike 放电骑行曲线，校验 SOC 与 truth SOC 接近 |
| `DSG_COULOMB` | 高放电电流下库仑积分下降 |
| `CHG_CLAMP_99` | 充电积分不能直接越过 99% |
| `FULL_CONFIRM` | 满电确认后 SOC 到 100% |
| `LOW_GUARD` | 低压保护将 SOC 拉到 0% |
| `ONLINE_OCV` | 在线 OCV 轻载修正 |
| `RTC_RELAX` | RTC 静置补偿下修 |

### 3.3 加速测试

新增配置：

```c
#define PROJECT_CFG_SOC_AUTO_TEST_ENABLE 0
#define PROJECT_CFG_SOC_AUTO_TEST_TICKS_PER_CALL 100
```

`PROJECT_CFG_SOC_AUTO_TEST_TICKS_PER_CALL=100` 表示每次 `App_SOC()` 调用推进 100 个虚拟 200ms 样本。当前真实骑行用例 `SOC_AUTO_TEST_REAL_RIDE_TICKS=4500`，代表 900s 虚拟骑行；100 倍加速时大约 9s 完成该用例。

### 3.4 构建保护

文件：

- `103 + 309/Project/Source/conf/Project_BuildGuard.h`

已增加：

- `PROJECT_CFG_SOC_AUTO_TEST_ENABLE` 只能是 0/1。
- `PROJECT_CFG_SOC_AUTO_TEST_TICKS_PER_CALL` 限制为 1 到 1000。
- 自动测试依赖 ebike 模拟开启。
- 自动测试要求 `PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE=0`。
- Release 构建禁止开启 ebike 模拟和 SOC 自动测试。

### 3.5 Keil 工程接入

文件：

- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`

已将 `SocAutoTest.c` 加入两个 Keil target 的 `Source` group。

## 4. 如何使用

### 4.1 自动化测试配置

测试时建议配置：

```c
#define PROJECT_CFG_BUILD_PROFILE 2
#define PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE 1
#define PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE 0
#define PROJECT_CFG_SOC_AUTO_TEST_ENABLE 1
#define PROJECT_CFG_SOC_AUTO_TEST_TICKS_PER_CALL 100
```

正式固件应配置：

```c
#define PROJECT_CFG_BUILD_PROFILE 0
#define PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE 0
#define PROJECT_CFG_SOC_AUTO_TEST_ENABLE 0
```

### 4.2 下载后观察

运行后在调试器观察：

```text
g_stSocAutoTestReport
g_stEbikeRideSimObserve
SOC_Enhance_Element
```

自动测试通过条件：

```text
g_stSocAutoTestReport.u8Done == 1
g_stSocAutoTestReport.u8Passed == 1
g_stSocAutoTestReport.u16CaseFailed == 0
g_stSocAutoTestReport.u16CasePassed == g_stSocAutoTestReport.u16CaseTotal
```

失败定位：

```text
u16FailCode = case_id * 100 + reason
```

真实骑行用例重点看：

```text
u8ActualSoc 和 u8TruthSoc 偏差 <= 3%
u16ObservedIDsgMax_A10 - u16ObservedIDsgMin_A10 >= 80
u16ObservedVMax_mV - u16ObservedVMin_mV >= 60
```

## 5. 当前验证结果

已用 Keil MDK ARMCC 编译验证：

1. 自动测试开启：
   - `PROJECT_CFG_SOC_AUTO_TEST_ENABLE=1`
   - 结果：`0 Error(s), 16 Warning(s)`
   - 固件大小：`Code=59500 RO-data=3588 RW-data=1292 ZI-data=5892`

2. 自动测试默认关闭：
   - `PROJECT_CFG_SOC_AUTO_TEST_ENABLE=0`
   - 结果：`0 Error(s), 16 Warning(s)`
   - 固件大小：`Code=57976 RO-data=3588 RW-data=1300 ZI-data=5852`

当前 16 个 warning 是工程已有告警，非本次自动测试新增的阻断问题。之前自动测试路径中出现过不可达/未引用变量告警，已经通过 `#if/#else` 分支消除。

## 6. Review 结论

### 6.1 已满足的点

- 自动测试默认关闭，Release 下有编译保护。
- ebike 自动 profile 已改为放电骑行，不再包含小电流充电段。
- 电压由电池模型推导，不再直接把路况段里的任意电压偏移和电流拼在一起。
- 自动测试支持加速，每个虚拟 tick 都会重新生成电流、电压、truth SOC。
- 自动测试有统一报告结构，可用调试器判断是否通过。
- 工程能编译通过，Keil 工程已包含新增源文件。

### 6.2 当前模型边界

当前模型是固件内轻量等效电路模型，不是完整电化学模型：

- OCV 表是简化表，未按温度、老化、倍率细分。
- 内阻只按 SOC 粗略上升，未按温度、SOH、倍率、单体差异分别建模。
- 极化是一阶近似，参数未由实测 HPPC 数据标定。
- 单体不一致只是时间/序号生成的轻微扰动，不代表真实弱单体演化。
- 电机负载电流曲线是可重复的工况脚本，不是由车速、坡度、风阻、滚阻和控制器效率反推。

这些限制不影响它作为 SOC 回归测试输入源，但如果后续要做高可信度算法标定，需要引入实测日志或更完整的车辆动力学模型。

### 6.3 当前测试覆盖不足

自动测试已经覆盖了部分关键逻辑，但还没有完全覆盖手动测试矩阵中的所有用例：

- 容量学习满到空、空到满还未自动化。
- Flash 快照持久化重启恢复还未自动化。
- 保护/系统故障阻断校准还未自动化。
- LFP 平台区在线 OCV 过滤还未自动化。
- 重载后在线 OCV holdoff 还未独立自动化。
- 显示 SOC 缓变策略还未独立自动化。

这些适合作为下一阶段补充。

## 7. 后续完善建议

### 7.1 短期

1. 在自动测试报告中增加 `u16CurrentCaseId` 和 `u32VirtualSeconds`，调试器观察会更直接。
2. 给真实骑行用例增加 `max_soc_error` 峰值记录，而不是只检查结束时误差。
3. 给 `g_stEbikeRideSimObserve` 增加 `u16TerminalVoltage_10mV`，便于和总压显示一致核对。
4. 将真实骑行 profile 参数移到配置或表格文件，方便按不同车型调整。
5. 增加自动测试完成后保持最后失败现场，不自动进入下一用例，便于调试。

### 7.2 中期

1. 引入实车 CAN/串口日志回放，把真实 `V/I/T/SOC` 数据转换成固件内回放输入。
2. 为 SOC 模块增加 PC 端单元测试 harness，把 `SocEnhance.c` 从硬件全局变量中解耦一部分。
3. 增加温度维度：低温内阻上升、低温容量折减、高温恢复。
4. 增加 SOH 维度：容量衰减、内阻增长，验证容量学习逻辑。
5. 增加弱单体场景：某一串内阻更高、容量更低，验证低压保护和 SOC 下修。

### 7.3 长期

1. 建立 SOC 回归测试基线，每次修改 SOC 算法都运行自动用例并记录报告。
2. 将 `g_stSocAutoTestReport` 扩展为可通过上位机读取的测试结果。
3. 在 CI 或构建脚本中增加至少一个自动测试配置编译目标。
4. 用实测数据标定 OCV 表、内阻模型和极化时间常数。

## 8. 关键文件清单

| 文件 | 作用 |
| --- | --- |
| `103 + 309/Project/Source/EbikeRideSim.c` | ebike 放电骑行模拟、OCV/内阻/极化/单体差异模型 |
| `103 + 309/Project/Source/EbikeRideSim.h` | 模拟器观察结构和接口 |
| `103 + 309/Project/Source/SocAutoTest.c` | SOC 自动测试用例和加速推进 |
| `103 + 309/Project/Source/SocAutoTest.h` | 自动测试报告结构 |
| `103 + 309/Project/Source/SOC.c` | 自动测试开启时接管 SOC 任务 |
| `103 + 309/Project/Source/SocEnhance.c` | 测试模式下设置/读取 SOC 内核值 |
| `103 + 309/Project/Source/conf/Project_Config.h` | 自动测试开关和加速倍率配置 |
| `103 + 309/Project/Source/conf/Project_BuildGuard.h` | 测试配置合法性和 Release 防护 |
| `EBIKE_RIDE_SOC_SIMULATION.md` | 使用说明、测试矩阵和自动化测试说明 |

## 9. 当前工作区注意事项

当前为了编译测试，工作区中可能保留了本地测试配置：

```c
#define PROJECT_CFG_BUILD_PROFILE 2
#define PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE 1
```

这类配置适合台架测试，不应进入正式发布固件。正式发布前应恢复为 Release 且关闭模拟。

另外，工作区中还有与本需求无关的本地改动，例如 `conf.h`、`todo.md`、部分 Keil 日志和调试目录。后续提交时应继续避免混入。

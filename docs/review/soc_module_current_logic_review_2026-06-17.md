# SOC 模块当前逻辑梳理与评价

日期：2026-06-17

## 结论摘要

本次梳理以当前工作区源码为准。当前 SOC 模块的主线是“AFE 电流 200ms 采样驱动的容量积分”，电压相关逻辑只作为校准层：满电锚定、低压尾段下修、长静置 OCV 下修、RTC STOP 静置补偿。

当前逻辑方向整体是对的：以库仑积分为主，电压只做慢校正，避免用瞬时电压直接跳 SOC。但当前工作区版本处在“源码、测试、历史文档不一致”的状态，不能直接判断为可交付稳定版本。尤其是 low-tail 已从表驱动改成插值和快速低压分支，但 `tools/soc_replay_test.py` 还在找旧 `s_empty_tail_table`，`tools/run_soc_host_c_test.py` 也有低压尾段用例失败。

## 当前源码边界

SOC 主要由两个源码文件承担：

- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/SocEnhance.c`

头文件入口：

- `103 + 309/Project/Source/SOC.h`
- `103 + 309/Project/Source/SocEnhance.h`

相关外部边界：

- `DataDeal.c`：从 AFE CADC 计算 `u16Ichg/u16IDischg`，并递增 AFE current sample seq。
- `Runtime.c`：启动时 `InitData_SOC()`，运行时通过 `App_AFEGet()` 间接调用 `App_SOC()`。
- `Flash.c` / `Flash.h`：SOC snapshot A/B journal 存储。
- `LowPowerSleep.c`：休眠前保存 SOC snapshot。
- `rtc_sleep_port.c`：HICCUP STOP 唤醒后调用 RTC 静置 SOC 补偿。
- `Sci_Upper.c`：上位机写 SOC 参数、写一次 SOC。
- `Can_HDX.c` / `CanFeidaoFrames.c` / `LedBar.c`：读取已发布的 `g_stCellInfoReport.SocElement`。

## 主调度链

当前运行链路：

```text
Runtime_RunOnce()
  App_AFEGet()
    UpdateVoltageFromBqMaximo()
    DataLoad_CellVolt()
    DataLoad_Temperature()
    DataLoad_Current()
    AfeCurrent_NextSeq()
    App_SOC()
      AfeCurrent_GetSeq()
      SOC_GetNetCurrentMilliAmp(u16Ichg, u16IDischg)
      SOC_IntEnhance_Ctrl(net_current_ma)
```

`App_SOC()` 只在 AFE current sample seq 变化时进入核心算法；没有新样本时只重新发布当前 SOC 数据。这个设计是合理的，可以避免重复积分。

## 电流输入

`DataLoad_Current()` 当前做以下事情：

1. 读取 `SH367309_Read_AFE1.u16Current`。
2. 转 signed raw。
3. 应用启动零点/自动零点。
4. raw 转 mA。
5. 小于 `AFE_CURRENT_OUTPUT_DEADBAND_MA` 时输出 0。
6. 按 raw 正负分别写：
   - `g_stCellInfoReport.u16Ichg`
   - `g_stCellInfoReport.u16IDischg`

`SOC.c` 再把两个 unsigned `A * 10` 字段合成 signed mA：

```text
net_current_ma = (u16Ichg - u16IDischg) * 100
```

当前工作区版本中，`SOC.c` 没有把 Type-C 输出电流折算进 SOC 输入。历史文档和部分检查脚本仍描述 `SOC_GetTypeCBatEquivCurrentA10()`，但当前源码里该函数不存在。这是一个明确的文档/工具/源码不一致点。

## 核心状态

`SocEnhance.c` 内部只有一个私有状态 `s_soc`，关键字段：

- `cap_factory_as10`：额定容量内部单位。
- `cap_full_as10`：SOH 修正后的满容量。
- `cap_now_as10`：当前剩余容量。
- `cycle_x100`：循环次数扩大 100。
- `dsg_acc_as10`：放电累计，用于循环计数。
- `rem_mams`：积分余量，避免小电流因为整数除法丢失。
- `soc`：内部真实 SOC，直接发布到外部。
- `soh`：按循环次数估算。
- `full_ticks`：满电确认计数。
- `empty_ticks`：低压尾段计数。
- `sag_hold_ticks`：大电流放电后的回弹保护计数。
- `rest_soc_ticks/stable_rest_soc_ticks/long_rest_down_soc_ticks`：静置 OCV 慢下修计数。
- `rest_down_target/rest_down_valid/rest_ocv_fired`：长静置 OCV 下修目标。
- `snapshot_flags`：当前只保留 rebound hold 标志。

当前已经没有单独的 `display_soc` 平滑层；对外发布直接使用 `s_soc.soc`。

## 初始化与恢复

`InitData_SOC()` 调用 `soc_param_lib_init()`，初始化过程：

1. 清零 `s_soc`。
2. 从 `OtherElement.u16Soc_Ah` 得到额定容量。
3. 从 `OtherElement.u16Soc_Cycle_times` 得到初始循环次数。
4. 计算 SOH 和 `cap_full_as10`。
5. 调用 `soc_load_or_default()`。

`soc_load_or_default()` 分支：

- Flash snapshot 有效：优先恢复 `u32CapNow`，否则按 `u16SocNow` 恢复。
- snapshot 无效且电压可校准：按 OCV 表用 `VCellMin` 初始化。
- snapshot 无效且电压不可校准：默认 60%。

评价：这条初始化路径合理。优先使用持久化容量，避免每次启动被瞬时电压重估。无 snapshot 时用 OCV，比固定 60% 更好。

## 持久化

SOC snapshot 结构在 `Flash.h`：

- `u16FormatVersion`
- `u16SocNow`
- `u16DsgSocInt`
- `u16MaxErrorPercent`
- `u32CycleTimes`
- `u32CapNow`
- `u32CapFull`
- `u32LearnPassedAs10`
- `u16Flags`

保存触发在 `soc_save_if_needed()`：

- SOC 变化。
- 循环次数变化。
- 满容量变化。
- snapshot flags 变化。

休眠前 `LowPowerSleep_SaveCoreState()` 调用 `SOC_SaveSnapshotBeforeSleep()`。

评价：存储字段覆盖了当前算法核心状态，A/B journal 方向也合理。风险是 SOC 每变化 1% 就可能保存，Flash 寿命取决于实际骑行/负载频率和 journal 页策略；后续应单独用当前 `StorageFlash_SaveJournalPair()` 的页轮转能力估算寿命。

## 容量积分

`soc_integrate()` 每 200ms 调用一次：

```text
integrate_current_ma = net_current_ma - PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA
delta_as10 = integrate_current_ma * 200ms / 100000 + rem
```

关键行为：

- RELAX 时，如果外部电流为 0，会按板载自耗 30mA 慢慢放电。
- 充电时也会扣除 30mA 自耗。
- 放电时会叠加外部放电和板载自耗。
- 充电积分到 100 前会被压到 99%，必须满电电压锚定后才能到 100%。
- 放电累计会推进 `cycle_x100`，SOH 每 100 次循环下降 1%，最低 80%。

评价：容量积分逻辑是当前模块最可靠的部分。`rem_mams` 能保留小电流余量，避免 30mA 自耗在 200ms tick 下被整数截断。

## 满电锚定

满电确认条件：

- 非放电模式。
- 电压合法。
- `VCellMax > 4180mV`。
- `VCellMax >= OtherElement.u16Soc_V_100 - margin`。
- `VCellMin >= OtherElement.u16Soc_V_100 - margin`。
- 单体压差不超过 `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV`。
- 连续满足 `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS`。

满足后每次最多上调 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT`，当前为 1%。

评价：逻辑偏保守，但用户体验可接受性取决于充电末端是否真的能满足 `VCellMax > 4180mV`。如果实际充电器或保护参数导致单体长期到不了这个条件，SOC 会卡在 99% 或更低，用户会认为充满不了。这个逻辑需要真机充满曲线验证。

## 低压尾段

当前工作区版本已经不是旧的 `s_empty_tail_table`。现在是：

- `soc_empty_tail_interpolate(offset_mv, is_relax)` 按 `VCellMin - V0` 插值得到目标 SOC。
- `soc_select_empty_tail_step()` 根据电压区间和放电电流选择下修速度。

当前关键分支：

- 充电模式不做 low-tail。
- 电压无效不做 low-tail。
- sag hold 且仍高于 `V0 + SOC_SAG_ALLOW_OFFSET_MV` 时阻塞。
- `VCellMin > V0 + PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV` 不做 low-tail。
- `VCellMin <= V0`：critical，约 5 秒/1%。
- `VCellMin <= V0 + 50mV`：force，约 10 秒/1%。
- RELAX 且高于 `V0 + 50mV`：直接不做 low-tail。
- DSG 且高于 `V0 + 50mV`：按电流强度动态 120-600 tick，下修速度约 24-120 秒/1%。

评价：当前 low-tail 改动明显增强了低压区“必须到 0”的能力，这是保护板 BMS 用户体验里很重要的一点。但也带来新风险：在 `VCellMin <= V0` 附近会非常快地下修，host test 已经显示 30% 在 2950mV、145A10 放电 60 秒后降到 18%，旧预期是 28-29。这个行为可能是有意优化，也可能过猛，必须用真机负载曲线确认。

## Sag Hold

大电流放电时，如果放电电流超过 `容量 / 2` 对应阈值，设置 `sag_hold_ticks = 30s * 5`，并写 snapshot flag。hold 期间如果电压还高于 `V0 + 50mV`，阻塞 OCV/low-tail 校准。

评价：这条逻辑必要。它避免爬坡/大负载时电压下陷导致 SOC 被误判为空。但当电压已经进入 `V0 + 50mV` 以下时，允许 low-tail 介入，这是为了“低电必须归零”的体验。边界合理，但要实测不同负载下是否过早进入 force/critical。

## 静置 OCV

当前配置：

- `PROJECT_CFG_SOC_REST_OCV_ENABLE = 1`
- `PROJECT_CFG_SOC_REST_OCV_SECONDS = 3600`
- `PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS = 1800`

当前工作区源码还额外加了条件：

```c
if (mode != SOC_MODE_RELAX || g_stCellInfoReport.u16VCellMin >= 3700)
{
    soc_reset_rest_confidence();
    return;
}
```

含义是：只有 RELAX 且 `VCellMin < 3700mV` 才会进入静置 OCV 慢下修。

静置 OCV 只下修，不上修。达到稳定静置条件后，如果 OCV 目标比当前 SOC 低超过 3%，设置下修目标，然后每 1800 秒最多下修 1%。

评价：方向上是保守的，避免静置高电压把 SOC 往上跳，比较安全。但 `VCellMin >= 3700` 直接重置静置置信度是一个产品策略假设：中高 SOC 区不做静置修正。这能避免中高段 OCV 平台误校准，但也意味着长期停放的中高 SOC 漂移不会被修正。该条件目前没有宏配置，也没有测试覆盖同步，建议保留前先明确需求。

## RTC STOP 补偿

HICCUP STOP 唤醒后通过：

```text
RtcSleep_PortApplySocRtcRest(rest_seconds)
  SOC_ApplyRtcRelaxationCompensation(rest_seconds, vcell_min, vcell_max)
```

当前 RTC 补偿只推进静置 OCV 计数和长静置下修，不额外按 30mA 自耗扣容量。

评价：这比较保守，避免低功耗期间把自耗重复算大。但如果板子真实 STOP 平均电流并不低，长期休眠后 SOC 会偏高，只能靠低压/静置 OCV 下修纠正。后续需要用实测低功耗电流决定是否需要单独的 RTC self-discharge 模型。

## 参数和上位机入口

`Sci_Upper.c` 中 `OtherElement` 的 SOC 参数区变化会：

- 调用 `InitData_SOC()`。
- 调用 `SOC_RequestCapacityReset()`。

一次性写 SOC 入口调用：

```c
SOC_RequestSetOnce((UINT8)u16SciRegData);
```

评价：外部写 SOC 参数后重载并重置容量基准是合理的。但当前 `SOC_RequestCapacityReset()` 会保留原 SOC 百分比，再按新容量重算容量，这对“改额定容量但不想改变显示 SOC”的上位机体验是合理的。

## 当前测试结果

本次只读运行了两个 SOC 专项测试：

```text
py tools\soc_replay_test.py
```

结果：失败。失败点：

```text
AssertionError: s_empty_tail_table
```

原因：当前源码已经删除旧 `s_empty_tail_table`，改为 `soc_empty_tail_interpolate()`，但 Python replay 仍按旧表解析。

```text
py tools\run_soc_host_c_test.py
```

结果：失败。失败点：

```text
FAIL line 342: host_internal_soc() actual=18 expected_range=[28,29]
```

原因：当前 low-tail 新策略在低压场景下明显比旧预期更快地下修。

编译时还出现两个 signedness warning：

```text
SocEnhance.c:869
SocEnhance.c:950
```

## 当前逻辑优点

1. 主线清晰：以容量积分为主，电压只做边界修正。
2. 200ms sample seq 防重复积分，调度边界合理。
3. `rem_mams` 保留积分余量，小电流和自耗不会被截断。
4. 满电必须电压锚定，避免单纯充电积分把 SOC 直接顶到 100%。
5. 低压尾段能强制收敛，避免“实际没电但 SOC 还有很多”。
6. sag hold 能减少大负载电压下陷误校准。
7. snapshot 字段覆盖当前核心状态，启动恢复路径合理。
8. 静置 OCV 只下修不上修，保守安全。

## 当前逻辑主要问题

### 1. 源码、文档、测试不一致

当前 `SOC.c` 不再包含 Type-C 等效电流折算，但历史设计文档、变量文档、`project_check.py` 仍认为 SOC 应该读取 `ADC_GetTypeCOutCurrentMilliAmp()` 并折算。

当前 `SocEnhance.c` 不再有 `s_empty_tail_table`，但 `tools/soc_replay_test.py` 仍按旧表解析。

评价：这是当前最大工程风险。算法是否正确先不谈，验证系统已经失配。

### 2. Low-tail 当前可能过猛

当前 low-tail 在 `VCellMin <= V0` 时 5 秒/1%，在 `V0 + 50mV` 以下 10 秒/1%。如果真实负载导致单体短时下陷到 3000mV 附近，SOC 会快速掉，用户可能看到跳变。

不过这也可能正是想解决“到 0 不及时”的改动。需要用真实放电曲线决定，不应只按主观判断。

### 3. 静置 OCV 只在 3700mV 以下工作

`VCellMin >= 3700` 直接清置信度，这个策略能避免中高 SOC 区误校准，但也让中高段长期漂移无法通过静置 OCV 修正。这个条件应该被明确为产品策略，而不是隐藏在算法内部。

### 4. Type-C 输出是否计入 SOC 需要重新确认

历史文档认为 Type-C 输出应折算为电池侧等效放电。当前源码没有计入。若产品有 Type-C 输出负载，此时 SOC 会偏高。

如果当前硬件没有 Type-C 输出或该功能已废弃，应同步删除文档和检查脚本里的 Type-C SOC 约束。

### 5. 满电到 100 可能偏保守

满电要求 `VCellMax > 4180` 且 min/max 条件满足。若实际充电截止、均衡或保护导致某些工况达不到，SOC 会长期不到 100%。用户体验上“充满但显示 99/98”会很敏感。

### 6. 对低功耗自耗的处理还需要实测闭环

运行态计入 30mA 自耗，RTC STOP 不额外计入自耗。这个选择是否正确取决于 STOP 平均功耗。如果 STOP 电流并非极低，长期停放后 SOC 偏高风险仍在。

## 总体评价

当前 SOC 架构比“纯电压表 SOC”更靠谱，主线选择是正确的：库仑积分负责连续性，电压负责端点和慢修正。对于保护板 BMS，这个方向适合继续保留。

但当前工作区状态还不是稳定收口状态。核心问题不是算法复杂，而是最近 low-tail 和 rest OCV 策略变化后，测试模型、历史文档、project_check 还没有同步。现在不能说“当前 SOC 逻辑已经没问题”，只能说“主架构合理，但低压尾段和静置修正策略仍处在待验证状态”。

如果按产品体验评价：

- “必须能到 0”：当前 low-tail 更积极，有利。
- “不能没电还显示很多”：当前更有利。
- “不能低压负载一压就跳很多”：当前有风险。
- “充满必须到 100”：当前偏保守，需要实测。
- “长期停放要准”：当前只做低段慢下修，中高段不修正，有漂移风险。
- “Type-C 输出耗电要算”：当前源码未计入，需要确认需求。

## 建议收口顺序

1. 先确定 Type-C 输出是否仍属于 SOC 输入。如果是，恢复或重写等效电流折算；如果不是，清理文档和检查脚本。
2. 同步 `tools/soc_replay_test.py` 和 `tools/soc_host_c_test.c` 到当前 low-tail 插值策略，或者回退到表驱动策略。
3. 用真实电池负载数据验证 low-tail 三个速度：
   - `V0 + 50mV` 以上放电动态下修。
   - `V0 + 50mV` 以下 force 下修。
   - `V0` 以下 critical 下修。
4. 明确 `VCellMin >= 3700` 不做静置 OCV 的产品策略。如果只是调试临时条件，应改为可配置或删除。
5. 用充满测试验证 `VCellMax > 4180` 和压差条件是否能让 SOC 稳定到 100。
6. 用 STOP 实测电流决定 RTC STOP 是否需要按真实低功耗电流补偿，而不是固定不扣。
7. 测试全部恢复后，再更新 `docs/design/soc_design.md` 或建立新的权威 SOC 设计文档。

## 当前建议判断

不建议现在继续叠加新 SOC 功能。应先把当前源码和测试对齐，再决定 low-tail 目标和速度是否合理。

如果目标是短期出货稳定，建议优先保守：

- 保留容量积分主线。
- 保留满电锚定。
- 保留 sag hold。
- low-tail 速度不要只靠主观调快，必须用真实低电放电曲线确认。
- 静置 OCV 只下修可以保留，但 3700mV 门槛要明确。

如果目标是后续重构，建议不要推翻整个 SOC 模块，而是把 `SocEnhance.c` 拆成三层概念即可：

1. 输入层：电流、电压、温度、RTC 秒数。
2. 估算层：容量积分、SOH、snapshot。
3. 修正层：full、tail、rest、sag hold。

当前代码已经接近这个结构，只是还混在一个文件里。真正需要先解决的是策略验证和测试同步，而不是大规模重写。

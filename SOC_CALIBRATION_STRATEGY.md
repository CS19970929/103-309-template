# SOC 校准策略、在线 OCV 融合与参数调优说明

本文说明当前 SOC 模块的校准策略、异常输入屏蔽逻辑、在线 OCV 融合方式，以及影响 SOC 收敛、准确性和用户体验的可配置参数。代码依据为 `SocEnhance.c`、`Project_Config.h`、`DataDeal.h`、`SOC.c` 和通信入口 `Sci_Upper.c`。

## 1. 总体结论

当前 SOC 不是单一 OCV 查表，也不是只靠安时积分，而是“安时积分为主、端点可信锚点校准、静置/在线 OCV 小步融合、显示平滑发布”的组合策略。

设计目标：

1. 正常充放电时 SOC 连续、稳定，不因端电压瞬态跳变。
2. 满电和空电只在可信条件下建立锚点，避免虚高或虚低。
3. 轻载运行中允许多点 OCV 慢速收敛，缩短只靠端点校准的收敛时间。
4. 异常电压、保护故障、AFE/ADC/CBC/温度异常状态不参与 SOC 校准，避免错误采样把 SOC 拉偏。
5. 对外显示 SOC 与内部 SOC 分离，用户看到的是平滑后的结果。

## 2. 当前所有 SOC 校准策略

| 策略 | 触发场景 | 校准方向 | 作用 |
| --- | --- | --- | --- |
| 安时积分 | 有新的 AFE 电流样本，充/放电电流 `>=0.4A` | 充电上升，放电下降 | SOC 主估算来源 |
| 充电端点渐进修正 | 充电且接近 `V100` | 最高到 99% | 避免快满电时长期停在低 SOC |
| 满电可信锚点 | `CHG`、`VCellMax >= V100`、`VCellMin` 接近 `V100`、压差合格、taper 电流持续满足 | 置 100% | 建立满电锚点，触发 FCC 学习 |
| 放电端点渐进修正 | `VCellMin` 接近或低于 `V0` | 分阶段下修 | 避免低压区 SOC 高估 |
| 弱单体保护 | 非充电、最低单体接近 `V0` | 加速下修，必要时到 0% | 安全保护，不属于 OCV 校准 |
| 空电可信锚点 | 放电容量归零，或弱单体把 SOC 压到 0 且 `VCellMin <= V0` | 记录 0% 锚点 | 建立空电锚点，触发 FCC 学习 |
| 手动 OCV 刷新 | 上位机发 OCV 刷新命令 | 非充电只允许下修，充电允许按 OCV | 调试/维护入口 |
| 运行态静置 OCV | `RELAX`、无电流、电压稳定、静置时间分桶 | 小步修正 | 长时间静置后慢速收敛 |
| RTC 休眠补偿 | RTC 唤醒后传入休眠时长和唤醒电压 | 小步修正并强制同步显示 | 休眠后恢复 SOC 可信度 |
| 在线 OCV 有界融合 | 轻载充/放电，OCV 目标与内部 SOC 持续有偏差 | 充电只上修，放电只下修 | 运行过程中多点慢速收敛 |
| FCC/SOH 学习 | 满/空可信锚点之间累计通过电量 | 更新 `CapFull` | 长期修正容量基准 |

## 3. 异常电压与异常状态不校准

新增统一校准门控 `SOC_IsCalibrationAllowed()`。以下场景不会执行手动 OCV 刷新、满电确认、在线 OCV 融合、运行态静置 OCV 和 RTC 静置补偿：

| 异常类型 | 当前默认条件 | 处理 |
| --- | --- | --- |
| 单体电压过低或未采到 | `VCellMin < 2000mV` 或 `VCellMax < 2000mV` | 不校准 |
| 单体电压物理异常偏高 | `VCellMin > 5000mV` 或 `VCellMax > 5000mV` | 不校准 |
| 最大/最小电压顺序异常 | `VCellMax < VCellMin` | 不校准 |
| 单体压差异常 | `VCellMax - VCellMin > 1000mV` | 不做 OCV/端点校准 |
| 三级保护故障 | `unMdlFault_Third.all != 0` | 不校准 |
| 系统采样/执行异常 | AFE1、AFE2、ADC、CBC_CHG、CBC_DSG、TEMP_BREAK 任一状态异常 | 不校准 |

说明：

1. “不校准”指不使用电压去改变 SOC 估计锚点或 OCV 目标。
2. 弱单体保护仍保留，因为它是安全下修逻辑，不是 OCV 校准；但它也要求基础电压合法，避免 `0mV`、反序、超上限等采样异常把 SOC 误压到 0。
3. 安时积分仍由电流样本驱动。异常电压只会阻止电压类校准，不直接停止容量积分。

## 4. 在线 OCV 融合策略

在线 OCV 融合不是“实时用端电压覆盖 SOC”，而是有条件、有方向、有速率限制的慢速融合。

执行链路：

1. 每 200ms SOC 调度一次，但只有 AFE 电流样本序号变化时才运行完整算法。
2. 判断当前方向：`Ichg >=0.4A` 且不小于 `Idsg` 为充电；否则 `Idsg >=0.4A` 为放电；否则进入静置候选。
3. 先执行安时积分和端点逻辑。
4. 再执行在线 OCV guard。
5. 对外 SOC 仍走显示平滑发布。

在线 OCV guard 条件：

| 条件 | 默认值 |
| --- | --- |
| 开关 | `PROJECT_CFG_SOC_ONLINE_OCV_GUARD_ENABLE = 1` |
| 电流窗口 | `0.4A <= I <= C/10`，27Ah 默认约 `0.4A~2.7A` |
| 重载恢复抑制 | 放电电流达到 `C/3` 后，默认 `180s` 内禁止在线 OCV 融合 |
| 电压门控 | 单体在 `2000~5000mV`，`VCellMax >= VCellMin`，压差 `<=1000mV` |
| 在线稳定门控 | `VCellMin/VCellMax` 在 `20s` 内变化不超过 `8mV` 后才开始累计修正时间 |
| 状态门控 | 无三级保护故障，无 AFE/ADC/CBC/温度异常 |
| OCV 目标范围 | 只接受 `5%~95%` |
| 最小有效偏差 | OCV 目标与内部 SOC 至少相差 `3%` |
| 持续时间 | 电压稳定后，偏差再持续 `30s` 才修正 |
| 单次步长 | 每次只修正 `1%` |
| 方向限制 | 充电只允许上修；放电只允许下修 |
| LFP 平台区 | `20%~90%` 默认不做在线 OCV 修正 |

收敛示例：

1. 当前内部 SOC 为 80%，轻载放电，OCV 查表目标约 70%。
2. 如果电流、电压、状态均可信，并且不在重载恢复抑制窗口内，先等待电压稳定。
3. 电压稳定后，偏差再持续 30s，则内部 SOC 从 80% 调到 79%。
4. 后续仍满足条件时，每 30s 再下修 1%。
5. 对外显示 SOC 继续按平滑规则跟随，不会一次跳到 70%。

这样做的意义：

1. 可在充放电过程中做多点 OCV 收敛。
2. 不把负载端电压瞬态当成真实开路电压。
3. 大电流骑行后电压未完全回弹时，不会立刻用偏低端电压下修 SOC。
4. 不在 LFP 平台区用很小电压差强行判断大 SOC 差。
5. 用户看到的是缓慢、可解释的 SOC 变化，而不是突然跳变。

## 5. 影响 SOC 的可配置参数

### 5.1 编译期参数

位置：`103 + 309/Project/Source/conf/Project_Config.h`

| 参数 | 默认值 | 影响 | 调整方向 |
| --- | ---: | --- | --- |
| `PROJECT_CFG_BAT_TYPE` | `1` | 选择默认容量，当前 `BAT_SLAVE` 对应 27Ah | 换硬件型号时调整 |
| `PROJECT_CFG_BAT_CHEMISTRY` | `0` | 选择三元锂/LFP 默认配置和 OCV 表 | 换电芯体系时必须调整 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | `80` | 满电确认时 `VCellMin` 距离 `V100` 的允许裕量 | 变小更严格，变大更容易确认满电 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | `120` | 满电确认允许的最大单体压差 | 变小更保守，变大更宽松，`0` 不建议 |
| `PROJECT_CFG_SOC_ONLINE_OCV_GUARD_ENABLE` | `1` | 是否启用在线 OCV 有界融合 | 现场不稳定时可临时关闭 |
| `PROJECT_CFG_SOC_ONLINE_OCV_CORRECTION_SECONDS` | `30` | 在线 OCV 每 1% 修正所需持续时间 | 变小收敛快但更易可见，变大更平滑 |
| `PROJECT_CFG_SOC_ONLINE_OCV_MIN_DELTA_PERCENT` | `3` | 触发在线 OCV 的最小 SOC 偏差 | 变小更敏感，变大更稳定 |
| `PROJECT_CFG_SOC_ONLINE_OCV_CURRENT_DIVIDER` | `10` | 在线 OCV 最大电流为 `C/divider` | divider 变大更严格，变小更宽松 |
| `PROJECT_CFG_SOC_ONLINE_OCV_HEAVY_DSG_CURRENT_DIVIDER` | `3` | 重载放电判定阈值为 `C/divider` | divider 变小更严格，变大更容易进入 holdoff |
| `PROJECT_CFG_SOC_ONLINE_OCV_HEAVY_DSG_HOLDOFF_SECONDS` | `180` | 重载放电结束后禁止在线 OCV 的时间 | 变大更稳，变小恢复更快 |
| `PROJECT_CFG_SOC_ONLINE_OCV_STABLE_SECONDS` | `20` | 在线 OCV 要求电压稳定的时间 | 变大更稳，变小收敛更快 |
| `PROJECT_CFG_SOC_ONLINE_OCV_STABLE_WINDOW_MV` | `8` | 在线 OCV 电压稳定窗口 | 变小更严格，变大更宽松 |
| `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | `2000` | 校准可接受的最低单体电压 | 一般不建议改低 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | `5000` | 校准可接受的最高单体电压 | 一般不建议改高 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV` | `1000` | OCV/端点校准允许的最大压差 | 变小更保守，异常更不易误校准 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT` | `1` | 三级保护故障时禁止校准 | 建议保持 1 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT` | `1` | AFE/ADC/CBC/温度异常时禁止校准 | 建议保持 1 |

上述 SOC 校准参数均由 `Project_BuildGuard.h` 做编译期范围检查。

### 5.2 上位机/Flash 参数

位置：`OtherElement`，通过 RS485/CAN 上位机参数区写入后保存到内部 Flash。

| 参数 | 典型入口 | 影响 |
| --- | --- | --- |
| `OtherElement.u16Soc_Ah` | `0x2318` | 出厂容量，影响积分 1% 所需电量、C/20、C/10、SOH |
| `OtherElement.u16Soc_Cycle_times` | `0x2319` | 初始循环次数 |
| `OtherElement.u16Soc_V_100` | `0x231A` | 满电端点，影响满电确认和端点渐进 |
| `OtherElement.u16Soc_V_0` | `0x231B` | 空电端点，影响低压下修、弱单体保护和空电锚点 |
| `OtherElement.u16Soc_TableSelect` | `OtherElement` 参数区 | 选择 OCV 表 |
| `SOC_Table_Set[42]` | `0x2200` 起 42 个寄存器 | 自定义 OCV 表，当前只在 RAM 生效，重启不持久 |

### 5.3 当前固定策略常量

这些参数目前在代码中固定，不是 `Project_Config.h` 开关：

| 常量 | 当前值 | 影响 |
| --- | ---: | --- |
| `SOC_CURRENT_ENTER_A10` | `4`，即 0.4A | 小电流死区，低于此值不积分 |
| `SOC_FULL_CONFIRM_SECONDS` | `60s` | 满电确认持续时间 |
| `SOC_DISPLAY_STEP_SECONDS` | `1s` | 对外 SOC 正常每秒跟随 1% |
| `SOC_RELAX_STABLE_SECONDS` | `30s` | 运行态静置电压稳定窗口 |
| `SOC_RELAX_VOLT_STABLE_WINDOW_MV` | `3mV` | 静置电压稳定判定窗口 |
| `SOC_WEAK_CELL_GUARD_WINDOW_MV` | `120mV` | 弱单体保护介入窗口 |
| `SOC_WEAK_CELL_CRITICAL_WINDOW_MV` | `30mV` | 弱单体临界加速下修窗口 |
| `SOC_LEARN_FIRST_SPAN_PERCENT` | `90%` | 首次 FCC 学习所需跨度 |
| `SOC_LEARN_NEXT_SPAN_PERCENT` | `40%` | 后续 FCC 学习所需跨度 |
| `SOC_LEARN_CAP_MAX_STEP_PERCENT` | `5%` | 单次 FCC 最大变化幅度 |

如需现场可调，应优先把这些常量中和用户体验强相关的项迁移到 `Project_Config.h`，不要直接散改算法。

## 6. 参数调优建议

### 6.1 满电体验

目标：充满后能稳定显示 100%，但不能虚高。

建议：

1. 默认 `V100 - 80mV` 和压差 `120mV` 适合先保守验证，仍是防误触发的主要门槛。
2. 满电不再要求 charger taper 电流一定低于 `C/20`；只要端点电压可信，并且 SOC/容量已接近满或出现真实小电流尾段，持续 60s 后即可到 100%。
3. 如果真实充满后长期不能到 100%，优先确认 `V100`、最低单体窗口和压差是否过严，再考虑把 margin 从 `80mV` 增到 `100~120mV`。
4. 如果早早显示 100%，先把 margin 降到 `50~60mV`，或把压差从 `120mV` 降到 `80~100mV`。
5. 不建议把压差检查关掉，除非有独立均衡/采样证明。

### 6.2 运行中 SOC 收敛

目标：避免长时间只靠积分造成累计误差，但用户不感到跳变。

建议：

1. 默认在线 OCV 为“电压稳定 `20s` 后，再按 `30s/1%` 修正”，适合优先用户体验。
2. 如果希望更快收敛，可把 `PROJECT_CFG_SOC_ONLINE_OCV_CORRECTION_SECONDS` 调到 `20s`，但要先台架确认负载极化不会误导。
3. 如果现场电压噪声较大，把 `PROJECT_CFG_SOC_ONLINE_OCV_MIN_DELTA_PERCENT` 从 `3%` 调到 `4~5%`。
4. 如果轻载定义过宽导致误修正，把 `PROJECT_CFG_SOC_ONLINE_OCV_CURRENT_DIVIDER` 从 `10` 调到 `15~20`，等价于从 `C/10` 收紧到 `C/15~C/20`。
5. 如果 e-bike 大电流松油后 SOC 仍偏低，把 `PROJECT_CFG_SOC_ONLINE_OCV_HEAVY_DSG_HOLDOFF_SECONDS` 从 `180s` 提高到 `240~300s`。
6. 如果电压回弹很慢或采样抖动明显，把 `PROJECT_CFG_SOC_ONLINE_OCV_STABLE_SECONDS` 提高到 `30s`，或把稳定窗口从 `8mV` 收紧到 `5mV`。
7. LFP 平台区不要开放在线 OCV 中段修正，除非有温度补偿和高分辨率 OCV 数据。

### 6.3 低压安全与显示一致性

目标：低压时不要高估 SOC，但也不要因采样异常误清零。

建议：

1. `V0` 必须来自电芯规格和系统保护策略，不要为了显示好看随意抬高或压低。
2. `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` 默认 `2000mV` 不建议降低；低于此值更像采样异常或极端故障，不适合做校准。
3. 弱单体保护不是 OCV 校准，它会在低压风险区保守下修，目的是安全。
4. 若客户抱怨低压区掉得快，应先检查电芯压差、内阻、负载电流和 `V0`，不要直接放宽弱单体保护。

### 6.4 容量与 SOH

目标：长期容量基准逐步贴近真实电池包。

建议：

1. `u16Soc_Ah` 必须按实际 pack 标称容量配置，错误容量会让积分全程漂移。
2. FCC 学习依赖满/空可信锚点。没有完整充放电跨度时，不应频繁更新 SOH。
3. 如果客户关注 SOH 精度，应优先补台架容量数据和温度分档，不要先上复杂模型。

## 7. 推荐验证步骤

1. 运行主机回放：`python3 tools/soc_replay_test.py`。
2. Keil 编译 `FD_Debug` 和 `FD_Release`。
3. 台架模拟异常电压：`VCellMax < VCellMin`、单体超 5000mV、单体低于 2000mV、压差超 1000mV，确认 OCV/满电/静置/在线校准均不动作。
4. 注入三级保护故障、AFE/ADC/CBC/温度异常，确认不做电压类校准。
5. 轻载充放电验证在线 OCV：只在 `0.4A~C/10` 内，电压稳定后按 `1%/30s` 单向收敛。
6. 重载后恢复验证：`Idsg >= C/3` 后，默认 `180s` 内不做在线 OCV；电压持续回弹未稳定时也不做在线 OCV。
7. 重载和 LFP 平台区验证：不做在线 OCV 修正。
8. 满电验证：taper 电流、最低单体窗口、压差和 60s 持续时间全部满足后才到 100%。
9. 低压验证：异常采样不误清零，真实弱单体低压能保守下修。

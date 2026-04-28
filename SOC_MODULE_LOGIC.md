# SOC 模块完整逻辑说明与首次烧录默认值

本文按当前代码实现核对，覆盖 `SOC.c`、`SocEnhance.c/.h`、`Flash.c/.h`、`EEPROM.c`、`Sci_Upper.c`、`rtc_sleep.c` 中和 SOC 直接相关的逻辑。当前配置来自 `103 + 309/Project/Source/conf/Project_Config.h`：

| 配置项 | 当前值 | 含义 |
| --- | ---: | --- |
| `PROJECT_CFG_BAT_TYPE` | `1` | `BAT_SLAVE`，当前 40A 档 |
| `PROJECT_CFG_BAT_CHEMISTRY` | `0` | `TERNARYLI` |
| `BMS_CAPCITY` | `270` | `10 * Ah`，即 27.0Ah |
| `SNum` | `10` | 默认 10 串 |
| `PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE` | `0` | 升级参数策略当前默认关闭 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | `80mV` | 满电确认时最低单体必须接近 `V100` 的裕量 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | `120mV` | 满电确认允许的最大单体压差，`0` 表示不检查 |
| `PROJECT_CFG_SOC_ONLINE_OCV_GUARD_ENABLE` | `1` | 启用轻载在线 OCV 有界融合 |
| `PROJECT_CFG_SOC_ONLINE_OCV_CORRECTION_SECONDS` | `30s` | 在线 OCV 偏差持续多久后允许修正 1% |
| `PROJECT_CFG_SOC_ONLINE_OCV_MIN_DELTA_PERCENT` | `3%` | 在线 OCV 目标与内部 SOC 的最小有效偏差 |
| `PROJECT_CFG_SOC_ONLINE_OCV_CURRENT_DIVIDER` | `10` | 在线 OCV 最大电流为 `C / divider`，默认 `C/10` |
| `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | `2000mV` | 电压类校准允许的最低单体电压 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | `5000mV` | 电压类校准允许的最高单体电压 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV` | `1000mV` | 电压类校准允许的最大单体压差 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT` | `1` | 三级保护故障时禁止 SOC 电压类校准 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT` | `1` | AFE/ADC/CBC/温度异常时禁止 SOC 电压类校准 |

如果切换到 `BAT_MASTER`、`LIFEPO` 或修改容量/端点参数，默认容量、OCV 表、满电确认阈值会随编译期配置变化。
这些 SOC 校准参数均由 `Project_BuildGuard.h` 做编译期范围检查，避免误配置导致 SOC 收敛速度或端点可信度失控。

## 1. 模块结论

当前 SOC 模块已经不是简单电压查表，而是“安时积分为主、端点/静置 OCV 校准为辅、内部 SOC 与对外显示分离、Flash 快照恢复现场”的中等增强方案。

当前实现的核心特征：

1. `SOC.c` 只做配置加载和 200ms 调度，不承载算法细节。
2. `SocEnhance.c` 维护内部运行上下文，负责 `CHG / DSG / RELAX` 状态、安时积分、端点校准、静置 OCV、弱单体保护、SOC 平滑发布和快照触发。
3. `Flash.c` 用 SOC journal 保存 V2 快照，并兼容旧 V1 快照。
4. SOC 基础配置保存在内部 Flash RW 参数区；SOC 运行快照保存在内部 Flash SOC journal；自定义 SOC 表当前只在 RAM 生效，不跨重启保存。
5. 充电积分到容量上限时不会直接显示 100%，内部 SOC 最多先到 99%，只有通过满电确认条件后才进入 100% 可信满电锚点。
6. 轻载充/放电过程中允许使用 OCV 表做多点有界融合，但只按方向小步收敛，不直接用端电压覆盖 SOC。

## 2. 文件边界

| 文件 | 责任 |
| --- | --- |
| `SOC.c` | 加载 `OtherElement` 与 `SOC_Table_Set`，提供 `InitData_SOC()` / `App_SOC()`，按 AFE 样本序号驱动 SOC 算法 |
| `SOC.h` | SOC 表长度、边界、模块入口声明 |
| `SocEnhance.h` | SOC 对外结构体、表选择枚举、对外 API |
| `SocEnhance.c` | SOC 算法主实现：状态机、积分、端点、静置/在线 OCV、学习、显示、快照 |
| `Flash.h` | SOC V2 快照结构体、SOC journal 地址与读写 API |
| `Flash.c` | V1/V2 SOC 快照加载、保存、journal 双槽、CRC/sequence 校验 |
| `EEPROM.c` | 当前实际已替换为内部 Flash 参数初始化/保存；负责默认 SOC 表、RW 参数加载、升级策略 |
| `Sci_Upper.c` | RS485 上位机写 SOC 表、SOC 基础参数、设置一次 SOC、功能开关 |
| `rtc_sleep.c` | RTC 休眠唤醒后调用 `SOC_ApplyRtcRelaxationCompensation()` 做静置补偿 |
| `LedBar.c` / `Can_HDX.c` / `Sci_Upper.c` | 使用 `g_stCellInfoReport.SocElement` 中的对外 SOC/SOH/容量/循环次数 |

## 3. 启动流程

当前主程序初始化顺序中，和 SOC 相关的路径是：

1. `StorageFlash_PrintBootCheck()` 打印后 64K 参数区检查结果。
2. `InitE2PROM()` 加载默认运行参数，再从内部 Flash RW 参数区加载现场参数。
3. `InitE2PROM()` 继续加载 AFE 参数、事件日志，并按升级策略执行一次性重置。
4. `InitData_SOC()` 复制 SOC 基础配置，初始化 SOC 算法。
5. 主循环 200ms 周期调用 `App_AFEGet()` 和 `App_SOC()`。

`InitData_SOC()` 的具体动作：

1. `SOC_LoadConfigData()` 从 `OtherElement` 复制 SOC 基础配置到 `SOC_Enhance_Element`。
2. 同时把 `SOC_Table_Set[42]` 复制到 `SOC_Enhance_Element.SOC_Table_CanSet`。
3. `soc_param_lib_init()` 清空内部计算状态，加载出厂容量/循环次数。
4. 读取 SOC journal；读取失败或数据非法时创建默认 V2 快照并尝试保存。
5. 设置 `u16_SOC_InitOver = 1`，重置运行上下文，强制同步一次对外数据。

## 4. 首次烧录默认值

首次烧录、后 64K 无有效 RW 参数且无有效 SOC journal 时，当前默认值如下：

| 变量/字段 | 当前值 | 说明 |
| --- | ---: | --- |
| `OtherElement.u16Soc_TableSelect` | `SOC_TABLE_TERNARYLI = 2` | 默认三元锂内置 OCV 表 |
| `OtherElement.u16Soc_Ah` | `270` | 单位 `10 * Ah`，即 27.0Ah |
| `OtherElement.u16Soc_Cycle_times` | `3` | 初始循环次数 |
| `OtherElement.u16Soc_V_100` | `4180mV` | 满电端点电压 `V100` |
| `OtherElement.u16Soc_V_0` | `3000mV` | 空电端点电压 `V0` |
| `SOC_DEFAULT_STARTUP_PERCENT` | `60%` | 无有效快照时的启动 SOC |
| `SOC_Calculate_Element.u32CapFactory` | `972000` | `270 * 3600`，内部单位 `As * 10` |
| `SOC_Calculate_Element.u32CapFull` | `972000` | 首次默认 FCC 等于出厂容量 |
| `SOC_Calculate_Element.u32CapNow` | `583200` | 60% 剩余容量 |
| `SOC_Calculate_Element.u32Cycle_times` | `300` | 内部单位 `cycle * 100`，对外显示 3 |
| `SOC_Calculate_Element.u8DSG_SOC_Int` | `0` | 放电循环百分比累计 |
| `SOC_Calculate_Element.u16MaxErrorPercent` | `100` | 快照可信度粗略字段，默认未收敛 |
| `SOC_Enhance_Element.u8_SOC` | `60` | 首次对外 SOC |
| `SOC_Enhance_Element.u8_SOH` | `100` | 首次对外 SOH |
| `SOC_Enhance_Element.u16_CapacityNow` | `1620` | `Ah * 100`，即 16.20Ah |
| `SOC_Enhance_Element.u16_CapacityFull` | `2700` | `Ah * 100`，即 27.00Ah |
| `SOC_Enhance_Element.u16_CapacityFactory` | `2700` | `Ah * 100`，即 27.00Ah |
| `SOC_Enhance_Element.u16_Cycle_times` | `3` | 对外循环次数 |

`SOC_Enhance_Element.u16_SOC_CycleT_Limit` 当前固定赋值 `5000`，但代码中没有参与 SOH、保护或告警计算，属于预留字段。

## 5. OCV 表

当前有 4 类表选择：

| 枚举 | 值 | 查表来源 | 是否持久化 |
| --- | ---: | --- | --- |
| `SOC_TABLE_TEST` | `0` | `SOC_Enhance_Element.SOC_Table_CanSet`，来自 `SOC_Table_Set` | 仅 RAM |
| `SOC_TABLE_LIFEPO` | `1` | `SOC_Table_LiFePO` | 固件常量 |
| `SOC_TABLE_TERNARYLI` | `2` | `SocTable_TernaryLi` | 固件常量 |
| `SOC_TABLE_LIFEPO2` | `3` | `SocTable_LiFePO2` | 固件常量 |

当前三元锂默认表为：

```text
4126/100, 4066/95, 4011/90, 3955/85, 3888/80,
3837/75, 3793/70, 3756/65, 3724/60, 3699/55,
3675/50, 3658/45, 3632/40, 3605/35, 3584/30,
3557/25, 3535/20, 3497/15, 3475/10, 3371/5,
3136/3
```

查表函数是 `GetEndValue()`，按相邻 `电压/SOC` 点线性插值。电压超出表范围时返回边界 SOC。`Get_OpenCircuit_Value()` 会把结果钳位到 `0~100`。

注意：

1. `SOC_Table_Default` 当前是 `SOC_TABLE_TEST` 自定义表的 RAM 初始值，不是当前产品默认运行表。
2. 上位机写 `0x2200` 起 42 个寄存器只更新 `SOC_Table_Set`，然后调用 `InitData_SOC()` 让 RAM 表立即生效。
3. 当前没有把 `SOC_Table_Set` 保存到内部 Flash。重启后 `EEPROM_LoadDefaultSocTable()` 会重新加载 `SOC_Table_Default`。
4. 只有 `OtherElement.u16Soc_TableSelect = SOC_TABLE_TEST` 时，上位机写入的自定义表才参与 OCV 查表。

## 6. 运行数据流

`App_SOC()` 挂在系统 200ms 时基上，但不会盲目重复积分旧电流：

1. `App_AFEGet()` 成功刷新电流采样后递增 `g_u32AfeCurrentSampleSeq`。
2. `App_SOC()` 发现样本序号变化后，调用 `SOC_UpdateSampleData()` 更新 `VCellMax/VCellMin/Ichg/Idsg`。
3. 有新样本时执行 `SOC_IntEnhance_Ctrl()`。
4. 没有新 AFE 样本时只调用 `SOC_PublishReportData()`，避免通信忙或 AFE 未刷新时重复积分。

`SOC_IntEnhance_Ctrl()` 当前执行顺序：

1. 处理上位机刷新命令：OCV 刷新、容量初始化、设置一次 SOC。
2. 判断当前电流方向并更新 `CHG / DSG / RELAX` 模式。
3. 根据方向执行安时积分和端点渐进修正。
4. 执行满电确认。
5. 执行轻载在线 OCV 有界融合。
6. 执行运行态静置 OCV 修正。
7. 执行弱单体低压保护修正。
8. 判断是否需要保存 SOC journal。
9. 更新对外 SOC/SOH/容量/循环次数并发布到 `g_stCellInfoReport`。

## 7. 电流模式与积分

当前积分周期为 200ms：

| 常量 | 当前值 | 说明 |
| --- | ---: | --- |
| `SOC_INTEGRATE_PERIOD_MS` | `200ms` | SOC 算法采样周期 |
| `SOC_TICKS_PER_SECOND` | `5` | 每秒 5 个 SOC tick |
| `SOC_CURRENT_ENTER_A10` | `4` | 进入充/放电的电流阈值，单位 `A * 10`，即 0.4A |
| `SOC_MODE_RELAX_ENTRY_SECONDS` | `5s` | 连续无有效电流后进入 `RELAX` |

方向判断：

| 条件 | 模式 | 行为 |
| --- | --- | --- |
| `Ichg >= 0.4A` 且 `Ichg >= Idsg` | `CHG` | 按充电积分 |
| `Idsg >= 0.4A` | `DSG` | 按放电积分 |
| 充/放电都低于阈值 | 保持/进入 `RELAX` | 不积分，累计 5s 后进入静置模式 |

方向切换时会清空：

1. 未满 1% 的容量累计 `u32CapChange`。
2. 200ms 不能整除留下的积分余量 `u32IntegrateRemainderMs`。
3. 充放电端点计数和满电确认计数。

单次容量增量：

```text
delta_as10 = (current_A10 * 200ms + remainder_ms) / 1000
```

其中 `current_A10` 单位为 `A * 10`，所以 `delta_as10` 的单位是 `As * 10`。

积分后的边界处理：

| 场景 | 内部处理 |
| --- | --- |
| 充电后 `u32CapNow >= cap_base` | `u32CapNow = cap_base`，内部 SOC 最高先到 `99%`，等待满电确认 |
| 放电后 `u32CapNow <= 0` | `u32CapNow = 0`，内部 SOC 到 `0%`，触发可信空电锚点 |
| 未到边界但累计容量达到 1% | 按累计百分比增减内部 SOC，并扣除已使用的 `u32CapChange` |

`cap_base` 优先使用学习后的 `u32CapFull`，无有效值时回退 `u32CapFactory`。

## 8. 端点校准

### 8.1 充电末端

充电末端有两层逻辑：

1. 端点渐进上修：靠近 `V100` 时每隔数秒上调 1%，但最多只上修到 99%。
2. 满电确认：只有满足更严格条件并持续约 60s，才把内部 SOC 设为 100%，并触发可信满电锚点。

端点渐进上修条件：

| 条件 | 周期 | 行为 |
| --- | ---: | --- |
| `VCellMax >= V100 + 50mV` 且 SOC `< 100` | 2s | SOC 每次 +1%，最多到 99% |
| `VCellMax >= V100` 且 SOC `< 100` | SOC `>95` 时 8s，否则 4s | SOC 每次 +1%，最多到 99% |
| `VCellMax >= V100 - 100mV` 且 `< V100` 且 SOC `<95` | 10s | SOC 每次 +1% |

满电确认条件：

| 条件 | 当前逻辑 |
| --- | --- |
| 模式 | 必须为 `CHG` |
| 最高单体 | `VCellMax >= V100` |
| 最低单体 | `VCellMin >= max(化学体系最低门槛, V100 - 80mV)` |
| 单体压差 | `abs(VCellMax - VCellMin) <= 120mV`；配置为 `0` 时不检查 |
| 电流 | `Ichg != 0` 且 `Ichg <= C/20`，并且不低于 0.4A |
| 持续时间 | `SOC_FULL_CONFIRM_SECONDS = 60s` |

当前 `TERNARYLI` 的化学体系最低门槛为 `4000mV`，`LIFEPO/LIFEPO2` 为 `3300mV`。在 27Ah 三元锂默认 `V100 = 4180mV` 下，实际最低单体确认阈值为 `4100mV`；默认 taper 电流为 `270 / 20 = 13`，单位 `A * 10`，约 1.3A。

满电确认成功后：

1. `SOC_ApplySocNow(100)` 把内部 SOC 和 `CapNow` 同步到满电。
2. `SOC_OnTrustedFullAnchor()` 记录可信满电锚点。
3. 如从可信空电锚点到满电且跨度满足条件，则尝试 FCC 学习。

### 8.2 放电末端

放电末端按最低单体电压逐步下修：

| 条件 | 周期 | 行为 |
| --- | ---: | --- |
| `VCellMin <= V0 + 100mV` 且 `> V0`，SOC `>5` | 10s | SOC 每次 -1% |
| `VCellMin <= V0`，SOC `>=5` | 4s | SOC 每次 -1% |
| `VCellMin <= V0`，SOC `<5` | 8s | SOC 每次 -1% |
| `VCellMin <= V0 - 50mV`，SOC `>0` | 2s | SOC 每次 -1% |

放电积分导致 `CapNow = 0`，或弱单体保护把 SOC 压到 0 且 `VCellMin <= V0` 时，会触发可信空电锚点。

## 9. 在线多点 OCV 有界融合

在线 OCV 融合用于解决只靠端点校准收敛慢的问题，但不把负载端电压当成真实 OCV 直接覆盖 SOC。当前策略是“轻载、持续、有方向、单步”的 guard：

| 条件 | 当前默认 |
| --- | --- |
| 开关 | `PROJECT_CFG_SOC_ONLINE_OCV_GUARD_ENABLE = 1` |
| 电流条件 | 充/放电电流必须在 `0.4A ~ C/10` 内；27Ah 默认上限约 `2.7A` |
| 电压条件 | `VCellMin/VCellMax` 必须在 `2000~5000mV`，且 `VCellMax >= VCellMin`、压差 `<=1000mV` |
| 状态条件 | 无三级保护故障；无 AFE1/AFE2/ADC/CBC/温度采样异常 |
| OCV 目标范围 | 只接受 `5% ~ 95%` 的 OCV 查表目标，端点仍交给满/空可信锚点 |
| 最小偏差 | OCV 目标和内部 SOC 至少相差 `3%` |
| 修正节奏 | 偏差持续 `30s` 后修正 `1%`，随后重新计时 |
| 方向限制 | 充电只允许上修；放电只允许下修 |
| LFP 平台区 | `20% ~ 90%` 中段默认不做在线 OCV 修正，只保留端点/静置修正 |

这套逻辑实现了多点 OCV 的收敛能力，但把收敛速度限制在用户可接受的范围内。对外 SOC 仍走平滑发布，在线 OCV 只改变内部目标，不会让显示瞬间跳变。

边界说明：

1. 高电流充/放电下端电压极化明显，在线 OCV guard 会拒绝修正，避免误校准。
2. 在线 OCV 只把 SOC 收敛到配置的误差窗口附近；最终 0%/100% 仍依赖可信空电/满电锚点。
3. 如果电流采样零点或 OCV 表本身不准，算法无法物理保证绝对精度，需要通过产测标定和曲线验证保证输入质量。
4. 异常电压、三级保护故障、AFE/ADC/CBC/温度异常均不参与 OCV/满电/静置类校准；弱单体保护属于安全下修，不按 OCV 校准处理。

## 10. 静置 OCV 与 RTC 休眠补偿

运行态静置 OCV 修正要求：

1. SOC 模块初始化完成。
2. 当前处于 `RELAX`。
3. 单体电压有效：`VCellMin >= 2000mV`。
4. 当前没有有效充/放电电流。
5. 运行态静置时，最低/最高单体电压需在 30s 内稳定在 `3mV` 窗口内。

静置时间分桶：

| 静置时间 | bucket | 最大修正步长 |
| ---: | ---: | --- |
| `< 10min` | 0 | 不修正 |
| `10min ~ 30min` | 1 | 向下最多 1% |
| `30min ~ 1h` | 2 | 向下最多 1% |
| `1h ~ 6h` | 3 | 向下最多 2%；若当前 SOC `>=90`，允许向上最多 1% |
| `>= 6h` | 4 | 向下最多 3%；若当前 SOC `>=90`，允许向上最多 2% |

`LIFEPO/LIFEPO2` 在平台区中段会进一步限制下修步长，避免 OCV 平台区误差造成明显跳变。

`SOC_ApplyRtcRelaxationCompensation(rest_seconds, vcell_min, vcell_max)` 用于 RTC 休眠唤醒。它复用 `SOC_ApplyRestCompensation()` 和弱单体保护，但入口最后会强制同步对外 SOC。RTC 入口本身依赖休眠时长和唤醒采样，不完全等同于运行态 30s 电压稳定窗口。

## 11. 弱单体保护

弱单体保护用于避免最低单体已经接近空电端点时，对外仍长期显示较高 SOC。

触发前提：

1. 当前不是充电。
2. `VCellMin >= 2000mV`。
3. `VCellMin <= V0 + 120mV`。

保护逻辑会先按 OCV 表得到 `target_soc`，再和电压窗口上限取更保守的值：

| `VCellMin` 条件 | SOC 上限 | 单次最大下修 |
| --- | ---: | ---: |
| `<= V0` | 0% | 5% |
| `<= V0 + 30mV` | 2% | 2% |
| `<= V0 + 60mV` | 4% | 1% |
| `<= V0 + 90mV` | 6% | 1% |
| `<= V0 + 120mV` | 8% | 1% |

当内部 SOC 被压到 0 且 `VCellMin <= V0` 时，记录可信空电锚点。

## 12. 对外 SOC、SOH 与容量发布

内部估算和对外发布分离：

| 层级 | 变量 | 说明 |
| --- | --- | --- |
| 内部 SOC | `SOC_Calculate_Element.u8SOC_Now` | 算法真实状态 |
| 内部容量 | `SOC_Calculate_Element.u32CapNow/u32CapFull/u32CapFactory` | 单位 `As * 10` |
| 对外 SOC | `SOC_Enhance_Element.u8_SOC` | 平滑后的 SOC |
| 全局上报 | `g_stCellInfoReport.SocElement.u16Soc` | RS485/CAN/LedBar 使用 |

正常情况下，`SOC_UpdateDisplaySoc()` 每 `SOC_DISPLAY_STEP_SECONDS = 1s` 向内部 SOC 靠近 1%。如果最低单体进入 `V0 + 30mV` 临界窗口且对外 SOC 高于内部 SOC，则每个 200ms 样本可下降 1%，避免低压风险被平滑延迟掩盖。

容量发布换算：

```text
Ah * 100 = (As * 10 + 180) / 360
```

SOH 计算：

| 条件 | 对外 SOH |
| --- | ---: |
| `u32CapFactory == 0` | 0 |
| `u32CapFull >= u32CapFactory` | 100 |
| 其他 | `100 * u32CapFull / u32CapFactory` |

循环次数：

1. 内部 `u32Cycle_times` 单位为 `cycle * 100`。
2. 每累计 `SOC_CYCLE_PERCENT_PER_COUNT = 80%` 放电量，循环次数增加 `100`，即对外 +1 次。
3. 对外 `u16_Cycle_times = u32Cycle_times / 100`，超过 `0xFFFF` 时饱和。

`SOC_Enhance_Element.u8_SOC_OCV_Cali` 当前实际被赋值为 `u8DSG_SOC_Int`，更像放电循环百分比累计的调试/兼容字段，不是独立的 OCV 校准状态。

## 13. FCC/SOH 学习

学习只在可信满电和可信空电锚点之间进行。

| 项 | 当前策略 |
| --- | --- |
| 学习路径 | 满电锚点到空电锚点，或空电锚点到满电锚点 |
| 首次学习跨度 | 至少 90% SOC |
| 后续学习跨度 | 至少 40% SOC |
| 容量允许范围 | 出厂容量的 50% ~ 110% |
| 单次 FCC 变化限制 | 不超过旧容量的 5% |
| 学习成功标志 | `SOC_LEARN_FLAG_LEARNED` |
| 学习后可信度 | `u16MaxErrorPercent = 5` |

如果在学习过程中方向反转，并且已经累计 `u32LearnPassedAs10`，本轮学习会被取消，避免半程充放电把 FCC 学坏。

## 14. SOC 快照与持久化

### 14.1 存储位置

| 数据 | 位置 | 说明 |
| --- | --- | --- |
| SOC 基础配置 | RW 参数区 `0x0801C400 / 0x0801CC00` | 属于 `OtherElement` 的 32 个 halfword |
| SOC 运行快照 | SOC journal `0x0801E000 / 0x0801E800` | V2 快照，双槽 journal |
| 自定义 SOC 表 | RAM `SOC_Table_Set[42]` | 当前不跨重启保存 |

`Flash.h` 中 `FLASH_STORAGE_PAGE_SIZE` 会随 `STM32F10X_MD` 选择为 `0x400`，否则为 `0x800`。SOC slot 起始地址仍是 `0x0801E000` 和 `0x0801E800`。

### 14.2 V2 快照字段

| 字段 | 说明 |
| --- | --- |
| `u16FormatVersion` | 固定 `FLASH_STORAGE_SOC_DATA_VERSION_V2 = 0x0002` |
| `u16SocNow` | 内部 SOC |
| `u16DsgSocInt` | 放电循环百分比累计 |
| `u16MaxErrorPercent` | 粗略可信度字段 |
| `u32CycleTimes` | 循环次数，内部单位 `cycle * 100` |
| `u32CapNow` | 当前剩余容量，单位 `As * 10` |
| `u32CapFull` | 当前 FCC/SOH 容量基准，单位 `As * 10` |
| `u32LearnPassedAs10` | 当前学习锚点之间累计通过电量 |
| `u16LearnAnchorSoc` | 当前学习锚点，0 或 100 |
| `u16LearnState` | `SOC_LEARN_STATE_*` |
| `u16Flags` | 学习标志 |
| `u16Reserved[4]` | 后续兼容预留 |

读取逻辑：

1. 优先按 V2 长度读取 SOC journal。
2. V2 无效时，尝试读取旧 V1 快照。
3. V1 只包含 `SOC / DSG_SOC_Int / CycleTimes`，读取后补齐 V2 字段。
4. 两种格式都无效时加载默认 60% 快照，并尝试写入 V2。

加载后的合法性检查：

| 字段 | 检查 |
| --- | --- |
| `SocNow` | `<= 100` |
| `DsgSocInt` | `<= 100` |
| `MaxErrorPercent` | `<= 100` |
| `LearnAnchorSoc` | `<= 100` |
| `LearnState` | 不超过 `SOC_LEARN_STATE_EMPTY_ANCHOR` |
| `CapFull` | 必须在出厂容量 50% ~ 110% 内，否则回退出厂容量并清除学习标志 |
| `CapNow` | 非 0 且不超过 `cap_base` 时使用，否则按 SOC 重新计算 |

### 14.3 保存触发

`SOC_PersistSnapshotIfChanged()` 当前在以下字段变化时保存：

1. `u8SOC_Now`
2. `u8DSG_SOC_Int`
3. `u32Cycle_times`
4. `u32CapFull`
5. `u16LearnAnchorSoc`
6. `u16LearnState`
7. `u16MaxErrorPercent`
8. `u16LearnFlags`

当前不会因为每个小的 `u32CapNow` 或 `u32LearnPassedAs10` 变化立即写 Flash。这样能降低写入压力，但代价是掉电时可能丢失尚未导致 SOC/循环/学习状态变化的细小积分。下一次触发保存时，V2 快照会携带当时的 `CapNow` 和 `LearnPassedAs10`。

如果默认快照首次保存失败，代码会把备份状态中的 SOC 置为 `0xFF`，后续运行中会持续尝试再次保存。

## 15. 上位机和控制入口

### 15.1 `0x10` 多寄存器写

| 地址 | 参数 | 当前行为 |
| --- | --- | --- |
| `0x2200` 起 | SOC 表 42 个寄存器 | 必须一次写满 42 个寄存器；只更新 RAM；调用 `InitData_SOC()` |
| `0x2318` | `OtherElement.u16Soc_Ah` | 保存到内部 Flash RW 参数区；影响容量基准 |
| `0x2319` | `OtherElement.u16Soc_Cycle_times` | 保存到内部 Flash RW 参数区；无快照或重置快照时影响初始循环 |
| `0x231A` | `OtherElement.u16Soc_V_100` | 保存到内部 Flash RW 参数区；影响满电端点 |
| `0x231B` | `OtherElement.u16Soc_V_0` | 保存到内部 Flash RW 参数区；影响空电端点 |

写 `0x2318~0x231B` 后会触发 `Sci_ApplyOtherElementSideEffects()`：

1. 调用 `InitData_SOC()` 重新加载配置和快照。
2. 设置 `u16_RefreshData_Flag = 2`，下一次 SOC 调度会执行容量/循环初始化，使容量基准回到新配置。

### 15.2 `0x06` 单寄存器控制

| 命令 | 行为 |
| --- | --- |
| 设置一次 SOC：`0x1005`，数据 `0~100` | 设置 `u16_RefreshData_Flag = 3` 和 `u8_SetSocOnce`，下一次 SOC 调度同步内部 SOC、`CapNow` 和对外 SOC |
| OCV 刷新 | 设置 `u16_RefreshData_Flag = 1`，按当前 `VCellMin` 查表；非充电状态不允许上修 |
| 容量/循环初始化 | 设置 `u16_RefreshData_Flag = 2`，重载容量配置、清放电累计、FCC 回出厂容量、清学习标志 |
| `SOC_Fixed` 功能开关 | 对外发布 SOC 强制显示 60%；通信打开时当前还会触发一次 OCV 刷新副作用 |
| `SOC_Zero` 功能开关 | 对外发布 SOC 强制显示 0%；通信打开时当前还会触发一次容量初始化副作用；该开关不写入旧 EEPROM 地址 |

`SOC_Fixed` 和 `SOC_Zero` 的显示覆盖发生在 `SOC_PublishReportData()`，覆盖的是 `g_stCellInfoReport.SocElement.u16Soc`，不是内部 `SOC_Calculate_Element.u8SOC_Now`。

## 16. 升级策略

升级策略当前由 `UpgradeParamPolicy.h` 和 `Project_Config.h` 控制。当前默认 `PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE = 0`，所以不会主动执行重置。

相关开关：

| 开关 | 当前作用 |
| --- | --- |
| `UPGRADE_PARAM_RESET_SOC_TABLE` | 把 `SOC_Table_Set` 重新加载为 `SOC_Table_Default`；由于表只在 RAM，重启本来也会恢复默认表 |
| `UPGRADE_PARAM_RESET_SOC_CONFIG` | 只重置 `OtherElement` 中的 SOC 基础参数：表选择、容量、循环次数、`V100`、`V0`，并保存 RW 参数区 |
| `UPGRADE_PARAM_RESET_SOC_SNAPSHOT` | 把 SOC journal 重写为默认 60% 快照 |

注意：

1. `UPGRADE_PARAM_RESET_SOC_CONFIG` 不会自动清空历史 SOC journal。
2. `UPGRADE_PARAM_RESET_SOC_SNAPSHOT` 不会自动修改 SOC 基础配置。
3. 如果升级后既要更换容量/端点，又要让现场 SOC 回到默认 60%，需要同时打开 SOC config 和 SOC snapshot，并递增策略版本。

## 17. 回放测试

当前新增主机侧策略回放脚本：

```bash
python3 tools/soc_replay_test.py
```

覆盖场景：

1. 单体不均衡时拒绝满电确认。
2. 单体接近 `V100` 且 taper 电流满足条件时确认满电。
3. 轻载放电时，在线 OCV 多点目标持续偏低后按 1%/30s 下修。
4. 轻载充电时，在线 OCV 多点目标持续偏高后按 1%/30s 上修。
5. 重载放电时拒绝在线 OCV 修正。
6. `LIFEPO` 平台区中段拒绝在线 OCV 修正。

该脚本不依赖 Keil/STM32 库，用于快速验证校准策略边界；固件发布前仍需要台架实测验证 OCV 表、采样电流和温度影响。

## 18. 当前评价

### 18.1 优点

1. 架构边界清楚：调度、算法、存储、通信基本分离。
2. 用户体验比纯电压查表稳定：内部 SOC 可快速校正，对外 SOC 默认平滑变化。
3. 满电和空电使用可信锚点，避免普通电压瞬态直接把 SOC 拉到 0 或 100。
4. V2 快照保存了容量、FCC、学习状态，掉电恢复体验明显好于只保存百分比。
5. `App_SOC()` 通过 AFE 样本序号避免重复积分旧电流，这是当前实现里很关键的正确性保护。
6. Flash 保存触发有节制，不会每 200ms 写入，写入压力可控。

### 18.2 风险与边界

1. 自定义 SOC 表不持久化。上位机写表重启后丢失，长期定制只能改固件默认表或新增表持久化。
2. SOC 算法仍依赖 AFE 电流零点和采样质量。小电流阈值为 0.4A，低于阈值的长期电流不会积分。
3. `u32CapNow` 小幅变化不单独触发保存，异常掉电时会损失未触发 SOC 变化的小积分。
4. 温度对容量和端点的影响当前没有建模，低温/高温下只能依赖端点和 OCV 的保守修正。
5. `SOC_Zero` / `SOC_Fixed` 的通信打开动作带有刷新副作用，调试时要区分“显示覆盖”和“内部状态被命令刷新”。
6. 当前实现不是专用 fuel gauge，也没有 impedance table、EKF 或温度容量矩阵，不应按高精度计量芯片的指标评估。

### 18.3 建议后续工作

1. 持续扩展 SOC 场景回放测试：冷启动、V1 迁移、CRC 错误、空电锚点、弱单体保护、RTC 唤醒、设置一次 SOC、容量学习。
2. 若客户需要上位机长期配置 OCV 表，新增 SOC 表内部 Flash 持久化，不能继续依赖 `SOC_Table_Set` RAM 表。
3. 若对 SOH 精度要求提高，先补温度分档端点和实验台数据，再考虑更复杂模型。
4. 保持当前 API 边界，不建议把通信、Flash 细节继续塞进 `SocEnhance.c`。

## 19. 文档核对结果

本次核对代码后，原文档中需要修正的关键点包括：

1. “充电容量到上限后 SOC 直接到 100%”不符合代码，当前真实逻辑是先到 99%，满电确认后才到 100%。
2. “SOC 表写入后落盘”不符合当前代码，当前只写 RAM。
3. “SOC 扩展项落到 EEPROM 地址 790”是旧 EEPROM 布局残留；当前 `0x2318~0x231B` 的 SOC 基础参数属于 `OtherElement`，保存到内部 Flash RW 参数区。
4. `SOC_Fixed` / `SOC_Zero` 需要区分显示覆盖和通信打开时的刷新副作用。

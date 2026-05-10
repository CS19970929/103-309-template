# SOC 模块逻辑说明

本文按当前代码实现记录 SOC 模块边界、数据流、存储、算法策略和验证要求。代码依据：

- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/SocEnhance.c/.h`
- `103 + 309/Project/Source/Flash.c/.h`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `tools/run_soc_host_c_test.py`
- `tools/soc_replay_test.py`

## 1. 设计目标

当前 SOC 面向 e-bike 场景，目标不是建立复杂电池模型，而是在工程可维护前提下做到：

1. 安时积分为主，保证骑行/充电过程连续。
2. 端点和静置 OCV 只做可信、小步修正，避免端电压瞬态导致跳变。
3. 显示 SOC 与内部 SOC 分离，用户看到的是平滑结果。
4. 低电压区优先防止高估，避免临近欠压仍显示较高电量。
5. SOH 暂按循环次数映射，不做 FCC 学习。
6. 保留原通信地址、函数签名和 Flash V2 快照格式。
7. OCV 表只作为可信场景下的参考锚点，不在骑行瞬态中直接覆盖 SOC。
8. 不把“满足 30min 静置”作为校准入口；运行态必须看到电压回弹变化已经变小，才允许慢速小步收敛。

运行态自动校准必须遵守硬原则：**每次最多校准 `1%`**。满电确认、低压收敛、静置/RTC OCV、低压表修正都只能按 `1%` 一步移动内部 SOC，不允许一次性从任意 SOC 跳到 `100%` 或 `0%`。启动初始化、上位机 `0x1005` 设置一次 SOC、`SOC_Fixed/SOC_Zero` 显示覆盖不属于运行态自动校准。

大电流骑行导致的 voltage sag 不能直接用于校准。当前以 `Idsg > C/2` 作为大电流判定，触发后进入 `30s` sag holdoff；holdoff 期间若 `VCellMin > V0 + 50mV`，禁止低压表和静置 OCV 校准。若电压已经低到 `V0 + 50mV` 以内，则认为进入真实末端，仍允许按 `1%` 步进收敛，保证保护前到 0。

为覆盖“重载后关机、未充分回弹又开机”的场景，大电流 holdoff 会通过 Flash 快照 `u16Flags bit0` 持久化。下次开机如果发现该标志，先进入 `5min` 回弹保护窗口，期间禁止把未回弹电压当成真实 OCV 下修 SOC。

## 2. 模块边界

| 文件 | 责任 |
| --- | --- |
| `SOC.c` | 加载 `OtherElement` 和 SOC 表，提供 `InitData_SOC()` / `App_SOC()`，按 AFE 电流样本序号驱动算法 |
| `SocEnhance.c` | 单一 `SOC_STATE` 状态机，负责积分、端点、OCV、低压表、显示、快照 |
| `SocEnhance.h` | 对外结构体、表选择枚举、API 声明 |
| `Flash.c/.h` | SOC V2 快照 journal 读写，并兼容旧 V1 快照 |
| `Sci_Upper.c` | 上位机写 SOC 表、SOC 基础参数、设置一次 SOC、功能开关 |
| `rtc_sleep.c` | RTC 唤醒后调用 `SOC_ApplyRtcRelaxationCompensation()` 做休眠静置修正 |
| `LedBar.c` / `Can_HDX.c` / `Sci_Upper.c` | 使用 `g_stCellInfoReport.SocElement` 的对外 SOC/SOH/容量/循环次数 |

## 3. 当前关键配置

| 项目 | 当前默认值 | 说明 |
| --- | ---: | --- |
| 电芯体系 | `TERNARYLI` | INR21700-45E，NMC/graphite |
| 串数 | `10` | `SNum = 10` |
| 标称容量 | `27Ah` | `OtherElement.u16Soc_Ah = 270`，单位 `10 * Ah` |
| 满电端点 | `4180mV` | `OtherElement.u16Soc_V_100`，满电确认不再硬依赖 taper 电流 |
| 显示空电端点 | `3000mV` | `OtherElement.u16Soc_V_0`，e-bike 显示 0% 口径 |
| SOH 下限 | `80%` | `SOC_SOH_MIN`，循环衰减不再低于 80% |
| 低压表范围 | `3400mV~2950mV` | 按电压和骑行电流给 SOC 设置保守上限，每次只修正 `1%` |
| 中低压弱约束 | `3500mV~3700mV` | 只向下限制明显高估 SOC，重载禁用，每次只修正 `1%` |
| 稳定静置校准 | `5min` 稳定后，每 `10min` 允许 `1%` | 电压未稳定时即使超过 30min 也不校准 |
| 重载重启回弹保护 | `5min` | 重载关机后重新开机，防止未回弹电压把 SOC 校低 |
| 控制器保护前归零 | `2950mV` | 默认 `V0 - 50mV`，以最高 `1%/200ms` 收敛到 0 |
| 默认启动 SOC | `60%` | 仅无快照且电压无效时使用 |
| 积分周期 | `200ms` | AFE 新电流样本触发 |
| 有效电流阈值 | `0.4A` | `Ichg/Idsg >= 4`，单位 `A * 10` |

## 4. OCV 表

当前三元锂默认表按 INR21700-45E 和 e-bike 3.0V 空电口径设置：

```text
4160/100, 4100/95, 4050/90, 3995/85, 3935/80,
3880/75, 3835/70, 3795/65, 3760/60, 3725/55,
3695/50, 3670/45, 3645/40, 3615/35, 3585/30,
3555/25, 3525/20, 3480/15, 3400/10, 3250/5,
3000/0
```

表选择仍兼容原枚举：

| 枚举 | 值 | 来源 |
| --- | ---: | --- |
| `SOC_TABLE_TEST` | `0` | 上位机写入的 `SOC_Table_Set`，仅 RAM 生效 |
| `SOC_TABLE_LIFEPO` | `1` | 固件内置 LFP 表 |
| `SOC_TABLE_TERNARYLI` | `2` | 当前三元锂默认表 |
| `SOC_TABLE_LIFEPO2` | `3` | 固件内置备用 LFP 表 |

上位机写 `0x2200` 起 42 个寄存器只更新 RAM 表，当前不新增 SOC 表持久化。

这张表的定位是“可信 OCV 参考”，不是所有电芯、所有温度、所有负载下的真实 SOC。运行态只在启动无快照、稳定静置/RTC 和受限弱约束场景使用它，避免把电芯批次、老化、温度或内阻差异直接放大成用户可见跳变。

## 5. 内部状态

`SocEnhance.c` 只保留一个内部状态结构 `SOC_STATE`：

| 字段 | 含义 |
| --- | --- |
| `cap_factory_as10` | 出厂容量，单位 `As * 10` |
| `cap_full_as10` | 当前有效容量，按 SOH 从出厂容量映射 |
| `cap_now_as10` | 当前剩余容量 |
| `cycle_x100` | 循环次数，内部单位 `cycle * 100` |
| `dsg_acc_as10` | 当前未满 1% 循环的放电累计 |
| `rem_ms` | 200ms 积分余量 |
| `soc` | 内部 SOC |
| `display_soc` | 对外显示 SOC |
| `soh` | 对外 SOH |
| `mode` | `RELAX / CHG / DSG` |
| `full/empty/rest/display/sag` ticks | 满电、低压表、静置、显示平滑、压降 holdoff 计数 |
| `stable_rest/short_rest` ticks | 静置稳定窗口和 OCV 小步校准节拍 |
| `mid_ticks` | 中低压弱约束每 `1%` 下修计数 |
| `rest_ref_vmin/rest_ref_vmax` | 静置稳定性参考电压 |
| `snapshot_flags` | 需要跨关机保留的 SOC 状态标志，目前 bit0 表示重载回弹保护 |

不再保留 FCC 学习状态、复杂在线 OCV 融合、学习锚点等复杂字段。Flash V2 结构中未使用字段仍保留为兼容位。

## 6. 启动流程

1. `InitE2PROM()` 加载默认参数和内部 Flash RW 参数。
2. `InitData_SOC()` 从 `OtherElement` 复制 SOC 基础配置到 `SOC_Enhance_Element`。
3. `soc_param_lib_init()` 初始化 `SOC_STATE` 容量、循环、SOH。
4. 读取 SOC journal：
   - 有效快照：恢复 `soc/cap_now/cycle/dsg_acc/snapshot_flags`。
   - 如果快照标记存在重载回弹保护，开机后先保持 `5min` 电压校准 holdoff。
   - 无有效快照且当前单体电压有效：按 OCV 表估算启动 SOC。
   - 无有效快照且电压无效：使用默认 `60%`。
5. 初始化后强制发布一次对外 SOC。

## 7. 安时积分

`App_SOC()` 只有在 `g_u32AfeCurrentSampleSeq` 变化时运行完整算法，避免重复积分旧电流。

方向判断：

| 条件 | 模式 |
| --- | --- |
| `Ichg >= 0.4A` 且 `Ichg >= Idsg` | `CHG` |
| `Idsg >= 0.4A` | `DSG` |
| 其他 | `RELAX` |

容量增量：

```text
delta_as10 = (current_A10 * 200ms + rem_ms) / 1000
```

处理规则：

- 充电增加 `cap_now_as10`，但满电确认前内部 SOC 最多到 `99%`。
- 放电减少 `cap_now_as10`，并累计等效循环。
- 方向切换时清空积分余量，避免反向继承。

## 8. SOH 与容量

SOH 当前只按循环次数映射：

```text
SOH = clamp(100 - cycle / 100, 80, 100)
cap_full = cap_factory * SOH / 100
```

循环计数：

```text
每累计 factory_capacity / 100 的放电量，cycle_x100 + 1
对外循环次数 = cycle_x100 / 100
```

该策略简单稳定，不依赖完整满空学习。后续若要提高 SOH 精度，应先补台架容量数据，再增加温度/倍率分档。

## 9. 满电与空电策略

### 满电确认

满电确认按电压可信度分两档，不再要求 `Ichg <= C/20`。这样可以兼容充电器不稳定 taper、充满后电流变成 0 或采样抖动的场景。

基础条件：

- 当前不是 `DSG`，即允许 `CHG` 或充电器停止后的 `RELAX`。
- 电压类校准门控通过。
- `VCellMax >= V100 - PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV`，当前默认 `4100mV`。

快速确认：

- `VCellMin >= V100 - PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV`，当前默认 `4150mV`。
- 单体压差 `<= PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV`，当前默认 `120mV`。
- 持续 `5s` 后确认 `100%`。

普通确认：

- 内部 SOC 已经 `>=95%`。
- `VCellMin >= V100 - PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV`，当前默认 `4100mV`。
- 单体压差 `<= PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV`，当前默认 `120mV`。
- 条件累计满足 `15s` 后确认 `100%`。

满电计数器采用“满足时累加、不满足时递减”的方式，不会因为一次采样抖动完全清零。确认前 SOC 最高只到 `99%`。确认条件满足后，内部 SOC 每次只上修 `1%`，直到 `100%`；显示 SOC 继续按显示平滑跟随，不做大跳变。

### 空电与低压表

低压逻辑改为表驱动，按 `V0` 相对电压和放电电流档位给内部 SOC 设置保守上限。默认 `V0=3000mV` 时表如下：

| VCellMin | RELAX 目标/周期 | 轻载 `<=C/5` | 中载 `<=C/2` | 重载 `>C/2` |
| --- | --- | --- | --- | --- |
| `<=3400mV` | `18% / 24s` | `22% / 20s` | `30% / 16s` | `40% / 12s` |
| `<=3300mV` | `14% / 18s` | `18% / 15s` | `25% / 12s` | `32% / 9s` |
| `<=3200mV` | `12% / 12s` | `14% / 10s` | `20% / 8s` | `25% / 6s` |
| `<=3100mV` | `8% / 7s` | `10% / 6s` | `14% / 5s` | `18% / 4s` |
| `<=3050mV` | `4% / 4s` | `5% / 3s` | `8% / 2s` | `12% / 1.6s` |
| `<=3000mV` | `0% / 2s` | `0% / 1s` | `0% / 1s` | `0% / 1s` |
| `<=2975mV` | `0% / 1s` | `0% / 1s` | `0% / 0.2s` | `0% / 0.2s` |
| `<=2950mV` | `0% / 0.2s` | `0% / 0.2s` | `0% / 0.2s` | `0% / 0.2s` |

表中的“周期”表示每下修 `1%` 所需时间，不允许一次下修超过 `1%`。低压逻辑在非充电状态下运行，包含放电和放电截止后的 `RELAX`。这样低压保护后即使电流变成 0，SOC 也能继续收敛到 0%。电压采样必须基本有效，避免异常采样把 SOC 误清零。

## 10. OCV 修正与低压表门控

电压类修正统一门控：

- `2000mV <= VCellMin <= VCellMax <= 5000mV`。
- 单体压差 `<=1000mV`。
- 当 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT=1` 时，无三级保护故障。
- 当 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT=1` 时，无 AFE1/AFE2/ADC/CBC/温度异常。

### 启动 OCV

无有效 SOC 快照时，如果上电时已有有效电压采样，直接按 OCV 表设置启动 SOC；否则使用默认 60%。

### 静置/RTC OCV

运行态 `RELAX` 不再按固定 30min 到点校准，而是按“最小静置时间 + 电压回弹稳定”判断是否可以近似使用 OCV：

- 只在 `RELAX` 下运行。
- 电压合法、单体压差 `<=200mV`。
- 没有大电流 sag/rebound holdoff。
- `VCellMin/VCellMax` 相对静置参考值波动 `<=30mV`。
- 连续稳定 `5min` 后，每累计 `10min` 最多按 OCV 表修正 `1%`。
- 如果中途电压跳动、重新骑行、进入低压表或触发 holdoff，静置可信度清零。
- 即使 `RELAX` 已经超过 `30min`，只要电压仍在回弹/跳动，也不会强行按 OCV 校准。

RTC 唤醒补偿仍按传入休眠时长和当前电压做一次小步修正，最小休眠时间使用同一 `5min` 置信下限；如果快照里带有重载回弹标志，开机 `5min` rebound holdoff 会继续阻断这类电压校准。

当前版本不强制显示跳变，显示 SOC 继续按平滑规则跟随。

### 骑行中电压表修正

骑行中电压修正分两层：中低压弱约束和低压表。两者都不是直接用端电压覆盖 SOC，而是在安时积分结果之上设置“当前电压和电流档位下不应高于多少”的上限。

中低压弱约束覆盖 `V0 + 500mV` 到 `V0 + 700mV`，默认即 `3500mV~3700mV`：

| VCellMin | RELAX 目标/周期 | 轻载 `<=C/5` | 中载 `<=C/2` | 重载 `>C/2` |
| --- | --- | --- | --- | --- |
| `<=3500mV` | `25% / 90s` | `32% / 90s` | `42% / 120s` | 禁用 |
| `<=3600mV` | `35% / 120s` | `42% / 120s` | `50% / 150s` | 禁用 |
| `<=3650mV` | `45% / 150s` | `50% / 150s` | `58% / 180s` | 禁用 |
| `<=3700mV` | `55% / 180s` | `60% / 180s` | 禁用 | 禁用 |

中低压弱约束只向下修正明显高估 SOC，SOC 低于目标时不向上修正。它要求单体压差 `<=200mV`，且不能处于 sag/rebound holdoff；重载档位禁用，避免爬坡或急加速电压下陷导致误校准。

低压表覆盖 `V0 + 400mV` 到 `V0 - 50mV`，默认即 `3400mV~2950mV`：

- SOC 低于表目标时不向上修正。
- SOC 高于表目标时，每到表中周期只下修 `1%`。
- 电流越大，允许的 SOC 上限越高，避免重载 voltage sag 过度下修。
- 电压越低，目标越低、周期越短，保证控制器保护前可收敛到 0。
- 低压表活跃时，不叠加静置 OCV，保证同一调度最多一个校准动作。
- 大电流 `>C/2` 或刚结束大电流后的 `30s` 内，若电压还高于 `V0 + 50mV`，低压表不参与校准；只有真实进入末端区才允许继续收敛。

## 11. 显示体验

内部 SOC 和显示 SOC 分离：

| 场景 | 显示跟随 |
| --- | --- |
| 普通上升/下降 | `5s / 1%` |
| 充电上升 | 与普通放电一致，`5s / 1%` |
| 低压下降 | `VCellMin <= V0 + 50mV` 时 `1s / 1%` |
| 设置一次 SOC / 工厂显示覆盖 | 可强制同步 |
| 自动满电 / 自动空电校准 | 内部每次 `1%`，显示继续平滑 |
| RTC/静置 OCV 小步修正 | 不强制同步，继续平滑跟随 |
| `SOC_Fixed` | 只对外显示 60%，不破坏内部 SOC |
| `SOC_Zero` | 只对外显示 0%，不破坏内部 SOC |

## 12. Flash 快照

SOC 运行快照仍使用 `STORAGE_FLASH_SOC_DATA` V2，地址不变：

| Slot | 地址 |
| --- | --- |
| A | `0x0801E000` |
| B | `0x0801E800` |

当前使用字段：

| 字段 | 用途 |
| --- | --- |
| `u16SocNow` | 内部 SOC |
| `u16DsgSocInt` | 放电循环小数百分比兼容字段 |
| `u32CycleTimes` | `cycle * 100` |
| `u32CapNow` | 当前剩余容量 |
| `u32CapFull` | 当前有效容量 |
| `u32LearnPassedAs10` | 当前未满 1% 循环的放电累计 |
| `u16Flags bit0` | 重载后关机/重启的回弹保护标志 |

保存触发：

- 内部 SOC 变化。
- 循环次数变化。
- `cap_full_as10` 因 SOH 变化。
- `snapshot_flags` 变化。
- 设置一次 SOC、容量/循环初始化、RTC 修正等需要落盘的动作。

## 13. 通信兼容

保持原接口和寄存器：

| 功能 | 入口 |
| --- | --- |
| 设置一次 SOC | `0x1005` |
| 写 SOC 表 | `0x2200` 起 42 个寄存器 |
| 写容量/循环/V100/V0 | `0x2318~0x231B` |
| 固定 SOC 显示 | 系统功能开关 `SOC_Fixed` |
| SOC 清零显示 | 系统功能开关 `SOC_Zero` |

保持原函数签名：

- `InitData_SOC()`
- `App_SOC()`
- `SOC_UpdateSampleData()`
- `SOC_PublishReportData()`
- `SOC_ApplyRtcRelaxationCompensation()`
- `SOC_ResetStoredSnapshotToDefault()`

## 14. 已验证项目

真实 C 源码主机测试：

```bash
python3 tools/run_soc_host_c_test.py
```

当前覆盖 10 个关键 C 路径：启动 OCV、放电积分、Type-C 电流抵消、满电确认、低压到 0、稳定静置小步校准、静置超过 30min 但电压不稳定时不校准、重载回弹标志清除、显示覆盖不污染内部 SOC、设置一次 SOC 保存快照。该测试直接编译 `SOC.c`、`SocEnhance.c` 和 `PubFunc.c`，硬件、Flash 和全局采样依赖由 host harness 替代。

主机回放矩阵：

```bash
python3 tools/soc_replay_test.py
```

当前覆盖 40 个场景：

- 无快照默认 60%。
- 无快照且电压有效时按启动 OCV。
- V2 快照恢复容量和循环 SOH。
- 设置一次 SOC。
- 充/放电积分。
- SOH 循环映射。
- 满电确认。
- 空电/低压表。
- RTC/静置 OCV 小步修正与显示平滑。
- 异常方向/故障不做 OCV。
- 显示平滑。
- `SOC_Fixed/SOC_Zero` 只影响显示。
- 低压表电流分档速率和重载屏蔽。
- 稳定静置不足 30min 时允许慢速 OCV 收敛。
- 静置超过 30min 但电压不稳定时不做 OCV 校准。
- 中低压弱约束按电压、电流、周期慢速下修。
- 中低压弱约束条件中断后计数器清零。
- 重载关机后的快照回弹标志会阻止开机电压误校准。
- OCV 表单调性、Python 模型与 C 源码表格一致性、方向阈值、满电计数器递减、低压/中低压全表矩阵。
- 异常电压矩阵、长期静置收敛、回弹保护过期清标志、随机运行不变量。

本机已通过真实 C 源码主机测试、Python 回放矩阵和 `clang -fsyntax-only -std=c99 -Wall -Wextra` 语法检查。Keil 完整编译仍需 Windows + Keil MDK 环境。

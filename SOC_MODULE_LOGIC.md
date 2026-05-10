# SOC 模块逻辑说明

本文按当前代码实现记录 SOC 模块边界、数据流、存储、算法策略和验证要求。代码依据：

- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/SocEnhance.c/.h`
- `103 + 309/Project/Source/Flash.c/.h`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `tools/soc_replay_test.py`

## 1. 设计目标

当前 SOC 面向 e-bike 场景，目标不是建立复杂电池模型，而是在工程可维护前提下做到：

1. 安时积分为主，保证骑行/充电过程连续。
2. 端点和静置 OCV 只做可信、小步修正，避免端电压瞬态导致跳变。
3. 显示 SOC 与内部 SOC 分离，用户看到的是平滑结果。
4. 低电压区优先防止高估，避免临近欠压仍显示较高电量。
5. SOH 暂按循环次数映射，不做 FCC 学习。
6. 保留原通信地址、函数签名和 Flash V2 快照格式。

运行态自动校准必须遵守硬原则：**每次最多校准 `1%`**。满电确认、低压收敛、静置/RTC OCV、低压表修正都只能按 `1%` 一步移动内部 SOC，不允许一次性从任意 SOC 跳到 `100%` 或 `0%`。启动初始化、上位机 `0x1005` 设置一次 SOC、`SOC_Fixed/SOC_Zero` 显示覆盖不属于运行态自动校准。

大电流骑行导致的 voltage sag 不能直接用于校准。当前以 `Idsg > C/2` 作为大电流判定，触发后进入 `30s` sag holdoff；holdoff 期间若 `VCellMin > V0 + 50mV`，禁止低压表和静置 OCV 校准。若电压已经低到 `V0 + 50mV` 以内，则认为进入真实末端，仍允许按 `1%` 步进收敛，保证保护前到 0。

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

不再保留 FCC 学习状态、在线 OCV 稳定窗口、学习锚点等复杂字段。Flash V2 结构中未使用字段仍保留为兼容位。

## 6. 启动流程

1. `InitE2PROM()` 加载默认参数和内部 Flash RW 参数。
2. `InitData_SOC()` 从 `OtherElement` 复制 SOC 基础配置到 `SOC_Enhance_Element`。
3. `soc_param_lib_init()` 初始化 `SOC_STATE` 容量、循环、SOH。
4. 读取 SOC journal：
   - 有效快照：恢复 `soc/cap_now/cycle/dsg_acc`。
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

运行态 `RELAX` 满 30min 或 RTC 唤醒传入休眠时长后，按 OCV 小步修正内部 SOC：

| 静置时长 | 最大修正 |
| ---: | ---: |
| `30min~1h` | `1%` |
| `1h~6h` | `1%` |
| `>=6h` | `1%` |

当前版本不强制显示跳变，显示 SOC 继续按平滑规则跟随。

### 骑行中电压表修正

低压表覆盖 `V0 + 400mV` 到 `V0 - 50mV`，默认即 `3400mV~2950mV`。它不是直接用端电压覆盖 SOC，而是在安时积分结果之上设置“当前电压和电流档位下不应高于多少”的上限：

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

保存触发：

- 内部 SOC 变化。
- 循环次数变化。
- `cap_full_as10` 因 SOH 变化。
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

主机回放：

```bash
python3 tools/soc_replay_test.py
```

当前覆盖 23 个场景：

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

本机已通过 `clang -fsyntax-only -std=c99 -Wall -Wextra` 语法检查。Keil 完整编译仍需 Windows + Keil MDK 环境。

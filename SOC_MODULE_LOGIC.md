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

## 2. 模块边界

| 文件 | 责任 |
| --- | --- |
| `SOC.c` | 加载 `OtherElement` 和 SOC 表，提供 `InitData_SOC()` / `App_SOC()`，按 AFE 电流样本序号驱动算法 |
| `SocEnhance.c` | 单一 `SOC_STATE` 状态机，负责积分、端点、OCV、低压 guard、显示、快照 |
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
| 满电端点 | `4180mV` | `OtherElement.u16Soc_V_100` |
| 显示空电端点 | `3000mV` | `OtherElement.u16Soc_V_0`，保留低压余量 |
| 放电快速兜底 | `2750mV` | 限制 SOC 到 `<=1%` |
| 放电强制兜底 | `2500mV` | 强制 SOC 到 `0%` |
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
| `full/empty/rest/low_guard/display` ticks | 端点、静置、低压 guard、显示平滑计数 |

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
SOH = clamp(100 - cycle / 50, 70, 100)
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

必须同时满足：

- 当前为 `CHG`。
- `Ichg <= max(0.4A, C/20)`。
- `VCellMax >= V100` 且 `VCellMax >= 4180mV`。
- `VCellMin >= 4120mV`。
- 单体压差 `<=120mV`。
- 持续 `60s`。

确认前 SOC 最高只到 `99%`，确认后内部和显示 SOC 同步到 `100%`。

### 空电与低压兜底

| 条件 | 行为 |
| --- | --- |
| `VCellMin <= V0(默认3000mV)` 持续 4s | 每次最多向 0% 下修 5% |
| `VCellMin <= 2750mV` | 立即限制到 `<=1%` |
| `VCellMin <= 2500mV` | 强制 `0%` |

低压逻辑只在电压采样基本有效时运行，避免异常采样把 SOC 误清零。

## 10. OCV 修正与低电压 guard

电压类修正统一门控：

- `2000mV <= VCellMin <= VCellMax <= 5000mV`。
- 单体压差 `<=1000mV`。
- 无三级保护故障。
- 无 AFE1/AFE2/ADC/CBC/温度异常。

### 启动 OCV

无有效 SOC 快照时，如果上电时已有有效电压采样，直接按 OCV 表设置启动 SOC；否则使用默认 60%。

### 静置/RTC OCV

运行态 `RELAX` 满 30min 或 RTC 唤醒传入休眠时长后，按 OCV 小步修正内部 SOC：

| 静置时长 | 最大修正 |
| ---: | ---: |
| `30min~1h` | `1%` |
| `1h~6h` | `2%` |
| `>=6h` | `3%` |

当前版本不强制显示跳变，显示 SOC 继续按平滑规则跟随。

### 低电压轻载 guard

当 `VCellMin <= 3400mV` 且处于静置或轻载放电时，按 OCV 表给内部 SOC 设置保守上限：

- 轻载阈值：`Idsg <= C/5`。
- `VCellMin > 3250mV`：允许 OCV 目标上方 `8%` 余量。
- `VCellMin <= 3250mV`：允许 OCV 目标上方 `3%` 余量。
- 若内部 SOC 高于上限，按 `10s / 1%` 慢速下修。
- 重载骑行时不触发，避免电压 sag 造成误判。

## 11. 显示体验

内部 SOC 和显示 SOC 分离：

| 场景 | 显示跟随 |
| --- | --- |
| 普通上升/下降 | `5s / 1%` |
| 充电上升 | `10s / 1%` |
| 低压下降 | `1s / 1%` |
| 满电确认 / 空电强制 / 设置一次 SOC | 可强制同步 |
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

当前覆盖 14 个场景：

- 无快照默认 60%。
- 无快照且电压有效时按启动 OCV。
- V2 快照恢复容量和循环 SOH。
- 设置一次 SOC。
- 充/放电积分。
- SOH 循环映射。
- 满电确认。
- 空电/低压兜底。
- RTC/静置 OCV 小步修正与显示平滑。
- 异常方向/故障不做 OCV。
- 显示平滑。
- `SOC_Fixed/SOC_Zero` 只影响显示。
- 低电压轻载 guard 速率和重载屏蔽。

本机已通过 `clang -fsyntax-only -std=c99 -Wall -Wextra` 语法检查。Keil 完整编译仍需 Windows + Keil MDK 环境。

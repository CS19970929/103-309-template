# SOC 体验调参配置说明（2026-05-12）

## 目标

本次改动不重写 SOC 主算法，而是把用户体验相关的三类问题做成可配置参数，方便后续台架和实车测试时快速调整：

1. 低电末端过于保守，最后 20% 可能掉得快。
2. 静置 OCV 校准偏慢，SOC 偏差恢复需要较长时间。
3. 低电显示体验需要能单独调快或调慢。

所有参数都集中在 `103 + 309/Project/Source/conf/Project_Config.h` 的 `SOC 校准参数` 段，并由 `Project_BuildGuard.h` 做范围检查。

## 新增配置项

### 静置 OCV

| 参数 | 默认值 | 作用 | 调小效果 | 调大效果 |
| --- | ---: | --- | --- | --- |
| `PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS` | `300` | 电压稳定多久后认为 OCV 可信 | 更快记录 OCV target | 更抗电压回弹误判 |
| `PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS` | `600` | 稳定静置刷新 target、后续充放电消化差值的节拍 | 更快修正 SOC 偏差 | 更平滑、更保守 |
| `PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS` | `1800` | 久置且 OCV 低于内部 SOC 时，静置下修节拍 | 久置虚高更快消除 | 停车显示更稳定 |

说明：

- `PROJECT_CFG_SOC_REST_OCV_SECONDS` 仍保留，作为久置低 OCV 下修的起始等待/兼容配置。
- 静置 OCV 仍然每次最多修正 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT`，当前建议保持 `1%`。
- 如果后续测试希望停车后更快恢复，可先试 `180 / 300 / 1800`。

### 低压尾段

| 参数 | 默认值 | 作用 | 建议调法 |
| --- | ---: | --- | --- |
| `PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV` | `400` | `VCellMin <= V0 + offset` 时才允许进入低压尾段表 | 最后 20% 掉得快时，先降到 `300` |
| `PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT` | `0` | 只抬高 `V0` 以上软尾段目标 SOC | 低电区显示过低时试 `3..8` |
| `PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT` | `100` | 只放慢或加快 `V0` 以上软尾段下修速度 | 掉得快时试 `125..180` |

安全边界：

- 这些软尾段参数只作用于 `V0` 以上的表项。
- `V0` 及以下仍保持强制向 `0%` 收敛，避免欠压前还显示很多电。
- 大电流 sag/rebound holdoff 仍优先生效，防止爬坡压降误触发。

### 显示平滑

| 参数 | 默认值 | 作用 |
| --- | ---: | --- |
| `PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS` | `5` | 普通显示每下降/上升 1% 的时间 |
| `PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS` | `5` | 充电显示上升每 1% 的时间 |
| `PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS` | `1` | 低压区显示下降每 1% 的时间 |
| `PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV` | `50` | `VCellMin <= V0 + offset` 时进入低压显示加速 |
| `PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV` | `50` | `VCellMin <= V0 - value` 时每 200ms 最多下降 1% |

建议：

- 用户觉得普通显示太迟钝：`DISPLAY_NORMAL_SECONDS` 可从 `5` 调到 `3`。
- 用户觉得低电掉得太吓人：先调低压尾段参数，不要只调显示速度。
- 欠压前显示仍偏高：调大 `DISPLAY_LOW_OFFSET_MV` 或减小 `DISPLAY_LOW_SECONDS`。

## 推荐测试组合

### 组合 A：默认稳健

保持当前默认值。适合量产基线和回归测试。

### 组合 B：低电更线性

```c
#define PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV 300
#define PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT 5
#define PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT 150
```

预期：`3400mV` 附近不再过早拉低，`3300mV~3050mV` 下修更平缓，`3000mV` 及以下仍快速归零。

### 组合 C：静置校准更快

```c
#define PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS 180
#define PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS 300
#define PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS 1800
```

预期：停车后更快记录 OCV target，后续充放电更快消化偏差。实车测试时要重点看停车电压仍在回弹时是否误校准。

### 组合 D：显示响应更快

```c
#define PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS 3
#define PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS 3
#define PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS 1
```

预期：用户更快看到 SOC 变化，但普通区也更容易感知到 1% 级跳动。

## 每次调参后的无板验证

```powershell
py tools\run_soc_host_c_test.py
py tools\soc_replay_test.py
py tools\soc_ride_sim_report.py --report build\SOC_RIDE_SIM_REPORT_tuning.md --csv build\SOC_RIDE_SIM_SAMPLES_tuning.csv
py tools\project_check.py
git diff --check
```

通过无板验证后，再上板读取 `0xD000` 和 `0xD300`，并用 `tools\start_soc_test_ui.ps1` 做充电、骑行、爬坡、低压、停车静置和休眠唤醒测试。

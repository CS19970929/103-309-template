# SOC 校准操作逻辑梳理

文档日期：2026-06-25  
适用范围：当前 `103 + 309` BMS App SOC 源码  
源码优先级：如本文与源码冲突，以当前源码为准。

## 1. 范围与结论

本文只梳理会改变 SOC、容量基准、静置校准状态或 SOC 持久化快照的逻辑。当前 SOC 主估算是容量积分，满电锚点、低压尾端、长静置 OCV、RTC 静置补偿、启动 OCV 和人工命令都是对容量积分的校准或覆盖。

当前源码里没有运行时可写 SOC 表、手动 OCV 刷新、mid-tail、short-rest deferred OCV 和显示 SOC 平滑层。对外发布的 `g_stCellInfoReport.SocElement.u16Soc` 直接来自内部 `s_soc.soc`。

主要源码入口：

| 类别 | 源码入口 |
|---|---|
| 周期 SOC 计算 | `SOC.c::App_SOC()` -> `SocEnhance.c::SOC_IntEnhance_Ctrl()` |
| RTC 静置补偿 | `rtc_sleep.c` -> `rtc_sleep_port.c::RtcSleep_PortApplySocRtcRest()` -> `SOC_ApplyRtcRelaxationCompensation()` |
| 人工容量重算 | `Sci_Upper.c` -> `SOC_RequestCapacityReset()` |
| 人工设 SOC | `Sci_Upper.c::Sci_WrReg_0x06_SetSocOnce()` -> `SOC_RequestSetOnce()` |
| 启动恢复 | `InitData_SOC()` -> `soc_param_lib_init()` -> `soc_load_or_default()` |
| 休眠前保存 | `LowPowerSleep_SaveCoreState()` -> `SOC_SaveSnapshotBeforeSleep()` |

## 2. 当前关键配置

配置来自 `103 + 309/Project/Source/conf/Project_Config.h`。

| 配置 | 当前值 | 作用 |
|---|---:|---|
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | `80` | 满电确认时，单体最低电压需要接近满电阈值 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | `120` | 满电确认最大压差 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS` | `15` | 满电确认每次上修前的连续确认时间 |
| `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | `2000` | OCV/full/rest 校准电压有效下限 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | `5000` | OCV/full/rest 校准电压有效上限 |
| `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` | `30` | 大电流放电后的回弹保护时间 |
| `PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV` | `50` | sag hold 阻断校准的 V0 上方余量 |
| `PROJECT_CFG_SOC_REST_OCV_ENABLE` | `1` | 启用长静置 OCV 慢下修 |
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | `3600` | 长静置 OCV 目标可信门槛 |
| `PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS` | `1800` | 长静置目标有效后，每 1% 下修的等待时间 |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | `1` | 自动校准单步最大变化 |
| `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` | `15` | 正常运行积分时计入的板载自耗 |
| `PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV` | `400` | 低压尾端校准启用区间 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG` | `0` | 升级时默认不重置 SOC 参数 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT` | `0` | 升级时默认不重置 SOC 快照 |

内部固定常量：

| 常量 | 当前值 | 作用 |
|---|---:|---|
| `SOC_TICK_MS` | `200ms` | SOC 周期 tick |
| `SOC_TICKS_PER_SECOND` | `5` | 秒到 SOC tick 换算 |
| `SOC_CURRENT_ACTIVE_A10` | `2` | 充/放/静置模式判定门槛，等于 200mA |
| `SOC_VALID_MAX_DELTA_MV` | `300mV` | 通用校准最大压差 |
| `SOC_REST_MAX_DELTA_MV` | `200mV` | 静置 OCV 最大压差 |
| `SOC_REST_STABLE_DELTA_MV` | `30mV` | 静置稳定参考漂移限制 |
| `SOC_FULL_CONFIRM_MIN_VMAX_MV` | `4180mV` | 满电确认额外要求的最高单体门槛 |
| `SOC_REBOUND_BOOT_HOLDOFF_SECONDS` | `300s` | 带 rebound flag 启动后的保护时间 |

## 3. 基础状态与公共校准门槛

`SOC_STATE s_soc` 是 SOC 核心状态。校准相关字段如下：

| 字段 | 含义 |
|---|---|
| `cap_factory_as10` | 额定容量基准，由 `OtherElement.u16Soc_Ah` 换算 |
| `cap_full_as10` | 按 SOH 修正后的满容量 |
| `cap_now_as10` | 当前剩余容量 |
| `soc` | 内部真实 SOC，对外直接发布 |
| `cycle_x100` | 循环次数放大 100 倍 |
| `dsg_acc_as10` | 放电累计，用于循环计数 |
| `rem_mams` | 积分余量，避免小电流被截断 |
| `full_ticks` | 满电确认计数 |
| `empty_ticks` | low-tail 计数 |
| `rest_soc_ticks` | 总静置计数，单位为 200ms tick |
| `stable_rest_soc_ticks` | 稳定静置计数，单位为 200ms tick |
| `long_rest_down_soc_ticks` | 长静置目标有效后的慢下修计数 |
| `rest_ref_vmin/rest_ref_vmax` | 静置稳定参考电压 |
| `rest_down_valid/rest_down_target` | 长静置 OCV 下修目标 |
| `rest_ocv_fired` | 本轮长静置 OCV 是否已评估 |
| `sag_hold_ticks` | 大电流放电后的回弹保护计数 |
| `snapshot_flags` | 当前只保存 rebound hold flag |

公共 SOC 赋值函数是 `soc_set(soc)`。它会限制 SOC 到 `0..100`，同步重算 `cap_now_as10 = cap_full_as10 * soc / 100`，并清零积分余量 `rem_mams`。满电、尾端、静置、启动 OCV 和人工设 SOC 最终都会通过它改写 SOC。

通用校准允许条件由 `soc_calibration_allowed()` 给出：

| 条件 | 当前行为 |
|---|---|
| `VCellMin/VCellMax` 有效 | 必须在 `2000..5000mV` 内，且 `VCellMax >= VCellMin` |
| 通用压差 | `VCellMax - VCellMin <= 300mV` |
| sag hold | 部分校准还会额外检查 `soc_sag_hold_blocks_calibration()` |

`soc_sag_hold_blocks_calibration()` 的含义是：大电流放电后，如果 `sag_hold_ticks > 0`，电压有效，并且 `VCellMin > V0 + 50mV`，则阻断 OCV/low-tail 等电压校准，避免电压跌落和回弹造成误校准。

## 4. 周期主链路顺序

`SOC.c::App_SOC()` 只在 `AfeCurrent_GetSeq()` 变化时进入核心计算。核心顺序在 `SOC_IntEnhance_Ctrl(net_current_ma)` 中固定为：

1. `soc_direction(net_current_ma)` 判定 `CHG/DSG/RELAX`。
2. `soc_integrate(net_current_ma)` 做库仑积分，并计入板载自耗。
3. `soc_update_sag_hold(mode, net_current_ma)` 更新大电流放电后的回弹保护。
4. 如果非放电且满电条件成立，先执行 `soc_apply_full_confirm()`。
5. 如果没有满电确认，再尝试 `soc_select_empty_tail_step()` 和 `soc_apply_empty_tail()`。
6. 如果 low-tail active 或 sag hold 阻断，清空静置置信度。
7. 如果本周期没有满电/尾端校准，才推进 `soc_update_rest_timer()`。
8. `soc_save_if_needed()` 按快照差异保存。
9. `SOC_PublishReportData()` 发布到 `g_stCellInfoReport.SocElement`。

这意味着优先级是：容量积分先发生，之后满电锚点优先于低压尾端，低压尾端和 sag hold 会打断长静置 OCV。

## 5. 容量积分

容量积分是 SOC 的主估算，不属于电压校准，但它是所有校准的基线。

`SOC_GetNetCurrentMilliAmp()` 把上报充电电流、上报放电电流和 Type-C 输出折算成 signed mA：

```text
net_current_ma = (u16Ichg - (u16IDischg + TypeC等效放电A10)) * 100mA
```

`soc_integrate()` 在 signed 电流上扣除板载自耗：

```text
integrate_current_ma = net_current_ma - PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA
```

当前板载自耗为 `15mA`。普通 RELAX 运行时，如果外部电流为 0，SOC 会按 `-15mA` 缓慢积分下降。RTC STOP 路径不额外扣这部分自耗。

积分边界：

| 行为 | 说明 |
|---|---|
| 放电积分 | `delta_as10 < 0` 时增加 `dsg_acc_as10`，并按额定容量 1% 单位累计循环次数 |
| SOH | `cycle_x100` 每满 100 个循环降低 1%，最低 `80%` |
| 容量限幅 | `cap_now_as10` 限制在 `0..cap_full_as10` |
| 充电到顶 | 充电积分如果让 SOC 到 100%，且之前小于 100%，会压到 99%，等待满电确认真正发布 100% |

## 6. 启动恢复与启动 OCV

入口：`soc_param_lib_init()` -> `soc_load_or_default()`。

启动先从 `OtherElement.u16Soc_Ah`、`OtherElement.u16Soc_Cycle_times` 初始化容量和循环基线，再加载 Flash SOC snapshot。

| 分支 | 条件 | 行为 |
|---|---|---|
| 有效 snapshot | `StorageFlash_LoadSocData()` 成功，`u16SocNow <= 100`，`u16DsgSocInt <= 100` | 恢复 `cycle_x100/cap_now/dsg_acc/snapshot_flags`，再由容量或 SOC 字段恢复当前 SOC |
| snapshot 带 rebound flag | `u16Flags & SOC_SNAPSHOT_FLAG_REBOUND_HOLD` | 设置 `sag_hold_ticks = 300s * 5`，启动后继续阻断回弹误校准 |
| 无效 snapshot 且电压允许 | `soc_calibration_allowed()` 成立 | 使用 `VCellMin` 查 OCV 表，直接 `soc_set(soc_ocv_percent())` |
| 无效 snapshot 且电压不允许 | 电压无效或压差过大 | 使用默认启动 SOC `60%` |

当前化学体系由 `PROJECT_CFG_BAT_CHEMISTRY` 编译期选择。`OtherElement.u16Soc_TableSelect` 仍可被协议读写，但当前算法不按它运行时切表。

## 7. 满电锚点校准

入口：`soc_full_confirm_allowed()` + `soc_apply_full_confirm()`。

满电锚点只在 `mode != SOC_MODE_DSG` 时允许。触发条件：

| 条件 | 当前阈值 |
|---|---|
| 通用校准允许 | 电压有效，通用压差 <= `300mV` |
| 最高单体要求 | `VCellMax > 4180mV` |
| 接近满电阈值 | `VCellMax >= OtherElement.u16Soc_V_100 - 80mV` |
| 最低单体要求 | `VCellMin >= OtherElement.u16Soc_V_100 - 80mV` |
| 满电压差 | `VCellMax - VCellMin <= 120mV` |
| 连续确认时间 | `15s` |

执行方式：

| 行为 | 说明 |
|---|---|
| active 时 | `full_ticks` 每 200ms 加 1，`empty_ticks` 清零 |
| 达到 15s | `soc_step(s_soc.soc, 100, 1)`，最多上修 1% |
| 未 active | `full_ticks` 清零 |
| 保存 | SOC 改变后由本周期 `soc_save_if_needed()` 保存 |

满电锚点是当前自动上修 SOC 的主要路径。普通静置 OCV 不做上修。

## 8. 低压尾端校准

入口：`soc_select_empty_tail_step()` + `soc_apply_empty_tail()`。

低压尾端用于限制低压附近 SOC 虚高，只做下修。触发条件：

| 条件 | 行为 |
|---|---|
| 充电模式 | `mode == CHG` 时不启用 |
| 电压有效 | 只要求 `soc_voltage_valid()`，不是完整 `soc_calibration_allowed()` |
| sag hold | `soc_sag_hold_blocks_calibration()` 成立时不启用 |
| 电压区间 | `VCellMin <= V0 + 400mV` 才进入尾端逻辑 |
| RELAX 特例 | RELAX 下只有 `VCellMin <= V0 + 50mV` 或 `VCellMin <= V0` 等低端强制区间才会继续；普通 V0+50mV 以上 RELAX 不做尾端 |

当前 target 由 `soc_empty_tail_interpolate(offset_mv, is_relax)` 计算，`offset_mv = VCellMin - OtherElement.u16Soc_V_0`。

关键区间：

| 电压区间 | 计数周期 | target 来源 |
|---|---:|---|
| `VCellMin <= V0` | `5s` | relax/放电插值 target，最低趋近 0 |
| `V0 < VCellMin <= V0 + 50mV` | `10s` | 插值 target |
| `V0 + 50mV` 以上且放电 | `120..600s`，按放电电流增强 | 插值 target |
| `V0 + 50mV` 以上且 RELAX | 不启用 | 无 |
| `V0 + 400mV` 以上 | 不启用 | 无 |

执行方式：

| 行为 | 说明 |
|---|---|
| active 时 | `empty_ticks` 加 1 |
| 达到 step ticks | 如果 `s_soc.soc > target`，按 `1%` 单步下修 |
| 未 active 且 RELAX | `empty_ticks` 逐步回退 |
| 未 active 且非 RELAX | `empty_ticks` 清零 |
| 与静置 OCV 关系 | low-tail active 会调用 `soc_reset_rest_confidence()`，打断长静置 OCV |

## 9. 长静置 OCV 慢下修

入口：`soc_update_rest_timer()` + `soc_apply_long_rest_down_step()`。

普通 200ms 主循环下，长静置 OCV 是慢速下修路径。触发条件：

| 条件 | 行为 |
|---|---|
| 模式 | 必须 `mode == RELAX` |
| 高电压限制 | 当前 `VCellMin >= 3700mV` 时直接 `soc_reset_rest_confidence()`，普通主循环不推进长静置 OCV |
| 未被打断 | low-tail active、sag hold blocked 或本周期已有满电/尾端校准时不推进 |
| 电压稳定 | `soc_rest_voltage_stable()` 成立 |
| 总静置 | `rest_soc_ticks >= 3600s * 5` |
| 稳定静置 | `stable_rest_soc_ticks >= 3600s * 5` |
| 目标方向 | 只接受 `OCV target < 当前 SOC` |
| 目标差值 | 只有 `当前 SOC - OCV target > 3%` 才设置目标 |

电压稳定条件：

| 条件 | 当前阈值 |
|---|---:|
| 通用校准允许 | 电压有效，通用压差 <= `300mV` |
| rest 最大压差 | `VCellMax - VCellMin <= 200mV` |
| sag hold | 不被 sag hold 阻断 |
| 参考漂移 | 当前 `Vmin/Vmax` 相对 `rest_ref_vmin/rest_ref_vmax` 均 <= `30mV` |

执行方式：

| 阶段 | 行为 |
|---|---|
| 建立目标 | 达到 3600s 总静置和 3600s 稳定静置后，查 `VCellMin` 对应 OCV target |
| 目标有效 | 只保存低于当前 SOC 且差值大于 3% 的目标 |
| 慢下修 | `long_rest_down_soc_ticks` 每满 `1800s * 5`，按 `1%` 下修一次 |
| 到达目标 | `rest_down_target == s_soc.soc` 时清掉目标 |
| 不能校准 | 电压不允许或 sag hold 阻断时，本次不下修并清掉 step 计数 |

普通静置 OCV 不会上修 SOC。若 OCV target 高于当前 SOC，只会设置 `rest_ocv_fired`，不修正。

## 10. RTC 静置补偿

入口：`rtc_sleep.c` 周期 RTC 唤醒后累计 `g_stLowPowerRtcStatus.sleep`，再调用 `RtcSleep_PortApplySocRtcRest(g_stLowPowerRtcStatus.sleep)`，最终进入 `SOC_ApplyRtcRelaxationCompensation(rest_seconds, vcell_min, vcell_max)`。

RTC 路径使用“当前 RTC 会话累计秒数”，`soc_apply_rtc_rest_ocv()` 内部只处理相对上次已应用值的增量：

```text
delta_seconds = rest_seconds - s_u32SocRtcRestAppliedSeconds
```

同一段 HICCUP RTC 会话内，正常 RTC 周期唤醒会继续累计；如果异常唤醒退出本段 RTC 会话，下次重新进入时 `g_stLowPowerRtcStatus.sleep` 从 0 开始，`rest_seconds < s_u32SocRtcRestAppliedSeconds` 会触发 `soc_reset_rest_confidence()` 并重置已应用秒数。

RTC 路径与普通主循环的差异：

| 项目 | 普通 `soc_update_rest_timer()` | RTC `soc_apply_rtc_rest_ocv()` |
|---|---|---|
| 时间来源 | 每 200ms tick 推进 | 按 RTC 累计休眠秒数增量推进 |
| `VCellMin >= 3700mV` | 直接清空静置置信度 | 没有这个专门判断 |
| 自耗 | 正常运行积分已按 `15mA` 扣除 | RTC STOP 不额外扣板载自耗 |
| low-tail | 可在主循环中执行 | RTC 路径不执行 low-tail |
| 保存 | 主循环最后 `soc_save_if_needed()` | 只有 SOC 变更时立即 `soc_save_current_snapshot()` |

RTC OCV 触发和下修仍复用长静置目标逻辑：

| 条件 | 当前行为 |
|---|---|
| 总静置 | `rest_soc_ticks` 累计到 3600s |
| 稳定静置 | `stable_rest_soc_ticks` 累计到 3600s |
| 首次稳定参考 | 若 `rest_ref_vmin/vmax` 原来为 0，本次只建立参考，不累计稳定秒数 |
| 目标 | `当前 SOC - OCV target > 3%` 才设置 `rest_down_target` |
| 下修速度 | 目标有效后，每 1800s 最多下修 1% |

已观察到的行为边界：`soc_reset_rest_confidence()` 不清 `rest_ocv_fired`，`rest_ocv_fired` 主要在主循环 `mode != RELAX` 时清零。若 RTC 会话重启但未经历非 RELAX 主循环，`rest_ocv_fired` 可能保留上次状态；后续如要调整，需要同步增加回归测试。

## 11. 人工命令与参数重算

### 11.1 容量参数写入后的重算

`Sci_WrRegs_0x10_OtherElement()` 写到 SOC 参数范围时，会设置 `reload_soc` 和 `reset_soc_capacity`，随后：

1. 调用 `InitData_SOC()` 重新初始化 SOC 模块。
2. 调用 `SOC_RequestCapacityReset()` 重算容量基准。

`SOC_RequestCapacityReset()` 行为：

| 步骤 | 行为 |
|---|---|
| 保留当前 SOC | 先保存 `soc_keep = s_soc.soc` |
| 重算容量基准 | 用新的 `OtherElement.u16Soc_Ah` 重算 `cap_factory_as10` |
| 重置循环 | 用新的 `OtherElement.u16Soc_Cycle_times` 更新 `cycle_x100` |
| 清放电累计 | `dsg_acc_as10 = 0` |
| 刷新满容量 | `soc_refresh_capacity_base()` |
| 恢复 SOC | `soc_set(soc_keep)`，用新容量基准重算 `cap_now_as10` |
| 保存发布 | 立即保存 snapshot 并发布 |

这类操作更像“容量基准校准”，不是电压 OCV 校准。

### 11.2 单次设 SOC

`Sci_WrReg_0x06_SetSocOnce()` 校验写入值 `<= 100` 后调用 `SOC_RequestSetOnce()`。

`SOC_RequestSetOnce(soc)` 直接 `soc_set(soc)`，立即保存 snapshot，并发布到 `g_stCellInfoReport.SocElement`。这是人工覆盖 SOC，不经过电压、压差、sag hold 或步长限制。

### 11.3 SOC 表写入

`Sci_WrRegs_0x10_SocTable()` 当前固定返回 `RS485_ACK_NEG` 和 `RS485_ERROR_CMD_INVALID`。SOC 表读接口仍可返回编译期表，但上位机写表不会改变算法。

### 11.4 升级策略

`PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG = 0`、`PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT = 0`，当前默认升级不重置 SOC 配置和 SOC snapshot。

若以后打开：

| 开关 | 行为 |
|---|---|
| `UPGRADE_PARAM_RESET_SOC_CONFIG` | 把 `OtherElement` 中 SOC 配置恢复默认值 |
| `UPGRADE_PARAM_RESET_SOC_SNAPSHOT` | 调用 `SOC_ResetStoredSnapshotToDefault()`，写入默认启动 SOC 相关 snapshot |

## 12. 保存与发布

SOC snapshot 保存内容来自 `soc_save()`，关键字段：

| 字段 | 来源 |
|---|---|
| `u16SocNow` | `s_soc.soc` |
| `u32CycleTimes` | `s_soc.cycle_x100` |
| `u32CapNow` | `s_soc.cap_now_as10` |
| `u32CapFull` | `s_soc.cap_full_as10` |
| `u32LearnPassedAs10` | `s_soc.dsg_acc_as10` |
| `u16DsgSocInt` | `dsg_acc_as10` 折算为额定容量百分比 |
| `u16Flags` | 当前只保存 rebound hold flag |

`soc_save_if_needed()` 只比较 `soc`、`cycle_x100`、`cap_full_as10`、`snapshot_flags`。这意味着每次 SOC 变化 1%、循环/满容量变化或 rebound flag 变化都会触发保存；单纯 `cap_now_as10` 小幅积分变化不一定立即保存，除非带来 SOC 或循环相关字段变化。

发布入口 `SOC_PublishReportData()` 会把当前内部状态写入：

| 对外字段 | 来源 |
|---|---|
| `u16Soc` | `s_soc.soc` |
| `u16Soh` | `s_soc.soh` |
| `u16CapacityNow` | `cap_now_as10` 换算为 Ah * 100 |
| `u16CapacityFull` | `cap_full_as10` 换算为 Ah * 100 |
| `u16CapacityFactory` | `cap_factory_as10` 换算为 Ah * 100 |
| `u16Cycle_times` | `cycle_x100 / 100` |

## 13. 已删除或无效的校准路径

| 路径 | 当前状态 |
|---|---|
| 运行时写 SOC 表 | 已无效，写接口固定返回错误 |
| `OtherElement.u16Soc_TableSelect` 运行时切表 | 不参与当前算法，只保留协议兼容 |
| 手动 OCV refresh | 已删除 |
| short-rest deferred OCV | 已删除 |
| mid-tail | 已删除 |
| 显示 SOC 平滑层 | 已删除，对外直接发布 `s_soc.soc` |
| RTC 自耗额外扣减 | 已删除，RTC 路径只推进静置 OCV 计数和慢下修 |

## 14. 排查建议

排查 SOC 校准是否发生时，优先观察这些输入和结果：

| 观察项 | 判断点 |
|---|---|
| `g_stCellInfoReport.u16VCellMin/u16VCellMax` | 电压是否有效、是否满足满电/尾端/静置条件 |
| `g_stCellInfoReport.u16Ichg/u16IDischg` | signed 净电流是否进入 `CHG/DSG/RELAX` |
| `OtherElement.u16Soc_V_100/u16Soc_V_0` | 满电和低压尾端基准是否正确 |
| `g_stCellInfoReport.SocElement.u16Soc` | 当前已发布 SOC，等于内部 `s_soc.soc` |
| `g_stCellInfoReport.SocElement.u16CapacityNow` | 判断容量积分是否在变化 |
| `g_stLowPowerRtcStatus.sleep/cycles` | RTC 路径是否在同一会话内继续累计 |

常见判断：

| 现象 | 优先判断 |
|---|---|
| 满电不上 100 | 看 `VCellMax > 4180mV`、`VCellMin >= V100 - 80mV`、压差 <= `120mV`、是否连续 15s |
| 静置不下修 | 看是否 `VCellMin >= 3700mV`、是否 low-tail/sag hold 打断、是否稳定满 3600s |
| RTC 不校准 | 看是否同一 RTC 会话累计、稳定参考是否刚建立、目标是否低于当前 SOC 且差值 > 3% |
| 低压快降 | 看 `VCellMin` 是否低于 `V0 + 400mV`，特别是 `V0 + 50mV` 和 `V0` 附近的强制尾端 |
| 人工设 SOC 后立即变化 | `SetSocOnce` 是人工覆盖，不受电压门槛和 1% 自动步长约束 |

## 15. 建议回归

本文未修改源码。后续如改 SOC 校准策略，至少执行：

| 验证 | 目的 |
|---|---|
| `python tools/soc_replay_test.py` | Python 模型回放，覆盖长静置、低压尾端、RTC、满电等策略 |
| `python tools/run_soc_host_c_test.py` | 编译并运行真实 `SOC.c/SocEnhance.c` host C 测试 |
| `git diff --check` | 检查空白和补丁格式 |
| Keil `FD_Release` 最小编译 | 出货构建口径 |
| 真板充放电 + RTC STOP | 确认电流方向、电压采样、STOP 唤醒和 SOC 发布 |


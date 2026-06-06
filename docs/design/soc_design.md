# SOC 模块设计与源码审查

文档状态：已按源码验证
源码验证日期：2026-06-04
适用范围：当前 `103 + 309` BMS App SOC 模块
权威性说明：本文是 SOC 模块当前唯一活跃权威入口；历史 review/devlog 只作追溯，不作为当前行为依据。

## 0. 合并范围与阅读规则

本次按当前源码重新核对 SOC 主链路、配置、校准、休眠、存储、调试和测试边界，并把以下 SOC 相关文档的有效内容合并到本文：

| 原文档 | 合并状态 | 当前处理 |
|---|---|---|
| `docs/review/soc_current_logic_2026-06-02.md` | 已合并 | 保留为历史参考 |
| `docs/review/soc_rest_fast_drop_analysis_2026-06-03.md` | 已合并 | 快降排查内容并入本文 low-tail、静置 OCV、发布口径和风险章节 |
| `docs/review/soc_simplification_candidates_2026-06-02.md` | 已合并 | 简化记录并入本文已处理问题和后续风险 |
| `docs/review/soc_test_script_usage_2026-06-03.md` | 已合并 | 测试脚本边界并入本文回归入口 |
| `docs/devlog/CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md` | 部分合并 | 只合并 SOC 常用控制入口；CAN 老化广播仍属于协议/开发历史 |
| `docs/design/soc_design.md` | 当前权威 | 后续 SOC 当前逻辑只维护本文 |

阅读规则：

1. 判断当前 SOC 行为时，以本文和当前源码为准。
2. 旧 review/devlog 可以追溯原因，但不能直接作为当前算法事实。
3. 如本文与源码冲突，以源码为准，并优先修本文。
4. 本文只描述当前已实现行为；不把未确认优化建议写成需求。

主要参考源码：

- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/SocEnhance.h`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/LowPowerSleep.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/SystemDebug.c`
- `103 + 309/Project/Source/System_Monitor.c`
- `103 + 309/Project/Source/conf/Project_Config.h`

源码证据索引：

| 主题 | 源码入口 |
|---|---|
| 启动初始化 | `AppInit.c::AppInit_InitDevice()` -> `InitData_SOC()` |
| 200ms 调度 | `System_Init.c::TIM3_IRQHandler()`、`SysTime_Post10msTick()`、`SysTime_Take200msTaskPeriod()` |
| AFE/SOC 主链 | `Runtime.c::Runtime_RunFrontTasks()`、`DataDeal.c::App_AFEGet()`、`SOC.c::App_SOC()` |
| Type-C 等效电流 | `SOC.c::SOC_GetTypeCBatEquivCurrentA10()`、`SOC_GetNetCurrentMilliAmp()` |
| 核心状态机 | `SocEnhance.c::SOC_IntEnhance_Ctrl(net_current_ma)` |
| 积分与自耗 | `SocEnhance.c::soc_integrate()` |
| 满电/低端/静置校准 | `soc_apply_full_empty()`、`soc_low_tail_config()`、`soc_update_rest_timer()` |
| RTC STOP 补偿 | `rtc_sleep_port.c::RtcSleep_PortApplySocRtcRest()`、`SocEnhance.c::SOC_ApplyRtcRelaxationCompensation()` |
| Flash snapshot | `SocEnhance.c::soc_save()`、`soc_load_or_default()`、`Flash.c::StorageFlash_LoadSocData()`、`StorageFlash_SaveSocData()` |
| 对外发布 | `soc_publish()`、`SOC_PublishReportData()`、`LedBar.c`、`Can_HDX.c`、`CanFeidaoFrames.c` |
| 上位机控制 | `Sci_Upper.c::Sci_WrRegs_0x10_SocElement()`、`Sci_WrReg_0x06_SetSocOnce()` |
| Debug Watch | `SocEnhance.h::SOC_DEBUG_WATCH`、`SystemDebug.c::SystemDebug_UpdateSnapshot()` |

本次文档合并后已执行验证：

- `git diff --check`：通过。
- `python3 tools/soc_replay_test.py`：当前 41 项通过。
- `python3 tools/project_check.py --quiet`：当前 checkout 为历史基线失败 `103 OK / 1 warning / 13 errors`；失败项为既有缺文件、非 UTF-8、历史文档引用和历史审计门禁。

未验证事项：

- `python3 tools/run_soc_host_c_test.py`：当前被 `Project_BuildGuard.h` release/debug 宏门禁阻断，错误为 `Debug watch, system debug and IRQ debug must stay disabled for release build profile`；本次文档合并未修改该工具或构建宏。
- `python3 tools/soc_visual_report.py --html build/host_tests/soc_visual_report_check.html --csv build/host_tests/soc_visual_trace_check.csv`：本次未执行。
- 未执行 Keil `FD_Release` 编译。
- 未做真板充放电、RTC STOP 功耗、CAN/Modbus 在线读取、Keil watch 实测。

## 1. 本次审查结论

1. SOC 主估算仍是容量积分，OCV、满电、低压尾端、静置和 RTC 休眠补偿都是校准/约束层。
2. 内部真实 SOC 是 `s_soc.soc`；对外发布到 CAN、Modbus、LedBar 的 SOC 现在直接等于 `s_soc.soc`，不再经过 `display_soc` 平滑层。
3. 正常运行 RELAX 模式下，板载自耗已经计入 SOC：`soc_integrate()` 内部按 RELAX 计算 `-PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA`，随后按放电积分累计容量损耗。
4. RTC STOP 补偿路径当前不再额外扣自耗，只按休眠秒数推进静置 OCV 相关计数和下修；这是为了避免把 RTC 低功耗期间的极低自耗重复或过度计入。
5. 当前只保留 low-tail 表 `s_empty_tail_table`。mid-tail 表、旧 `#if 0` tail 对照表和 mid-tail debug 字段已按确认删除。
6. `SOC_IntEnhance_Ctrl(net_current_ma)` 当前按一条直线表达核心顺序：方向、signed 积分、sag hold、low-tail/full、静置、保存、发布。
7. 已删除无消费者或误导性字段/路径：`SOC_Enhance_Element`、runtime SOC table、手动 OCV、mid-tail、short-rest deferred OCV、`u16_SOC_CycleT_Limit`、`u8_SOC_OCV_Cali`、`SOC_WATCH_BLOCK_REASON/u8LastBlockReason` 以及 debug monitor 中的伪造派生字段。
8. 当前代码未发现剩余必须立即修复的 SOC 协议兼容问题；硬件验证仍是后续风险边界。

## 2. 模块边界

| 文件 | 当前职责 |
|---|---|
| `SOC.c` | 顶层调度、配置装载、Type-C 输出折算为电池侧等效放电电流、按 AFE sample seq 触发核心算法 |
| `SocEnhance.c` | SOC 核心算法：容量积分、SOH、OCV 表、满电锚点、tail、静置、RTC 休眠补偿、Flash snapshot、对外发布 |
| `SocEnhance.h` | SOC 对外结构、调试 watch、请求 API |
| `DataDeal.c` | AFE 数据加载后递增 `AfeCurrent_GetSeq()`，驱动 `App_SOC()` 只处理新样本 |
| `Sci_Upper.c` | Modbus 写容量/一次 SOC，通过 `SOC_Request*()` 进入 SOC 模块；SOC 表写入固定返回错误 |
| `rtc_sleep_port.c` | HICCUP STOP 周期唤醒后调用 `SOC_ApplyRtcRelaxationCompensation()` |
| `LowPowerSleep.c` | reset sleep/STOP 前调用 `SOC_SaveSnapshotBeforeSleep()` |
| `Flash.c` | A/B journal 方式保存和恢复 SOC snapshot |
| `LedBar.c` | 读取已发布 SOC，保存/加载睡眠快显 SOC |
| `Can_HDX.c`、`CanFeidaoFrames.c` | 读取 `g_stCellInfoReport.SocElement` 作为 CAN 对外口径 |

SOC 模块没有引入 HAL、RTOS、malloc，也没有新增协议字段。

## 3. 主数据流

```text
Runtime_RunOnce()
  APP_LedBar()
  App_AFEGet()
    DataLoad_CellVolt()
    DataLoad_Current()
    AFE sample seq +1
    App_SOC()
      AfeCurrent_GetSeq()
      SOC_GetNetCurrentMilliAmp()
      SOC_IntEnhance_Ctrl(net_current_ma)
        soc_integrate(mode, net_current_ma)
        low-tail/full/rest
        soc_save_if_needed()
        soc_publish(0, net_current_ma)
```

关键边界：

- `App_SOC()` 只在 AFE sample seq 变化时执行核心算法；没有新样本时只重新发布当前数据。
- `SOC_GetNetCurrentMilliAmp()` 会把 Type-C 输出电流按 `TYPEC_OUT_VOLTAGE_MV`、电池总压和 DCDC 效率折算成电池侧等效放电电流，并形成 signed mA 净电流。
- 发布路径是 `soc_publish()` -> `SOC_PublishReportData()`。

## 4. 配置事实

配置原则：`Project_Config.h` 只保留产品调试、现场体验或确实需要编译期切换的 SOC 参数；算法内部常量不要继续扩展为 `PROJECT_CFG_*` 宏。已删除默认无效或仅预留的 SOC table 复位、校准故障阻断、empty-tail soft target/tick 调参开关。

| 配置 | 当前值 | 影响 |
|---|---:|---|
| `PROJECT_CFG_BAT_CHEMISTRY` | `0` | 当前编译使用三元锂 `SocTable_TernaryLi` |
| `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` | `30` | 正常运行积分中的板载自耗 |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | `1` | 自动校准单次最多 1% |
| `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS` | `15` | 普通满电确认时间 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS` | `5` | 快速满电确认时间 |
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | `1800` | 静置 OCV 基础门槛 |
| `PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS` | `1800` | 长静置下修周期 |
| `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` | `30` | 大电流放电后回弹保护 |
| `PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV` | `400` | low-tail 最高启动区间 |

已删除：

- `PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS`
- `PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS`
- `PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS`
- `PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV`
- `PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV`

删除原因：当前自动校准最大步长为 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT = 1`，用户确认不再需要额外 `display_soc` 平滑层；对外 SOC 直接发布内部真实估算值。

## 5. 状态与输出口径

### 5.1 内部状态

`SOC_STATE s_soc` 是模块私有状态，关键字段如下。

| 字段 | 含义 |
|---|---|
| `cap_factory_as10` | 额定容量，内部容量积分单位 |
| `cap_full_as10` | SOH 修正后的满容量 |
| `cap_now_as10` | 当前剩余容量 |
| `cycle_x100` | 循环次数扩大 100 倍 |
| `dsg_acc_as10` | 放电累计，用于循环计数 |
| `rem_mams` | 200ms 积分余量，避免小电流损失 |
| `soc` | 内部真实 SOC |
| `mode` | `RELAX/CHG/DSG` |
| `full_ticks/empty_ticks` | 满电确认和 low-tail 计数 |
| `rest_soc_ticks/stable_rest_soc_ticks/long_rest_down_soc_ticks` | 静置 OCV 慢下修计数，单位为 200ms SOC tick |
| `sag_hold_ticks` | 电压 sag/rebound holdoff |
| `rest_down_valid/rest_down_target` | 长静置慢下修目标，只接受低于当前 SOC 的 OCV 目标 |
| `snapshot_flags` | 当前只保留 rebound hold 标志 |

### 5.2 外部数据边界

`SOC_Enhance_Element` 中间层已删除。当前边界：

| 数据角色 | 当前来源/去向 |
|---|---|
| 参数 | 直接读取 `OtherElement.u16Soc_Ah/u16Soc_Cycle_times/u16Soc_V_100/u16Soc_V_0` |
| 电压 | 直接读取 `g_stCellInfoReport.u16VCellMax/u16VCellMin` |
| 电流 | `SOC_IntEnhance_Ctrl(net_current_ma)` 入参，signed mA，充电为正、放电为负 |
| 命令 | `SOC_RequestCapacityReset()`、`SOC_RequestSetOnce()` 直接执行 |
| 发布 | `SOC_PublishReportData()` 直接写 `g_stCellInfoReport.SocElement` |

### 5.3 发布口径

| 对外字段 | 当前值来源 |
|---|---|
| `g_stCellInfoReport.SocElement.u16Soc` | `s_soc.soc` |
| `g_stCellInfoReport.SocElement.u16Soh` | cycle-based SOH |
| `u16CapacityNow` | `cap_now_as10` 换算为 Ah * 100 |
| `u16CapacityFull` | `cap_full_as10` 换算为 Ah * 100 |
| `u16CapacityFactory` | `cap_factory_as10` 换算为 Ah * 100 |
| `u16Cycle_times` | `cycle_x100 / 100` |

当前已取消内部/显示双口径；调试和产品判断统一看 `s_soc.soc`、`g_dbg_soc_watch.u8InternalSoc` 或已发布的 `g_stCellInfoReport.SocElement.u16Soc`。

## 6. 核心状态机顺序

`SOC_IntEnhance_Ctrl(net_current_ma)` 当前顺序如下，后续重构必须保持：

1. `soc_direction(net_current_ma)` 判断 `RELAX/CHG/DSG`。
2. `soc_integrate(mode, net_current_ma)` 做容量积分和板端自耗积分。
3. `soc_update_sag_hold(mode, net_current_ma)` 更新大电流回弹保护。
4. `soc_low_tail_config(mode, net_current_ma, &step)` 计算本周期 low-tail 状态，并刷新 debug watch。
5. `soc_apply_full_empty()` 处理满电锚点或 low-tail。
6. 如果没有 low-tail、没有校准、没有 sag hold，推进 `soc_update_rest_timer()`；否则在 low-tail 或 sag hold 时清空静置 confidence。
7. `soc_save_if_needed()` 按保存 mark 判断是否写 Flash。
8. `soc_publish(0U, net_current_ma)` 直接把内部 SOC 和容量字段发布到对外结构。

本顺序保证低端安全和满电锚点优先于静置 OCV。

## 7. 容量积分与自耗

### 7.1 方向判断

| 模式 | 条件 |
|---|---|
| `CHG` | `net_current_ma >= 200mA` |
| `DSG` | `net_current_ma <= -200mA` |
| `RELAX` | signed 净电流未达到 active 门槛 |

### 7.2 自耗口径

正常运行链路中，自耗已经算进 SOC：

```text
net_current_ma = (u16Ichg - (u16IDischg + TypeC等效放电A10)) * 100mA
integrate_current_ma = net_current_ma - PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA
```

因此，普通无外部电流运行时，`30mA` 会以放电积分形式逐步减少 `cap_now_as10`。对 27Ah 电池，约 9 小时才接近 1% SOC，不能解释秒级或分钟级快降。

### 7.3 Type-C 输出

Type-C 输出电流不直接使用 ADC mA 作为电池放电电流，而是按输出功率折算：

```text
电池侧等效放电电流 = TypeC输出电流 * TypeC输出电压 / 电池总压 / DCDC效率
```

该等效电流只在进入 SOC 核心前并入 `u16IDischg` 参与 signed 净电流计算；协议字段 `g_stCellInfoReport.u16Ichg/u16IDischg` 仍保持 unsigned `A * 10` 兼容口径。

## 8. 校准策略

### 8.1 启动

入口：`soc_param_lib_init()` -> `soc_load_or_default()`。

| 分支 | 条件 | 行为 |
|---|---|---|
| snapshot | Flash V2/V1 snapshot 有效且 SOC 合法 | 恢复容量、循环、放电累计、rebound hold |
| startup OCV | snapshot 无效且电压校准允许 | 用 `VCellMin` 查 OCV 表 |
| default | snapshot 无效且电压校准不允许 | 使用 `SOC_DEFAULT_STARTUP_PERCENT = 60` |

启动时 `soc_param_lib_init()` 直接从 `OtherElement.u16Soc_Ah/u16Soc_Cycle_times` 初始化容量和循环基线；满电/空电阈值运行时直接读取 `OtherElement.u16Soc_V_100/u16Soc_V_0`。当前化学体系由 `PROJECT_CFG_BAT_CHEMISTRY` 编译期选择；`OtherElement.u16Soc_TableSelect` 保留为协议兼容字段，不参与算法选表。

### 8.2 满电锚点

满电锚点只在非放电模式执行。满足电压、压差和持续时间后，每次最多上修 1%，直到 100%，并置位 `full_anchor`。充电积分在未锚定前最高压到 99%，避免未确认满电时直接发布 100%。

### 8.3 Low-Tail

low-tail 用 `s_empty_tail_table` 约束 V0 附近低端 SOC 虚高。

当前活动表说明：

- offset 是相对 `V0` 的 mV。
- target 是不同负载档位下允许的最高 SOC。
- ticks 是 200ms SOC tick，每达到一次最多下修 1%。
- 当前活动 ticks 全部为 `DELAY_SOC_TEST = 5 * 60`，即约 `60s/1%`。

生效条件：

- 当前不是 `CHG`。
- 电压有效。
- `VCellMin <= V0 + PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV`。
- sag hold 未阻塞。

low-tail 允许在 `RELAX` 下生效；这是无放电静置快降的优先排查点。

### 8.4 Mid-Tail

已按确认删除。

- 源码不再定义 `s_mid_tail_table`。
- `SOC_STATE` 不再保留 `mid_ticks`。
- `SOC_DEBUG_WATCH` 不再导出 `u8MidTailActive/u16MidTailTarget/u16MidTailTicks/u16MidTicks`。
- Python replay 不再模拟或校验 mid-tail 表。

当前中段电压不会触发独立 mid-tail 下修；低端虚高只由 low-tail 和长静置慢下修约束。

### 8.5 Sag Hold

大电流放电后，`soc_update_sag_hold()` 置 `sag_hold_ticks = 30s * 5 tick/s` 并保存 rebound hold flag。只要 hold 未过期且电压仍高于 `V0 + 50mV`，OCV/tail 校准会被阻塞，避免电压跌落或回弹造成误校准。低到 tail 安全区时仍允许 low-tail。

### 8.6 静置 OCV

普通运行 RELAX 下，静置 OCV 是慢路径：

1. `soc_update_rest_timer()` 只在 `RELAX` 且未被 tail/校准/sag hold 打断时推进。
2. 电压稳定要求 `Vmin/Vmax` 相对 rest reference 都在 `30mV` 内。
3. `rest_soc_ticks` 和 `stable_rest_soc_ticks` 都达到 `1800s` 后，才设置 `rest_down_target`。
4. `rest_down_target` 只接受低于当前 SOC 的 OCV 目标；不做静置上修。
5. 目标有效后继续按 `1800s/1%` 慢速下修，因此连续稳定静置首次下修约在 60 分钟量级。

普通静置 OCV 不做静置上修；上修主要由满电锚点承担。

### 8.7 Deferred OCV

已删除自动 deferred OCV 路径。

- 短静置不再锁存 OCV 目标。
- active 充放电阶段不再消化静置期间锁存的隐藏目标。
- 当前只保留长静置慢下修 `rest_down_target`。

### 8.8 RTC STOP 补偿

HICCUP STOP 周期唤醒时，`rtc_sleep.c` 累计休眠秒数并经 `rtc_sleep_port.c` 调用：

```c
SOC_ApplyRtcRelaxationCompensation(rest_seconds, vcell_min, vcell_max);
```

当前 RTC 补偿只做：

- 按新增量秒数推进 `rest_soc_ticks/stable_rest_soc_ticks`。
- 只有长静置条件满足后才设置 `rest_down_target`。
- 目标有效后按 `1800s/1%` 慢速下修。
- 如果 SOC 改变，则保存 snapshot 并发布。

当前 RTC 补偿不做：

- 不额外扣 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA`。
- 不执行 low-tail。
- 不锁存短静置 deferred OCV 目标，也不在后续 active 放电中消化隐藏目标。
- 不改变 reset sleep 的早期快显逻辑。

### 8.9 命令校准

外部命令通过请求 API 进入：

| API | 行为 |
|---|---|
| `SOC_RequestCapacityReset()` | 设置 flag 2，重算容量基准并保存 |
| `SOC_RequestSetOnce(UINT8 soc)` | 设置 flag 3，把内部 SOC 设置到指定值并保存 |

命令路径使用 `soc_publish(1U)` 立即发布最新 SOC；当前没有单独的显示平滑层。

上位机/协议入口：

| 入口 | 当前行为 |
|---|---|
| Modbus 多寄存器写 SOC 参数区 | `Sci_WrRegs_0x10_SocElement()` 转入 `OtherElement` 写入；覆盖 SOC 参数范围时先 `InitData_SOC()`，再 `SOC_RequestCapacityReset()` |
| 单寄存器 `SetSocOnce` | `Sci_WrReg_0x06_SetSocOnce()` 校验 `0..100` 后调用 `SOC_RequestSetOnce()` |
| CAN App 状态查询 | `Can_HDX.c` 读取 `g_stCellInfoReport.SocElement.u16Soc/u16Soh` |
| 飞道周期 SOC 帧 | `CanFeidaoFrames.c` 读取已发布 SOC |
| LedBar | `LedBar.c` 读取 `g_stCellInfoReport.SocElement.u16Soc` 并限幅显示 |

## 9. 存储与休眠

Flash snapshot 采用 `STORAGE_FLASH_SOC_DATA` V2：

- `u16SocNow`
- `u16DsgSocInt`
- `u32CycleTimes`
- `u32CapNow`
- `u32CapFull`
- `u32LearnPassedAs10`
- `u16Flags`

保存触发现在只比较真实参与持久化判重的字段：

- `soc`
- `cycle_x100`
- `cap_full_as10`
- `snapshot_flags`

`LowPowerSleep_SaveCoreState()` 在休眠前调用 `SOC_SaveSnapshotBeforeSleep()`；`LowPowerSleep_SaveResetState()` 还会调用 `LedBar_SaveSleepSoc()` 保存睡眠快显 SOC。

reset sleep 和 HICCUP STOP 的 SOC 口径不同：

| 低功耗路径 | SOC 行为 |
|---|---|
| `HICCUP_MODE` RTC STOP | 周期唤醒时累计秒数并调用 RTC SOC 补偿 |
| `NORMAL/DEEP` reset sleep | 睡前保存 snapshot 和 LedBar 快显 SOC；已确认不增加 reset sleep 秒数 SOC 补偿 |

## 10. 调试口径

`g_dbg_soc_watch` 当前保留有用实时字段：

- 内部 SOC：`u8InternalSoc`
- 当前模式：`u8Mode/u8LastMode`
- 容量：`u32CapFactoryAs10/u32CapFullAs10/u32CapNowAs10`
- tail 状态：`u8LowTailActive/u16EmptyTailTarget/u16EmptyTailTicks`
- 静置状态：`u32RestTicks/u32StableRestTicks/u32LongRestDownTicks/u8RestVoltageStable/u8RestDownValid/u8RestDownTarget`
- sag：`u16SagHoldTicks/u8SagHoldBlocksCalibration`
- 最近校准：`u8LastCalibSource/u8LastSocBefore/u8LastSocAfter`

已删除：

- `SOC_WATCH_BLOCK_REASON`
- `u8LastBlockReason`
- debug monitor 中不真实或易误导的 `cal_allowed/sag_blocked/rest_stable/low_tail/ocv_target/last_calib_soc/display_ticks` 派生字段，以及 mid-tail 兼容字段

排查 SOC 快降时优先看：

1. `u8LastCalibSource`
2. `u8LowTailActive`
3. `u8InternalSoc`
4. `u16VCellMin` 与 `u16_SOC_0_Vol` 的关系
5. `u32RestTicks/u32LongRestDownTicks`

## 11. 本次源码 review 已处理的问题

| 项目 | 原问题 | 处理结果 |
|---|---|---|
| tail 主流程 | 旧实现混有 low/mid tail、历史表和兼容计数，阅读成本高 | 保留 low-tail 主流程，删除 mid-tail 表、计数和 debug 字段 |
| RTC 自耗 | RTC 休眠补偿额外扣板载自耗，与“RTC 低功耗自耗可忽略”的需求不一致 | 删除 RTC 秒级自耗扣减，仅保留正常运行自耗积分 |
| 无用字段 | `u16_SOC_CycleT_Limit/u8_SOC_OCV_Cali/u8LastBlockReason` 无有效消费者或已误导 | 删除字段、调用和打印 |
| debug monitor | 多个 debug 字段固定为 0 或伪造来源 | 删除，保留真实内部计数和 display tick |
| host test stub | host 工具缺 `ADC_GetVbatMilliVolt()`、`AfeCurrent_GetSeq()` stub | 补齐 `soc_host_c_test.c` 和 `soc_host_visual_trace.c` 桩函数，host 测试/trace 可按当前接口链接运行 |
| replay 表解析 | Python replay 之前可能解析到 `#if 0` 对照表 | 改为解析活动 C 源码和 `DELAY_SOC_TEST` 宏 |
| mid-tail 运行复杂度 | V0 上方 mid-tail 在 RELAX 下也会下修，体验上容易与静置快降混淆 | 删除 mid-tail 源码、测试模型和 debug 字段 |
| deferred OCV 隐藏目标 | 短静置锁存目标后在 active 放电消化，行为不直观 | 删除自动 deferred OCV，只保留长静置慢速下修 |
| runtime table | 上位机写 SOC 表长期关闭，仍保留宏、运行时数组和 EEPROM 分支 | 删除 runtime table 宏、数组、写入分支和 EEPROM 默认表装载 |
| 手动 OCV | flag=1 手动 OCV 与“只保留长静置慢下修”不一致 | 删除 `SOC_RequestManualOcvRefresh()` 和 flag=1 处理 |
| display_soc 平滑 | 对外 SOC 需要维护内部/显示两套状态，但自动校准最大只有 1%，额外平滑收益很低 | 删除 `display_soc/display_ticks/display_ready`、显示平滑宏、Fixed/Zero 覆盖逻辑；对外直接发布 `s_soc.soc` |
| 函数粒度 | 删除历史功能后仍保留一层转发、一次性小 helper 和 mid-tail 时代泛化查表 | 合并 `soc_publish` 包装层、积分模式判断、长静置 OCV 单步、重放电判断和 low-tail 泛化 helper；详见 `docs/review/soc_function_granularity_review_2026-06-04.md` |
| 函数粒度二次收敛 | 前一轮后仍有单纯默认值 helper、保存判重包装和未被当前 tail 表使用的 disabled 分支 | 合并 `soc_integrate_current_ma()`、`soc_empty_mv()`、`soc_full_mv()`、`soc_save_mark_changed()`，删除 `SOC_TAIL_TARGET_DISABLED` 分支 |

## 12. 风险与后续建议

| 风险 | 当前判断 | 建议 |
|---|---|---|
| 当前 low-tail 测试表 tick 很快 | 活动表 `5 tick = 1s/1%`，用户正在测试；可能造成低压场景显示快降 | 不在本次改表。上板时用 `u8LastCalibSource` 和 low-tail active 字段确认体验 |
| reset sleep 无 RTC 秒数补偿 | 已按确认不做 reset sleep 秒数 SOC 补偿 | 文档和测试只验证睡前 snapshot/快显 SOC，不再把 reset sleep 补偿列为待确认功能 |
| 未做 Keil/真板验证 | host/replay 已过，但硬件时序、电流方向、STOP 功耗仍未确认 | 后续跑 Keil `FD_Release`、Modbus/CAN、充放电、RTC STOP |
| 运行时 SOC 表已删除 | 上位机写表固定返回错误，算法只使用编译期化学体系表 | 上位机若仍暴露写表入口，应提示该功能无效 |

## 13. 回归入口

| 类别 | 命令/方法 |
|---|---|
| Python 模型回放 | `python3 tools/soc_replay_test.py` |
| C host 测试 | `python3 tools/run_soc_host_c_test.py` |
| C visual trace | `python3 tools/soc_visual_report.py --html build/host_tests/soc_visual_report_check.html --csv build/host_tests/soc_visual_trace_check.csv` |
| 静态空白检查 | `git diff --check` |
| 仓库检查 | `python3 tools/project_check.py --quiet` |
| 量产构建 | `./tools/bms_dev_workflow.ps1 -Mode build -Target FD_Release` |
| 上板协议 | `COM4/19200/slave=1` 读 `0xD000/0xD300`，CAN 抓 `0x14F80200+index` |

测试结论表达必须分层：

1. `soc_replay_test.py` 只能说明 Python 模型与预期表一致，不等价于真实 C 固件验证。
2. `run_soc_host_c_test.py` 编译并运行真实 `SOC.c/SocEnhance.c`，但外设、Flash、ADC、RTC、CAN 都是 host stub。
3. `project_check.py` 是仓库静态门禁，不是 SOC 功能测试；当前 checkout 若存在历史失败，需要按基线解释。
4. 出货判断必须补 Keil `FD_Release`、真板充放电、RTC STOP、CAN/Modbus 在线读取和 Keil Watch。

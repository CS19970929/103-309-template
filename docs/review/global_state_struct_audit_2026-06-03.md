# 全局状态结构体化审计与方案

文档状态：部分验证

更新时间：2026-06-03

参考源码：

- `103 + 309/Project/Source/ADC.c/.h`
- `103 + 309/Project/Source/DataDeal.c/.h`
- `103 + 309/Project/Source/Sci_Upper.c/.h`
- `103 + 309/Project/Source/System_Init.c/.h`
- `103 + 309/Project/Source/SystemDebug.c/.h`
- `103 + 309/Project/Source/rtc_sleep.c/.h`
- `103 + 309/Project/Source/RTC.c/.h`
- `103 + 309/Project/Source/SleepDeal.c/.h`
- `103 + 309/Project/Source/Fault.c/.h`
- `103 + 309/Project/Source/SocEnhance.c/.h`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/LogRecord.c`
- `103 + 309/Project/Source/Flash.c/.h`
- `103 + 309/Project/Source/conf/conf.c/.h`

## 结论

当前 BMS APP 项目仍然存在不少单独的全局变量、文件级静态变量和函数内静态变量。它们不是全部都应该机械合并到一个总结构体里。

推荐原则：

1. 模块运行状态按模块收口：例如 `s_adc`、`s_sci`、`s_rtc`、`s_fault`、`s_sys`。
2. 对外协议镜像暂时保留：例如 `g_stCellInfoReport`、`OtherElement`、`PRT_E2ROMParas`、`System_ErrFlag`，先不要直接改名或挪位置。
3. Keil 统一观察可以做一个全局入口：优先扩展已有 `g_dbg`，必要时新增 `g_app` 或 `g_watch`，它只做快照或指针聚合，不做所有模块的唯一可写控制源。
4. 常量表不纳入运行状态结构体：例如 SOC 表、NTC 表、AFE 档位表、CAN dispatch 表。
5. 新增变量命名应简洁，不再新增 `s_u16XxxLongName` 这类零散变量。

不建议把所有结构体整合成一个巨大的全局可写结构体作为唯一控制源。原因是：模块耦合会变重，ISR 与主循环并发边界会变模糊，Flash/协议镜像/调试快照容易互相污染，后续定位问题反而更难。Keil 观察的目标可以通过 `g_dbg` 或 `g_watch` 达成，不必牺牲模块边界。

## 当前已结构体化较好的模块

| 模块 | 当前状态 | 判断 |
|---|---|---|
| 低功耗 RTC | `g_stLowPowerRtcStatus` 已收口 idle/block/sleep/last/cycles/vlow/force/comm | 保持，后续只扩展字段，不再新增低功耗散变量 |
| CAN | `s_tx`、`s_runtime`、`s_app` 已模块化 | 可后续统一命名，但不急于合并 |
| LedBar | `s_ledbar` 已覆盖主要显示和按键状态 | 剩余 `key_release_wakeup` 应收口进模块状态或调试快照 |
| SOC | `s_soc`、`s_saved_soc`、`s_soc_debug_watch` 已模块化 | 剩余 RTC rest 累计和 rest stable 标志可再收口 |
| 工厂老化 | `s_factory_aging` 已模块化 | 保持 |
| LogRecord | `s_log_record` 已模块化 | `su32_Interval_S_Tcnt` 仍是外部变量，需确认是否保留 |
| SystemDebug | `g_dbg` 已是统一调试快照 | 推荐作为 Keil 观察主入口继续扩展 |

## 单独变量候选清单

| 模块 | 代表变量 | 建议处理 |
|---|---|---|
| ADC | `ADC_RUNTIME s_adc` | 已收口 DMA raw、滤波、result、Vbat、Type-C 和调度计数；外部通过 `ADC_GetResult()`、`ADC_GetRaw()`、`ADC_GetVbatMilliVolt()`、`ADC_GetTypeCOutCurrentMilliAmp()` 读取 |
| DataDeal | `DATA_RUNTIME s_data`、`g_u16CalibCoefK[]`、`g_i16CalibCoefB[]`、`g_u32CS_Res_AFE` | AFE 电流零点、AFE 通信监控计数、sleep delay 和 sample seq 已进 `s_data`；校准参数/采样电阻仍按协议和 Flash 边界保留 |
| DataDeal 函数内静态变量 | `su16_Sleep_DelayT1/T2/T3`、`mos_state`、`state_fuse`、`rong_fuse*`、`delay_cnt` | 建议按功能拆进 `DATA_RUNTIME` 子结构，减少隐藏状态 |
| Sci_Upper | `g_stCurrentMsgPtr_SCI1/2/3`、`gu16_CommuErrCnt_SCI1/2/3`、`gu8_TxEnable_SCI1/2/3`、`gu8_TxFinishFlag_SCI1/2/3`、`g_u8SCITxBuff[]`、`u8FlashUpdateFlag`、`u8FlashUpdateE2PROM` | 已有 `SCI_PORT_RUNTIME`，建议把旧 per-port 散变量继续并入端口结构；升级标志另建 `SCI_RUNTIME` 或 `FLASH_UPDATE_STATE` |
| System_Init | `s_st_SysTimePending`、`s_u32Sys10msTickCount`、`s_u8Cnt50ms/100ms/200ms/1000ms`、`fac_us`、`fac_ms`、`s_u8Sys200msPendingPeriods`、`s_u16Sys200msOverflowCnt` | 建议新增 `SYS_RUNTIME s_sys`；`g_st_SysTimeFlag` 作为 ISR 对外旗标先保留 |
| RTC | `TimeDisplay`、`RTC_time`、`s_u32RtcLastWakeupPeriodSeconds`、`s_u32RtcWakeupPeriodOverrideSeconds`、`is_rtc_wakekup` | 建议新增 `RTC_RUNTIME s_rtc`；`RTC_time` 和 `is_rtc_wakekup` 是否改 extern 需确认 |
| SleepDeal | `RTC_ExtComCnt`、`s_u8BootFromSleepStartup`、`s_u8BootFromSleepChargerWakeup` | 建议新增 `SLEEP_RUNTIME s_sleep`；`RTC_ExtComCnt` 已被低功耗阻塞边沿使用，改动要同步端口函数 |
| Fault | `PRT_E2ROMParas`、`Fault_Flag_Fisrt/Second/Third`、`Fault_record_Third*`、`FaultPoint_Third*` | 这是协议/持久化相关公开镜像，暂不建议直接改名；可以先做 `FAULT_RUNTIME` 包装和访问函数 |
| SystemDebug | `s_dbg_events[]`、`s_dbg_event_head`、`s_dbg_event_count`、`s_dbg_fault_snap`、`s_dbg_fault_valid` | 低风险，建议收口成 `DBG_RUNTIME s_dbg_rt` |
| Runtime | `s_dbg_print_tick`、函数内 `s_last_fault`、`s_last_lp_mode` | 低风险，建议收口成 `APP_RUNTIME s_app_rt` |
| Flash | `s_u8StorageFlashBusy` | 低风险，建议收口成 `FLASH_RUNTIME s_flash` |
| LedBar | `key_release_wakeup` | 建议并入 `s_ledbar`，或改成 `LedBar_GetWakeupReleased()` 访问 |
| SH367309 | `SH367309_Reg_Store`、`AFE_PARAM_WRITE_Flag`、`AFE_ROM_PARAMETERS_Struction`、`AFE_Parameters_RS485_Struction`、`ucMTPBuffer[]` | AFE 寄存器和参数镜像影响通信/保护，需先确认；常量表不需要收口 |
| AppInit/conf | `SeriesNum`、`sys_time` | 当前被多模块直接使用，建议作为兼容层保留；后续可通过 `APP_CONFIG` 或访问函数逐步替代 |

## 推荐架构

### 控制状态

模块拥有自己的运行状态，保持私有或有限导出：

```c
static ADC_RUNTIME s_adc;
static SCI_RUNTIME s_sci;
static SYS_RUNTIME s_sys;
static RTC_RUNTIME s_rtc;
static FAULT_RUNTIME s_fault;
```

模块之间通过函数或端口层访问，不直接跨模块写对方结构体字段。这个模式和当前 `s_ledbar`、`s_soc`、`s_factory_aging`、`s_log_record` 更一致。

### Keil 观察入口

优先路线：继续扩展已有 `g_dbg`。它已经覆盖 GPIO、MOS、系统、CAN、低功耗、ADC、SOC、AFE、故障、老化、Flash、LedBar、计数器等信息，适合做统一观察。

需要直接观察内部状态时，新增只读观察根：

```c
struct APP_WATCH {
    volatile struct LOW_POWER_RTC_STATUS *lp;
    struct SYSTEM_DEBUG *dbg;
    struct SOC_DEBUG_WATCH *soc;
    const ADC_RUNTIME *adc;
    const SCI_RUNTIME *sci;
    const SYS_RUNTIME *sys;
    const RTC_RUNTIME *rtc;
};

extern const struct APP_WATCH g_watch;
```

`g_watch` 只聚合指针，方便 Keil 展开，不作为业务写入口。这样能满足“掌控全局变量和调试观察”，又不破坏模块归属。

## 命名建议

| 类型 | 建议 | 示例 |
|---|---|---|
| 模块私有状态 | `s_模块名`，简短清晰 | `s_adc`、`s_sci`、`s_rtc` |
| 全局调试快照 | 保持 `g_dbg` | `g_dbg.lp.block` |
| 观察根 | 如新增，使用 `g_watch` 或 `g_app` | `g_watch.lp`、`g_watch.soc` |
| 结构体类型 | 模块名 + `Runtime` 或 `State` | `AdcRuntime`、`SciRuntime` |
| 私有函数 | 小写模块前缀 + 动作 | `adc_sync()`、`rtc_sync()` |
| 对外函数 | 保持现有公开 API 风格 | `ADC_GetVbatMilliVolt()` |

不建议继续新增 `s_u16IdleDelaySeconds` 这类按类型编码的长变量名。已有对外符号暂不强行改名，避免一次性破坏太多引用。

## 分阶段实施建议

| 阶段 | 范围 | 风险 | 建议 |
|---|---|---|---|
| 第 1 阶段 | `SystemDebug`、`Runtime`、`Flash`、`SleepDeal`、`RTC` 的散计数器和标志 | 低 | 已执行，基本不碰协议 |
| 第 2 阶段 | `ADC` 运行状态结构体化 | 中 | 已执行，保留读取函数，未改协议字段 |
| 第 3 阶段 | `DataDeal` 函数内静态变量与 AFE 电流运行状态继续收口 | 中 | 每个功能点独立提交，防止保护逻辑回归 |
| 第 4 阶段 | `Sci_Upper` 旧 per-port 散变量合并到 `SCI_PORT_RUNTIME` | 中高 | 需要重点验证 USART 收发、Modbus、升级写寄存器 |
| 第 5 阶段 | `Fault`、`OtherElement`、`g_stCellInfoReport`、`PRT_E2ROMParas` 等协议/持久化镜像 | 高 | 暂不直接改，必须先确认上位机、Modbus/CAN、Flash 兼容 |
| 第 6 阶段 | 新增 `g_watch` 或扩展 `g_dbg` 为统一 Keil 观察入口 | 低到中 | 可与前几阶段同步推进 |

## 第 1 阶段执行记录

执行时间：2026-06-03

源码修改：

- `SystemDebug.c`：新增 `DBG_RUNTIME s_dbgRt`，收口事件环、事件 head/count、故障快照和故障快照有效标志。
- `Runtime.c`：新增 `APP_RUNTIME s_rt`，收口调试打印 tick、上次故障快照和上次低功耗模式。
- `Flash.c`：新增 `FLASH_RUNTIME s_flash`，收口 Flash 写入 busy 标志，保留 `StorageFlash_IsBusy()` 外部接口。
- `SleepDeal.c/.h`：新增 `SLEEP_RUNTIME s_sleep`，收口外部通信计数、休眠启动标志和充电唤醒标志；新增 `SleepDeal_RecordExternalComm()`、`SleepDeal_GetExternalCommCounter()`。
- `RTC.c/.h`：新增 `RTC_RUNTIME s_rtc`，收口秒中断显示标志、RTC 唤醒标志、唤醒周期配置和 RTC 时间内部镜像；新增 `RTC_IsStopWakeup()`、`RTC_ClearStopWakeup()`。
- `Sci_Upper.c`：USART 收包入口改为调用 `SleepDeal_RecordExternalComm()`。
- `rtc_sleep.c`、`rtc_sleep_port.c`、`conf.c`：低功耗和恢复流程改为通过 RTC/SleepDeal 访问函数读取或清除状态。
- `tools/project_check.py`：新增第 1 阶段门禁，防止旧散变量回流。

保持不变：

- 不修改 `g_stCellInfoReport`、`OtherElement`、`PRT_E2ROMParas`、`System_ErrFlag` 等协议/持久化镜像。
- 不修改 Modbus/CAN 寄存器、帧格式、CAN ID、Flash 持久化结构布局。
- `RTC_time` 作为对外兼容镜像暂时保留，内部 RTC 状态以 `s_rtc.time` 为主。

验证记录：

- `git diff --check`：通过，仅有仓库既有 CRLF 换行提示。
- 旧符号 `rg` 检查：本阶段旧散变量无命中。
- `py -3.9 tools/project_check.py --quiet`：`103 OK / 1 Warning / 39 Errors`；新增第 1 阶段门禁通过，剩余失败为仓库既有缺文件、编码和配置门禁问题。
- `./tools/bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil 日志显示 `0 Error(s), 3 Warning(s)`。
- 上板验证仍需覆盖：RTC Alarm 唤醒后 `RTC_IsStopWakeup()` 状态、USART 收包后外部通信阻塞位、reset sleep 唤醒路径。

## 第 2 阶段执行记录

执行时间：2026-06-03

源码修改：

- `ADC.c`：新增 `ADC_RUNTIME s_adc`，收口 DMA raw、滤波缓存、结果数组、Vbat、Type-C 电流、调度 tick、Type-C 平滑计数和 VBC 平滑计数。
- `ADC.h`：删除 ADC 原始数组、结果数组、Vbat 和 Type-C 电流的 extern 声明；新增 `ADC_GetResult()`、`ADC_GetRaw()`。
- `DataDeal.c`：温度和 Vbat 诊断路径改为通过 `ADC_GetResult()`、`ADC_GetVbatMilliVolt()` 读取。
- `SOC.c`：总压 fallback 和 Type-C 电流注释路径改为使用 ADC getter。
- `SystemDebug.c`：`g_dbg.adc` 快照改为通过 ADC getter 读取 raw/result/Vbat/Type-C。
- `tools/project_check.py`：新增 ADC 结构体化门禁，防止旧 ADC 散变量回流。

保持不变：

- ADC DMA 通道、触发源、采样通道顺序不变。
- Type-C 电流从 PA2 delta 电压换算为 mA 的公式不变。
- VBC 分压换算公式不变。
- SOC 使用 Type-C 电池侧等效电流路径不变。
- `g_stCellInfoReport` 协议镜像不迁移。

待验证：

- `git diff --check`。
- `rg "g_u16ADCValFilter|g_i32ADCResult|g_u32Vbat_mV|g_u16TypeCOutCurrent_mA" "103 + 309/Project/Source"` 无命中。
- `python3 tools/project_check.py --quiet` ADC 门禁通过；若全仓有历史 warning/fail，应按当前基线解读。
- Keil `FD_Release` 编译。
- 上板确认 ADC DMA raw 更新、MOS 温度、VBC、电流方向和 Type-C 电流显示/上报行为不变。

## 第 3 阶段执行记录

执行时间：2026-06-03

源码修改：
- `DataDeal.c`：新增 `DATA_RUNTIME s_data`，内部划分 `cur/mon/afeSeq`，分别管理 AFE 电流零点运行态、AFE 通信监控运行态和 AFE 电流采样序号。
- `DataDeal.c`：将原 `s_afe_current` 并入 `s_data.cur`，将 `u8IICFaultcnt1/2`、`u8WakeCnt1/2`、`su16_Sleep_DelayT1/T2/T3` 并入 `s_data.mon`，将 `g_u32AfeCurrentSampleSeq` 并入 `s_data.afeSeq`。
- `DataDeal.h`：删除 `g_u32AfeCurrentSampleSeq` extern，新增 `AfeCurrent_GetSeq()`。
- `I2C_AFE1.c`、`SOC.c`：外部模块改为通过 `AfeCurrent_GetSeq()` 读取采样序号。
- `tools/project_check.py`：新增 `check_datadeal_runtime_state()`，检查旧散变量是否回流、`s_data` 是否存在、外部是否通过 getter 读取 AFE 采样序号。

保持不变：
- AFE 电流 raw 转 signed、mA、A10 的公式不变。
- AFE 电流自动零点学习、启动零点校准、输出死区、充放电方向判断不变。
- AFE 通信异常计数、恢复触发、wake retry 和通信异常延时 sleep 的阈值不变。
- `g_u16CalibCoefK/B`、`g_u32CS_Res_AFE`、`OtherElement`、`g_stCellInfoReport` 暂不迁移；这些对象和校准参数、保护参数、协议镜像、Flash/上位机兼容关系更紧，需后续单独阶段处理。

验证记录：
- `git diff --check`：通过，仅有仓库既有 CRLF 换行提示。
- `rg "g_u32AfeCurrentSampleSeq|u8IICFaultcnt|u8WakeCnt|su16_Sleep_DelayT|s_afe_current" "103 + 309/Project/Source"`：无命中。
- `py -3.9 tools/project_check.py --quiet`：`109 OK / 1 Warning / 39 Errors`；新增 DataDeal 门禁通过，剩余失败为仓库既有缺文件、编码和配置门禁问题。
- `./tools/bms_dev_workflow.ps1 -Mode build -Target FD_Release`：生成 `FD_Release.axf/bin`，Keil 日志显示 `0 Error(s), 3 Warning(s)`；Keil 进程码为 1，表现为 post-build 路径编码相关的非零退出，但产物已更新。
- 上板确认仍需覆盖：AFE 通信异常、自动恢复、启动零点校准、SOC 新样本触发和通信异常延时 sleep 行为不变。

## 需求确认表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| GS-001 | 后续不再新增低功耗/运行状态类单独散变量，优先归入模块状态结构体 | `g_stLowPowerRtcStatus`、`s_ledbar`、`s_soc`、`s_factory_aging` 已体现该方向 | 新旧风格混用 | 继续新增散变量会让 Keil 观察和维护困难 | KEEP_BUT_REFACTOR | 是否确认作为后续编码规则？ | 确认 | 待确认 |
| GS-002 | 不把所有模块可写状态合并成一个巨大全局控制结构体 | 多模块已有私有结构体和公开协议镜像 | 状态所有权分散但边界清楚 | 巨大全局结构体会增加耦合、并发和误写风险 | MUST_KEEP | 是否接受“模块归属 + 全局观察根”的方案？ | 接受 | 待确认 |
| GS-003 | Keil 观察统一入口优先使用 `g_dbg`，必要时新增 `g_watch` 指针聚合 | `SystemDebug.h` 已有 `struct SYSTEM_DEBUG g_dbg` | `g_dbg` 已覆盖多数调试信息 | 若用业务总结构体替代 `g_dbg`，容易污染运行逻辑 | CHANGE_NEEDED | 是否需要新增 `g_watch`，还是只扩展 `g_dbg`？ | 先扩展 `g_dbg`，必要时再加 `g_watch` | 待确认 |
| GS-004 | 协议/持久化镜像暂不直接改名或迁移 | `g_stCellInfoReport`、`OtherElement`、`PRT_E2ROMParas`、`System_ErrFlag` 被通信、Flash、保护逻辑直接使用 | 对外协议依赖这些全局镜像 | 直接迁移可能破坏 Modbus/CAN/上位机兼容 | MUST_KEEP | 是否确认这些对象先只纳入观察，不做控制源迁移？ | 确认 | 待确认 |
| GS-005 | 常量表和 dispatch 表不纳入运行状态结构体 | SOC 表、NTC 表、AFE 档位表、CAN dispatch 表均为 const 或查表用途 | 静态常量独立放置 | 强行塞进状态结构体会浪费 RAM 或降低可读性 | MUST_KEEP | 是否确认常量表不参与“单变量结构体化”？ | 确认 | 待确认 |
| GS-006 | 命名从长类型前缀转向短模块名和清晰字段名 | 当前存在 `s_u16...`、`gu8_...`、`g_st...` 等混合风格 | 老代码命名不一致 | 一次性全改会带来大范围风险 | KEEP_BUT_REFACTOR | 是否同意新代码使用 `s_adc.field`、`s_sci.port1` 这类简洁命名，旧公开 API 暂不强制改名？ | 同意 | 待确认 |

## 本轮不建议立即修改的内容

- 不直接移动 `g_stCellInfoReport`。
- 不直接移动 `OtherElement`。
- 不直接移动 `PRT_E2ROMParas`。
- 不直接改变 Modbus/CAN 协议字段。
- 不直接改变 Flash 持久化结构布局。
- 不直接重命名全部公开 API。
- 不直接把 const 表塞进 RAM 结构体。

这些项目必须等需求确认后再分批处理。

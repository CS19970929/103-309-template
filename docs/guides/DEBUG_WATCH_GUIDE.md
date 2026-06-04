# Keil Watch 调试观察入口说明

## 2026-06-04 补充：Debug/Release 隔离

- `FD_Debug` 是调试入口，Keil Watch 只需要添加 `g_dbg_watch`。
- `g_dbg` 不再建议单独添加；SystemDebug 快照从 `g_dbg_watch.system.snapshot` 展开。
- Runtime 事件、profile、模块心跳和 debug print 周期输出已集中到 `DebugHooks.c`。
- `FD_Release` 对 `DebugHooks.c`、`DebugWatch.c`、`SystemDebug.c`、`IrqDebug.c` 设置 `IncludeInBuild=0`，不编译不链接；业务流程中的 `DebugHooks_Runtime*()` 在 Release 下为空宏。
- 后续新增调试点应优先放到 `DebugHooks.c` 或 Debug-only 模块，不要把 `SystemDebug_Event()`、profile 记录和调试打印细节写回业务调度文件。

文档状态：已按源码验证
最后更新时间：2026-06-04
参考源码：
- `103 + 309/Project/Source/DebugHooks.c/.h`
- `103 + 309/Project/Source/DebugWatch.h`
- `103 + 309/Project/Source/conf/Project_BuildGuard.h`
- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/LogRecord.c`
- `103 + 309/Project/Source/System_Init.c`
- `103 + 309/Project/Source/System_Monitor.c`
- `103 + 309/Project/Source/SystemDebug.c/.h`
- `103 + 309/Project/Source/IrqDebug.c/.h`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/I2C_AFE1.c`
- `103 + 309/Project/Source/SH367309_Func.c`
- `103 + 309/Project/Source/SH367309_DataDeal.c`
- `103 + 309/Project/Source/Fault.c`
- `103 + 309/Project/Source/ProductionID.c`
- `103 + 309/Project/Source/CanFeidaoFrames.c`
- `103 + 309/Project/Source/SocEnhance.c/.h`

## 目标

变量重构后，模块内部状态优先保持 `static runtime` 封装，避免为了在线调试把业务状态全部改成裸全局变量。

需要 Keil Watch 观察时，只添加一个全局根结构体变量：

```c
g_dbg_watch
```

`g_dbg_watch` 内部集中挂载各模块 `static runtime` 指针和关键全局状态指针。

## 配置开关

默认配置：

```c
#define PROJECT_CFG_DEBUG_WATCH_ENABLE 0
#define PROJECT_CFG_DEBUG_MONITOR_ENABLE 0
#define PROJECT_CFG_IRQ_DEBUG_ENABLE 0
#define PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE 0
```

规则：

- 量产 Release 必须全部保持 `0`。
- `FD_Debug` target 当前定义 `PROJECT_CFG_DEBUG_WATCH_ENABLE=1`、`PROJECT_CFG_DEBUG_MONITOR_ENABLE=1`、`PROJECT_CFG_IRQ_DEBUG_ENABLE=1`、`PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE=0`。
- `Project_BuildGuard.h` 会阻止 build profile 0 打开 Debug Watch、SystemDebug 或 IRQ debug。
- `PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE` 默认保持 `0`，避免高频中断进入事件环带来额外干扰；需要追高频 IRQ 顺序时再临时打开。
- 开启后只增加观察符号，不改变业务状态更新逻辑。
- 不为了 Watch 给普通业务变量添加 `volatile`。

## 当前观察入口

Keil Watch 里优先只添加：

```c
g_dbg_watch
```

推荐按目录展开：

| 目录 | 用途 |
|---|---|
| `g_dbg_watch.runtime` | 各模块 `static runtime`，如 ADC、DataDeal、CAN、LedBar、Sleep、Flash、Log、FactoryAging、RTC、App、SOC |
| `g_dbg_watch.comm` | SCI1/2/3 端口 runtime、当前 Modbus 消息、发送缓存、通信错误计数、升级标志 |
| `g_dbg_watch.system` | 系统 tick、任务节拍、状态/错误、低功耗、IRQ 计数、`SystemDebug` 快照 |
| `g_dbg_watch.afe` | SH367309 寄存器、读回缓存、ROM 参数、RS485 参数、MTP 默认缓存 |
| `g_dbg_watch.fault` | 保护参数、一级/二级/三级故障位、三级故障记录和写入点 |
| `g_dbg_watch.public_data` | 对外状态结构，如电池实时数据、OtherElement、保护参数、生产信息、RTC 时间、SOC 对外状态 |
| `g_dbg_watch.app` | 应用层零散全局状态，如串数、事件日志运行秒计数 |
| `g_dbg_watch.calib` | K/B 校准系数和 AFE 采样电阻换算值 |
| `g_dbg_watch.tables` | 只读查表数据，如 NTC、AFE 阈值、SOC OCV、LED 映射、CAN 飞道广播调度表 |

旧字段如 `g_dbg_watch.adc`、`g_dbg_watch.data`、`g_dbg_watch.can_tx`、`g_dbg_watch.low_power` 仍保留，用于兼容已经保存的 Watch 表达式；新调试建议优先用目录字段。

`g_dbg` 不需要再单独加入 Keil Watch。`FD_Debug` 默认打开 `SystemDebug` 后，`g_dbg` 会作为快照实体挂到：

```c
g_dbg_watch.system.snapshot
```

IRQ 计数统一查看：

```c
g_dbg_watch.system.irq
g_dbg_watch.system.irq->total[IRQDBG_TIM3_10MS]
g_dbg_watch.system.irq->total[IRQDBG_CAN1_RX0]
g_dbg_watch.system.irq->last_id
g_dbg_watch.system.irq->last_phase
```

### ADC

```c
g_dbg_watch.runtime.adc
g_dbg_watch.runtime.adc->raw[0]
g_dbg_watch.runtime.adc->raw[1]
g_dbg_watch.runtime.adc->raw[2]
g_dbg_watch.runtime.adc->result[ADC_TEMP_MOS1]
g_dbg_watch.runtime.adc->vbat
g_dbg_watch.runtime.adc->typec
g_dbg_watch.runtime.adc->ready
g_dbg_watch.tables.adc_ntc_10k
```

### DataDeal / AFE 电流

```c
g_dbg_watch.data
g_dbg_watch.data->cur.zeroState
g_dbg_watch.data->cur.zeroReady
g_dbg_watch.data->cur.zeroOffsetRawQ4
g_dbg_watch.data->cur.lastRawSigned
g_dbg_watch.data->mon.ch[0].faultCnt
g_dbg_watch.data->mon.ch[0].wakeCnt
g_dbg_watch.data->mon.sleepDelay[0]
g_dbg_watch.data->afeSeq
```

### LedBar

```c
g_dbg_watch.ledbar
g_dbg_watch.ledbar->number
g_dbg_watch.ledbar->indicator_mask
g_dbg_watch.ledbar->scan_index
g_dbg_watch.ledbar->frame.length
g_dbg_watch.ledbar->soc_display_10ms
g_dbg_watch.ledbar->key_hold_10ms
g_dbg_watch.ledbar->key_active
g_dbg_watch.ledbar->mcu_wk_active
g_dbg_watch.ledbar->sleep
g_dbg_watch.ledbar->blank
```

### 飞道 CAN

Debug Watch 入口分成发送队列、周期调度和 App 命令三部分：

```c
g_dbg_watch.can_tx
g_dbg_watch.can_tx->count
g_dbg_watch.can_tx->mailbox
g_dbg_watch.can_tx->start_tick

g_dbg_watch.can_runtime
g_dbg_watch.can_runtime->tick
g_dbg_watch.can_runtime->last_1000ms_tick
g_dbg_watch.can_runtime->last_5000ms_tick

g_dbg_watch.can_app
g_dbg_watch.can_app->cmd_count
g_dbg_watch.can_app->write_pending
g_dbg_watch.can_app->write_addr
g_dbg_watch.can_app->read_block_active
g_dbg_watch.can_app->enter_iap_delay_ticks
```

`FD_Debug` 已默认打开 `PROJECT_CFG_DEBUG_MONITOR_ENABLE=1`。不要单独把 `g_dbg` 加入 Watch，需要快照时展开 `g_dbg_watch.system.snapshot->can`。

### SleepDeal / Flash / Log

```c
g_dbg_watch.sleep
g_dbg_watch.sleep->ext_comm
g_dbg_watch.sleep->boot_sleep
g_dbg_watch.sleep->chg_wake

g_dbg_watch.flash
g_dbg_watch.flash->busy

g_dbg_watch.log_record
g_dbg_watch.log_record->point
g_dbg_watch.log_record->uptimeSeconds
g_dbg_watch.log_record->lastSaveValid[0]
g_dbg_watch.log_record->eventLatch[0]
```

低功耗 RTC 状态当前仍可直接观察：

```c
g_dbg_watch.low_power
g_dbg_watch.low_power->mode
g_dbg_watch.low_power->block
g_dbg_watch.low_power->idle
g_dbg_watch.low_power->sleep
g_dbg_watch.irq_wakeup
*g_dbg_watch.irq_wakeup
```

### 系统节拍 / 状态

```c
g_dbg_watch.sys_time_latched
g_dbg_watch.sys_time_latched->all
g_dbg_watch.sys_time_pending
g_dbg_watch.sys_time_pending->all
g_dbg_watch.sys_10ms_tick_count
*g_dbg_watch.sys_10ms_tick_count
g_dbg_watch.sys_200ms_pending_periods
*g_dbg_watch.sys_200ms_pending_periods
g_dbg_watch.sys_200ms_overflow_count
*g_dbg_watch.sys_200ms_overflow_count

g_dbg_watch.system_feature
g_dbg_watch.system_feature->all
g_dbg_watch.system_status
g_dbg_watch.system_status->all
g_dbg_watch.system_error
g_dbg_watch.system_error->u8ErrFlag_ADC
```

核心状态也集中到根结构体：

```c
g_dbg_watch.cell_report
g_dbg_watch.cell_report->u16VCellTotle
g_dbg_watch.cell_report->u16Ichg
g_dbg_watch.cell_report->u16IDischg
g_dbg_watch.cell_report->SocElement.u16Soc

g_dbg_watch.other
g_dbg_watch.protect
```

### SOC

```c
g_dbg_watch.soc
g_dbg_watch.soc->u8InternalSoc
g_dbg_watch.soc->u8DisplaySoc
g_dbg_watch.soc->u8Mode
g_dbg_watch.soc->u8LastCalibSource
g_dbg_watch.soc->u8LowTailActive
g_dbg_watch.soc->u16EmptyTailTarget
g_dbg_watch.soc->u32RestTicks
g_dbg_watch.soc->u32StableRestTicks

g_dbg_watch.soc_public
g_dbg_watch.soc_public->u8_SOC
g_dbg_watch.soc_public->u16_CapacityNow
```

### 新增目录常用入口

```c
g_dbg_watch.comm.sci1
g_dbg_watch.comm.sci_msg1
g_dbg_watch.comm.sci_tx_buffer
g_dbg_watch.comm.sci_err1
g_dbg_watch.comm.flash_update_flag

g_dbg_watch.afe.registers_afe1
g_dbg_watch.afe.read_afe1
g_dbg_watch.afe.reg_store
g_dbg_watch.afe.rom_params
g_dbg_watch.afe.rs485_params
g_dbg_watch.afe.mtp_buffer

g_dbg_watch.fault.first
g_dbg_watch.fault.second
g_dbg_watch.fault.third
g_dbg_watch.fault.record_third
g_dbg_watch.fault.point_third

g_dbg_watch.public_data.cell_report
g_dbg_watch.public_data.other
g_dbg_watch.public_data.protect
g_dbg_watch.public_data.production
g_dbg_watch.public_data.rtc_time

g_dbg_watch.runtime.factory_aging
g_dbg_watch.runtime.rtc
g_dbg_watch.runtime.app
g_dbg_watch.runtime.soc_state
g_dbg_watch.runtime.soc_saved

g_dbg_watch.tables.afe_ntc_10k
g_dbg_watch.tables.afe_crc8
g_dbg_watch.tables.sh_ntc_10k
g_dbg_watch.tables.sh_afe_scv
g_dbg_watch.tables.sh_afe_sct
g_dbg_watch.tables.soc_lifepo
g_dbg_watch.tables.soc_ternary
g_dbg_watch.tables.can_feidao_dispatch
g_dbg_watch.tables.can_feidao_dispatch_count
```

## 后续扩展原则

新增观察入口时只扩展 `DEBUG_WATCH_ROOT`，不要再新增单模块 `g_dbg_*` 全局符号。

```c
#if DEBUG_WATCH_ENABLED
void Module_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
    watch->module = &s_module_runtime;
}
#endif
```

不建议把以下变量为了 Watch 改成全局：

- 文件内只服务单模块状态机的 `static` 变量。
- 协议、Flash、SOC 等已经有明确所有权的结构体字段。
- 中断或 DMA 相关变量，除非本身确实需要 `volatile`。

已经是全局变量的核心状态也优先挂到 `g_dbg_watch`，例如 `g_stCellInfoReport`、`System_ErrFlag`、`OtherElement` 和 `PRT_E2ROMParas`。

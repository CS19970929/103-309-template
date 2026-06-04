# Keil Watch 调试观察入口说明

文档状态：已按源码验证
最后更新时间：2026-06-04
参考源码：
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
```

规则：

- 量产 Release 必须保持 `0`。
- `FD_Debug` target 当前定义 `PROJECT_CFG_DEBUG_WATCH_ENABLE=1`。
- `Project_BuildGuard.h` 会阻止 build profile 0 打开该开关。
- 开启后只增加观察符号，不改变业务状态更新逻辑。
- 不为了 Watch 给普通业务变量添加 `volatile`。

## 当前观察入口

Keil Watch 里优先只添加：

```c
g_dbg_watch
```

### ADC

```c
g_dbg_watch.adc
g_dbg_watch.adc->raw[0]
g_dbg_watch.adc->raw[1]
g_dbg_watch.adc->raw[2]
g_dbg_watch.adc->result[ADC_TEMP_MOS1]
g_dbg_watch.adc->vbat
g_dbg_watch.adc->typec
g_dbg_watch.adc->ready
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

如果另行打开 `PROJECT_CFG_DEBUG_MONITOR_ENABLE=1`，仍可看 `SystemDebug` 的 `g_dbg.can` 快照；默认 `FD_Debug` 主要使用 `g_dbg_watch`。

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

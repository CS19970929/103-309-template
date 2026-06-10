# Keil Watch 任务 Trace 观察说明

## Watch 变量

建议在 Keil Debug Watch 窗口加入以下变量：

```c
gu32_AppTraceLoopCnt
gu32_AppTrace1msTick
gu32_AppTrace10msPhaseTick
gu32_AppTrace10msFlag1Tick
gu8_AppTraceLast10msPhase
gu8_AppTraceCurrentTask
gu8_AppTraceCurrentWarnCheck
gu16_SysTime1msOverrunCnt
gu16_SysTime10msPhaseOverrunCnt
g_stAppTraceTask
g_stAppTraceWarnCheck
```

## 主循环任务判断

`g_stAppTraceTask[index]` 用于观察主循环任务：

- `runCnt`：任务入口调用次数，持续增长表示主循环仍在调度该任务。
- `lastLoopInterval`：该任务两次入口之间间隔了多少轮主循环。
- `maxLoopInterval`：调试期间观察到的最大主循环间隔。
- `last10msFlag1Interval`：该任务两次入口之间跨过多少个 `b1Sys10msFlag1` 调度点。
- `max10msFlag1Interval`：调试期间观察到的最大 `b1Sys10msFlag1` 间隔。
- `active`：CPU 停在任务内部时为 1，任务返回后为 0。

常用任务索引：

- `APP_TRACE_TASK_WARN_CTRL`
- `APP_TRACE_TASK_LED_BAR`
- `APP_TRACE_TASK_AFE_GET`
- `APP_TRACE_TASK_POWER_UI`
- `APP_TRACE_TASK_SCI`
- `APP_TRACE_TASK_ANALOG_CAL`
- `APP_TRACE_TASK_EEPROM`
- `APP_TRACE_TASK_CELL_BALANCE`
- `APP_TRACE_TASK_SLEEP`
- `APP_TRACE_TASK_SOC`
- `APP_TRACE_TASK_BMS_EUAVCAN`

对 `APP_TRACE_TASK_WARN_CTRL` 和 `APP_TRACE_TASK_LED_BAR`，正常情况下 `last10msFlag1Interval` 应主要保持为 1。如果变大，说明对应 10ms Flag1 调度点被跳过或主循环调度存在异常。

## App_WarnCtrl 子任务判断

`g_stAppTraceWarnCheck[index]` 用于观察 `App_WarnCtrl()` 内部每个保护检查项：

- `runCnt`：该检查项调用次数。
- `lastWarnCtrlInterval`：该检查项两次调用之间跨过多少次 `App_WarnCtrl()`。
- `maxWarnCtrlInterval`：调试期间观察到的最大 `App_WarnCtrl()` 间隔。
- `active`：CPU 停在该检查项内部时为 1，返回后为 0。

正常情况下，每个 `g_stAppTraceWarnCheck[index].lastWarnCtrlInterval` 应保持为 1，且所有检查项的 `runCnt` 应与 `g_stAppTraceTask[APP_TRACE_TASK_WARN_CTRL].runCnt` 基本一致。

常用告警检查索引：

- `APP_WARN_CHECK_CELL_OVP_SECOND`
- `APP_WARN_CHECK_CELL_OVP_THIRD`
- `APP_WARN_CHECK_CELL_UVP_SECOND`
- `APP_WARN_CHECK_CELL_UVP_THIRD`
- `APP_WARN_CHECK_BAT_OVP_SECOND`
- `APP_WARN_CHECK_BAT_OVP_THIRD`
- `APP_WARN_CHECK_IDISCHG_OCP_SECOND`
- `APP_WARN_CHECK_IDISCHG_OCP_THIRD`
- `APP_WARN_CHECK_ICHG_OCP_SECOND`
- `APP_WARN_CHECK_ICHG_OCP_THIRD`
- `APP_WARN_CHECK_CELL_CHG_OTP_SECOND`
- `APP_WARN_CHECK_CELL_CHG_OTP_THIRD`
- `APP_WARN_CHECK_CELL_CHG_UTP_SECOND`
- `APP_WARN_CHECK_CELL_CHG_UTP_THIRD`

## 过载判断

`gu16_SysTime1msOverrunCnt` 或 `gu16_SysTime10msPhaseOverrunCnt` 只要递增，就说明主循环没有及时消费 pending 时基事件，队列已经饱和。此时单看任务 `runCnt` 还不够，应优先排查阻塞延时、EEPROM/Flash 写入、AFE 访问、日志记录或串口/CAN 发送路径是否耗时过长。

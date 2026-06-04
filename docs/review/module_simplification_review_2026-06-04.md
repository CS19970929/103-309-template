# 其他模块简化审查与 LedBar 净删减记录

文档状态：已按源码部分验证
源码验证：PARTIAL
主要参考源码：
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/LedBar.h`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/LowPowerSleep.c`
- `103 + 309/Project/Source/rtc_sleep.c`
最后更新时间：2026-06-04
未确认事项：未做上板显示扫描验证；`docs/code_flow_analysis/` 为历史/生成索引，本次未人工同步。

## 1. 审查原则

本轮按 SOC 模块重构后的同一原则检查其它模块：

- 优先删除没有真实调用方、没有参与当前行为的状态和接口。
- 保留有明确边界价值的小函数，例如调度阶段、睡眠保存边界、硬件时序边界。
- 不修改协议字段、低功耗条件、显示扫描时序、SOC 发布口径和保护判断。
- 不为了“结构完整”新增包装层或未来预留接口。

## 2. 已修改模块：LedBar

### 2.1 删除内容

| 类型 | 删除项 | 原因 |
|---|---|---|
| 公开 API | `LedBar_SetSingleSegmentIndex()` | 无外部调用，写入的 `test_single_segment_id` 不参与当前帧构建和扫描输出。 |
| 公开 API | `LedBar_SetIndicators()` | 无外部业务调用；当前图标发布在 `APP_LedBar()` 内直接计算并写入 `s_ledbar.indicator_mask`。 |
| 公开 API | `LedBar_SetIndicatorState()` | 无外部调用，只是 `LedBar_SetIndicators()` 的二次包装。 |
| 运行时字段 | `s_ledbar.test_single_segment_id` | 仅由已删除测试 API 写入，没有消费者。 |
| 宏 | `LEDBAR_SINGLE_SEG_ID_MIN/MAX` | 删除单段测试 API 后无消费者。 |

### 2.2 保留内容

| 保留项 | 理由 |
|---|---|
| `APP_LedBar()` 内部图标计算 | 这是当前真实显示策略入口，直接读取 SOC、故障、MOS 和窗口状态。 |
| `LedBar_SetNumber()` | 仍是外部可用的数字显示入口，并执行 SOC 限幅、blank 状态刷新和输出刷新。 |
| `LedBar_Scan1ms()` | 当前由 `TIM4_IRQHandler()` 调用，负责查理复用扫描输出，不能删除。 |
| `LedBar_SaveSleepSoc()` / `LedBar_LoadSleepSoc()` | 低功耗前后 SOC 预览依赖 BKP 保存/恢复。 |
| `LedBar_SetSleep()` / `LedBar_PrepareForStop()` | STOP 前 GPIO 状态和显示关闭边界仍有效。 |

### 2.3 行为边界

本次不改变：

- `TIM4_IRQHandler()` 到 `LedBar_Scan1ms()` 的扫描链路。
- `APP_LedBar()` 的显示窗口、故障闪烁、充电/百分号图标规则。
- BKP 休眠 SOC 保存/恢复寄存器。
- 低功耗阻塞条件 `LedBar_IsActiveForLowPower()`。
- Modbus/CAN/SOC 对外发布字段。

## 3. 已审查但暂不修改的模块

| 模块 | 观察 | 判断 |
|---|---|---|
| `Runtime.c` | `Runtime_RunFrontTasks()`、`Runtime_RunIoAndPowerTasks()`、`Runtime_RunBackgroundTasks()` 是主循环阶段边界，中间插入了 DebugHooks 计时和事件钩子。 | 保留。虽然函数短，但它们承载调试剖面边界，合并会降低运行阶段可观测性。 |
| `LowPowerSleep.c` | `LowPowerSleep_SaveCoreState()` 和 `LowPowerSleep_SaveResetState()` 分别对应普通睡眠保存和 reset-sleep 额外 LedBar SOC 保存。 | 保留。函数短，但调用语义不同，边界清楚。 |
| `rtc_sleep.c` | `lp_idle()` 已合并回 `lp_select()`，无调用的 `LP_GetLastSleepSeconds()` / `LP_RecordLastSleepSeconds()` 已删除；`lp_sync()`、`lp_deep()`、`lp_select()`、`rtc_sleep_prepare_rtc()`、`rtc_sleep_run_hiccup_cycle()` 仍保留。 | 已小步净删减；剩余函数分别对应调试状态同步、deep 优先级判断、模式选择和 STOP 进入/恢复边界，暂不继续合并。 |

## 4. 结论

本轮只落地 `LedBar` 的净删减，因为它符合“无调用方、无消费者、无当前行为”的低风险条件。其它看起来短的小函数暂不按代码行数机械合并，后续如果继续简化，应优先从静态调用关系明确的 dead API、无消费者状态、已失效配置分支继续下手。

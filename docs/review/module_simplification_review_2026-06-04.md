# 其他模块简化审查与 LedBar 净删减记录

文档状态：已按源码部分验证
源码验证：PARTIAL
主要参考源码：
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/LedBar.h`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/LowPowerSleep.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/Can_HDX.c`
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
| 输入滤波 | `key` / `MCU_WK` 3 tick 软件滤波计数和通用二值滤波 helper | 按本轮试验要求删除滤波计数；保留上一拍原始电平用于边沿检测。 |
| 运行时字段 | `key_release_wakeup`、`key_last_pressed` | 裸全局/字段无外部消费者；长按 armed 状态收进 `s_ledbar.key_wakeup_armed`。 |

### 2.2 保留内容

| 保留项 | 理由 |
|---|---|
| `APP_LedBar()` 内部图标计算 | 这是当前真实显示策略入口，直接读取 SOC、故障、MOS 和窗口状态。 |
| `LedBar_SetNumber()` | 仍是外部可用的数字显示入口，并执行 SOC 限幅、blank 状态刷新和输出刷新。 |
| `LedBar_Scan1ms()` | 当前由 `TIM4_IRQHandler()` 调用，负责查理复用扫描输出，不能删除。 |
| `LedBar_SaveSleepSoc()` / `LedBar_LoadSleepSoc()` | 低功耗前后 SOC 预览依赖 BKP 保存/恢复。 |
| `LedBar_SetSleep()` / `LedBar_PrepareForStop()` | STOP 前 GPIO 状态和显示关闭边界仍有效。 |
| `s_ledbar.key_active` / `s_ledbar.mcu_wk_active` | 不再表示滤波后状态，只作为上一拍原始电平，用于按下/唤醒上升沿检测。 |

### 2.3 行为边界

本次不改变：

- `TIM4_IRQHandler()` 到 `LedBar_Scan1ms()` 的扫描链路。
- `APP_LedBar()` 的显示窗口、故障闪烁、充电/百分号图标规则。
- BKP 休眠 SOC 保存/恢复寄存器。
- 低功耗阻塞条件 `LedBar_IsActiveForLowPower()`。
- Modbus/CAN/SOC 对外发布字段。
- 按键和 `MCU_WK` 软件滤波已删除，需上板观察抖动体验；本轮不修改显示窗口时长和长按阈值。

## 3. 已修改模块：CAN App 返回帧

### 3.1 删除内容

| 类型 | 删除项 | 原因 |
|---|---|---|
| 内部 helper | `feidao_can_app_send_ack()` | 与 `READ_BLOCK_DATA` 发送函数重复组 `0x61` 标准帧，只是 `Data[2..5]` 不同。 |
| 内部 helper | `feidao_can_app_send_word_frame()` | 与普通 ACK 使用同一返回帧格式，合并为 `feidao_can_app_send_frame()` 后仍按 `0x86/seq/value_hi/value_lo` 填充。 |
| 内部 helper | `feidao_can_u16_to_percent()` | 只服务 `GET_STATUS` 一处分支，100% 限幅直接写回命令分支更直观。 |

### 3.2 保留内容

| 保留项 | 理由 |
|---|---|
| `s_app.write_pending/write_addr/write_value_hi` | `WRITE_PREP/WRITE_COMMIT` 两阶段写寄存器是 CAN App 协议边界，不能按状态变量简化删除。 |
| `Can_IsBusy()` / `Can_PeekBusy()` 分工 | `Can_IsBusy()` 会消费 CAN RX 活动计数，低功耗依赖该副作用；debug/heartbeat 使用无副作用查询。 |
| `READ_BLOCK` 分包状态 | 上位机依赖 `0x86` 顺序分包，且读块期间要阻塞 STOP。 |

### 3.3 行为边界

本次不改变：

- CAN App 命令 ID `0x60`、ACK ID `0x61`。
- ACK magic `5A A5`、CRC16 和 `Data[2..5]` 字段含义。
- `READ_BLOCK` 最大 120 words、1 tick 分包间隔和 `0x86` 数据帧顺序。
- TX 队列 request/periodic 来源标记。
- `Can_IsBusy()` 对低功耗的阻塞语义。

## 4. 已审查但暂不修改的模块

| 模块 | 观察 | 判断 |
|---|---|---|
| `Runtime.c` | `Runtime_RunFrontTasks()`、`Runtime_RunIoAndPowerTasks()`、`Runtime_RunBackgroundTasks()` 是主循环阶段边界，中间插入了 DebugHooks 计时和事件钩子。 | 保留。虽然函数短，但它们承载调试剖面边界，合并会降低运行阶段可观测性。 |
| `LowPowerSleep.c` | `LowPowerSleep_SaveCoreState()` 和 `LowPowerSleep_SaveResetState()` 分别对应普通睡眠保存和 reset-sleep 额外 LedBar SOC 保存。 | 保留。函数短，但调用语义不同，边界清楚。 |
| `rtc_sleep.c` / `SleepDeal.c` | `lp_idle()` 已合并回低功耗请求更新流程，无调用的 `LP_GetLastSleepSeconds()` / `LP_RecordLastSleepSeconds()` 已删除；`lp_sync()`、`lp_deep()`、`lp_select()` 已改为 `lp_refresh_status()`、`lp_select_deep_if_low_voltage()`、`lp_update_sleep_request()`；reset-sleep 三段重复 STOP 等待收敛为 `SleepDeal_WaitStopWakeup()`。 | 已小步净删减和命名收敛；保留状态刷新、deep 优先级判断、模式选择和 STOP 进入/恢复边界，不继续做大范围合并。 |

## 5. 结论

本轮落地 `LedBar` 和 CAN App 返回帧两处净删减，因为它们符合“无调用方、无消费者或重复实现、无当前行为变化”的低风险条件。其它看起来短的小函数暂不按代码行数机械合并，后续如果继续简化，应优先从静态调用关系明确的 dead API、无消费者状态、已失效配置分支继续下手。

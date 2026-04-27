# RTC CAN 自适应休眠说明

## 目标

空闲进入 HICCUP RTC 休眠后，根据 CAN 总线是否存在其他设备自动调整 RTC 唤醒周期：

- 检测到 CAN 总线上有其他设备：RTC 休眠 1 秒，唤醒后继续按原 1s / 5s CAN 报文节奏发送。
- 未检测到其他设备：RTC 休眠 10 秒，唤醒后只发送一帧轻量探测帧；未得到 ACK 时不继续发送业务周期帧，降低无 ACK 场景下的功耗。

## 总线设备判定

当前没有专用硬件检测“是否接设备”，软件使用 CAN 行为判断：

- 发送报文返回 `CAN_TxStatus_Ok`，说明总线上至少有节点 ACK，认为总线有其他设备。
- CAN RX0 中断收到报文，也认为总线有其他设备。
- 连续出现 ACK 失败或发送超时达到阈值后，认为总线无其他设备。
- 无设备状态下每 10 秒 RTC 唤醒会发送一帧探测帧；如果中途接入的 CAN 设备能 ACK 该帧，立即恢复为总线有设备。

如果完全不发送探测帧，且休眠时收发器关闭，那么软件无法发现只会 ACK、不主动发帧的中途接入设备。因此当前实现把“无设备不发业务 CAN”收敛为“无设备每 10 秒只发一帧探测”。

## 时序处理

CAN 发送调度保留原来的 10ms tick 状态机，并新增 CAN 内部逻辑 tick：

- 正常运行时，逻辑 tick 跟随 `SysTime_Get10msTickCount()` 推进。
- RTC Stop 期间 TIM3 停止，RTC 唤醒后通过实际 RTC 休眠秒数补偿逻辑 tick。
- 有设备时，RTC 唤醒后调用 CAN RTC 服务窗口，复用原 `feidao_can_send()` 状态机完成上电、稳定等待、发送、发送完成/超时处理。
- 无设备时，RTC 唤醒后只压入一帧 1000ms 电压/电流报文作为探测，不补发 10 秒内积累的周期报文；探测成功后把 1s / 5s 调度锚点重置到当前逻辑时间。
- 低功耗入口只等待正在进行的 CAN 上电/发送动作完成；已排队但还没开始发送的周期 pending 会在进入睡眠前丢弃，避免周期 CAN 任务反复打断 RTC 入睡计时。
- 主循环先执行 `App_LowPowerProcess()` 再执行 `App_Can()`，避免 1 秒 tick 到来时 CAN 先进入 `FEIDAO_CAN_POWER_WAIT_STABLE`，导致低功耗判断每次都认为 CAN 正在上电。

这样 1000ms 报文和 5000ms 报文不会因为 Stop 期间 TIM3 停止而长期漂移。

## 关键文件

- `103 + 309/Project/Source/Can_HDX.c`
  - 维护 CAN 总线活跃状态。
  - 提供 `Can_GetIdleRtcPeriodSeconds()`，输出 1 秒或 10 秒 RTC 周期。
  - 提供 `Can_RtcWakeService()`，RTC 唤醒后按补偿时间执行 CAN 发送窗口；无设备状态下只执行单帧探测。
- `103 + 309/Project/Source/RTC.c`
  - `RTC_GetWakeupPeriodSeconds()` 改为读取 CAN 给出的自适应周期。
  - `RTC_WKTimeConfig()` 记录本次实际配置的 RTC 周期，供休眠累计使用。
- `103 + 309/Project/Source/rtc_sleep.c`
  - 按每次 RTC Alarm 的实际周期累计休眠秒数。
  - RTC 周期内有设备时执行 CAN 发送，无设备时只执行单帧探测。

## 问题记录与处理

### 调试器下卡在 `RTC_WaitForSynchro`

现象：

- 使用调试器复位或单步运行时，程序可能一直停在 `RTC_WaitForSynchro()`。
- 正常上电不一定复现。

原因：

- 调试复位后备份域可能保留 RTC 标志，但 RTC 时钟源、`RTCEN`、`LSERDY` 等状态并不完整。
- 标准库 `RTC_WaitForSynchro()` 没有超时保护，RTC APB 同步条件不满足时会死等。

处理：

- `RTC.c` 中改为带超时的 RTC 等待接口。
- 复用 RTC 前先检查 `RCC->BDCR` 中 RTC 时钟状态；状态不完整时重新配置备份域。
- LSE 异常时回退 LSI，避免调试场景死锁。

### 无设备静默后中途接入无法发现

现象：

- 无 CAN 对端时进入 10 秒 RTC 休眠。
- 如果这期间中途接入 CAN 设备，但对端只 ACK、不主动发帧，软件无法知道总线上已经有设备。

原因：

- CAN 总线没有“纯软件被动检测新节点”的能力。
- 休眠期间收发器关闭，且完全不发送 CAN 时，既没有 RX，也没有 ACK 结果可用于判断。

处理：

- 无设备状态仍保持 10 秒 RTC 周期，但每次 RTC 唤醒只发送一帧探测帧。
- 探测帧得到 ACK 后，立即恢复为有设备状态，并重新锚定 1s / 5s 报文调度。
- 探测失败时不补发业务周期帧，继续保持低功耗策略。

### CAN pending 导致无法进入 RTC

现象：

- CAN 状态机看似已经回到空闲，但系统一直进不了 RTC。
- 日志可能反复出现 `can busy`。

原因：

- 低功耗入口原先调用 `Can_IsBusy()`。
- `Can_IsBusy()` 把尚未开始发送的周期 pending 也当作忙状态。
- 周期 pending 会反复清零 `sys_time.enter_rtc_delay`，导致 RTC 入睡计时不能累计。

处理：

- 新增 `Can_IsSleepBlocked()`。
- 低功耗入口只等待真实进行中的 CAN 动作：收发器上电等待、发送等待、硬件邮箱未空。
- 尚未开始发送的周期 pending 不再阻止 RTC；进入睡眠前由 `Can_PrepareSleep()` 丢弃，RTC 唤醒后用逻辑 tick 补偿。

### `FEIDAO_CAN_POWER_WAIT_STABLE` 阻止 RTC 入睡

现象：

- 低功耗判断中一直命中 `s_u8FeidaoCanPowerState != FEIDAO_CAN_POWER_IDLE`。
- 调试时状态常停在 `FEIDAO_CAN_POWER_WAIT_STABLE`。

原因：

- 主循环原先先执行 `App_Can()`，再执行 `App_LowPowerProcess()`。
- 每到 1 秒 tick，`App_Can()` 先启动 CAN 发送，把状态切到 `FEIDAO_CAN_POWER_WAIT_STABLE`。
- 随后低功耗判断立即看到 CAN 正在上电稳定等待，于是清零 RTC 入睡计时。

处理：

- 主循环顺序改为先执行 `App_LowPowerProcess()`，再执行 `App_Can()`。
- 1 秒 tick 到来时先判断是否应该进入 RTC；未进入 RTC 时，再启动新的 CAN 周期发送。

### 已能进入 RTC，但 RTC 期间未观察到 CAN 发送

现象：

- 前述修改后，系统可以进入 RTC 休眠。
- 但在 RTC 周期唤醒期间，暂未观察到 CAN 报文发送。

当前判断：

- RTC 周期唤醒后，CAN 发送只会在 `rtc_sleep_run_hiccup_cycle()` 中满足 `is_rtc_wakekup && !isException()` 时调用 `Can_RtcWakeService(rtc_elapsed_seconds)`。
- 如果 `isException()` 为真，代码会退出 RTC 流程，不会执行 RTC 周期 CAN 服务。
- 如果 `s_u8FeidaoCanBusActive == 0`，RTC 唤醒只发送一帧探测帧，不会发送完整 1s / 5s 业务报文。
- 如果总线活跃状态在入睡前没有被 ACK 或 RX 标记为 1，现象会表现为“休眠期间没有业务 CAN”，实际只应看到 10 秒一次探测帧。

排查办法：

- 在 `Can_RtcWakeService()` 入口打断点或加日志，确认 RTC Alarm 唤醒后是否进入该函数。
- 同时观察 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL：如果 `GPIO_CMNT_EN` 没有短时上电，说明 CAN RTC 服务没有被调用或提前返回。
- 检查唤醒时 `isException()` 的结果；异常唤醒路径当前不会发送 CAN。
- 检查入睡前 `Can_IsBusActive()` 是否为 1；若为 0，只应期待 10 秒一次探测帧。
- 若 `GPIO_CMNT_EN` 有上电脉冲但 CAN_TX 没有波形，继续检查 `FEIDAO_CAN_POWER_STABLE_TICKS`、`feidao_can_service_until_idle()` 内部 10ms 推进，以及 `CAN_Transmit()` 返回邮箱状态。

后续处理方向：

- 先用断点或示波器确认问题落在“未进入 `Can_RtcWakeService()`”还是“进入后未发出 CAN_TX”。
- 如果是 `isException()` 导致跳过，需要明确异常唤醒时是否仍要求发送 CAN；若要求，需要在异常路径增加受控的 CAN 服务窗口。
- 如果是总线活跃状态为 0，则当前行为符合低功耗策略；需要业务 CAN 时，应在入睡前由 ACK/RX 标记总线活跃，或调整策略为每次 RTC 唤醒都发送业务帧。

## 验证建议

1. 不接 CAN 对端：进入 RTC 后应为约 10 秒唤醒一次，`GPIO_CMNT_EN` 每 10 秒只出现一次短探测窗口，不应连续发送周期业务帧。
2. 接 CAN 对端并能 ACK：进入 RTC 后应约 1 秒唤醒一次，`GPIO_CMNT_EN` 在发送窗口短时上电，1s / 5s 报文周期保持。
3. 运行中拔掉对端：连续 ACK 失败后切到 10 秒静默周期。
4. 进入 10 秒无设备周期后再接入 CAN 对端：下一次 10 秒 RTC 唤醒的探测帧得到 ACK 后，应切回约 1 秒 RTC 周期。
5. 用示波器同时看 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL，确认收发器上电稳定时间和发送窗口满足芯片要求。

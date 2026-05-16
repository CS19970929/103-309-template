# RTC CAN 自适应休眠说明

## 目标

空闲进入 HICCUP RTC 休眠后，根据 CAN 总线是否存在其他设备自动调整 RTC 唤醒周期：

- 检测到 CAN 总线上有其他设备：RTC 休眠 1 秒，唤醒后继续按原 1s / 5s CAN 报文节奏发送。
- 未检测到其他设备：RTC 休眠 10 秒，唤醒后只发送两帧轻量探测帧；未得到 ACK 时不继续发送业务周期帧，降低无 ACK 场景下的功耗。

## 总线设备判定

当前没有专用硬件检测“是否接设备”，软件使用 CAN 行为判断：

- 发送报文返回 `CAN_TxStatus_Ok`，说明总线上至少有节点 ACK，认为总线有其他设备。
- CAN RX0 中断收到报文，也认为总线有其他设备。
- 上电或状态未知时先按有设备处理，避免有对端时直接进入 10 秒静默周期。
- 连续多个 CAN 上电发送窗口都没有 ACK 或发送超时后，才认为总线无其他设备；单个发送窗口内多帧失败只累计一次无 ACK，避免收发器供电刚切换时误判。
- 无设备状态下每 10 秒 RTC 唤醒会发送两帧探测帧；普通唤醒退出 RTC 后也保留 1 秒一次的轻量探测路径。如果中途接入的 CAN 设备能 ACK 任一帧，立即恢复为总线有设备。

如果完全不发送探测帧，且休眠时收发器关闭，那么软件无法发现只会 ACK、不主动发帧的中途接入设备。因此当前实现把“无设备不发业务 CAN”收敛为“无设备每 10 秒只发两帧探测”。

## 时序处理

CAN 发送调度保留原来的 10ms tick 状态机，并新增 CAN 内部逻辑 tick：

- 正常运行时，逻辑 tick 跟随 `SysTime_Get10msTickCount()` 推进。
- RTC Stop 期间 TIM3 停止，RTC 唤醒后通过实际 RTC 休眠秒数补偿逻辑 tick。
- 有设备时，RTC 唤醒后调用 CAN RTC 服务窗口，复用原 `feidao_can_send()` 状态机完成上电、稳定等待、发送、发送完成/超时处理。
- 无设备时，RTC 唤醒后只压入 1000ms 电压/电流和 SOC 两帧作为探测，不补发 10 秒内积累的周期报文；探测成功后把 1s / 5s 调度锚点重置到当前逻辑时间。
- 低功耗入口只等待正在进行的 CAN 上电/发送动作完成；已排队但还没开始发送的周期 pending 会在进入睡眠前丢弃，避免周期 CAN 任务反复打断 RTC 入睡计时。
- 主循环先执行 `App_LowPowerProcess()` 再执行 `App_Can()`，避免 1 秒 tick 到来时 CAN 先进入 `FEIDAO_CAN_POWER_WAIT_STABLE`，导致低功耗判断每次都认为 CAN 正在上电。

这样 1000ms 报文和 5000ms 报文不会因为 Stop 期间 TIM3 停止而长期漂移。

## 关键文件

- `103 + 309/Project/Source/Can_HDX.c`
  - 维护 CAN 总线活跃状态。
  - 提供 `Can_GetIdleRtcPeriodSeconds()`，输出 1 秒或 10 秒 RTC 周期。
  - 提供 `Can_RtcWakeService()`，RTC 唤醒后按补偿时间执行 CAN 发送窗口；无设备状态下只执行两帧探测。
  - `InitCan_GPIO()` 会恢复 CAN_TX/CAN_RX 和 `GPIO_CMNT_EN`，保证 RTC 模式把 IO 置为模拟输入后，唤醒发送窗口能重新驱动 CAN 收发器电源脚。
- `103 + 309/Project/Source/RTC.c`
  - `RTC_GetWakeupPeriodSeconds()` 改为读取 CAN 给出的自适应周期。
  - `RTC_WKTimeConfig()` 记录本次实际配置的 RTC 周期，供休眠累计使用。
- `103 + 309/Project/Source/rtc_sleep.c`
  - 按每次 RTC Alarm 的实际周期累计休眠秒数。
  - RTC 周期内有设备时执行 CAN 发送，无设备时只执行两帧探测。

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

- 无设备状态仍保持 10 秒 RTC 周期，但每次 RTC 唤醒只发送两帧探测帧。
- 任一探测帧得到 ACK 后，立即恢复为有设备状态，并重新锚定 1s / 5s 报文调度。
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

- 当前源码不再保留单独的 `Can_IsSleepBlocked()` 判定。
- 主循环顺序已经调整为先 `App_LowPowerProcess()`、后 `App_Can()`，避免 1 秒 tick 到来时 CAN 先启动发送窗口再阻断 RTC 入睡计时。
- 即将进入 STOP 或 reset sleep 时统一调用 `Can_PrepareSleep()`，丢弃尚未开始发送的周期 pending、取消 mailbox、关闭收发器；RTC 唤醒后再用逻辑 tick 补偿调度。

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
- 但在 RTC 周期唤醒期间，没有观察到 `GPIO_CMNT_EN` 上电脉冲或 CAN_TX 波形。

原因：

- 进入 RTC 前 `IOstatus_RTCMode()` 会把 GPIOB 置为模拟输入，`GPIO_CMNT_EN` 是 PB4，也会被关闭输出功能。
- RTC Alarm 唤醒后 `Can_RtcWakeService()` 会调用 `InitCan()`，但原先 `InitCan_GPIO()` 只恢复 PA11/PA12 的 CAN 引脚，没有恢复 PB4 的收发器电源控制脚。
- 另外，如果 CAN 调度锚点还没有初始化，第一次 RTC CAN 服务可能只设置 1s / 5s 调度基准，不会立即压入到期报文。

处理：

- `InitCan_GPIO()` 增加 GPIOB 时钟、JTAG disable remap 和 `GPIO_CMNT_EN/PIN_CMNT_EN` 推挽输出配置。
- RTC 有设备路径调用 `feidao_can_schedule_rtc_period_frames()`，按实际 RTC 休眠秒数补偿并压入到期 1s / 5s 报文，避免第一次 RTC 服务只初始化调度而不发送。
- 无设备路径仍只压入两帧探测，探测成功后恢复有设备状态。
- CAN 收发器从断电到重新供电需要硬件稳定时间；`FEIDAO_CAN_POWER_STABLE_TICKS` 由 10ms 放宽到 100ms，RTC 服务窗口放宽到 1.5s，避免电源刚打开时第一帧过早发送导致误判无 ACK。
- RTC 服务窗口内设置 `s_u8FeidaoCanRtcServiceActive`，只发送本次预加载的到期帧或探测帧，不再因为 1.5s 服务窗口跨过 1s 周期而额外生成新一轮周期帧。
- 发送失败判定改为每个 CAN 上电发送窗口最多累计一次无 ACK；窗口内任一帧发送成功或 RX 收到报文都会立即清零无 ACK 状态。
- 有设备策略下如果本次上电窗口第一帧就无 ACK，则结束本窗口并关闭 CAN 收发器，下一轮再试；无设备探测窗口仍保留两帧探测，兼顾功耗和中途接入检测。

验证：

- 在 `Can_RtcWakeService()` 入口打断点，确认 RTC Alarm 唤醒后进入该函数。
- 用示波器同时看 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL：有设备时应看到 1 秒 RTC 唤醒窗口内短时上电并发送业务帧；无设备时应看到 10 秒一次两帧探测。
- 如果 `GPIO_CMNT_EN` 有脉冲但 CAN_TX 没有波形，继续检查 `CAN_Transmit()` 返回邮箱状态和 `feidao_can_service_until_idle()` 的 10ms 推进。

### 有设备时断开 CAN 对端后未切到 10 秒休眠

现象：

- 总线上有其它 CAN 设备时，系统按有设备策略进行 RTC 周期发送。
- 中途断开其它 CAN 设备后，没有按预期切换到 10 秒 RTC 休眠。

原因：

- 原先 `CAN_TxStatus_Failed` 路径只在 CAN ESR 的 LEC 恰好为 `CAN_ErrorCode_ACKErr` 时才累计无 ACK。
- 实测断线时，发送失败状态不一定稳定保留为 ACKErr；如果只依赖 LEC，可能只记录失败次数，但不推进 `s_u8FeidaoCanNoAckCnt`。
- 无 ACK 计数达不到阈值时，`s_u8FeidaoCanBusActive` 会一直保持 1，`Can_GetIdleRtcPeriodSeconds()` 就仍返回有设备周期。

处理：

- `feidao_can_record_tx_failed()` 改为所有 TX failed 都纳入无 ACK 判定；如果 LEC 是 ACKErr，则额外统计 ACK error 计数。
- TX timeout 仍按无 ACK 处理。
- 无 ACK 判定按 CAN 上电发送窗口累计，不按单帧累计；连续多个窗口失败达到 `FEIDAO_CAN_NO_ACK_INACTIVE_LIMIT` 后，`s_u8FeidaoCanBusActive` 置 0，后续 RTC 周期切换到 10 秒。
- 为避免供电切换或对端唤醒瞬间造成误判，无 ACK 阈值由 3 次放宽到 6 次；无设备探测帧由 1 帧改为 2 帧，提高中途接入设备被 ACK 检出的概率。

验证：

- 接 CAN 对端运行，确认 `Can_IsBusActive()` 为 1，RTC 周期为有设备周期。
- 断开 CAN 对端，观察连续发送失败或 timeout 后，`s_u8FeidaoCanNoAckCnt` 增加并最终将 `s_u8FeidaoCanBusActive` 置 0。
- 下一轮 RTC 配置应使用 `FEIDAO_CAN_RTC_IDLE_PERIOD_SECONDS`，表现为约 10 秒唤醒一次。

### 退出 RTC 后 CAN 永久不再发送

现象：

- CAN 已经被判定为无设备后，RTC 10 秒唤醒期间没有观察到有效 CAN 通信。
- 之后即使退出 RTC 低功耗回到普通主循环，CAN 也一直不再发送，导致中途接入设备无法恢复通信。

原因：

- 无设备状态下 `s_u8FeidaoCanNoAckCnt` 已达到阈值。
- 普通 `App_Can()` 调度出的周期 pending 会被 `feidao_can_drop_pending_if_bus_inactive()` 清掉。
- 如果没有单独保留探测 pending，CAN 状态机就会一直保持空闲，既不会发送业务帧，也不会发送探测帧。

处理：

- 无设备状态下，普通主循环不再调度完整业务帧，但每 1 秒允许压入两帧轻量探测帧，并设置探测标志，避免 pending 被 inactive 逻辑清掉。
- 探测发送结束后关闭 CAN 收发器供电并清除探测标志，保证低功耗入口仍能正常进入 RTC。
- 任一探测帧发送成功或 RX 收到报文后，立即恢复有设备状态，后续重新按 1s / 5s 业务周期发送。

## 验证建议

1. 不接 CAN 对端：进入 RTC 后应为约 10 秒唤醒一次，`GPIO_CMNT_EN` 每 10 秒只出现一次短探测窗口，不应连续发送周期业务帧。
2. 接 CAN 对端并能 ACK：进入 RTC 后应约 1 秒唤醒一次，`GPIO_CMNT_EN` 在发送窗口短时上电，1s / 5s 报文周期保持。
3. 运行中拔掉对端：连续 ACK 失败后切到 10 秒静默周期。
4. 进入 10 秒无设备周期后再接入 CAN 对端：下一次 10 秒 RTC 唤醒的任一探测帧得到 ACK 后，应切回约 1 秒 RTC 周期。
5. 用示波器同时看 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL，确认收发器上电稳定时间和发送窗口满足芯片要求。

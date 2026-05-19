# STM32 CAN 低功耗配置与当前工程逻辑梳理

日期：2026-05-19

## 1. 目标与边界

本项目 CAN 需求：

- 低功耗优先，CAN 不应长期保持高功耗发送状态。
- 有设备时，至少 1s 发送一次 `0x02`。
- 无设备时，不能连续重发拉高功耗。
- 不能依赖 CAN 接收中断判断总线上是否有设备。
- 总线设备可能静默，不一定主动发送业务报文。
- 不再通过外部电源脚或收发器使能脚做周期性开关；当前实测这类控制反而导致功耗偏高。

因此推荐策略是：

- 外部 CAN 电源/收发器控制脚保持稳定，不做 1s 周期翻转。
- MCU 内部 bxCAN 控制器只在短发送窗口进入 Normal。
- 发送窗口结束后，请求 bxCAN Sleep，并关闭 CAN1 APB1 外设时钟。
- 发送使用单次发送，不启用自动重发。
- 用发送结果是否 ACK 成功作为“总线上是否存在至少一个正常 CAN 节点”的判断依据。
- 无设备时仍保留低占空比探测，否则在“不用中断、对方静默”的前提下无法自动发现设备重新接入。

## 2. 官方资料来源

主要官方资料：

- ST 官方 STM32F103 文档页：<https://www.st.com/en/microcontrollers-microprocessors/stm32f103/documentation.html>
- ST RM0008 Reference manual：<https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf>
- ST 低功耗模式说明：<https://community.st.com/t5/stm32-mcus/tips-for-using-stm32-low-power-modes/ta-p/621007>
- 本工程自带 ST 标准外设库：
  - `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_can.h`
  - `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_can.c`
  - `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h`

## 3. STM32F1 低功耗与 CAN 的关系

RM0008 对 STM32F10xxx 低功耗模式的核心描述：

- Sleep：CPU 时钟关闭，外设时钟通常仍运行，唤醒快，但外设仍可能耗电。
- Stop：大部分时钟停止，功耗更低，但唤醒后通常需要恢复系统时钟配置。
- Standby：1.8V 域关闭，功耗最低，但上下文丢失，需要按复位路径恢复。
- Run 模式下也可以通过降低系统时钟、关闭未使用 APB/AHB 外设时钟降低功耗。

对当前 CAN 场景，重点不是 MCU 全局 Stop/Standby，而是 CAN 外设本身的工作窗口：

1. 需要发送时，打开 CAN1 外设时钟并进入 Normal。
2. 发送一次帧，等待发送成功或失败。
3. 发送结束后取消残留邮箱，请求 CAN Sleep。
4. 再关闭 CAN1 APB1 时钟，减少外设动态功耗。

这个策略不会动外部电源脚，适合当前“控电源反而功耗高”的实测结果。

## 4. bxCAN 关键配置项

### 4.1 `CAN_NART`

ST 标准库中 `CAN_NART` 含义是 No Automatic Retransmission。对应寄存器位是 `CAN_MCR_NART`。

本工程低功耗场景必须启用：

```c
CAN_InitStructure.CAN_NART = ENABLE;
```

原因：

- 若 `CAN_NART = DISABLE`，无 ACK 时硬件会自动重发，直到成功、出错或进入错误状态。
- 总线上无设备时，自动重发会让 CAN 长时间保持活动，功耗升高。
- 启用 `NART` 后，每个发送请求只尝试一次，软件可以在发送失败后立即休眠。

当前工程已经配置为 `ENABLE`。

### 4.2 `CAN_AWUM`

`CAN_AWUM` 是 Automatic Wake-Up Mode，对应 `CAN_MCR_AWUM`。

当前项目建议保持：

```c
CAN_InitStructure.CAN_AWUM = DISABLE;
```

原因：

- 当前需求明确不能依赖中断判断设备存在。
- 对方设备可能静默，靠总线活动唤醒不可靠。
- 本工程采用 1s 周期主动短窗口发送/探测，不需要 CAN 自动唤醒。

如果未来要做“总线活动唤醒 MCU”，才需要重新评估 `AWUM`、RX 中断、EXTI/收发器唤醒脚和 MCU Stop 模式的组合。

### 4.3 `CAN_ABOM`

`CAN_ABOM` 是 Automatic Bus-Off Management，对应 `CAN_MCR_ABOM`。

当前工程设置：

```c
CAN_InitStructure.CAN_ABOM = DISABLE;
```

当前代码已有手动 Bus-Off 监控和恢复逻辑，因此保持 `DISABLE` 可以接受。低功耗策略下更关键的是：

- 不因无 ACK 自动高频重发。
- Bus-Off 恢复尝试也要低占空比。
- 不能在 Bus-Off 状态中持续占用发送窗口。

### 4.4 `CAN_OperatingModeRequest`

ST 标准库提供：

```c
CAN_OperatingModeRequest(CAN1, CAN_OperatingMode_Normal);
CAN_OperatingModeRequest(CAN1, CAN_OperatingMode_Sleep);
```

标准库实现会操作 `CAN_MCR_SLEEP`、`CAN_MCR_INRQ`，并等待 `MSR` 中的模式确认位。

低功耗建议：

- 进入低功耗前先取消所有 TX mailbox。
- 请求 `CAN_OperatingMode_Sleep`。
- 最好检查返回值或确认 `SLAK`，再关闭 `RCC_APB1Periph_CAN1` 时钟。
- 唤醒时先打开 CAN1 APB1 时钟，再请求 Normal。

### 4.5 CAN 外设时钟门控

RM0008 明确 Run 模式下可通过关闭未使用 APB/AHB 外设时钟降低功耗。

当前 CAN 发送窗口结束后关闭：

```c
RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, DISABLE);
```

这是当前项目更合适的低功耗手段。它不同于切断外部 CAN 收发器电源，不会引入外部电源脚翻转导致的额外功耗问题。

## 5. CAN ACK 与“总线上有设备”的判断

CAN 协议中 ACK 不是应用层回复。正常工作的 CAN 控制器只要正确接收到帧，就会在 ACK slot 应答。

因此在“设备可能静默”的情况下，判断总线上是否存在正常 CAN 节点，不能只看是否收到业务报文；更合适的是：

- 本机主动发送一帧。
- 若发送状态为 `CAN_TxStatus_Ok`，说明该帧至少被一个 CAN 节点 ACK。
- 若发送失败或一直 Pending 后超时，认为当前无可 ACK 的正常节点，立即取消邮箱并休眠。

限制：

- 如果对方 CAN 节点电源关闭、收发器 Standby、波特率不一致、总线物理层异常，都不会 ACK。
- 如果完全停止发送，且不用中断、对方也不主动发报文，则无法自动发现设备重新接入。
- 因此“无设备不发送”在工程上应理解为“不连续发送业务帧，只保留低占空比探测帧”。

## 6. 当前工程 CAN 逻辑

相关文件：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/Can_HDX.h`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/main.c`

### 6.1 初始化路径

主初始化调用：

```c
#ifdef __FUNC__CAN__
    InitCan();
#endif
```

`InitCan()` 当前流程：

1. 清 `Can_Status_Flag`。
2. 清 `CanTxType_Flag`。
3. 初始化 CAN GPIO。
4. 初始化 CAN NVIC。
5. 初始化 CAN1 控制器。
6. 初始化 CAN Filter。
7. 设置 `s_bCanLowPower = FALSE`。
8. 调用 `Can_EnterLowPower()`，开机后立即进入 CAN 低功耗状态。

### 6.2 CAN1 配置

当前 `InitCan_CAN1()` 关键配置：

```c
CAN_InitStructure.CAN_TTCM = DISABLE;
CAN_InitStructure.CAN_ABOM = DISABLE;
CAN_InitStructure.CAN_AWUM = DISABLE;
CAN_InitStructure.CAN_NART = ENABLE;
CAN_InitStructure.CAN_RFLM = DISABLE;
CAN_InitStructure.CAN_TXFP = DISABLE;
CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE);
```

这组配置适合当前低功耗方案：

- `NART=ENABLE`：无 ACK 不自动重发。
- `AWUM=DISABLE`：不依赖总线自动唤醒。
- `RX interrupt DISABLE`：不用中断判断设备。
- `Normal mode`：发送探测帧时需要真实上总线，不能用 LoopBack。

### 6.3 发送路径

`CAN_Tx_Data()` 当前流程：

1. 给标准 ID 加本机地址偏移。
2. 调用 `CAN_Transmit()` 申请邮箱并启动发送。
3. 如果没有邮箱，返回 `FALSE`。
4. 循环读取 `CAN_TransmitStatus()`。
5. `CAN_TxStatus_Ok` 返回 `TRUE`。
6. 若一直 Pending 到超时，调用 `CAN_CancelTransmit()` 取消邮箱。
7. 其它失败返回 `FALSE`。

这个函数现在承担两个职责：

- 发送 CAN 帧。
- 通过发送是否成功判断是否被 ACK。

这是当前“静默设备也要识别”的核心。

### 6.4 周期任务 `App_Can()`

当前 `App_Can()` 逻辑：

1. 由主循环周期调用。
2. 只在 `b1Sys10msFlag2` 到来时运行一次计数。
3. `cnt_send_0x02` 累计到 `CAN_0X02_SEND_PERIOD_TICKS`，即约 1s。
4. 未到 1s 时，调用 `Can_EnterLowPower()` 并返回。
5. 到 1s 后：
   - `Can_ExitLowPower()`
   - `Can_BusOFF_Monitor()`
   - `Can_PollReceive()`
   - `Can_TransmitDeal()`
   - `CAN_TX_0x02()`
   - 按发送结果更新 `sys_time.can_enable`
   - `Can_EnterLowPower()`

即当前设计已经是 1s 短窗口发送，然后立即回低功耗。

### 6.5 接收路径

当前不使用 RX 中断作为设备判断。

`Can_PollReceive()` 在发送窗口内轮询 FIFO0：

- 最多读取 3 帧。
- 每帧调用 `CAN_Receive()`。
- 设置 `b1Can_Received`。
- 更新 `sys_time.can_rcv_cnt`。
- 调用 `Can_ReceiveDeal()` 将收到的请求转换为待发送标志。

`Can_TransmitDeal()` 根据 `CanTxType_Flag` 发送对应报文，每次处理一个请求分支。

### 6.6 低功耗进入与退出

当前 `Can_EnterLowPower()`：

```c
sys_time.canPow_enable = false;
CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE);
CAN_CancelTransmit(CAN1, 0);
CAN_CancelTransmit(CAN1, 1);
CAN_CancelTransmit(CAN1, 2);
CAN_OperatingModeRequest(CAN1, CAN_OperatingMode_Sleep);
RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, DISABLE);
s_bCanLowPower = TRUE;
```

当前 `Can_ExitLowPower()`：

```c
sys_time.canPow_enable = true;
RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
CAN_WakeUp(CAN1);
CAN_OperatingModeRequest(CAN1, CAN_OperatingMode_Normal);
CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE);
s_bCanLowPower = FALSE;
```

关键点：

- 外部电源脚控制已注释：
  - `GPIO_M_STB`
  - `GPIO_CMNT_EN`
- 实际低功耗靠 bxCAN Sleep + CAN1 APB1 时钟关闭。
- `sys_time.canPow_enable` 现在更像调试状态标志，不应再作为真实电源开关语义使用。

## 7. 当前逻辑与官方建议的匹配度

匹配点：

- 已使用 `CAN_NART = ENABLE`，避免无 ACK 自动重发。
- 已禁用 RX FIFO 中断，符合“不用中断判断设备”。
- 已使用轮询接收，接收只发生在 CAN 活动窗口内。
- 已使用 CAN Sleep 和 CAN1 时钟门控降低功耗。
- 已避免在低功耗路径中翻转外部 CAN 电源/使能脚。
- 已用 `CAN_TX_0x02()` 发送结果更新 `sys_time.can_enable`，可表示 ACK 探测结果。

仍建议完善的点：

1. `Can_EnterLowPower()` 请求 Sleep 后，建议检查 `CAN_OperatingModeRequest()` 返回值，再关闭 CAN1 时钟。
2. `Can_ExitLowPower()` 建议检查 `CAN_WakeUp()` 和 `CAN_OperatingModeRequest(Normal)` 返回值，失败时不要继续发送。
3. `Can_TransmitDeal()` 和固定 `0x02` 在同一个窗口内可能连续发送两帧；如果极限功耗敏感，可以限制每个 1s 窗口最多发送一类帧，或者把响应优先、`0x02` 延后。
4. `CAN_TX_Test()` 仍是旧式等待逻辑，调试时可能绕过低功耗策略，建议不要在低功耗测试中调用。
5. `sys_time.canPow_enable` 命名容易误导，后续建议改为 `can_active_window` 或只作为调试观测标志。

## 8. 推荐配置表

| 项目 | 推荐值 | 原因 |
| --- | --- | --- |
| `CAN_NART` | `ENABLE` | 无 ACK 时只发送一次，避免自动重发拉高功耗 |
| `CAN_AWUM` | `DISABLE` | 当前不用中断/总线活动唤醒，采用 1s 主动窗口 |
| `CAN_ABOM` | 当前可保持 `DISABLE` | 已有手动 Bus-Off 监控；后续可按测试结果评估 |
| RX FIFO 中断 | `DISABLE` | 接收改为窗口内轮询 |
| CAN 工作模式 | `Normal` | ACK 探测必须真实上总线，不能使用 LoopBack |
| 发送超时 | 必须有限 | Pending 不能无限等待 |
| 超时处理 | `CAN_CancelTransmit()` | 释放邮箱，避免下一轮卡住 |
| 发送后处理 | CAN Sleep + CAN1 时钟关闭 | 降低 MCU 内部 CAN 外设功耗 |
| 外部电源/使能脚 | 不周期翻转 | 当前实测控电源反而功耗高 |

## 9. 测试建议

无设备测试：

- 观察 CAN TX 引脚每秒只有一次短活动。
- `CAN_Tx_Data()` 返回 `FALSE`。
- `sys_time.can_enable == false`。
- 发送窗口结束后 `RCC_APB1Periph_CAN1` 时钟应关闭。
- 功耗应回到接近 CAN 未发送状态。

有静默设备测试：

- 对方不发业务报文，只保持 CAN 控制器正常在线。
- 本机每秒发送 `0x02`。
- 由于对方 ACK，`CAN_Tx_Data()` 应返回 `TRUE`。
- `sys_time.can_enable == true`。
- 发送结束后仍应回到低功耗。

有主动报文设备测试：

- 发送窗口内 `Can_PollReceive()` 可读取 FIFO0。
- `Can_ReceiveDeal()` 置对应 `CanTxType_Flag`。
- `Can_TransmitDeal()` 在后续窗口回复。
- 验证回复帧不会导致 CAN 长时间保持唤醒。

功耗测试：

- 分别测量：
  - CAN 功能编译关闭或 `InitCan()` 后不发送。
  - 无设备，1s 单次探测。
  - 有静默设备，1s ACK 成功。
  - 有业务请求，窗口内响应 + `0x02`。
- 用示波器同步观察 TX、CAN_H/CAN_L 和电流波形，确认高功耗只出现在发送窗口。

## 10. 当前结论

当前工程的方向已经与 STM32 bxCAN 官方能力相匹配：通过 `NART` 单次发送、Sleep 模式、APB1 时钟门控和轮询接收实现低功耗 CAN。

后续优化不应再从“周期性控制 CAN 电源脚”入手，而应继续收紧软件发送窗口：

- 发送前唤醒。
- 单次发送。
- 失败立即取消。
- 发送后确认 Sleep。
- 关闭 CAN1 时钟。
- 保留低占空比 ACK 探测，避免设备静默时无法自动恢复通信。

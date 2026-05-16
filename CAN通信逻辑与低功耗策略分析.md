# CAN 通信逻辑与低功耗策略分析

## 1. 分析结论

当前 CAN 逻辑已经从旧的“收到标准帧请求后回复 0x00~0x11 数据帧”切换为 `feidao_can_send()` 周期广播状态机。主循环仍初始化 CAN 接收中断和过滤器，但 `App_Can()` 中的 `Can_ReceiveDeal()`、`Can_TransmitDeal()` 已被注释，因此当前运行逻辑以主动周期发送为主，基本不处理 CAN 请求帧。

当前策略理论上可以降低 CAN 通信功耗，核心原因有两个：

1. `GPIO_CMNT_EN` 只在发送窗口内拉到上电电平，发送完成后立即断电，显著降低 CAN transceiver 的静态待机电流。
2. `CAN_NART = ENABLE` 关闭 bxCAN 自动重发，无接收设备、无 ACK 时不会持续重发同一帧，避免“无人应答时一直占用总线和拉高功耗”。

但该策略降低的是外部 CAN transceiver 和总线发送窗口功耗，不是完整关闭 MCU 内部 CAN 外设功耗。`CAN1` 时钟仍保持打开，PA11/PA12 仍按 CAN 引脚配置；如果要进一步降低 MCU 侧功耗，需要在非发送窗口关闭 CAN 外设时钟或进入 CAN sleep，但会增加恢复和接收复杂度。

本次已针对问题 2、4、5 做了代码修复：

1. 入睡前统一调用 `Can_PrepareSleep()`，丢弃待发 mask、取消 mailbox、关闭 transceiver，避免低功耗流程在发送窗口中途切入。
2. 增加 `g_stCanErrorSnapshot`，在发送失败、发送超时、无 mailbox、BusOff 时记录 `LEC/REC/TEC/EWGF/EPVF/BOFF`，并统计 `ACK Error`、发送失败、超时、取消、无 mailbox、BusOff 次数。
3. `CAN_CancelTransmit()` 后等待 mailbox 变空，并清理 `RQCPx`，避免取消后 mailbox 状态残留影响下一次发送窗口。

注意：这次修复没有把 `ACK Error` 作为业务故障立即重发。原因是当前策略是低功耗周期广播，无接收设备时无 ACK 属于可接受场景；现在只是把它统计出来，便于区分“对端不在线”和“物理层异常”。

## 2. 相关代码入口

| 模块 | 位置 | 当前作用 |
| --- | --- | --- |
| 主循环 | `103 + 309/Project/Source/main.c` | 每轮调用 `App_Can()` |
| 初始化 | `103 + 309/Project/Source/Can_HDX.c` | `InitCan()` 初始化 GPIO、NVIC、CAN1、过滤器，最后关闭 transceiver |
| 发送状态机 | `103 + 309/Project/Source/Can_HDX.c` | `feidao_can_send()` 负责周期调度、上电、发送、等待完成、断电 |
| 供电控制 | `103 + 309/Project/Source/Can_HDX.c` | `feidao_can_power_on()` / `feidao_can_power_off()` 控制 `GPIO_CMNT_EN` |
| GPIO 定义 | `103 + 309/Project/Source/conf/conf_gpio.h` | `GPIO_CMNT_EN = GPIOB`，`PIN_CMNT_EN = GPIO_Pin_4` |
| 休眠 IO | `103 + 309/Project/Source/conf/conf.c` | 低功耗 IO 状态中也会把 `GPIO_CMNT_EN` 置为 `Bit_SET` |

## 3. 初始化逻辑

`InitDevice()` 在常规启动路径中执行：

```text
SystemInit()
InitDelay()
IsSleepStartUp()
jtag_disableAndConfIO()
InitNVIC()
InitIO()
InitSci()
InitSystemWakeUp()
InitE2PROM()
InitAFE1()
InitCan()
InitADC()
InitSci()
```

`InitCan()` 当前执行顺序：

```text
Can_Status_Flag.all = 0
CanTxType_Flag.all = 0
InitCan_GPIO()
InitCan_NVIC()
InitCan_CAN1()
InitCan_Filter()
feidao_can_power_off()
```

关键点：

- `InitCan_GPIO()` 配置 PA11 为 CAN RX 上拉输入，PA12 为 CAN TX 复用推挽输出。
- `InitCan_NVIC()` 使能 `USB_LP_CAN1_RX0_IRQn`，优先级为抢占 1、子优先级 1。
- `InitCan_CAN1()` 打开 `RCC_APB1Periph_CAN1`，配置 bxCAN。
- `InitCan_Filter()` 配置标准帧过滤器并分配到 FIFO0。
- 初始化最后调用 `feidao_can_power_off()`，把 `GPIO_CMNT_EN` 置为断电状态。

`GPIO_CMNT_EN` 当前硬件逻辑：

| GPIO 电平 | 含义 |
| --- | --- |
| `Bit_RESET` | CAN transceiver 上电 |
| `Bit_SET` | CAN transceiver 断电 |

## 4. CAN1 参数

当前 `InitCan_CAN1()` 的主要参数：

| 参数 | 当前值 | 影响 |
| --- | --- | --- |
| `CAN_TTCM` | `DISABLE` | 不使用时间触发通信 |
| `CAN_ABOM` | `DISABLE` | BusOff 不自动恢复，由软件监控恢复 |
| `CAN_AWUM` | `DISABLE` | 不使用 CAN 自动唤醒 |
| `CAN_NART` | `ENABLE` | 禁止自动重发，低功耗策略关键点 |
| `CAN_RFLM` | `DISABLE` | FIFO 溢出时新报文可覆盖旧报文 |
| `CAN_TXFP` | `DISABLE` | 发送优先级按报文 ID 仲裁 |
| `CAN_Mode` | `CAN_Mode_Normal` | 正常 CAN 模式 |
| `CAN_SJW` | `CAN_SJW_1tq` | 同步跳转宽度 1 tq |
| `CAN_BS1` | `CAN_BS1_5tq` | bit segment 1 为 5 tq |
| `CAN_BS2` | `CAN_BS2_2tq` | bit segment 2 为 2 tq |
| `CAN_Prescaler` | `4` | CAN 时钟分频 |

按当前 `system_stm32f10x.c`，非 value line 分支定义的是 `SYSCLK_FREQ_HSE = HSE_VALUE`，且 HSE 默认值为 8 MHz，`SetSysClockToHSE()` 配置 `PCLK1 = HCLK = 8 MHz`。因此当前源码对应的 CAN bit rate 为：

```text
bitrate = PCLK1 / CAN_Prescaler / (1 + BS1 + BS2)
        = 8 MHz / 4 / (1 + 5 + 2)
        = 250 kbit/s
```

注意：CAN 目标波特率固定为 250 kbit/s。工程中历史文档或源码注释若出现非 250 kbit/s 描述，均属于旧配置残留；后续调整系统时钟时，也必须按 250 kbit/s 重新计算 `BS1/BS2/Prescaler/PCLK1`。

## 5. 当前发送逻辑

`App_Can()` 当前逻辑：

```text
now_tick = SysTime_Get10msTickCount()

如果本轮有 10ms 系统标志：
    Can_BusOFF_Monitor()

feidao_can_send(now_tick)
```

旧逻辑：

```text
Can_ReceiveDeal()
Can_TransmitDeal()
```

目前已经被注释，所以旧的请求应答路径不再参与主流程。

### 5.1 发送状态机

`feidao_can_send()` 是非阻塞状态机，状态如下：

| 状态 | 行为 |
| --- | --- |
| `FEIDAO_CAN_POWER_IDLE` | 空闲；如果有待发报文，打开 transceiver 电源 |
| `FEIDAO_CAN_POWER_WAIT_STABLE` | 等待 transceiver 上电稳定，当前等待 10 个 10ms tick |
| `FEIDAO_CAN_POWER_TX_WAIT` | 等待当前 mailbox 发送完成、失败，或超时取消 |

关键时间参数：

| 宏 | 当前值 | 实际含义 |
| --- | --- | --- |
| `FEIDAO_CAN_POWER_STABLE_TICKS` | `10` | 上电后等待 100ms 再发第一帧 |
| `FEIDAO_CAN_TX_DONE_TIMEOUT_TICKS` | `20` | 单帧最多等待 200ms，超时取消发送 |
| `FEIDAO_CAN_PERIOD_1000MS_TICKS` | `100` | 1000ms 周期 |
| `FEIDAO_CAN_PERIOD_5000MS_TICKS` | `500` | 5000ms 周期 |

状态机流程：

```text
周期调度生成 pending mask
    ↓
空闲且 pending != 0
    ↓
GPIO_CMNT_EN = Bit_RESET，上电
    ↓
等待 100ms
    ↓
取 pending mask 中优先级最高的一帧发送
    ↓
轮询 CAN_TransmitStatus()
    ↓
OK / Failed / 200ms timeout
    ↓
还有 pending：继续发下一帧
没有 pending：GPIO_CMNT_EN = Bit_SET，断电
```

### 5.2 报文周期与顺序

当前周期调度由 `feidao_can_schedule_period_frames()` 维护两个 tick：

- 1000ms 到期：置位 `FEIDAO_CAN_MSG_VOLTAGE_CURRENT_1000MS` 和 `FEIDAO_CAN_MSG_SOC_1000MS`。
- 5000ms 到期：置位容量、SOH、版本、状态、出厂时间 5 类报文。

当前发送顺序固定由 `feidao_can_start_next_frame()` 决定：

| 顺序 | 函数 | 周期 | `chd_index` | ExtId |
| --- | --- | --- | --- | --- |
| 1 | `feidao_send_volage_current_1000ms()` | 1000ms | `0` | `0x14F80200` |
| 2 | `feidao_send_soc_1000ms()` | 1000ms | `2` | `0x14F80202` |
| 3 | `feidao_send_cap_5000ms()` | 5000ms | `1` | `0x14F80201` |
| 4 | `feidao_send_soh_5000ms()` | 5000ms | `3` | `0x14F80203` |
| 5 | `feidao_send_version_5000ms()` | 5000ms | `4` | `0x14F80204` |
| 6 | `feidao_send_status_5000ms()` | 5000ms | `5` | `0x14F80205` |
| 7 | `feidao_send_factory_time_5000ms()` | 5000ms | `8` | `0x14F80208` |

当 1000ms 和 5000ms 同时到期时，只上电一次，连续发送 7 帧，然后断电。

## 6. 报文内容概览

当前 `CAN_Battery_SendData_feidao()` 使用扩展帧：

```text
IDE = CAN_ID_EXT
RTR = CAN_RTR_DATA
ExtId = 0x14F80200 | chd_index
DLC = length
```

主要数据内容：

| 报文 | 数据内容 |
| --- | --- |
| voltage/current | `data[0..3]` 为总压，单位换算为 `u16VCellTotle * 10`；`data[4..7]` 为电流，放电为负、充电为正 |
| SOC | 充电状态、SOC、最高温度、剩余充电时间、电池主从类型、保留字段 |
| CAP | 当前能量容量、设计能量容量 |
| SOH | SOH、循环次数 |
| Version | 协议版本、软件版本 |
| Status | MOS/充放电工作状态、异常编码、容量字段 |
| Factory time | 出厂容量和年月日 |

所有多字节字段按 big endian 写入。

## 7. CAN 硬件自动收发边界

### 7.1 发送是否由硬件自动完成

结论：CAN 控制器会自动完成“总线协议层发送”，但不会自动决定业务何时发送、发送什么、失败后业务如何处理。

软件需要做的事情：

1. 填充 `CanTxMsg`，包括 `StdId/ExtId`、`IDE`、`RTR`、`DLC`、`Data[]`。
2. 调用 `CAN_Transmit()`，标准库会选择空 mailbox，写入标识符、DLC、数据，并置位 `TXRQ` 请求发送。
3. 如果业务关心发送结果，软件需要读取 `CAN_TransmitStatus()`、`CAN_TSR` 标志，或使能发送完成中断。
4. 如果发送长时间 pending，软件需要决定继续等、取消发送，还是进入错误恢复。

硬件自动做的事情：

| 硬件行为 | 说明 |
| --- | --- |
| 总线空闲检测 | 只有总线满足帧间隔和空闲条件后才开始发送 |
| 仲裁 | 多节点同时发送时，按 CAN ID 位级仲裁；仲裁失败不是协议错误 |
| 位时序输出 | 按 `SJW/BS1/BS2/Prescaler` 产生 CAN bit timing |
| CRC 生成 | 数据帧 CRC 由硬件生成，不需要软件计算 |
| ACK 检测 | ACK slot 没有其他节点拉 dominant 时，硬件判定 ACK error |
| 错误检测 | 硬件检测 bit/stuff/form/CRC/ACK 等错误 |
| 错误计数 | 硬件维护 `TEC/REC`，并进入 Error Warning、Error Passive、BusOff 状态 |
| 自动重发 | 只有 `CAN_NART = DISABLE` 时才会自动重发失败帧；当前工程为 `ENABLE`，即禁止自动重发 |

因此，`CAN_Transmit()` 不是“同步发送完成”函数，只是把报文交给 bxCAN mailbox。真正的发送结果要看后续 `CAN_TransmitStatus()`：

| 状态 | 含义 | 当前工程处理 |
| --- | --- | --- |
| `CAN_TxStatus_Pending` | mailbox 仍在发送/等待发送 | 最多等待 `FEIDAO_CAN_TX_DONE_TIMEOUT_TICKS` |
| `CAN_TxStatus_Ok` | 已成功发送并收到 ACK | 清 `RQCPx`，继续下一帧或断电 |
| `CAN_TxStatus_Failed` | 发送未成功，可能是 ACK/error/取消等 | 记录错误快照和 ACK 计数，清 `RQCPx`，继续下一帧或断电 |
| `CAN_TxStatus_NoMailBox` | 没有空 mailbox | 记录错误快照和无 mailbox 计数，上报 `ERROR_CAN`，当前批次丢弃并断电 |

当前低功耗状态机必须确认发送结果，原因不是 CAN 协议本身必须由软件确认，而是系统要知道“什么时候可以安全断开 transceiver 电源”。如果不确认就断电，可能在帧还没发完时关闭物理层，造成错误帧、无 ACK 或总线异常。

### 7.2 接收是否由硬件自动完成

结论：CAN 控制器会自动监听总线、校验帧并把通过过滤器的报文放入 FIFO；但软件必须及时取走 FIFO 数据，否则会满或溢出。

硬件自动做的事情：

| 硬件行为 | 说明 |
| --- | --- |
| 总线监听 | 正常模式下 bxCAN 持续采样 RX，引脚和 transceiver 必须处于工作状态 |
| 位填充检查 | 自动检查 bit stuffing 规则 |
| CRC 校验 | CRC 正确才认为帧有效 |
| ACK 参与 | 正常模式下成功接收有效帧时会参与 ACK；是否最终进入 FIFO 取决于过滤器 |
| 过滤 | 按 `InitCan_Filter()` 配置决定是否接收进 FIFO0/FIFO1 |
| FIFO 入队 | 通过过滤器的帧进入 FIFO，并置位 `FMP0/FMP1` |
| 接收中断 | 使能 `CAN_IT_FMP0` 后，FIFO0 有消息会触发 `USB_LP_CAN1_RX0_IRQHandler()` |

软件需要做的事情：

1. 配置过滤器，否则硬件不知道哪些 ID 应进入 FIFO。
2. 在中断或轮询中调用 `CAN_Receive()` 读取 FIFO。
3. 读取后释放 FIFO 输出 mailbox。标准库 `CAN_Receive()` 内部会写 `RFOM0/RFOM1` 释放 FIFO。
4. 处理 FIFO full/overrun。当前工程未使能 `CAN_IT_FF0/FOV0`，也没有专门处理 `CAN_FLAG_FOV0`。

当前低功耗策略下，transceiver 大部分时间被 `GPIO_CMNT_EN = Bit_SET` 断电。即使 bxCAN 外设还在正常模式，物理层断电后也无法可靠接收总线帧。因此“硬件能自动接收”的前提是 transceiver 上电、RXD 到 MCU 的通路有效、bit rate 正确、过滤器匹配。

### 7.3 对“是否不需要软件确认”的判断

不能简单理解为“不需要软件确认”。

| 场景 | 是否必须软件确认 | 原因 |
| --- | --- | --- |
| 只要求硬件尽力发送一帧，不关心结果 | 可以不等结果 | bxCAN 会自动仲裁、发帧、检测错误 |
| 需要知道对端是否 ACK | 必须检查发送状态或错误码 | 无 ACK 会表现为发送失败或 ACK error |
| 需要低功耗发送后立刻断电 | 必须确认完成/失败/超时 | 否则可能发送过程中断电 |
| 需要可靠送达 | 必须有软件重发/确认策略 | 当前 `CAN_NART = ENABLE`，硬件不会自动重发 |
| 需要接收报文 | 必须软件读取 FIFO | 硬件只负责入 FIFO，不会替业务解析 |
| 需要长期稳定运行 | 必须处理 FIFO 溢出、BusOff、错误计数 | 否则错误只能累积到不可通信状态 |

当前工程的选择是“低功耗优先”：禁止自动重发，并用软件状态机等待成功、失败或超时，然后尽快断电。这能降低无人 ACK 场景的功耗，但牺牲了硬件自动重发带来的送达可靠性。

## 8. 接收逻辑现状

接收相关代码仍存在：

- `InitCan_Filter()` 配置标准帧过滤器。
- `CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE)` 使能 FIFO0 消息挂号中断。
- `USB_LP_CAN1_RX0_IRQHandler()` 会读取 FIFO0 到全局 `RxMessage`，并置位 `Can_Status_Flag.bits.b1Can_Received`。
- `Can_ReceiveDeal()` 会根据 `RxMessage.StdId & 0x007F` 置位旧的 `CanTxType_Flag`。
- `Can_TransmitDeal()` 会按 `CanTxType_Flag` 回复旧 0x00~0x11 标准帧。

但当前 `App_Can()` 中 `Can_ReceiveDeal()` 和 `Can_TransmitDeal()` 被注释。因此从主流程看：

1. 中断可能仍然置位 `b1Can_Received`。
2. 主循环不消费这个标志。
3. 当前软件不会按收到的标准帧请求回复 0x00~0x11。
4. transceiver 大部分时间断电时，外部 CAN 帧也无法被正常接收。

因此，当前策略适合“本机主动周期广播”的产品形态，不适合“必须随时响应上位机 CAN 请求”的产品形态。如果后续要支持低功耗接收，需要设计独立的通信唤醒或 transceiver standby/wake 机制，而不能简单依赖当前 FIFO0 中断。

## 9. BusOff 与异常处理

`Can_BusOFF_Monitor()` 在 10ms 系统标志下执行，内部包含：

| 函数 | 作用 |
| --- | --- |
| `Can_BusOFF_FaultChk()` | 读取 `CAN1->ESR & CAN_ESR_BOFF`，检测到 BusOff 后置故障、进入初始化模式 |
| `Can_BusOFF_FaultTimeCtrl()` | BusOff 和恢复状态计时 |
| `Can_BusOFF_Recover()` | 前 10 次按 100ms 尝试恢复，之后按 1s 间隔恢复 |

当前 `CAN_ABOM = DISABLE`，所以 BusOff 不依赖硬件自动恢复，而是软件控制 `CAN1->MCR` 的 `INRQ` 位进出初始化模式。

本次修复后，`Can_BusOFF_FaultChk()` 在首次检测到 `CAN_ESR_BOFF` 时会先刷新 `g_stCanErrorSnapshot`，并累加 `u16BusOffCnt`。这样现场调试时可以同时看到 BusOff 标志、最近 `LEC`、`TEC/REC` 和 BusOff 发生次数。

从低功耗角度看，BusOff 路径不会无限阻塞发送流程。即使发送异常，`feidao_can_send()` 仍有 `Failed` 判断和 200ms 超时取消，最终会回到断电状态。

## 10. CAN 错误类型、判定与处理

### 10.1 CAN 协议层错误

bxCAN 的 `LEC` 是 Last Error Code，位于 `CAN_ESR_LEC`。标准外设库给出的错误码如下：

| 错误码 | 名称 | 判定条件 | 常见原因 | 处理方向 |
| --- | --- | --- | --- | --- |
| `CAN_ErrorCode_NoErr` | No Error | 最近没有错误 | 正常状态 | 无需处理 |
| `CAN_ErrorCode_StuffErr` | Stuff Error | 连续位违反 bit stuffing 规则 | 波特率不一致、采样点不合适、干扰、总线波形差 | 检查 bit timing、终端电阻、线束、EMI、地参考 |
| `CAN_ErrorCode_FormErr` | Form Error | CRC delimiter、ACK delimiter、EOF 等固定格式位错误 | 干扰、节点配置错误、总线被异常拉 dominant | 检查物理层、收发器、是否有异常节点 |
| `CAN_ErrorCode_ACKErr` | ACK Error | 发送帧 ACK slot 未被其他节点拉 dominant | 没有其他在线节点、对端未上电、波特率不一致、transceiver 断电、CANH/CANL 断线 | 低功耗广播可接受；可靠通信需确认对端、恢复供电、或软件重发 |
| `CAN_ErrorCode_BitRecessiveErr` | Bit Recessive Error | 本节点发送 recessive，但监测到 dominant | 总线短路、其他节点异常、仲裁/错误场景、收发器问题 | 检查总线短路、冲突节点、线束 |
| `CAN_ErrorCode_BitDominantErr` | Bit Dominant Error | 本节点发送 dominant，但监测到 recessive | TXD/RXD/收发器异常、总线断路、驱动能力不足 | 检查 transceiver、电源、CANH/CANL、终端 |
| `CAN_ErrorCode_CRCErr` | CRC Error | 接收帧 CRC 校验失败 | 干扰、波特率/采样点错误、线束质量差 | 优先检查波形、采样点、终端、线束 |
| `CAN_ErrorCode_SoftwareSetErr` | Software Set Error | 软件写入 LEC 触发 | 调试或清错误码过程 | 一般不作为现场总线错误判断 |

这些错误由硬件在位级自动检测，软件不需要逐 bit 判断。软件应读取 `CAN_GetLastErrorCode(CAN1)` 或 `CAN1->ESR & CAN_ESR_LEC` 来定位最近一次错误类型。

### 10.2 CAN 错误计数与状态

CAN 节点不是一次错误就退出总线，而是通过错误计数逐级降级：

| 状态 | 硬件标志 | 判定依据 | 含义 |
| --- | --- | --- | --- |
| Error Active | 无 `EWGF/EPVF/BOFF` | `TEC/REC` 较低 | 正常参与通信，发现错误会发 active error flag |
| Error Warning | `CAN_ESR_EWGF` / `CAN_FLAG_EWG` | `TEC` 或 `REC` 达到 warning 阈值 | 总线错误已偏多，应记录和诊断 |
| Error Passive | `CAN_ESR_EPVF` / `CAN_FLAG_EPV` | 错误计数超过被动阈值；标准库注释明确 `REC > 127` 进入 error passive | 节点仍可通信，但错误影响能力被限制 |
| BusOff | `CAN_ESR_BOFF` / `CAN_FLAG_BOF` | `TEC` 继续升高到 BusOff 条件 | 节点退出总线，不能继续正常收发 |

标准外设库提供：

- `CAN_GetReceiveErrorCounter(CAN1)`：读取 `REC`。
- `CAN_GetLSBTransmitErrorCounter(CAN1)`：读取 `TEC` 低 8 位。
- `CAN_GetLastErrorCode(CAN1)`：读取 `LEC`。

本次修复后，工程没有调用上述标准库 API，而是直接读取 `CAN1->ESR` 生成 `g_stCanErrorSnapshot`。两种方式本质上读取的是同一组硬件状态位；直接读 `ESR` 的好处是一次可以同时保存 `LEC/REC/TEC/EWGF/EPVF/BOFF`，便于低成本诊断。

当前快照触发点包括：

1. `CAN_TxStatus_Failed`。
2. 发送等待超时。
3. `CAN_Transmit()` 返回 `CAN_TxStatus_NoMailBox`。
4. 首次检测到 BusOff。

因此现在可以区分常见的无 ACK、发送失败、发送超时、无 mailbox 和 BusOff 事件，但仍没有对 FIFO full/overrun 做统计。

### 10.3 STM32 bxCAN 可用错误中断

标准库支持以下错误中断：

| 中断 | 含义 | 当前工程状态 |
| --- | --- | --- |
| `CAN_IT_EWG` | Error Warning | 未使能 |
| `CAN_IT_EPV` | Error Passive | 未使能 |
| `CAN_IT_BOF` | BusOff | 未使能 |
| `CAN_IT_LEC` | Last Error Code | 未使能 |
| `CAN_IT_ERR` | Error Interrupt 总开关 | 未使能 |

当前只使能了 `CAN_IT_FMP0` 接收 FIFO0 中断，没有实现 `CAN1_SCE_IRQHandler()`，也没有配置 `CAN1_SCE_IRQn`。因此错误状态不会通过中断上报，只能依赖发送状态机和 `App_Can()` 中的 10ms 轮询。

本次新增的 `g_stCanErrorSnapshot` 已覆盖轻量错误快照需求：

```c
struct CAN_ERROR_SNAPSHOT {
    UINT8 u8LastErrorCode;
    UINT8 u8ReceiveErrorCounter;
    UINT8 u8TransmitErrorCounter;
    UINT8 u8ErrorWarning;
    UINT8 u8ErrorPassive;
    UINT8 u8BusOff;
    UINT16 u16AckErrorCnt;
    UINT16 u16TxFailedCnt;
    UINT16 u16TxTimeoutCnt;
    UINT16 u16TxAbortCnt;
    UINT16 u16TxNoMailboxCnt;
    UINT16 u16BusOffCnt;
};
```

如果后续要做更强实时诊断，可以再启用 `CAN1_SCE_IRQHandler()` 记录错误中断；但不建议在错误中断里做复杂恢复，恢复策略仍应留在主循环或低频任务中。

### 10.4 当前工程各类错误的处理策略

| 错误/状态 | 当前是否处理 | 当前处理方式 | 风险 |
| --- | --- | --- | --- |
| ACK error | 已做轻量统计 | `CAN_NART = ENABLE` 后单次失败，不自动重发；`CAN_TxStatus_Failed` 时读取 `LEC`，若为 `CAN_ErrorCode_ACKErr` 则累加 `u16AckErrorCnt` | 只统计，不做业务重发或对端在线判定 |
| Stuff/Form/CRC/Bit error | 已做快照记录 | 发送失败、超时、BusOff 时保存最近 `LEC` 和 `TEC/REC` | 不是每一次错误都实时中断记录，可能只保留最近一次 |
| Error Warning | 快照记录，未做策略处理 | 快照触发时保存 `EWGF`，未使能错误中断 | 不能做到每次进入 warning 都实时上报 |
| Error Passive | 快照记录，未做策略处理 | 快照触发时保存 `EPVF`，未使能错误中断 | 不能做到每次进入 passive 都实时上报 |
| BusOff | 已处理并统计 | 10ms 轮询 `CAN_ESR_BOFF`，记录快照和 `u16BusOffCnt`，置 `ERROR_CAN`，进入初始化模式，按 100ms/1s 节奏尝试恢复 | 与 transceiver 断电策略耦合，需实测恢复期间 RX 是否能看到空闲总线 |
| FIFO full/overrun | 未处理 | 只使能 `FMP0`，未使能 `FF0/FOV0` | 如果未来恢复接收，可能丢帧且无统计 |
| No mailbox | 已处理并统计 | `CAN_Tx_Data()` 记录快照和 `u16TxNoMailboxCnt`，上报 `ERROR_CAN` | 当前批次可能丢帧 |
| 发送 pending 过久 | 已处理并统计 | 200ms 超时后记录快照和 `u16TxTimeoutCnt`，再取消发送 | 超时阈值仍需结合实际 bus load 验证 |
| 仲裁失败 | 作为发送失败处理 | `CAN_TxStatus_Failed` 统一处理 | 多节点高负载时，`CAN_NART = ENABLE` 可能降低送达率 |

### 10.5 ACK error 当前处理方式

你的判断在原始代码里是对的：软件没有直接读取或处理 ACK error，只是把发送失败当作普通失败处理。

本次修复后，ACK 本身仍然完全由 bxCAN 硬件检测，软件只在 `CAN_TxStatus_Failed` 时读取 `ESR.LEC`，如果最近错误为 `CAN_ErrorCode_ACKErr`，则累加 `g_stCanErrorSnapshot.u16AckErrorCnt`。发送超时也会保存 `ESR` 快照，但不累加 ACK 计数，避免把历史 `LEC` 误算成本次 ACK error。

当前链路更新为：

```text
CAN_Transmit()
    ↓
bxCAN 硬件发送帧，并在 ACK slot 检测是否有其他节点 ACK
    ↓
如果无 ACK：
    硬件记录 ACK error，更新 ESR.LEC/TEC
    因为 CAN_NART = ENABLE，不做硬件自动重发
    mailbox 结束本次请求
    ↓
feidao_can_send() 看到 CAN_TxStatus_Failed
    ↓
软件记录 ESR 快照，如果 LEC == CAN_ErrorCode_ACKErr 则累加 ACK 计数
    ↓
软件清理/取消当前发送，继续下一帧或关闭 transceiver
```

也就是说，软件现在“识别并统计 ACK error”，但仍不“业务处理 ACK error”。这符合当前低功耗广播目标：没接通信设备时，无 ACK 是可接受场景，软件不应该为了 ACK 无限重发。

但这不适合可靠送达场景。如果后续某些帧必须确认送达，需要增加上层策略：

1. 使用现有 `u16AckErrorCnt` 和 `u16TxFailedCnt` 判断对端是否长期不在线。
2. 按业务重要性决定是否重发，而不是打开硬件自动重发。
3. 记录连续 ACK error 次数，超过阈值再上报“对端不在线/总线异常”。
4. 区分 ACK error 和 CRC/stuff/form/bit error，避免把无人接收误判为物理层故障。

### 10.6 建议的错误处理分层

建议把 CAN 错误处理分成三层，不要把所有错误都等同于 `ERROR_CAN`：

| 层级 | 目标 | 建议动作 |
| --- | --- | --- |
| L1 发送窗口内错误 | 保证低功耗策略闭环 | 保留当前 `Ok/Failed/Timeout` 判断，确保最终断电 |
| L2 总线健康诊断 | 区分无人 ACK、波特率错误、干扰、物理层故障 | 记录 `LEC/REC/TEC/EWG/EPV/BOFF` 快照和计数 |
| L3 恢复策略 | 防止总线错误导致长期不可通信或高功耗 | BusOff 后分级退避恢复；必要时重新初始化 CAN 和 transceiver |

本次已经完成的最小改进：

1. 在 `Can_BusOFF_Monitor()` 内增加 `ESR` 快照，不改变现有发送策略。
2. 统计 `LEC == CAN_ErrorCode_ACKErr` 的次数，用于区分“未接设备”与“物理层错误”。
3. 在发送失败、发送超时、无 mailbox、BusOff 时记录 `EWGF/EPVF/BOFF` 状态。

仍建议后续保留的改进：

1. 如果恢复请求应答式 CAN 接收，同时使能并处理 `CAN_IT_FOV0` 或轮询 `CAN_FLAG_FOV0`。
2. 对关键业务帧增加上层重发，而不是关闭 `CAN_NART` 依赖硬件无限重发。这样可以保持低功耗可控。
3. 如需连续错误趋势，增加“连续 ACK error 次数”和“连续成功次数”，而不仅是累计计数。

## 11. 功耗降低原理

### 11.1 降低 transceiver 静态功耗

常见 CAN transceiver 在正常模式下有 mA 级工作电流，即使没有通信，只要 VCC/standby 处于工作态，也会消耗静态电流。当前策略把 transceiver 的工作时间限制在发送窗口：

```text
平均电流 ≈ I_off + duty * (I_on - I_off)
```

其中：

- `I_on`：transceiver 上电工作电流。
- `I_off`：`GPIO_CMNT_EN` 断电后的漏电或关断电流。
- `duty`：上电窗口占总时间的比例。

如果 `I_off` 明显低于 `I_on`，且发送窗口很短，则平均功耗会明显下降。

### 11.2 降低无人 ACK 时的重复发送功耗

CAN 正常发送需要其他节点在 ACK slot 给出 dominant ACK。如果总线上没有接收设备，发送节点会认为发送失败。

如果允许自动重发，bxCAN 会持续重试同一帧：

```text
无 ACK → 发送失败 → 自动重发 → 再次无 ACK → 继续重发
```

这会带来三个问题：

1. transceiver 长时间处于驱动总线状态。
2. MCU CAN mailbox 长时间 pending，软件难以及时断电。
3. 错误计数上升，可能进入 Error Passive 或 BusOff。

当前 `CAN_NART = ENABLE` 会设置 bxCAN 的 `NART` 位，禁止自动重发。无 ACK 时单次发送失败后，`CAN_TransmitStatus()` 可以进入 `CAN_TxStatus_Failed`，软件记录错误快照、清标志、发下一帧或断电。因此它能直接降低“未接通信设备时持续重发”造成的功耗。

### 11.3 降低阻塞等待造成的系统功耗浪费

旧需求中提到曾使用 `__delay_ms()` 等待 transceiver 稳定。当前实现改为 10ms tick 状态机，不在 `App_Can()` 内阻塞 CPU。

这有两个收益：

1. 主循环可以继续执行 AFE、SOC、存储、低功耗判断等任务。
2. 后续如果恢复 `MainLoop_EnterIdleSleep()`，非阻塞状态机更容易让系统在等待期间进入轻量 Sleep。

目前主循环尾部的 `MainLoop_EnterIdleSleep()` 仍被注释，所以这部分收益主要体现在结构上，尚未完全转化为 MCU 空闲功耗收益。

## 12. 当前策略的功耗收益估算

假设主循环及时运行，且 CAN 发送没有长时间 pending：

- 每 1000ms 至少上电一次。
- 每次上电固定等待 100ms。
- 1000ms 周期发送 2 帧。
- 5000ms 周期和 1000ms 周期重合时发送 7 帧，但仍只上电一次。

以 5 秒为观察窗口：

```text
上电窗口数量 = 5 次
固定稳定等待 = 5 * 100ms = 500ms
CAN 帧发送时间 = 若干 ms 级，通常小于固定 100ms 等待
理论 duty 约等于 10% 左右
```

如果总线异常导致每帧都走 200ms 超时，则最差 duty 会升高：

```text
普通 1s 窗口：100ms + 2 * 200ms = 500ms
5s 重合窗口：100ms + 7 * 200ms = 1500ms
5 秒内总上电时间约 4 * 500ms + 1500ms = 3500ms
duty 约 70%
```

异常最坏路径下 duty 会明显升高，因此无 ACK 判定、BusOff 恢复和 RTC 无设备 10 秒探测策略必须一起验证。真实收益必须以实际 transceiver 型号、`GPIO_CMNT_EN` 控制的电源拓扑、总线负载和主循环调度延迟实测为准。

## 13. 主要风险与边界

### 13.1 随时接收能力被牺牲

transceiver 断电期间无法接收 CAN 报文。当前旧接收处理也已被注释，所以系统不具备“随时响应 CAN 请求”的能力。

如果产品要求上位机随时读取或控制，需要选择以下方案之一：

1. transceiver 常上电，接受较高待机功耗。
2. 使用支持 standby/wake 的 CAN transceiver，让总线活动通过 wake pin 唤醒 MCU。
3. 保留低功耗广播策略，但定义明确的通信窗口，让上位机只在窗口内访问。

### 13.2 需要确认 transceiver 断电后的 IO 回灌

当前 PA12 仍是 CAN TX 复用推挽输出，PB4 控制 transceiver 断电。如果 transceiver VCC 被关闭，但 TXD/RXD 引脚没有断电容忍能力，可能通过 MCU IO 保护二极管回灌 transceiver，导致：

- 实际功耗降不下来。
- transceiver 处于半供电状态。
- 总线电平异常。

建议用原理图和 transceiver datasheet 确认：

- `GPIO_CMNT_EN` 是控制 VCC、EN，还是 standby pin。
- VCC 关闭时 TXD/RXD 是否 5V/3.3V tolerant 或允许输入高电平。
- CANH/CANL 在 transceiver 断电时是否高阻，是否影响总线。

### 13.3 上电稳定时间需要实测收敛

当前 `FEIDAO_CAN_POWER_STABLE_TICKS = 10`，即等待 100ms。这个值用于保证 RTC 唤醒后收发器重新上电足够稳定，功耗上偏保守。是否可以缩短取决于：

- transceiver VCC 上升时间。
- EN 到 Normal mode 的启动时间。
- TXD/RXD 到 CANH/CANL 可正常工作的延迟。
- 低温、低压、负载条件下的最差情况。

建议用示波器同时观察：

```text
GPIO_CMNT_EN
transceiver VCC / EN
MCU CAN_TX
CANH / CANL
对端接收时间戳或 ACK
```

从 100ms 开始向下收敛，例如 50ms、20ms、10ms，以第一帧稳定收到为准。

### 13.4 CAN bit rate 固定为 250 kbit/s

当前产品目标 CAN bit rate 明确为 250 kbit/s，源码组合在 8MHz PCLK1 下正好满足该目标：

```text
8 MHz / 4 / (1 + 5 + 2) = 250 kbit/s
```

如果后续恢复 72 MHz 或修改 APB1 分频，必须继续以 250 kbit/s 为目标重算 `BS1/BS2/Prescaler`。这个问题和低功耗无关，但会直接影响通信是否能被对端正确识别。

### 13.5 周期广播不会刷新外部通信活动计数

旧 `Can_TransmitDeal()` 中会在存在回复请求时执行 `RTC_ExtComCnt++`，用于低功耗逻辑识别外部通信活动。当前 `feidao_can_send()` 周期广播不再递增 `RTC_ExtComCnt`。

如果业务希望“CAN 正在周期通信时不进入休眠”，需要显式定义规则：是把周期广播视为内部任务，还是把收到对端请求/ACK 才视为外部通信活动。当前代码更接近前者。

### 13.6 低功耗切入发送窗口的问题

原始逻辑存在一个边界风险：`main.c` 中 `App_Can()` 先运行，`App_LowPowerProcess()` 后运行。如果 CAN 刚进入 `WAIT_STABLE` 或 `TX_WAIT`，低功耗流程本轮又决定入睡，可能在 mailbox pending 或 transceiver 上电期间切入 RTC/Reset sleep。

本次修复没有选择“只要 `Can_IsBusy()` 就禁止睡眠”，因为当前 CAN 周期广播每 1s 触发一次，单纯阻塞睡眠可能导致满足睡眠条件时被周期 CAN 任务反复推迟。当前采用的是更符合低功耗广播场景的策略：

```text
准备入睡
    ↓
Can_PrepareSleep()
    ↓
清 pending mask
取消 3 个 TX mailbox
等待 mailbox 变空
清 RQCPx
GPIO_CMNT_EN = Bit_SET，关闭 transceiver
    ↓
继续 RTC sleep 或 Reset sleep
```

接入点：

| 入口 | 修复动作 |
| --- | --- |
| `low_power_log_and_commit_sleep()` | reset sleep 前调用 `Can_PrepareSleep()` |
| `rtc_sleep_prepare_rtc()` | RTC sleep 配置 IO 前调用 `Can_PrepareSleep()` |
| `SleepDeal_Continue(mode)` | 旧路径或直接调用睡眠处理前兜底调用 `Can_PrepareSleep()` |

这样处理后，睡眠优先级高于周期广播：即将入睡时放弃未完成 CAN 批次，避免为了一个低优先级周期广播保持 transceiver 上电。

## 14. 建议验证清单

1. 验证 CAN bit rate：用 CAN analyzer 确认当前实际为目标 250 kbit/s。
2. 测 `GPIO_CMNT_EN` 电平：确认 `Bit_RESET` 上电、`Bit_SET` 断电与硬件一致。
3. 测 transceiver 电流：分别测常上电、当前状态机正常发送、无接收设备三种场景。
4. 测第一帧可靠性：重点验证上电 100ms 后第一帧是否稳定收到，并评估能否向 50ms/20ms 收敛。
5. 测无 ACK 场景：断开对端，确认不会连续重发，`GPIO_CMNT_EN` 会按期断电。
6. 测 BusOff 恢复：制造总线短路/错误帧，确认不会长时间保持 transceiver 上电。
7. 确认接收需求：如果需要随时 CAN 接收，当前策略需要补通信唤醒或接收窗口设计。
8. 验证错误快照：断开对端确认 `u16AckErrorCnt` 增加，强制发送拥塞确认 `u16TxTimeoutCnt/u16TxAbortCnt` 变化，制造 BusOff 确认 `u16BusOffCnt` 增加。
9. 验证低功耗切入：在 CAN 发送窗口内触发 RTC sleep / reset sleep，确认 `GPIO_CMNT_EN` 被拉到断电电平，mailbox 不残留 pending。

## 15. 后续优化方向

优先级建议：

| 优先级 | 事项 | 原因 |
| --- | --- | --- |
| P0 | 验证 CAN bit rate | 目标固定为 250 kbit/s，需用 CAN analyzer 确认实测值与源码计算一致 |
| P0 | 实测 `GPIO_CMNT_EN` 与 transceiver 电源拓扑 | 决定功耗收益是否真实成立 |
| P1 | 测量上电稳定时间并收敛 `FEIDAO_CAN_POWER_STABLE_TICKS` | 当前 100ms 偏保守，缩短后可进一步降低 duty |
| P1 | 明确是否需要随时接收 CAN | 当前策略偏广播，不适合实时请求应答 |
| P1 | 扩展错误统计到 FIFO overrun 和连续错误趋势 | 当前快照已覆盖发送失败/超时/BusOff，但接收溢出和连续趋势仍不足 |
| P2 | 非发送窗口关闭 CAN1 外设时钟或使用 CAN sleep | 可进一步降低 MCU 侧功耗，但需要处理恢复和错误状态 |
| P2 | 将 FEIDAO 周期广播与旧请求应答逻辑拆成独立模块 | 降低维护耦合，避免旧逻辑残留误导 |

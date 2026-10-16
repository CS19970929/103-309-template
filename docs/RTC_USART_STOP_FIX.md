# RTC STOP、USART 与唤醒中断整改说明

## 1. 问题结论

板上抓到 `USART1->SR = 0xE2`，即 `TXE | TC | RXNE | FE`。这不是收到了一帧
有效通信，而是 RX 电平在休眠切换期间形成了帧错误并同时置位 `RXNE`。

问题由以下条件共同造成：

1. 原理图中 USART1 使用 `PB6/SCI1-TX`、`PB7/SCI1-RX`，RX1 通路只有串联电阻、
   ESD/电容，没有直流上拉或下拉；通信盒未驱动时 RX 可能悬空。
2. 旧代码在 USART1/2、`RXNEIE`、`EIE` 和 NVIC 仍使能时，将 RX GPIO 攑成
   模拟输入。
3. `USART1_IRQHandler` 先把所有 `RXNE` 都当成有效通信，递增
   `RTC_ExtComCnt` 并写入 `g_irq_t`，之后才处理错误。
4. 旧错误处理使用 `USARTx->SR &= ~flag`。STM32F1 的 `ORE/NE/FE/PE` 不能这样
   清除，必须先读 `SR`，再读 `DR`。

因此，“盒子一直插着但从未通信”仍可能进入 USART IRQ，并被软件误判为外部通信
唤醒。

## 2. 原理图约束

依据 `HS-C073-V2.pdf`：

| 信号 | MCU 引脚 | 空闲状态与用途 | 固件配置 |
|---|---|---|---|
| `SCI1-RX` | PB7 | UART idle-high，板上无直流偏置 | RUN：`GPIO_Mode_IPU`；STOP：IPU + EXTI7 falling |
| `SCI2-RX` | PA3 | UART idle-high | RUN：`GPIO_Mode_IPU`；STOP：IPU + EXTI3 falling |
| `INT-WK-MCU` | PA0 | 板上 470 kΩ 下拉，active-high | `GPIO_Mode_IPD` + EXTI0 rising |
| `M-DI` | PA9 | 板上 1 MΩ 上拉，按键 active-low | `GPIO_Mode_IPU` + EXTI9 falling |
| `INT-WK-CMNT` | PB12 | 板上 100 kΩ 下拉，active-high | `GPIO_Mode_IPD` + EXTI12 rising |

`VCC33-U` 是受控通信电源。通信盒未上电或输出高阻时，MCU 内部上拉保证 UART RX
保持 idle-high，避免悬空噪声。

## 3. 当前低功耗状态流

### 3.1 进入 RTC STOP 的前置条件

`rtc_sleep()` 仅在以下条件都满足时进入周期 RTC STOP：

- 没有 Flash/EEPROM/事件记录等待提交；
- USART1/2 没有发送、半帧接收或尚未完成的发送；
- AFE、保护、加热、均衡、MOS 和电流状态允许休眠；
- RTC 初始化、Alarm 配置成功；
- PA0、PA9、PB12、PB7、PA3 没有处于有效唤醒电平；
- 没有已经锁存的唤醒原因。

### 3.2 STOP 前的顺序

`Sci_CommonUpperPrepareStop()` 在临界区内再次检查通信忙状态，然后按以下顺序处理：

1. 先把 PB7/PA3 的下降沿 EXTI 配好，使 USART 与 EXTI 的唤醒能力短暂重叠；
2. 关闭 `RXNEIE` 和 `EIE`；
3. 关闭对应 NVIC 通道；
4. 依次读 `SR`、`DR`，排空旧状态；
5. 关闭 USART；
6. 清除 NVIC pending；
7. 再由 `IOstatus_RTCMode()` 切换非唤醒 GPIO；
8. 配置 RTC Alarm，最后执行 STOP/WFI。

此顺序保证不会在 USART 仍工作时改变 RX 的电气模式。

### 3.3 周期唤醒与继续休眠

RTC Alarm 唤醒后恢复系统时钟，再开放全局中断。MCU读取 AFE 电流、电压、温度、
MOS 和保护状态：

- 无充放电、无异常、无外部唤醒：继续下一轮 RTC STOP；
- 有充放电、保护异常、按键、通信或其他外部唤醒：退出 RTC 循环并恢复 RUN 外设。

USART 在整个连续 RTC STOP 循环中保持关闭；只有退出循环后才重新初始化，避免每个
RTC 周期反复打开通信外设并产生假中断。GPIO/EXTI 唤醒源也只在进入连续 RTC 循环时
配置一次，周期采样后仅重新降低 AFE/I2C 等非唤醒引脚，避免清除刚锁存的外部边沿。

## 4. 真正串口通信如何唤醒

STOP 时 USART 时钟不可用，因此不能依赖 `USART1_IRQHandler` 直接接收第一字节。
固件把 PB7/PA3 临时配置为上拉 GPIO，以 UART 起始位的下降沿触发 EXTI：

- EXTI7 记录 `uart1_irq`；
- EXTI3 记录 `uart2_irq`；
- MCU 退出 STOP、恢复时钟和 USART；
- 触发唤醒的首字节可能丢失，通信主机必须按 Modbus/现有协议 timeout 重试整帧。

这一区分了两类事件：空闲高电平不会唤醒；真实起始位会唤醒，但第一帧用于唤醒而非
保证完整接收。`PB12/INT-WK-CMNT` 仍保留为独立的通信域唤醒源。

## 5. USART IRQ 处理规则

USART1/2 IRQ 现在只做以下短路径：

1. 读取一次 `SR`；
2. 若有 `RXNE` 或错误，再读取一次 `DR`；
3. 若 `ORE/NE/FE/PE` 任一置位，丢弃该字节、复位当前半帧、错误计数加一并返回；
4. 只有无错误的 `RXNE` 才进入协议收字节状态机；
5. 只有有效字节才递增 `RTC_ExtComCnt` 并记录 UART 唤醒原因。

USART3 当前产品未启用，但相同的错误标志清除和 RX 边界保护也已修正，防止以后启用时
复制同类问题。

## 6. 中断配置原则

- 本工程使用 `NVIC_PriorityGroup_1`，抢占优先级有效范围为 0..1。USART 原先配置 3
  不符合该分组语义，现统一为 preemption 1、subpriority 3。
- 初始化顺序为：关闭 NVIC、配置 GPIO、配置外设、清旧状态、打开本地中断、最后打开
  NVIC。
- 休眠唤醒源配置前先关闭对应 EXTI/NVIC、配置确定的上下拉、清 `EXTI->PR` 与 NVIC
  pending，再使能。
- 多个 IRQ 可能同时到达，`rtc_sleep_record_wakeup()` 只保留第一个原因，避免后来的
  中断覆盖真正的首个唤醒源。
- ISR 只做寄存器清理、字节搬运、计数和事件记录；协议解析、保护判断和外设恢复留在
  主流程。

## 7. 单步调试 HardFault

现场单步复现时：

- `CFSR = 0x00020000`：`UsageFault.INVSTATE`；
- `HFSR = 0x40000000`：可配置 Fault 被升级为 HardFault；
- 全速连续运行未出现同样 HardFault；
- 故障发生在 STOP 返回、异常进出和 `POP {...,PC}` 附近。

这与调试器在异常返回边界单步时破坏或错误恢复 EPSR/ICI/IT 状态相符。全速运行只表现为
假唤醒，是独立的 USART/GPIO 配置问题；单步才出现的 `INVSTATE` 更像调试器异常返回边界
效应，不能仅凭原来的空循环 HardFault handler 判断业务代码地址。

现在 `HardFault_Handler` 会在 `g_hardfault_record` 中保存：

- 堆栈中的 R0-R3、R12、LR、PC、xPSR；
- `CFSR/HFSR/DFSR/MMFAR/BFAR/AFSR/SHCSR/ICSR`；
- 当前唤醒原因、USART1/2 SR、EXTI PR、RTC CRL。

Keil 调试时直接观察 `g_hardfault_record`。建议不要在异常返回指令或 STOP 返回的
`POP {...,PC}` 上逐条单步；在 `Sys_StopMode()` 返回后的 C 语句设置断点，再查看记录。

## 8. 验证边界与板上验收

已提供的软件检查：

- `python -B tests/test_uart_stop.py`
- `python -B tests/test_sleep.py`
- `python -B tests/test_rtc_gpio.py`
- ARMCC Target 1 全部 61 个对象编译并链接 AXF，不执行 HEX/BIN 转换，不烧录。

仍需在目标板完成：

1. 通信盒连接 USART1、保持静默，连续观察多个 RTC 周期，不得出现 UART/EXTI7 唤醒；
2. 在睡眠期间发送报文，确认 EXTI7 唤醒，主机重试后通信成功；
3. 对 USART2 重复上述测试；
4. 分别验证 PA0、PA9、PB12 的极性和唤醒原因；
5. 测量 RUN、RTC STOP、盒子连接/断开四种情况下的电流；
6. 全速运行和“STOP 返回后断点”两种调试方式观察 HardFault 记录。

只有完成这些板上测试，才能把源码修复结论升级为硬件行为已验证。

## 9. 后续避免同类问题的检查表

- 先确认引脚在原理图上的空闲电平、外部偏置和供电域；
- GPIO 改模拟输入前，先关闭所属外设的本地中断、NVIC 和外设本身；
- 按 Reference Manual 规定的读写序列清外设状态，不凭直觉对只读状态位写 0；
- STOP 前在临界区最后检查一次 busy、pending 和有效电平；
- 唤醒后先恢复时钟，再开放中断，最后恢复外设；
- 区分“唤醒帧”和“业务帧”，明确是否需要协议重试；
- 给 HardFault 留下寄存器和堆栈证据，不使用只有 `while (1)` 的处理器。

# RTC STOP、USART 与唤醒中断整改说明

## 1. 问题结论

板上抓到 `USART1->SR = 0xE2`，即 `TXE | TC | RXNE | FE`。这不是收到了一帧
有效通信，而是 RX 电平在休眠切换期间形成了帧错误并同时置位 `RXNE`。

最初的 USART 假唤醒由以下条件共同造成：

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

后续板上复测还发现了三个独立问题，它们会让新程序看起来“始终进不了 RTC 低功耗”：

1. `IOstatus_RTCMode()` 会关闭 `C33V/VCC33-U` 通信电源，掉电后的 PB7/PA3 不再是
   可靠的 UART idle 电平；把它们作为 RTC 模式的最终电平门禁会直接拒绝 STOP。
2. 通信电源切换后，PB12/`INT-WK-CMNT` 会出现短高脉冲。条件断点确认该脉冲来自
   `sleep_has_active_wakeup()` 的电平复核，而不是 USART 数据，也不是 EXTI ISR。
3. `Sys_StopMode()` 返回时外层仍设置了 `PRIMASK`，旧代码先恢复中断再立即检查
   `is_rtc_wakekup`，可能在 `RTCAlarm_IRQHandler` 执行前就误判为非 RTC 唤醒。

当前实现把 UART RX 从 RTC 唤醒源中移除，在通信电源切换后等待有界 5 ms，再清除并
使能 PB12 EXTI；STOP 返回后在解除 `PRIMASK` 前同步锁存 RTC Alarm 标志。无有效唤醒
原因的 WFI 返回会重新进入 STOP，不再无故恢复全速 RUN。

## 2. 原理图约束

依据 `HS-C073-V2.pdf`：

| 信号 | MCU 引脚 | 空闲状态与用途 | 固件配置 |
|---|---|---|---|
| `SCI1-RX` | PB7 | UART idle-high，通信电源关闭后电平无效 | RUN：`GPIO_Mode_IPU`；RTC STOP：`GPIO_Mode_AIN`，不作为唤醒源 |
| `SCI2-RX` | PA3 | UART idle-high，通信电源关闭后电平无效 | RUN：`GPIO_Mode_IPU`；RTC STOP：`GPIO_Mode_AIN`，不作为唤醒源 |
| `INT-WK-MCU` | PA0 | 板上 470 kΩ 下拉，active-high | `GPIO_Mode_IPD` + EXTI0 rising |
| `M-DI` | PA9 | 板上 1 MΩ 上拉，按键 active-low | `GPIO_Mode_IPU` + EXTI9 falling |
| `INT-WK-CMNT` | PB12 | 板上 100 kΩ 下拉，active-high | `GPIO_Mode_IPD` + EXTI12 rising |

`VCC33-U` 是受控通信电源。RUN 状态下 MCU 内部上拉保证 UART RX 在通信盒未驱动时
保持 idle-high；RTC STOP 为降低功耗会关闭该供电域，因此 PB7/PA3 必须改为模拟输入，
不能再依赖 UART RX 电平唤醒。

## 3. 当前低功耗状态流

### 3.1 进入 RTC STOP 的前置条件

`rtc_sleep()` 仅在以下条件都满足时进入周期 RTC STOP：

- 没有 Flash/EEPROM/事件记录等待提交；
- USART1/2 没有发送、半帧接收或尚未完成的发送；
- AFE、保护、加热、均衡、MOS 和电流状态允许休眠；
- RTC 初始化、Alarm 配置成功；
- PA0、PA9、PB12 没有处于有效唤醒电平；
- 没有已经锁存的唤醒原因。

### 3.2 STOP 前的顺序

`sleep_run_rtc()` 在临界区内再次检查通信忙状态，然后按以下顺序处理：

1. `Sci_CommonUpperPrepareStop()` 再检查 TX、半帧 RX 和错误状态；
2. 关闭 `RXNEIE/EIE`、USART NVIC 和 USART 外设，并按 `SR` 后 `DR` 的顺序排空状态；
3. `IOstatus_RTCMode()` 关闭通信供电域，把 PB7/PA3 等非唤醒 GPIO 改为模拟输入；
4. 等待固定 5 ms，使板上通信检测电路的切换瞬态结束；
5. `InitWakeUp_RTCMode()` 只配置 PA0、PA9、PB12，并在清 pending 后使能 EXTI；
6. 配置 RTC Alarm，最后复核 busy、有效唤醒电平和已锁存原因；
7. 执行 STOP/WFI，返回后先调用 `RTC_LatchAlarmWake()`，再恢复 `PRIMASK`。

此顺序保证不会在 USART 仍工作时改变 RX 的电气模式，也不会把本机受控电源切换产生的
PB12 瞬态误判为外部通信唤醒。`EXTI_InitTypeDef.EXTI_Mode` 现在每次都显式赋值，避免局部
结构体未初始化导致随机 EXTI 配置。

### 3.3 周期唤醒与继续休眠

RTC Alarm 唤醒后恢复系统时钟，再开放全局中断。MCU读取 AFE 电流、电压、温度、
MOS 和保护状态：

- 无充放电、无异常、无外部唤醒：继续下一轮 RTC STOP；
- 有充放电、保护异常、按键、通信或其他外部唤醒：退出 RTC 循环并恢复 RUN 外设。

USART 在整个连续 RTC STOP 循环中保持关闭；只有退出循环后才重新初始化，避免每个
RTC 周期反复打开通信外设并产生假中断。GPIO/EXTI 唤醒源也只在进入连续 RTC 循环时
配置一次，周期采样后仅重新降低 AFE/I2C 等非唤醒引脚，避免清除刚锁存的外部边沿。

## 4. RTC 模式为什么不能直接用 USART1 唤醒

STOP 时 USART 时钟不可用，而本板在 RTC 低功耗中还会关闭 `C33V/VCC33-U`。这意味着
PB7/PA3 所连接的通信域已经掉电，UART RX 的 idle/high、起始位/low 都不再可靠。仅通过
软件改 EXTI 边沿无法同时满足“关闭通信电源降低功耗”和“USART1 任意首字节立即唤醒”。

当前策略是：RTC 模式禁用 EXTI3/7，保留一直供电的 PA0、PA9、PB12 唤醒，并由 RTC
周期唤醒主动检查充放电、AFE 和保护状态。USART 只在退出连续 RTC 循环后恢复。若产品
必须支持串口首字节立即唤醒，需要硬件提供一直供电的独立唤醒线，或调整电源策略使 RX
接收域在 STOP 中保持供电；这属于硬件/功耗需求变更，不能靠当前软件安全补偿。

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

这些寄存器说明 CPU 遇到了非法执行状态，但不能单凭 `INVSTATE` 认定调试器破坏了
EPSR/ICI/IT，也不能排除堆栈/异常返回或 STOP 指令取指问题。全速假唤醒与单步 Fault
需要分别验证；异常源指令应由堆栈 PC/LR 和反汇编定位。

现在 `HardFault_Handler` 会在 `g_hardfault_record` 中保存：

- 堆栈中的 R0-R3、R12、LR、PC、xPSR；
- `CFSR/HFSR/DFSR/MMFAR/BFAR/AFSR/SHCSR/ICSR`；
- 当前唤醒原因、USART1/2 SR、EXTI PR、RTC CRL。

Keil 调试时直接观察 `g_hardfault_record`。建议不要在异常返回指令或 STOP 返回的
`POP {...,PC}` 上逐条单步；在 `Sys_StopMode()` 返回后的 C 语句设置断点，再查看记录。

## 8. 验证边界与板上验收

已完成的软件检查：

- `python -B tests/test_uart_stop.py`
- `python -B tests/test_sleep.py`
- `python -B tests/test_rtc_gpio.py`
- ARMCC Target 1 编译并链接 AXF：`0 Error(s), 0 Warning(s)`；构建时禁用了 post-build
  BIN 转换，已有 BIN 的时间和大小未变化。

已完成串口盒连接、全程无通信场景的板上验证：

- 修复前条件断点抓到 `source=rs485_irq`，调用者为 `sleep_has_active_wakeup()`；读取后
  PB12 已恢复低电平，证明是短瞬态而不是持续外部通信。
- 修复后自由运行 50 秒再附着 ST-Link，PC 停在 `PWR_EnterSTOPMode().__WFI()`，
  `s_sleep_mode=HICCUP_MODE`、`s_common_usarts_stopped=1`、`g_irq_t=NO_IRQ`、
  `RTC_ExtComCnt=0`、`g_hardfault_record.magic=0`。
- 第一次读取 `rtc_cnt=17`，继续运行 25 秒后为 `rtc_cnt=28`，说明多个 RTC 周期持续
  唤醒、检测并重新进入 STOP，而非偶然停住。
- 已使用最新 AXF 烧录，Keil Flash 日志为 `Erase Done. Programming Done. Verify OK.`。

仍需在目标板完成以下产品验收，不把尚未实测项写成已验证：

1. 实际接入充电器和负载，验证周期采样发现充/放电后退出 RTC；
2. 分别验证 PA0、PA9、PB12 的物理极性、脉宽和 `g_irq_t`；
3. 验证 RTC 期间的通信产品需求；当前设计明确不支持掉电 UART RX 直接唤醒；
4. 测量 RUN、RTC STOP、盒子连接/断开四种情况下的电流；
5. 在 Keil 使用“STOP 返回后断点”方式复查 HardFault 记录，不在异常返回指令上单步。

当前“串口1盒子持续连接但不通信时，MCU 可连续 RTC STOP”已经由板上状态验证；充放电
退出、其他唤醒源和最终功耗仍以以上实测为准。

## 9. 后续避免同类问题的检查表

- 先确认引脚在原理图上的空闲电平、外部偏置和供电域；
- GPIO 改模拟输入前，先关闭所属外设的本地中断、NVIC 和外设本身；
- 按 Reference Manual 规定的读写序列清外设状态，不凭直觉对只读状态位写 0；
- STOP 前在临界区最后检查一次 busy、pending 和有效电平；
- 唤醒后先恢复时钟，再开放中断，最后恢复外设；
- 区分“唤醒帧”和“业务帧”，明确是否需要协议重试；
- 给 HardFault 留下寄存器和堆栈证据，不使用只有 `while (1)` 的处理器。

## 10. 框架版本

后续实现已把运行态 RTC 休眠迁入 `low_power.c` 状态机：PB12 的固定关中断延时替换为
中断开启的非阻塞稳定检查；支持多源记录、block/exit/fault 分离和 HardFault 上下文。
最新配置、移植说明和板上证据以 [LOW_POWER_FRAMEWORK.md](LOW_POWER_FRAMEWORK.md) 为准，
本文件前述验证数字属于早期修复版本，不是后续版本的编译或硬件结论。

# MCU 中断与 STOP 观察

Keil Watch 添加 `sys_time.irq`，按资源展开即可。`Project_Config.h` 的 `PROJECT_CFG_IRQ_DEBUG_ENABLE` 默认 1，Release/Debug 均采集；设为 0 后结构体和采集代码均不编译。Debug 目标不再单独覆盖这个开关。无 Flash 日志、格式化打印或事件环，所有计数为上电以来的 `uint32_t`，溢出按 2^32 回绕；STOP 和外设恢复不清零，MCU 复位会清零。

| Watch 路径 | 含义 |
| --- | --- |
| `sys_time.irq.exti` | 各 EXTI 向量入口次数，以及实际处理的 0/2/3/5/6/7/9/12/13/17 线次数；共享向量可同时处理多条线 |
| `sys_time.irq.uart1` / `uart2` / `uart3` | IRQ 入口、实际 RXNE/TXE/TC 分支、使能 IDLEIE 时的 IDLE 观察、ORE/NE/FE/PE 错误次数 |
| `sys_time.irq.uart1.last_error_sr` | 最近一次错误 SR，后续正常收发不会覆盖；`last_sr/last_cr1/last_cr3` 为最近 ISR 入口快照，其余端口相同 |
| `sys_time.irq.rtc` | RTC 与 RTCAlarm 入口、秒标志、ALR 标志处理次数、OW 标志观察次数，以及入口 CRL/CRH |
| `sys_time.irq.tim3` | TIM3 入口和有效更新中断次数；正常运行更新约 100 次/秒 |
| `sys_time.irq.can` | CAN FIFO0 IRQ 入口、读出帧数和最近 RF0R；帧数与入口数不同 |
| `sys_time.irq.core` | SysTick、NMI、HardFault、MemManage、BusFault、UsageFault、SVC、DebugMon、PendSV |
| `sys_time.irq.unhandled` / `last_unhandled_vector` | 启动文件默认处理函数捕获的未实现向量；保留原来停留/复位行为 |

当前实际启用 USART1/2、TIM3、RTC/RTCAlarm、CAN FIFO0 和相应 EXTI 唤醒。USART3 与未配置的 EXTI 线通常为 0，保留已有可选处理函数的观察。ADC 为轮询采样，AFE 使用软件 I2C；未启用 ADC/I2C/SPI/DMA 中断，没有为了观察而开启它们。SysTick 用于轮询延时，正常情况下其中断计数为 0。UART IDLEIE 本来关闭，统计不会将其打开。

调试低功耗优先看 `sys_time.irq.stop`：

- `entries/returns`：STOP 调用尝试和 WFI 返回次数，不能单独证明 MCU 实际停留了多久。
- `exti_pr/exti_imr`、`pending_nvic[2]`、`enabled_pending_nvic[2]`：WFI 返回后、恢复系统时钟及执行 ISR 之前的挂起快照。后者为 ISPR & ISER，排除未使能的 NVIC 挂起位；EXTI 有效线可用 PR & IMR 判断。
- `rtc_crl/rtc_crh`、`scb_icsr`、`gpioa_idr/gpiob_idr`：同时保留 RTC 标志、核心挂起状态及开关/充电/UART 唤醒引脚所在端口电平。
- `first_vector/last_vector`、`serviced_nvic[2]`：本次 STOP 返回、恢复时钟后，首次/最后记录到的可屏蔽中断和所有已记录外设向量。服务位图位号为 CMSIS `IRQn`，第二个字从 IRQ32 开始；向量号为 IRQn + 16。
- `active`：观察窗口，从 STOP 前屏蔽中断开始，到恢复时钟、开放中断并处理挂起 ISR 后结束。下一次 STOP 会覆盖本次快照，累计计数继续保留。日常 TIM3 中断不会覆盖已结束窗口的唤醒快照。

| 向量号 | 来源 |
| --- | --- |
| 15 | SysTick |
| 19 / 57 | RTC / RTCAlarm |
| 22 / 24 / 25 | EXTI0 / EXTI2 / EXTI3 |
| 39 / 56 | EXTI9_5 / EXTI15_10 |
| 53 / 54 / 55 | USART1 / USART2 / USART3 |
| 45 / 36 | TIM3 / CAN FIFO0 |

例如只有 `rtc.irq/second` 持续增长、STOP 快照出现 RTC 的 NVIC 位而无 EXTI17，可定位为 RTC 秒中断触发；目前 `RTC_DisableSecondInterrupt` 中禁用秒中断的语句本就被注释，统计如实反映这一状态。本次不修改唤醒源或休眠策略。UART RX EXTI、串口错误和 WFI 返回都只是候选活动；是否接受为有效唤醒仍由原协议校验/开关/充电判断决定，结合 `g_stLowPowerRtcStatus.mode` 判断是否恢复 RUN。多个源同时挂起或存在中断嵌套时，`first_vector` 表示首个被记录的向量，不代表电气信号出现的先后。

旧字段 `sci1_irq_cnt/sci2_irq_cnt/sci3_irq_cnt` 对应新 `uart1/2/3.irq`；`cnt_PA0_irq/cnt_bms1_keyirq` 对应 `exti.line0/line9`。旧 `rtc_alm_cnt` 是 ALR/EXTI17 的混合次数，新版分别观察 `rtc.alarm_flag`、`exti.line17`；这些是标志被处理/观察的次数，不将多个向量对同一标志的观察误称为独立硬件事件。异常路径只直接更新各自 RAM 计数，再执行已有故障处理；复位后原因仍通过已有备份域机制读取。

验证：`python tools/test_irq_debug.py` 运行实际 ISR、计数及 STOP 代码，覆盖开/关宏、共享 EXTI 多线、RTC 双向量、NVIC 两字位图、PRIMASK 保留、计数回绕、UART 分发、CAN 多帧、异常与未处理向量，以及先捕获挂起位再执行 ISR 的顺序。`python tools/test_uart_protocols.py` 运行实际 UART 收发代码，覆盖三个端口同时出现 ORE/NE/FE/PE、IDLE 使能条件、RX/TX/TC 和错误快照。工程编译仅临时 AXF/MAP，不生成 BIN/HEX，不代表实板唤醒时序或功耗验收。

本轮以上测试及保护、两种开关模式回归通过。ARMCC 5.06 update 7 对 Release/Debug 分别验证观察开/关，共四组，均为 0 错误、0 警告。开启时 Code 分别为 39776/48556 字节，RO/RW/ZI 均为 3196/1424/5856 字节；关闭时 Code 分别为 38608/47276，RO/RW/ZI 为 3196/1040/5856。观察结构体为 384 字节；合并旧 24 字节计数字段后，当前配置相对改动前净增 360 字节 RAM。构建及测试文件统一位于用户临时区，未生成固件镜像或连接实板。

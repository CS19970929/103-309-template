# 2026-10-20 低功耗修复与验收

基线 `b826cc0`，只提交本轮文件；保留用户对 SNum、EEPROM 标识和 Keil 配置的改动。
协议地址、报文、缩放、OTHER_ELEMENT 结构和 EEPROM/Flash 地址不变。

## 第一轮：进入策略与事件保留

- 普通 RTC 进入和巡检采用相同的非零报告电流判据（100 mA/LSB，沿用标定/死区）。
- `u16Sleep_TimeRTC` 非零时按原字段分钟单位执行，使用 32 位秒数；零保持现有板级空闲门槛，兼容已存默认零值。不写回、迁移或重置参数。
- STOP 恢复只重配充电检测硬件，不清 ChargerLoad_Func 中的事件和保护状态；冷启动清零保持不变。
- `test_low_power_policy.py` 覆盖 500 mA、30 分钟参数、零值兼容及恢复保留事件。修复前可复现失败。

## 硬件验证边界

本轮用户确认未连接板子和 ST-Link。OpenOCD 返回 `open failed`；不得把 host/编译结果当成下载、实板唤醒或功耗验证。
当前 EEPROM 标识为用户未提交的 `0x6701`。如果设备保存其他标识，原冷启动逻辑会恢复默认参数；实际下载前必须读取/确认设备数据，禁止为本修复擅自清 EEPROM。

## 第二轮：RTC 安全采样与 ADC 失败处理

- RTC 唤醒后启动 AD-EN 和必要 ADC/DMA/TIM2；TIM3 仅为局部非阻塞采样期限提供时间基准，普通任务仍被框架门控。
- 偏置稳定 5 ms 后丢弃旧转换，再取 4 个新 DMA 样本；100 ms 无结果或备用 RTC 超时则退出。没有增加 DelayMs。
- ADC 校准等待均有独立轮询预算，冷启动与恢复共用；TIM2 分频按实际 APB1 时钟计算，不再假设 72 MHz。
- RTC 检查新鲜 MOS/AFE 温度、传感器有效性、单体/总压/压差以及系统安全错误。触及二级边界先退出 RUN，原保护滤波、故障锁存及 MOS 策略保持原所有者；不在 RTC 伪造 10 ms 调度。
- ADC 失败置系统错误并禁止软件重新打开 MOS。关停顺序包含 TIM2、ADC 触发、DMA 和 ADC 本体。
- 原有非 UTF-8 文件仅做定位明确的函数/语句机械替换，保留其他字节与原换行，避免整文件转码。
- `test_low_power_adc.py` 验证校准失败上限、稳定期、4 次新转换、异常 ADC 和 tick 回绕；策略测试覆盖温度/断线/压差/总压及系统错误。

## 第三轮：UART1 有效通信唤醒

- RTC 保留 PB4/BLE-EN 低电平，使 VCC33-U 和 UART1 Vout1 不因休眠反复掉电；PB4 不经过模拟模式。PB6 保持高电平，PB7 保留上拉数字输入和下降沿 EXTI7。PA3 仍随关闭的 RS485 电源域进入模拟模式。
- 使用实际 PA15/PB3/PB4/PA4/PB5 电源控制，移除 RTC 路径 PC12/PC13/PD2 旧板操作；PB14 CTLC 保持。
- 新状态 `LP_UART_VERIFY=8` 不改变已有 0..7 状态编号。RX 边沿只启动候选接收；主循环有界验证原地址、命令长度和 CRC，无新增线协议、唤醒命令或地址。原 ISR 仍只接收/计数。
- 3 秒内收到合法请求才完整恢复 RUN；保留该请求缓冲区，不被 UART 重初始化清除，唤醒碎片的错误也不会取消合法请求回复。噪声/错误帧/超时回到安全巡检，不重置正常空闲资格，也不能无限推迟安全采样。按键/PA0 等请求仍优先恢复。
- F103 STOP 期间 USART 不能缓存首个请求，触发唤醒的请求仍可能损失；现有主机应在候选窗口内重试。若要求任意低频单次请求也必达，需要独立握手、外部缓存或更换有接收保持能力的 MCU，纯软件不能承诺。
- 保持 UART1 供电必然增加板子/通信盒静态耗电；尚无电流测量，不能宣称达到某个微安目标。
- PB12 是 RS485/CAN 检测，仍有原供电切换的受控 5 ms 稳定窗口。此窗口内任意短脉冲与电源瞬态无法仅靠软件可靠区分；需要 RS485/CAN 零丢失时必须加锁存/保持请求或验收重试。这不是 UART1 候选路径的限制。
- 诊断版本 2 新增候选/拒绝/合法帧计数，以及独立的返回现场：EXTI/RTC/NVIC/ICSR/DFSR/DHCSR/DBGMCU_CR，不再被普通状态快照覆盖。DFSR 含粘滞位，需结合前后记录判断，不能只凭非零值认定本次调试唤醒。
- `test_low_power_uart.py` 执行实际字节接收器、IRQ 和 CRC：静默、错误、中断碎片、错误 CRC、合法 0x03/0x10、保留缓冲区、长度 0..255；框架测试覆盖候选超时后巡检、合法帧和并发按键。

## 构建与实板门禁

`tools/build_keil_no_bin.py` 将当前工程复制到用户 TEMP，解析绝对源码/包含/散装加载路径，关闭副本的生成 HEX/BIN 与用户钩子，使用原生 UV4 编译；不修改原工程和输出，不烧录。
`tests/check_armcc_no_bin.py --configured-optimization` 提供补充 ARMCC 编译；Keil Optim=0 是默认选项，不应强制解释为 -O0。最终产物大小以原生 Keil 为准。

现有 Target 声明 C8/64 KiB，但 APP 散装加载区为 `0x08004800 + 0x20000`，更新/休眠标志也在 64 KiB 之外；本修复不扩大、不迁移这些区域。构建脚本会报告实际加载范围及 C8 容量不匹配，禁止拿“链接成功”替代实物型号/BOM/存储布局验收。未接板子时不做任何下载。

## 最终源码与编译验证

| 提交 | 内容 |
| --- | --- |
| `e8a2324` | 进入电流、原分钟参数、充电事件保留 |
| `8aca14f` | 新鲜 ADC/安全巡检、失败边界、MOS ADC 错误互锁 |
| `9daa820` | 供电有效的 UART1 候选验证、GPIO、诊断和测试/构建工具 |

最终编译源码为 `9daa820` 加用户既有未提交配置（SNum=6、EEPROM 标识 0x6701 和工程文件当前状态），不是干净 checkout。未将这些用户文件带入提交。

- 10 项相关脚本通过：`test_low_power_framework.py`、`test_low_power_uart.py`、`test_low_power_policy.py`、`test_low_power_adc.py`、`test_rtc_gpio.py`、`test_low_power_edges.py`、`test_sleep.py`、`test_uart_stop.py`、`test_startup_afe.py`、`test_current_calibration.py`。覆盖真实 C 函数的 host 执行和部分源码顺序检查，硬件被 stub，不代表电气验收。
- 框架额外覆盖 UART1 候选期间显式请求 RUN、同时按键和候选拒绝后的安全采样；GPIO 检查 PB4 和 PB7 均不经过模拟模式。
- 原生 Keil Target 1 / ARMCC 5.06u7：0 errors、11 个现有业务警告；Code=46960、RO=2972、RW=1388、ZI=6272 字节。补充 ARMCC 编译/链接同样通过，63 个对象；反汇编中 Delay1ms 仍调用 __delay_ms。
- MAP：`PWR_EnterSTOPMode` 保持在 RAM `0x20000000`，未撤销既有 STOP 返回规避措施。
- 实际 Flash 加载区 `[0x08004800, 0x08011078)`，51320 字节；超过标称 C8 的 `0x08010000` 边界 4216 字节。**不满足当前声明的 64 KiB Flash，不能下载验收。** 不擅自假设芯片有额外可用 Flash，也不扩区或改参数地址。
- 原生 Keil 证据目录：`C:\Users\Administrator\AppData\Local\CodexTemp\103-309-template\keil-no-bin\run-3ycknaax`，包含隔离工程、AXF、MAP、日志及 manifest；AXF SHA256：`d9e1eebe83a97d049d1e62e2bf5e6811ed7821123c6f00862e07f15446e033e2`。
- 补充编译目录：`C:\Users\Administrator\AppData\Local\CodexTemp\103-309-template\armcc-no-bin\run-_xn_n6n6`。均未生成 BIN、未烧录、未操作目标板。

## Keil 与后续实板验收

Watch 添加 `g_low_power_diag` 和 `g_hardfault_record`；在 `LowPower_DebugHook()` 对 `state==8` 观察候选验证，对 `state==7` 观察恢复原因。`wake_mask` 是会话累计来源，不等于仍未处理的唤醒；判断 UART1 是否真的退出看 `uart1_valid_frames`、`restore_count` 和 trace，不只看 `uart1_candidates`。

无来源 WFI 返回应查看 `wake_sequence`、`wake_exti_pr`、`wake_rtc_crl`、`wake_nvic_pending`、`wake_icsr`、`wake_dfsr`、`wake_dhcsr` 和 `wake_dbgmcu_cr`。当前没有板子，因此没有新现场，不能确认它由调试器还是某个 IRQ 引起。现有现场保存和 8 次上限是可诊断/恢复措施，不是根因结论。

单步 HardFault 同样不能由截图中的 handler 循环或“全速正常”判定根因。观察捕获的原始 PC/LR/xPSR、CFSR/HFSR、BFAR/MMFAR；调试配置 1 用于单步，配置 0 脱离调试器才用于真实低功耗验收。不要把暂停导致的 UART 溢出、RTC 超时与实际运行时故障混为一谈。

恢复接板后，先确认实物 MCU/容量、Bootloader/APP/持久化布局和设备 EEPROM 标识，再决定可下载产物。之后依次验收：通信盒持续连接且无发送、各种波特率合法请求与重试、错误帧/接插毛刺、按键/PA0、充放电检测、温度/AFE/ADC 异常、单步 HardFault 现场、断开调试器的长时间 RTC 循环及独立电流表测量。当前这些项目均未完成，不能签署实板稳定或功耗合格结论。

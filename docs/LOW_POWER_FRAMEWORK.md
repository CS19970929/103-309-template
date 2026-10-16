# 低功耗框架实施与验证

## 第一轮：进入条件的基础正确性

- RTC 使用高、低、高读取，避免 16 位进位造成计数拼接错误；STOP 返回后有界等待 RSF 同步。
- TIM3 提供单调的运行态毫秒计数。STOP 期间暂停，不当作实际日历时间；实际休眠时间使用 RTC 差值。
- USART1/2 半帧超过 100 ms 未收到后续字节时丢弃，仅处理 `IDLE + ptr_no != 0`，不截断已完整接收或发送中的帧。无符号差值兼容 tick 回绕。
- 恢复原有 RTC/SOC 表/铜损参数地址的写标志消费者，消除永久 pending。没有改变地址、协议或默认数据。

验证：`test_sleep.py`、`test_uart_stop.py`、`test_rtc_gpio.py` 通过。Keil Target 1 / ARMCC 5.06u7 编译链接成功，0 errors、10 warnings（现有业务代码警告），Code=43604、RO=2972、RW=1316、ZI=5872。临时关闭 BIN 后处理，构建结束恢复。尚未作为完整框架进行板上验证。

工程当前 `Optim=0`，旧 `check_armcc_no_bin.py` 固定要求 `Optim=3`，所以该旧检查不能代表当前工程；本轮以 UV4 实际构建为准。

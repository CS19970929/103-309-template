# CommonUpper 串口异常与中断发送修复说明

## 背景

USART1 在调试时反复进入 `ORE` 接收溢出错误和串口中断。直接写 `USARTx->SR` 不能清除 STM32F10x 的 `ORE/NE/FE/PE` 错误标志，错误标志需要按硬件要求执行“读 SR 后读 DR”的清除序列。

在清除方式修正后，仍需要从调度和收发流程上减少 `ORE` 产生条件：19200bps 下两个字节间隔约 520us，若更高优先级中断执行过久，USART 接收数据寄存器来不及读取，就会触发 `ORE`。

## 修复点

1. USART 错误标志统一通过 `Sci_CommonUpper_ClearUsartFault()` 清除，按 STM32F10x 规定读取 `SR` 后读取 `DR`。
2. CommonUpper 发送由主循环轮询 `TXE` 改为 `TXE/TC` 中断发送：
   - `TXE` 中断负责装载下一个待发送字节。
   - 最后一个字节装载后关闭 `TXEIE` 并打开 `TCIE`。
   - `TC` 中断确认最后一位已经发送完成，再通知主状态机切回接收。
3. 接收完整帧后不再在接收函数结尾无条件重新打开 `RXNEIE`，等待协议处理和发送完成后再恢复接收。
4. USART 中断优先级提升到抢占优先级 0，TIM3 调整为抢占优先级 1。
5. TIM3 中断只保留计数和置标志，`App_WarnCtrl()`、`APP_LedBar()` 改到主循环 10ms 节拍执行，避免长业务逻辑阻塞 USART 中断。

## 验证建议

- 用 Keil 构建 `Target 1`，确认 0 error、0 warning。
- 用上位机连续发送 Modbus 请求，观察 `gu16_CommuErrCnt_SCI1` 是否不再持续增长。
- 若仍出现 `ORE`，重点检查是否存在其他长时间关中断、阻塞式 EEPROM/I2C 操作或更高优先级 ISR。

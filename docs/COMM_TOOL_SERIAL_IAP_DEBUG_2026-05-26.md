# COMM TOOL 串口 IAP 调试记录

## COM3 复现现象

- 调试口：`COM3`，设备枚举为 `USB-SERIAL CH340`。
- 波特率：`115200 8N1`。
- 旧串口上位机源码固定参考路径：`E:\sync\git\upper\BMS-upper - 副本 (4)`，后续排查串口升级协议必须先参考该工程。
- 新 PC-to-comm-tool 协议 `GET_INFO` 无响应，说明板端当时不在 comm tool App 新协议态。
- 旧 BMS 串口 IAP 协议可读状态，`0xFFFD` 连接命令有 ACK，说明不是波特率错误，也不是完全进不了 IAP。
- 发送 `0xFFFE` 数据块时，1024 字节长帧偶发无 ACK，状态读也会偶发无响应，上位机表现为升级卡住。

## 旧上位机协议细节

- `Form1.cs` 中 `FlashUpgrateConnect()` 发送 `0x10/0xFFFD`，寄存器数量 `1`，字节数 `2`。
- `Form1.cs` 中 `FlashUpgrate()` 发送 `0x10/0xFFFE`，满包寄存器数量为 `1024`，`byte_count=0`，随后追加 1024 字节 bin 数据。
- `Form1.cs` 中 `FlashUpgrateComplete()` 发送 `0x10/0xFFFF`，寄存器数量 `1`，字节数 `2`。
- 旧上位机每次 `serialPort1.Write()` 后会立即 `DiscardInBuffer()`，再依赖 ACK 回调继续下一包，因此 IAP 必须在完整收包后延迟 ACK，不能在 PC 清接收缓冲期间过早回包。

## 根因判断

comm tool IAP 原实现使用主循环轮询 `USART_FLAG_RXNE`，`USARTx_IRQHandler` 为空。115200 下旧协议单帧最大 1024 字节，长帧期间主循环还要处理 CAN 轮询、1ms 任务、应答延时和 Flash 操作，容易出现 USART overrun 或帧接收不同步。

## 修复策略

- comm tool App 串口发送从轮询 TXE 改为 TXE 中断环形缓冲发送，接收继续使用 RXNE 中断。
- comm tool IAP 串口收发均改为中断驱动：RXNE 中断写入 2048 字节接收环形缓冲，TXE 中断从发送环形缓冲出队。
- IAP 主循环继续执行旧协议解析和 Flash 写入，避免长帧接收期间因为主循环忙而丢字节。
- 保留 1024 字节旧 BMS 升级数据块格式，不要求用户更换旧上位机协议。
- overrun 或环形缓冲满时计入 `s_serial_fault_count`，仍可通过 `0xD050`/`0xD000` 状态读观察。

## 验证要求

更新 comm tool IAP 后，再用旧 BMS 串口升级上位机或调试脚本在 `COM3/115200` 下升级 `COMM_TOOL_Release.bin`。预期 `0xFFFD`、每个 `0xFFFE` 数据块、`0xFFFF` 完成命令均返回 ACK，完成后复位进入 comm tool App。

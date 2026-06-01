# BMS CAN-IAP 可靠性实施记录

## 当前约束

- IAP 起始地址保持 `0x08000000`。
- App 起始地址保持 `0x08004800`。
- App 写入上限收紧为 `0x0801F800`，避免覆盖升级标志页和睡眠标志页。
- CAN-IAP 数据帧 ID 调整为 `0x14000000 | (seq << 8) | node`，避免旧 `0x14F90000` 与 `seq << 8` 位域重叠。
- IAP 端 CAN1 使用 PA11/PA12，CAN 收发器供电脚跟随当前 BMS App，为 PB4。

## IAP 端稳定性策略

- `START` 后立即保持 IAP 标志，升级中断或复位后继续留在 IAP。
- 数据先进入 256B RAM 块，只有 `COMMIT` 块 CRC 正确才擦写 App Flash。
- Flash 擦写只允许落在 App 区，写后逐半字回读校验。
- `END` 校验帧数、总长度、总 CRC 和 App 向量，通过后才写回 App 有效标志。
- 跳 App 前关闭 SysTick、TIM、USART、CAN，清 NVIC pending，设置 `VTOR` 和 MSP。

## 仍需实测

- comm tool 与 IAP 的真实 CAN ACK/NACK 时序。
- 断电、丢帧、重复帧、CRC 错误、最后一块非 256B、BusOff 恢复场景。

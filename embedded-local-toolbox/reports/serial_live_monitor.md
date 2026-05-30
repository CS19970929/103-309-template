# 串口实时监控报告

- mode: `demo`
- baud: `19200`
- duration_s: `5.0`

## 事件

| 时间(s) | Payload | 关键字 | Modbus |
|---|---|---|---|
| 0.0 | BOOT BMS demo firmware v1.2.3 |  |  |
| 0.1 | WARN cell voltage near threshold | WARN |  |
| 0.2 | 010321000002CE37 |  | RTU slave=1 func=0x03 len=8 crc=ok |
| 0.3 | FAULT CELL_OVP set | FAULT |  |
| 0.4 | INFO loop alive |  |  |

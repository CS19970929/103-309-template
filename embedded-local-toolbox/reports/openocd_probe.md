# OpenOCD/STLink 只读探测报告

安全边界：本工具不执行 erase/program/reset，不自动烧录。

| 项目 | 结果 |
|---|---|
| openocd_path | /opt/homebrew/bin/openocd |
| interface | interface/stlink.cfg |
| target | target/stm32f1x.cfg |
| connect | False |
| DBGMCU_IDCODE | 0x10016414 |

## OpenOCD 输出

```text
未传 --connect，未访问硬件。

离线示例：
Open On-Chip Debugger 0.12.0
Info : STLINK V2J37M26
Info : stm32f1x.cpu: hardware has 6 breakpoints, 4 watchpoints
0xe0042000: 0x10016414
shutdown command invoked

```

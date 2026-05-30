# STLink Flash Size 只读检查报告

安全边界：本工具只读 flash size 系统存储器地址，不执行 erase/program/reset。

| 项目 | 结果 |
|---|---|
| mcu | stm32f1 |
| flash_size_addr | 0x1FFFF7E0 |
| openocd_path | /opt/homebrew/bin/openocd |
| connect | False |
| returncode | demo |
| flash_size_kb | 128 |

## 原始输出

```text
Open On-Chip Debugger 0.12.0
Info : STLINK V2J37M26
Info : stm32f1x.cpu: hardware has 6 breakpoints, 4 watchpoints
0x1ffff7e0: 0x00000080
shutdown command invoked

```

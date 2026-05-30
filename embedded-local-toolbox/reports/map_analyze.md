# Map 占用分析报告

源文件：`data/examples/example_keil.map`

## 总览

| 项目 | 字节 |
|---|---|
| Code | 4096 |
| RO-data | 512 |
| RW-data | 128 |
| ZI-data | 2048 |
| Flash 估算 | 4736 |
| RAM 估算 | 2176 |

## 大函数/代码符号

| 符号 | 大小 | 地址 | 模块 |
|---|---|---|---|
| Soc_Calc | 320 | 0x080001F0 | soc.o |
| main | 160 | 0x08000100 | main.o |
| Can_Process | 128 | 0x08000330 | can.o |
| App_MosControl | 80 | 0x080001A0 | main.o |
| USART1_IRQHandler | 48 | 0x080003B0 | main.o |

## 大变量/数据符号

| 符号 | 大小 | 地址 | 模块 |
|---|---|---|---|
| g_log_buffer | 256 | 0x20000044 | log.o |
| g_cell_voltage | 64 | 0x20000004 | afe.o |
| g_ms_tick | 4 | 0x20000000 | main.o |

## 模块占用

| 模块 | 大小 |
|---|---|
| soc.o | 320 |
| main.o | 292 |
| log.o | 256 |
| can.o | 128 |
| afe.o | 64 |

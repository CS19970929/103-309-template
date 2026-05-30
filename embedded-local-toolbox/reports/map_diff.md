# Map 差异报告

旧文件：`data/examples/example_keil_old.map`

新文件：`data/examples/example_keil.map`

## 总览差异

| 区域 | 旧值 | 新值 | 变化 |
|---|---|---|---|
| Code | 3840 | 4096 | 256 |
| RO-data | 480 | 512 | 32 |
| RW-data | 128 | 128 | 0 |
| ZI-data | 1984 | 2048 | 64 |

## 符号变化

| 符号 | 旧大小 | 新大小 | 变化 | 模块 |
|---|---|---|---|---|
| Soc_Calc | 256 | 320 | 64 | soc.o |
| g_log_buffer | 192 | 256 | 64 | log.o |
| App_MosControl | 64 | 80 | 16 | main.o |
| Can_Process | 112 | 128 | 16 | can.o |
| main | 144 | 160 | 16 | main.o |

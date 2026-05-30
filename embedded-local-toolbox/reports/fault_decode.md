# Cortex-M HardFault 解析报告

## 寄存器

| 寄存器 | 值 |
|---|---|
| r0 | 0x00000000 |
| r1 | 0x20000000 |
| r2 | 0x00000001 |
| r3 | 0x00000002 |
| r12 | 0x00000000 |
| lr | 0x080001A6 |
| pc | 0x080001F8 |
| psr | 0x21000000 |
| cfsr | 0x02000000 |
| hfsr | 0x40000000 |
| bfar | 0x00000000 |
| mmfar | 0x00000000 |

## CFSR 位解析

| 位含义 |
|---|
| DIVBYZERO |

## PC/LR 符号定位

| 寄存器 | 地址 | 最近符号 |
|---|---|---|
| PC | 0x080001F8 | Soc_Calc+0x8 (soc.o) |
| LR | 0x080001A6 | App_MosControl+0x6 (main.o) |

# Include 路径迁移对照

记录日期：2026-05-31

## Keil IncludePath 提取

相对 Keil 工程目录 `103 + 309/Project/Users`：

| 序号 | Keil IncludePath | GCC/CMake 目标路径 | 状态 |
|---:|---|---|---|
| 1 | `../Lib` | `103 + 309/Project/Lib` | TODO：当前目录不存在，保留兼容记录 |
| 2 | `../STM32F10x_StdPeriph_Lib_V3.5.0/drivers` | `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers` | 存在 |
| 3 | `../STM32F10x_StdPeriph_Lib_V3.5.0/inc` | `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc` | 存在 |
| 4 | `../Source` | `103 + 309/Project/Source` | 存在 |
| 5 | `../Source/conf` | `103 + 309/Project/Source/conf` | 存在 |
| 6 | `../Source/easylogger/inc` | `103 + 309/Project/Source/easylogger/inc` | 存在 |

## CMSIS/StdPeriph 检查

- 当前 tracked 工程使用 `STM32F10x_StdPeriph_Lib_V3.5.0`。
- `conf.h` 中 `#include "stm32f0xx.h"` 被注释。
- 当前迁移分支不包含原主工作区未跟踪的 `C030v1.0/`，不把 F0 工程混入本次 F1 CMake 目标。

## 风险

- 中风险：`../Lib` 目录不存在，Keil 仍配置了该 include path。GCC 保留该路径不会破坏构建，但需要确认历史上是否存在外部库目录。
- 低风险：路径中包含空格 `103 + 309`，CMake 必须使用变量和 list，不写死绝对路径。

# Scatter 到 GCC Linker Script 迁移对照

记录日期：2026-05-31

## Keil scatter 来源

当前提交分支中 `Objects/` 被 `.gitignore` 忽略，未保存 `.sct`。本次审查读取原工作区 `D:\code\103-309-template\103 + 309\Project\Users\Objects\FD_Release.sct`，内容为 Keil/uVision 自动生成。

## Keil scatter 摘要

| 区域 | 起始地址 | 长度 | 用途 |
|---|---:|---:|---|
| `LR_IROM1` | `0x08004800` | `0x00020000` | Load region |
| `ER_IROM1` | `0x08004800` | `0x00020000` | RO/XO execution region |
| `RW_IRAM1` | `0x20000000` | `0x00005000` | RW/ZI RAM region |

Keil section 放置规则：

- `*.o (RESET, +First)`
- `*(InRoot$$Sections)`
- `.ANY (+RO)`
- `.ANY (+XO)`
- `.ANY (+RW +ZI)`

## GCC linker 目标

| 配置项 | Keil 原值 | GCC `.ld` 目标 | 说明 |
|---|---|---|---|
| App 起始 | `0x08004800` | `FLASH ORIGIN = 0x08004800` | 与 IAP 安全规则一致 |
| App 可放置代码上限 | Keil 未显式避开参数区，长度到 `0x08024800` | `FLASH LENGTH = 0x00017800`，结束于 `0x0801C000` | 避免覆盖持久化参数/日志区 |
| IAP 区 | scatter 未显式建区 | `IAP ORIGIN = 0x08000000 LENGTH = 0x4800` | 仅声明，不放置 App section |
| 参数/日志区 | scatter 未显式建区 | `FLASH_STORAGE ORIGIN = 0x0801C000 LENGTH = 0x4000` | 来自 `Flash.h` |
| RAM | `0x20000000/0x5000` | `RAM ORIGIN = 0x20000000 LENGTH = 0x5000` | 一致 |
| 栈顶 | ARMASM startup `__initial_sp` | `_estack = ORIGIN(RAM) + LENGTH(RAM)` | GCC startup 使用 |
| `.isr_vector` | `RESET, +First` | 放在 `FLASH` 起始，`KEEP(*(.isr_vector))` | 必须 KEEP |
| `.text` | `.ANY(+RO)` | `*(.text*)` | 使用 `--gc-sections` |
| `.rodata` | `.ANY(+RO)` | `*(.rodata*)` | 使用 `--gc-sections` |
| `.ARM.exidx`/`.ARM.extab` | Keil 未同名展示 | 显式放入 FLASH 并 KEEP start/end 符号 | GCC/newlib 需要 |
| `.init_array`/`.fini_array` | `InRoot$$Sections` | `KEEP(*(SORT(.init_array.*)))` 等 | GCC C runtime 需要 |
| `.data` | RW load 在 Flash、run 在 RAM | `> RAM AT> FLASH`，定义 `_sidata/_sdata/_edata` | startup 拷贝 |
| `.bss` | ZI in RAM | `NOLOAD`，定义 `_sbss/_ebss` | startup 清零 |
| heap/stack | ARMASM startup 中定义 | `. _user_heap_stack` + `_end` | newlib/nosys 兼容 |

## 自定义 section 审查

源码中未发现业务自定义 `#pragma arm section`、`Image$$`、`Load$$`、`RW$$`、`ZI$$` 符号依赖。发现 `SocEnhance.c` 使用 `__attribute__((used))` 保留调试观察对象，不是自定义 linker section。

## 必须 KEEP 的 section

- `.isr_vector`
- `.init`
- `.fini`
- `.preinit_array`
- `.init_array`
- `.fini_array`
- `.ARM.exidx*`

## 风险和 TODO

- 高风险：Keil `.uvprojx` 设备为 `STM32F103C8`，但 scatter/App/参数区使用到 `0x0801FFFF`，需要确认实际 MCU Flash 容量。
- 高风险：Keil scatter 没有显式保护 `0x0801C000` 后持久化区，GCC linker 必须补上。
- 中风险：如果后续发现业务使用自定义 section，需要同步更新 `.ld`。

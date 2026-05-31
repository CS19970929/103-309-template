# ARMCC/GCC 编译兼容审查

记录日期：2026-05-31

## 差异项检查

| 差异项 | 当前工程情况 | GCC 处理 |
|---|---|---|
| `__packed` | 当前扫描未发现业务使用 | `compiler_port.h` 预留 `COMPILER_PACKED` |
| `__weak` | 当前扫描未发现业务使用 | `compiler_port.h` 预留 `COMPILER_WEAK` |
| `__align` | 当前扫描未发现业务使用 | `compiler_port.h` 预留 `COMPILER_ALIGNED(x)` |
| `__asm` | `stm32f10x_it.c` 存在 `__asm void wait()` | 已做单点条件编译适配：ARMCC 保留原实现，GCC 使用 `COMPILER_NAKED` |
| `__STATIC_INLINE` | CMSIS 内部处理 | 不改 CMSIS |
| `#pragma arm section` | 未发现 | 不迁移 |
| scatter 符号 | 未发现 `Image$$/Load$$/RW$$/ZI$$` | 不迁移 |
| `__attribute__((used))` | `SocEnhance.c` 已兼容 GCC/ARMCC | 保持 |
| `fputc` | `Sci_Upper.c` 已实现串口阻塞输出 | 已新增 GCC 专用 `_write`，转接现有 `fputc`，不修改通信协议 |
| semihosting | 未见启用 | 使用 `--specs=nosys.specs`，不默认 semihosting |

## 统一兼容层要求

已新增 `cmake/compat/compiler_port.h`，集中定义：

- `COMPILER_WEAK`
- `COMPILER_PACKED`
- `COMPILER_ALIGNED(x)`
- `COMPILER_USED`
- `COMPILER_NAKED`
- `COMPILER_SECTION(name)`
- `COMPILER_NORETURN`

后续如需适配 ARMCC/GCC 差异，优先引用这些宏，不在业务代码中散落编译器判断。

## 已完成兼容修复

- `stm32f10x_it.c` 的 `__asm void wait()` 已完成最小条件编译修复。
- 旧 CMSIS `core_cm3.c` 的 GCC `strex/strexb/strexh` inline asm 输出约束已改为 early-clobber，避免 GCC 12 分配相同寄存器。
- 已新增 `cmake/compat/syscalls_gcc.c`，补齐 newlib-nano 所需最小 syscall，并通过 `_write` 转接现有 `fputc`。
- linker script 已增加 PHDRS，消除 `LOAD segment with RWX permissions` 警告。

## 仍需上板确认

- `_write` 当前依赖 `Sci_Upper.c` 的阻塞式 `fputc`，需要确认 UART 初始化后日志输出正常。
- HardFault Debug 模式下 `wait()` 的行为与 ARMCC 等价为立即返回；仍需在调试器下确认断点/单步体验。

# ARMCC/GCC 编译兼容审查

记录日期：2026-05-31

## 差异项检查

| 差异项 | 当前工程情况 | GCC 处理 |
|---|---|---|
| `__packed` | 当前扫描未发现业务使用 | `compiler_port.h` 预留 `COMPILER_PACKED` |
| `__weak` | 当前扫描未发现业务使用 | `compiler_port.h` 预留 `COMPILER_WEAK` |
| `__align` | 当前扫描未发现业务使用 | `compiler_port.h` 预留 `COMPILER_ALIGNED(x)` |
| `__asm` | `stm32f10x_it.c` 存在 `__asm void wait()` | 高风险，需条件编译适配 |
| `__STATIC_INLINE` | CMSIS 内部处理 | 不改 CMSIS |
| `#pragma arm section` | 未发现 | 不迁移 |
| scatter 符号 | 未发现 `Image$$/Load$$/RW$$/ZI$$` | 不迁移 |
| `__attribute__((used))` | `SocEnhance.c` 已兼容 GCC/ARMCC | 保持 |
| `fputc` | `Sci_Upper.c` 已实现串口阻塞输出 | GCC/newlib 还需要 `_write` retarget 或确认 `nosys` 行为 |
| semihosting | 未见启用 | 使用 `--specs=nosys.specs`，不默认 semihosting |

## 统一兼容层要求

新增 `compiler_port.h`，集中定义：

- `COMPILER_WEAK`
- `COMPILER_PACKED`
- `COMPILER_ALIGNED(x)`
- `COMPILER_USED`
- `COMPILER_NAKED`
- `COMPILER_SECTION(name)`
- `COMPILER_NORETURN`

后续如需适配 ARMCC/GCC 差异，优先引用这些宏，不在业务代码中散落编译器判断。

## 当前阻塞

- `stm32f10x_it.c` 的 `__asm void wait()` 是 GCC 构建阻塞项。建议最小修改为：
  - ARMCC 保留原实现。
  - GCC 使用 `COMPILER_NAKED void wait(void)` 加 `__asm volatile("bx lr");`。
- `printf` 输出在 GCC 下必须验证。若 `nosys.specs` 导致 `_write` 默认失败，需要新增 GCC 专用 retarget 源文件，不修改通信协议。

# 当前迁移阻塞项

记录日期：2026-05-31

| 阻塞项 | 等级 | 说明 | 解除条件 |
|---|---|---|---|
| 实际 MCU 型号/Flash 容量不确定 | 高 | `.uvprojx` 设备为 `STM32F103C8`，但项目名、Keil scatter 和 `Flash.h` 使用到 `0x0801FFFF` | 从 BOM、丝印、Keil Pack 或 ST-LINK 读芯片容量确认 |
| 可选烧录/调试工具未安装 | 中 | `scripts/check_env.py` 未发现 `JLinkGDBServer`、`STM32_Programmer_CLI`、`openocd` | 安装 J-Link Software、STM32CubeProgrammer 或 OpenOCD 后按目标硬件验证 |
| 硬件功能未验证 | 高 | 构建迁移不能证明 BMS/SOC/低功耗行为一致 | 上板烧录后按验证清单检查 |

## 已解除阻塞

| 原阻塞项 | 处理结果 |
|---|---|
| Keil scatter 未入库 | 已新增 `linker/stm32f103_app_0x08004800.ld`，并在 `docs/scatter_to_ld_compare.md` 固定转换来源和 Flash 参数区边界 |
| ARMCC 汇编函数无法直接 GCC 编译 | 已对 `stm32f10x_it.c` 的 `wait()` 增加 GCC 条件编译实现，ARMCC 路径保持原实现 |
| printf/newlib retarget 未验证 | 已新增 `cmake/compat/syscalls_gcc.c`，通过 `_write` 转接现有 `fputc`，并提供最小 `_read/_close/_fstat/_isatty/_lseek` stub |
| 本机 GCC 工具链缺失 | 已通过 winget 安装 Python 3.9、CMake、Ninja、Arm GNU Toolchain，并通过 `scripts/check_env.py` |
| 标准 Python 启动入口不稳定 | 已安装 Python 3.9，`py -3.9` 可用 |
| GCC 构建尚未完成 | Debug 和 Release 均已完成 clean build，产物包含 `.elf/.hex/.bin/.map` |

## 当前不允许做的事

- 不删除 Keil 工程。
- 不把 App bin 写到 `0x08000000`。
- 不默认全片擦除。
- 不改业务逻辑来换取编译通过。
- 不开启 SOC 测试宏到量产 Release。

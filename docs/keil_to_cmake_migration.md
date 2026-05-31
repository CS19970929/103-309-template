# Keil 到 CMake/GCC 迁移执行记录

记录日期：2026-05-31

## 执行范围

- 保留 Keil 工程，不删除 `.uvprojx`、`.uvoptx`、ARMCC startup 或 Keil 生成目录。
- 新增并行 CMake/Ninja/arm-none-eabi-gcc 构建入口。
- 不修改业务逻辑、不修改通信协议、不引入 HAL、不引入 RTOS。
- App 固件地址固定为 `0x08004800`，禁止把 App bin 裸写到 `0x08000000`。

## 已完成步骤

| 阶段 | 结果 | 说明 |
|---|---|---|
| Keil 基准扫描 | 已完成 | 已提取 Keil 目标、芯片、宏、include、源文件、scatter、startup、尺寸等信息 |
| 逐项迁移审查 | 已完成 | 已生成宏、include、scatter、startup、优化、Flash 布局、IAP、BMS 风险等审查文档 |
| CMake/GCC 骨架 | 已完成 | 已新增 toolchain、flags、芯片配置、linker script、GCC startup、顶层 CMake |
| Python 工具脚本 | 已完成 | 已新增环境检查、构建、清理、size、烧录脚本 |
| VS Code 配置 | 已完成 | 已新增插件推荐、settings、tasks、J-Link/ST-LINK 调试配置 |
| 环境检查 | 已通过必需工具 | Python 3.9、CMake、Ninja、arm-none-eabi-gcc/objcopy/size/gdb 已可用；J-Link/ST-LINK/OpenOCD 仍未安装 |
| Debug 构建 | 已通过 | 已生成 `firmware.elf/.hex/.bin/.map`，size 已输出 |
| Release 构建 | 已通过 | 已生成 `firmware.elf/.hex/.bin/.map`，size 已输出 |
| 烧录 dry-run | 已通过 | J-Link、ST-LINK、OpenOCD dry-run 可输出命令；`0x08000000` 地址拒绝生效 |
| ST-LINK 上板烧录 | 已通过 | OpenOCD 按 `0x08004800` 写入 Release bin，verify OK，IAP 和参数区保持不变 |
| ST-LINK GDB 调试 | 已通过 | OpenOCD + GDB 命中 `main.c:94`，PC/SP/xPSR 正常 |

## 首次环境检查结果

执行命令：

```powershell
& 'C:\SiliconLabs\SimplicityStudio\v5\developer\adapter_packs\python\python.exe' scripts\check_env.py
```

结果摘要：

| 工具 | 状态 |
|---|---|
| Python | 可用，当前用于验证的是 `3.10.5` |
| CMake | 缺失 |
| Ninja | 缺失 |
| arm-none-eabi-gcc | 缺失 |
| arm-none-eabi-objcopy | 缺失 |
| arm-none-eabi-size | 缺失 |
| arm-none-eabi-gdb | 缺失 |
| JLinkGDBServer/JLinkGDBServerCL | 未发现 |
| STM32_Programmer_CLI | 未发现 |
| openocd | 未发现 |

补充：当前 shell 中 `py` 不在 PATH，`python` 命令指向 WindowsApps 占位程序，不能作为稳定脚本入口。

## Debug 构建尝试结果

执行命令：

```powershell
& 'C:\SiliconLabs\SimplicityStudio\v5\developer\adapter_packs\python\python.exe' scripts\build.py --config Debug
```

结果：

```text
error: cmake not found. Run scripts/check_env.py and install missing tools.
```

当前未进入 GCC 编译阶段，因此还不能确认后续是否存在 ARMCC/GCC 语法差异、retarget 或 section 兼容问题。

## 继续验证结果

本机已通过 `winget` 安装 Python 3.9、CMake、Ninja、Arm GNU Toolchain。重新打开终端或刷新 PATH 后，执行：

```powershell
py -3.9 scripts\check_env.py
```

必需工具全部通过：

| 工具 | 版本/状态 |
|---|---|
| Python | `3.9.13` |
| CMake | `4.3.3` |
| Ninja | `1.13.2` |
| Arm GNU Toolchain | `12.2.1 20230214` |
| arm-none-eabi-objcopy | 可用 |
| arm-none-eabi-size | 可用 |
| arm-none-eabi-gdb | 可用 |

仍未发现的可选工具：

- `JLinkGDBServer`
- `JLinkGDBServerCL`
- `STM32_Programmer_CLI`

OpenOCD 已在后续步骤通过 winget 安装并验证。

## GCC 构建修复记录

| 问题 | 处理 |
|---|---|
| CMake `BYPRODUCTS` 使用 `$<TARGET_FILE_DIR:firmware>` 导致生成失败 | 改为显式 `${CMAKE_CURRENT_BINARY_DIR}/firmware.hex` 和 `.bin` |
| `stm32f10x_it.c` 的 `__asm void wait()` 无法被 GCC 编译 | 增加 `#if defined(__GNUC__)` 分支，使用 `COMPILER_NAKED` + `__asm volatile("bx lr")`，ARMCC 原实现不变 |
| 旧 CMSIS `core_cm3.c` 的 `strex` inline asm 在 GCC 12 下可能分配相同寄存器 | 将 `__STREXB/__STREXH/__STREXW` 输出约束从 `=r` 改为 `=&r` |
| newlib-nano/nosys `_write/_read/...` 链接警告 | 新增 `cmake/compat/syscalls_gcc.c`，`_write` 转接现有 `fputc` |
| ELF 出现 RWX LOAD segment 警告 | linker script 增加 PHDRS，并将 heap/stack 预留段排除出 LOAD program header |
| dry-run 依赖烧录工具存在 | `scripts/flash.py` 改为 dry-run 不要求工具存在，实际 `--flash` 时才强制检查 |
| `0x08000000` 安全检查输出 traceback | `scripts/flash.py` 改为简洁 `error:` 输出，并明确提示会覆盖 IAP |
| OpenOCD Windows 路径反斜杠导致烧录失败 | `scripts/flash.py` 对 OpenOCD program 路径使用正斜杠并加花括号引用 |
| ST-LINK 调试流程需要可重复执行 | 新增 `scripts/debug_smoke.py`，固化 OpenOCD + GDB 到 `main` 的冒烟调试 |

## 构建产物与尺寸

| 配置 | text | data | bss | dec | bin 大小 |
|---|---:|---:|---:|---:|---:|
| Debug | 63368 | 916 | 6852 | 71136 | 64284 |
| Release | 53488 | 908 | 6716 | 61112 | 54396 |

## ST-LINK 上板验证摘要

| 项目 | 结果 |
|---|---|
| ST-LINK | `VID:PID 0483:3748`，`STLINK V2J37S7` |
| Target voltage | 约 `3.29V` |
| Device ID | `0x20036410` |
| Flash size | `128 KiB` |
| 烧录 | `py -3.9 scripts\flash.py --method openocd --config Release --flash`，verify OK |
| IAP 区 | `0x08000000` 向量烧录前后不变 |
| App 区 | `0x08004800` 向量已更新为 GCC Release |
| 参数区 | `0x0801C000` 头部烧录前后不变 |
| GDB | 命中 `main.c:94`，`pc=0x08007590`，`sp=0x20005000` |

详细记录见 `docs/stlink_debug_report.md`。

产物路径：

- `build/gcc-debug/firmware.elf`
- `build/gcc-debug/firmware.hex`
- `build/gcc-debug/firmware.bin`
- `build/gcc-debug/firmware.map`
- `build/gcc-release/firmware.elf`
- `build/gcc-release/firmware.hex`
- `build/gcc-release/firmware.bin`
- `build/gcc-release/firmware.map`

## 当前 GCC 告警

以下告警来自现有源码，未为通过 GCC 构建而修改业务逻辑：

| 文件 | 告警摘要 | 风险 |
|---|---|---|
| `I2C_AFE1.c` | `MTPWrite`、`MTPWriteROM` 的 `result` 可能未初始化 | AFE MTP 写入返回值需审查 |
| `PubFunc.c` | `Usart_9bitOddEvenData_Frame` 的 `result` 可能未初始化 | UART 9-bit 数据计算需审查 |
| `Sci_Upper.c` | `Sci_ACK_0x03` 的 `i` 可能未初始化 | 上位机响应帧索引需审查 |
| `SH367309_DataDeal.h` | 结构体默认初始化缺少括号 | 参数结构体布局需确认 |
| `SH367309_Func.c`、`DataDeal.c`、`rtc_sleep.c`、`PubFunc.c` | 未使用变量/函数 | 当前不影响构建，后续按业务风险处理 |

## 已按验证结果修正的工具问题

| 问题 | 处理 |
|---|---|
| `build.py` 在缺少 `cmake` 时输出底层 Windows traceback | 改为明确输出 `cmake not found`，提示先运行 `scripts/check_env.py` |
| 新增脚本使用 Python 3.10 union 类型写法 | 已调整为 Python 3.9 可解析写法 |
| J-Link command file 使用了不可靠的 SWD 命令 | 已改为 `si SWD` |

## 后续必须继续验证

- 如需要 STM32CubeProgrammer 流程，安装后补充 ST-LINK CLI 验证。
- 如需要 J-Link 流程，安装 J-Link Software 后补充 J-Link 烧录/调试验证。
- 上板后验证 UART 日志、IAP 跳转、Flash 参数区保护、BMS 保护、SOC、CAN、低功耗。
- 每修复一类问题必须更新本文档和对应对照文档。

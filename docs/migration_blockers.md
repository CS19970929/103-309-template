# 当前迁移阻塞项

记录日期：2026-05-31

| 阻塞项 | 等级 | 说明 | 解除条件 |
|---|---|---|---|
| 实际 MCU 型号/Flash 容量不确定 | 高 | `.uvprojx` 设备为 `STM32F103C8`，但项目名、Keil scatter 和 `Flash.h` 使用到 `0x0801FFFF` | 从 BOM、丝印、Keil Pack 或 ST-LINK 读芯片容量确认 |
| Keil scatter 未入库 | 中 | `FD_Release.sct` 是忽略目录中的生成产物，本次只能从原工作区读取 | GCC `.ld` 入库并在文档中固定来源 |
| ARMCC 汇编函数无法直接 GCC 编译 | 高 | `stm32f10x_it.c` 存在 `__asm void wait()` | 新增 `compiler_port.h` 并做单点条件编译适配 |
| printf/newlib retarget 未验证 | 中 | Keil microlib 的 `fputc` 与 GCC newlib `_write` 路径不同 | GCC 构建后验证串口输出；必要时新增 GCC retarget |
| 本机 GCC 工具链缺失 | 高 | 已运行 `scripts/check_env.py`，当前未发现 `cmake`、`ninja`、`arm-none-eabi-gcc`、`arm-none-eabi-objcopy`、`arm-none-eabi-size`、`arm-none-eabi-gdb` | 安装工具链并设置 PATH 或 `ARM_GNU_TOOLCHAIN_PATH` 后重新检查 |
| 标准 Python 启动入口不稳定 | 中 | 当前 shell 中 `py` 不存在，`python` 指向 WindowsApps 占位程序；本次验证临时使用 SimplicityStudio 内置 Python 3.10.5 | 安装正式 Python 3.9+，或在 VS Code/终端中固定真实解释器路径 |
| GCC 构建尚未完成 | 高 | 已尝试 `scripts/build.py --config Debug`，阻塞于 `cmake not found`，尚未进入编译 | Debug/Release 至少完成编译、产物和 size 输出 |
| 硬件功能未验证 | 高 | 构建迁移不能证明 BMS/SOC/低功耗行为一致 | 上板烧录后按验证清单检查 |

## 当前不允许做的事

- 不删除 Keil 工程。
- 不把 App bin 写到 `0x08000000`。
- 不默认全片擦除。
- 不改业务逻辑来换取编译通过。
- 不开启 SOC 测试宏到量产 Release。

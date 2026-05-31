# 当前迁移阻塞项

记录日期：2026-05-31

| 阻塞项 | 等级 | 说明 | 解除条件 |
|---|---|---|---|
| 实际 MCU 型号/Flash 容量不确定 | 高 | `.uvprojx` 设备为 `STM32F103C8`，但项目名、Keil scatter 和 `Flash.h` 使用到 `0x0801FFFF` | 从 BOM、丝印、Keil Pack 或 ST-LINK 读芯片容量确认 |
| Keil scatter 未入库 | 中 | `FD_Release.sct` 是忽略目录中的生成产物，本次只能从原工作区读取 | GCC `.ld` 入库并在文档中固定来源 |
| ARMCC 汇编函数无法直接 GCC 编译 | 高 | `stm32f10x_it.c` 存在 `__asm void wait()` | 新增 `compiler_port.h` 并做单点条件编译适配 |
| printf/newlib retarget 未验证 | 中 | Keil microlib 的 `fputc` 与 GCC newlib `_write` 路径不同 | GCC 构建后验证串口输出；必要时新增 GCC retarget |
| 本机 GCC 工具链状态未知 | 中 | 尚未运行 `scripts/check_env.py` | 完成工具脚本后执行环境检查 |
| GCC 构建尚未完成 | 高 | CMake/linker/startup 尚未落地验证 | Debug/Release 至少完成编译、产物和 size 输出 |
| 硬件功能未验证 | 高 | 构建迁移不能证明 BMS/SOC/低功耗行为一致 | 上板烧录后按验证清单检查 |

## 当前不允许做的事

- 不删除 Keil 工程。
- 不把 App bin 写到 `0x08000000`。
- 不默认全片擦除。
- 不改业务逻辑来换取编译通过。
- 不开启 SOC 测试宏到量产 Release。

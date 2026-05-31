# 迁移风险汇总清单

记录日期：2026-05-31

## 高风险

| 风险 | 影响 | 当前处理 |
|---|---|---|
| 实际 MCU 容量未确认 | `.uvprojx` 是 `STM32F103C8`，但工程使用到 `0x0801FFFF`，可能涉及 64K/128K 标识不一致 | linker 暂按现有 Flash 使用边界保护；要求硬件确认 |
| IAP 覆盖风险 | App bin 写到 `0x08000000` 会覆盖 IAP | `scripts/flash.py` 拒绝 `0x08000000`，默认 dry-run |
| GCC 产物尚未上板验证 | Debug/Release 已构建成功，但还不能证明 GCC 产物与 Keil 功能一致 | 继续做烧录、调试和 BMS 功能回归 |
| ARMCC/GCC 条件编译适配 | `wait()`、CMSIS inline asm、syscalls 已做 GCC 适配 | 已通过 GCC 构建；需确认 Keil ARMCC 构建不受影响 |
| Flash 参数区覆盖 | 参数、日志、SOC/AFE 数据使用 `0x0801C000-0x0801FFFF` | GCC linker 显式预留，不默认全片擦除 |

## 中风险

| 风险 | 影响 | 当前处理 |
|---|---|---|
| `printf`/retarget 差异 | Keil microlib 与 GCC newlib 输出路径不同 | 已新增 `_write` 转接 `fputc`；仍需上板验证串口输出 |
| 软件 I2C 时序 | 优化等级变化可能改变延时循环 | Debug 用 `-Og`，Release 用 `-Os`，对 `LedBar.c` 先设置 `-O0`，其他时序文件待编译后审查 |
| UART/Modbus 中断 | 中断符号或优先级差异会影响通信 | startup 已链接通过；仍需串口和中断回归 |
| CAN 结构体对齐 | GCC packed/align 语义差异可能影响帧解析 | `compiler_port.h` 集中处理 ARMCC/GCC 属性差异 |
| 低功耗唤醒 | SystemInit、VTOR 和时钟初始化顺序差异可能影响唤醒 | startup 保持 SystemInit -> data/bss -> main 流程，VTOR 仍需上板验证 |

## 低风险但需跟踪

| 风险 | 影响 | 当前处理 |
|---|---|---|
| VS Code 本地路径差异 | 不同电脑调试工具安装路径不同 | 使用工具名和 PATH，不在仓库 settings 写死绝对路径 |
| macOS 大小写路径差异 | include/source 路径大小写不一致会在 macOS 暴露 | CMake 使用现有实际路径，构建后继续修正 |
| CMake 源文件列表漂移 | 后续 Keil 工程新增文件后 CMake 未同步 | `docs/keil_to_gcc_config_compare.md` 作为对照基准，后续变更必须更新 |

## 当前 GCC 告警风险

| 文件 | 风险 | 处理 |
|---|---|---|
| `I2C_AFE1.c` | `MTPWrite`/`MTPWriteROM` 返回值可能未初始化 | 不为构建通过修改业务逻辑，列入 AFE 写入回归 |
| `PubFunc.c` | 9-bit UART 数据计算返回值可能未初始化 | 列入通信回归 |
| `Sci_Upper.c` | `Sci_ACK_0x03` 帧索引可能未初始化 | 列入上位机协议回归 |
| `SH367309_DataDeal.h` | AFE 参数默认结构体初始化括号告警 | 列入参数结构体布局/CRC 回归 |

## 功能验证前禁止事项

- 不删除 Keil 工程。
- 不替换现有量产烧录流程。
- 不修改 BMS 保护逻辑、SOC 逻辑、通信协议、低功耗逻辑。
- 不打开 SOC 测试宏到 Release 量产配置。
- 不通过删除业务代码换取 GCC 编译通过。

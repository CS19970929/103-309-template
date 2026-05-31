# 迁移风险汇总清单

记录日期：2026-05-31

## 高风险

| 风险 | 影响 | 当前处理 |
|---|---|---|
| 实际 MCU 容量未确认 | `.uvprojx` 是 `STM32F103C8`，但工程使用到 `0x0801FFFF`，可能涉及 64K/128K 标识不一致 | linker 暂按现有 Flash 使用边界保护；要求硬件确认 |
| IAP 覆盖风险 | App bin 写到 `0x08000000` 会覆盖 IAP | `scripts/flash.py` 拒绝 `0x08000000`，默认 dry-run |
| GCC 构建未完成 | 还不能证明 GCC 产物与 Keil 功能一致 | 工具链安装后继续编译验证 |
| ARMCC 汇编函数 | `__asm void wait()` 不能直接被 GCC 编译 | 已建立兼容层文档，后续进入编译阶段后做单点适配 |
| Flash 参数区覆盖 | 参数、日志、SOC/AFE 数据使用 `0x0801C000-0x0801FFFF` | GCC linker 显式预留，不默认全片擦除 |

## 中风险

| 风险 | 影响 | 当前处理 |
|---|---|---|
| `printf`/retarget 差异 | Keil microlib 与 GCC newlib 输出路径不同 | 待 GCC 编译后验证 `_write` 或串口输出 |
| 软件 I2C 时序 | 优化等级变化可能改变延时循环 | Debug 用 `-Og`，Release 用 `-Os`，对 `LedBar.c` 先设置 `-O0`，其他时序文件待编译后审查 |
| UART/Modbus 中断 | 中断符号或优先级差异会影响通信 | startup 文档已要求 ISR 名称一致，待链接验证 |
| CAN 结构体对齐 | GCC packed/align 语义差异可能影响帧解析 | `compiler_port.h` 集中处理 ARMCC/GCC 属性差异 |
| 低功耗唤醒 | SystemInit、VTOR 和时钟初始化顺序差异可能影响唤醒 | startup 保持 SystemInit -> data/bss -> main 流程，VTOR 仍需上板验证 |

## 低风险但需跟踪

| 风险 | 影响 | 当前处理 |
|---|---|---|
| VS Code 本地路径差异 | 不同电脑调试工具安装路径不同 | 使用工具名和 PATH，不在仓库 settings 写死绝对路径 |
| macOS 大小写路径差异 | include/source 路径大小写不一致会在 macOS 暴露 | CMake 使用现有实际路径，构建后继续修正 |
| CMake 源文件列表漂移 | 后续 Keil 工程新增文件后 CMake 未同步 | `docs/keil_to_gcc_config_compare.md` 作为对照基准，后续变更必须更新 |

## 功能验证前禁止事项

- 不删除 Keil 工程。
- 不替换现有量产烧录流程。
- 不修改 BMS 保护逻辑、SOC 逻辑、通信协议、低功耗逻辑。
- 不打开 SOC 测试宏到 Release 量产配置。
- 不通过删除业务代码换取 GCC 编译通过。

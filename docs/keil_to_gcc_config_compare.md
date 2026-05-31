# Keil 到 GCC/CMake 配置对照审查

记录日期：2026-05-31

本文件是 Keil 配置迁移到 GCC/CMake 的总对照表。结论来自 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`、原工作区 Keil 构建产物 `Objects/FD_Release.sct`、`Objects/FD_Release.lnp`、`Listings/FD_Release.map` 以及源码中的 `Project_Config.h`、`Flash.h`、`system_stm32f10x.c`。

## 总表

| 配置项 | Keil 原值 | GCC/CMake 新值 | 是否一致 | 风险等级 | 处理结果 |
|---|---|---|---|---|---|
| 工程目标 | `FD_Release`、`FD_Debug` | `firmware`，通过 `CMAKE_BUILD_TYPE=Release/Debug` 区分 | 行为等价 | 低 | 保留 Keil 目标，不改旧工程 |
| 芯片型号 | `STM32F103C8` | 先按 `STM32F103xB/STM32F10X_MD` 兼容配置 | 待确认 | 高 | TODO：实物/BOM/Keil Pack 需确认；项目文件名和 Flash 地址显示可能不是 64KB C8 |
| 内核 | `Cortex-M3` | `-mcpu=cortex-m3` | 是 | 低 | 可迁移 |
| Thumb | Keil ARMCC Cortex-M 默认 Thumb | `-mthumb` | 是 | 低 | 必须启用 |
| FPU | Cortex-M3 无 FPU | 不启用 FPU，`-mfloat-abi=soft` | 是 | 低 | 可迁移 |
| Keil 宏 Release | `STM32F10X_MD,USE_STDPERIPH_DRIVER` | `STM32F10X_MD;USE_STDPERIPH_DRIVER;PROJECT_CFG_BUILD_PROFILE=0` | 行为等价 | 中 | Release 显式固定量产 profile 为 0 |
| Keil 宏 Debug | `STM32F10X_MD,USE_STDPERIPH_DRIVER,PROJECT_CFG_BUILD_PROFILE=1,PROJECT_CFG_DEBUG_WATCH_ENABLE=1,_DEBUG_` | 同 Keil Debug 宏 | 是 | 低 | 可迁移 |
| HSE_VALUE | `.uvprojx` 未定义；`stm32f10x.h` 在非 CL 设备默认 `8000000` | 不在 CMake 中覆盖，继续使用源码默认 | 是 | 中 | TODO：实物晶振频率需确认 |
| Include 路径 | `../Lib;../STM32F10x_.../drivers;../STM32F10x_.../inc;../Source;../Source/conf;../Source/easylogger/inc` | 同等相对路径 | 部分一致 | 中 | `../Lib` 当前不存在，保留为兼容路径并记录 TODO |
| Startup | `startup_stm32f10x_hd.s`，ARMASM 语法 | 新增 GCC startup，不覆盖旧文件 | 目标一致 | 高 | 需确认 HD startup 与 `STM32F10X_MD` 的历史组合 |
| Reset_Handler | ARMASM 中调用 `SystemInit` 后跳转 `__main` | GCC startup 调用 `SystemInit`、拷贝 `.data`、清 `.bss`、调用 `__libc_init_array` 和 `main` | 行为等价 | 中 | 新增 startup 检查文档 |
| Flash 起始 | `.uvprojx` IROM `0x08000000`；实际 sct App 为 `0x08004800` | App linker `ORIGIN=0x08004800` | 使用实际 App 值 | 高 | 以 IAP 安全规则和实际 sct 为准 |
| Flash 大小 | `.uvprojx` IROM `0x10000`；实际 sct `0x20000` | GCC App 代码区先限制到 `0x0801C000` 前 | 不完全一致 | 高 | 为保护参数区，GCC 不允许代码覆盖 `0x0801C000+` |
| RAM | `0x20000000` 长度 `0x5000` | `RAM ORIGIN=0x20000000 LENGTH=0x5000` | 是 | 低 | 可迁移 |
| IAP/Bootloader | IAP 起始 `0x08000000`，App 起始 `0x08004800` | 保留 `0x08004800`，烧录脚本默认禁止写 `0x08000000` | 是 | 高 | 必须保持 dry-run 和地址检查 |
| VTOR | `system_stm32f10x.c` 中 `_IAP` 时 `FLASH_BASE | 0x4800` | 保持 `_IAP` 来源于 `Project_Config.h`/`conf.h` | 是 | 高 | 构建必须保留 include/macro 传递链 |
| 参数/日志区 | `Flash.h` 使用 `0x0801C000` 到 `0x0801FFFF` | linker 预留 `FLASH_STORAGE`，不放置固件 section | Keil 未显式保护 | 高 | GCC linker 增加保护 |
| 输出文件 | `.axf` + after-build `.bin`，hex 未开启 | `.elf`、`.hex`、`.bin`、`.map` | 增强 | 低 | CMake post-build 生成 |
| Map/Size | Keil map + summary | `-Wl,-Map=firmware.map` + `arm-none-eabi-size` | 是 | 低 | 保留尺寸统计 |
| 优化等级 Release | Keil `Optim=1` | 先用 `-Os -g` | 不完全一致 | 中 | 不使用 `-O3`，后续对照尺寸与时序 |
| 优化等级 Debug | Keil `Optim=1` + Debug 宏 | `-Og -g3` | 不一致 | 中 | Debug 可调试优先，行为验证后再评估 |
| 单文件优化 | `LedBar.c` 文件级 `Optim=0` | `LedBar.c` 文件级 `-O0` | 是 | 中 | 迁移保留，降低 LED 时序风险 |
| 链接 GC | Keil 移除未用 section | `-ffunction-sections -fdata-sections -Wl,--gc-sections` | 行为近似 | 中 | 必须用 `KEEP` 保护向量表/初始化数组 |
| C 库 | Keil microlib | `--specs=nano.specs --specs=nosys.specs` | 不完全一致 | 中 | printf/retarget 需验证 |
| semihosting | 未见启用 | 不使用 semihosting，使用 `nosys.specs` | 是 | 中 | 调试不默认依赖 semihosting |
| ARMCC 语法 | 存在 `__asm void wait()` | 需要 `compiler_port.h` + 极小条件编译适配 | 否 | 高 | GCC 构建前必须修复 |
| 业务逻辑 | Keil 编译业务源码 | GCC 只并行构建，不改协议/BMS/SOC/低功耗逻辑 | 是 | 高 | 修改仅限构建、启动、兼容层 |

## 当前结论

- 可以安全开始 GCC/CMake 并行骨架，但必须把“芯片实际 Flash 容量”和“GCC 兼容语法”列为阻塞项跟踪。
- GCC linker 应以 `0x08004800` 为 App 起点，并显式预留 `0x0801C000` 起的持久化区域。
- 在 GCC 构建成功前不删除 Keil 工程；在功能验证完成前不修改业务逻辑。

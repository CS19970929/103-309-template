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
| 环境检查 | 已执行，未通过 | 当前机器缺少 CMake、Ninja、arm-none-eabi 工具链和调试/烧录工具 |
| Debug 构建 | 已尝试，未进入编译 | `scripts/build.py --config Debug` 阻塞于 `cmake not found` |

## 环境检查结果

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

## 已按验证结果修正的工具问题

| 问题 | 处理 |
|---|---|
| `build.py` 在缺少 `cmake` 时输出底层 Windows traceback | 改为明确输出 `cmake not found`，提示先运行 `scripts/check_env.py` |
| 新增脚本使用 Python 3.10 union 类型写法 | 已调整为 Python 3.9 可解析写法 |
| J-Link command file 使用了不可靠的 SWD 命令 | 已改为 `si SWD` |

## 后续必须继续验证

- 安装工具链后重新运行 `scripts/check_env.py`。
- 运行 `scripts/build.py --config Debug` 和 `scripts/build.py --config Release`。
- 若进入编译阶段失败，按顺序修复 include、宏、startup、linker、ARMCC/GCC 语法、printf/retarget、section 属性。
- 每修复一类问题必须更新本文档和对应对照文档。

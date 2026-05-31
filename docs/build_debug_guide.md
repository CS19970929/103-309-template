# 构建、烧录与调试指南

记录日期：2026-05-31

## 检查环境

Windows：

```powershell
python scripts\check_env.py
```

macOS：

```bash
python3 scripts/check_env.py
```

当前机器验证结果：必需工具已通过；可选的 `JLinkGDBServer`、`STM32_Programmer_CLI`、`openocd` 尚未安装。

## 构建 Debug

```powershell
python scripts\build.py --config Debug
```

当前机器也可明确使用：

```powershell
py -3.9 scripts\build.py --config Debug
```

等价 CMake 命令：

```powershell
cmake --preset gcc-debug
cmake --build --preset gcc-debug
```

目标产物：

- `build/gcc-debug/firmware.elf`
- `build/gcc-debug/firmware.hex`
- `build/gcc-debug/firmware.bin`
- `build/gcc-debug/firmware.map`

## 构建 Release

```powershell
python scripts\build.py --config Release
```

当前机器也可明确使用：

```powershell
py -3.9 scripts\build.py --config Release
```

等价 CMake 命令：

```powershell
cmake --preset gcc-release
cmake --build --preset gcc-release
```

Release 默认使用 `-Os`，不使用 `-O3`。

## 查看 size

```powershell
python scripts\size.py --config Debug
python scripts\size.py --config Release
```

CMake 构建后也会自动执行 `arm-none-eabi-size`。

当前已验证尺寸：

| 配置 | text | data | bss | dec | bin 大小 |
|---|---:|---:|---:|---:|---:|
| Debug | 63368 | 916 | 6852 | 71136 | 64284 |
| Release | 53488 | 908 | 6716 | 61112 | 54396 |

## 安全烧录

App 起始地址是 `0x08004800`。禁止把 App bin 裸写到 `0x08000000`。

默认 dry-run，不真正烧录：

```powershell
python scripts\flash.py --method jlink --config Release
python scripts\flash.py --method stlink --config Release
python scripts\flash.py --method openocd --config Release
```

实际烧录必须显式加 `--flash`：

```powershell
python scripts\flash.py --method jlink --config Release --flash
python scripts\flash.py --method stlink --config Release --flash
python scripts\flash.py --method openocd --config Release --flash
```

raw `.bin` 烧录地址默认是 `0x08004800`。脚本会拒绝 `0x08000000`，且不提供默认全片擦除。

已验证安全检查：

```powershell
python scripts\flash.py --method stlink --config Release --address 0x08000000
```

会输出：

```text
error: refuse to flash App image at 0x08000000; this would overwrite IAP
```

现有 Keil 产物仍优先使用仓库既有安全脚本：

```powershell
.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
```

## VS Code 调试

安装推荐插件后，打开 Run and Debug：

- `Debug STM32 with JLink`
- `Debug STM32 with STLINK`

两者都会先执行 `Build Debug`。当前 `device` 暂按 Keil 工程值 `STM32F103C8` 填写，但实际 Flash 容量和丝印仍需硬件确认。

ST-LINK 调试配置默认走 OpenOCD：

- `interface/stlink.cfg`
- `target/stm32f1x.cfg`

如果现场使用 ST-LINK GDB Server，可在本地另行添加 Cortex-Debug 配置，不要删除现有 OpenOCD 配置。

## ST-LINK 冒烟调试

当前已验证 OpenOCD + ST-LINK 路径：

```powershell
py -3.9 scripts\debug_smoke.py --config Release
```

脚本会复位目标、断点到 `main`、输出寄存器和 backtrace，然后 `reset run` 让板子继续运行。

本次上板报告见 `docs/stlink_debug_report.md`。

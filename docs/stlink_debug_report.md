# ST-LINK 上板烧录与调试报告

记录日期：2026-05-31

## 环境

| 项目 | 结果 |
|---|---|
| 调试器 | ST-LINK/V2，`VID:PID 0483:3748` |
| 固件版本 | `STLINK V2J37S7` |
| 目标电压 | 约 `3.29V` |
| OpenOCD | xPack OpenOCD `0.12.0-7` |
| GDB | Arm GNU Toolchain `arm-none-eabi-gdb 13.1.90` |
| 调试 ELF | `build/gcc-release/firmware.elf` |
| 烧录 BIN | `build/gcc-release/firmware.bin` |
| 烧录地址 | `0x08004800` |

## 芯片与 Flash 容量

OpenOCD 识别：

```text
device id = 0x20036410
flash size = 128 KiB
```

Flash size register：

```text
0x1ffff7e0: ffff0080
```

低 16 位 `0x0080` 表示 128KB Flash。由此确认当前板子 Flash 可覆盖 `0x08000000-0x0801FFFF`，但 Keil 工程设备名仍写 `STM32F103C8`，具体订货型号/丝印仍建议记录。

## 烧录前检查

```text
0x08000000: 20001b18 08000129 08000bb9 080007b9
0x08004800: 20001c08 08004949 0800b339 08008957
0x0801c000: 41464531 00300001 00000001 ffffd9d4
```

- `0x08000000` 为 IAP/Bootloader 向量。
- `0x08004800` 为旧 App 向量。
- `0x0801C000` 参数区存在有效数据。

## 烧录命令

```powershell
py -3.9 scripts\flash.py --method openocd --config Release --flash
```

结果：

```text
** Programming Started **
Info : flash size = 128 KiB
Warn : Adding extra erase range, 0x08011c7c .. 0x08011fff
** Programming Finished **
** Verify Started **
** Verified OK **
```

烧录范围未进入 `0x0801C000` 参数区，也未写 `0x08000000` IAP 区。

## 烧录后检查

```text
0x08000000: 20001b18 08000129 08000bb9 080007b9
0x08004800: 20005000 0800f80d 0800e565 0800e567
0x0801c000: 41464531 00300001 00000001 ffffd9d4
```

- IAP 向量保持不变。
- App 向量已更新为 GCC Release 固件。
- 参数区头部保持不变。

## GDB 调试结果

手工 GDB 调试和脚本化调试均已命中 `main`：

```text
Breakpoint 1, main () at .../Project/Source/main.c:94
94        InitDevice();
pc        0x08007590 <main>
sp        0x20005000
xpsr      0x61000000
#0 main () at .../Project/Source/main.c:94
```

运行态抽样：

```text
pc  = 0x0800d79c
msp = 0x20004ff0
xpsr = 0x01000000
```

PC 位于 App 区域，说明 IAP 已跳转到 App。

## 可重复脚本

新增：

```powershell
py -3.9 scripts\debug_smoke.py --config Release
```

脚本流程：

- 启动 OpenOCD。
- GDB 连接 `localhost:3333`。
- `monitor reset halt`。
- 检查 `0x08004800` App 向量。
- 设置 `main` 断点。
- `continue` 到 `main`。
- 输出 PC/SP/xPSR 和 backtrace。
- 删除断点，`monitor reset run`，detach。

## 未完成

- 未验证 J-Link。
- 未安装 STM32CubeProgrammer CLI。
- 未做串口 `COM4/19200/slave=1` 的上位机功能回归。
- 未做 CAN、SOC、低功耗、Flash 参数写入/恢复的完整功能回归。

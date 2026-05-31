# Flash 布局与持久化存储检查

记录日期：2026-05-31

## 地址来源

地址来自 `103 + 309/Project/Source/Flash.h` 和 Keil 生成 scatter。

## 布局

| 区域 | 起始地址 | 结束地址 | 用途 | GCC linker 处理 |
|---|---:|---:|---|---|
| IAP/Bootloader | `0x08000000` | `0x080047FF` | Bootloader/IAP | 不放置 App |
| App 固件 | `0x08004800` | `0x0801BFFF` | `.isr_vector/.text/.rodata/.data load` | `FLASH` 区 |
| AFE Slot A | `0x0801C000` | `0x0801C3FF` | AFE 参数 | 预留 |
| RW Param Slot A | `0x0801C400` | `0x0801C7FF` | RW 参数 | 预留 |
| AFE Slot B | `0x0801C800` | `0x0801CBFF` | AFE 参数备份 | 预留 |
| RW Param Slot B | `0x0801CC00` | `0x0801CFFF` | RW 参数备份 | 预留 |
| Log Slot A | `0x0801D000` | `0x0801D7FF` | BMS 日志 | 预留 |
| Log Slot B | `0x0801D800` | `0x0801DFFF` | BMS 日志备份 | 预留 |
| SOC Slot A | `0x0801E000` | `0x0801E7FF` | SOC 快照 | 预留 |
| SOC Slot B | `0x0801E800` | `0x0801EFFF` | SOC 快照备份 | 预留 |
| Upgrade Param Flag | `0x0801F000` | `0x0801F3FF` | 升级参数策略标志 | 预留 |
| Factory Aging Flag | `0x0801F400` | `0x0801F7FF` | 出厂老化状态 | 预留 |
| Update Flag | `0x0801F800` | `0x0801FBFF` | App/IAP 跳转标志 | 预留 |
| Sleep Flag | `0x0801FC00` | `0x0801FFFF` | 睡眠/唤醒标志 | 预留 |

## 发现

- 项目使用内部 Flash 做 AFE、RW 参数、日志、SOC、老化、升级、睡眠标志持久化。
- Keil scatter 未显式隔离 `0x0801C000` 后的存储区，当前 Keil Release ROM size `63952` bytes 尚未覆盖该区。
- GCC linker 必须把 App 代码区限制到 `0x0801C000` 前，避免未来固件变大时静默覆盖参数区。
- 烧录脚本默认不得全片擦除参数区；全片擦除必须显式参数确认。

## TODO

- TODO：确认实际 MCU Flash 容量是否支持 `0x0801FFFF`。若实际是 STM32F103C8 标称 64KB，则当前后 64K 存储布局不成立。

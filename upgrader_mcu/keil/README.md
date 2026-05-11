# 升级器 MCU Keil 工程

本目录是升级器 MCU 的 Keil MDK 工程，不是 BMS App 工程。

## 工程入口

- 工程文件：`upgrader_mcu/keil/UPG_F103C8.uvprojx`
- Target：`UPG_F103C8`
- 芯片：`STM32F103C8`
- Flash 起始地址：`0x08000000`
- Flash 范围：`0x08000000..0x0800FFFF`
- RAM 起始地址：`0x20000000`

## 打开方式

直接用 Keil 打开：

```powershell
& "C:\Keil_v5\UV4\UV4.exe" "upgrader_mcu\keil\UPG_F103C8.uvprojx"
```

命令行编译：

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -b "upgrader_mcu\keil\UPG_F103C8.uvprojx" -t "UPG_F103C8"
```

也可以继续使用仓库脚本编译，脚本输出路径固定为：

```powershell
.\tools\build_upgrader_mcu_f103c8.ps1 -Clean
```

```text
upgrader_mcu/build/f103c8/UPG_F103C8.bin
upgrader_mcu/build/f103c8/UPG_F103C8.axf
upgrader_mcu/build/f103c8/UPG_F103C8.map
```

## 源码分组

- `Startup`：STM32F10x 启动文件
- `Board`：STM32F103C8 串口、CAN、时钟、中断和主循环适配
- `Core`：升级器 MCU 协议、参数读写、IAP 长包升级状态机
- `StdPeriph`：升级器 MCU 当前用到的 STM32 标准外设库源文件

## 注意事项

- 这个工程烧录的是升级器 MCU 自己，不是 BMS 板的 IAP，也不是 BMS App。
- 升级器 MCU 固件可以烧录到自己的 `0x08000000`。
- BMS App 仍然必须烧录到 `0x08004800`，不要把 `FD_Release.bin` 裸写到 BMS 的 `0x08000000`。
- Keil 工程会在本目录生成 `Objects/` 和 `Listings/`，这些输出文件不纳入版本管理。

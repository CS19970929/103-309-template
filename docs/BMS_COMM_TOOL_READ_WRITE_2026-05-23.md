# comm tool 读写 BMS 功能

## 目标

PC 只连接 comm tool 串口，comm tool 通过 CAN 和 BMS App 通信，实现：

- 读取 BMS 运行信息和原始寄存器。
- 读取、写入保护参数和其它参数。
- 读取 BMS 事件日志。
- 保留原 BMS 串口寄存器地址、范围检查、写权限和参数副作用处理。

## 用户操作

推荐给用户使用图形上位机：

```powershell
.\tools\start_comm_tool_upgrade_ui.ps1 -Port COM4 -Baud 115200
```

界面使用顺序：

1. 点击 `连接检测`，确认能读到 comm tool 版本和缓存信息。
2. 点击 `读取BMS信息`，确认能看到 SOC、SOH、总压、单体电压、温度、电流、容量和故障字。
3. 在 `保护/其它参数` 表格中选择参数，点击 `读取选中` 或先读取整组参数。
4. 需要修改时填写 `写入值`，点击 `写入选中`，确认后写入。
5. 点击 `读取BMS日志` 查看板端事件记录。
6. 工程调试时才使用 `高级地址` 直接读写原始寄存器。

## CLI 调试

读取 SOC/SOH：

```powershell
py -3.9 tools\comm_tool_host.py bms-read --port COM4 --baud 115200 --address 0xD034 --count 2
```

读取 BMS 信息窗口：

```powershell
py -3.9 tools\comm_tool_host.py bms-read --port COM4 --baud 115200 --address 0xD000 --count 63 --long-timeout 90
```

写单个参数示例：

```powershell
py -3.9 tools\comm_tool_host.py bms-write --port COM4 --baud 115200 --address 0x2100 4200 --long-timeout 15
```

读取 BMS 事件日志：

```powershell
py -3.9 tools\comm_tool_host.py bms-read --port COM4 --baud 115200 --address 0xC008 --count 100 --long-timeout 90
```

## 写权限策略

BMS App 写入仍由 `Sci_Upper.c` 控制。当前项目要求 Release 默认开启上位机写保护参数和其它参数：

```c
PROJECT_CFG_HOST_WRITE_ENABLE 1
```

写入仍会检查寄存器地址、数量、范围和副作用。UI 只开放 `0x2100` 保护参数、`0x2300` 其它参数和日志读取，不开放校准等其它旧串口上位机功能。

## CAN 实现

- BMS App 标准帧请求 ID：`(CAN_ADRESS_STD_ID << 7) | 0x60`。
- BMS App 标准帧响应 ID：`(CAN_ADRESS_STD_ID << 7) | 0x61`。
- `READ_REG` 一帧读一个 16 位寄存器。
- `WRITE_PREP` + `WRITE_COMMIT` 两帧写一个 16 位寄存器。
- CAN 中断只缓存请求帧，寄存器读写在 `App_Can()` 主循环处理，避免中断里执行复杂逻辑。

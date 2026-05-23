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

1. 在 `实时监控` 页点击 `连接检测`，确认能读到 comm tool 版本和缓存信息。
2. 点击 `开始监控` 或 `读取一次`，确认能看到 SOC、SOH、总压、单体电压、温度、电流、容量和故障字。
3. 单体电压为 `0` 或 `61001` 时视为不存在，UI 不显示该串。
4. 在 `参数设置` 页点击 `读取保护参数`、`读取其它参数` 或 `一键读取`，参数会按旧上位机习惯分组显示为输入框。
5. 修改需要调整的输入框后点击 `写入修改`，UI 会逐项写入并回读校验。
6. 在 `存储信息` 页点击 `读取BMS日志` 查看板端事件记录。
7. 工程调试时才到 `其它功能` 页使用 `高级寄存器` 直接读写原始寄存器。

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

用户版 UI 的实时监控页默认每 2 秒读取一次。为避免 CAN 总线压力过高，间隔输入框下限固定为 1 秒；主窗口执行升级、写缓存、参数写入、读取 100 条日志等任务时，实时监控会等待任务结束后再继续读取。PC 端所有串口任务使用同一把锁排队打开 COM 口，避免实时监控和日志读取同时抢占 COM4 导致 Windows 返回 `PermissionError(13)`。

## CAN 实现

- BMS App 标准帧请求 ID：`(CAN_ADRESS_STD_ID << 7) | 0x60`。
- BMS App 标准帧响应 ID：`(CAN_ADRESS_STD_ID << 7) | 0x61`。
- `READ_REG` 一帧读一个 16 位寄存器。
- `WRITE_PREP` + `WRITE_COMMIT` 两帧写一个 16 位寄存器。
- CAN 中断只缓存请求帧，寄存器读写在 `App_Can()` 主循环处理，避免中断里执行复杂逻辑。

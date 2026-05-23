# comm tool 读写 BMS 功能

## 目标

PC 只连接 comm tool 串口，comm tool 通过 CAN 和 BMS App 通信，实现：

- 读取 BMS 运行信息和原始寄存器。
- 读取、写入常用 BMS 参数。
- 保留原 BMS 串口寄存器地址、范围检查、写权限和参数副作用处理。

## 用户操作

推荐给用户使用图形上位机：

```powershell
.\tools\start_comm_tool_upgrade_ui.ps1 -Port COM4 -Baud 115200
```

界面使用顺序：

1. 点击 `连接检测`，确认能读到 comm tool 版本和缓存信息。
2. 点击 `读取BMS信息`，确认能看到 SOC、SOH、总压、单体电压、温度、电流、容量和故障字。
3. 在 `常用参数` 下拉框选择参数，点击 `读取参数`。
4. 需要修改时填写 `新值`，点击 `写入参数`，确认后写入。
5. 工程调试时才使用 `高级地址` 直接读写原始寄存器。

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

## 写保护和调参固件

BMS App 写入仍由 `Sci_Upper.c` 控制。量产默认：

```c
PROJECT_CFG_HOST_WRITE_ENABLE 0
```

因此量产固件可以读取，但会拒绝写参数。需要给用户调参时，应单独提供打开写权限的调参固件，并保留 `Project_BuildGuard.h` 对量产默认配置的检查。

生成调参固件：

```powershell
.\tools\build_bms_param_app.ps1
```

输出文件：

```text
103 + 309\Project\Users\Objects\FD_Param.bin
```

`FD_Param.bin` 只用于调参或售后，不作为量产默认固件。量产 `FD_Release.bin` 仍保持 `PROJECT_CFG_HOST_WRITE_ENABLE=0`。

## CAN 实现

- BMS App 标准帧请求 ID：`(CAN_ADRESS_STD_ID << 7) | 0x60`。
- BMS App 标准帧响应 ID：`(CAN_ADRESS_STD_ID << 7) | 0x61`。
- `READ_REG` 一帧读一个 16 位寄存器。
- `WRITE_PREP` + `WRITE_COMMIT` 两帧写一个 16 位寄存器。
- CAN 中断只缓存请求帧，寄存器读写在 `App_Can()` 主循环处理，避免中断里执行复杂逻辑。

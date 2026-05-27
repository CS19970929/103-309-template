# CAN 老化模式和 SOC 常用控制

## 背景

CAN 上位机需要把写 SOC 作为常用功能单独展示，同时支持老化模式剩余时间上报，以及开启、关闭、重置老化时间三个独立控制动作。

## 固件侧约定

- 周期广播 `0x14F80208` 每 5 秒上报出厂信息，并新增老化状态和剩余分钟。
- 字节 2 为老化状态：`0` 停止/未运行，`1` 运行中，`2` 已完成。
- 字节 3~4 为老化剩余分钟，高字节在前。当前默认 3 天老化为 `4320` 分钟。
- `AGING_START 0x07`、`AGING_STOP 0x08`、`AGING_RESET_TIME 0x09` 是三个独立 App CAN 命令。
- `AGING_SET_HOURS 0x0A` 用于修改老化总时长，单位小时，范围 `1..168`；设置成功后会持久化新时长并重置累计老化时间。
- 关闭老化模式会提前结束本轮老化时间，板端保存完成状态并把剩余时间清零；后续上电不会自动恢复老化。
- 重置老化时间只清零累计时间。如果当前正在老化，清零后继续运行；如果已经停止或完成，则保持停止状态。
- 升级包需要自动重置老化时间时，通过 `Project_Config.h` 的 `PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME` 控制；默认关闭，开启时必须同步递增 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION`。

## 上位机入口

用户版 exe 固定生成在：

```text
dist\BMS_CommTool_Upgrade_UI.exe
```

CAN 用户上位机必须在原 `BMS_CommTool_Upgrade_UI.exe` 基础上改，软件名字不变，不要另起新的 exe 名称。只要更新用户上位机代码，必须重新编译 exe 并直接覆盖原文件：

```powershell
.\tools\build_comm_tool_upgrade_ui_exe.ps1 -Clean
```

写 SOC 是单独常用功能，不要求用户输入寄存器地址：

```powershell
其它功能 -> 常用功能 -> 写SOC
```

老化模式三个动作必须是原 UI 里的独立按钮：

- 开启老化模式
- 关闭老化模式：提前结束本轮老化时间，剩余时间变为 0
- 重置老化时间

修改老化时间也必须在原 UI 常用功能里单独展示，用户输入单位为小时。点击后通过 comm tool 串口协议 `0x14 BMS_AGING_SET_HOURS` 下发，板端持久化新时长并自动重置老化累计时间。

老化剩余时间必须在原 UI 里单独可见：

- `其它功能 -> 常用功能 -> 读取老化时间`
- 该按钮通过 comm tool 串口协议 `0x13 BMS_AGING_STATUS` 等待 `0x14F80208` 广播，并显示老化状态和剩余分钟。

监听广播时，上位机会在 `ch=8` 输出老化状态和剩余时间：

```powershell
.\tools\start_can_bms_host.ps1 -Mode listen -Duration 15
```

## 地址与权限

- 写 SOC 底层固定写 `0x1005 RS485_CMD_ADDR_SET_ONCE_SOC`，范围 `0..100`。
- 老化控制命令带 `0xA9` 防误触发字节，并校验 `CAN_ADRESS_STD_ID`。
- PC 到 comm tool 串口协议新增 `0x12 BMS_AGING_CTRL`，由 comm tool 转发到 BMS App CAN 服务。
- PC 到 comm tool 串口协议新增 `0x13 BMS_AGING_STATUS`，由 comm tool 等待并解析 `0x14F80208` 广播，供原 UI 显示老化剩余时间。
- PC 到 comm tool 串口协议新增 `0x14 BMS_AGING_SET_HOURS`，payload 为 `hours:u16_le`，由 comm tool 转发为 BMS App `0x0A AGING_SET_HOURS`。
- CAN App 服务仍在主循环中调用 `Sci_HostWriteWords()`，不在 CAN 中断里写 Flash 或修改业务状态。

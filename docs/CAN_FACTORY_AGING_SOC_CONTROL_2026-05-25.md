# CAN 老化模式和 SOC 常用控制

## 背景

CAN 上位机需要把写 SOC 作为常用功能单独展示，同时支持老化模式剩余时间上报，以及开启、关闭、重置老化时间三个独立控制动作。

## 固件侧约定

- 周期广播 `0x14F80208` 每 5 秒上报出厂信息，并新增老化状态和剩余分钟。
- 字节 2 为老化状态：`0` 停止/未运行，`1` 运行中，`2` 已完成。
- 字节 3~4 为老化剩余分钟，高字节在前。当前默认 3 天老化为 `4320` 分钟。
- `AGING_START 0x07`、`AGING_STOP 0x08`、`AGING_RESET_TIME 0x09` 是三个独立 App CAN 命令。
- 关闭老化模式会保存停止状态，后续上电不会自动恢复老化；需要再次发送开启命令。
- 重置老化时间只清零累计时间。如果当前正在老化，清零后继续运行；如果已经停止或完成，则保持停止状态。

## 上位机入口

用户版 exe 固定生成在：

```text
dist\BMS_CAN_Host_UI.exe
```

只要更新 CAN 用户上位机代码，必须重新编译 exe：

```powershell
.\tools\build_can_bms_host_ui_exe.ps1 -Clean
```

写 SOC 是单独常用功能，不要求用户输入寄存器地址：

```powershell
.\tools\start_can_bms_host.ps1 -Mode app-write-soc -Soc 80 -ConfirmWriteSoc
```

老化模式三个动作必须单独调用：

```powershell
.\tools\start_can_bms_host.ps1 -Mode app-aging-start -ConfirmAgingStart
.\tools\start_can_bms_host.ps1 -Mode app-aging-stop -ConfirmAgingStop
.\tools\start_can_bms_host.ps1 -Mode app-aging-reset-time -ConfirmAgingResetTime
```

监听广播时，上位机会在 `ch=8` 输出老化状态和剩余时间：

```powershell
.\tools\start_can_bms_host.ps1 -Mode listen -Duration 15
```

## 地址与权限

- 写 SOC 底层固定写 `0x1005 RS485_CMD_ADDR_SET_ONCE_SOC`，范围 `0..100`。
- 老化控制命令带 `0xA9` 防误触发字节，并校验 `CAN_ADRESS_STD_ID`。
- CAN App 服务仍在主循环中调用 `Sci_HostWriteWords()`，不在 CAN 中断里写 Flash 或修改业务状态。

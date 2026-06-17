# BMS 直连串口老化控制补充

日期：2026-06-17

## 变更范围

- BMS App Modbus 新增老化状态只读窗口 `0xC080`，长度 5 个寄存器：
  - `0xC080`：老化状态，沿用 `FactoryAging_GetState()` 返回值。
  - `0xC081`：剩余分钟，向上取整并饱和到 `0xFFFF`。
  - `0xC082..0xC083`：剩余秒数 `uint32` 高/低字。
  - `0xC084`：当前老化总时长，单位小时。
- BMS App Modbus 新增两个单寄存器写命令：
  - `0x1008=0x005A`：重置老化累计时间。
  - `0x1009=1..168`：修改老化总时长，并重置累计时间。
- `BMS_CommTool_Upgrade_UI.exe` 的 `BMS直连串口` 模式接入：
  - `读取老化时间` 读 `0xC080`。
  - `重置老化时间` 写 `0x1008=0x005A` 后回读状态。
  - `修改老化时间` 写 `0x1009=hours` 后回读状态。

## 未变更

- `comm tool/CAN桥` 模式仍使用原 `BMS_AGING_STATUS`、`BMS_AGING_CTRL`、`BMS_AGING_SET_HOURS`。
- 本次不实现直连串口 CAN 诊断、缓存升级或串口 IAP 数据下载。
- 不修改老化状态机、BKP/Flash 保存格式和 `0x14F80208` CAN 广播格式。

## 验证

- 上位机自测：`py -3 tools\comm_tool_upgrade_ui.py --self-test`
- 上位机语法检查：`py -3 -m py_compile tools\comm_tool_upgrade_ui.py`
- 上位机 exe 构建：`powershell -ExecutionPolicy Bypass -File tools\build_comm_tool_upgrade_ui_exe.ps1`
- BMS App 目标化编译：`FD_Release`

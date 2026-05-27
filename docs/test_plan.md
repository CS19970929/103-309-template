# Test Plan

状态：部分验证

## 老化模式提前结束

参考源码：

- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `tools/comm_tool_upgrade_ui.py`
- `tools/can_bms_host.py`

验证步骤：

1. 编译 BMS App Release，确认无错误。
2. 编译 `dist/BMS_CommTool_Upgrade_UI.exe`，确认输出文件名保持 `BMS_CommTool_Upgrade_UI.exe`。
3. 使用 CAN 用户上位机开启老化模式，读取老化时间，应显示状态为运行且剩余时间大于 0。
4. 点击 `关闭老化模式`，确认提示中说明会提前结束本轮老化时间。
5. 关闭命令完成后读取老化时间，应显示状态为完成、剩余时间为 0。
6. 断电重启后再次读取老化时间，应保持完成状态且不会自动恢复老化。
7. 如需再次开始老化，先执行 `重置老化时间` 或 `修改老化时间`，再确认状态重新进入运行。

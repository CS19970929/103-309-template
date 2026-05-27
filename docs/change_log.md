# Change Log

状态：部分验证

## 2026-05-27

- 老化模式 `关闭老化模式` 命令语义调整为“提前结束本轮老化时间”：板端调用完成路径，持久化 `DONE` 状态，并让剩余时间归零。
- CAN 用户上位机关闭老化模式确认文案改为明确提示“提前结束本轮老化时间，剩余时间将变为 0”。
- 同步更新 CAN App 服务、comm tool 串口协议和用户上位机说明文档。

参考源码：

- `103 + 309/Project/Source/FactoryAging.c`
- `tools/comm_tool_upgrade_ui.py`
- `tools/can_bms_host.py`
- `tools/comm_tool_host.py`

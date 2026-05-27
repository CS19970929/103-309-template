# Change Log

状态：部分验证

## 2026-05-27

- D009 同步老化模式 `关闭老化模式` 命令语义：提前结束本轮老化时间，板端持久化 `DONE` 状态，并让剩余时间归零。
- D009 老化模式 `开启老化模式` 在完成态下会清零累计时间并开启新一轮，避免完成态返回 `BMS_ERROR`。
- CAN 用户上位机关闭老化模式确认文案改为明确提示“提前结束本轮老化时间，剩余时间将变为 0”。
- 同步更新 CAN App 服务、comm tool 串口协议和用户上位机说明文档。

参考源码：

- `103 + 309/Project/Source/FactoryAging.c`
- `tools/comm_tool_upgrade_ui.py`
- `tools/can_bms_host.py`
- `tools/comm_tool_host.py`

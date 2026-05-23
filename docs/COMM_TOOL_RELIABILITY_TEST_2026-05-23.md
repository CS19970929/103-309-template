# comm tool/BMS 升级可靠性测试

## 日志结论

用户日志中两次升级都已经成功：

- `state=2 percent=100% error=0x00` 表示 BMS IAP 已完成写入和校验。
- `BMS App 第 2 次确认成功` 表示升级后 App 复位跳转正常，只是第一次轮询时 App 还未完全恢复。
- 后续 `读取BMS信息` 返回 `BMS_ERROR`，问题点在 App 恢复后的信息读取链路，不是 IAP 写入失败。

旧版本 `读取BMS信息` 会连续发 63 次单寄存器 CAN 请求。升级后 App 刚恢复时，这种读法容易因为串口响应缓存忙、CAN 队列或时序问题失败。当前版本改为 CAN App `READ_BLOCK` 块读：BMS App 一次生成 63 个寄存器快照，再分帧返回，适合 UI 状态页和实时监控。

## 自动测试脚本

只做连接和只读状态测试：

```powershell
.\tools\start_comm_tool_reliability_test.ps1 -Port COM4 -Baud 115200 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin"
```

批次开始前，把当前 App 写入 comm tool 缓存并验证：

```powershell
.\tools\start_comm_tool_reliability_test.ps1 -Port COM4 -Baud 115200 -DownloadCache -ConfirmDownload
```

使用已验证缓存连续升级 3 次：

```powershell
.\tools\start_comm_tool_reliability_test.ps1 -Port COM4 -Baud 115200 -UpgradeLoops 3 -ConfirmUpgrade
```

脚本检查项：

- comm tool 版本、协议、CAN 波特率和缓存区。
- CAN 诊断计数清零和测试后统计。
- BMS App `0xD034/0xD035` SOC/SOH 响应。
- BMS App `0xD000` 起 63 个寄存器块读快照。
- 可选校验所选 bin 与 comm tool 缓存 CRC 是否一致。
- 可选执行多轮缓存升级，并在每轮升级后等待 BMS App 恢复响应，再读取完整快照。

## 人工故障模式

以下测试需要现场硬件配合，自动脚本不能替代人工断电/断线动作：

| 模式 | 操作 | 期望 |
| --- | --- | --- |
| App 正常运行 | 直接执行只读测试 | `BMS App read-only check` 通过 |
| BMS 已在 IAP | 执行缓存升级 | 能完成 `HELLO/START/DATA/END` 并跳 App |
| 批量升级 | 批次开始只写一次缓存，多块 BMS 使用缓存升级 | 每块板升级前都会校验缓存 CRC |
| 升级中断电 | 传输中切断 BMS 电源后重新上电 | BMS 留在 IAP，不跳坏 App；重新执行升级可恢复 |
| CAN 断线 | 升级前断开 CAN | comm tool 返回 CAN 超时，BMS 不擦写有效 App |
| 错误 bin | 选择地址/向量非法的 bin | PC 工具在下载前拒绝 |

## 通过标准

- `comm_tool_reliability_test.py` 只读模式连续 10 次无 `BMS_ERROR`。
- `使用缓存升级` 连续升级不少于 10 次，均出现 `state=2 percent=100% error=0x00`，且升级后 20 秒内 BMS App 恢复响应。
- 人工断电测试后，BMS 不能死机；重新执行缓存升级可以成功恢复。
- UI 实时监控开启时，主窗口执行升级会自动暂停监控，升级完成后可继续刷新。

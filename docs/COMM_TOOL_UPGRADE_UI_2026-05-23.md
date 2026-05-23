# comm tool 升级上位机

## 启动方式

固定使用仓库脚本启动，避免双击 Python 文件导致环境不一致：

```powershell
.\tools\start_comm_tool_upgrade_ui.ps1 -Port COM4 -Baud 115200
```

默认升级文件为：

```text
103 + 309\Project\Users\Objects\FD_Release.bin
```

也可以显式指定：

```powershell
.\tools\start_comm_tool_upgrade_ui.ps1 -Port COM4 -Baud 115200 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin"
```

## 一键升级流程

UI 的“一键升级”不是直接执行缓存升级，而是固定执行以下流程：

1. 校验当前选择的 BMS App bin。
2. 检查 App 起始地址固定为 `0x08004800`。
3. 检查向量表：MSP 必须在 SRAM，ResetHandler 必须在 App Flash 范围。
4. 将当前选择的 bin 写入 comm tool Flash 缓存。
5. 读取 comm tool 缓存信息，并核对大小、CRC16、CRC32。
6. 缓存核对一致后，通过 CAN 触发 BMS IAP 升级。
7. 升级完成后最多等待 15 秒，重试读取 `0xD000/0xD001`，确认 BMS App 已恢复响应。

这样可以避免用户只点击升级命令却使用 comm tool 旧缓存。

如果第 6 步已经返回 `state=2, percent=100%, error=0x00`，代表 CAN-IAP 写入和校验已经完成。第 7 步只是 App 复位后的在线确认；如果 BMS 复位跳 App 慢，UI 会重试等待，不会把已经成功的升级误报成失败。

## 按钮说明

- `连接检测`：读取 comm tool 固件版本、CAN 波特率和缓存区域。
- `校验文件`：只检查当前选择的 bin，不写入硬件。
- `读取缓存`：读取 comm tool 当前缓存固件信息。
- `写入缓存`：只把当前 bin 写入 comm tool，并核对 CRC。
- `一键升级`：写入当前 bin 后立即升级 BMS。
- `读取BMS状态`：读取 `0xD000/0xD001`。
- `CAN诊断`：读取 comm tool CAN 统计信息。

## 使用约束

- 不允许把 App 当作 IAP 烧到 `0x08000000`。
- 用户升级 BMS 时必须选择 App bin，正常使用 `FD_Release.bin`。
- `FD_Debug.bin` 只用于调试，不建议给普通用户升级。
- 串口是 PC 到 comm tool 的链路；BMS 升级仍通过 comm tool 的 CAN 完成。

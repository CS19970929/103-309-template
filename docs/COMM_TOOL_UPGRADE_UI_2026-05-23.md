# comm tool 升级上位机

## 启动方式

固定使用仓库脚本启动，避免双击 Python 文件导致环境不一致：

```powershell
.\tools\start_comm_tool_upgrade_ui.ps1 -Port COM4 -Baud 115200
```

给用户使用的 exe 打包命令：

```powershell
.\tools\build_comm_tool_upgrade_ui_exe.ps1 -Clean
```

生成位置：

```text
dist\BMS_CommTool_Upgrade_UI.exe
```

如果 exe 保持在仓库的 `dist` 目录下运行，会默认选择仓库内的 `103 + 309\Project\Users\Objects\FD_Release.bin`。如果复制到其他目录给用户使用，需要用户在界面里手动选择 BMS App bin。

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
7. 升级完成后最多等待 15 秒，重试读取 `0xD034/0xD035`，确认 BMS App 已恢复响应。

这样可以避免用户只点击升级命令却使用 comm tool 旧缓存。

如果第 6 步已经返回 `state=2, percent=100%, error=0x00`，代表 CAN-IAP 写入和校验已经完成。第 7 步只是 App 复位后的在线确认；如果 BMS 复位跳 App 慢，UI 会重试等待，不会把已经成功的升级误报成失败。

## 批量升级流程

批量给多块 BMS 升级时，不需要每块板都重新把 bin 写入 comm tool。推荐流程：

1. 选择本批次要使用的 BMS App bin。
2. 点击 `写入缓存`，只在批次开始前写入一次 comm tool Flash 缓存。
3. 换上第一块 BMS，点击 `使用缓存升级`。
4. 每换一块 BMS，继续点击 `使用缓存升级`。
5. 只有更换了 bin 文件，或者不确定 comm tool 缓存内容时，才重新点击 `写入缓存` 或 `一键升级`。

`使用缓存升级` 会先读取 comm tool 缓存，并核对缓存大小、CRC16、CRC32 是否和当前选择的 bin 完全一致；不一致时会拒绝升级，防止把旧缓存误刷到 BMS。

## 按钮说明

- `连接检测`：读取 comm tool 固件版本、CAN 波特率和缓存区域。
- `校验文件`：只检查当前选择的 bin，不写入硬件。
- `读取缓存`：读取 comm tool 当前缓存固件信息。
- `写入缓存`：只把当前 bin 写入 comm tool，并核对 CRC。
- `一键升级`：写入当前 bin 后立即升级 BMS。
- `使用缓存升级`：批量升级入口，跳过串口写缓存，但会先核对缓存 CRC 和当前文件一致。
- `读取BMS状态`：读取 `0xD034/0xD035`，显示 SOC/SOH。
- `读取BMS信息`：读取 `0xD000` 起 63 个只读寄存器，显示 SOC、SOH、总压、电流、单体最大/最小/压差、温度、容量、故障字和均衡字。
- `实时监控`：打开独立监控窗口，默认每 2 秒读取 `0xD000` 起 63 个只读寄存器，实时显示每串电压、总压、电流、SOC、SOH、温度、容量、故障字和均衡字。主窗口执行写缓存、升级、参数读写等任务时，监控窗口会自动暂停，避免串口被两个任务同时占用。
- `保护/其它参数`：只显示 `0x2100` 保护参数和 `0x2300` 其它参数，支持读取选中、写入选中、读取保护参数、读取其它参数、读取全部参数。
- `读取BMS日志`：读取 `0xC008` 起 100 条事件记录，并按旧串口上位机的事件名和间隔格式显示。
- `高级地址`：工程调试入口，可以按寄存器地址和数量读取，也可以写入一组原始寄存器值。
- `CAN诊断`：读取 comm tool CAN 统计信息。

## 参数和日志

UI 当前只开放保护参数和其它参数，不开放校准、AFE、蓝牙/WiFi 等旧上位机其它功能。地址仍然是 BMS 原串口寄存器地址。

| 分组 | 地址范围 |
| --- | --- |
| 保护参数 | `0x2100` 起，13 组保护参数，每组一级/二级/三级/恢复/延时 |
| 其它参数 | `0x2300` 起，均衡、短路、限流、休眠、SOC、系统参数 |
| 事件日志 | `0xC008` 起 100 条，每条 1 个寄存器，高字节事件、低字节间隔 |

写参数仍由 BMS App 的 `Sci_Upper.c` 做地址、范围和副作用处理。当前项目要求 `PROJECT_CFG_HOST_WRITE_ENABLE=1`，Release 固件也允许通过上位机写保护参数和其它参数。

## 使用约束

- 不允许把 App 当作 IAP 烧到 `0x08000000`。
- 用户升级 BMS 时必须选择 App bin，正常使用 `FD_Release.bin`。
- `FD_Debug.bin` 只用于调试，不建议给普通用户升级。
- 串口是 PC 到 comm tool 的链路；BMS 升级仍通过 comm tool 的 CAN 完成。

# PC 上位机工具规划

日期：2026-05-21

## 1. 工具形态

第一版先实现命令行工具，路径规划：

```text
pc_tool/comm_tool_host.py
```

GUI 在命令行协议稳定后再实现。

## 2. 固定启动方式

后续需要新增固定启动脚本：

```powershell
.\tools\start_comm_tool_host.ps1
```

不要直接双击 Python 文件。脚本负责选择已验证 Python 环境和依赖。

## 3. 主要功能

| 功能 | 命令行模式 |
|---|---|
| 检测 Comm Tool | `info` |
| 查询 BMS | `bms-read` |
| 写 BMS 单寄存器 | `bms-write` |
| 写 BMS 多寄存器 | `bms-write-multi` |
| 下载固件到 Comm Tool | `fw-download` |
| 查询缓存固件 | `fw-info` |
| 一键升级 BMS | `upgrade` |
| 查询升级状态 | `upgrade-status` |
| 中止升级 | `upgrade-abort` |

## 4. 典型命令

```powershell
.\tools\start_comm_tool_host.ps1 -Port COM4 -Mode info

.\tools\start_comm_tool_host.ps1 -Port COM4 -Mode bms-read -Address 0xD000 -Count 63

.\tools\start_comm_tool_host.ps1 -Port COM4 -Mode fw-download -Bin ".\build\bms_next_app.bin"

.\tools\start_comm_tool_host.ps1 -Port COM4 -Mode upgrade -NodeId 1
```

## 5. 安全要求

1. 固件下载前必须显示文件长度和 CRC。
2. 一键升级前必须确认 Comm Tool 内部固件 valid。
3. PC 工具不能直接发送裸 Flash 地址擦写命令。
4. 真实升级必须显示目标 node id、App 地址、固件长度、CRC。
5. dry-run 必须作为默认可用模式。

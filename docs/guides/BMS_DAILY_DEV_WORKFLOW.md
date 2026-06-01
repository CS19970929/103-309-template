# BMS 日常开发工作流

本文记录当前仓库推荐的日常开发、快速测试、ST-Link 调试和安全烧录入口。目标是把容易出错的步骤固定到本地脚本里，尤其是 App 烧录地址 `0x08004800` 和 IAP 地址 `0x08000000` 的隔离。

这些工具默认不需要 AI。AI 只在你需要分析问题时读取压缩后的日志包。

## 一键入口

主入口：

```powershell
.\tools\bms_dev_workflow.ps1 -Mode quick
```

本地 UI：

```powershell
.\tools\start_bms_dev_dashboard.ps1
```

UI 面板只是调用本文脚本并显示输出。除非手动勾选“允许烧录 App”并点击“安全烧录”，否则不会写 Flash。

常用模式：

```powershell
# 只跑静态配置检查、SOC Python 回放、SOC C host 测试
.\tools\bms_dev_workflow.ps1 -Mode quick

# 只编译 Keil Release 目标
.\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release

# 只用 ST-Link/OpenOCD/GDB 读取运行快照
.\tools\bms_dev_workflow.ps1 -Mode probe -Target FD_Release

# 深度验证 SOC 主链路：额外硬件断点 SOC_IntEnhance_Ctrl 和 soc_update_rest_timer
.\tools\bms_dev_workflow.ps1 -Mode probe -Target FD_Release -DeepProbe

# 编译并安全烧录 App 到 0x08004800
.\tools\bms_dev_workflow.ps1 -Mode flash -Target FD_Release

# 全流程：检查 + SOC 回归 + 编译；加 -Flash/-Probe 才会烧录和硬件快照
.\tools\bms_dev_workflow.ps1 -Mode full -Target FD_Release -Flash -Probe

# 全流程并做 SOC 断点验证
.\tools\bms_dev_workflow.ps1 -Mode full -Target FD_Release -Flash -Probe -DeepProbe
```

串口在线 SOC 采样：

```powershell
.\tools\bms_dev_workflow.ps1 -Mode quick -Port COM4 -Baud 19200 -Slave 1 -OnlineSamples 10 -OnlineInterval 0.5
```

`Mode quick` 默认允许 `tools/project_check.py` 报出调试态配置问题后继续跑 SOC 测试。提交或出货前使用严格检查：

```powershell
.\tools\bms_dev_workflow.ps1 -Mode quick -StrictProjectCheck
```

## ST-Link 快照

独立入口：

```powershell
.\tools\stlink_snapshot.ps1 -Elf "103 + 309\Project\Users\Objects\FD_Release.axf" -SocBreak -RestFinish
```

该脚本会：

- 启动 OpenOCD，配置 `interface/stlink.cfg` + `target/stm32f1x.cfg`。
- 使用 `arm-none-eabi-gdb` 加载 AXF 符号。
- 读取寄存器、Fault 寄存器、App 向量表和 SOC 关键全局变量。
- 可选硬件断点命中 `SOC_IntEnhance_Ctrl`。
- 可选在 `soc_update_rest_timer` 入口和返回后读取静置计数，用于确认自耗不阻止静置 OCV。

为规避 GDB 对中文/空格路径的解析问题，脚本会把 AXF 临时复制到 `%TEMP%` 的 ASCII 路径，只作为符号文件使用，不修改仓库。

## 长期监控

长期监控入口：

```powershell
# 每 5 分钟跑一次 quick，本地一直跑，日志写入 logs\bms_watch
.\tools\bms_watch.ps1 -Mode quick -IntervalSeconds 300

# 每 10 分钟做一次 ST-Link 只读快照，跑 12 次后结束
.\tools\bms_watch.ps1 -Mode probe -IntervalSeconds 600 -Count 12

# 深度 SOC 断点验证，适合短时间定位问题，不建议长期开
.\tools\bms_watch.ps1 -Mode deep-probe -IntervalSeconds 600 -Count 3
```

长期监控不会自动烧录，也不会修改固件源码。生成日志位于 `logs\bms_watch`，该目录已加入 `.gitignore`。

## 给 AI 的最小日志包

本地测试或长期监控发现问题后，生成一个小日志包再交给 AI：

```powershell
.\tools\bms_collect_ai_logs.ps1
```

输出文件位于 `logs\bms_watch\ai_context_*.md`，内容只包含：

- `git status --short`
- 最近 5 个提交
- 关键构建/SOC 配置
- `project_check.py -q` 输出
- 最近 5 个本地监控日志的尾部

这样 AI 只看问题现场，不需要读取完整仓库，能减少 token 和误判。

## 烧录安全

所有 App 烧录必须走：

```powershell
.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
```

脚本固定检查 App 地址必须是 `0x08004800`。禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`，否则会覆盖 IAP。

## 可选 AI 口令

可以直接对 Codex 说：

- “跑 BMS quick 工作流”
- “编译 Release 并用 ST-Link 做运行快照，不烧录”
- “安全烧录当前 Release App 后验证 SOC 静置 OCV 链路”
- “提交前按严格模式检查项目配置”

需要 AI 介入时，优先把 `logs\bms_watch\ai_context_*.md` 发给 AI；让 AI 根据日志判断，而不是重新扫描整个项目。

## 对量产代码的影响

新增内容集中在 `tools/`、`.gitignore` 和本文档，不改变 `103 + 309/Project/Source/` 下的固件逻辑。

会影响板子的操作只有：

- `bms_dev_workflow.ps1 -Mode flash`
- `bms_dev_workflow.ps1 -Mode full -Flash`
- UI 中勾选“允许烧录 App”后的“安全烧录”

这些烧录入口仍调用 `tools\soc_flash_app_safe.ps1`，固定检查 App 地址为 `0x08004800`，不会裸写 `0x08000000`。

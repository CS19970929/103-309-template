# Debug 调试实现与 Release 量产隔离说明

文档状态：已按源码部分验证
最后更新时间：2026-06-04

参考源码：
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/DebugHooks.c`
- `103 + 309/Project/Source/DebugHooks.h`
- `103 + 309/Project/Source/DebugWatch.c`
- `103 + 309/Project/Source/SystemDebug.c`
- `103 + 309/Project/Source/IrqDebug.c`
- `103 + 309/Project/Source/StartupDefaultHandler.c`
- `103 + 309/Project/Source/conf/Project_BuildGuard.h`
- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- `tools/project_check.py`

## 目标

`FD_Debug` 保留 Keil Watch、SystemDebug 快照、运行事件、profile 和 IRQ 计数能力；`FD_Release` 不编译这些调试实现文件，量产业务代码中只保留空 hook，不再夹杂 `SystemDebug_Event()`、profile 记录和 debug print 细节。

## 当前实现

- `Runtime.c` 只调用 `DebugHooks_Runtime*()`，不再直接调用 `SystemDebug_Event()`、`SystemDebug_ProfileRecord()`、`SystemDebug_GetCycleCount()` 或 `DbgPrint_Summary()`。
- `DebugHooks.h` 在 `PROJECT_CFG_DEBUG_MONITOR_ENABLE=0` 时把所有 hook 编译为空宏，Release 主流程不生成调试调用实体。
- `DebugHooks.c` 集中保存运行事件、profile、模块心跳和 debug print 周期输出，并只在 Debug 相关开关打开时提供实现。
- `Runtime_DebugWatchBind()` 已迁入 `DebugHooks.c`，继续把 runtime app 状态挂到 `g_dbg_watch.runtime.app`。
- Keil `FD_Debug` target 包含 `DebugHooks.c`、`DebugWatch.c`、`SystemDebug.c`、`IrqDebug.c`。
- Keil `FD_Release` target 对上述 Debug-only 源文件设置 `IncludeInBuild=0`，不参与编译和链接。
- Keil `FD_Debug` 输出目录改为 `Objects_Debug` / `Listings_Debug`，避免与 `FD_Release` 共用 `.o` 中间文件导致增量构建串目标。
- `StartupDefaultHandler.c` 在 `PROJECT_CFG_IRQ_DEBUG_ENABLE=0` 时提供启动默认异常回调的 no-op 兜底，避免 Release 为了一个空回调去编译 `IrqDebug.c`。
- `tools/project_check.py` 会检查 Release 不得构建 Debug-only 源文件、Debug 必须构建这些源文件，并检查 `Runtime.c` 不得重新出现调试实现 token。

## 使用方式

Debug 调试时使用 Keil `FD_Debug` target，在 Watch 中添加：

```c
g_dbg_watch
```

系统快照从下面入口展开：

```c
g_dbg_watch.system.snapshot
```

IRQ 计数从下面入口展开：

```c
g_dbg_watch.system.irq
```

Release 量产时使用 Keil `FD_Release` target。该 target 中 Debug-only 源文件必须保持 `IncludeInBuild=0`，不参与编译和链接；即使 `Runtime.c` 保留 hook 调用，最终也会被空宏替换。

## 检查命令

```powershell
py -3.9 tools\project_check.py
rg "SystemDebug_Event|SystemDebug_ProfileRecord|SystemDebug_GetCycleCount|DbgPrint_Summary|DBG_PROFILE_|DBG_MODULE_" "103 + 309/Project/Source/Runtime.c"
powershell -ExecutionPolicy Bypass -File tools\bms_dev_workflow.ps1 -Mode build -Target FD_Debug
powershell -ExecutionPolicy Bypass -File tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release
Select-String -Path "103 + 309\Project\Users\Objects\FD_Release.lnp" -Pattern "debughooks|debugwatch|systemdebug|irqdebug"
```

## 风险边界

- 本次不改变协议寄存器、CAN 帧、Flash 地址、保护阈值、SOC 算法和低功耗策略。
- Debug 功能仍可能增加 `FD_Debug` 的 RAM/Flash 占用；该成本不进入 `FD_Release`。
- 如果后续新增调试功能，应优先放入 Debug-only 源文件或 `DebugHooks.c`，不要把事件和 profile 细节重新写回业务模块。

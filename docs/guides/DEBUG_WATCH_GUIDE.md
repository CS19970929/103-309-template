# Keil Watch 调试观察入口说明

## 目标

变量重构后，模块内部状态优先保持 `static runtime` 封装，避免为了在线调试把业务状态全部改成裸全局变量。

需要 Keil Watch 观察时，使用 `PROJECT_CFG_DEBUG_WATCH_ENABLE` 导出只用于调试的 `g_dbg_*` 指针。

## 配置开关

默认配置：

```c
#define PROJECT_CFG_DEBUG_WATCH_ENABLE 0
```

规则：

- 量产 Release 必须保持 `0`。
- `Project_BuildGuard.h` 会阻止 Release 构建打开该开关。
- 开启后只增加观察符号，不改变业务状态更新逻辑。
- 不为了 Watch 给普通业务变量添加 `volatile`。

## 当前观察入口

### LedBar

开启 `PROJECT_CFG_DEBUG_WATCH_ENABLE` 后，Keil Watch 可添加：

```c
g_dbg_ledbar_runtime
g_dbg_ledbar_runtime->number
g_dbg_ledbar_runtime->indicator_mask
g_dbg_ledbar_runtime->scan_index
g_dbg_ledbar_runtime->soc_display_10ms
g_dbg_ledbar_runtime->key_hold_10ms
g_dbg_ledbar_runtime->mcu_wk_active
```

### 飞道 CAN

当前 CAN 诊断优先看 `SystemDebug` 的 `g_dbg.can`；旧的 `g_dbg_feidao_can_runtime` 指针不再作为当前入口。

```c
g_dbg.can.power_on
g_dbg.can.bus_off
g_dbg.can.tx_queue
g_dbg.can.esr
```

## 后续扩展原则

新增观察入口时优先按模块导出 runtime 指针：

```c
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
ModuleRuntime * const g_dbg_module_runtime = &s_module_runtime;
#endif
```

不建议把以下变量为了 Watch 改成全局：

- 文件内只服务单模块状态机的 `static` 变量。
- 协议、Flash、SOC 等已经有明确所有权的结构体字段。
- 中断或 DMA 相关变量，除非本身确实需要 `volatile`。

已经是全局变量的核心状态可以直接加入 Watch，例如 `g_stCellInfoReport`、`SystemStatus`、`System_ErrFlag`、`OtherElement` 和 `PRT_E2ROMParas`。

# 老化时间升级重置配置说明

## 配置项

当前分支和 `fd_T3Max_D009` 都保留同一个升级策略开关：

```c
#define PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME 0
```

- 默认 `0`：升级后保留现场老化累计时间。
- 改成 `1`：升级后首次启动调用 `FactoryAging_ResetTimeByHost()`，清零老化累计时间。
- 启用后必须递增 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION`，否则已经执行过同版本策略的板子不会再次执行。

## 校验

`Project_BuildGuard.h` 会检查：

- `PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME` 只能为 `0` 或 `1`。
- 启用重置老化时间时，`PROJECT_CFG_FACTORY_AGING_ENABLE` 必须同步开启。

## 通讯可靠性

CAN 上位机通过 comm tool 读写 BMS 寄存器时，板端复用 `Sci_HostReadWords()` / `Sci_HostWriteWords()`。内部 CAN 读写不再因为本机串口短暂忙而返回 `RS485_ERROR_CMD_INVALID`，避免实时监控偶发映射成 `BMS_ERROR`。

地址、参数范围和写权限仍由原寄存器表校验。

# IAP 升级重置老化时间后 MCC 无输出问题分析

日期：2026-06-17

## 背景

本次问题发生在通过修改 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` 触发 IAP 升级参数策略，并借助升级策略重置工厂老化时间之后。

当前相关配置位于：

- `103 + 309/Project/Source/conf/Project_Config.h`
- `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION`
- `PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME`

问题现象是：升级完成后的首次启动中，观察到 AFE MOS 似乎已经打开，但 MCC 没有输出。必须让 BMS 休眠并重新唤醒后，MCC 输出才恢复正常。

## 现象描述

升级后首次启动：

1. 老化时间被升级策略重置。
2. 老化状态进入运行状态。
3. AFE 侧 `CHGMOS` / `DSGMOS` 看起来已经按老化或启动条件打开。
4. `MCC_C` 没有保持输出，导致 MCC 无输出。

休眠重启后：

1. 升级策略不再重复执行。
2. 老化状态从存储区重新加载。
3. 主循环重新应用老化运行状态对应的 MOS/MCC。
4. MCC 输出恢复正常。

## 相关代码路径

系统启动顺序在 `Runtime_Boot()`：

```c
InitE2PROM();
InitAFE1();
```

也就是说，升级参数策略先于 AFE 初始化执行。

`InitE2PROM()` 内部会调用：

```c
UpgradeParamPolicy_ApplyOnce();
```

当 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` 与 Flash 中已记录版本不一致，且启用老化时间重置策略时，`UpgradeParamPolicy_ApplyOnce()` 会调用：

```c
FactoryAging_ResetTimeByHost();
```

`FactoryAging_ResetTimeByHost()` 会把老化时间清零，并通过 `FactoryAging_EnterRunningFromHost()` 进入运行态：

```c
s_factory_aging.state = FACTORY_AGING_STATE_RUNNING;
FactoryAging_ApplyRunningMos();
```

`FactoryAging_ApplyRunningMos()` 最终会进入 `MosStartup_EnterFactoryMode(true)`，在非 5V 充电输入场景下写入：

```c
CHGMOS = 1;
DSGMOS = 1;
MCC_C  = Bit_SET;
```

随后 `InitAFE1()` 继续执行 AFE 初始化。当前流程中，`InitAFE1()` 会先调用：

```c
MosStartup_ApplyInitialState();
```

然后如果需要启动 AFE 电流零点校准，会调用：

```c
AfeCurrent_StartupZeroCal();
```

`AfeCurrent_StartupZeroCal()` 内部会再次调用：

```c
close_ctlc();
```

而 `close_ctlc()` 当前实现不只关闭 AFE CTLC，还会把 `MCC_C` 拉低：

```c
void close_ctlc(void)
{
    MCUO_AFE_CTLC = 0;
    GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_RESET);
}
```

零点校准结束时只调用：

```c
open_ctlc();
```

`open_ctlc()` 只恢复 `MCUO_AFE_CTLC`，不会恢复 `MCC_C`：

```c
void open_ctlc(void)
{
    MCUO_AFE_CTLC = 1;
}
```

## 根因

根因是升级首次启动时序造成 `MCC_C` 最终状态被启动零点校准覆盖。

具体链路：

1. `InitE2PROM()` 执行升级策略。
2. 升级策略调用 `FactoryAging_ResetTimeByHost()`。
3. 老化运行态被提前置为 `FACTORY_AGING_STATE_RUNNING`。
4. 老化路径提前应用 MOS/MCC，`MCC_C` 被拉高。
5. 随后 `InitAFE1()` 执行启动零点校准。
6. `AfeCurrent_StartupZeroCal()` 内部调用 `close_ctlc()`。
7. `close_ctlc()` 把 `MCC_C` 清零。
8. 零点校准结束只 `open_ctlc()`，没有重新恢复 `MCC_C`。
9. 主循环执行 `FactoryAging_Task()` 时，发现老化状态已经是 RUNNING，不会再走 UNINIT 启动路径，也就不会重新应用老化 MOS/MCC。

所以升级后首次启动会留下一个不一致状态：

- AFE MOS 配置可能已经打开。
- `MCC_C` 被启动零点校准最后清零。
- MCC 实际无输出。

## 为什么休眠重启后正常

休眠重启后，`UpgradeParamPolicy_ApplyOnce()` 已经写入过升级策略版本标志，不会再次调用 `FactoryAging_ResetTimeByHost()`。

此时 `s_factory_aging.state` 从 RAM 初始值 `FACTORY_AGING_STATE_UNINIT` 开始。主循环第一次执行 `FactoryAging_Task()` 时，会进入 `FactoryAging_Start()`，重新根据存储状态判断老化应运行，并调用：

```c
FactoryAging_ApplyRunningMos();
```

这次调用发生在 AFE 初始化和启动零点校准之后，因此 `MCC_C` 会被重新拉高，MCC 输出恢复正常。

## 推荐修改方案

推荐在 `InitAFE1()` 的启动零点校准完成后，重新应用一次当前启动条件对应的 MOS/MCC 初始状态。

当前逻辑：

```c
MosStartup_ApplyInitialState();
if (do_startup_zero != 0U)
{
    AfeCurrent_StartupZeroCal();
}
else
{
    open_ctlc();
}
```

建议改为：

```c
MosStartup_ApplyInitialState();
if (do_startup_zero != 0U)
{
    AfeCurrent_StartupZeroCal();
    MosStartup_ApplyInitialState();
}
else
{
    open_ctlc();
}
```

该方案的含义是：启动零点校准可以临时关闭 CTLC/MCC，但校准结束后，最终输出状态必须回到当前启动条件决定的 MOS/MCC 状态。

## 推荐方案理由

该方案改动范围最小：

1. 不修改升级策略。
2. 不修改老化状态机。
3. 不修改 AFE 参数。
4. 不修改保护阈值。
5. 不修改 CAN、Modbus、上位机协议字段。
6. 不改变启动零点校准的采样策略，只修正校准后的最终输出状态。

同时，该方案与现有设计意图一致：

- `MosStartup_ApplyInitialState()` 已经是启动阶段统一决定 MOS/MCC 初始状态的入口。
- 启动零点校准期间临时关闭输出是合理的。
- 启动零点校准完成后重新应用启动状态，可以避免 MCC 状态被中间过程遗留。

## 不推荐方案

不建议直接修改 `open_ctlc()`，让它自动恢复 `MCC_C`。

原因是 `open_ctlc()` 不只在启动零点校准结束时调用，也被故障恢复、温度恢复、过压恢复等运行期路径调用。如果在 `open_ctlc()` 中无条件恢复 `MCC_C`，可能扩大影响面，导致某些保护或故障恢复场景下提前打开 MCC。

也不建议在 `FactoryAging_Task()` 的 RUNNING 分支每 200ms 或每轮循环持续调用 `FactoryAging_ApplyRunningMos()`。

原因是这会让老化任务持续覆盖其他模块对 MOS/MCC 的临时控制，尤其可能干扰故障、保护、5V 充电输入切换或低功耗准备路径。

## 验证建议

建议按以下顺序验证：

1. IAP 升级触发 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` 变化。
2. 启用老化时间重置策略。
3. 升级完成后首次启动，不进入休眠，观察 `MCC_C` 是否有输出。
4. 观察老化状态应为 RUNNING。
5. 观察 AFE `CHGMOS` / `DSGMOS` 与 `MCC_C` 状态是否一致。
6. 观察 AFE 电流启动零点校准完成后，`MCC_C` 是否仍保持正确状态。
7. 执行一次 BMS 休眠重启，确认行为与当前正常现象一致。
8. 插入 5V 充电输入，确认仍优先走 `MosStartup_OpenChargeCloseDischarge()`，不会被老化模式错误覆盖。

建议重点观察变量或调试项：

- `s_factory_aging.state`
- `g_dbg.mos.mtp_chgmos`
- `g_dbg.mos.mtp_dsgmos`
- `g_dbg.mos.mcc_c_gpio`
- `g_dbg.afe.current_zero_state`
- `g_dbg.afe.current_zero_ready`

## 结论

本问题不是单纯的 AFE MOS 未打开，而是升级首次启动的执行顺序导致 `MCC_C` 在老化 MOS/MCC 已应用后，又被启动零点校准路径清零。

推荐修复点放在 `InitAFE1()`：启动零点校准完成后重新调用一次 `MosStartup_ApplyInitialState()`。这样可以保证启动最终态由统一启动 MOS/MCC 策略决定，同时避免修改通用 `open_ctlc()` 或老化任务运行分支带来的额外风险。

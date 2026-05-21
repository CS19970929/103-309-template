# AFE 短路/放电过流负载移除 5min 解除策略

日期：2026-05-21

## 需求

短路保护 `SC`、放电过流 `OCD1/OCD2` 触发后，不能在检测到负载移除后立即清除 AFE 硬件保护标志，必须满足：

1. AFE 负载检测确认负载已经移除。
2. 负载移除状态连续保持 5 分钟。
3. 5 分钟计时完成后，才允许清除 `SC/OCD1/OCD2` 硬件保护标志并关闭负载检测。

## 实现位置

主逻辑在 `103 + 309/Project/Source/DataDeal.c`：

| 函数/宏 | 作用 |
| --- | --- |
| `AFE_LOAD_REMOVE_DELAY_MS` | 负载移除后等待时间，固定 5min |
| `AFE_LOAD_REMOVE_DELAY_CNT` | 按 `AFE_UPDATE_PERIOD_MS=200ms` 折算为 1500 个周期 |
| `AFE_GetLoadRemoveProtectFlags()` | 从 AFE `FLAG1` 提取 `OCD1/OCD2/SC` |
| `AFE_LoadRemoveSetCrld()` | 开启/关闭 AFE 负载移除检测 |
| `AFE_IsLoadRemoved()` | 判断 `BSTATUS2.LOADOFF=1 && LOADON=0` |
| `func_LoadRemove()` | 统一处理 `OCD1/OCD2/SC` 的负载移除延时解除 |

## 状态机行为

`func_LoadRemove()` 只处理 `AFE_FLAG_OCD1 | AFE_FLAG_OCD2 | AFE_FLAG_SC`。

1. 无 `OCD1/OCD2/SC` 标志时，关闭 CRLD，清空状态和计时。
2. 新保护类型进入时，重新开启 CRLD 并清零计时。
3. 每个 200ms 周期刷新 `BSTATUS2`。
4. 若 `LOADOFF=1 && LOADON=0`，计时加 1。
5. 若负载重新接入或状态读取失败，计时清零。
6. 连续达到 1500 次后，清除对应的 `OCD1/OCD2/SC` 标志。
7. 清除成功后关闭 CRLD 并退出状态机。

## 注意点

- 这次只改变 AFE 硬件 `SC/OCD1/OCD2` 的清除条件，不改变充电过流 `OCC` 的解除策略。
- 软件层放电过流故障是否也延迟 5min，由 `Fault.h` 中 `CurOverFaultDelay` 控制；该宏当前工作区已有改动，需要在最终提交范围里确认是否一起纳入。
- 若负载移除期间通信异常，计时会被清零，避免在状态不可确认时误清硬件保护。

# AFE 负载移除恢复修复记录

生成日期：2026-05-19

## 背景

`is_AFE_ODC` 后，旧代码虽然在前面统一调用过 `func_LoadRemove()`，但 ODC 分支本身是空处理，且 `func_LoadRemove()` 只依赖上一次全量 AFE 采样带回来的 `BSTATUS2.LOADOFF`。如果负载移除状态没有被及时刷新，OCD1/OCD2 保护标志就可能一直不清，表现为负载已经移除但放电过流保护不能恢复。

本次修复把放电过流 1、放电过流 2、短路保护统一收敛到 `func_LoadRemove()` 中处理，`is_AFE_ODC` 和 `IS_AFE_SC` 分支显式调用该函数。

## 官方 Demo 对应关系

参考目录：

- `SH3673520+STM32F072CBT6 DemoCode V1.2_20241227/BMS_Drivers/Src/AFE.c`
- `SH3673520+STM32F072CBT6 DemoCode V1.2_20241227/BMS_Drivers/Src/ChargerLoad.c`
- `SH3673520+STM32F072CBT6 DemoCode V1.2_20241227/BMS_Drivers/Src/Protect.c`

关键逻辑：

- 负载检测使能时配置 `SCONF7.RLD=0`，选择 60uA 负载检测上拉电流。
- 写 `SCONF3.CRLD_EN=2` 开启 Normal/IDLE 模式下的负载状态检测。
- 通过 `BSTATUS2.LOADOFF=1` 且 `BSTATUS2.LOADON=0` 判断负载未连接。
- 负载移除确认后分别清除 `OCD1`、`OCD2`、`SC` 硬件保护标志。

## 当前实现

涉及文件：

- `103 + 309/Project/Source/DataDeal.c`

恢复流程：

1. `AFE_GetLoadRemoveProtectFlags()` 从 `FLAG1` 中收集 `OCD1/OCD2/SC`。
2. `is_AFE_ODC` 或 `IS_AFE_SC` 分支显式调用 `func_LoadRemove(load_remove_flags)`。
3. `func_LoadRemove()` 进入恢复状态机后先写 `SCONF7.RLD=0`，再写 `SCONF3.CRLD_EN=2`。
4. 每个恢复周期主动读取 `AFE_BSTATUS2`，不再只依赖全量采样缓存。
5. 连续 2 次确认 `LOADOFF=1 && LOADON=0` 后，逐个清除 `OCD1/OCD2/SC`。
6. 清标志成功后关闭 `CRLD_EN`，避免负载检测长期保持带来额外功耗。
7. 没有 `OCD1/OCD2/SC` 标志时调用 `func_LoadRemove(0)`，确保异常中断后的检测通道会被关闭。

## 验证

Keil `Target 1` 编译通过：

- `0 Error(s), 0 Warning(s)`
- `Program Size: Code=53372 RO-data=2372 RW-data=1248 ZI-data=6040`


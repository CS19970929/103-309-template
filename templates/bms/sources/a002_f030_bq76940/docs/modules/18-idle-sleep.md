# IdleSleep 废止说明

## 源码事实

当前 A002 模板源码没有 `IdleSleep.c`、`IdleSleep.h`、`IdleSleep_Init()` 或 `App_IdleSleep()`。旧文档中“当前默认启用的轻量级空闲睡眠逻辑”描述不符合源码，已废止。

## 当前低功耗入口

当前唯一主入口是 [SleepDeal.c](../../Code/Source/SleepDeal.c) 中的 `App_SleepDeal()`，在 `main.c` 中由 `PROJECT_FEATURE_LOW_POWER` 控制：

```c
#if PROJECT_FEATURE_LOW_POWER
    App_SleepDeal();
#endif
```

RTC 初始化由 `PROJECT_FEATURE_RTC` 控制，实际唤醒源和恢复路径需要同时查看 [RTC.c](../../Code/Source/RTC.c)、[SleepDeal.c](../../Code/Source/SleepDeal.c) 和中断文件。

## 后续移植规则

1. 不要按旧文档恢复 `IdleSleep` 第二套状态机。
2. 如果新项目只需要 `WFI` 级轻睡眠，应先定义统一的低功耗接口，再替换 `SleepDeal` 内部实现。
3. 如果新项目需要 Stop/Standby 深低功耗，应明确保存策略、RTC 周期、唤醒源、外设重初始化和看门狗策略后再实现。
4. 所有低功耗入口必须受 `PROJECT_FEATURE_LOW_POWER` 控制，不能散落在主循环外部。

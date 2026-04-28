# OtherElement 升级字段覆盖说明

## 目的

已有设备升级固件后，内部 Flash 中保存的 RW 参数会优先覆盖 `OtherElement_default`。因此只修改 `DataDeal.h` 中的默认值，不能保证存量设备启动后变成新值。

本次新增 `PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE` 开关，用于在升级策略执行时，只把 `OtherElement.u16Balance_OpenVoltage` 覆盖为当前固件 `OtherElement_default` 的第一个值，并保存回 RW 参数 Flash。

## 使用方式

1. 在 `103 + 309/Project/Source/DataDeal.h` 修改 `OtherElement_default` 的第一个值，例如三元分支中的 `4160`。
2. 在 `103 + 309/Project/Source/conf/Project_Config.h` 保持：

```c
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE 1
#define PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE 1
#define PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY 0
```

3. 每次需要让存量设备再次执行覆盖时，递增 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION`。

## 执行效果

`InitE2PROM()` 会先加载现场保存的 RW 参数，然后执行 `UpgradeParamPolicy_ApplyOnce()`。当版本号未执行过且开关打开时，固件会：

1. 读取当前固件中的 `OtherElement_default`。
2. 将 `OtherElement.u16Balance_OpenVoltage` 设置为默认表第一个值。
3. 调用 `EEPROM_SaveRWParametersToFlash()` 保存整组 RW 参数。
4. 写入升级策略版本号，避免后续每次启动重复覆盖。

该策略只覆盖均衡开启电压，不会重置整块 `OtherElement`。

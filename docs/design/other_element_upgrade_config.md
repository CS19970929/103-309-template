# OtherElement 升级配置说明

状态：CURRENT
最后更新：2026-07-30
主要源码：`103 + 309/Project/Source/DataDeal.h`、`103 + 309/Project/Source/conf/Project_Config.h`、`103 + 309/Project/Source/UpgradeParamPolicy.h`、`103 + 309/Project/Source/EEPROM.c`

## 1. 配置方式

现场升级包需要覆盖已保存的 `OtherElement` 参数时：

1. 在 `DataDeal.h` 中按电池类型修改 `OtherElement_default`。
2. 在 Keil Configuration Wizard 中把 `PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT` 置为 `1`。
3. 提高 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION`。

`Project_Config.h` 不再维护 32 个 `PROJECT_CFG_UPGRADE_OTHER_*` 独立参数，初始化默认值和升级覆盖值统一使用 `OtherElement_default`，避免两套默认值不一致。

## 2. 执行规则

`UpgradeParamPolicy_ApplyOnce()` 使用 `FLASH_ADDR_UPGRADE_PARAM_FLAG` 做一次性执行标记。设备启动时先从 Flash 加载 RW 参数，再执行升级策略；策略版本已执行过时直接跳过。

启用 `PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT` 后，固件调用 `EEPROM_LoadDefaultOtherElement()`，把当前电池类型对应的 `OtherElement_default` 32 个字段整体写入运行参数，并通过 `EEPROM_SaveRWParametersToFlash()` 保存到 RW 参数区。

保存前继续使用 `OtherElement_min/max` 做完整范围校验。保存失败会置 `ERROR_EEPROM_STORE`，且不会写入升级策略完成 flag。

## 3. 安全边界

- 仅改变升级策略的参数来源，不改变 `OtherElement` 结构体布局、Flash RW 参数布局、RS485 `0x2300..0x231F` 映射、帧格式或 CAN/上位机协议。
- 若同时启用 `Reset balance open voltage` 或 `Reset SOC config`，完整 `OtherElement_default` 覆盖最后执行，最终以 `OtherElement_default` 为准。
- SOC 初始化在 `InitE2PROM()` 之后执行，因此升级覆盖 SOC 参数后，会在同次启动被 `InitData_SOC()` 读取。

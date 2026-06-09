# OtherElement 升级配置说明

状态：CURRENT
最后更新：2026-06-09
主要源码：`103 + 309/Project/Source/conf/Project_Config.h`, `103 + 309/Project/Source/UpgradeParamPolicy.h`, `103 + 309/Project/Source/EEPROM.c`

## 1. 用途

现场升级包需要覆盖 `OtherElement` 参数时，不再要求修改 `DataDeal.h` 默认表。发布人员可以在 Keil Configuration Wizard 中打开 `Project_Config.h`，配置 `Upgrade Parameter Policy` 下的 `Update all OtherElement words from Project_Config on upgrade` 和 32 个 `Upgrade OtherElement Values`。

默认 `PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT = 0`，因此当前升级包不会自动覆盖现场保存的 `OtherElement`。需要覆盖时必须同时：

1. 把 `PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT` 置为 `1`。
2. 按目标产品填写 32 个 `PROJECT_CFG_UPGRADE_OTHER_*` 值。
3. 提高 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION`。

## 2. 执行规则

`UpgradeParamPolicy_ApplyOnce()` 仍使用 `FLASH_ADDR_UPGRADE_PARAM_FLAG` 做一次性执行标记。设备启动时先从 Flash 加载 RW 参数，再执行升级策略；当策略版本已执行过时直接跳过。

启用 `PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT` 后，固件会把 `Project_Config.h` 中的 32 个配置值按协议顺序写入 `OtherElement`，再通过 `EEPROM_SaveRWParametersToFlash()` 保存到 RW 参数区。保存前继续复用现有 `OtherElement_min/max` 范围校验，保存失败会置 `ERROR_EEPROM_STORE` 且不会写入升级策略完成 flag。

## 3. 字段映射

配置字段按 RS485 `0x2300..0x231F` 连续映射，不改变协议地址、帧格式或上位机读写语义。

| 配置前缀 | 覆盖范围 |
| --- | --- |
| `PROJECT_CFG_UPGRADE_OTHER_BALANCE_*` | `u16Balance_*`, offset `0..7` |
| `PROJECT_CFG_UPGRADE_OTHER_CS_*`, `PROJECT_CFG_UPGRADE_OTHER_CBC_*` | 电流采样和 CBC 参数，offset `8..11` |
| `PROJECT_CFG_UPGRADE_OTHER_SOC_TABLE_SELECT`, `PASSWORD`, `CUR_LIMIT` | 表选择和限流保留参数，offset `12..15` |
| `PROJECT_CFG_UPGRADE_OTHER_SLEEP_*` | 休眠参数，offset `16..23` |
| `PROJECT_CFG_UPGRADE_OTHER_SOC_*` | 容量、循环次数、满/空电压，offset `24..27` |
| `PROJECT_CFG_UPGRADE_OTHER_SYS_*` | 串数、采样电阻和预充时间，offset `28..31` |

## 4. 安全边界

- 新能力只改变升级策略覆盖来源，不修改 `OtherElement` 结构体布局、Flash RW 参数布局、RS485 地址或 CAN/上位机协议。
- `Project_BuildGuard.h` 在启用覆盖时检查 32 个配置值范围，避免非法升级包进入构建。
- 若同时启用旧的 `Reset balance open voltage` 或 `Reset SOC config`，`Project_Config.h` 的完整 `OtherElement` 覆盖最后执行，最终以 `PROJECT_CFG_UPGRADE_OTHER_*` 值为准。
- SOC 初始化在 `InitE2PROM()` 之后执行，因此升级覆盖 SOC 参数后会在同次启动被 `InitData_SOC()` 读取。

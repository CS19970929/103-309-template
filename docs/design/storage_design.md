# Flash / EEPROM / 参数存储设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`EEPROM.c`, `Flash.c`, `Flash.h`, `UpgradeParamPolicy.h`, `SocEnhance.c`, `FactoryAging.c`
最后更新时间：2026-05-26
未确认事项：实际 MCU Flash 容量、后 64K 量产可靠性、Host 写权限策略。

## 1. 当前实现结论

当前工程保留 `EEPROM` 命名作为兼容层，但主要参数已经迁移到内部 Flash。外部 EEPROM 旧读写函数基本为空实现，不应再把旧 EEPROM 文档当作当前行为。

## 2. Flash 布局

| 数据 | 地址 | API |
|---|---|---|
| AFE 参数 A/B | `0x0801C000`, `0x0801C800` | `StorageFlash_LoadAfeData/SaveAfeData` |
| RW 参数 A/B | `0x0801C400`, `0x0801CC00` | `StorageFlash_LoadRwParamData/SaveRwParamData` |
| 日志 A/B | `0x0801D000`, `0x0801D800` | `StorageFlash_LoadLogData/SaveLogData` |
| SOC snapshot A/B | `0x0801E000`, `0x0801E800` | `StorageFlash_LoadSocData/SaveSocData` |
| 升级参数 flag | `0x0801F000` | `UpgradeParamPolicy_ApplyOnce()` |
| 老化状态 | `0x0801F400` | `StorageFlash_LoadFactoryAgingData/SaveFactoryAgingData` |
| Sleep/update legacy flag | `0x0801F800`, `0x0801FC00` | 保留 |

## 3. 掉电保护

当前存储实现包含：

- header magic/version/length/sequence/crc。
- 双槽或 journal page。
- 擦写和编程后校验。
- 写失败触发 `ERROR_EEPROM_STORE`。

仍需实测：

- 写入中断电恢复。
- Flash 小于 128KB 时所有保存路径的保护。
- 写频率和寿命。

## 4. 参数初始化

启动链：

```text
InitE2PROM()
  EEPROM_LoadDefaultRuntimeData()
  EEPROM_LoadRWParametersFromFlash()
  ReadEEPROM_AFE_Parameters()
  ReadEEPROM_EventRecord_Parameters()
  UpgradeParamPolicy_ApplyOnce()
```

保护参数和 OtherElement 会先加载默认，再尝试用 Flash 参数覆盖。写入时做范围检查和失败回滚。

## 5. 升级参数策略

当前配置：

- `PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE 1`
- `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION 0x0004`
- 当前 reset SOC snapshot 和 event record。
- 当前不 reset AFE/protect/balance/SOC config/factory aging time。

## 6. 风险和建议

| 风险 | 建议 |
|---|---|
| Flash 地址依赖后 64K | 先确认真实 MCU 容量，再把检查写入脚本 |
| Host 写权限直接写 Flash/AFE 参数 | 后续增加工装权限或写保护策略 |
| EEPROM 命名误导 | 文档统一称为“EEPROM 兼容层 / Flash 参数存储” |
| IAP 与 App 地址混淆 | 所有烧录只走 `tools/soc_flash_app_safe.ps1` |

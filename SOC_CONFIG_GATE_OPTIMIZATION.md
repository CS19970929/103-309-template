# SOC 配置门控接入与调试记录

## 背景

当前工程已经在 `conf/Project_Config.h` 中提供了 SOC 校准相关配置项，包括满电确认裕量、最大单体压差、有效电压范围，以及保护/系统故障是否阻断电压类校准。但 `SocEnhance.c` 仍有部分逻辑使用固定阈值，导致配置界面修改后不会完整影响 SOC 算法。

本次优化目标是让 SOC 模块的电压类校准门控与项目配置保持一致，同时不改变安时积分、低压尾段表、Flash V2 快照和通信地址。

## 代码调整

1. `SocEnhance.c` 的有效单体电压范围改为读取：
   - `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV`
   - `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV`
   - `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV`

2. 保护/系统故障阻断变为可配置：
   - `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT = 1` 时，三级保护故障会阻断 OCV、满电和低压电压类校准。
   - `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT = 1` 时，AFE/ADC/CBC/温度系统故障会阻断电压类校准。
   - 当前默认值为 `0`，即不因这些故障标志额外阻断校准，只保留基础电压合法性检查。

3. 满电确认阈值从固定 `4180/4150/4100mV` 改为跟随 `V100`：
   - 普通确认门槛：`V100 - PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV`
   - 快速确认门槛：`V100 - 30mV`
   - 最大单体压差：`PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV`

这样三元锂默认 `V100=4180mV` 时仍对应 `4100mV` 普通确认和 `4150mV` 快速确认；若切换到磷酸铁锂并配置 `V100=3650mV`，满电确认会自动随参数下移，不再被固定 4.18V 阈值卡住。

## 回放测试

`tools/soc_replay_test.py` 已同步更新：

- 测试满电确认跟随配置的 `V100`。
- 测试最大压差参数会阻断满电确认。
- 测试保护故障阻断行为跟随配置开关。

验证命令：

```powershell
py tools\soc_replay_test.py
```

当前结果：17 项 SOC 回放测试全部通过。

## 未改变内容

- SOC 内部状态、容量单位和安时积分周期未变。
- 低压尾段表策略未变，仍按 `V0` 相对电压和放电电流档位每次最多下修 `1%`。
- Flash SOC 快照格式未变，仍兼容 V1/V2。
- RS485/CAN 对外字段未变，`g_stCellInfoReport.SocElement` 发布路径未变。

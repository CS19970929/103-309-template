# EEPROM 写标志清理说明

## 结论

`u32E2P_Pro_VolCur_WriteFlag` 这一类旧 EEPROM 写标志已经没有实际作用。

清理前的源码里，这些变量只有三类行为：

1. 在 `EEPROM.c` 中定义和周期性清零。
2. 在 `Sci_Upper.c` 的通信写参数路径里赋值。
3. 在 `DataDeal.c` 的旧 AFE 采样保护判断里作为注释残留。

没有任何有效代码读取这些标志并据此完成 EEPROM 或 Flash 落盘。`App_E2promDeal()` 也只是清零旧标志，不执行真实写入。因此保留这些变量只会造成误导。

## 删除范围

本次清理删除了以下旧标志及配套代码：

- `u32E2P_Pro_VolCur_WriteFlag`
- `u32E2P_Pro_Temp_WriteFlag`
- `u32E2P_Pro_Other_WriteFlag`
- `u32E2P_RTC_Element_WriteFlag`
- `u32E2P_OtherElement1_WriteFlag`
- `u32E2P_HeatCool_WriteFlag`
- `u8E2P_SocTable_WriteFlag`
- `u8E2P_CopperLoss_WriteFlag`
- `u8E2P_KB_WriteFlag`
- `u8E2P_KB_WritePos`
- `ProductionInfor.BMS_*_WriteFlag`
- 对应的 `extern` 声明、清零函数、赋值语句和旧 bit mask 宏。

保留了仍有实际作用的逻辑，例如：

- 通信写参数后更新 RAM 参数。
- 电压电流保护参数、SOC 表、系统参数变化后调用 `InitData_SOC()`。
- 会影响 AFE 配置的参数变化后置位 `AFE_PARAM_WRITE_Flag`。

## 后续注意

这次清理不新增参数持久化能力，只移除已经失效的旧 EEPROM 写标志。当前需要持久化的模块应继续走已有 Flash 接口，例如 AFE 参数、日志、SOC 数据等。

如果后续需要把通信写入的保护参数、校准参数、SOC 表等也持久化到内部 Flash，建议重新设计块级 dirty 机制，不要恢复这些分散的旧写标志。

# 可读写运行参数持久化说明

本文说明 `PRT_E2ROMParas`、`OtherElement`、`Heat_Cool_Element` 三组运行参数的通信读写和内部 Flash 持久化策略。

## 参数范围

| 参数块 | 结构体 | 读写地址范围 | 字数 |
|---|---|---:|---:|
| 保护参数 | `PRT_E2ROMParas` | `0x2100` ~ `0x2140` | 65 |
| 其他参数 | `OtherElement` | `0x2300` ~ `0x231F` | 32 |
| 加热/冷却参数 | `Heat_Cool_Element` | `0x2320` ~ `0x2337` | 24 |

读取仍走原有 `0x03` 读保持寄存器流程。写入走 `0x10` 多寄存器写流程，现在支持在上述三个参数块内任意起点的连续写入，但不允许一次写入跨越参数块边界。

## Flash 存储

三组参数合并为一个 `STORAGE_FLASH_RW_PARAM_DATA` 数据块，使用内部 Flash 双槽保存：

| 槽 | 地址 |
|---|---:|
| `FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A` | `0x0801C400` |
| `FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B` | `0x0801CC00` |

当前 Keil 目标为 `STM32F103C8`，按 `STM32F10X_MD` 的 1KB Flash 页布局使用这些地址。

数据块包含：

- `protect[65]`
- `other[32]`
- `heat_cool[24]`

保存接口为 `StorageFlash_SaveRwParamData()`，加载接口为 `StorageFlash_LoadRwParamData()`。写入格式复用现有 `StorageFlash_SavePair()` 双槽记录格式，通过 magic、长度、序号和 CRC 校验选择有效槽。

## 启动加载

`InitE2PROM()` 的启动顺序为：

1. 先加载编译期默认值到 RAM。
2. 从 RW 参数 Flash 双槽读取三组参数。
3. 对读取结果按各结构体已有 min/max 表做范围校验。
4. 校验成功则覆盖 RAM 默认值。
5. 校验失败或首次无数据时保留默认值，并尝试把默认值写入 RW 参数 Flash 双槽。
6. 继续加载 AFE 参数、事件记录并执行升级参数策略。

## 写入策略

`0x10` 写入流程：

1. 按起始地址分派到保护参数、其他参数或加热/冷却参数块。
2. 校验寄存器数量、字节数和块内边界。
3. 按对应 min/max 表校验每个写入值。
4. 先备份 RAM 中的旧参数，再写入 RAM。
5. 调用 `EEPROM_SaveRWParametersToFlash()` 立即落盘。
6. Flash 保存失败时恢复 RAM 备份，并返回负响应。
7. 保存成功后执行必要的运行时副作用，例如 `InitData_SOC()`、`AFE_PARAM_WRITE_Flag`、`SeriesNum` 和 `g_u32CS_Res_AFE` 更新。

`0x06` 的三类恢复默认命令也同步落盘：

- `RS485_CMD_ADDR_RESET_PROTECT_ELEMENT`
- `RS485_CMD_ADDR_RESET_OTHER_CANADD`
- `RS485_CMD_ADDR_RESET_HEAT_COOL`

恢复默认时同样采用先备份、写默认值、保存 Flash、失败回滚的流程。

## 维护注意

- 三个参数块的 Flash 字数宏与 EEPROM 参数数量宏有编译期一致性检查。
- 后续新增字段时，需要同步更新结构体、默认值、min/max、`E2P_PARA_NUM_*` 和 `FLASH_STORAGE_RW_PARAM_*_WORD_COUNT`。
- `0x10` 写入不支持跨块写入，跨块请求会返回 `RS485_ERROR_CMD_INVALID`。
- `OtherElement.u16Sys_CS_Res` 为 0 时不会更新 `g_u32CS_Res_AFE`，避免除零。

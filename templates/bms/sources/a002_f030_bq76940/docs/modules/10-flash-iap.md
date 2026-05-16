# Flash、IAP 与启动标志

## 相关文件

- [Flash.c](../../Code/Source/Flash.c)
- [Flash.h](../../Code/Include/Flash.h)
- [main.h](../../Code/Include/main.h)
- [Project_Template_Config.h](../../Code/Include/Project_Template_Config.h)

## 模块职责

Flash 模块负责内部 Flash 标志读写、IAP 应用向量重映射、升级检测和低功耗唤醒原因保存。

当前模板将 `Storage_*` facade 和旧 `EEPROM` 兼容 API 都映射到内部 Flash，因此 Flash 模块同时承担参数、SOC、日志、生产信息等持久化后端。

## Flash 地址规划

| 地址 | 宏 | 用途 |
| --- | --- | --- |
| `0x08000000` | `FLASH_ADDR_IAP_START` | IAP 起始地址。 |
| `0x08001C00` | `APPLICATION_ADDRESS` | APP 起始地址。 |
| `0x08001C00..0x0800DFFF` | `PROJECT_CFG_FLASH_APP_SIZE` | App 链接区，scatter 长度 `0x0000C400`。 |
| `0x0800E000..0x0800FFFF` | `PROJECT_CFG_FLASH_STORAGE_START/SIZE` | 内部 Flash 存储区。 |
| `0x0800FFC0` | `EEPROM_ADDR_PASS` 映射后地址 | 参数初始化完成标志。 |
| `0x0800FFF0` | `FLASH_ADDR_WAKE_TYPE` | 唤醒原因。 |
| `0x0800FFF2` | `FLASH_ADDR_UPDATE_FLAG` | 升级标志。 |
| `0x0800FFF4` | `FLASH_ADDR_SLEEP_FLAG` | 睡眠状态标志。 |

Keil scatter 必须与上表一致：`LR_IROM1 0x08001C00 0x0000C400`，保证应用代码不会覆盖 `0x0800E000` 后的参数页。

## IAP 启动适配

`_IAP` 默认启用。`Init_IAPAPP()` 会将应用向量表从 `APPLICATION_ADDRESS` 拷贝到 SRAM `0x20000000`，然后通过 SYSCFG 将 SRAM 映射为向量表区域。

当前拷贝向量数为 48 个。若后续 MCU 型号或中断数量变化，需要同步检查该数量。

## 升级检测

`App_FlashUpdateDet()` 检查升级标志。如果检测到需要升级，会触发系统复位，让 IAP 接管后续升级流程。

## 睡眠与唤醒标志

低功耗相关模块会使用 Flash 保存：

- 是否从睡眠启动。
- 唤醒来源，例如 RTC、外部中断、通信等。
- 进入睡眠前的模式标志。

这些标志主要服务于 `SleepDeal`。当前模板没有 `IdleSleep` 源码。

## 维护建议

- Flash 标志页与应用代码共享 64KB Flash，新增页前必须确认不会覆盖应用代码或 IAP 区。
- Flash 擦写耗时较长，新增擦写路径时要考虑 IWDG，并优先走分时写入。
- `FlashWriteOneHalfWord()` 采用整页缓存、擦除、恢复的方式写半字；修改时不能退回“擦页后只写一个 halfword”的实现。
- 修改 `APPLICATION_ADDRESS` 时必须同步 IAP、Keil 工程链接地址、向量重映射和升级工具。

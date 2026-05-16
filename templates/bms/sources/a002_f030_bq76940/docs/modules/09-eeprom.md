# 内部 Flash 参数与记录存储

## 相关文件

- [EEPROM.c](../../Code/Source/EEPROM.c)
- [EEPROM.h](../../Code/Include/EEPROM.h)
- [Flash.c](../../Code/Source/Flash.c)

## 模块职责

旧项目外部 EEPROM 已完全废除，后续不再使用 24xx EEPROM、PB3/PB4 软件 I2C 或 PA15 WP。当前 `EEPROM.c/.h` 只保留历史地址表和兼容 API 名称；新模板入口使用 `Storage_Init()`、`Storage_Task()`、`Storage_ReadWord()` 和 `Storage_WriteWord()`，实际读写固定映射到 MCU 内部 Flash。

默认配置由 [Project_Template_Config.h](../../Code/Include/Project_Template_Config.h) 控制：

| 配置 | 当前值 | 说明 |
| --- | --- | --- |
| `PROJECT_CFG_STORAGE_INTERNAL_FLASH` | `1` | 参数存储固定使用内部 Flash。 |
| `PROJECT_CFG_FLASH_VEEPROM_START` | `0x0800E000` | 逻辑参数区起始地址。 |
| `PROJECT_CFG_FLASH_VEEPROM_SIZE` | `0x00002000` | 逻辑参数区可用大小。 |

## 废除的硬件资源

| 信号 | GPIO | 当前状态 |
| --- | --- | --- |
| EEPROM SCL | PB3 | 已废除，不再初始化。 |
| EEPROM SDA | PB4 | 已废除，不再初始化。 |
| EEPROM WP | PA15 | 已废除，不再初始化。 |

`bsp_i2c_eeprom_24xx.c/.h`、旧 `param.c/.h` 和 `todo.c` 草稿已从模板源删除，避免后续项目配置生成器误接入外部 EEPROM 路径。

## 首次初始化

工程使用逻辑地址 `EEPROM_ADDR_PASS` 保存 `EEPROM_VALUE_BEGIN_FLAG = 0x2234`。该逻辑地址为 `0x1FC0`，实际物理地址是 `0x0800FFC0`。如果启动时未读到该标志，则认为参数区未初始化：

1. 写入默认保护参数。
2. 写入默认校准参数。
3. 写入 SOC 表、生产信息和其他默认参数。
4. 在内部 Flash 参数区写入初始化完成标志。
5. 复位系统。

## 逻辑地址布局

| 地址范围/起始 | 内容 |
| --- | --- |
| `0` 至 `128` | 保护参数。 |
| `130` 至 `152` | RTC 相关数据。 |
| `154` | 校准 K 参数，长度 47。 |
| `248` | 校准 B 参数，长度 47。 |
| `342` | SOC OCV 表，长度 42。 |
| `426` / `458` | 铜损相关参数。 |
| `490` | 故障记录。 |
| `676` 至 `738` | 其他系统参数。 |
| `740` 至 `788` | 加热/冷却参数。 |
| `790` | 增强 SOC 参数。 |
| `830` | 序列号字符串。 |
| `870` | 硬件版本字符串。 |
| `910` | 软件版本字符串。 |
| `1000` 至 `1198` | 事件记录。 |
| `1200` | 事件记录指针。 |
| `1202` | SOC RTC 计数。 |

以上地址都是相对 `0x0800E000` 的逻辑偏移。`Storage_*()` 入口以及旧 `ReadEEPROM_Word_WithZone()` / `WriteEEPROM_Word_WithZone()` 兼容入口不再使用外部 EEPROM 的 A/B/C 冗余区，而是直接映射到内部 Flash，避免 8KB 存储区被旧冗余偏移放大后越界。

## 分时写入机制

`Storage_Task()` 根据各类写入标志逐项处理内部 Flash 写操作。旧 `App_E2promDeal()` 仅调用 `Storage_Task()`，用于兼容未改名的旧模块。这样可以避免在通信命令或保护逻辑中直接执行长时间 Flash 写入。

典型写入触发来自：

- 上位机写保护参数。
- 上位机写校准参数。
- SOC 参数更新。
- 事件日志记录。
- 生产 ID 写入。

## 维护建议

- 新增参数必须分配固定地址并更新地址布局文档。
- 不要在协议处理函数中直接长时间循环写 Flash，应设置写入标志交给 `Storage_Task()`。
- 参数结构调整要考虑旧版本内部 Flash 数据兼容，必要时增加版本号或迁移逻辑。
- 当前配置下，写入一个 halfword 会擦写并恢复所在 1KB Flash 页；新增高频写入前必须评估擦写寿命。
- 不再新增或恢复外部 EEPROM 后端。若未来某个新硬件确实需要外部 EEPROM，应作为新的存储 profile 和独立模块重新设计，不能复用已废除的旧路径。

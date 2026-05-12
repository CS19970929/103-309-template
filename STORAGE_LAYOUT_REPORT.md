# 存储布局说明

本文档梳理当前工程里和存储相关的内容，重点包括：

- 外部 EEPROM 地址布局
- 内部 Flash 地址布局
- 参数写入和读取逻辑
- 工程代码大小和链接地址
- EasyLogger 的 Flash 日志插件状态

本文内容基于当前仓库中的 `103 + 309` 工程，以及同目录下的 `C030v1.0` 构建产物。

## 1. 外部 EEPROM

### 1.1 基本信息

外部 EEPROM 由软件 I2C 模拟访问，地址定义在 [EEPROM.h](103 + 309/Project/Source/EEPROM.h)：

- 设备地址：`0xA0`
- I2C 引脚：`PB10` 作为 SCL，`PB11` 作为 SDA
- 读写接口：
  - `ReadEEPROM_Byte()`
  - `WriteEEPROM_Byte()`
  - `ReadEEPROM_Word_NoZone()`
  - `WriteEEPROM_Word_NoZone()`

`ReadEEPROM_Word_NoZone()` / `WriteEEPROM_Word_NoZone()` 都按“两个字节一个半字”的方式处理数据。

### 1.2 地址分区

#### 1.2.1 哨兵和状态位

| 地址 | 宏 | 用途 |
|---|---|---|
| `0x3FFC` | `EEPROM_ADDR_PASS` | 首次上电完成标志 |
| `0x3FFA` | `EEPROM_ADDR_SLEEP` | 休眠相关哨兵位 |
| `0x3FFE` | `EEPROM_ADDR_FLASHUPDATE` | Flash 更新相关哨兵位 |
| `2040` | `EEPROM_ADDR_SWITCH_ONOFF` | 系统功能开关位 |
| `2044` | `EEPROM_ADDR_SYS_FUNC_SELECT` | 系统功能选择位 |

#### 1.2.2 主参数区

| 起始地址 | 宏 | 数据块 | 长度 / 说明 |
|---|---|---|---|
| `0` | `E2P_ADDR_E2POS_PROTECT` | 保护参数 | 65 个半字，按结构体顺序连续映射 |
| `130` | `E2P_ADDR_E2POS_RTC` | RTC 参数 | 12 个半字 |
| `154` | `E2P_ADDR_START_CALIB_K` | `K` 校准值 | `KB_NUM` 个半字 |
| `248` | `E2P_ADDR_START_CALIB_B` | `B` 校准值 | `KB_NUM` 个半字 |
| `342` | `E2P_ADDR_START_SOC_TABLE` | SOC 表 | `SOC_TABLE_SIZE` 个半字 |
| `426` | `E2P_ADDR_START_COPPERLOSS` | 铜损补偿 | `CompensateNUM` 个半字 |
| `458` | `E2P_ADDR_START_COPPERLOSS_NUM` | 铜损补偿数量 | `CompensateNUM` 个半字 |
| `490` | `E2P_ADDR_START_FAULT_RECORD` | 故障记录起点 | 3 段故障记录 + 临时区 |
| `676` | `E2P_ADDR_START_OTHER_ELEMENT1` | 扩展参数 1 | 32 个半字 |
| `740` | `E2P_ADDR_E2POS_HEAT_COOL` | 加热/冷却参数 | 24 个半字 |
| `790` | `E2P_ADDR_E2POS_ENHANCE_SOC` | 历史 SOC 扩展预留 | 当前未作为 SOC 基础参数落点；实际 SOC 基础参数在 `OtherElement`，由内部 Flash RW 参数区保存 |
| `830` | `E2P_ADDR_E2POS_SERIAL_NUM` | 序列号 | 首字存长度，后续为内容 |
| `870` | `E2P_ADDR_E2POS_HAEDWARE_VER` | 硬件版本 | 首字存长度，后续为内容 |
| `910` | `E2P_ADDR_E2POS_SOFTWARE_VER` | 软件版本 | 首字存长度，后续为内容 |
| `1000` | `E2P_ADDR_START_EVENT_RECORD` | 事件记录区 | 100 条事件，每条 2 字节 |
| `1200` | `E2P_ADDR_E2POS_EVENT_POINT` | 事件指针 | 当前事件写入位置 |
| `1500` | `E2P_ADDR_SH367309_VALUE` | SH367309 偏移值 | 当前代码中作为“虚拟电流校准值”使用 |

### 1.3 保护参数的内部划分

保护参数按结构体字段顺序连续排列，宏定义在 [EEPROM.h](103 + 309/Project/Source/EEPROM.h) 中：

- `E2P_PARA_NUM_VOLCUR_PROTECT = 30`
- `E2P_PARA_NUM_TEM_PROTECT = 25`
- `E2P_PARA_NUM_OTHER_PROTECT = 10`
- `E2P_PARA_NUM_PROTECT = 65`

写入掩码也按组划分：

- `u32E2P_Pro_VolCur_WriteFlag`
- `u32E2P_Pro_Temp_WriteFlag`
- `u32E2P_Pro_Other_WriteFlag`
- `u32E2P_OtherElement1_WriteFlag`
- `u32E2P_HeatCool_WriteFlag`
- `u8E2P_KB_WriteFlag`

### 1.4 EEPROM 读写流程

读写逻辑主要在 [EEPROM.c](103 + 309/Project/Source/EEPROM.c)：

1. `InitE2PROM()` 初始化 I2C 引脚。
2. `InitData_E2prom()` 判断 `EEPROM_ADDR_PASS`：
   - 如果不是首次上电，进入正常读取流程。
   - 如果是首次上电，执行默认值下发、产品 ID 初始化、AFE 初始化、电流偏移校准等流程。
3. `ReadEEPROM_ByteData_StartUp()` 在正常启动时按地址表把数据读入 RAM 结构体，并做范围校验。
4. `WriteEEPROM_ByteData_Circle()` 在后台逐项写回 EEPROM，一次只写一个被置位的字段，避免连续大批量写入。

### 1.5 事件记录逻辑

事件记录在 [LogRecord.c](103 + 309/Project/Source/LogRecord.c)：

- 内存中缓存为 `BMS_LOG_RECORD[100][2]`
- 每条记录占 2 字节
- 写入地址：
  - `E2P_ADDR_START_EVENT_RECORD + (index << 1)`
  - 当前指针写到 `E2P_ADDR_E2POS_EVENT_POINT`

读取时会校验：

- 事件号不能超出 `EVENT_NUM`
- 时间间隔编码不能超过 `171`

出错后会把对应条目清成 `0`，日志区是少数允许自动修复的区域之一。

### 1.6 产品 ID 逻辑

产品 ID 在 [ProductionID.c](103 + 309/Project/Source/ProductionID.c)：

- `E2P_ADDR_E2POS_SERIAL_NUM`
- `E2P_ADDR_E2POS_HAEDWARE_VER`
- `E2P_ADDR_E2POS_SOFTWARE_VER`

每个字段都采用：

1. 头部半字保存长度
2. 后续半字保存内容

默认值由 `WriteProID_Default()` 下发。

## 2. 内部 Flash

内部 Flash 地址定义在 [Flash.h](103 + 309/Project/Source/Flash.h)：

| 地址 | 宏 | 用途 |
|---|---|---|
| `0x08000000` | `FLASH_ADDR_IAP_START` | IAP/Boot 区起点 |
| `0x08004800` | `FLASH_ADDR_APP_START` | 应用程序起点 |
| `0x0801C000` | `FLASH_ADDR_STORAGE_AFE_SLOT_A` | AFE 参数 Slot A |
| `0x0801C400` | `FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A` | 运行参数 Slot A |
| `0x0801C800` | `FLASH_ADDR_STORAGE_AFE_SLOT_B` | AFE 参数 Slot B |
| `0x0801CC00` | `FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B` | 运行参数 Slot B |
| `0x0801D000` | `FLASH_ADDR_STORAGE_LOG_SLOT_A` | 事件日志 Slot A |
| `0x0801D800` | `FLASH_ADDR_STORAGE_LOG_SLOT_B` | 事件日志 Slot B |
| `0x0801E000` | `FLASH_ADDR_STORAGE_SOC_SLOT_A` / `FLASH_ADDR_SH367309_VALUE` | SOC 数据 Slot A |
| `0x0801E800` | `FLASH_ADDR_STORAGE_SOC_SLOT_B` / `FLASH_ADDR_SH367309_FLAG` | SOC 数据 Slot B |
| `0x0801F000` | `FLASH_ADDR_UPGRADE_PARAM_FLAG` | 升级参数策略执行标志 |
| `0x0801F400` | `FLASH_ADDR_FACTORY_AGING_FLAG` | 出厂老化进度 journal / 完成标志 |
| `0x0801F800` | `FLASH_ADDR_UPDATE_FLAG` | IAP/更新标志位 |
| `0x0801FC00` | `FLASH_ADDR_SLEEP_FLAG` | 休眠模式标志位 |

### 2.1 Flash 读写逻辑

实现见 [Flash.c](103 + 309/Project/Source/Flash.c)：

- `FlashWriteOneHalfWord()`：
  - 解锁 Flash
  - 擦除整页
  - 写入一个半字
  - 上锁
- `FlashReadOneHalfWord()`：
  - 直接按半字读指定地址

### 2.2 典型用途

#### 2.2.1 休眠模式记忆

[SleepDeal.c](103 + 309/Project/Source/SleepDeal.c) 会把睡眠模式写到 `FLASH_ADDR_SLEEP_FLAG`：

- `FLASH_NORMAL_SLEEP_VALUE = 0x1234`
- `FLASH_DEEP_SLEEP_VALUE = 0x1235`
- `FLASH_HICCUP_SLEEP_VALUE = 0x1236`

启动后再从同一地址读回，决定恢复哪种休眠路径。

#### 2.2.2 IAP 跳转控制

[Flash.c](103 + 309/Project/Source/Flash.c) 里：

- `FLASH_TO_IAP_VALUE = 0x00AB`
- `FLASH_TO_APP_VALUE = 0xFFFF`

`InitAreaSelect()` 会检查 `FLASH_ADDR_UPDATE_FLAG`：

- 若等于 `0x00AB`，则跳回 IAP
- 否则继续正常应用启动

#### 2.2.3 更新和清除标志

更新标志和睡眠标志都是“半字哨兵位”。
写入逻辑的核心不是保存大数据，而是保存一个状态字。

#### 2.2.4 出厂老化进度 journal

[main.c](103 + 309/Project/Source/main.c) 会读取 `FLASH_ADDR_FACTORY_AGING_FLAG`：

- `FLASH_FACTORY_AGING_DONE_VALUE = 0xA93D`
- `FLASH_FACTORY_AGING_RESET_VALUE = 0xFFFF`
- `FLASH_FACTORY_AGING_STATE_RUNNING = 0xA931`

如果该页保持 `0xFFFF`，固件首次运行会进入出厂老化模式；运行态累计过程中会低频追加 journal record，保存累计 `elapsed10ms` 和状态。Flash checkpoint 当前约 2 小时一次，正常 3 天老化不会写满该 1KB 页。MCU reset 或主动深睡唤醒后，会优先使用 BKP 最近值，再结合 Flash journal 最新有效记录恢复进度，不从 0 重新开始。

运行态累计满 `PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS` 后写入 `DONE` record。后续重启读到 `0xA93D` 状态时不再自动进入老化，并保持充电管关闭、放电管打开。完成记录写失败时，运行态不会进入 `DONE`，后续继续重试，避免本次误判完成、下次重启又重新老化。

返工需要重新执行老化时，除了擦除 `0x0801F400` journal 页，还需要清除 BKP `DR6~DR10` 中的老化进度缓存，避免刚擦完 Flash 又被 BKP 里的旧进度恢复。

## 3. 工程大小与链接布局

### 3.1 主工程链接脚本

当前主工程的 scatter 文件在：

- [CommomSH367309_16series_103RCT6_C.sct](103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.sct)

关键配置：

- `LR_IROM1 0x08004800 0x00020000`
- `ER_IROM1 0x08004800 0x00020000`
- `RW_IRAM1 0x20000000 0x00005000`

这说明：

- 程序从 `0x08004800` 开始装载
- Flash 载入区按 128KB 规划
- RAM 按 20KB 规划

### 3.2 当前构建尺寸

主工程 build log 显示：

- `Code = 50104`
- `RO-data = 4318`
- `RW-data = 2932`
- `ZI-data = 1512`
- `Total ROM Size = 53556 bytes = 52.30 KB`
- `Total RW Size = 7592 bytes = 7.41 KB`

### 3.3 另一套构建 `C030v1.0`

`C030v1.0` 的布局相同：

- `LR_IROM1 0x08004800 0x00020000`
- `RW_IRAM1 0x20000000 0x00005000`

其 ROM 尺寸为：

- `Total ROM Size = 56088 bytes = 54.77 KB`

## 4. EasyLogger Flash 日志插件

仓库里带了 EasyLogger 的 Flash 插件源码：

- [elog_flash.c](103 + 309/Project/Source/easylogger/plugins/flash/elog_flash.c)
- [elog_flash.h](103 + 309/Project/Source/easylogger/plugins/flash/elog_flash.h)
- [elog_flash_cfg.h](103 + 309/Project/Source/easylogger/plugins/flash/elog_flash_cfg.h)
- [elog_flash_port.c](103 + 309/Project/Source/easylogger/plugins/flash/elog_flash_port.c)

当前状态：

- 已开启 `ELOG_FLASH_USING_BUF_MODE`
- `ELOG_FLASH_BUF_SIZE`  هنوز未配置具体数值
- `elog_flash_port_*()` 仍是空实现

所以这部分目前还是框架状态，没有真正接入持久化 Flash 区。

## 5. 重点结论

1. 外部 EEPROM 是主参数仓库，承担了保护参数、校准值、SOC 表、故障记录、产品 ID 等绝大部分可配置数据。
2. 内部 Flash 主要用于启动模式、休眠模式和 IAP 跳转控制。
3. 当前工程代码区从 `0x08004800` 开始，ROM 约 `52.30 KB`，RAM 约 `7.41 KB`。
4. `FLASH_ADDR_SH367309_VALUE` 这个名字容易误导，它的实际访问路径是 EEPROM API，不是内部 Flash API。
5. 日志记录区是允许自动修复的，保护参数区则只做校验，不会自动改默认值。

## 6. 相关文档

- [EEPROM_LAYOUT_OPTIMIZATION.md](EEPROM_LAYOUT_OPTIMIZATION.md)：EEPROM 地址规划与读写优化说明。
- [COMMUNICATION_EEPROM_FLAG_REFACTOR_DEBUG.md](COMMUNICATION_EEPROM_FLAG_REFACTOR_DEBUG.md)：通信写 EEPROM 标志收敛与 Keil 调试方案。
- [COMMUNICATION_EEPROM_FLAG_MAPPING.md](COMMUNICATION_EEPROM_FLAG_MAPPING.md)：通信字段、dirty 位和 EEPROM 参数块映射表。

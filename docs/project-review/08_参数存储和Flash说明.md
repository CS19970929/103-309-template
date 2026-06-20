# 08 — 参数存储和 Flash 说明

## Flash 地址总布局

**源文件**: `Flash.h` (BMS APP), `main.h` (IAP)

### BMS 固件视角 (STM32F103RCT6, 256KB)

```
地址范围               大小      内容
──────────────────────────────────────────────────────
0x08000000 - 0x080047FF  18 KB   IAP 引导程序
0x08004800 - 0x0801BFFF  94 KB   APP 应用固件
0x0801C000 - 0x0801BFFF           (保留/未使用)
0x0801C000 - 0x0801C3FF   1 KB   AFE 参数 Slot A (24 字)
0x0801C400 - 0x0801C7FF   1 KB   读写参数 Slot A (65+32+24=121 字)
0x0801C800 - 0x0801CBFF   1 KB   AFE 参数 Slot B
0x0801CC00 - 0x0801CFFF   1 KB   读写参数 Slot B
0x0801D000 - 0x0801D7FF   2 KB   事件日志 Slot A (100 条)
0x0801D800 - 0x0801DFFF   2 KB   事件日志 Slot B
0x0801E000 - 0x0801E7FF   2 KB   SOC 数据 Slot A
0x0801E800 - 0x0801EFFF   2 KB   SOC 数据 Slot B
0x0801F000 - 0x0801F3FF   1 KB   升级参数策略标志
0x0801F400 - 0x0801F7FF   1 KB   工厂老化数据
0x0801F800 - 0x0801FBFF   1 KB   固件更新标志
0x0801FC00 - 0x0801FFFF   1 KB   休眠模式标志
──────────────────────────────────────────────────────
                            256 KB (结束)
```

### IAP 固件视角 (STM32F103CB, 128KB)

```
地址范围               大小      内容
──────────────────────────────────────────────────────
0x08000000 - 0x080047FF  18 KB   IAP 引导程序
0x08004800 - 0x0801F7FF          APP 区域 (IAP 通过 CAN 写入)
0x0801F800 - 0x0801FFFF   2 KB   (保留，IAP 不使用)
──────────────────────────────────────────────────────
                            128 KB (结束)
```

**链接脚本** (`IAP_103_Plus.sct`):
```
LR_IROM1 0x08000000 0x00020000 { ... }     ; 128 KB 总 Flash
RW_IRAM1 0x20000000 0x00004FE0 { ... }     ; 20 KB SRAM (保留最后 32B 给邮箱)
```

## 存储策略

### 三种存储模式

| 模式 | 适用数据 | 实现方式 | 磨损均衡 |
|------|---------|---------|---------|
| **日志型 (Journal Pair)** | SOC 数据, 事件日志, 老化, 升级标志, 休眠标志 | 两个页面交替写入，序列号递增，CRC 校验 | ✅ 页面级 |
| **双槽位 (Pair)** | AFE 参数, 读写参数 | 两个固定槽位 (A/B)，写入时选序大的擦除 | ✅ 槽位级 |
| **单页面** | （已废弃） | 单页面写入 | ❌ |

### 日志型存储结构

**源文件**: `Flash.c:23-31`

```c
typedef struct {
    UINT32 magic;      // 幻数标识
    UINT16 version;    // 格式版本
    UINT16 length;     // 数据长度
    UINT32 sequence;   // 递增序列号
    UINT16 crc;        // CRC 校验
    UINT8  reserved[4];
} STORAGE_FLASH_HEADER;  // 12 字节头
```

**写入流程** (`StorageFlash_SaveSocData`):
1. 比较两个 slot 的 sequence，选较小的
2. 擦除该 slot 所在页面 (1KB)
3. 写入 header + data
4. 回读校验 CRC

**读取流程** (`StorageFlash_LoadSocData`):
1. 检查两个 slot 的有效性 (magic + CRC)
2. 选 sequence 更大（更新）的 slot
3. 返回数据

### 双槽位存储结构

用于 AFE 参数和读写参数。每个槽位有 version 和 CRC：
1. 读取时：验证两个槽位，选有效的（version + CRC 正确），选 version 更大的
2. 写入时：选 version 较小的槽位，擦除后写入新数据并更新 version

## 各存储区详情

### AFE 参数 (0x0801C000/0x0801C800)

**大小**: 24 字 (48 字节)
**内容**: SH367309 校准值、电芯配置
**API**: `StorageFlash_LoadAfeData()`, `StorageFlash_SaveAfeData()`

### 读写参数 (0x0801C400/0x0801CC00)

**大小**: 121 字 (242 字节)

| 子区 | 偏移 | 字数 | 内容 |
|------|------|------|------|
| Protect | 0 | 65 | 13 种故障 × 5 参数 (三级阈值 + 恢复 + 滤波) |
| Other | 65 | 32 | OtherElement: 均衡/电流/休眠/SOC/系统参数 |
| Reserved | 97 | 24 | 保留 |

**API**: `StorageFlash_LoadRwParamData()`, `StorageFlash_SaveRwParamData()`

### 事件日志 (0x0801D000/0x0801D800)

**大小**: 2 KB × 2 = 4 KB
**容量**: 100 条记录
**结构**: `records[100][2]` (每条 2 字: 类型 + 时间戳)

**API**: `StorageFlash_LoadLogData()`, `StorageFlash_SaveLogData()`

### SOC 数据 (0x0801E000/0x0801E800)

**大小**: 2 KB × 2 = 4 KB
**结构**: `STORAGE_FLASH_SOC_DATA` (36 字节)
**版本**: V2 (`0x0002`)，兼容 V1 自动迁移

**API**: `StorageFlash_LoadSocData()`, `StorageFlash_SaveSocData()`

### 升级参数策略标志 (0x0801F000)

**大小**: 1 KB
**内容**: `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` (当前 0x0618)
**重置值**: `FLASH_UPGRADE_PARAM_FLAG_RESET = 0xFFFF`

### 工厂老化数据 (0x0801F400)

**大小**: 1 KB
**结构**: `STORAGE_FLASH_FACTORY_AGING_DATA`

| 字段 | 类型 | 含义 |
|------|------|------|
| `u32Elapsed10ms` | UINT32 | 已老化时间 (10ms 单位) |
| `u16State` | UINT16 | 状态: 0xA931(RUNNING) / 0xA930(STOPPED) / 0xA93D(DONE) |
| `u16DurationHours` | UINT16 | 老化时长 (小时) |

### 固件更新标志 (0x0801F800)

**大小**: 1 KB
**用途**: `App_FlashUpdate()` 检查此标志，设定时触发复位跳转

### 休眠模式标志 (0x0801FC00)

**大小**: 1 KB
**用途**: `SleepDeal_Continue()` 在进入休眠前写入 Boot Flag 值

| 标志值 | 含义 |
|--------|------|
| `0x1234` | NORMAL 休眠 |
| `0x1235` | DEEP 休眠 |
| `0x1236` | HICCUP 休眠 |
| `0x1237` | 充电器唤醒 |
| `0xFFFF` | 复位/上电 |

## EEPROM 抽象层

**源文件**: `EEPROM.c/.h`

EEPROM 层对外提供统一接口，**实际存储全部委托给 Flash**：

```
上层 (Sci_Upper, 保护, SOC...)
  │
  ├── EEPROM_SaveRWParametersToFlash()
  ├── ReadEEPROM_AFE_Parameters()
  ├── ReadEEPROM_EventRecord_Parameters()
  └── UpgradeParamPolicy_ApplyOnce()
  │
  └──→ Flash.c (StorageFlash_Save*/Load*)
```

旧的 I2C EEPROM 函数 (`ReadEEPROM_Byte`, `WriteEEPROM_Byte` 等) 为桩函数，**实际上不使用**。

## 参数加载流程 (`InitE2PROM()`)

**源文件**: `EEPROM.c:318-325`

```
1. EEPROM_LoadDefaultRuntimeData()
   → 加载编译时默认值（保护参数、校准系数、OtherElement）
2. EEPROM_LoadRWParametersFromFlash()
   → 从 Flash 加载上次保存的参数，覆盖默认值
3. ReadEEPROM_AFE_Parameters()
   → 加载 AFE 特定参数，验证范围，无效则用默认值
4. ReadEEPROM_EventRecord_Parameters()
   → 加载事件日志环
5. UpgradeParamPolicy_ApplyOnce()
   → 如果升级策略版本变化，按 Project_Config 的设定重置指定参数
```

## 参数合法性验证

**源文件**: `DataDeal.h` (OtherElement_min/max), `Fault.h` (Protect_min/max)

每个参数都有编译时定义的 min/max/default 三元组：
- `EEPROM_LoadDefaultRuntimeData()` 使用 default
- RS485 写操作时使用 min/max 范围检查
- Flash 加载失败时回退到 default

## 磨损均衡

- **日志型**: 每次写入选 sequence 较小的页面，交替使用两个页面
- **双槽位**: 每次写入选 version 较小的槽位
- **预期寿命**: STM32F103 Flash 擦写次数 ≥10,000 次。以 6 次/小时 (RS485 修改) 估计，约可支持 1,667 小时连续修改

## 数据完整性

| 保护机制 | 适用 |
|---------|------|
| Magic Number | 所有存储类型 |
| CRC16 | 日志型 header |
| Sequence Number | 日志型（选最新的有效页） |
| Version Number | 双槽位（选最新的有效槽） |
| Min/Max 范围检查 | 所有参数（加载后验证） |

## 待确认问题

1. CRC 多项式具体是什么？（不同存储类型可能不同）
2. 页面擦除失败时的重试机制？（Flash 擦除可能因电源问题失败）
3. 是否所有参数字段都有范围检查？（`reserved` 字段似乎无检查）
4. Flash 写入期间如果断电，恢复策略是什么？（双槽位有回退，日志型可能丢失最新一条）

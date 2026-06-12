# BMS 存储（Flash）与 LED 显示模块分析

---

## 第一部分：存储模块

### 1. Flash 内存布局

Flash 存储区位于 STM32 片上 Flash 的后段（`0x0801C000` 起始），与 IAP（`0x08000000`）和 App（`0x08004800`）区域互不干扰。整体布局如下：

```
地址             用途                   大小        备注
─────────────────────────────────────────────────────────────
0x08000000       IAP Bootloader         -          禁止覆盖
0x08004800       App 起始               -          正常 App 入口
...              App 代码/数据          -          -
0x0801C000       AFE Slot A             1KB/2KB    AFE 校准参数
0x0801C400       RW_PARAM Slot A        1KB/2KB    可读写保护/校准参数
0x0801C800       AFE Slot B             1KB/2KB    AFE 校准参数（冗余）
0x0801CC00       RW_PARAM Slot B        1KB/2KB    可读写保护/校准参数（冗余）
0x0801D000       LOG Slot A             2KB        事件日志
0x0801D800       LOG Slot B             2KB        事件日志（冗余）
0x0801E000       SOC Slot A (SH367309_VALUE) 1KB/2KB  SOC 数据
0x0801E800       SOC Slot B (SH367309_FLAG)  1KB/2KB  SOC 数据（冗余）
0x0801F000       Upgrade Param Flag     1页        升级参数策略标记
0x0801F400       Factory Aging Flag     1页        工厂老化标记
0x0801F800       Update Flag            1页        更新标记
0x0801FC00       Sleep Flag             1页        睡眠标记
```

**页大小**（`FLASH.h` 第 16-19 行）：
- STM32F10X_MD（中容量）：`0x400`（1KB）
- 其他型号：`0x800`（2KB）

**Flash 大小检测**：通过读取 `0x1FFFF7E0` 地址的寄存器获取实际 Flash 容量，若 < 128KB 则跳过后段存储检查（`Flash.c` 第 1006-1035 行）。

### 2. A/B 双槽与 Journal 存储机制

#### 2.1 A/B 双槽（Pair）模式

用于 **AFE 校准参数** 和 **RW 参数**（`FLASH.h` 第 22-25 行）：

- 每类数据分配两个独立 Flash 页（Slot A 和 Slot B）
- 写入时只擦写其中一个槽，另一个保留为上一次的备份
- 通过递增的 `sequence` 号选择最新数据：**sequence 大的为最新**

**读取流程**（`StorageFlash_LoadPair`，`Flash.c` 第 244-286 行）：
1. 分别读取 Slot A 和 Slot B
2. 验证 magic、version、length、CRC
3. 两者都有效时选 sequence 更大的；只有一个有效时选有效的
4. 将选中数据复制到输出缓冲区

**写入流程**（`StorageFlash_SavePair`，`Flash.c` 第 309-371 行）：
1. 读取两个槽，确定哪个 sequence 更大
2. 将新数据写入另一个槽（非最新槽）
3. sequence = 最新 sequence + 1
4. 写入后立即回读验证

#### 2.2 Journal 模式

用于 **SOC 数据** 和 **事件日志**，比 Pair 模式增加了一个关键特性：**同一页内追加写入**，减少擦写次数。

**SOC Journal**（`Flash.c` 第 28-29 行）：
```
SOC Slot A = 0x0801E000 (SH367309_VALUE)
SOC Slot B = 0x0801E800 (SH367309_FLAG)
```

**日志 Journal**（`Flash.c` 第 26-27 行）：
```
LOG Slot A = 0x0801D000
LOG Slot B = 0x0801D800
```

**读取流程**（`StorageFlash_LoadJournalPage`，`Flash.c` 第 373-439 行）：
1. 从页首开始，按 `record_span`（对齐到 2 字节）扫描每条记录
2. 遇到 `0xFFFF` 区域（blank）则停止
3. 对每条有效记录验证 magic + CRC，保留 sequence 最大的
4. 同时记录空白区域起始地址（`next_addr`），用于后续追加写入

**写入流程**（`StorageFlash_SaveJournalPair`，`Flash.c` 第 485-596 行）：
1. 加载两个 Journal 页，获得各自的 `next_addr` 和 `sequence`
2. 选择 sequence 更大的槽，尝试在其 `next_addr` 追加写入
3. 若追加空间不足（`target_addr + record_span > page_end`），切换到另一个槽并擦除
4. 若两个槽都已满或初始为空，擦除当前页后写入
5. 写入后立即回读验证（magic + CRC + sequence + memcmp）

**单页 Journal**（`StorageFlash_SaveJournalPage`，`Flash.c` 第 598-677 行）：用于 Factory Aging 数据，只用单个页做 Journal。

#### 2.3 记录跨度计算

```c
// Flash.c 第 157-168 行
record_span = sizeof(STORAGE_FLASH_HEADER) + payload_length;
if (record_span & 0x0001) record_span += 1;  // 2 字节对齐
```

每条记录 = 12 字节 Header + payload，保证 2 字节对齐（Flash 最小编程单元为 HalfWord）。

### 3. 关键函数（含行号）

#### Flash.c 核心存储函数

| 函数 | 行号 | 功能 |
|------|------|------|
| `StorageFlash_CalcCrc` | 78-86 | CRC16 计算，委托给 `Sci_CRC16RTU` |
| `FlashErasePageVerified` | 88-108 | 擦除一页并逐 HalfWord 验证全部为 `0xFFFF` |
| `FlashProgramHalfWordVerified` | 110-126 | 编程单个 HalfWord 并回读验证 |
| `FlashProgramBytesVerified` | 128-155 | 按字节序列编程（内部转为 HalfWord） |
| `StorageFlash_ReadSlot` | 185-217 | 读取单个 Slot：验证 magic/version/length/CRC |
| `StorageFlash_ProgramRecord` | 219-242 | 写入 Header + Payload 到指定地址 |
| `StorageFlash_LoadPair` | 244-286 | A/B 双槽读取（选择最新 sequence） |
| `StorageFlash_WriteSlot` | 288-307 | 擦除+写入单个 Slot（含 Flash 解锁/锁定） |
| `StorageFlash_SavePair` | 309-371 | A/B 双槽写入（写非最新槽+回读验证） |
| `StorageFlash_LoadJournalPage` | 373-439 | Journal 页扫描：找最新有效记录+空白地址 |
| `StorageFlash_LoadJournalPair` | 441-483 | 双 Journal 页读取 |
| `StorageFlash_SaveJournalPair` | 485-596 | 双 Journal 页写入（追加/切换/擦除） |
| `StorageFlash_SaveJournalPage` | 598-677 | 单 Journal 页写入 |

#### Flash.c 对外 API

| 函数 | 行号 | 功能 |
|------|------|------|
| `StorageFlash_LoadSocData` | 752-789 | 加载 SOC 数据（兼容 V1/V2 格式） |
| `StorageFlash_SaveSocData` | 791-812 | 保存 SOC 数据（Journal 模式） |
| `StorageFlash_LoadAfeData` | 814-826 | 加载 AFE 校准数据（Pair 模式） |
| `StorageFlash_SaveAfeData` | 828-846 | 保存 AFE 校准数据 |
| `StorageFlash_LoadRwParamData` | 848-860 | 加载 RW 参数（Pair 模式） |
| `StorageFlash_SaveRwParamData` | 862-880 | 保存 RW 参数 |
| `StorageFlash_LoadLogData` | 882-903 | 加载事件日志（Journal 模式） |
| `StorageFlash_SaveLogData` | 905-939 | 保存事件日志（含相同数据跳过优化） |
| `StorageFlash_LoadFactoryAgingData` | 941-966 | 加载工厂老化数据（单 Journal + 旧格式兼容） |
| `StorageFlash_SaveFactoryAgingData` | 968-985 | 保存工厂老化数据 |
| `FlashWriteOneHalfWord` | 679-704 | 带重试的半字写入（用于 Flag 类数据） |
| `FlashReadOneHalfWord` | 706-709 | 直接指针读取半字 |
| `StorageFlash_PrintBootCheck` | 1004-1123 | 启动时打印所有存储区状态 |
| `App_FlashUpdate` | 1125-1136 | 触发 Flash 更新（关 MOS 后复位） |
| `APP_To_IAP_Jump` | 1138-1145 | 请求跳转到 IAP |
| `InitAreaSelect` | 1147-1154 | 启动时检查 IAP 请求 |

#### EEPROM.c 函数

| 函数 | 行号 | 功能 |
|------|------|------|
| `EEPROM_UpdateOtherElementRuntime` | 16-23 | 从 OtherElement 更新运行时变量 |
| `EEPROM_LoadDefaultProtect` | 25-34 | 加载保护参数默认值 |
| `EEPROM_LoadDefaultCalib` | 36-45 | 加载校准系数默认值 |
| `EEPROM_LoadDefaultOtherElement` | 47-58 | 加载其他参数默认值 |
| `EEPROM_BuildRWParamData` | 60-76 | 从内存构建 RW 参数存储结构 |
| `EEPROM_RWParamDataIsValid` | 93-114 | 范围校验：保护参数 + 其他参数 |
| `EEPROM_ApplyRWParamData` | 116-129 | 将 Flash 数据应用到运行时结构体 |
| `EEPROM_SaveRWParametersToFlash` | 131-143 | 校验后保存 RW 参数到 Flash |
| `EEPROM_LoadRWParametersFromFlash` | 145-157 | 从 Flash 加载 RW 参数（失败则写默认值） |
| `UpgradeParamPolicy_ApplyOnce` | 206-290 | 升级参数策略一次性应用（含多条件编译开关） |
| `InitE2PROM` | 318-325 | EEPROM 子系统初始化入口 |

#### LogRecord.c 函数

| 函数 | 行号 | 功能 |
|------|------|------|
| `LogRecord_CanSaveEvent` | 30-56 | 重复间隔检查（最小间隔秒数） |
| `LogRecord_MarkEventSaved` | 58-65 | 记录事件最后保存时间 |
| `LogRecord_IsEntryValid` | 67-80 | 校验日志条目合法性 |
| `LogTime_Map` | 92-111 | 秒数映射到 1 字节（≤60s=171, ≤7天=小时数, >7天=170） |
| `LogEvent_EEPROM` | 113-137 | 写入一条事件日志（环形写入 + Flash 持久化） |
| `LogEvent_Record` | 139-188 | 事件记录主逻辑：按事件类型分派（启动/睡眠/CBC/通用锁存） |
| `App_LogRecord` | 190-222 | 1s 周期调用：扫描所有保护标志并记录 |
| `Sci_ACK_0x03_ReadRegs_EventRecord` | 224-240 | Modbus 0x03 读事件记录 |
| `Sci_WrReg_0x06_Reset_EventRecord` | 242-264 | Modbus 0x06 重置事件记录 |
| `EEPROM_ResetData_EventRecord_ToDefault` | 266-278 | 清空事件记录并持久化 |
| `ReadEEPROM_EventRecord_Parameters` | 280-314 | 启动时加载事件记录（含完整性校验） |

### 4. 数据结构

#### Flash.c 内部结构

```c
// Flash.c 第 16-21 行 — SOC 数据 V1（旧版兼容）
typedef struct {
    UINT16 u16SocNow;       // 当前 SOC
    UINT16 u16DsgSocInt;    // 放电 SOC 整数
    UINT32 u32CycleTimes;   // 循环次数
} STORAGE_FLASH_SOC_DATA_V1;

// Flash.c 第 23-31 行 — 通用存储头
typedef struct {
    UINT32 magic;       // 类型标识（如 0x534F4331 = "SOC1"）
    UINT16 version;     // 格式版本（当前 0x0001）
    UINT16 length;      // Payload 长度
    UINT32 sequence;    // 递增序列号（用于新旧比较）
    UINT16 crc;         // Payload 的 CRC16
    UINT16 reserved;    // 保留（0xFFFF）
} STORAGE_FLASH_HEADER;  // 共 12 字节

// Flash.c 第 33-38 行 — 事件日志数据
typedef struct {
    UINT8 point;                                    // 写入指针（环形索引）
    UINT8 reserved;
    UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]; // [event][delta_time]
} STORAGE_FLASH_LOG_DATA;

// Flash.c 第 40-47 行 — App→IAP 升级邮箱
typedef struct {
    UINT32 magic;         // 0x49415031 ("IAP1")
    UINT32 magic_inv;     // ~magic（反码校验）
    UINT32 request;       // 0x5AA55AA5
    UINT32 request_inv;   // ~request
    UINT32 crc;           // magic ^ request ^ 0xA5A55A5A
} APP_UPGRADE_MAILBOX;

// Flash.c 第 49-52 行 — Flash 运行时状态
typedef struct FLASH_RUNTIME_TAG {
    volatile UINT8 busy;  // 写入忙标志
} FLASH_RUNTIME;
```

#### Flash.h 公开结构

```c
// Flash.h 第 61-75 行 — SOC 数据 V2（当前版本）
typedef struct {
    UINT16 u16FormatVersion;       // 版本号（0x0002）
    UINT16 u16SocNow;              // 当前 SOC
    UINT16 u16DsgSocInt;           // 放电 SOC 整数
    UINT16 u16MaxErrorPercent;     // 最大误差百分比
    UINT32 u32CycleTimes;          // 循环次数
    UINT32 u32CapNow;              // 当前容量
    UINT32 u32CapFull;             // 满充容量
    UINT32 u32LearnPassedAs10;     // 学习通过量（×10）
    UINT16 u16LearnAnchorSoc;      // 学习锚点 SOC
    UINT16 u16LearnState;          // 学习状态
    UINT16 u16Flags;               // 标志位
    UINT16 u16Reserved[4];         // 保留
} STORAGE_FLASH_SOC_DATA;  // 共 40 字节

// Flash.h 第 77-82 行 — RW 参数数据
typedef struct {
    UINT16 protect[65];   // 保护参数（OVP/UVP/OCP 等）
    UINT16 other[32];     // 其他参数（均衡/SOC 配置等）
    UINT16 reserved[24];  // 保留
} STORAGE_FLASH_RW_PARAM_DATA;  // 共 242 字节

// Flash.h 第 84-89 行 — 工厂老化数据
typedef struct {
    UINT32 u32Elapsed10ms;    // 已用时间（10ms 单位）
    UINT16 u16State;           // 状态（RUNNING/STOPPED/DONE）
    UINT16 u16DurationHours;   // 老化时长（小时）
} STORAGE_FLASH_FACTORY_AGING_DATA;
```

#### EEPROM.c 内部结构

```c
// LogRecord.c 第 6-16 行 — 日志运行时
typedef struct LOG_RECORD_RUNTIME_TAG {
    UINT8 point;                            // 环形写入指针
    UINT8 records[EVENT_RECORD_LENGTH][2];  // [event][delta_time]
    LOG_RECORD_FLAG flags;                  // 启动/睡眠请求标志
    UINT32 uptimeSeconds;                   // 运行时间（秒）
    UINT32 lastSaveSeconds[EVENT_NUM];      // 各事件最后保存时间
    UINT8 lastSaveValid[EVENT_NUM];         // 各事件最后保存时间有效位
    UINT8 eventLatch[EVENT_NUM];            // 事件锁存状态
    UINT8 cbcTemp;                          // CBC 错误临时值
} LogRecordRuntime;
```

### 5. CRC 验证机制

#### 存储记录 CRC

- **算法**：CRC16-RTU（`Sci_CRC16RTU`）
- **覆盖范围**：仅覆盖 Payload 数据（不含 Header）
- **计算时机**：
  - 写入时：`StorageFlash_ProgramRecord`（`Flash.c` 第 232 行）计算并存入 Header
  - 读取时：`StorageFlash_ReadSlot`（`Flash.c` 第 200 行）重新计算并与 Header 中的 CRC 比较
- **空值保护**：`StorageFlash_CalcCrc`（`Flash.c` 第 78-86 行）当 data==NULL 或 length==0 时返回 `0xFFFF`

#### IAP Mailbox CRC

- **算法**：XOR + 常量
  ```c
  crc = magic ^ request ^ 0xA5A55A5A;  // Flash.c 第 718 行
  ```
- **双重反码保护**：magic/magic_inv 和 request/request_inv 同时存储，校验时必须匹配

#### 老化数据兼容性校验

`StorageFlash_LoadFactoryAgingData`（`Flash.c` 第 941-966 行）：
- 优先尝试 Journal 格式读取
- 若 Journal 为空但 `FLASH_ADDR_FACTORY_AGING_FLAG` 地址存储了 `FLASH_FACTORY_AGING_DONE_VALUE`（0xA93D），则兼容旧格式返回 DONE 状态

### 6. BootFlag 机制

#### SRAM 升级邮箱（IAP 请求）

位于 SRAM 地址 `0x20004FE0`（`Flash.c` 第 12 行），使用 `APP_UPGRADE_MAILBOX` 结构：

- `magic` = `0x49415031` + `magic_inv` = `~magic`
- `request` = `0x5AA55AA5` + `request_inv` = `~request`
- `crc` = `magic ^ request ^ 0xA5A55A5A`

**写入**（`AppUpgrade_RequestIap`，`Flash.c` 第 738-750 行）：App 调用后复位，Bootloader 在 `InitAreaSelect`（`Flash.c` 第 1147-1154 行）中检查，匹配则跳转 IAP 流程。

**检测**（`AppUpgrade_IsIapRequested`，`Flash.c` 第 721-736 行）：校验全部五个字段，任一不匹配即返回 0。

#### Flash Flag 页

| 地址 | 用途 | 典型值 |
|------|------|--------|
| `0x0801F000` | 升级参数策略标记 | `0xFFFF`=未应用, 其他=已应用版本号 |
| `0x0801F400` | 工厂老化状态 | `0xFFFF`=复位, `0xA931`=运行, `0xA930`=停止, `0xA93D`=完成 |
| `0x0801F800` | 更新标记 | `u8FlashUpdateFlag` 触发时复位 |
| `0x0801FC00` | 睡眠标记 | `0x1234`=普通睡眠, `0x1235`=深度睡眠, `0x1236`=Hiccup, `0x1237`=充电唤醒 |

#### 升级参数策略（`UpgradeParamPolicy_ApplyOnce`，EEPROM.c 第 206-290 行）

1. 读取 `FLASH_ADDR_UPGRADE_PARAM_FLAG`
2. 若已等于 `UPGRADE_PARAM_POLICY_VERSION` 则跳过（幂等）
3. 按条件编译开关执行：重置 AFE/保护参数/均衡电压/SOC 配置/事件记录/老化时间
4. 全部成功后写入版本标记，防止重复执行

#### SOC 数据版本迁移（`StorageFlash_LoadSocData`，Flash.c 第 752-789 行）

1. 优先以 V2 格式（40 字节）读取
2. 若 V2 读取失败，尝试 V1 格式（8 字节）读取
3. V1 数据自动迁移到 V2 结构：补零 + 设置 `u16MaxErrorPercent = 100`

---

## 第二部分：LED 显示模块

### 1. Charlieplexing 实现

系统使用 **5 根 GPIO 引脚**（`LedBar.c` 第 9-18 行）通过 Charlieplexing 驱动 LED 灯条：

| Pin ID | GPIO | 引脚 | 原始用途 |
|--------|------|------|----------|
| 0 | GPIOB | Pin 11 | 通用 IO |
| 1 | GPIOB | SCK (Pin 13) | SPI1_SCK |
| 2 | GPIOB | NSS (Pin 12) | SPI1_NSS |
| 3 | GPIOB | MOSI (Pin 15) | SPI_MOSI |
| 4 | GPIOB | SEG_EN | 段使能 |

**Charlieplexing 原理**：N 根线最多驱动 N×(N-1) 个 LED。5 根线可驱动 20 个 LED，本项目使用 18 路路由：

```
路由编号  引脚对 (low_pin → high_pin)
──────────────────────────────────────
  0       3 → 2        百位竖线（上）
  1       3 → 1        百位竖线（下）
  2       2 → 1        十位段 A
  3       1 → 2        十位段 B
  4       2 → 3        十位段 C
  5       1 → 3        十位段 D
  6       1 → 4        十位段 E
  7       2 → 4        十位段 F
  8       3 → 4        十位段 G
  9       1 → 0        个位段 A
 10       0 → 1        个位段 B
 11       2 → 0        个位段 C
 12       0 → 2        个位段 D
 13       3 → 0        个位段 E
 14       0 → 3        个位段 F
 15       0 → 4        个位段 G
 16       4 → 2        充电图标
 17       4 → 1        百分号图标
```

**路由表**定义在 `s_ledbar_routes[]`（`LedBar.c` 第 117-137 行），每条路由指定 `low_pin`（输出 LOW）和 `high_pin`（输出 HIGH），电流从 high_pin 流向 low_pin 经过对应的 LED。

**段编码**：使用标准七段显示映射 `s_ledbar_digit_map[]`（`LedBar.c` 第 148-169 行），每一位对应 A~G 段的位掩码（第 37-43 行定义）。

**显示数值构建**（`LedBar_BuildTargetMask`，`LedBar.c` 第 482-511 行）：
1. 提取百位/十位/个位数字
2. 百位 ≥100 时点亮路由 0 和 1（竖线）
3. 十位 ≥10 时查表添加路由 2~8
4. 个位始终添加路由 9~15
5. 充电图标和百分号图标按 `indicator_mask` 添加路由 16~17

### 2. TIM4 扫描定时器

**定时器初始化**（`LedBar_ScanTimerInit`，`LedBar.c` 第 410-432 行）：

```
时钟源: APB1 (TIM4)
预分频: SystemCoreClock / 100000 - 1  → 得到 100kHz 基频
周期: 50 ticks - 1  → 每 500μs 产生一次中断（即 2kHz 刷新率）
```

**具体配置**：
- `TIM_Prescaler`：由 `LedBar_GetTimerPrescalerFor100kHz()`（第 394-408 行）计算
- `TIM_Period`：`LEDBAR_SCAN_TIMER_100KHZ_TICKS - 1 = 49`
- `TIM_ClockDivision`：`TIM_CKD_DIV1`
- `TIM_CounterMode`：`TIM_CounterMode_Up`
- `NVIC`：抢占优先级 1，子优先级 3

**中断处理**（`TIM4_IRQHandler`，`LedBar.c` 第 1126-1134 行）：
```c
void TIM4_IRQHandler(void) {
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
        IrqDebug_CountFast(IRQDBG_TIM4_LEDBAR);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        LedBar_Scan1ms();  // 推进到下一个路由
    }
}
```

**扫描周期**：500μs 切换一个路由。18 路由全扫描一轮 = 18 × 500μs = **9ms**，刷新率约 **111Hz**，肉眼无闪烁。

**启动/停止**（第 434-464 行）：
- `LedBar_StartScanTimer`：初始化 GPIO → 初始化 TIM4 → 启用中断 → 启动计数器
- `LedBar_StopScanTimer`：禁用中断 → 停止计数器 → 关闭 APB1 时钟

### 3. 关键函数（含行号）

#### LedBar.c 核心函数

| 函数 | 行号 | 功能 |
|------|------|------|
| `LedBar_Init` | 960-986 | 初始化运行时状态、GPIO、输出关闭 |
| `LedBar_SetSleep` | 996-1013 | 设置睡眠模式（禁用/启用显示） |
| `LedBar_SaveSleepSoc` | 1015-1022 | 将 SOC 存入 Backup 寄存器 |
| `LedBar_LoadSleepSoc` | 1024-1047 | 从 Backup 寄存器恢复 SOC（含反码校验） |
| `LedBar_ShowSleepSocPreview` | 1049-1058 | 显示睡眠前缓存的 SOC 值 |
| `LedBar_RequestSocDisplay` | 1060-1065 | 请求显示 SOC（启动显示窗口） |
| `LedBar_PrepareForStop` | 1067-1075 | 进入停机前准备（关闭显示） |
| `LedBar_IsActiveForLowPower` | 1077-1097 | 检查 LED 模块是否阻止低功耗 |
| `APP_LedBar` | 1136-1189 | **主调度函数**：处理唤醒/按键/显示更新 |

#### LedBar.c 内部 GPIO/扫描函数

| 函数 | 行号 | 功能 |
|------|------|------|
| `LedBar_PinModeF1` | 274-292 | 直接操作 CRL/CRH 寄存器设置 GPIO 模式 |
| `LedBar_PinWrite` | 294-307 | 直接操作 BSRR/BRR 写引脚 |
| `LedBar_PinToOutput` | 309-317 | 设为推挽输出并写电平 |
| `LedBar_PinToOutputMode` | 319-325 | 仅设为推挽输出模式 |
| `LedBar_AllPinsHiZ` | 327-331 | 所有引脚设为浮空输入（高阻态） |
| `LedBar_AllPinsOutputLow` | 333-342 | 所有引脚输出 LOW |
| `LedBar_OutputRoute` | 371-387 | 输出单条路由（设置 high/low 引脚） |
| `LedBar_OutputOff` | 389-392 | 关闭所有输出（高阻态） |
| `LedBar_Scan1ms` | 1099-1124 | TIM4 中断中的扫描推进逻辑 |
| `TIM4_IRQHandler` | 1126-1134 | TIM4 中断服务程序 |

#### LedBar.c 帧构建与优化函数

| 函数 | 行号 | 功能 |
|------|------|------|
| `LedBar_BuildTargetMask` | 482-511 | 从数值+图标生成路由位掩码 |
| `LedBar_BuildFrameFromMask` | 732-776 | 从位掩码构建最优扫描帧 |
| `LedBar_BuildGreedyFrameFromStart` | 628-674 | 贪心算法构建帧 |
| `LedBar_ImproveFrameOrder` | 686-730 | 改进扫描顺序（减少鬼影） |
| `LedBar_TransitionCost` | 545-598 | 计算两路由间切换的鬼影代价 |
| `LedBar_FrameTransitionCost` | 600-626 | 计算整帧的总切换代价 |
| `LedBar_ApplyFrame` | 815-858 | 应用帧到硬件（含中断安全保护） |
| `LedBar_RefreshOutput` | 860-866 | 构建并应用当前帧 |

#### LedBar.c 服务函数

| 函数 | 行号 | 功能 |
|------|------|------|
| `LedBar_ServiceSwitch` | 909-958 | 按键扫描与长按检测（10ms 周期） |
| `LedBar_ServiceMcuWake` | 893-902 | MCU 唤醒引脚边沿检测 |
| `LedBar_ServiceStartupDisplayWindow` | 884-891 | 启动显示窗口管理 |

### 4. 鬼影消除算法

Charlieplexing 的核心问题是**鬼影**：路由切换时，前一路由的引脚电平与新路由的引脚电平瞬间叠加，可能意外点亮不属于当前帧的 LED。

#### 4.1 代价模型（`LedBar_TransitionCost`，第 545-598 行）

对相邻两条路由 (prev, next)，计算鬼影代价：

```
ghost_cost =
    // 情况1：prev.low 与 next 的引脚组合可能形成意外路由
    if (prev.low != next.low && prev.low != next.high):
        ghost_route = findRoute(prev.low, next.high)
        if ghost_route 存在且不是目标路由:
            cost += (ghost_route 在目标帧中? LEDBAR_TRANSITION_ON_GHOST_COST(1) : LEDBAR_TRANSITION_OFF_GHOST_COST(8))

    // 情况2：prev.high 与 next 的引脚组合可能形成意外路由
    if (prev.high != next.low && prev.high != next.high):
        ghost_route = findRoute(next.low, prev.high)
        if ghost_route 存在且不是目标路由:
            cost += (ghost_route 在目标帧中? 1 : 8)

    // 情况3：无共享引脚
    if (prev 的两个引脚与 next 的两个引脚完全不重叠):
        cost += LEDBAR_TRANSITION_NO_SHARED_PIN_COST(2)
```

**代价说明**：
- **意外点亮目标帧外的 LED**（`OFF_GHOST_COST = 8`）：代价最高，必须避免
- **意外点亮目标帧内的 LED**（`ON_GHOST_COST = 1`）：代价较低，可接受（短暂闪一下目标 LED）
- **无共享引脚**（`NO_SHARED_PIN_COST = 2`）：切换时所有引脚状态翻转，有一定风险

#### 4.2 贪心构建（`LedBar_BuildGreedyFrameFromStart`，第 628-674 行）

1. 从指定起始路由开始
2. 每次从未使用的路由中选择切换代价最低的路由
3. 重复直到所有目标路由都被添加

#### 4.3 改进扫描顺序（`LedBar_ImproveFrameOrder`，第 686-730 行）

多轮局部搜索优化：
1. 遍历帧中所有路由对 (left, right)
2. 尝试交换它们的位置
3. 若交换后总代价降低则保留，否则回退
4. 最多执行 `LEDBAR_ORDER_IMPROVE_MAX_PASSES`（= 18）轮
5. 无改进时提前退出

#### 4.4 最终帧选择（`LedBar_BuildFrameFromMask`，第 732-776 行）

1. 对每个可能的起始路由运行贪心构建
2. 选择总代价最低的帧
3. 再用 `LedBar_ImproveFrameOrder` 局部优化

### 5. 显示时序

```
                    10ms 周期任务（APP_LedBar）
                         │
                         ▼
              ┌─── ServiceMcuWake ─── 检测 MCU_WK 引脚边沿
              │
              ├─── ServiceSwitch ──── 按键扫描（含去抖）
              │    │                   按下 → 启动显示窗口
              │    │                   长按 500ms → 深度睡眠
              │    └───────────────── soc_display_10ms 递减
              │
              ├─── ServiceStartupDisplay ── 首次 10s 显示窗口
              │
              ├─── 检查 display_requested ── soc_display_10ms != 0
              │    │
              │    ├── 不请求 → 清除显示，停止扫描
              │    │
              │    └── 请求 → 获取 SOC 值 + 充电状态
              │         │
              │         ▼
              │    RefreshOutput()
              │         │
              │         ├── BuildCurrentFrame()
              │         │      ├── BuildTargetMask() ── 数值→路由掩码
              │         │      └── BuildFrameFromMask() ── 贪心+优化排序
              │         │
              │         └── ApplyFrame()
              │                ├── 禁用 TIM4 中断
              │                ├── 更新 frame 数据
              │                ├── OutputRoute(第一个路由)
              │                └── 启用 TIM4 中断
              │
              ▼
    TIM4 中断 (500μs 周期)
         │
         └── LedBar_Scan1ms()
              ├── 检查 scan_index
              ├── OutputRoute(当前路由)
              └── scan_index++（循环）
```

**关键时间参数**：

| 参数 | 值 | 来源 |
|------|-----|------|
| TIM4 中断周期 | 500μs | `LEDBAR_SCAN_TIMER_100KHZ_TICKS = 50`, 100kHz 基频 |
| 单路由持续时间 | 500μs | = TIM4 周期 |
| 全帧扫描周期 | 9ms (18路由) | 18 × 500μs |
| 帧刷新率 | ~111 Hz | 1 / 9ms |
| 主循环刷新周期 | 100ms | `b1Sys100msFlag` 条件检查 |
| SOC 显示窗口 | 5s | `LEDBAR_SOC_DISPLAY_10MS = 500`（×10ms） |
| 启动显示窗口 | 10s | `LEDBAR_STARTUP_DISPLAY_10MS = 1000`（×10ms） |
| 按键长按阈值 | 500ms | `LEDBAR_KEY_LONG_PRESS_10MS = 50`（×10ms） |

### 6. 按键处理

**硬件**：`GPIO_SW` 引脚，低电平有效（`LedBar_ReadSwitchRaw`，`LedBar.c` 第 252-255 行）

**处理逻辑**（`LedBar_ServiceSwitch`，`LedBar.c` 第 909-958 行）：

1. **触发条件**：仅在 10ms 周期任务中执行（`b1Sys10msFlag`）

2. **按下检测**（边沿触发，第 924-931 行）：
   ```
   if (从松开→按下):
       key_wakeup_armed = 1          // 武装唤醒标志
       RequestSocDisplayWindow()     // 启动 5s 显示窗口
       key_press_start_10ms = 当前时间
       key_hold_10ms = 0
       key_long_handled = 0
   ```

3. **长按检测**（第 933-944 行）：
   ```
   if (持续按住 && key_wakeup_armed):
       key_hold_10ms = 当前时间 - 按下时间
       if (key_hold_10ms >= 50 [即 500ms] && !key_long_handled):
           key_long_handled = 1
           low_power_log_and_commit_sleep(DEEP_MODE)  // 进入深度睡眠
   ```

4. **松开处理**（第 946-952 行）：
   ```
   if (松开):
       key_hold_10ms = 0
       key_press_start_10ms = 当前时间
       key_long_handled = 0
       key_wakeup_armed = 0
   ```

5. **显示倒计时**（第 954-957 行）：
   ```
   if (soc_display_10ms != 0):
       soc_display_10ms--
   ```

**MCU_WK 唤醒**（`LedBar_ServiceMcuWake`，第 893-902 行）：
- 检测 `GPIO_MCU_WK` 引脚上升沿
- 边沿触发一次 `RequestSocDisplayWindow()`
- 通过 `mcu_wk_active` 记忆上一次状态实现边沿检测

---

## 总结

### 存储模块设计要点

1. **双层冗余**：A/B 双槽 + sequence 号，任意时刻断电不丢数据
2. **写放大优化**：Journal 模式在同一页内追加记录，减少擦写次数
3. **全链路校验**：Header magic + version + length + CRC16 + 回读验证
4. **擦除验证**：每次擦除后逐 HalfWord 验证全 `0xFFFF`
5. **兼容性**：SOC V1→V2 迁移、老化数据旧格式兼容
6. **幂等升级策略**：升级参数通过 Flag 页防止重复应用

### LED 显示模块设计要点

1. **高效 Charlieplexing**：5 引脚驱动 18 路 LED，复用 SPI 引脚
2. **鬼影消除**：贪心 + 局部搜索优化扫描顺序，最小化切换代价
3. **TIM4 中断扫描**：500μs 精确切换，111Hz 刷新率无闪烁
4. **低功耗管理**：停止显示时关闭定时器 + 时钟 + 所有引脚高阻态
5. **Backup 寄存器跨复位**：SOC 值存入 BKP_DR4/DR5，带反码校验
6. **按键多功能**：短按查看 SOC，长按 500ms 进入深度睡眠

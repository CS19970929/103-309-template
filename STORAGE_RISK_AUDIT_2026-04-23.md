# 存储相关代码审计报告

日期：2026-04-23  
范围：`103 + 309/Project/Source` 当前主工程中所有与持久化、跨复位状态保存、掉电恢复相关的实现。  
重点：`Flash`、备份域 `BKP/RTC`、旧 `EEPROM` 兼容层、`SOC`、事件日志、AFE 参数、系统功能位、产品信息。

## 1. 执行结论

当前工程真正还在工作的持久化通道只有 3 类：

1. 内部 Flash：
   - AFE 参数
   - SOC 快照
   - 事件日志
   - IAP 更新标志
2. 备份域 BKP：
   - 睡眠/唤醒启动标志
3. 运行期 RAM 默认值：
   - 大量旧参数表面上还保留“EEPROM 写接口”，但实际上已经不再落盘

最重要的结论不是“Flash 有风险”，而是：

- 新迁移到 Flash 的几块存储是有效的；
- 旧 `EEPROM` 参数链路已经大面积失效；
- 上层代码仍然保留了很多“写 EEPROM”的行为，导致系统表现为“本次修改有效，重启丢失”。

## 2. 当前存储架构

### 2.1 内部 Flash

定义位置：

- `103 + 309/Project/Source/Flash.h:4-21`

当前保留页如下：

| 区域 | 地址 |
| --- | --- |
| AFE Slot A | `0x0801C000` |
| AFE Slot B | `0x0801C800` |
| Log Slot A | `0x0801D000` |
| Log Slot B | `0x0801D800` |
| SOC Slot A | `0x0801E000` |
| SOC Slot B | `0x0801E800` |
| IAP Flag | `0x0801F800` |
| Sleep Flag 旧地址定义 | `0x0801FC00` |

当前 Flash 存储实现分两类：

1. 双槽镜像：
   - AFE 参数
2. 双页日志式 journal：
   - SOC 快照
   - 事件日志

实现位置：

- `103 + 309/Project/Source/Flash.c:195-237`
- `103 + 309/Project/Source/Flash.c:260-321`
- `103 + 309/Project/Source/Flash.c:392-547`
- `103 + 309/Project/Source/Flash.c:573-672`

### 2.2 备份域 BKP

当前睡眠启动标志已经不再走 EEPROM，也不再实际走 `FLASH_ADDR_SLEEP_FLAG`，而是走备份寄存器：

- `BKP_DR2`：标志值
- `BKP_DR3`：按位取反校验值

实现位置：

- `103 + 309/Project/Source/SleepDeal.c:677-721`
- `103 + 309/Project/Source/SleepDeal.c:723-770`

### 2.3 旧 EEPROM 兼容层

`EEPROM.c` 当前不是一个真实的 EEPROM 驱动，而是一个兼容壳：

- `ReadEEPROM_Byte()` 固定返回 `0xFF`
- `ReadEEPROM_Word_NoZone()` 固定返回 `0xFFFF`
- `WriteEEPROM_Byte()` 空实现
- `WriteEEPROM_Word_NoZone()` 空实现

关键位置：

- `103 + 309/Project/Source/EEPROM.c:120-143`

这意味着：凡是还在调用 `WriteEEPROM_*` 的代码，现在都不会真正落盘。

## 3. 当前哪些存储“真的在用”

### 3.1 AFE 参数

用途：

- 保存 SH367309 AFE 参数快照
- 上电恢复参数
- 上位机修改后落盘
- 参数非法时恢复默认值

关键路径：

- 读取：`103 + 309/Project/Source/SH367309_DataDeal.c:340-368`
- 写入：`103 + 309/Project/Source/SH367309_DataDeal.c:131-137`
- 上位机修改后保存：`103 + 309/Project/Source/SH367309_DataDeal.c:245-279`
- 恢复默认值并保存：`103 + 309/Project/Source/SH367309_DataDeal.c:319-337`

结论：

- 有用
- 当前实现有效
- 属于应该保留的持久化数据

### 3.2 SOC 快照

用途：

- 掉电后恢复 `SOC`
- 恢复 `DSG_SOC_Int`
- 恢复循环次数 `CycleTimes`

关键路径：

- 定义：`103 + 309/Project/Source/Flash.h:41-46`
- 读写实现：`103 + 309/Project/Source/Flash.c:573-600`
- 上电恢复/运行期保存：`103 + 309/Project/Source/SocEnhance.c:1029-1074`
- 按变化触发保存：`103 + 309/Project/Source/SocEnhance.c:292-308`

结论：

- 有用
- 当前实现有效
- 但保存内容偏少，不是完整 SOC 状态镜像

### 3.3 事件日志

用途：

- 保存最近 100 条事件
- 上电恢复日志指针和内容
- 支持清空

关键路径：

- 写事件：`103 + 309/Project/Source/LogRecord.c:47-63`
- 周期记录：`103 + 309/Project/Source/LogRecord.c:120-153`
- 上电恢复：`103 + 309/Project/Source/LogRecord.c:206-240`
- 清空：`103 + 309/Project/Source/LogRecord.c:191-204`
- Flash 存储实现：`103 + 309/Project/Source/Flash.c:631-672`

结论：

- 有用，但要看现场是否真的读日志
- 当前实现有效
- 也是当前写入压力最大的存储项

### 3.4 睡眠启动标志

用途：

- 区分普通睡眠、深睡、打嗝睡眠恢复路径

关键路径：

- 写入：`103 + 309/Project/Source/SleepDeal.c:686-691`
- 读取校验：`103 + 309/Project/Source/SleepDeal.c:693-716`
- 启动分流：`103 + 309/Project/Source/SleepDeal.c:723-770`
- 休眠前写入：`103 + 309/Project/Source/SleepDeal.c:118-127`
- RTC 睡眠流程写入：`103 + 309/Project/Source/rtc_sleep.c:975-986`

结论：

- 有用
- 当前实现有效
- 放在 BKP 比放在 Flash 更合理

### 3.5 IAP 更新标志

用途：

- 请求重启后跳入 IAP

关键路径：

- 写标志：`103 + 309/Project/Source/Sci_Upper.c:1708-1724`
- 启动检查：`103 + 309/Project/Source/Flash.c:701-706`

结论：

- 有用
- 当前实现有效
- 但当前 `InitAreaSelect()` 没有看到在主启动流程里被调用，需要进一步确认 IAP 项目侧或其他启动入口是否负责执行

## 4. 当前哪些存储“名义存在，实际已失效”

### 4.1 保护参数

包括：

- 电压/电流保护
- 温度保护
- 其他保护参数

现状：

- 默认值仍会装载到 RAM：`103 + 309/Project/Source/EEPROM.c:36-45`
- 但真实 EEPROM 读取已经失效
- 上位机仍在设置“写标志位”：`103 + 309/Project/Source/Sci_Upper.c:1762-1772`
- `App_E2promDeal()` 现在只会直接清空这些标志位：`103 + 309/Project/Source/EEPROM.c:158-161`

结论：

- 运行时参数本身有用
- 持久化已经失效
- 风险高

### 4.2 校准参数 K/B

现状：

- 启动时统一回默认值：`103 + 309/Project/Source/EEPROM.c:47-56`
- 上位机“恢复默认”时仍在调用 `WriteEEPROM_Word_NoZone()`：`103 + 309/Project/Source/Sci_Upper.c:2104-2155`
- 实际不会保存

结论：

- 参数有用
- 持久化失效
- 风险高

### 4.3 OtherElement / 睡眠阈值 / SOC 配置 / 系统参数

这些参数包括：

- 睡眠时间与电压阈值
- SOC 电压表选择
- 标称容量
- 串数、采样电阻等

它们运行时是被大量使用的：

- `103 + 309/Project/Source/SOC.c:86-93`
- `103 + 309/Project/Source/SleepDeal.c:388-483`
- `103 + 309/Project/Source/RTC.c:226`

但启动时来源只是默认值：

- `103 + 309/Project/Source/EEPROM.c:58-69`
- 默认定义：`103 + 309/Project/Source/DataDeal.h:175-189`

结论：

- 参数很有用
- 持久化失效
- 风险高

### 4.4 Heat/Cool 参数

现状：

- 启动默认值：`103 + 309/Project/Source/EEPROM.c:71-80`
- 默认定义：`103 + 309/Project/Source/Heat_Cool.h`
- 上位机仍在置写标志：`103 + 309/Project/Source/Sci_Upper.c:2003`

结论：

- 如果项目启用了热管理，这块参数有用
- 目前持久化失效

### 4.5 系统功能位 `System_OnOFF_Func`

现状：

- 启动读取 EEPROM 的代码已注释：`103 + 309/Project/Source/System_Monitor.c:52-53`
- 修改功能位时仍在写 EEPROM：`103 + 309/Project/Source/Sci_Upper.c:2357-2358`、`2393-2394`
- 重启后仍按固定默认值初始化：`103 + 309/Project/Source/System_Monitor.c:9-70`

结论：

- “功能开关跨重启保存”当前无效
- 风险高

### 4.6 产品信息 Product ID

现状：

- 启动时直接装默认值：`103 + 309/Project/Source/ProductionID.c:24-27`
- 后台处理不做真实落盘：`103 + 309/Project/Source/ProductionID.c:29-53`
- 上位机可修改 RAM 内的字符串，但只会置写标志：`103 + 309/Project/Source/Sci_Upper.c:1750-1790`

结论：

- 若现场需要修改序列号/版本号，这块当前不可靠
- 若只是编译固化显示，风险可接受

### 4.7 故障历史记录

现状：

- 旧 EEPROM 写故障记录代码虽然还在文件里，但整个逻辑被 `#if 0` 包住：`103 + 309/Project/Source/Fault.c:2010-2052`

结论：

- 当前实际未启用
- 不构成当前运行风险
- 但属于明显遗留代码

### 4.8 电流偏置 `curr_offset / OffsetValue_CHG / OffsetValue_DSG`

现状：

- 运行中仍参与电流修正：`103 + 309/Project/Source/DataDeal.c:257-304`
- 但启动时在 `EEPROM_LoadDefaultRuntimeData()` 被直接清零：`103 + 309/Project/Source/EEPROM.c:103-118`
- 没有发现新的 Flash 持久化路径

结论：

- 如果现场依赖偏置校准，这块当前会丢失
- 风险中等

## 5. 风险评级

| 项目 | 当前是否有效 | 是否有用 | 风险等级 | 说明 |
| --- | --- | --- | --- | --- |
| AFE 参数 | 是 | 高 | 中 | 方案正确，但依赖保留 Flash 页不被代码覆盖 |
| SOC 快照 | 是 | 高 | 中 | 可恢复 SOC，但不是完整状态镜像 |
| 事件日志 | 是 | 中 | 中高 | 有用，但写入频率和擦写压力较大 |
| BKP 睡眠标志 | 是 | 高 | 低 | 设计合理，低频写入 |
| IAP 更新标志 | 是 | 高 | 低中 | 功能本身简单，但需确认启动检查路径 |
| 保护参数 | 否 | 高 | 高 | 修改后重启丢失 |
| K/B 校准 | 否 | 高 | 高 | 校准结果重启丢失 |
| OtherElement | 否 | 高 | 高 | 睡眠、SOC、系统关键参数均会丢失 |
| Heat/Cool 参数 | 否 | 中高 | 高 | 若现场调参，重启丢失 |
| 系统功能位 | 否 | 中 | 高 | 功能开关修改不能跨重启保留 |
| Product ID | 否 | 中 | 中 | 现场改写无持久化 |
| 故障历史 | 否 | 低 | 低 | 目前已基本停用 |
| 电流偏置 | 否 | 中 | 中 | 当前看起来只在 RAM 中有效 |

## 6. 对 SOC、日志存储的单独判断

### 6.1 SOC 存储是否有用

有用。

原因：

1. 可以避免每次重启都从纯默认 SOC 开始；
2. 保留 `DSG_SOC_Int` 和 `CycleTimes`，对循环计数连续性有价值；
3. 休眠一段时间后还有 RTC 静置补偿逻辑，SOC 连贯性更好。

不足：

1. 没有保存完整的 `CapFull`、`CapChange`、长期衰减状态；
2. 当前只要三字段变化就写 Flash，写入节流偏弱；
3. 变化粒度和运行节奏相关，长期寿命还需要控制。

### 6.2 SOC 存储是否有风险

有中等风险。

风险点：

1. Flash 擦写寿命；
2. 保存字段不完整，恢复后状态是“近似恢复”；
3. 如果后续应用程序膨胀覆盖保留页，SOC 区会直接失效。

### 6.3 日志存储是否有用

有条件有用。

适用前提：

1. 现场真的会读取日志；
2. 售后需要看启停、故障、加热、冷却等事件历史。

如果现场几乎不查日志，这块收益会低于寿命成本。

### 6.4 日志存储是否有风险

有中高风险。

原因：

1. 每次新事件都会调用 `StorageFlash_SaveLogData()`；
2. 当前不是“追加单条事件”，而是把整个 `100x2` 字节日志结构整体写成一条新 journal 记录；
3. 单页 `2KB`，两页轮换，容纳记录数有限；
4. 若故障状态抖动频繁，日志区擦写会很快。

## 7. Flash 布局风险

文档里已经声明保留页从 `0x0801C000` 起：

- `FLASH_LAYOUT_BUILD_GUARD.md:5-27`

当前 map 文件显示实际程序末尾仍远低于该地址：

- `103 + 309/Project/Users/Listings/CommomSH367309_16series_103RCT6_C.map:4384-4386`

但当前工程配置仍保留了完整 `0x20000` 的 IROM 窗口：

- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:276-280`

且 After Build 只执行了 `fromelf`，未执行 `check_flash_layout.ps1`：

- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:82-91`

结论：

- 现在还没撞上
- 但构建保护没有真正生效
- 后续代码增长会有覆盖存储页的风险

## 8. 建议

### 8.1 优先级 P0

1. 明确哪些参数必须跨重启保存：
   - 保护参数
   - 校准参数
   - `OtherElement`
   - Heat/Cool 参数
   - 系统功能位
   - 电流偏置
2. 对这些参数二选一：
   - 正式迁移到内部 Flash
   - 明确宣布不再持久化，并删除假写入逻辑

### 8.2 优先级 P1

1. 给 SOC 存储增加节流：
   - 最小写入间隔
   - 变化阈值
   - 休眠前/关机前强制保存一次
2. 重构事件日志写法：
   - 从“整块快照重写”改成“单条追加”

### 8.3 优先级 P2

1. 把 `check_flash_layout.ps1` 真正接入 Keil 构建
2. 清理无效旧接口：
   - `EEPROM_ADDR_SLEEP`
   - `EEPROM_ADDR_FLASHUPDATE`
   - 旧故障 EEPROM 区
   - 未使用的 EEPROM 地址定义

## 9. 总结

当前工程不是“没有存储”，而是处于“新旧存储体系混合、迁移只完成一半”的状态：

1. AFE、SOC、日志、BKP 睡眠标志已经迁移并在运行；
2. 保护参数、校准参数、系统参数、产品信息等旧 EEPROM 路径已经失效；
3. 最大现实风险是现场修改后的参数不能跨重启保存；
4. 次级风险是日志/SOC 的 Flash 寿命与构建布局保护不足。

如果后续要继续改造，建议第一步不要再扩展旧 EEPROM 接口，而是直接把“仍必须持久化的参数”统一迁移到内部 Flash。

# 存储现状梳理（用于内部 Flash 替代前决策）

## 1. 目的

这份文档只做三件事：

1. 梳理当前工程里所有真正参与持久化的存储路径。
2. 区分哪些区域还在用，哪些只是定义了但实际上已经废掉或半废掉。
3. 给出裁剪建议，方便后续决定哪些功能直接砍掉，哪些值得保留后再迁移到内部 Flash。

本文基于当前仓库代码状态，不基于旧版本记忆。

## 2. 当前存储介质概览

当前工程实际同时存在 3 套“持久化/跨复位状态”机制：

### 2.1 外部 EEPROM 逻辑层

- 主要接口文件：`103 + 309/Project/Source/EEPROM.c`
- 对外接口：
  - `ReadEEPROM_Byte()`
  - `WriteEEPROM_Byte()`
  - `ReadEEPROM_Word_NoZone()`
  - `WriteEEPROM_Word_NoZone()`
  - `InitE2PROM()`
  - `App_E2promDeal()`
- 物理访问方式：GPIO 模拟 I2C bit-bang
- 相关引脚初始化：
  - `EEPROM.c:587` `InitE2PROM_i2c()`
  - `EEPROM.c:607` `InitE2PROM()`
  - `conf/conf.c:486` 唤醒链路里也会调 `InitE2PROM_i2c()`

### 2.2 内部 Flash 专用标志层

- 主要接口文件：`103 + 309/Project/Source/Flash.c`
- 当前只看到少量专用用途：
  - IAP 升级标志 `FLASH_ADDR_UPDATE_FLAG`
  - 一些旧的休眠/RTC 标志宏仍保留在 `Flash.h`
- 当前 `FlashWriteOneHalfWord()` 的行为是“擦整页 + 写 1 个 halfword”，不适合作为通用存储后端。

### 2.3 BKP 备份寄存器

- 主要接口文件：`103 + 309/Project/Source/SleepDeal.c`
- 当前休眠唤醒标志已经不走 EEPROM，也不走普通 Flash 页，而是走：
  - `BKP_DR2`
  - `BKP_DR3`
- 相关接口：
  - `BootFlag_Write()`
  - `BootFlag_Read()`
  - `BootFlag_Clear()`
  - `IsSleepStartUp()`

结论：

- “存储”现在不是单一机制。
- 后面做内部 Flash 替代时，不能把所有东西硬塞进同一个接口里。
- 至少要区分：
  - 配置类数据
  - 高热度运行数据
  - 启动/唤醒类专项标志

## 3. 当前启动与后台链路

### 3.1 启动链路现状

- `main.c:192` 里 `InitE2PROM();` 已被注释掉。
- 但 `main.c:204` 注释写着 `InitData_SOC(); // 必须放在读完eeprom数据后面`
- 说明当前工程已经出现“依赖 EEPROM 数据，但初始化入口被屏蔽”的不一致状态。

直接影响：

1. 上电参数恢复链路已经断开。
2. 但运行期很多模块仍然假设持久化数据已经加载。

### 3.2 后台写链路现状

- 主循环里仍然会调用 `main.c:149` `App_E2promDeal()`
- `App_E2promDeal()` 负责消费一批脏标志，后台慢慢落盘。
- `DataDeal.c:717` 当这些 EEPROM 脏标志不为 0 时，会跳过一部分 AFE 采样逻辑。

这意味着：

1. 当前 EEPROM 写入不是纯粹独立的后台动作。
2. 它已经影响主循环调度和采样节奏。

## 4. 活跃存储区域总表

下表只列“当前代码里仍然有真实读写路径”的区域。

| 区域 | 地址/范围 | 当前用途 | 读取路径 | 写入路径 | 热度 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| 保护参数 Protect | `0 ~ 128` | 各类 OVP/UVP/OCP/温度/其他保护点 | `EEPROM.c:325~342` | `EEPROM.c:480~521`，SCI 写命令会置脏标志 | 低频 | 建议保留 |
| 校准 K | `154 ~ 247` | 电压/温度/电流校准 K | `EEPROM.c:344~358` | `EEPROM.c:473~479`，部分复位命令直接写 | 低频 | 建议保留 |
| 校准 B | `248 ~ 341` | 电压/温度/电流校准 B | `EEPROM.c:359~370` | 同上 | 低频 | 建议保留 |
| OtherElement1 | `676 ~ 738` | 均衡、开机时间、CS 电阻、休眠阈值、SOC 相关、串数等 | `EEPROM.c:373~390` | `EEPROM.c:522~535`，SCI 写命令置位 | 低频到中频 | 建议保留，但可裁剪子字段 |
| Heat/Cool 参数 | `740 ~ 788` | 加热/冷凝控制参数 | `EEPROM.c:392~409` | `EEPROM.c:563~574`，SCI 写命令置位 | 低频 | 如果项目不要冷热管理，可整体砍 |
| SOC 增强参数 | `790 ~ 794` | `SOC`、`DSG_SOC_Int`、`Cycle_times` | `SocEnhance.c:665~667` | `SocEnhance.c:732~744` | 高频 | 建议保留但单独做热数据区 |
| 产线序列号 | `830 ~ 868` | BMS 序列号 | `ProductionID.c:13~28` | `ProductionID.c:35~43` | 极低频 | 按需求决定 |
| 硬件版本号 | `870 ~ 908` | 硬件版本字符串 | `ProductionID.c:13~28` | `ProductionID.c:46~55` | 极低频 | 按需求决定 |
| 软件版本号 | `910 ~ 948` | 软件版本字符串 | `ProductionID.c:13~28` | `ProductionID.c:57~66` | 极低频 | 按需求决定 |
| 事件记录 | `1000 ~ 1198` | 100 条事件日志 | `LogRecord.c:254~284` | `LogRecord.c:32~49` | 中频到高频 | 看需求，建议独立出来 |
| 事件记录指针 | `1200` | 当前日志写指针 | `LogRecord.c:259` | `LogRecord.c:48` | 中频到高频 | 跟日志同处理 |
| AFE 参数区 | `3000 ~ 3046` | 24 个 AFE ROM 参数 | `SH367309_DataDeal.c:323~341` | `SH367309_DataDeal.c:263~269`，`309~319` | 调试时中频 | 若现场支持调参，建议保留 |
| 初始化完成标志 | `0x3FFC` | 判断是否第一次上电 | `EEPROM.c:683` | `EEPROM.c:722` | 极低频 | 建议保留语义，但不要再保留 EEPROM 地址语义 |
| 系统功能选择 | `2044 ~ 2046` | `System_OnOFF_Func` 持久化 | 读取逻辑已注释掉 | `System_Monitor.c:88~89`，`Sci_Upper.c:2357~2358`，`2393~2394` | 低频 | 先确认是否还要跨重启保留 |

## 5. 定义了但当前基本废掉的区域

这些区域在头文件里还有定义，但从当前代码看，要么没有实际写入，要么相关逻辑已经被注释或替换。

| 区域 | 地址/范围 | 当前状态 | 说明 | 建议 |
| --- | --- | --- | --- | --- |
| RTC 参数区 | `130 ~ 152` | 半废弃 | `EEPROM.c` 里的 RTC 读写分支基本被注释 | 如果 RTC 参数不需要持久化，直接删 |
| SOC 表 | `342 ~ 425` | 基本废弃 | `EEPROM.c` 对应写分支已注释 | 大概率可删 |
| 铜损参数 | `426 ~ 489` | 基本废弃 | 对应写分支已注释 | 大概率可删 |
| 故障记录区 | `490 ~ 675` | 逻辑定义仍在，但持久化写入被 `#if 0` 包住 | `Fault.c:2010~2052` 处于关闭状态 | 如果不上报历史故障，可整块删 |
| `EEPROM_ADDR_SLEEP` | `0x3FFA` | 已废弃 | 休眠标志已改走 BKP | 直接删 |
| `EEPROM_ADDR_FLASHUPDATE` | `0x3FFE` | 已废弃 | IAP 标志已改走 `FLASH_ADDR_UPDATE_FLAG` | 直接删 |
| `EEPROM_ADDR_SWITCH_ONOFF` | `2040` | 未见真实使用 | 只定义，没找到实际读写 | 直接删 |

## 6. 专项状态区，不应与“普通配置存储”混用

### 6.1 IAP 升级标志

- 地址：`FLASH_ADDR_UPDATE_FLAG = 0x0801F800`
- 写入：
  - `Sci_Upper.c:2018`
- 读取：
  - `Flash.c:70`

这是专项启动标志，不应继续挂在 EEPROM 兼容层下面。

### 6.2 休眠启动标志

- 当前实现不在 EEPROM，也不在普通 Flash 页
- 当前走 `SleepDeal.c` 的 BKP 备份寄存器

相关函数：

- `SleepDeal.c:686` `BootFlag_Write()`
- `SleepDeal.c:693` `BootFlag_Read()`
- `SleepDeal.c:723` `IsSleepStartUp()`

这是现在比较干净的一条链，建议不要回退。

## 7. 当前真实写入来源梳理

### 7.1 后台脏标志慢写

由 `App_E2promDeal()` 统一处理的内容：

- 校准 K/B
- Protect
- OtherElement1
- Heat/Cool
- 事件清空流程

特点：

- 写入被拆成多次主循环执行
- 会影响主循环某些业务调度

### 7.2 立即写入

这些地方不是走后台块写，而是直接写当前地址：

- AFE 参数
  - `SH367309_DataDeal.c:263~269`
  - `SH367309_DataDeal.c:318`
- 事件日志
  - `LogRecord.c:47~48`
- Product ID
  - `ProductionID.c:37~63`
- 系统功能位
  - `System_Monitor.c:88~89`
  - `Sci_Upper.c:2357~2358`
  - `Sci_Upper.c:2393~2394`
- SOC 三个量
  - `SocEnhance.c:732~744`
- 某些校准复位命令
  - `Sci_Upper.c:2114~2155`

这类路径如果后面全部直接映射到“整页擦写 Flash”，寿命会很难看。

### 7.3 写入后还要联动硬件的区域

AFE 参数不只是落盘，还会触发实际 AFE ROM 参数刷新：

- `SH367309_DataDeal.c:187~205`

所以 AFE 参数区后面即使裁剪保留，也不能只考虑存储，还要考虑：

1. 上电读取
2. 运行时修改
3. 修改后同步到 AFE 芯片

## 8. 热度分级

### 8.1 高频区

- `SOC` 三个量
- 事件日志指针与日志内容

不建议跟大块配置共用同一个“整包镜像”。

### 8.2 中频区

- AFE 参数
- 某些运行中可调的系统功能位

### 8.3 低频区

- Protect
- 校准参数
- OtherElement1 多数配置
- Heat/Cool
- Product ID
- 初始化标志

## 9. 当前已知问题

### 9.1 启动恢复链已断

- `main.c:192` 注释掉了 `InitE2PROM()`
- 但系统其余逻辑仍然依赖持久化数据已经读入

这是当前最实际的问题，不是后续迁移问题，而是现在就已经有行为风险。

### 9.2 仍在初始化 EEPROM 引脚

- `conf/conf.c:486` 仍会调 `InitE2PROM_i2c()`

如果硬件上已经没有 EEPROM，这块代码后面应该清理掉。

### 9.3 Flash 地址被错误地塞进 EEPROM 接口

`EEPROM.c` 里存在下面两处风险用法：

- `EEPROM.c:673`
- `EEPROM.c:688`

这里把 `FLASH_ADDR_SH367309_VALUE` 传给了 `WriteEEPROM_Word_NoZone(UINT16 addr, ...)` / `ReadEEPROM_Word_NoZone(UINT16 addr)`。

问题：

- `FLASH_ADDR_SH367309_VALUE` 是 32 位绝对地址 `0x0803E000`
- EEPROM 接口参数是 `UINT16`
- 绝对地址会被截断

这条链不能继续保留。

### 9.4 当前 Flash 通用写接口不适合作为 EEPROM 替代

`FlashWriteOneHalfWord()` 当前行为：

1. 解锁 Flash
2. 擦整页
3. 写一个 halfword
4. 上锁

这只能做专项标志位，不能直接替代 EEPROM 的随机更新语义。

## 10. 建议的裁剪优先级

### 10.1 建议优先直接砍掉

这些功能当前代码价值低，且迁移后会增加很多无效复杂度：

1. RTC 参数区
2. SOC 表区
3. 铜损参数区
4. EEPROM 的 `SLEEP` 标志地址
5. EEPROM 的 `FLASHUPDATE` 标志地址
6. `EEPROM_ADDR_SWITCH_ONOFF`

### 10.2 建议按需求决定

1. Product ID 三块字符串区
2. 事件日志区
3. 故障历史区
4. 系统功能选择持久化区

建议判断标准：

- 客户是否真的会读
- 现场是否真的会改
- 掉电后是否真的必须保留

### 10.3 建议优先保留

这些属于核心配置，删了会直接影响系统功能：

1. Protect
2. 校准 K/B
3. OtherElement1 里真正还在用的系统参数
4. AFE 参数区
5. Heat/Cool 参数区
6. 初始化完成标志语义
7. `curr_offset` 或其等效数据

## 11. 针对后续内部 Flash 迁移的建议

不是现在就改代码，只是先给决策方向。

### 11.1 建议分成三类

#### A. 冷配置区

建议放一起做双槽镜像：

- Protect
- 校准 K/B
- OtherElement1
- Heat/Cool
- AFE 参数
- 系统功能位
- Product ID
- 初始化完成标志
- `curr_offset`

#### B. 热数据区

建议单独处理：

- `SOC`
- `DSG_SOC_Int`
- `Cycle_times`
- 事件日志

#### C. 专项启动区

保持独立：

- IAP 升级标志
- 休眠/唤醒标志

### 11.2 不建议的做法

不建议把所有 `WriteEEPROM_Word_NoZone()` 一把替成“擦一页 + 写一页”的 Flash 接口。

原因：

1. 语义不匹配
2. 高频区寿命不够
3. 事件日志/SOC 会把整包配置一起拖着频繁擦写

## 12. 建议你先拍板的决策项

下面这些问题如果先定下来，后面实现会快很多：

1. `SOC` 掉电保持还要不要？
2. 事件日志还要不要？
3. 故障历史还要不要？
4. Product ID 现场可写还要不要？
5. 系统功能位跨重启保持还要不要？
6. Heat/Cool 功能还保不保留？
7. AFE 参数现场改写还要不要？
8. `curr_offset` 还要不要持久化？
9. 旧的 RTC 参数区、SOC 表、铜损参数区是不是可以直接删除？

## 13. 我当前的建议结论

如果目的是“尽快把 EEPROM 去掉，并稳定切到内部 Flash”，我建议先按下面的最小保留集推进：

### 13.1 第一优先级保留

1. Protect
2. 校准 K/B
3. OtherElement1 中仍影响系统运行的字段
4. AFE 参数
5. `curr_offset`
6. 初始化完成标志

### 13.2 第二优先级保留

1. Heat/Cool 参数
2. 系统功能位
3. Product ID

### 13.3 优先考虑砍掉

1. RTC 参数区
2. SOC 表
3. 铜损参数
4. 旧故障记录持久化
5. EEPROM 的 sleep/update 旧标志

### 13.4 单独处理，不跟配置块混写

1. SOC 三元组
2. 事件日志
3. IAP 标志
4. 睡眠唤醒标志


# 主 BMS 配置与升级更新手册

适用：本仓库 `103 + 309/Project` 的 STM32F103CBT6 + SH367309 主工程，Keil 目标 `FD_Release` / `FD_Debug`。核对日期：2026-11-16。不包含独立 RET6 通信工具固件。

本文覆盖 `Project_Config.h` 的全部配置项、产品默认参数入口、升级更新的全部 8 个选择位，以及组合、保留和验收规则。这里的配置示例不是设备已执行记录，也不会修改实际配置。

## 1. 现有文档与配置入口

此前已有分散说明；本手册补齐统一操作入口和逐组范围。

| 内容 | 现有说明 / 拥有实现的源码 |
| --- | --- |
| Flash 布局、CRC、双页与掉电事务 | [flash-storage.md](flash-storage.md)、[Flash.c](../103%20%2B%20309/Project/Source/Flash.c) |
| 全部工程配置开关 | [Project_Config.h](../103%20%2B%20309/Project/Source/conf/Project_Config.h)、[Project_BuildGuard.h](../103%20%2B%20309/Project/Source/conf/Project_BuildGuard.h) |
| 板级 IO、外设、唤醒映射 | [board-configuration.md](board-configuration.md) |
| 开关、HICCUP/NORMAL/DEEP 行为 | [sleep-policy.md](sleep-policy.md) |
| 软件保护、ADC、AFE 与 MOS 仲裁 | [software-protection-and-adc.md](software-protection-and-adc.md) |
| SOC 参数和状态 | [soc.md](soc.md)、[SOC.c](../103%20%2B%20309/Project/Source/SOC.c) |

先区分三类配置：

| 类型 | 修改后怎样生效 | 是否需要更新 MASK |
| --- | --- | --- |
| 编译开关 / 固件常量 | 重新编译并升级，新程序使用新的代码或初始化值 | 不需要 |
| 持久化参数的默认模板 | 已有设备优先加载 Flash，修改模板不会自动覆盖旧值 | 需要选择对应组并使用新代次；也可走现有协议修改单项 |
| 状态和历史数据 | SOC 快照、日志、寿命统计分别保存 | 只有明确的清除位或专用命令能清除相应数据 |

## 2. Project_Config.h 全部配置项

在 Keil 中可右键该文件打开 `Configuration Wizard`，也可直接修改宏。取值以本节和实际代码为准；重新编译、升级后生效。

| 配置项 | 设置 | 实际影响及持久化关系 |
| --- | --- | --- |
| `PROJECT_CFG_BAT_CHEMISTRY` | `0` 三元；`1` 磷酸铁锂 | 在 `conf.h` 选择 `TERNARYLI` / `LIFEPO`，影响相关编译分支、默认模板等。软件保护默认宏还需单独核对 `Fault.h`，不能假定所有门限都随此开关切换。已保存的 AFE、保护和 SOC/Other 参数仍需相应 MASK 才会替换 |
| `PROJECT_CFG_HOST_WRITE_ENABLE` | `0` 禁止；`1` 允许 | 控制 Modbus 写入口及客户协议中的写/控制入口；读入口仍保留。允许写入也仍需通过范围、长度和权限检查。无需 MASK |
| `PROJECT_CFG_WDOG_ENABLE` | `0` / `1` | 控制 `System_Init.c` 的 IWDG 初始化和软件喂狗入口。无需 MASK |
| `PROJECT_CFG_RTC_ENABLE` | `0` / `1` | **当前不是 RTC 总开关**：它在 `conf.h` 定义 `__FUNC_RTC__`，当前消费点是 `Board_Config.c` 的 IWDG 预分频/重装值选择。`Runtime_Boot` 和 RTC 休眠准备仍调用 `Init_RTC()`，不能用 `0` 代表彻底禁用 RTC/STOP。无需 MASK |
| `PROJECT_CFG_SWITCH_MODE` | `0` 保持型；`1` 按键型 | 选择 PA9 开关语义。保持型采用 50 ms 确认；按键型采用 RUN 长按 3 s 请求 NORMAL、NORMAL/DEEP 按住 1 s 唤醒及松手规则，HICCUP 使用既有边沿唤醒策略。完整规则见休眠文档。无需 MASK |
| `PROJECT_CFG_UART1_WAKEUP_ENABLE` | `0` / `1` | 控制相应低功耗准备路径中的 USART1 RX 唤醒配置；不负责启用串口协议或更改波特率。无需 MASK |
| `PROJECT_CFG_RS485_WAKEUP_ENABLE` | `0` / `1` | 控制相应低功耗准备路径中的 USART2/RS485 RX 唤醒配置；不代表所有休眠模式都允许通信唤醒。无需 MASK |
| `PROJECT_CFG_SCI1_ROLE` | `0` 停用；`1` 上位机 | 选择 `_COMMOM_UPPER_SCI1`，影响 USART1 业务初始化与处理。USART2 由 `conf.h` 当前独立定义的 `_COMMOM_UPPER_SCI2` 控制；本开关不同时停用它。无需 MASK |
| `PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` | `0`～`86400` 秒 | 同类事件最短重复保存间隔；`0` 取消这层间隔限制，但事件边沿逻辑仍在。启动、休眠事件不受该限频约束。不清旧日志，也不清寿命统计。无需 MASK |
| `PROJECT_CFG_STORAGE_UPDATE_GENERATION` | `0`～`65534` | 本次持久化更新的全局代次；见第 3 节 |
| `PROJECT_CFG_STORAGE_UPDATE_MASK` | `0`～`255` | 本次更新哪些参数组、清哪些状态；见第 4 节 |

`PROJECT_SWITCH_MAINTAINED=0` 和 `PROJECT_SWITCH_PUSHBUTTON=1` 是上述模式的枚举常量，不是另外两个独立开关。

## 3. 更新代次 GENERATION 的准确规则

```c
#define PROJECT_CFG_STORAGE_UPDATE_GENERATION 0
#define PROJECT_CFG_STORAGE_UPDATE_MASK       0
```

- `0/0`：保留已存参数、状态和日志，不执行本机制的按组更新。正常业务仍会产生新日志、统计和 SOC 保存。
- 有意更新时两个值都必须非零；代次范围 `1`～`65534`，MASK 范围 `1`～`255`。一零一非零会被构建检查拒绝。
- 新 App 启动加载有效配置后，将固件代次与 Flash 保存的**最后一次已应用代次**比较；不同则按 MASK 执行，提交成功后记录新代次。
- **这是一个全局代次，不是每组各一个代次。去重只比较代次，不比较 MASK。** 同一代次改变 MASK、默认值或版本字符串，已执行过该代次的设备仍会跳过更新。
- 代码比较的是“不相等”，不是“数值更大”。设备从代次 2 回刷带代次 1 的固件，也会再次执行代次 1 的 MASK。发布时应按批次管理并使用新的递增代次，不能把回刷旧固件视为保留参数。
- 后续普通升级可以恢复 `0/0`，但这不会抹掉设备保存的最后应用代次。例如设备执行过 2，之后装过 `0/0`，再更新仍应选新代次 3，而不是重复使用 2。
- 同一升级包在同一设备上正常重复启动不会反复覆盖参数或清日志。不同设备各自判断，不能用一块板的执行结果代表整批设备。

`BMS_SOFTWARE_VERDION_DEFAULT` 只参与固件身份信息，不再作为清参数触发器。

## 4. MASK 全部 8 个选择位

MASK 按位或组合，十六进制和十进制写法等价；例如 `0x0F` 等于 `15`，`0x8F` 等于 `143`。

| bit | 十六进制 | 十进制 | 执行内容 | 默认值来源 / 保留边界 |
| --- | --- | --- | --- | --- |
| 0 | `0x01` | 1 | 用新固件的默认值替换 AFE 24 字参数 | `SH367309_DataDeal.h` 的 `AFE_PARAMETERS_RS485_STRUCTION_DEFAULT` 中各项 `defaultValue`；不是读取临时修改的 `curValue` |
| 1 | `0x02` | 2 | 替换全部 65 字软件保护参数 | `Fault.h` 的 `E2P_PROTECT_DEFAULT_PRT`；不是清故障次数、日志或寿命统计 |
| 2 | `0x04` | 4 | 替换 Other 中 5 字 SOC 配置 | `DataDeal.h` 的 `OtherElement_default` 中指定字段；不主动清 SOC 快照 |
| 3 | `0x08` | 8 | 替换 Other 中其余 27 字配置 | 同一默认表；排除上面的 5 字 SOC 配置，并非只替换串数 |
| 4 | `0x10` | 16 | 恢复充、放电电流 K/B 校准 | `SYSKDEFAULT`、`SYSBDEFAULT`，并写入现有校准标记/格式；会替换现场持久化的电流校准 |
| 5 | `0x20` | 32 | 替换 24 字加热/制冷配置 | `Heat_Cool.h` 的 `HeatCoolElement_Default`；更新存储不等于启用加热/制冷业务 |
| 6 | `0x40` | 64 | 清除 SOC 状态快照的有效性及 SOC 状态内容 | 不替换 SOC 配置，不清日志和寿命统计；下次 SOC 初始化按当前启动规则重建，见第 6 节 |
| 7 | `0x80` | 128 | 清空可读的最近 100 条日志 | 之后继续记录新事件；不清故障次数、重启次数和其它寿命统计 |

配置更新的动作是**用编译进新固件的默认模板覆盖选中整组**。它不是保留客户当前值再补几个新字段，也不是随意指定某一个字段的更新清单。

## 5. 每个参数组具体覆盖哪些字段

### 5.1 AFE：bit0，24 字

源码：[SH367309_DataDeal.h](../103%20%2B%20309/Project/Source/SH367309_DataDeal.h)。协议范围 `0x2400`～`0x2417`。

| 地址 | 参数范围 | 字数 |
| --- | --- | ---: |
| `0x2400`～`0x2402` | 单节过压门限、恢复、延时 | 3 |
| `0x2403`～`0x2405` | 单节欠压门限、恢复、延时 | 3 |
| `0x2406`～`0x2409` | 一级/二级充电过流门限及各自延时 | 4 |
| `0x240A`～`0x240D` | 一级/二级放电过流门限及各自延时 | 4 |
| `0x240E`～`0x240F` | 充电高温及恢复 | 2 |
| `0x2410`～`0x2411` | 充电低温及恢复 | 2 |
| `0x2412`～`0x2413` | 放电高温及恢复 | 2 |
| `0x2414`～`0x2415` | 放电低温及恢复 | 2 |
| `0x2416`～`0x2417` | 短路电流、短路延时 | 2 |

新配置通过原有参数应用路径换算为 SH367309 配置并核对；串数、采样电阻和并联数量来自 Other，不在这 24 字中。修改这些硬件适配输入时须同时核对 bit3。

### 5.2 软件保护：bit1，65 字

源码：[Fault.h](../103%20%2B%20309/Project/Source/Fault.h)。协议范围 `0x2100`～`0x2140`。

| 地址 | 字段组前缀 | 内容 |
| --- | --- | --- |
| `0x2100`～`0x2104` | `u16VcellOvp` | 单节过压 |
| `0x2105`～`0x2109` | `u16VcellUvp` | 单节欠压 |
| `0x210A`～`0x210E` | `u16VbusOvp` | 总压过压 |
| `0x210F`～`0x2113` | `u16VbusUvp` | 总压欠压 |
| `0x2114`～`0x2118` | `u16IchgOcp` | 充电过流 |
| `0x2119`～`0x211D` | `u16IdsgOcp` | 放电过流 |
| `0x211E`～`0x2122` | `u16TChgOTp` | 充电高温 |
| `0x2123`～`0x2127` | `u16TchgUTp` | 充电低温 |
| `0x2128`～`0x212C` | `u16TdischgOTp` | 放电高温 |
| `0x212D`～`0x2131` | `u16TdischgUTp` | 放电低温 |
| `0x2132`～`0x2136` | `u16TmosOTp` | MOS 高温 |
| `0x2137`～`0x213B` | `u16VdeltaOvp` | 压差过大 |
| `0x213C`～`0x2140` | `u16SocUp` | 历史低 SOC 参数组 |

每组均为 `_First`、`_Second`、`_Third`、`_Rcv`、`_Filter` 五字。更新覆盖所有已保存字段；存在参数不代表当前固件启用了每一级对应保护。例如旧低 SOC 关断分支已经删除，恢复 `u16SocUp` 的默认值不会重新启用它。

AFE 与软件保护是两个独立参数组。只改 `Fault.h` 不会改 AFE 硬件保护默认表；同时更新两套应选 `0x03`。

### 5.3 SOC 配置：bit2，5 字

| 地址 | 字段 | 含义 / 影响 |
| --- | --- | --- |
| `0x230C` | `u16Soc_TableSelect` | SOC 曲线选择保留字段；当前 `SOC.c` 不通过它执行 OCV 查表初始化 |
| `0x2318` | `u16Soc_Ah` | 额定容量，单位 `0.1 Ah`；默认来源 `BMS_CAPCITY`，例如 `245` 表示 `24.5 Ah` |
| `0x2319` | `u16Soc_Cycle_times` | 配置中的循环次数初值；有效快照仍可恢复其累计循环，不能把 bit2 当成强制重置循环命令 |
| `0x231A` | `u16Soc_V_100` | SOC 满端电压参数，单位 mV |
| `0x231B` | `u16Soc_V_0` | SOC 低端电压参数，单位 mV |

没有选择 bit6 时保留有效 SOC 快照；容量改变后按当前实现恢复原比例，不能把“更新额定容量”理解成“自动充满”。要更新 SOC 配置并重建状态，组合为 `0x44`。精确设置 SOC 百分比或循环次数，使用已有对应协议写入流程。

### 5.4 Other：bit3，其余 27 字

以下是 `OTHER_ELEMENT` 中排除上述 5 个 SOC 字后的完整范围，默认值来自 [DataDeal.h](../103%20%2B%20309/Project/Source/DataDeal.h) 的 `OtherElement_default`。

| 地址 | 字段 | 内容 |
| --- | --- | --- |
| `0x2300`～`0x2302` | `u16Balance_OpenVoltage`、`u16Balance_OpenWindow`、`u16Balance_CloseWindow` | 均衡配置字段 |
| `0x2303`～`0x2307` | `u16Balance_Res1`～`u16Balance_Res5` | 5 字保留项，同样随整组恢复 |
| `0x2308`～`0x2309` | `u16CS_Cur_CHGmax`、`u16CS_Cur_DSGmax` | 采样电流范围配置字段 |
| `0x230A`～`0x230B` | `u16CBC_DelayT`、`u16CBC_Cur_DSG` | Other 中的短路配置字段；与 AFE 表里的同名字段分属两组 |
| `0x230D` | `u16Password_Always` | 保留的密码字段 |
| `0x230E`～`0x230F` | `u16CurLimit_Vdelta`、`u16CurLimit_Cur` | 限流配置字段 |
| `0x2310` | `u16Sleep_VNormal` | 保留字段，当前不参与 NORMAL 休眠条件 |
| `0x2311` | `u16Sleep_TimeNormal` | 长期空闲进入 DEEP 的分钟数，当前默认表为 10080 分钟（7 天） |
| `0x2312`～`0x2313` | `u16Sleep_Vlow`、`u16Sleep_TimeVlow` | 参数低压休眠的 mV 门限、分钟延时 |
| `0x2314`～`0x2315` | `u16Sleep_VirCur_Chg`、`u16Sleep_VirCur_Dsg` | 保留的休眠电流字段 |
| `0x2316`～`0x2317` | `u16Sleep_RTC_WakeUpTime`、`u16Sleep_TimeRTC` | 保留字段，当前不控制 RTC 周期检查间隔 |
| `0x231C` | `u16Sys_SeriesNum` | 电池串数；默认来源 `SNum`，当前主 AFE 路径支持 5～16 串 |
| `0x231D`～`0x231E` | `u16Sys_CS_Res`、`u16Sys_CS_Res_Num` | 采样电阻及并联数量；默认来源 `CS_Res` / `CS_Res_Num` |
| `0x231F` | `u16Sys_PreChg_Time` | 预充时间配置字段 |

这里列的是**保存和更新范围**，不表示每个命名功能当前都有运行业务。均衡、限流、预充等部分字段当前只有存储/读写或保留用途；不能通过写入这些字段推断对应控制已经启用。AFE 短路实际换算读取的是 AFE 参数表的 `curValue`。

只想通过升级改变串数，bit3 也会覆盖上述其它 26 个字段。若需要保留客户所有其它 Other 设置而只改一项，应走现有单项寄存器写入；当前升级 MASK 不提供字段级选择。

### 5.5 电流校准：bit4

持久化内容为充电 K、充电 B、放电 K、放电 B，加现有标记/格式字。恢复来源为 `SYSKDEFAULT`（当前为 1024，即 Q10 的 1 倍）和 `SYSBDEFAULT`（当前为 0），会覆盖现场标定的两路校准。

它不清波特率、编号或寿命统计，也不表示清除 MCU 本次启动的 RAM 零点校准状态。其它电压/温度 K/B 当前不持久化，启动时由 `Storage_ApplyConfig()` 使用固件默认 K/B 初始化，不能用 bit4 管理它们的历史版本。

### 5.6 加热/制冷：bit5，24 字

完整结构是 [Heat_Cool.h](../103%20%2B%20309/Project/Source/Heat_Cool.h) 的 `HEAT_COOL_ELEMENT`：`u16Heat_OpenTemp`、`u16Heat_CloseTemp`、`u16Heat_OpenCur`、`u16Cool_OpenTemp`、`u16Cool_CloseTemp`，以及 `u16Heat_Res3`～`u16Heat_Res10`、`u16Cool_Res1`～`u16Cool_Res11` 的 19 字保留项。

温度沿用 `(摄氏度 + 40) × 10`，电流沿用 `A × 10`。`HEAT_OPEN_CURR` 参与默认模板。当前两个有效 Keil 目标没有加入 `Heat_Cool.c`，恢复这组参数不会启用其控制代码。

## 6. 清状态、日志与始终保留的数据

| 操作 | 清除后会怎样 | 仍然保留 |
| --- | --- | --- |
| bit6：清 SOC 快照 | 剩余容量、累计循环/余量、未完成校准等 SOC 状态重新初始化；当前 `InitData_SOC()` 在没有有效快照时以 60% 和配置循环初值开始，不是强制 SOC=0 或 100% | SOC 配置、日志、电流 K/B、寿命统计、通信字段 |
| bit7：清最近 100 条日志 | 旧历史不再作为可读日志恢复，新启动和故障事件重新记录；仍按原容量循环保存最近 100 条 | 参数、SOC 快照、寿命统计、重启次数、通信字段 |
| `0xFF`：选择全部 8 位 | 更新全部 6 组参数，并清 SOC 快照和日志 | **仍不清寿命统计、持久化波特率、11 字节电池编号等独立通信字段** |

日志清空是一次升级事务，不是关闭日志记录。升级启动后很快出现新的启动日志或当前故障日志是正常现象；正常启动计数仍按原规则变化，不能把“保留计数”理解成升级重启后数值绝对不变。

清状态/日志前先持久化待完成意图，完成后才清标记；中途掉电后启动会继续已提交事务。MASK 不是损坏存储或未知格式的强制格式化开关，合法性与提交检查仍然有效，详见 Flash 持久化说明。

## 7. 常用 MASK 组合

每次有意更新都要配一个新的非零 GENERATION。表中描述均为整组更新。

| 目的 | 十六进制 | 十进制 |
| --- | --- | ---: |
| 保留已有存储（GENERATION 也必须为 0） | `0x00` | 0 |
| 只更新 AFE 参数 | `0x01` | 1 |
| 同时更新 AFE 和软件保护 | `0x03` | 3 |
| 只更新 SOC 配置，不清状态 | `0x04` | 4 |
| 只更新 Other，包括串数、采样电阻、休眠参数 | `0x08` | 8 |
| 更新软件保护 + Other，例如同步串数与总压保护默认值 | `0x0A` | 10 |
| 更新 AFE + 软件保护 + Other，例如同步电流档位相关默认值 | `0x0B` | 11 |
| 更新 AFE + 软件保护 + SOC 配置 + Other | `0x0F` | 15 |
| 恢复全部 6 组参数，包含电流校准，不清快照和日志 | `0x3F` | 63 |
| 只重建 SOC 状态 | `0x40` | 64 |
| 更新 SOC 配置并重建 SOC 状态 | `0x44` | 68 |
| 只清日志 | `0x80` | 128 |
| 更新前 4 组参数，同时清日志，保留 SOC 快照和电流校准 | `0x8F` | 143 |
| 只清 SOC 状态与日志，参数不恢复默认 | `0xC0` | 192 |
| 全部选择，仍保留第 6 节所列独立数据 | `0xFF` | 255 |

例如设备已执行代次 1、MASK 为 15，现在要保持同样参数更新范围并清日志：

```c
#define PROJECT_CFG_STORAGE_UPDATE_GENERATION 2
#define PROJECT_CFG_STORAGE_UPDATE_MASK       0x8F
```

这会再次恢复前 4 组参数。若前 4 组已经更新完、这次只需要清日志，应使用新代次配 `0x80`，避免再次覆盖客户后续设置。

## 8. 常用产品配置修改到哪里

| 修改项 | 源码入口 | 已有设备升级的选择 |
| --- | --- | --- |
| 串数 `SNum` | `DataDeal.h`，进入 `OtherElement_default`，也影响 `Fault.h` 中按串数计算的默认总压门限 | 至少 bit3；要同步软件总压保护默认值用 `0x0A`，并核对实际接线 |
| 容量 `BMS_CAPCITY` | `DataDeal.h`，进入 `u16Soc_Ah` | bit2；默认保留有效状态比例，明确需要重建时才加 bit6 |
| 电流档位 `LEVEL_CURR` | `conf/conf.h`，选 `CURR_10A/15A/20A/30A/40A` | 检查该档派生的 `CS_Res`、`CS_Res_Num`、`CBC_*`、`AFE_OCC*`、`AFE_ODC*` 以及软件保护模板；覆盖三组通常为 `0x0B`。档名不能代替实际 BOM 和量程确认 |
| AFE 门限/延时默认值 | `SH367309_DataDeal.h`，部分电流默认宏来自 `conf.h` | bit0；修改 `defaultValue` 使用的宏，而不只是运行中的 `curValue` |
| 软件保护默认值 | `Fault.h` 的 `E2P_PROTECT_DEFAULT_PRT` 及其引用宏 | bit1；不自动更改 AFE 那一组 |
| 休眠、采样电阻及 Other 其它默认值 | `DataDeal.h` 的 `OtherElement_default`，部分输入来自 `conf.h` | bit3；注意整组覆盖 |
| 电流 K/B 默认值 | `DataDeal.h` 的 `SYSKDEFAULT` / `SYSBDEFAULT` | 已有电流校准须 bit4；其它非持久化 K/B 每次启动使用固件默认值 |
| 加热/制冷默认值 | `Heat_Cool.h` 的 `HeatCoolElement_Default` / `HEAT_OPEN_CURR` | bit5；仍受当前目标是否编入控制代码的限制 |
| `sys_time.time_enter_rtc` | `conf/conf.c` 的 `Time_T sys_time` 初始化 | 直接随新 App 复位初始化生效，不需要 MASK；它是连续符合条件后进入 HICCUP 的秒数，`uint8_t`，不是 7 天 DEEP 参数 |
| RTC 周期检查间隔 | `RTC.c` 的 `RTC_WAKEUP_DEFAULT_SECONDS` | 固件常量，当前不使用 Other 的 `0x2316/0x2317` 控制，不需要 MASK |
| 同类日志限频 | `PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` | 直接生效；清旧日志是另一个动作，须 bit7 |
| 硬件版本、软件版本、BMS 序列号默认字符串 | `DataDeal.h` 的 `BMS_HARDWARE_VERDION_DEFAULT`、`BMS_SOFTWARE_VERDION_DEFAULT`、`BMS_SERIAL_NUMBER_DEFAULT` | `ProductionID.c` 每次启动从固件初始化身份信息，无需 MASK；与另行持久化的 11 字节电池编号区分 |
| GPIO、UART、CAN、ADC、IWDG 板级映射 | `conf/Board_Config.h` / `.c` | 固件配置，见板级文档；MASK 不负责迁移引脚、协议或 Flash 地址 |
| 虚拟电流调试 | `conf.h` 的 `__VIRTURE_CURRENT__` 和 RAM 中 `sys_time.isdebugenable`、`CHG`、`DSG` | 编入该宏且运行使能为 1 时以虚拟 A×10 电流替代上报、SOC 和软件电流保护的输入；这是调试入口，不是持久化更新选项 |

`time_enter_rtc=60` 是连续 60 个满足条件的秒检查，不是开机 60 秒。充放电、有效外部通信、忙状态、按键、AFE/保护等阻塞会影响进入条件；完整条件和计时复位位置在 `rtc_sleep.c`。MASK 不会修改这些 RAM 计时逻辑。

## 9. 实际升级操作

1. 确认板型、串数、化学体系和本次要覆盖的组，在对应默认模板中修改值。选择当前设备未应用过的新代次；保留本次发布的 GENERATION、MASK 和默认值记录。
2. 核对组合是否会恢复其它现场参数，特别是 bit3 的其它 Other 字段、bit4 的现场电流校准、bit6 的 SOC 状态和 bit7 的日志。MASK 不能绕过参数范围和已有存储合法性检查。
3. 涉及默认值/更新策略时，先运行相关存储回归及默认无镜像编译：

   ```powershell
   py -B tools/test_storage.py
   powershell -ExecutionPolicy Bypass -File .\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release
   ```

4. 确实需要升级文件时，显式生成 Release 镜像：

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release -GenerateImage
   ```

5. 使用现有升级工具选择本次新生成的 `103 + 309/Project/Users/Objects/FD_Release.bin`。主 App 起点为 `0x08004800`，最大长度 `0x17800`，结束地址不得超过 `0x0801C000`；保留 IAP 和持久化页。ST-Link 写入使用已有安全脚本，保持 dry-run 和地址检查。
6. 更新提交在新 App 启动时执行，下载完成不等于已经应用参数。程序完成启动后，回读选中组。例如串数读 `0x231C`、容量读 `0x2318`、AFE 读 `0x2400` 参数区；清日志时确认旧历史消失并允许出现新的启动记录。
7. 再正常重启一次，确认同代次不重复恢复参数/清日志。需要验证掉电恢复时另做实板试验，host 半字中断测试不代替板上电压、MOS 动作和真实断电验证。

升级过程中先完成以前已提交的待完成事务，再处理本次代次；当前实现不按软件版本字符串全盘清空，也没有一个 MASK 位可以清掉全部寿命统计。

## 10. 文档核对与维护

升级范围由 `Flash.c` 的 `enum STORAGE_PARAM_GROUP`、`Storage_IsSocParameter()`、`Storage_UpdateModules()` 和 `Storage_CommitParameterUpdate()` 定义；范围或组顺序变化时同步本手册与 `flash-storage.md`，不能只改表格。

验证入口：`tools/test_storage.py` 覆盖所有按组选择、更新提交/掉电恢复、状态、日志、统计与采样质量；按本次实际行为影响补充 SOC、保护或休眠回归。仅改文档时核对源码映射、掩码组合、字段数量和本地链接，无需生成固件或连接设备。

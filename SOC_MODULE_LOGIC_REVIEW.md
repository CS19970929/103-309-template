# SOC 模块逻辑梳理

## 1. 文档范围

本文基于当前仓库可见代码，对 `SOC` 相关模块做一次面向维护和重构的梳理。

本次梳理覆盖以下内容：

- 模块边界与文件职责
- 启动初始化流程
- 运行态周期流程
- 低功耗/RTC 唤醒后的静置补偿
- 对外通信与参数刷新入口
- Flash 持久化策略
- 对故障、显示、CAN 的影响范围
- 当前实现边界、缺口与风险点

说明：

- 本文结论基于当前仓库代码，不假设板外工具链或未纳入仓库的补充逻辑。
- 文中“内部 SOC”指 `SOC_Calculate_Element.u8SOC_Now`。
- 文中“显示 SOC”指 `SOC_Enhance_Element.u8_SOC`，最终通过 `g_stCellInfoReport.SocElement.u16Soc` 对外输出。

## 2. 模块边界

### 2.1 文件职责

#### `103 + 309/Project/Source/SOC.c`

职责是外层桥接，不承担主要算法：

- 从 `g_stCellInfoReport` 采集输入
- 将 `OtherElement` 中的 SOC 参数搬运到 `SOC_Enhance_Element`
- 调用 `SOC_IntEnhance_Ctrl()`
- 将计算结果回填到 `g_stCellInfoReport.SocElement`

核心函数：

- `InitData_SOC()`
- `App_SOC()`
- `RefreshData_SOC()`
- `GetData_SOC()`

#### `103 + 309/Project/Source/SocEnhance.c`

这是 SOC 主算法文件，承担：

- 运行时状态机
- 安时积分
- 端点校准
- 静置 OCV 纠偏
- 弱单体约束
- 显示值平滑
- 外部刷新命令处理
- SOC 快照持久化

#### `103 + 309/Project/Source/rtc_sleep.c`

负责低功耗 RTC 唤醒后的静置补偿入口：

- 统计休眠时长
- 唤醒后读取最新单体电压
- 调用 `SOC_ApplyRtcRelaxationCompensation()`

#### `103 + 309/Project/Source/Flash.c`

负责 SOC 掉电快照存储：

- `StorageFlash_LoadSocData()`
- `StorageFlash_SaveSocData()`

当前使用两页 Flash journal 记录，不是旧式 EEPROM 单地址覆盖。

## 3. 核心数据结构

### 3.1 外部桥接结构：`SOC_Enhance_Element`

定义于 `SocEnhance.h`，作用是“外部世界与 SOC 内核之间的交换层”。

主要字段分四类：

#### 1. 配置输入

- `u16_SOC_Ah`
- `u16_SOC_CycleT_Ever`
- `u16_SOC_CycleT_Limit`
- `u16_SOC_TableSelect`
- `u16_SOC_DsgVcell_Limit`
- `u16_SOC_0_Vol`
- `u16_SOC_100_Vol`
- `SOC_Table_CanSet[]`

#### 2. 实时测量输入

- `u16_VCellMax`
- `u16_VCellMin`
- `u16_Ichg`
- `u16_Idsg`

#### 3. 对外输出

- `u8_SOC`
- `u8_SOH`
- `u16_CapacityNow`
- `u16_CapacityFull`
- `u16_CapacityFactory`
- `u16_Cycle_times`

#### 4. 外部控制/刷新标志

- `u8_SetSocOnce`
- `u16_RefreshData_Flag`
- `u16_SOC_InitOver`

### 3.2 内部真实计算结构：`SOC_Calculate_Element`

这是 SOC 算法真正依赖的内部状态，关键字段包括：

- `u8SOC_Now`
- `u32CapNow`
- `u32CapFactory`
- `u32CapFull`
- `u32CapChange`
- `u8DSG_SOC_Int`
- `u32Cycle_times`

其中：

- `u8SOC_Now` 是内部真实 SOC
- `u32CapNow` 是内部剩余容量基准
- `u32CapChange` 是积分残差累积量
- `u8DSG_SOC_Int` 用于循环次数累计

### 3.3 运行态上下文：`g_soc_runtime`

这个结构是近阶段新增的“显示层和静置层运行态”：

- `u8DisplaySoc`
- `u8DisplayReady`
- `u8RestBucketApplied`
- `u32RestTicks`

职责不是做容量计算，而是：

- 维护显示百分比
- 维护静置时长累计
- 控制静置补偿分桶只执行一次

## 4. 参数来源与配置层

### 4.1 `OtherElement` 是 SOC 参数主入口

`InitData_SOC()` 会把以下参数从 `OtherElement` 拷贝到 SOC 模块：

- `u16Soc_Ah`
- `u16Soc_Cycle_times`
- `u16Soc_TableSelect`
- `u16Soc_V_100`
- `u16Soc_V_0`

因此，当前 SOC 算法参数边界是：

- 运行参数来自 `OtherElement`
- 表格参数来自 `SOC_Table_Set`
- 实时测量来自 `g_stCellInfoReport`

### 4.2 SOC 表来源

当前仓库里的 SOC 表分为两类：

#### 固化默认表

- `SOC_Table_Default`
- `SOC_Table_LiFePO`
- `SocTable_TernaryLi`
- `SocTable_LiFePO2`

#### 运行态可改表

- `SOC_Table_Set`

`SOC_Table_Set` 在启动时默认由 `EEPROM_LoadDefaultSocTable()` 装载为 `SOC_Table_Default`，之后可通过通信接口改写。

### 4.3 OCV 不是离散查点，而是线性插值

OCV 查表统一走 `GetEndValue()`。

其行为不是“找最近点”，而是：

1. 找到当前电压落在哪两个拐点之间
2. 对这两点做线性插值
3. 若超表范围，则取表头或表尾对应值

因此当前 OCV 输出是连续百分比近似，不是纯台阶值。

## 5. 启动初始化流程

### 5.1 上电主链路

主流程位于 `main.c`：

```text
InitDevice()
  -> InitE2PROM()
  -> InitAFE1()
  -> InitCan()
  -> InitADC()
  -> InitSci()
  -> InitMosRelay_DOx()
  -> InitData_SOC()
  -> InitTimer()
```

其中 `InitData_SOC()` 明确被放在“读完参数之后”执行。

### 5.2 `InitE2PROM()` 当前实际行为

当前仓库里的 `EEPROM.c` 已经不是传统 EEPROM 读写实现，而更像兼容壳。

`InitE2PROM()` 的动作是：

```text
EEPROM_LoadDefaultRuntimeData()
  -> 默认装载 Protect / Calib / OtherElement / HeatCool / SOC_Table / CopperLoss

ReadEEPROM_AFE_Parameters()
  -> 从 Flash 恢复 AFE 参数区

ReadEEPROM_EventRecord_Parameters()
  -> 读取日志相关参数
```

这意味着至少从当前代码看：

- `OtherElement` 启动时先被装成默认值
- `SOC_Table_Set` 启动时先被装成默认表
- SOC 运行快照单独从 `Flash` 的 SOC 区恢复

### 5.3 `InitData_SOC()` 做了什么

`InitData_SOC()` 的职责可以拆成三步：

#### 1. 参数搬运

把 `OtherElement` 与 `SOC_Table_Set` 中的值复制到 `SOC_Enhance_Element`。

#### 2. 调用 `soc_param_lib_init()`

这是 SOC 内核真正初始化入口。

#### 3. 调用 `GetData_SOC()`

将初始结果回填给 `g_stCellInfoReport.SocElement`。

### 5.4 `soc_param_lib_init()` 做了什么

这个函数是当前 SOC 启动逻辑的核心。

动作如下：

1. 用 `u16_SOC_Ah` 初始化 `u32CapFactory`
2. 用 `u16_SOC_CycleT_Ever` 初始化 `u32Cycle_times`
3. 清零积分状态
4. 清零 `u8SOC_Now / u32CapNow / u8DSG_SOC_Int / u32CapFull`
5. 调用 `SOC_DealEEPROM_Data(EEPROM_DATA_READ)` 读取 Flash 中的 SOC 快照
6. 设置 `u16_SOC_InitOver = 1`
7. 清空 `g_soc_runtime`
8. 强制让显示值跟随当前真实值

### 5.5 启动恢复策略

`SOC_DealEEPROM_Data(EEPROM_DATA_READ)` 当前恢复逻辑是：

#### 若 Flash 快照有效

恢复：

- `u8SOC_Now`
- `u8DSG_SOC_Int`
- `u32Cycle_times`

然后：

- `u32CapFull = u32CapFactory`
- `u32CapNow = SOC% * u32CapFactory`

#### 若 Flash 快照无效

走默认值：

- `SOC = 60`
- `DSG_SOC_Int = 0`
- `Cycle_times = OtherElement.u16Soc_Cycle_times * 100`
- `CapFull = CapFactory`
- `CapNow = 60% * CapFactory`

然后立即写回 Flash。

结论：

- 当前启动恢复优先级是“历史快照优先”
- 不会在启动时直接用当前单体电压重新估算 SOC
- 无历史快照时默认从 `60%` 启动

## 6. 运行态主流程

### 6.1 调度周期

`App_SOC()` 只有在 `gu8_200msAccClock_Flag != 0` 时才执行。

因此 SOC 主循环基础调度周期是 `200ms`。

### 6.2 `App_SOC()` 执行顺序

```text
App_SOC()
  -> RefreshData_SOC()
  -> GetData_SOC()
  -> SOC_IntEnhance_Ctrl()
  -> 清 200ms 标志
  -> 若已初始化完成，清启动标志 b1StartUpFlag_SOC
```

这里有一个值得注意的点：

- `GetData_SOC()` 在 `SOC_IntEnhance_Ctrl()` 之前先执行一次
- 真正更新后的结果，需要等待 `SOC_Result_Pass()` 与下一次桥接过程再完全对外体现

### 6.3 `SOC_IntEnhance_Ctrl()` 是真实运行主线

当前顺序如下：

```text
SOC_IntEnhance_Ctrl()
  -> 根据状态机做 State Transfer / CHG 积分 / DSG 积分
  -> soc_cali()
  -> SOC_UpdateRestMonitor()
  -> SOC_ApplyWeakCellGuard()
  -> SOC_EEPROM_Deal_Monitor()
  -> SOC_RefreshData_Monitor()
  -> SOC_Result_Pass()
```

这条顺序很重要，因为它定义了优先级：

1. 主状态机先执行
2. 端点硬校正随后执行
3. 静置补偿和弱单体约束再覆盖
4. 再决定是否持久化
5. 最后才平滑显示并同步对外值

## 7. 状态机逻辑

### 7.1 状态定义

SOC 运行态状态机只有三种有效态：

- `SOC_CALI_STATE_TRANSFER`
- `SOC_CALI_CONT_CHG`
- `SOC_CALI_CONT_DSG`

### 7.2 状态切换策略

`SOC_State_Transfer()` 的判定逻辑：

- 连续 3 次检测到 `Ichg >= 0.2A`，进入 `CONT_CHG`
- 连续 3 次检测到 `Idsg >= 0.2A`，进入 `CONT_DSG`
- 否则留在 `TRANSFER`

因为外层调度周期是 `200ms`，所以：

- 连续 3 次意味着大约 `600ms` 的稳态确认

### 7.3 充放电有效电流阈值

当前阈值：

- `SOC_VIRTUAL_CURRENT_CHG = 2`
- `SOC_VIRTUAL_CURRENT_DSG = 2`

单位是 `A*10`，即约 `0.2A`。

低于该阈值：

- 不认为进入有效积分状态
- 会被视为趋向静置

## 8. 安时积分逻辑

### 8.1 充电积分

`SOC_Cont_AH_Int_CHG()` 的行为：

1. 若充电电流持续有效，每累计 5 个 `200ms` 周期，置位一次积分执行标志
2. 即大约每 `1s` 真正做一次 SOC 充电积分
3. 积分前先做 `Correction_Terminal(CurCHG)`
4. 再用电流累计到 `u32CapChange` 和 `u32CapNow`
5. 把变化量折算成百分比，叠加到 `u8SOC_Now`

等价理解：

- 外部调度是 `200ms`
- 积分主节拍约为 `1s`

### 8.2 放电积分

`SOC_Cont_AH_Int_DSG()` 与充电类似：

1. 放电电流持续有效时，每 `1s` 做一次积分
2. 积分前先做 `Correction_Terminal(CurDSG)`
3. 从 `u32CapNow` 中减去累计容量
4. 按变化百分比从 `u8SOC_Now` 中扣减

### 8.3 循环次数统计

放电积分中会累积 `u8DSG_SOC_Int`：

- 每次放电按本次折算出的 `C_change_per` 增加
- 达到 `80` 后清零
- `u32Cycle_times += 100`

注意：

- 注释里写的是“90% 算一个循环”
- 代码实际阈值是 `80`

这是当前实现与注释不一致的一个点。

## 9. 末端校准逻辑

### 9.1 末端校准入口

所有端点校准统一走：

- `Correction_Terminal(CurCHG)`
- `Correction_Terminal(CurDSG)`

当前大电流版本 `CorrectionTerminal_CC()` 为空，实际生效的是 `CorrectionTerminal_CV()`。

### 9.2 充电末端校准

充电端主要依据 `VCellMax` 相对 `SOC_100_Vol` 的关系，把内部 SOC 往 100% 拉：

- 接近满电阈值前先拉向 95%
- 达到满电阈值后分速度继续拉高
- 超过 `SOC_100_Vol + 50mV` 后可更激进上拉

目标是：

- 避免“明明到端压了，SOC 还卡在 90% 多”

### 9.3 放电末端校准

放电端主要依据 `VCellMin` 相对 `SOC_0_Vol` 的关系，把内部 SOC 往 0% 拉：

- 接近 `SOC_0_Vol + 100mV` 开始慢速下拉
- 到达 `SOC_0_Vol` 后加快下拉
- 低于 `SOC_0_Vol - 50mV` 时进一步激进

目标是：

- 避免“低压保护都快到了，SOC 还剩很多”

## 10. 端点硬钳位逻辑

`soc_cali()` 在状态机之外额外做一层硬钳位：

### 10.1 充电方向

若满足：

- `isCHG()`
- `VCellMax >= SOC_100_Vol`
- `VCellMin >= Totle_soc100`

则直接 `SOC_ApplySocNow(100)`。

### 10.2 放电方向

若满足：

- 非充电
- `VCellMin <= SOC_0_Vol`
- `VCellMin >= 2000`

并持续一定时间后，直接 `SOC_ApplySocNow(0)`。

这相当于在“积分 + 末端校准”外，额外增加一层极限硬修正。

## 11. 静置补偿逻辑

### 11.1 静置累计

`SOC_UpdateRestMonitor()` 在每次运行态周期中检查：

- 若正在充电或放电，清零静置计时
- 若无电流且 `VCellMin >= 2000`，则累计 `u32RestTicks`

代码按：

- `1 tick = 5s`

做换算。

### 11.2 静置分桶

静置时长被分成四档：

- `10min`
- `30min`
- `1h`
- `6h`

`SOC_GetRestBucket()` 返回 0~4 档。

### 11.3 静置补偿策略

`SOC_ApplyRestCompensation()` 的逻辑是：

1. 仅在静置状态下允许执行
2. 用 `Get_OpenCircuit_Value()` 算出 OCV 目标 SOC
3. 与当前内部 SOC 做差值
4. 仅在静置分桶触发时执行一次下调修正

调整规则：

- 无充电时不允许把 SOC 往上修正，只允许持平或下调
- 连续静置 `10min / 30min / 1h / 6h` 各触发一次
- 每次最多只允许下调 `1%`

这套逻辑体现的是：

- 不希望静置补偿一下子大跳
- 长时间静置只增加“触发机会”，不增加单次跳变幅度

## 12. RTC 唤醒后的静置补偿

### 12.1 入口

低功耗 RTC 唤醒后，`rtc_sleep.c` 中的 `update_rtc_soc()` 会：

1. 按睡眠次数与 RTC 周期换算总静置秒数
2. 读取当前 `VCellMin/VCellMax`
3. 调用 `SOC_ApplyRtcRelaxationCompensation()`

### 12.2 `SOC_ApplyRtcRelaxationCompensation()` 做了什么

它并没有新写一套算法，而是直接复用运行态的两段逻辑：

- `SOC_ApplyRestCompensation(rest_seconds)`
- `SOC_ApplyWeakCellGuard()`

然后：

- `SOC_PersistSnapshotIfChanged()`
- `SOC_SyncOutputData(1U)`
- 把结果直接写回 `g_stCellInfoReport.SocElement`

结论：

- RTC 唤醒补偿是“复用静置补偿框架”
- 不是单独的第二套 SOC 算法

## 13. 弱单体约束

### 13.1 目的

该逻辑用于解决这样一种体验问题：

- 包整体看似还有一定 SOC
- 但最弱单体已经逼近欠压端点
- 最终用户感觉是“还剩不少电却突然掉电”

### 13.2 触发条件

若满足：

- 非充电
- `VCellMin` 低于 `SOC_0_Vol + 120mV`

则启动弱单体约束。

### 13.3 约束方式

先计算一个 `guard_soc`，来源包括：

- OCV 查表值
- 弱单体与 `SOC_0_Vol` 的接近程度

随后把当前内部 SOC 往 `guard_soc` 方向下拉。

大致规则：

- 到 `SOC_0_Vol` 附近时，可直接压到 `0%`
- 临界区域可压到 `2%`
- 再外一层压到 `4%`
- 再外一层压到 `6%`
- 更宽窗口压到 `8%`

但当前实现里，每次执行只允许向 `guard_soc` 方向下降 `1%`，不会单次大跳。

这个逻辑比纯 OCV 更保守，明显带有“用户体验保护”导向。

## 14. 显示层平滑逻辑

### 14.1 内部值与显示值分离

当前实现已明确把：

- 内部真实 SOC
- 对外显示 SOC

分成两层。

内部真实值：

- `SOC_Calculate_Element.u8SOC_Now`

显示值：

- `g_soc_runtime.u8DisplaySoc`
- `SOC_Enhance_Element.u8_SOC`

### 14.2 平滑规则

`SOC_UpdateDisplaySoc()` 的行为：

- 上升时每次最多 `+1`
- 下降时每次最多 `-1`

结论：

- 显示值被故意设计为“慢涨慢跌”

### 14.3 对外同步

`SOC_SyncOutputData()` 除了同步 `u8_SOC` 外，还会同时写出：

- `u8_SOH`
- `u16_CapacityNow`
- `u16_CapacityFull`
- `u16_CapacityFactory`
- `u16_Cycle_times`

这里要注意：

- `u8_SOC` 是显示层值
- `CapacityNow` 是基于真实容量的直接换算

所以界面或上位机可能看到：

- 百分比已经被平滑
- 容量值没有同样程度平滑

## 15. Flash 持久化策略

### 15.1 保存内容

当前 SOC 快照只保存三元组：

- `u16SocNow`
- `u16DsgSocInt`
- `u32CycleTimes`

没有保存：

- `u32CapNow`
- `u32CapChange`
- `u32CapFull`
- `g_soc_runtime`

### 15.2 保存时机

`SOC_PersistSnapshotIfChanged()` 发现以下任一变化即保存：

- `u8SOC_Now`
- `u8DSG_SOC_Int`
- `u32Cycle_times`

这意味着：

- SOC 每变动 1% 基本就会触发存储
- 休眠补偿后若有变化，也会立即落盘

### 15.3 存储结构

`Flash.c` 当前为 SOC 使用两页 Flash journal：

- `FLASH_ADDR_STORAGE_SOC_SLOT_A`
- `FLASH_ADDR_STORAGE_SOC_SLOT_B`

每条记录包含：

- `magic`
- `version`
- `length`
- `sequence`
- `crc`
- payload

策略是：

1. 在当前页内顺序追加记录
2. 页满后切换到另一页
3. 擦空目标页后继续写入
4. 启动时从两页中找 `sequence` 最新的记录恢复

这比整页覆盖耐久性更好，也更适合热数据。

## 16. 外部命令与刷新入口

当前上位机/通信层对 SOC 有三类入口。

### 16.1 改 SOC 表

`Sci_WrRegs_0x10_SocTable()`

行为：

- 改写 `SOC_Table_Set[]`
- 置写标志
- 调用 `InitData_SOC()`

### 16.2 改 SOC 扩展参数

`Sci_WrRegs_0x10_SocElement()`

写入的是 `OtherElement` 中这四个参数：

- `u16Soc_Ah`
- `u16Soc_Cycle_times`
- `u16Soc_V_100`
- `u16Soc_V_0`

之后：

- `InitData_SOC()`
- `u16_RefreshData_Flag = 2`

### 16.3 一次性设 SOC

`Sci_WrReg_0x06_SetSocOnce()`

行为：

- `u16_RefreshData_Flag = 3`
- `u8_SetSocOnce = 用户值`

随后由 `SOC_RefreshData_Monitor()` 驱动生效。

### 16.4 功能位影响

系统功能位中有两个与 SOC 直接相关：

- `b1OnOFF_SOC_Fixed`
- `b1OnOFF_SOC_Zero`

对应行为：

- `SOC_Fixed` 会让对外读到的 `u16Soc = 60`
- `SOC_Zero` 会让对外读到的 `u16Soc = 0`

注意：

- 这是输出层覆盖，不等于内部积分状态被同步重置

## 17. `u16_RefreshData_Flag` 的真实含义

这个字段是外部命令进入内核的关键桥。

当前定义如下：

### `Flag = 1`

执行：

- 用 `Get_OpenCircuit_Value()` 做一次 OCV 刷新
- 若当前无充电，则只允许持平或下调，不允许上调

作用：

- 用当前 OCV 重新估算 SOC

### `Flag = 2`

执行：

- 清 `u8DSG_SOC_Int`
- 重装 `CapFactory / CycleTimes / CycleLimit`
- `u32CapFull = u32CapFactory`

注意：

- 原本应有的 `u8SOC_Now = 0` 被注释掉了

这意味着此路径更接近“容量/循环统计复位”，不是真正意义上的“内部 SOC 归零”。

### `Flag = 3`

执行：

- `u8SOC_Now = u8_SetSocOnce`
- 该路径是上位机一次性直接设置，不做单步限幅

然后统一：

- `SOC_ApplySocNow()`
- 清空显示运行态
- 强制显示值跟随
- 状态机切回 `TRANSFER`

## 18. 对外影响范围

### 18.1 故障模块

`Fault.c` 直接使用 `g_stCellInfoReport.SocElement.u16Soc` 做低电量分级判断。

影响：

- SOC 输出不仅影响显示，也直接影响故障与告警

现状还有一个问题：

- `App_CellSocUp_FirstCheck()` 已定义
- 但当前总调度里只调用了 `SecondCheck` 和 `ThirdCheck`
- 没有看到 `FirstCheck` 被真正挂到周期执行链

这意味着一级 SOC 低电量判断可能未实际生效。

### 18.2 显示模块

`LedBar.c` 用 `g_stCellInfoReport.SocElement.u16Soc` 做 LED 百分比显示。

因此用户看到的灯条百分比，是显示层平滑后的结果。

### 18.3 CAN 通信

`Can_HDX.c` 直接上报：

- `Soc`
- `Soh`
- `CapacityNow`
- `CapacityFactory`
- `Cycle_times`

因此：

- SOC 对整车通信也有直接影响

## 19. 当前实现边界与风险点

### 19.1 这是“积分主线”，不是“纯 OCV 模型”

真实主线是：

- 安时积分
- 叠加末端校准
- 叠加静置补偿
- 叠加弱单体约束
- 最后做显示平滑

所以后续如果要重构，不能把它误当成一个简单查表模块。

### 19.2 当前 `SOH` 基本不形成真实衰减闭环

虽然代码里存在：

- `u32CapFull`
- `u32CapFull_Cal_As`
- `u8_SOH`

但从当前仓库可见代码看：

- `u32CapFull` 在多数路径都直接等于 `u32CapFactory`
- 没有形成完整的容量学习闭环

结果是：

- `SOH` 当前大概率长期保持 `100`

### 19.3 `SOC_Zero` 名义与实际不完全一致

外部看起来存在“清零 SOC”能力，但实际代码表现为：

- 输出层可以强制显示 0
- `RefreshData_Flag == 2` 不会明确把内部 `u8SOC_Now` 设为 0

这会导致：

- 表面显示与内部积分态可能存在分离

### 19.4 启动参数恢复边界需要额外确认

按当前仓库代码：

- SOC 运行快照从 Flash SOC 区恢复
- AFE 参数从 Flash AFE 区恢复
- `OtherElement` 与 `SOC_Table_Set` 启动时先走默认装载

若系统期望：

- `Soc_Ah`
- `Soc_V_100`
- `Soc_V_0`
- `SOC_Table_Set`

也能掉电保持，那么还需要继续核查是否存在仓库外补充逻辑。

### 19.5 故障一档检查存在漏挂风险

当前调度只看到了：

- `App_CellSocUp_SecondCheck()`
- `App_CellSocUp_ThirdCheck()`

没看到 `App_CellSocUp_FirstCheck()` 被周期调用。

如果这不是故意裁剪，那么：

- 一档 SOC 低电量故障逻辑可能未实际生效

### 19.6 持久化保存的是“轻快照”，不是完整状态镜像

当前未保存：

- `u32CapChange`
- `u32CapNow` 的全部上下文
- `g_soc_runtime`
- 端点校准计数器

因此掉电恢复后虽然能恢复大致 SOC，但不是完整续算。

## 20. 一句话总结当前 SOC 架构

当前 SOC 模块可以概括为：

```text
配置层: OtherElement + SOC_Table_Set
输入层: VCellMin/VCellMax/Ichg/Idsg
算法层: 积分 + 末端校准 + 静置补偿 + 弱单体约束
显示层: 显示 SOC 平滑
存储层: Flash journal 保存 SOC 三元组
输出层: Fault / LedBar / CAN / 上位机读数
```

它已经不是一个“单文件单算法”的模块，而是一个跨：

- 参数配置
- 运行态积分
- 低功耗恢复
- 热数据存储
- 下游故障/显示/通信

的组合系统。

## 21. 后续建议

如果后续要继续演进，建议按下面顺序推进：

1. 先把“内部真实 SOC”和“显示 SOC”在命名层彻底分开，避免误用。
2. 明确 `SOC_Fixed`、`SOC_Zero` 是“输出覆盖”还是“内部状态重置”，不要继续混用。
3. 核查 `OtherElement` 与 `SOC_Table_Set` 的掉电保持链路，确认是否已完整迁到 Flash。
4. 处理 `App_CellSocUp_FirstCheck()` 未挂调度的问题。
5. 若要做容量学习，再单独闭环 `u32CapFull / SOH`，不要与当前显示 SOC 改动混在一起。
6. 若要进一步提升掉电连续性，可把 `u32CapNow` 与少量运行态一起做更完整快照。

# SOC 模块综合分析报告

> 源文件：`SOC.c`（入口调度）、`SocEnhance.c`（核心算法）、`SocEnhance.h`（接口声明）
> 日期：2026-06-12

---

## 1. 模块概览与架构

SOC 模块负责 BMS 系统中 **荷电状态（State of Charge）** 的实时估算与管理。整体采用 **安时积分 + 电压校准** 的经典 BMS SOC 估算方案，辅以多项增强策略（低电量加速、Sag 保持、空尾阶梯校准、长时间静置修正等）。

### 架构层次

```
┌─────────────────────────────────────────────────────┐
│                   应用调度层                          │
│  App_SOC()          -- SOC.c:64   周期调度入口       │
│  InitData_SOC()     -- SOC.c:59   初始化入口        │
└────────────┬───────────────────────────┬────────────┘
             │                           │
             ▼                           ▼
┌────────────────────┐   ┌──────────────────────────┐
│  电流采样判断       │   │  报告数据发布             │
│  AfeCurrent_GetSeq │   │  SOC_PublishReportData() │
│  SOC_GetNetCurrent │   │  → g_stCellInfoReport    │
└────────┬───────────┘   └──────────────────────────┘
         │
         ▼
┌───────────────────────────────────────────────────┐
│           SocEnhance.c  核心算法层                 │
│                                                   │
│  SOC_IntEnhance_Ctrl(net_current_ma)              │
│  ┌──────────┐  ┌──────────┐  ┌───────────────┐   │
│  │ 安时积分  │→│ Sag保持   │→│ 电压校准       │   │
│  │ integrate │  │ sag_hold  │  │ full/empty/   │   │
│  │           │  │           │  │ rest_down     │   │
│  └──────────┘  └──────────┘  └───────────────┘   │
│         │                              │           │
│         ▼                              ▼           │
│  ┌──────────┐                ┌───────────────┐    │
│  │ 低SOC加速 │                │ 静置OCV修正    │    │
│  │ low_soc   │                │ rest_timer     │    │
│  └──────────┘                └───────────────┘    │
│                                    │               │
│                                    ▼               │
│                            ┌───────────────┐      │
│                            │ Flash 存储     │      │
│                            │ soc_save_if_   │      │
│                            │ needed()       │      │
│                            └───────────────┘      │
└───────────────────────────────────────────────────┘
```

### 文件职责

| 文件 | 行数 | 职责 |
|------|------|------|
| `SocEnhance.h` | 33 行 | 接口声明、OOC 查找表大小定义、电池化学类型枚举 |
| `SOC.c` | 83 行 | 应用层调度入口：判断是否有新 AFE 采样、计算净电流、分发调用 |
| `SocEnhance.c` | 1014 行 | 核心算法全部实现：积分、校准、存储、OOC 查找、SOH 计算等 |

---

## 2. 数据流（从电流采样到 SOC 输出）

### 2.1 电流采样路径

```
AFE 硬件采样
    │
    ▼
AfeCurrent_GetSeq()            -- 检测是否有新采样
    │
    ├── 无新采样 → SOC_PublishReportData()   (仅发布)
    │
    └── 有新采样 →
         │
         ├── g_stCellInfoReport.u16Ichg       (充电电流, A*10)
         ├── g_stCellInfoReport.u16IDischg    (放电电流, A*10)
         │
         ▼
    SOC_GetNetCurrentMilliAmp()               -- SOC.c:47
         │
         ├── 充电分量: chg_a10 = u16Ichg
         ├── 放电分量: dsg_a10 = u16IDischg + TypeC等效电流
         │
         ▼
    SOC_GetTypeCBatEquivCurrentA10()          -- SOC.c:20
         │
         ├── ADC_GetTypeCOutCurrentMilliAmp() -- Type-C口输出电流
         ├── SOC_GetPackVoltageForTypeCMv()   -- 电池包电压
         │
         ▼  (功率守恒: I_out * V_out = I_bat * V_bat * η)
    返回净电流 (int32_t, mA, 正=充电 负=放电)
         │
         ▼
    SOC_IntEnhance_Ctrl(net_current_ma)       -- SocEnhance.c:945
```

### 2.2 SOC 核心处理路径

```
SOC_IntEnhance_Ctrl(net_current_ma)
    │
    ├──① soc_direction()                    判断工作模式 (CHG/DSG/RELAX)
    │
    ├──② soc_integrate(net_current_ma)      安时积分
    │      ├── 扣除板级自耗 SOC_BOARD_SELF_CONSUMPTION_MA
    │      ├── 计算 delta_as10 (净容量变化)
    │      ├── 更新 cap_now_as10
    │      ├── 放电时累积循环计数 cycle_x100
    │      └── 更新 soc = soc_from_cap()
    │
    ├──③ soc_update_sag_hold()              Sag保持管理
    │
    ├──④ soc_apply_full_confirm()           满充校准
    │      └── 条件: 非放电 + VCellMax >= 4180mV + 一致性OK
    │
    ├──⑤ soc_select_empty_tail_step()       空尾阶梯校准
    │      └── 根据电流档位和电压偏差选择下降阶梯
    │
    ├──⑥ 低SOC加速 (soc <= 10% 放电时)
    │      └── 每50 ticks强制 SOC-1%
    │
    ├──⑦ soc_update_rest_timer()            静置OCV修正
    │      └── 静置>=30min + 电压稳定 → 渐进修正到OCV查表值
    │
    ├──⑧ soc_save_if_needed()               条件写入Flash
    │
    └──⑨ SOC_PublishReportData()            发布到全局报告结构体
```

### 2.3 SOC 数据发布路径

```
SOC_PublishReportData()                     -- SocEnhance.c:378
    │
    ├── g_stCellInfoReport.SocElement.u16Soc          (0~100%)
    ├── g_stCellInfoReport.SocElement.u16Soh          (0~100%)
    ├── g_stCellInfoReport.SocElement.u16CapacityNow  (Ah*100)
    ├── g_stCellInfoReport.SocElement.u16CapacityFull (Ah*100)
    ├── g_stCellInfoReport.SocElement.u16CapacityFactory (Ah*100)
    └── g_stCellInfoReport.SocElement.u16Cycle_times  (次)
```

---

## 3. 关键函数详解

### 3.1 SOC.c 中的函数

| 函数 | 行号 | 说明 |
|------|------|------|
| `SOC_LimitA10()` | 4-7 | 将 UINT32 电流值截断到 UINT16 范围（A*10 单位） |
| `SOC_GetPackVoltageForTypeCMv()` | 9-18 | 获取电池包电压(mV)，优先用 u16VCellTotle*10，回退到 ADC |
| `SOC_GetTypeCBatEquivCurrentA10()` | 20-45 | Type-C 口输出电流等效到电池侧电流（功率守恒换算），单位 A*10 |
| `SOC_GetNetCurrentMilliAmp()` | 47-57 | 计算净电流(mA)，正=充电 负=放电，放电分量含 Type-C 等效 |
| `InitData_SOC()` | 59-62 | 初始化入口，调用 soc_param_lib_init() |
| `App_SOC()` | 64-83 | **周期调度主函数**：检测新 AFE 采样，有则计算净电流并调用增强算法，无则仅发布数据 |

### 3.2 SocEnhance.c 中的函数

#### 初始化与存储

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_param_lib_init()` | 912-921 | 模块初始化：清零状态、设工厂容量、加载或默认值 |
| `soc_load_or_default()` | 459-504 | 从 Flash 加载 SOC 数据，无效时使用 OOC 查表或默认 60% |
| `soc_save()` | 410-430 | 将当前 SOC 快照打包为 STORAGE_FLASH_SOC_DATA 并写入 Flash |
| `soc_save_if_needed()` | 448-457 | 仅在数据变化时才写入（对比 s_saved_soc） |
| `soc_save_current_snapshot()` | 440-446 | 立即保存并更新 save mark |
| `soc_update_save_mark()` | 432-438 | 更新 s_saved_soc 缓存，避免重复写入 |
| `SOC_SaveSnapshotBeforeSleep()` | 940-943 | 休眠前保存接口 |
| `SOC_ResetStoredSnapshotToDefault()` | 923-938 | 重置 Flash 中 SOC 数据为出厂默认值 |

#### 安时积分

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_integrate()` | 528-571 | **核心积分函数**：扣自耗、累积残余(mams)、更新容量和循环数、限幅、充电到100%时强制99%（等待满充确认） |
| `soc_factory_cap_as10_from()` | 177-180 | 将 Ah*10 容量转换为 as10 单位（乘 3600） |
| `soc_from_cap()` | 198-209 | 从当前容量计算 SOC 百分比（四舍五入） |
| `soc_set()` | 217-226 | 设置 SOC 并反算 cap_now_as10，清零残余 |
| `soc_cap_to_ah100()` | 211-215 | 将 as10 容量转为 Ah*100 报告值 |

#### SOH 与容量

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_soh_from_cycle()` | 182-186 | 根据循环次数线性退化 SOH，每 SOC_SOH_CYCLE_STEP(100) 次循环降1%，最低80% |
| `soc_refresh_capacity_base()` | 188-196 | 重算 cap_full = cap_factory * soh / 100，cap_now 不超过 cap_full |

#### 电压校准

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_ocv_percent()` | 269-281 | 查 OOC 表获取当前电压对应的 SOC 百分比 |
| `soc_table_percent()` | 283-310 | **线性插值查找表函数**：在电压-SOC 查找表中做分段线性插值 |
| `soc_voltage_valid()` | 243-254 | 检查电压是否在有效范围 [2000mV, 5000mV] 内 |
| `soc_calibration_allowed()` | 256-267 | 校准前提条件：电压有效 + 单体压差 <= 300mV |

#### 满充校准

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_full_confirm_allowed()` | 573-594 | 满充确认条件：VCellMax >= 4180mV、所有单体接近满充电压、压差 <= 120mV |
| `soc_apply_full_confirm()` | 690-712 | 持续满足条件 SOC_FULL_SECONDS(15s) 后，逐步将 SOC 拉到 100% |

#### 空尾校准（低电量区间）

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_calc_dynamic_empty_mv()` | 327-354 | 根据内阻估算动态压降，计算动态放空阈值 |
| `soc_empty_threshold_mv()` | 312-325 | 基础放空阈值 + 偏移 |
| `soc_select_empty_tail_step()` | 628-688 | **核心空尾策略**：根据电流档位（RELAX/LIGHT/MID/HEAVY）和电压偏差选择阶梯目标和等待时间 |
| `soc_apply_empty_tail()` | 714-733 | 执行空尾阶梯：到时间后逐步将 SOC 拉向目标值 |

#### Sag 保持

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_update_sag_hold()` | 596-618 | 大电流放电时启动 sag_hold 倒计时，防止电压恢复后的 OCV 校准误判 |
| `soc_sag_hold_blocks_calibration()` | 620-626 | Sag 保持期间阻止校准（电压回升但非真实 OCV） |

#### 静置 OCV 修正

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_update_rest_timer()` | 813-844 | 追踪静置时间和电压稳定性，满足条件后设定 rest_down_target |
| `soc_rest_voltage_stable()` | 789-811 | 判断电压是否稳定（Vmin/Vmax 变化 <= 30mV） |
| `soc_apply_long_rest_down_step()` | 744-787 | 每 SOC_LONG_REST_DOWN_STEP_SECONDS(1800s) 执行一次渐进修正（每次最多步进 SOC_CAL_STEP=1%） |
| `SOC_ApplyRtcRelaxationCompensation()` | 1002-1014 | 休眠唤醒后的 RTC 时间补偿接口 |
| `soc_add_rest_seconds()` | 846-863 | 安全地累加静置时间（带上限） |
| `soc_apply_rtc_rest_ocv()` | 865-910 | 基于 RTC 的静置 OCV 修正（唤醒路径） |

#### 辅助与工具

| 函数 | 行号 | 说明 |
|------|------|------|
| `soc_direction()` | 228-241 | 根据净电流判断 CHG/DSG/RELAX 模式（门限 0.2A） |
| `soc_cell_delta()` | 154-157 | 计算单体最大压差 |
| `soc_step()` | 164-175 | 步进函数：从当前值向目标值步进（防止过冲） |
| `soc_net_current_idsg_a10()` | 365-376 | 将净电流转为放电分量 A*10（仅放电为正，充电返回0） |
| `soc_current_limit_a10()` | 356-363 | 计算电流档位阈值（容量 / divider） |
| `soc_seconds_to_ticks()` | 149-152 | 秒数转 SOC tick 数（*5） |
| `SOC_PublishReportData()` | 378-388 | 将内部状态写入全局报告结构体 g_stCellInfoReport |
| `SOC_RequestCapacityReset()` | 390-401 | 重置容量参数（保留当前 SOC） |
| `SOC_RequestSetOnce()` | 403-408 | 一次性设定 SOC 值并保存 |
| `soc_set_rest_down_target()` | 513-526 | 设定静置修正目标（仅允许向下修正） |
| `soc_clear_rest_down_target()` | 506-511 | 清除静置修正状态 |

---

## 4. 关键数据结构

### 4.1 SOC_STATE（SocEnhance.c:82-104）

SOC 模块的核心内部状态结构体（static 变量 `s_soc`）：

```c
typedef struct SOC_STATE_TAG {
    UINT32 cap_factory_as10;     // 工厂额定容量 (A*10 * 3600 = as10单位)
    UINT32 cap_full_as10;        // 当前满充容量 (cap_factory * soh / 100)
    UINT32 cap_now_as10;         // 当前剩余容量 (as10)
    UINT32 cycle_x100;           // 循环次数 * 100 (用于精度累积)
    UINT32 dsg_acc_as10;         // 放电累积量 (用于循环计数进位)
    int32_t rem_mams;            // 积分残余 (mAs*10, 用于下一周期累积)
    UINT32 rest_soc_ticks;       // 静置时间 (200ms tick 计数)
    UINT32 stable_rest_soc_ticks;// 电压稳定静置时间
    UINT32 long_rest_down_soc_ticks; // 长静置修正步进计时器
    UINT16 full_ticks;           // 满充确认计时器
    UINT16 empty_ticks;          // 空尾阶梯计时器
    UINT16 sag_hold_ticks;       // Sag保持倒计时
    UINT16 rest_ref_vmin;        // 静置参考电压 Vmin
    UINT16 rest_ref_vmax;        // 静置参考电压 Vmax
    UINT16 snapshot_flags;       // 快照标志位 (bit0: REBOUND_HOLD)
    UINT8  soc;                  // 当前 SOC (0~100%)
    UINT8  soh;                  // 当前 SOH (80~100%)
    UINT8  rest_down_target;     // 静置修正目标 SOC
    UINT8  rest_down_valid;      // 静置修正是否有效
} SOC_STATE;
```

### 4.2 STORAGE_FLASH_SOC_DATA（SocEnhance.c:11-25）

Flash 存储结构体，用于 SOC 数据断电保持：

```c
typedef struct {
    UINT16 u16FormatVersion;       // 格式版本号 (当前 V2 = 0x0002)
    UINT16 u16SocNow;              // 当前 SOC (0~100)
    UINT16 u16DsgSocInt;           // 放电SOC积分 (0~100, 循环进度百分比)
    UINT16 u16MaxErrorPercent;     // 最大误差百分比 (固定100)
    UINT32 u32CycleTimes;          // 循环次数 * 100
    UINT32 u32CapNow;              // 当前容量 (as10)
    UINT32 u32CapFull;             // 当前满充容量 (as10)
    UINT32 u32LearnPassedAs10;     // 学习通过量 (as10)
    UINT16 u16LearnAnchorSoc;      // 学习锚点SOC (保留)
    UINT16 u16LearnState;          // 学习状态 (保留)
    UINT16 u16Flags;               // 标志位 (bit0: REBOUND_HOLD)
    UINT16 u16Reserved[4];         // 保留字段
} STORAGE_FLASH_SOC_DATA;
```

### 4.3 SOC_SAVE_MARK（SocEnhance.c:106-112）

SOC 保存标记，用于避免重复写入 Flash：

```c
typedef struct SOC_SAVE_MARK_TAG {
    UINT32 cycle_x100;
    UINT32 cap_full_as10;
    UINT16 snapshot_flags;
    UINT8  soc;
} SOC_SAVE_MARK;
```

### 4.4 SOC_TAIL_STEP（SocEnhance.c:114-118）

空尾阶梯参数：

```c
typedef struct {
    UINT8  target;   // 目标 SOC 值
    UINT16 ticks;    // 等待时间 (200ms tick)
} SOC_TAIL_STEP;
```

### 4.5 SOC_MODE 枚举（SocEnhance.c:66-71）

```c
typedef enum {
    SOC_MODE_RELAX = 0,   // 静置
    SOC_MODE_CHG  = 1,    // 充电
    SOC_MODE_DSG  = 2     // 放电
} SOC_MODE;
```

### 4.6 SOC_EMPTY_BAND 枚举（SocEnhance.c:73-80）

空尾电流档位分级：

```c
typedef enum {
    SOC_EMPTY_BAND_RELAX = 0,   // 静置
    SOC_EMPTY_BAND_LIGHT,        // 轻载
    SOC_EMPTY_BAND_MID,          // 中载
    SOC_EMPTY_BAND_HEAVY,        // 重载
    SOC_EMPTY_BAND_COUNT
} SOC_EMPTY_BAND;
```

### 4.7 上报结构体 SOC_CAL_ELEMENT_UPPER（Sci_Upper.h:23-30）

```c
struct SOC_CAL_ELEMENT_UPPER {
    UINT16 u16Soc;              // 当前 SOC (0~100%)
    UINT16 u16Soh;              // SOH (0~100%)
    UINT16 u16CapacityNow;      // 当前容量 (Ah*100)
    UINT16 u16CapacityFull;     // 当前满充容量 (Ah*100)
    UINT16 u16CapacityFactory;  // 工厂额定容量 (Ah*100)
    UINT16 u16Cycle_times;      // 循环次数
};
```

---

## 5. 关键常量及其含义

### 5.1 时间与采样常量

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_TICK_MS` | 200 | L31 | SOC 算法 tick 周期 (200ms) |
| `SOC_TICKS_PER_SECOND` | 5 | L32 | 每秒的 tick 数 (1000/200=5) |
| `SOC_CURRENT_ACTIVE_A10` | 2 | L33 | 电流活跃门限 (0.2A, A*10) |
| `SOC_MA_PER_A10` | 100 | L34 | mA 与 A*10 的换算因子 |
| `SOC_MAMS_PER_AS10` | 100000 | L35 | 1 as10 = 100000 mAs (1A*10 秒) |

### 5.2 容量与 SOH 常量

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_BOARD_SELF_CONSUMPTION_MA` | 30 | L36 | 板级自耗电流 (mA)，来自 Project_Config |
| `SOC_SOH_MIN` | 80 | L37 | SOH 最低值 (80%) |
| `SOC_SOH_CYCLE_STEP` | 100 | L38 | 每 100 次循环 SOH 降 1% |
| `s_soc_default_startup_percent` | 60 | L122 | Flash 无效时的默认启动 SOC |

### 5.3 满充校准常量

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_FULL_SECONDS` | 15 | L39 | 满充确认等待时间 (秒) |
| `SOC_FULL_CONFIRM_MIN_VMAX_MV` | 4180 | L40 | 满充确认最低 VCellMax (mV) |
| `SOC_FULL_MIN_MARGIN_MV` | 80 | L41 | 满充时单体距满充电压的最小裕量 (mV) |
| `SOC_FULL_MAX_DELTA_MV` | 120 | L42 | 满充确认允许的最大单体压差 (mV) |
| `SOC_CAL_STEP` | 1 | L49 | 校准步进百分比 (%/次) |

### 5.4 空尾校准常量

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_EMPTY_CUR_LIGHT_DIVIDER` | 5 | L43 | 轻载电流 = 容量/5 |
| `SOC_EMPTY_CUR_MID_DIVIDER` | 2 | L44 | 中载电流 = 容量/2 |
| `SOC_EMPTY_TAIL_START_OFFSET_MV` | 400 | L50 | 空尾起始偏移 (mV) |

### 5.5 静置 OCV 常量

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_REST_OCV_SECONDS` | 1800 | L47 | OCV 静置确认时间 (30分钟) |
| `SOC_LONG_REST_DOWN_STEP_SECONDS` | 1800 | L48 | 长静置修正步进间隔 (30分钟) |
| `SOC_REST_MAX_DELTA_MV` | 200 | L54 | 静置时允许的最大单体压差 (mV) |
| `SOC_REST_STABLE_DELTA_MV` | 30 | L55 | 电压稳定性判定门限 (mV) |

### 5.6 Sag 保持常量

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_SAG_HOLDOFF_SECONDS` | 30 | L45 | Sag 保持时间 (秒) |
| `SOC_SAG_ALLOW_OFFSET_MV` | 50 | L46 | Sag 保持期间的电压偏移容限 (mV) |
| `SOC_SAG_MAX_MV` | 800 | L62 | 最大 Sag 压降估算 (mV) |
| `SOC_REBOUND_BOOT_HOLDOFF_SECONDS` | 300 | L56 | 重启后的 Sag 保持时间 (5分钟) |

### 5.7 电压与保护常量

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_VALID_MIN_MV` | 2000 | L51 | 电压有效下限 (mV) |
| `SOC_VALID_MAX_MV` | 5000 | L52 | 电压有效上限 (mV) |
| `SOC_VALID_MAX_DELTA_MV` | 300 | L53 | 校准允许的最大单体压差 (mV) |
| `SOC_UVP_MV` | 2800 | L59 | 欠压保护电压 (mV) |
| `SOC_SAFETY_MARGIN_MV` | 100 | L60 | UVP 安全裕量 (mV) |
| `SOC_R_INTERNAL_ESTIMATE_X10` | 100 | L61 | 内阻估算 (mΩ * 10) |

### 5.8 低 SOC 加速常量

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_LOW_SOC_ACCEL_SOC` | 10 | L63 | 低 SOC 加速阈值 (10%) |
| `SOC_LOW_SOC_ACCEL_TICKS` | 50 | L64 | 加速间隔 (50 * 200ms = 10秒/次) |

### 5.9 OOC 查找表

| 常量 | 值 | 位置 | 含义 |
|------|-----|------|------|
| `SOC_TABLE_SIZE` | 42 | SocEnhance.h:9 | 查找表大小 (21对 电压-SOC 数据点 + 尾部填充) |
| `SOC_Table_LiFePO[]` | - | L132-137 | 磷酸铁锂电池 OOC 查找表 (电压范围 3112~3336mV) |
| `SocTable_TernaryLi[]` | - | L141-146 | 三元锂电池 OOC 查找表 (电压范围 3000~4160mV) |

---

## 6. 算法流程图

### 6.1 主循环流程

```
                        ┌──────────────┐
                        │  App_SOC()   │
                        │  (周期调用)   │
                        └──────┬───────┘
                               │
                     ┌─────────▼─────────┐
                     │ 有新 AFE 采样？    │
                     └────┬──────────┬───┘
                     是   │          │ 否
                          ▼          ▼
              ┌───────────────┐  ┌──────────────┐
              │ 计算净电流     │  │ 仅发布数据   │
              │ GetNetCurrent │  │ PublishData  │
              └───────┬───────┘  └──────────────┘
                      │
                      ▼
         ┌─────────────────────────────┐
         │  SOC_IntEnhance_Ctrl(net_ma)│
         └─────────────┬───────────────┘
                       │
    ┌──────────────────┼─────────────────────────────┐
    │                  │                             │
    ▼                  ▼                             ▼
┌────────┐    ┌──────────────┐              ┌──────────────┐
│ 1.积分  │    │ 2. Sag保持   │              │ 3.满充确认   │
│integrate│   │  sag_hold     │              │ full_confirm │
└────┬───┘    └──────┬───────┘              └──────┬───────┘
     │               │                             │
     │               │                             │
     │        ┌──────▼───────┐              ┌──────▼───────┐
     │        │ 4.空尾阶梯    │              │ 5.低SOC加速   │
     │        │ empty_tail    │              │ soc--         │
     │        └──────┬───────┘              └──────┬───────┘
     │               │                             │
     └───────┬───────┘─────────────────────────────┘
             │
             ▼
    ┌─────────────────────┐
    │ 6. 静置OCV修正       │
    │ rest_timer →         │
    │ long_rest_down_step  │
    └─────────┬───────────┘
              │
              ▼
    ┌─────────────────────┐
    │ 7. 条件保存 Flash    │
    │ save_if_needed       │
    └─────────┬───────────┘
              │
              ▼
    ┌─────────────────────┐
    │ 8. 发布数据          │
    │ PublishReportData    │
    └─────────────────────┘
```

### 6.2 安时积分详细流程

```
soc_integrate(net_current_ma)
    │
    ├── acc_mams = (net_ma - 30) * 200 + rem_mams
    │              (扣自耗)  (tick ms)  (累积残余)
    │
    ├── delta_as10 = acc_mams / 100000
    │   rem_mams = acc_mams % 100000
    │
    ├── delta_as10 == 0 ? ──→ return (不变化)
    │
    ├── delta_as10 < 0 ? (放电)
    │   ├── dsg_acc_as10 += |delta|
    │   ├── cycle_x100 += dsg_acc_as10 / unit
    │   ├── dsg_acc_as10 %= unit
    │   └── soc_refresh_capacity_base()  ← SOH 可能变化
    │
    ├── cap_now += delta_as10
    │   ├── < 0 → cap_now = 0
    │   └── > cap_full → cap_now = cap_full
    │
    ├── soc = soc_from_cap()
    │
    └── 充电到 100% → 强制 99% (等满充确认)
```

### 6.3 空尾阶梯决策流程

```
soc_select_empty_tail_step(mode, current_ma, &step)
    │
    ├── 充电模式 或 电压无效 → 不启用
    │
    ├── Sag 保持阻止校准 → 不启用
    │
    ├── dynamic_mv = UVP(2800) + 安全裕量(100) + sag压降
    │   (sag = |I| * R_internal / 10, 上限 800mV)
    │
    ├── VCellMin > dynamic_mv → 不启用
    │
    ├── 根据电流确定档位:
    │   ├── RELAX → SOC_EMPTY_BAND_RELAX
    │   ├── I <= 容量/5 → LIGHT
    │   ├── I <= 容量/2 → MID
    │   └── I > 容量/2  → HEAVY
    │
    └── 根据档位 + 电压偏差选择阶梯:
        ├── 偏差 <= 50mV:
        │   HEAVY → target=5%, ticks=100 (20s)
        │   其他  → target=3%, ticks=100
        ├── 偏差 <= 150mV:
        │   HEAVY → target=3%, ticks=50 (10s)
        │   其他  → target=1%, ticks=50
        └── 偏差 > 150mV:
            target=0%, ticks=20 (4s)
```

### 6.4 启动初始化流程

```
soc_param_lib_init()
    │
    ├── memset(&s_soc, 0, sizeof)
    ├── cap_factory = OtherElement.u16Soc_Ah * 3600
    ├── cycle_x100 = OtherElement.u16Soc_Cycle_times * 100
    ├── soc_refresh_capacity_base()  → soh, cap_full
    │
    └── soc_load_or_default()
         │
         ├── StorageFlash_LoadSocData(&data)
         │
         ├── 数据有效？
         │   ├── 是 → 恢复 cycle_x100, dsg_acc, cap_now, soc
         │   │        若有 REBOUND_HOLD → 恢复 sag_hold_ticks
         │   │
         │   └── 否 → 使用 OtherElement 参数
         │           ├── 电压有效？→ soc_ocv_percent() 查表
         │           └── 否 → 默认 60%
         │           └── 保存到 Flash
         │
         └── soc_update_save_mark()
```

---

## 7. 潜在问题与改进建议

### 7.1 精度与准确性

**问题 1：三元锂查表缺失尾部填充（SocEnhance.c:141-146）**

三元锂查表 `SocTable_TernaryLi` 只有 44 个元素（22 对数据点），而 `SOC_TABLE_SIZE` 定义为 42（21 对 + 尾部预留）。三元锂表刚好填满但没有尾部冗余。对比 LiFePO 表在尾部有 `(1000, 0, 1000, 0)` 的填充对，三元锂表缺少这种保护。如果查找逻辑遍历到最后一个有效对且电压极低，`soc_table_percent()` 在 line 309 会返回 `table[SOC_TABLE_SIZE-1]` 即 `0`（SOC=0），这恰好是正确行为，但属于隐式依赖而非显式保护。

> **建议**：在三元锂表尾部增加填充对 `(1000, 0)` 以保持一致性，减少未来维护风险。

**问题 2：积分残余累积精度（SocEnhance.c:535-539）**

`rem_mams` 是 int32_t 类型，在连续运行中长期累积可能产生微小漂移。`SOC_MAMS_PER_AS10` = 100000，每次 tick 的最大积分量 = (max_current - 30) * 200。若电流为 100A，则 delta = 99970 * 200 = 19,994,000，除以 100000 后残余最大 99999，远在 int32 范围内，短期无溢出风险。但如果系统长时间运行不重启且电流方向频繁切换，残余值的符号交替可能累积。

> **建议**：定期（如每次保存时）将 rem_mams 归零，或在积分更新时增加饱和检查。

**问题 3：SOH 线性退化模型过于简化（SocEnhance.c:182-186）**

当前 SOH 模型为纯线性退化：每 100 次循环降 1%，最低 80%。这忽略了实际锂电池的非线性退化特性（前期退化慢、后期加速退化、温度影响等）。

> **建议**：如精度要求不高，当前方案可接受（简单可靠）。若需改进，可引入分段退化曲线或库仑效率因子。

### 7.2 安全性与鲁棒性

**问题 4：Flash 写入无异常保护（SocEnhance.c:410-430）**

`soc_save()` 直接调用 `StorageFlash_SaveSocData()` 但没有重试机制。如果 Flash 写入失败（返回 0），`soc_update_save_mark()` 不会执行（line 443-445），下次循环会再次尝试写入，这是一个正确的降级策略。但如果 Flash 持续故障，会导致每个 200ms tick 都尝试写入，产生不必要的开销。

> **建议**：增加写入失败计数器和退避间隔，避免在 Flash 故障时频繁重试。

**问题 5：充电到 100% 强制 99% 的边界行为（SocEnhance.c:566-570）**

当积分使 SOC 从 99.x% 充到 100% 时，代码强制回退到 99%。这意味着如果满充确认条件（`soc_full_confirm_allowed()`）因某种原因不满足（如某个单体电压偏低），SOC 将永远停在 99%，即使电池实际已充满。

> **建议**：考虑在连续充电且无放电的情况下，增加一个超时机制，超时后允许 SOC 到 100%。

**问题 6：`soc_update_rest_timer` 中 VCellMin >= 3700 硬编码跳过（SocEnhance.c:817）**

当 VCellMin >= 3700mV 时直接跳过静置修正逻辑。这个阈值在三元锂电池的中高 SOC 区间是合理的（OCV 曲线平坦区），但对于磷酸铁锂电池（标称电压 ~3.2V），3700mV 几乎永远不会达到，意味着磷酸铁锂电池几乎不会跳过此检查。

> **建议**：将 3700mV 阈值改为可配置常量，或根据电池化学类型使用不同值。

### 7.3 可维护性与设计

**问题 7：Type-C 等效电流计算中的整数溢出风险（SOC.c:34-44）**

`SOC_GetTypeCBatEquivCurrentA10()` 使用 uint64_t 做中间计算，但 `numerator = typec_out_current_mA * TYPEC_OUT_VOLTAGE_MV * 1000` 中，如果 `typec_out_current_mA` 接近 65535 且 `TYPEC_OUT_VOLTAGE_MV` = 5000，则 numerator = 65535 * 5000 * 1000 = 3.27e11，在 uint64 范围内安全。但如果 TYPEC_OUT_VOLTAGE_MV 未来变更，需要注意。

> **建议**：当前无实际风险，但建议添加注释标注最大值边界。

**问题 8：空尾阶梯中使用硬编码阈值（SocEnhance.c:671-685）**

`voltage_deficit` 的判断阈值 50mV、150mV 是硬编码的，对应的 target 和 ticks 也是硬编码。这些值与具体的电池内阻和放电特性相关，不同电池包可能需要不同的配置。

> **建议**：将这些阈值提取为可配置常量（如 `PROJECT_CFG_SOC_EMPTY_DEFICIT_LIGHT_MV` 等）。

**问题 9：`soc_integrate()` 中充电到 99% 的逻辑位置**

充电到 99% 的强制回退发生在 `soc_integrate()` 内部（line 566-570），但满充确认逻辑在 `soc_apply_full_confirm()` 中。这意味着积分和校准的职责边界不够清晰——积分函数不应该直接修改 SOC 值来配合校准逻辑。

> **建议**：将 99% 限制逻辑移到 `soc_apply_full_confirm()` 的决策层，使 `soc_integrate()` 保持纯粹的安时积分职责。

**问题 10：缺少温度补偿**

整个 SOC 算法没有考虑温度对容量和内阻的影响。低温下电池可用容量降低、内阻增大，当前算法会低估实际消耗的容量（因为内阻压降会导致电压更早达到放空阈值）。

> **建议**：如应用场景温度范围不大（如室内储能），当前方案可接受。若需户外应用，建议增加温度-SOC 偏移补偿表。

### 7.4 代码风格

**问题 11：`SOC_TABLE_SIZE` 与实际表大小不匹配**

`SOC_TABLE_SIZE` = 42 定义在 `SocEnhance.h` 中，但三元锂表实际有 44 个元素（22 对），LiFePO 表有 48 个元素（24 对）。`soc_table_percent()` 的遍历范围是 `0 <= i <= SOC_TABLE_SIZE - 4`（即 0 到 38），这意味着两个表的最后几对数据不会被遍历到。对于 LiFePO 表，最后两对 `(1000, 0, 1000, 0)` 不会参与插值计算（但作为返回值使用）。这是一个设计意图不清晰的地方。

> **建议**：明确 `SOC_TABLE_SIZE` 的语义（是有效数据对数还是总元素数），并确保表大小与遍历逻辑一致。

---

## 附录：OOC 查找表数据

### 三元锂电池 (SocTable_TernaryLi)

| 电压 (mV) | SOC (%) |
|-----------|---------|
| 4160 | 100 |
| 4100 | 95 |
| 4050 | 90 |
| 3995 | 85 |
| 3935 | 80 |
| 3880 | 75 |
| 3835 | 70 |
| 3795 | 65 |
| 3760 | 60 |
| 3725 | 55 |
| 3695 | 50 |
| 3670 | 45 |
| 3645 | 40 |
| 3615 | 35 |
| 3585 | 30 |
| 3555 | 25 |
| 3525 | 20 |
| 3480 | 15 |
| 3400 | 10 |
| 3250 | 5 |
| 3000 | 0 |

### 磷酸铁锂电池 (SOC_Table_LiFePO)

| 电压 (mV) | SOC (%) |
|-----------|---------|
| 3336 | 100 |
| 3332 | 90 |
| 3330 | 80 |
| 3327 | 75 |
| 3316 | 70 |
| 3301 | 65 |
| 3294 | 60 |
| 3291 | 55 |
| 3290 | 50 |
| 3288 | 45 |
| 3286 | 40 |
| 3279 | 35 |
| 3266 | 30 |
| 3254 | 25 |
| 3236 | 20 |
| 3212 | 15 |
| 3198 | 10 |
| 3112 | 5 |
| 2526 | 0 |

---

*文档生成日期: 2026-06-12*

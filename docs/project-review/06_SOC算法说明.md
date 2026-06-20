# 06 — SOC 算法说明

## 算法概述

SOC（State of Charge，剩余电量百分比 0-100%）采用**安时积分（库仑计数）+ 多源校准**的混合方法。

**核心源文件**:
- `SOC.c/.h` — 入口/接口层
- `SocEnhance.c/.h` — 核心算法层

**配置源文件**: `Project_Config.h:116-171`

## SOC 入口

### `InitData_SOC()`

**源文件**: `SOC.c:59`

在启动序列中调用，初始化 SOC 状态机并从 Flash 加载上次保存的 SOC 快照。

### `App_SOC()`

**源文件**: `SOC.c:64`

在 200ms 任务中调用。流程：
```
1. 检查是否有新的 AFE 电流采样
2. 计算净电流:
   net_current = I_chg - I_dsg - TypeC_equiv_current
3. 如果新数据: SOC_IntEnhance_Ctrl(net_current)
4. 否则: SOC_PublishReportData()
```

### 净电流计算

**源文件**: `SOC.c:47-56`

```
net_current_mA = report_ichg_mA - report_idsg_mA
               - SOC_GetTypeCBatEquivCurrentA10()  (TypeC 等效电流)
```

TypeC 等效电流考虑了 DCDC 效率：
```
TypeC_equiv = TypeC_out_mA × TypeC_voltage_mV × 1000
            / pack_voltage_mV / DCDC_efficiency_per_mille
```

> **效率问题**: 当前实现使用 `uint64_t` 除法（Cortex-M3 无硬件 64 位除法），建议重构为两次 32 位除法。

## 核心算法 (`SOC_IntEnhance_Ctrl()`)

**源文件**: `SocEnhance.c`

### SOC 运行模式

**源文件**: `SocEnhance.c`（SOC_MODE 枚举）

| 模式 | 条件 | 说明 |
|------|------|------|
| RELAX = 0 | 电流 < 阈值 | 静置模式，启用 OCV 校准 |
| CHG = 1 | 电流 > 0 | 充电模式 |
| DSG = 2 | 电流 < 0 | 放电模式 |

### 安时积分（库仑计数）

**周期**: 200ms

```
cap_now_as10 += net_current_mA × (200ms/3600s/1000ms) × 10
             = net_current_mA × 5.556e-4 × 10
```

每 200ms 累加当前 × 时间片。当累计放电量超过总容量时，循环计数器递增。

### SOC 百分比计算

```
SOC% = rem_mams × 100 / cap_full_as10
```

`rem_mams` 为剩余容量（毫安时 × 1000），`cap_full_as10` 为满充容量（安时 × 10）。

## 校准机制

### 1. 满充校准

**条件** (所有条件同时满足):
- Vmax ≥ (V_100 - 80mV) = 4100mV（默认）
- Vdelta (最大-最小) ≤ 120mV
- 条件持续 15 秒无中断

**动作**: 将 SOC 逐步提升至 100%（每步 1%，间隔 200ms）

**配置参数** (`Project_Config.h`):
| 参数 | 默认值 | 含义 |
|------|--------|------|
| `SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | 80mV | 满充确认电压余量 |
| `SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | 120mV | 满充确认最大压差 |
| `SOC_FULL_CONFIRM_SECONDS` | 15s | 满充确认持续时间 |

### 2. OCV 开路电压校准

**条件**:
- 处于 RELAX 模式
- Vmin 在有效范围 (2000mV ~ 5000mV)
- Sag 保持时间 > 30s（大电流放电后防回弹）
- 静置时间 > 3600s（1 小时）

**动作**: 通过 OCV 表将最小单体电压映射为 SOC 目标值，逐步趋近

**配置参数**:
| 参数 | 默认值 | 含义 |
|------|--------|------|
| `SOC_SAG_HOLDOFF_SECONDS` | 30s | 放电后 sag 保持时间 |
| `SOC_REST_OCV_SECONDS` | 3600s | 静置 OCV 等待时间 |
| `SOC_REST_DOWN_STEP_SECONDS` | 1800s | 长静置下行步进间隔 |

### 3. 低尾校准 (Empty Tail)

**条件**:
- Vmin ≤ (V_0 + 400mV) = 3400mV（默认）
- 不在满充校准中

**动作**: 根据 Vmin 距离空电电压的偏移量，计算目标 SOC 下行速率

**源文件**: `SocEnhance.c: soc_empty_tail_interpolate()`

```
Vmin 偏移量     →  放电目标 SOC   →  静置目标 SOC
    50mV             -1%/步           -1%/步
   100mV             -2%/步           -1%/步
   200mV             -3%/步           -2%/步
   300mV             -4%/步           -2%/步
   400mV             -6%/步           -3%/步
```

**配置参数**:
| 参数 | 默认值 | 含义 |
|------|--------|------|
| `SOC_EMPTY_TAIL_START_OFFSET_MV` | 400mV | 低尾校准启动偏移 |
| `SOC_CALIBRATION_STEP_PERCENT` | 1% | 每步最大调整量 |
| `SOC_CALIBRATION_MIN_CELL_VALID_MV` | 2000mV | 校准最低有效电压 |
| `SOC_CALIBRATION_MAX_CELL_VALID_MV` | 5000mV | 校准最高有效电压 |

### 4. 静置 OCV 漂移修正 (Rest OCV)

**条件**:
- `PROJECT_CFG_SOC_REST_OCV_ENABLE = 1`（已启用）
- 长期稳定静置（电流波动极小）
- 电压稳定在 ±5mV 范围内

**动作**: 每 1800s 将 SOC 向 OCV 表目标值移动一步

### 5. RTC 休眠补偿

**源文件**: `SocEnhance.c` — `SOC_ApplyRtcRelaxationCompensation()`

从 RTC 休眠唤醒后调用。根据休眠时长和电池电压变化修正 SOC。

## SOC 数据持久化

### Flash 存储

**地址**: `0x0801E000` (Slot A) / `0x0801E800` (Slot B)

**数据结构** (`Flash.h:61-75`):
```c
STORAGE_FLASH_SOC_DATA {
    UINT16 u16FormatVersion;      // V2 = 0x0002
    UINT16 u16SocNow;             // 当前 SOC (0-100)
    UINT16 u16DsgSocInt;          // 放电积分
    UINT16 u16MaxErrorPercent;
    UINT32 u32CycleTimes;         // 循环次数
    UINT32 u32CapNow;             // 当前容量 (Ah*10)
    UINT32 u32CapFull;            // 满充容量
    UINT32 u32LearnPassedAs10;    // 学习进度
    UINT16 u16LearnAnchorSoc;
    UINT16 u16LearnState;
    UINT16 u16Flags;
    UINT16 u16Reserved[4];
};
```

### 保存时机
- 休眠前: `SOC_SaveSnapshotBeforeSleep()`
- 参数保存: 随 `EEPROM_SaveRWParametersToFlash()` 一起

### V1 → V2 迁移

`StorageFlash_LoadSocData()` 兼容 V1 格式（22 字节）。检测到 V1 时自动迁移到 V2 结构。

## SOC 关键阈值

| 参数 | 默认值 | 源 |
|------|--------|-----|
| 满充电压 (V_100) | 4180mV | `OtherElement.u16SocV_100` |
| 空电电压 (V_0) | 3000mV | `OtherElement.u16SocV_0` |
| 电池容量 | 27Ah | `OtherElement.u16SocAh` |
| 板级自耗电 | 15mA | `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` |
| OCV 等待时间 | 3600s | `PROJECT_CFG_SOC_REST_OCV_SECONDS` |
| Sag 保持时间 | 30s | `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` |

## SOC 输出

`SOC_PublishReportData()` 将内部状态拷贝到 RS485 上报区：
```c
g_stCellInfoReport.SocElement.u16Soc              // SOC 百分比 (0-100)
g_stCellInfoReport.SocElement.u16Soh              // SOH 百分比 (0-100)
g_stCellInfoReport.SocElement.u16CapacityNow       // 当前容量 (Ah*100)
g_stCellInfoReport.SocElement.u16CapacityFull      // 满充容量 (Ah*100)
g_stCellInfoReport.SocElement.u16CapacityFactory   // 出厂容量 (Ah*100)
g_stCellInfoReport.SocElement.u16Cycle_times       // 循环次数
```

## 已知问题（来自 `todo.md` 开发笔记）

1. "soc100逻辑需要调整，要求不能太严，例如有时候满足不了电流和时间，充电到不了100"
2. "soc必须要保证能到0，同时要体验好，不能说到0后，实际电池还有很多电"
3. "soc融合逻辑，充电有点快，待仔细测试，确认soc计算频率"
4. "soc安时积分改成任意" — 意涵不明，待确认
5. "梳理行业保护板bms soc逻辑...该如何优化解决，或者完全重写soc模块"

## 待确认问题

1. OCV 表 (`soc_ocv_table`) 的具体内容（电压-SOC 映射点）
2. SOH 的计算方式（当前似乎未被主动更新）
3. 循环次数的计数逻辑（是仅计满充循环还是部分循环？）
4. `g_u16CalibCoefK` 是否影响 SOC 输入电流的精度

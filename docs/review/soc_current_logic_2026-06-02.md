# SOC 当前逻辑与校准策略梳理

文档状态：已按源码验证
源码验证日期：2026-06-02
验证范围：只读源码梳理，未修改 `.c/.h`、Keil 工程、协议、配置宏和测试脚本
未验证范围：未执行 Keil 编译、上板充放电、RTC STOP 功耗、CAN/Modbus 在线读取、ST-Link watch 实测
主要参考源码：`103 + 309/Project/Source/SOC.c`、`SocEnhance.c`、`SocEnhance.h`、`DataDeal.c`、`rtc_sleep.c`、`rtc_sleep_port.c`、`LowPowerSleep.c`、`LedBar.c`、`Sci_Upper.c`、`Flash.c`、`System_Monitor.c`、`conf/Project_Config.h`

## 1. 总结

当前 SOC 不是单一 OCV 表算法，而是以容量积分为主，叠加多种边界校准和显示平滑：

1. `s_soc.soc` 是内部估算 SOC。
2. `s_soc.display_soc` 是对外发布和 LedBar/CAN/Modbus 看到的显示 SOC。
3. `g_stCellInfoReport.SocElement.u16Soc` 当前发布的是 `display_soc`，不是内部 `s_soc.soc`。
4. 自动校准多数只允许每次 `1%` 步进，配置来自 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT`。
5. 静置 OCV 主要做下修，不做静置上修；满电锚点是主要上修到 100 的路径。
6. RTC STOP `HICCUP_MODE` 路径会在 RTC 周期唤醒时调用 SOC 休眠补偿；`NORMAL/DEEP` 复位式休眠当前只保存 snapshot 和 LedBar 快显 SOC，没有看到把复位休眠秒数带回 SOC 补偿的闭环。

按 `SOC_WATCH_CALIB_SOURCE` 调试枚举看，非 `NONE` 的校准来源有 15 个。按工程行为归并，当前实际可以分为 11 类策略：

| 类别 | 策略 | 是否自动 | 主要作用 |
|---|---|---|---|
| 1 | 启动 snapshot/default/OCV 初始化 | 自动 | 决定上电初值 |
| 2 | 充放电容量积分 | 自动 | 主 SOC 估算 |
| 3 | 板载自耗补偿 | 自动 | 无外部电流时扣自耗 |
| 4 | 满电锚点 | 自动 | 满电条件满足后按步进到 100 |
| 5 | 低压尾端下修 | 自动 | 低端防虚高，趋近 0 或低目标 |
| 6 | 中段尾端下修 | 自动 | V0 上方较宽区间防虚高 |
| 7 | 静置 OCV 目标锁存 | 自动 | 静置稳定后记录下修目标 |
| 8 | 放电期 deferred OCV 下修 | 自动 | 放电中慢慢消化静置目标 |
| 9 | 长静置下修 | 自动 | 长时间静置后继续按 OCV 目标下修 |
| 10 | RTC 休眠补偿 | 自动 | RTC STOP 周期内自耗+静置下修 |
| 11 | 上位机命令校准 | 人工/通信 | 手动 OCV、容量重算、一次设置 SOC |

## 2. 主调度链

主循环由 `Runtime_RunOnce()` 驱动。前段任务里先跑 LedBar，再跑 `App_AFEGet()`，这意味着普通运行中 LedBar 显示的是上一次已发布的 SOC，本轮 AFE/SOC 更新会在后续循环显示。

```text
main()
  AppInit_Boot()
    InitData_SOC()
      SOC_LoadConfigData()
      soc_param_lib_init()
      SOC_PublishReportData()
  while (1)
    Runtime_RunOnce()
      APP_LedBar()
      App_AFEGet()
        DataLoad_CellVolt()
        DataLoad_Current()
        ++g_u32AfeCurrentSampleSeq
        App_SOC()
          SOC_GetNetCurrentForCalc()
          SOC_UpdateSampleData()
          SOC_IntEnhance_Ctrl()
          SOC_PublishReportData()
```

关键源码证据：

- `DataDeal.c:1068-1090`：`App_AFEGet()` 每 200ms 采样电压、温度、电流，递增 `g_u32AfeCurrentSampleSeq` 后调用 `App_SOC()`。
- `SOC.c:116-140`：只有 `g_u32AfeCurrentSampleSeq` 变化时才执行 `SOC_IntEnhance_Ctrl()`，否则只重新发布当前 SOC 数据。
- `SOC.c:69-84`：SOC 计算电流会把 Type-C 输出折算成等效放电电流。
- `SocEnhance.c:1677-1722`：`SOC_IntEnhance_Ctrl()` 是核心状态机，顺序为命令处理、方向判断、积分、sag hold、低压/中段尾端、满空锚点、deferred OCV、静置计时、保存、发布。

## 3. 输入、状态与输出

### 3.1 输入

| 输入 | 来源 | 用途 |
|---|---|---|
| 单体最大/最小电压 | `g_stCellInfoReport.u16VCellMax/u16VCellMin` | OCV、满电、低端、中段、静置稳定、有效性判断 |
| 充电/放电电流 | `g_stCellInfoReport.u16Ichg/u16IDischg` | 判断方向、积分 |
| Type-C 输出电流 | `ADC_GetTypeCOutCurrentMilliAmp()` | 折算进放电电流 |
| 容量/循环/表选择/V0/V100 | `OtherElement` | 初始化容量、OCV 表、满空阈值 |
| Flash snapshot | `StorageFlash_LoadSocData()` | 上电恢复 SOC、容量、循环、rebound hold 标志 |
| RTC 休眠秒数 | `SOC_ApplyRtcRelaxationCompensation()` | HICCUP STOP 周期休眠补偿 |
| 功能开关 | `SystemFeature_IsSocFixed/Zero()` | 覆盖显示目标或阻塞命令 |

### 3.2 内部状态

核心状态在 `SocEnhance.c` 的 `SOC_STATE s_soc`：

| 字段 | 含义 |
|---|---|
| `cap_factory_as10` | 额定容量基础，来自 `u16_SOC_Ah`，内部换算为容量积分单位 |
| `cap_full_as10` | 按 SOH 修正后的满容量 |
| `cap_now_as10` | 当前剩余容量 |
| `cycle_x100` | 循环次数扩大 100 倍 |
| `dsg_acc_as10` | 放电累计，用于循环计数 |
| `rest_ticks/stable_rest_ticks/short_rest_ticks/long_rest_down_ticks` | 静置 OCV 相关计时 |
| `full_ticks/empty_ticks/mid_ticks` | 满电、低端、中段修正计时 |
| `sag_hold_ticks` | 大电流放电后的电压回弹保护计时 |
| `deferred_ocv_target/valid/ticks` | 静置 OCV 延迟下修目标 |
| `soc` | 内部估算 SOC |
| `display_soc` | 对外显示 SOC |
| `mode` | 当前 SOC 模式：relax/charge/discharge |

### 3.3 输出

`soc_publish()` 把显示 SOC 和容量信息写入 `SOC_Enhance_Element`，随后 `SOC_PublishReportData()` 发布到 `g_stCellInfoReport.SocElement`。

| 输出 | 当前口径 |
|---|---|
| `g_stCellInfoReport.SocElement.u16Soc` | `s_soc.display_soc` |
| `u16Soh` | cycle-based SOH |
| `u16CapacityNow` | 当前容量，单位 Ah * 100 |
| `u16CapacityFull` | SOH 修正满容量，单位 Ah * 100 |
| `u16CapacityFactory` | 额定容量，单位 Ah * 100 |
| `u16Cycle_times` | `cycle_x100 / 100` |

源码证据：`SocEnhance.c:1513-1584`、`SocEnhance.c:562-570`。

## 4. 通用有效性与阻塞条件

多数 OCV/尾端/满电校准会先经过 `soc_calibration_allowed()`：

| 条件 | 当前配置/逻辑 |
|---|---|
| 电压有效范围 | `Vmin/Vmax` 都必须在 `2000..5000mV`，且 `Vmax >= Vmin` |
| 单体压差 | `Vmax - Vmin <= 1000mV` |
| 保护 fault 阻塞 | `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT = 0`，当前不阻塞 |
| 系统 fault 阻塞 | `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT = 0`，当前不阻塞 |
| sag hold 阻塞 | `sag_hold_ticks > 0` 且电压仍高于 `V0 + 50mV` 时阻塞 OCV/尾端校准 |

源码证据：`Project_Config.h:137-156`、`SocEnhance.c:353-418`、`SocEnhance.c:981-1015`。

## 5. 当前配置参数

| 参数 | 当前值 | 作用 |
|---|---:|---|
| `PROJECT_CFG_BAT_CHEMISTRY` | `0` | 当前编译选择三元锂 OCV 表 |
| `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE` | `0` | 上位机运行时写 SOC 表关闭 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | `80mV` | 满电普通确认 Vmin/Vmax margin |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | `120mV` | 满电允许最大压差 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS` | `15s` | 满电普通确认时间 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS` | `5s` | 满电快速确认时间 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT` | `95%` | 普通满电确认最低内部 SOC |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV` | `30mV` | 快速满电确认 margin |
| `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | `2000mV` | 校准有效电压下限 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | `5000mV` | 校准有效电压上限 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV` | `1000mV` | 校准有效压差上限 |
| `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` | `30s` | 大电流 sag holdoff |
| `PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV` | `50mV` | sag hold 阻塞释放参考 |
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | `1800s` | 静置 OCV wait/长静置门槛 |
| `PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS` | `300s` | 静置稳定最短时间 |
| `PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS` | `600s` | 静置锁存/放电消化目标周期 |
| `PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS` | `1800s` | 长静置下修周期 |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | `1%` | 单次最大校准步长 |
| `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` | `30mA` | 板载自耗补偿 |
| `PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV` | `400mV` | 低压尾端启动区间 |
| `PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT` | `0%` | 当前不抬高低端目标 |
| `PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT` | `100%` | 当前不缩放低端计时 |
| `PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS` | `5s/%` | 普通显示平滑 |
| `PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS` | `5s/%` | 充电上升显示平滑 |
| `PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS` | `1s/%` | 低压下降显示平滑 |
| `PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV` | `50mV` | 低压显示加速阈值 |
| `PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV` | `50mV` | 低于 V0 后显示每 tick 快速下降 |

源码证据：`Project_Config.h:15-21`、`Project_Config.h:117-198`。

## 6. 校准策略明细

### 6.1 启动 snapshot/default/OCV 初始化

入口：`soc_param_lib_init()` -> `soc_load_or_default()`。

| 分支 | 条件 | 行为 |
|---|---|---|
| `STARTUP_SNAPSHOT` | Flash V2/V1 snapshot 有效，`u16SocNow <= 100` 且 `u16DsgSocInt <= 100` | 恢复 cycle、容量、放电累计、rebound hold 标志；优先用 `u32CapNow` 反算 SOC，否则用 `u16SocNow` |
| `STARTUP_OCV` | snapshot 无效，且 `soc_calibration_allowed()` | 用当前 `VCellMin` 查 OCV 表作为启动 SOC |
| `STARTUP_DEFAULT` | snapshot 无效，且校准不允许 | 使用 `SOC_DEFAULT_STARTUP_PERCENT = 60` |

时间参数：无持续时间，初始化时一次完成。

风险点：如果复位式休眠前 snapshot 没及时保存，启动可能回到旧 snapshot；当前 `LowPowerSleep_SaveCoreState()` 会在休眠前调用 `SOC_SaveSnapshotBeforeSleep()`。

源码证据：`SocEnhance.h:16`、`SocEnhance.c:616-675`、`LowPowerSleep.c:5-15`。

### 6.2 充放电容量积分

入口：`SOC_IntEnhance_Ctrl()` 每个新 AFE 样本调用 `soc_integrate()`。

条件：

- `SOC_MODE_CHG`：`Ichg >= 2A*10` 且 `Ichg >= Idsg`。
- `SOC_MODE_DSG`：`Idsg >= 2A*10`。
- 其余为 relax。

行为：

- 充电时增加 `cap_now_as10`。
- 放电时减少 `cap_now_as10`，并累计 `dsg_acc_as10` 用于循环次数。
- 电流单位内部按 mA 处理；`SOC_TICK_MS = 200ms`。
- 充电积分未经过满电锚点前，SOC 达到 100 会被压到 99，避免未确认满电直接显示 100。

时间参数：

- 调度周期：200ms。
- SOC 每变化 1% 的时间取决于容量和电流，不是固定秒数。

源码证据：`SOC.c:116-140`、`SocEnhance.c:311-350`、`SocEnhance.c:754-813`。

### 6.3 板载自耗补偿

入口有两条：

1. 普通运行 `soc_integrate_current_ma()` 在 relax 下返回 `-30mA`，但 `soc_integrate()` 把 relax 视为不积分，因此普通 relax 周期没有直接扣自耗。
2. RTC 休眠补偿 `soc_apply_board_self_consumption_seconds()` 明确按休眠秒数扣 `30mA`。

当前实际有效路径主要是 RTC 补偿。

条件：

- `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA != 0`。
- `seconds != 0`。

行为：

- 按 `30mA * seconds` 换算容量减少。
- 如果 SOC 改变，调试来源为 `BOARD_SELF_CONSUMPTION`；在 `SOC_ApplyRtcRelaxationCompensation()` 外层也可能记录为 `RTC_REST`。

时间参数：

- RTC STOP 每次唤醒累计 `rest_seconds` 的增量。

源码证据：`Project_Config.h:173-174`、`SocEnhance.c:325-338`、`SocEnhance.c:815-845`、`rtc_sleep.c:256-268`。

### 6.4 满电锚点

入口：`soc_apply_full_empty()` 内部先处理满电。

条件：

- 当前模式不是放电。
- `soc_calibration_allowed()` 通过。
- `VCellMax > 4180mV` 且 `VCellMax >= V100 - 80mV`。
- 快速确认：`VCellMin >= V100 - 30mV` 且压差 `<= 120mV`。
- 普通确认：内部 SOC `>= 95%`，`VCellMin >= V100 - 80mV` 且压差 `<= 120mV`。

行为：

- 满足快速确认后，持续 `5s`，按 `1%` 步进靠近 100。
- 满足普通确认后，持续 `15s`，按 `1%` 步进靠近 100。
- 每次到确认时间只推进一次，直到 SOC 到 100。
- 达到 100 后 `full_anchor = 1`。

源码证据：`Project_Config.h:119-135`、`SocEnhance.c:924-952`、`SocEnhance.c:1269-1308`。

### 6.5 低压尾端下修

入口：`soc_low_tail_config()` -> `soc_empty_tail_config()` -> `soc_apply_full_empty()` 的 empty 分支。

条件：

- 当前模式不是充电。
- 电压有效。
- sag hold 不阻塞。
- `VCellMin <= V0 + 400mV`，默认 `V0` 若未配置则为 `3000mV`。
- 根据放电强度分 band：
  - relax
  - light：`Idsg <= CapacityAh10 / 5`
  - mid：`Idsg <= CapacityAh10 / 2`
  - heavy：更大放电

行为：

- 从 `s_empty_tail_table` 查目标 SOC 和计时。
- 每达到对应 ticks，按 `1%` 下修到目标。
- 低压区越低，目标越低、计时越短。

当前低压尾端表：

| Vmin 相对 V0 offset | relax 目标/ticks | light 目标/ticks | mid 目标/ticks | heavy 目标/ticks |
|---:|---:|---:|---:|---:|
| -50mV | 0 / 1 | 0 / 1 | 0 / 1 | 0 / 1 |
| -25mV | 0 / 5 | 0 / 5 | 0 / 1 | 0 / 1 |
| 0mV | 0 / 10 | 0 / 5 | 0 / 5 | 0 / 5 |
| +50mV | 4 / 20 | 5 / 15 | 8 / 10 | 12 / 8 |
| +100mV | 8 / 35 | 10 / 30 | 14 / 25 | 18 / 20 |
| +200mV | 12 / 60 | 14 / 50 | 20 / 40 | 25 / 30 |
| +300mV | 14 / 90 | 18 / 75 | 25 / 60 | 32 / 45 |
| +400mV | 18 / 120 | 22 / 100 | 30 / 80 | 40 / 60 |

时间换算：1 tick = 200ms。比如 ticks=10 约 2s，ticks=120 约 24s。

源码证据：`SocEnhance.c:177-186`、`SocEnhance.c:954-971`、`SocEnhance.c:1105-1226`、`SocEnhance.c:1269-1321`。

### 6.6 中段尾端下修

入口：`soc_mid_tail_config()` -> `soc_apply_mid_tail()`。

条件：

- 当前模式不是充电。
- 电压有效。
- 单体压差 `<= 200mV`。
- sag hold 不阻塞。
- `VCellMin > V0 + 400mV`。
- 命中 `s_mid_tail_table`。
- 仅当当前 SOC 高于目标时才下修。

当前中段尾端表：

| Vmin 相对 V0 offset | relax 目标/ticks | light 目标/ticks | mid 目标/ticks | heavy |
|---:|---:|---:|---:|---:|
| +500mV | 25 / 450 | 32 / 450 | 42 / 600 | disabled |
| +600mV | 35 / 600 | 42 / 600 | 50 / 750 | disabled |
| +650mV | 45 / 750 | 50 / 750 | 58 / 900 | disabled |
| +700mV | 55 / 900 | 60 / 900 | disabled | disabled |

时间换算：450 ticks 约 90s，900 ticks 约 180s。

源码证据：`SocEnhance.c:188-193`、`SocEnhance.c:1228-1267`。

### 6.7 静置 OCV 目标锁存

入口：`soc_update_rest_timer()`。

条件：

- 当前模式必须是 relax。
- 校准允许。
- 单体压差 `<= 200mV`。
- sag hold 不阻塞。
- 电压稳定：当前 `Vmin/Vmax` 与静置参考差值都 `<= 30mV`。
- `stable_rest_ticks >= 300s`。
- `short_rest_ticks >= 600s`。

行为：

- 只锁存目标，不立即改变 SOC。
- 目标来自 `VCellMin` 查 OCV 表。
- 只有当 OCV target 低于当前 `s_soc.soc` 时才设置 `deferred_ocv_target`。
- OCV target 高于等于当前 SOC 时清除 deferred 目标，所以静置 OCV 不上修。

源码证据：`Project_Config.h:158-168`、`SocEnhance.c:722-751`、`SocEnhance.c:1373-1440`。

### 6.8 放电期 deferred OCV 下修

入口：`soc_apply_deferred_ocv_step()`。

条件：

- 已有 `deferred_ocv_valid`。
- 当前模式不是 relax。
- 目标低于当前 SOC。
- 只有放电模式允许下修；如果不是放电则清除 deferred 目标。
- 每 `600s` 尝试一次。

行为：

- 每次按 `1%` 向 deferred target 下修。
- 到达目标后清除 deferred 目标。

源码证据：`SocEnhance.c:881-922`、`SocEnhance.c:1705-1708`。

### 6.9 长静置下修

入口：`soc_apply_long_rest_down_step()`，在普通静置和 RTC rest 中都会调用。

条件：

- 已有 `deferred_ocv_valid`。
- deferred target 低于当前 SOC。
- `rest_ticks >= 1800s`。
- `long_rest_down_ticks >= 1800s`。

行为：

- 在 relax 模式下也允许每 `1800s` 按 `1%` 向 OCV target 下修。

源码证据：`Project_Config.h:158-168`、`SocEnhance.c:1333-1371`、`SocEnhance.c:1439`、`SocEnhance.c:1509`。

### 6.10 RTC 休眠补偿

入口：`rtc_sleep_run_hiccup_cycle()` -> `RtcSleep_PortApplySocRtcRest()` -> `SOC_ApplyRtcRelaxationCompensation()`。

条件：

- 仅 RTC STOP `HICCUP_MODE` 周期唤醒路径明确调用。
- RTC 唤醒且 `isException()` 为 false。
- 复位式 `NORMAL/DEEP` 休眠当前只保存 snapshot 和 LedBar 快显 SOC，没有看到启动后按休眠秒数调用 `SOC_ApplyRtcRelaxationCompensation()`。

行为：

- 使用累计 `s_u32RtcSleepElapsedSeconds`。
- 先按板载自耗扣容量。
- 再按静置稳定逻辑累计 `rest/stable/short/long` 计时。
- 满足静置条件后锁存 OCV 下修目标，并通过长静置下修慢慢消化。
- 如果 SOC 改变，保存 snapshot。

显示顺序：

- RTC 周期醒来时先补偿 SOC，再继续 STOP。
- 最终按键唤醒时才请求 LedBar 显示，所以 HICCUP 路径不是“先显示后校准”。
- 复位式休眠启动快显显示的是 BKP 中睡前保存的显示 SOC。

源码证据：`rtc_sleep.c:247-286`、`rtc_sleep_port.c:107-112`、`SocEnhance.c:1461-1510`、`SocEnhance.c:1725-1745`、`LowPowerSleep.c:5-15`、`LedBar.c:1175-1218`。

### 6.11 上位机命令校准

入口：`SOC_Enhance_Element.u16_RefreshData_Flag`。

| flag | 来源 | 行为 | 阻塞 |
|---:|---|---|---|
| 1 | 手动 OCV refresh | 调用 `soc_apply_rest_ocv(1800s, soc_direction())`，只允许 OCV target 低于当前 SOC 时按 `1%` 修正 | `SystemFeature_IsSocFixed()` 会阻塞 |
| 2 | SOC 容量参数变化副作用 | 重新计算额定容量、cycle、SOH、满容量，并保持原 SOC 比例 | `SystemFeature_IsSocZero()` 会阻塞 |
| 3 | `0x06 SetSocOnce` | 直接 `soc_set(value)`，合法范围 `0..100` | 输入大于 100 返回 NEG |

命令处理后会 `soc_publish(1)`，强制显示 SOC 跟随内部目标，可能造成用户可见跳变。

源码证据：`SocEnhance.h:80-95`、`Sci_Upper.c:600-640`、`Sci_Upper.c:2052-2066`、`SocEnhance.c:1587-1638`。

## 7. 显示平滑与用户体验

`soc_publish(force_display)` 的规则：

1. `force_display = 1` 或首次发布时，`display_soc = target`，不平滑。
2. `SOC fixed` 功能开启时，对外目标固定为 `60%`。
3. `SOC zero` 功能开启时，对外目标固定为 `0%`。
4. 普通显示变化：每 `5s` 变化 `1%`。
5. 充电上升：每 `5s` 变化 `1%`。
6. 低压下降：当目标低于显示值且 `Vmin <= V0 + 50mV` 时，每 `1s` 下降 `1%`。
7. 若 `Vmin <= V0 - 50mV`，低压下降每个 SOC tick 都可推进，约 `200ms/%`。

用户体验结论：

- 自动校准多数通过 `soc_publish(0)` 发布，会被显示平滑吸收。
- 命令校准、初始化、参数重载使用 `soc_publish(1)`，可能立即跳变。
- 复位式休眠早期快显来自 BKP 保存值，后续正常启动后使用 snapshot/display 口径，存在前后不一致风险。

源码证据：`SocEnhance.c:1513-1584`、`System_Monitor.c:191-199`、`LedBar.c:1175-1218`。

## 8. SOC 存储与休眠保存

SOC snapshot 保存内容包括：

- `soc`
- `cycle_x100`
- `cap_now_as10`
- `cap_full_as10`
- `dsg_acc_as10`
- rebound hold flag

保存触发：

1. SOC、cycle、满容量或 snapshot flags 相比 `s_saved_soc` 变化时保存。
2. 休眠前 `SOC_SaveSnapshotBeforeSleep()` 调用 `soc_save_if_needed()`。
3. RTC rest 补偿改变 SOC 后立即保存。
4. 上位机命令改变 SOC/容量后保存。

Flash 使用 `StorageFlash_SaveJournalPair()` 双槽 journal 保存，加载兼容 V1/V2。

源码证据：`SocEnhance.c:572-614`、`SocEnhance.c:1672-1675`、`SocEnhance.c:1725-1745`、`Flash.c:739-799`、`LowPowerSleep.c:5-15`。

## 9. OCV 表与当前电池类型

当前 `PROJECT_CFG_BAT_CHEMISTRY = 0`，且 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE = 0`，因此编译期选择 `SocTable_TernaryLi`，上位机运行时 SOC 表不参与当前量产算法。

当前三元锂表大致范围：

- `4160mV -> 100%`
- `4100mV -> 95%`
- `3670mV -> 45%`
- `3400mV -> 10%`
- `3000mV -> 0%`

查表使用 `VCellMin` 做线性插值，超出表区间时按端点处理。

源码证据：`Project_Config.h:15-21`、`SocEnhance.c:204-210`、`SocEnhance.c:420-502`。

## 10. 当前判断与后续优化边界

当前 SOC 校准“实际有用”，但不是所有路径都有同样效果：

1. 充放电积分是主路径，实际有效。
2. 满电锚点、低压尾端、中段尾端都是实际有效的自动修正。
3. 静置 OCV 很保守，主要用于下修；短时间看不到明显变化是符合源码逻辑的。
4. RTC STOP `HICCUP_MODE` 有休眠补偿闭环；复位式 `NORMAL/DEEP` 当前缺少休眠秒数回灌到 SOC 补偿的闭环。
5. 用户体验上，自动校准大多被 display smoothing 吸收；但启动、命令、参数重载和复位式休眠快显仍可能产生立即跳变或前后不一致。

建议下一步先做低风险优化设计，不立即改源码：

1. 把 `real_soc` 与 `display_soc` 的对外口径写成明确需求：用户可见显示、CAN、Modbus 默认只发 `display_soc`；调试 watch 才看内部 `real_soc`。
2. 复位式休眠若要做 RTC SOC 补偿，必须先明确休眠秒数来源和显示策略，避免启动快显后马上跳变。
3. 上位机命令 `SetSocOnce`/手动 OCV/容量重算是否允许强制跳变，需要单独确认。
4. 后续源码优化应优先减少散落状态和命令 shadow，但不能改变上述校准顺序、时间参数和低端安全策略。

# SystemDebug 调试监控使用指南

> 功能: `SystemDebug` — Keil Watch 全局状态快照
> 版本: v1.0 2026-06-01
> 控制宏: `PROJECT_CFG_DEBUG_MONITOR_ENABLE` (0=Release, 1=Debug)

---

## 1. 概述

`g_dbg` 是一个全局结构体，200ms 更新一次，把所有 IO 状态、外设状态、功能状态、运行计数器拍成一张快照。在 Keil Watch 窗口添加 `g_dbg` 即可展开查看所有字段。

**零开销**: `PROJECT_CFG_DEBUG_MONITOR_ENABLE=0` 时，整个模块编译为空，不占 Flash 也不占 RAM。

## 2. 启用方法

1. 打开 `conf/Project_Config.h`
2. 找到 Feature Switches 组的 `PROJECT_CFG_DEBUG_MONITOR_ENABLE`
3. 设为 `1`
4. 重新编译，烧录
5. Keil 进入 Debug → Watch 窗口 → 添加 `g_dbg`

## 3. 字段清单 (86 字段，12 组)

### GPIO 状态 (16 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `gpioA_in` | uint16 | PA0~PA15 输入状态 (IDR) |
| `gpioB_in` | uint16 | PB0~PB15 输入状态 (IDR) |
| `gpioA_out` | uint16 | PA0~PA15 输出状态 (ODR) |
| `gpioB_out` | uint16 | PB0~PB15 输出状态 (ODR) |
| `chg_in` | uint8 | PA0 — 充电检测 (1=充电器插入) |
| `sw_key` | uint8 | PA9 — 用户按键 (1=按下, 低有效) |
| `mcu_wk` | uint8 | PB13 — MCU 唤醒信号 |
| `cmnt_en` | uint8 | PB4 — CAN 收发器供电 (1=上电) |
| `dc_en` | uint8 | PA10 — DC 使能 |
| `dbg_led` | uint8 | PB15 — 调试 LED |
| `afe_ctlc` | uint8 | PB14 — AFE CTLC 控制 |
| `afe_pro_en` | uint8 | PB0 — AFE 保护使能 |
| `m_stb` | uint8 | PA15 — 主电源待机 |
| `ad_en` | uint8 | PB3 — AD 使能 |
| `adc_bus_en` | uint8 | PB5 — ADC 总线使能 |
| `_2727_en` | uint8 | PA3 — 升压使能 |

### MOS 状态 (4 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `mos_chg` | uint8 | 充电 MOS 软件状态 (1=开) |
| `mos_dsg` | uint8 | 放电 MOS 软件状态 (1=开) |
| `afe_dsg_fet` | uint8 | AFE 硬件 DSG_FET 状态 (BSTATUS3.bit4) |
| `afe_chg_fet` | uint8 | AFE 硬件 CHG_FET 状态 (BSTATUS3.bit3) |

> **调试技巧**: `mos_dsg != afe_dsg_fet` 说明软件层和 AFE 硬件 MOS 状态不一致，可能是保护逻辑或软件 bug。

### 系统状态 (4 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `sys_status` | uint32 | System_Status 快照 (含 MOS 状态、AFE 状态、ToSleep 等) |
| `sys_feature` | uint32 | System_OnOFF_Function 快照 (功能开关) |
| `sys_err_lo` | uint16 | System_ErrFlag 前 16 字节 (AFE1,AFE2,CAN,EEPROM...) |
| `sys_err_hi` | uint16 | System_ErrFlag 后 16 字节 |

### CAN 状态 (8 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `can_bus_active` | uint8 | CAN 总线上是否有其他设备 (1=有) |
| `can_power_on` | uint8 | CAN 收发器供电状态 (1=上电) |
| `can_bus_off` | uint8 | CAN 控制器 BUS-OFF 状态 (1=离线) |
| `can_no_ack_cnt` | uint8 | 连续无 ACK 计数 (≥3 退避) |
| `can_tx_queue` | uint8 | TX 队列当前长度 (0~32) |
| `can_probe` | uint8 | 探测模式 (1=只发探测帧) |
| `can_rtc_svc` | uint8 | RTC 唤醒 CAN 服务进行中 |
| `can_esr` | uint16 | CAN ESR 错误状态寄存器 |

> **调试技巧**: CAN 收不到帧时先看 `can_bus_off` 和 `can_no_ack_cnt`。
> `can_power_on=0` 且 `can_tx_queue>0` 说明收发器供电有问题。

### RTC / 低功耗 (6 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `lp_mode` | uint8 | 休眠模式: 0=NORMAL, 1=HICCUP, 2=DEEP, 3=NO_SLEEP |
| `lp_ready` | uint8 | 准备休眠标志 (1=可进入) |
| `lp_block_reason` | uint8 | 阻塞原因码 (见枚举 `LOW_POWER_RTC_BLOCK_REASON`) |
| `lp_block_mask` | uint32 | 阻塞原因位掩码 (见 `LP_BLOCK_*`) |
| `lp_sleep_sec` | uint32 | 上次休眠秒数 |
| `lp_elapsed_sec` | uint32 | 累计休眠秒数 |

> **调试技巧**: 进不了低功耗时先看 `lp_block_reason` 和 `lp_block_mask`。
> `lp_block_mask` 逐位对应的阻塞原因见 `app_lowpower.h`。

### ADC (6 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `adc_mos_temp` | uint16 | MOS 温度 (查表后值, °C×10+400) |
| `adc_typec_cur_ma` | uint16 | TypeC 输出电流 mA |
| `adc_vbat_mv` | uint32 | 电池总压 mV |
| `adc_raw_vbus` | uint16 | VBUS ADC DMA 原始值 (PA1) |
| `adc_raw_cur` | uint16 | TypeC 电流 ADC DMA 原始值 (PA2) |
| `adc_raw_mos` | uint16 | MOS 温度 ADC DMA 原始值 (PB1) |

### SOC (10 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `soc_pct` | uint8 | 当前 SOC % (0~100) |
| `soh_pct` | uint8 | 当前 SOH % |
| `soc_cap_now` | uint16 | 当前容量 Ah×100 |
| `soc_vmax` | uint16 | 最高电芯电压 mV |
| `soc_vmin` | uint16 | 最低电芯电压 mV |
| `soc_ichg` | uint16 | 充电电流 A×10 |
| `soc_idsg` | uint16 | 放电电流 A×10 |
| `soc_init_over` | uint8 | SOC 初始化完成标志 |
| `soc_ocv_cali` | uint8 | OCV 校准标志 |
| `soc_vtotal` | uint16 | 总压 V×100 |

### AFE (8 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `afe_bstatus1` | uint8 | SH367309 BSTATUS1 (OV,UV,OCD1,OCD2,OCC,SC,PF,WDT) |
| `afe_bstatus3` | uint8 | SH367309 BSTATUS3 (CHG_FET,DSG_FET,PCHG_FET,L0V,EEPR_WR) |
| `afe_fault1` | uint8 | BSTATUS1 副本 (作为故障判断) |
| `afe_cur_raw` | uint16 | AFE 电流寄存器原始值 mA |
| `afe_pec_err` | uint16 | PEC 通信错误累计计数 |
| `afe_cell_min_mv` | uint16 | 最低电芯电压 mV |
| `afe_cell_max_mv` | uint16 | 最高电芯电压 mV |

> **调试技巧**: `afe_pec_err` 持续增长说明 I2C 通信有问题。
> `afe_bstatus1` 各 bit 含义见 SH367309 数据手册。

### 故障 (4 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `fault_first` | uint16 | 一级故障标志 (Fault_Flag_Fisrt.all) |
| `fault_third` | uint16 | 三级故障标志 (Fault_Flag_Third.all) |
| `fault_mdl1` | uint16 | 模型一级故障 (unMdlFault_First.all) |
| `fault_mdl3` | uint16 | 模型三级故障 (unMdlFault_Third.all) |

### 工厂老化 (2 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `aging_state` | uint8 | 老化状态: 0=STOPPED, 1=RUNNING, 2=DONE |
| `aging_remain_sec` | uint32 | 老化剩余秒数 |

### Flash (3 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `flash_update_flag` | uint8 | 请求进 IAP 标志 |
| `flash_e2prom_flag` | uint8 | 请求写 Flash 参数标志 |
| `flash_busy` | uint8 | Flash 操作忙 (1=忙) |

> **调试技巧**: `flash_busy=1` 时系统不会进入低功耗。

### 运行计数器 (10 字段)

| 字段 | 类型 | 说明 |
|------|------|------|
| `main_cycle` | uint32 | 主循环计数 (64位截低32位) |
| `afe_get_cnt` | uint32 | AFE 数据获取次数 |
| `can_rcv_cnt` | uint32 | CAN 接收帧计数 |
| `rtc_sleep_cnt` | uint32 | RTC 休眠次数 |
| `rtc_sec_cnt` | uint32 | RTC 秒中断计数 |
| `rtc_alm_cnt` | uint32 | RTC 闹钟中断计数 |
| `sci1_irq_cnt` | uint32 | 串口1 中断计数 |
| `pa0_irq_cnt` | uint16 | PA0 (充电) 中断计数 |
| `key_irq_cnt` | uint16 | 按键中断计数 |
| `tick_10ms` | uint32 | 10ms 时基 tick 计数 |

---

## 4. 常见调试场景

### 场景1: CAN 通信异常

```
看: can_bus_active, can_bus_off, can_no_ack_cnt, can_esr, can_power_on, can_tx_queue
- can_bus_off=1 → CAN 控制器离线，等待自动恢复 (ABOM)
- can_no_ack_cnt≥3 → 连续无 ACK，已进入退避模式
- can_power_on=0, can_tx_queue>0 → 收发器供电异常
- can_esr 中 LEC≠0 → 查看具体错误码
```

### 场景2: 不进低功耗

```
看: lp_mode, lp_block_reason, lp_block_mask
- lp_block_mask bit0 → 充电电流 >10mA
- lp_block_mask bit1 → 放电电流 >10mA
- lp_block_mask bit2 → 串口/CAN 通讯忙
- lp_block_mask bit3 → MCU_WK 高 (按键活动)
- lp_block_mask bit8 → LED 显示中
- lp_block_mask bit9 → 唤醒周期超过 IWDG
```

### 场景3: MOS 状态异常

```
看: mos_chg, mos_dsg, afe_dsg_fet, afe_chg_fet
- mos_dsg=1, afe_dsg_fet=0 → 软件开了但 AFE 硬件没开 → 检查 AFE 保护状态
- mos_dsg=0, afe_dsg_fet=1 → AFE 开了但软件认为关着 → 检查 SystemRuntime_SetMosStatus 调用
- 再看 fault_third/fault_mdl3 → 哪条保护触发了
```

### 场景4: SOC 异常

```
看: soc_pct, soc_vmax, soc_vmin, soc_ichg, soc_idsg, soc_ocv_cali, soc_init_over
- soc_init_over=0 → SOC 未完成初始化，可能是首次上电或快照丢失
- soc_ocv_cali=0 且长时间静置 → 未进入 OCV 校准，检查 vmin/vmax 是否在校准窗口内
```

### 场景5: GPIO 异常

```
看: gpioA_in, gpioA_out, gpioB_in, gpioB_out + 对应单字段
- 对比 ODR 和预期值: 如 cmnt_en 应为 1 但 gpioB_out bit4=0 → 配置被其他代码覆盖
- 对比 IDR 和预期值: 如 chg_in 应为 1 但 gpioA_in bit0=0 → 外部电路问题
```

---

## 5. 扩展方法

如需增加新字段:

1. 在 `SystemDebug.h` 的 `struct SYSTEM_DEBUG` 中增加字段
2. 在 `SystemDebug.c` 的 `SystemDebug_Snapshot()` 中填充
3. Release 编译不参与，无需顾虑代码体积

---

## 6. 相关文件

| 文件 | 作用 |
|------|------|
| `Source/SystemDebug.h` | `g_dbg` 结构体定义 + 宏开关 |
| `Source/SystemDebug.c` | `SystemDebug_Snapshot()` 实现 |
| `Source/Runtime.c` | 调用点 (Runtime_RunFrontTasks 末尾) |
| `Source/Can_HDX.c` | `Can_GetDebugSnapshot()` — CAN 状态暴露 |
| `conf/Project_Config.h` | `PROJECT_CFG_DEBUG_MONITOR_ENABLE` 宏 |

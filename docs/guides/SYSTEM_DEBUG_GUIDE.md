# SystemDebug 调试监控使用指南 v2.0

> 功能: `SystemDebug` — Keil Watch 全局状态快照
> 版本: v2.0 2026-06-01 (子结构体重组 + printf 完善)
> 控制宏: `PROJECT_CFG_DEBUG_MONITOR_ENABLE` (0=Release, 1=Debug)

## 1. 概述

`g_dbg` 是一个全局结构体，200ms 更新一次，把所有 IO 状态、外设状态、功能状态、运行计数器拍成快照。

**v2.0 改进**: 14 个子结构体替代 108 个平铺字段。Keil Watch 中按需展开目标分组，无需在百级字段中翻找。

**零开销**: `PROJECT_CFG_DEBUG_MONITOR_ENABLE=0` 时，整个模块编译为空，不占 Flash/RAM。

## 2. 启用方法

1. `conf/Project_Config.h` → `PROJECT_CFG_DEBUG_MONITOR_ENABLE` → 设为 `1`
2. 重新编译，烧录
3. Keil → Debug → Watch 窗口 → 添加 `g_dbg`
4. 展开子结构体查看，例如 `g_dbg.gpio` → 展开看所有 IO

## 3. 结构体层级（14 组）

### g_dbg.gpio — GPIO 状态 (16 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `a_in` | uint16 | GPIOA IDR |
| `b_in` | uint16 | GPIOB IDR |
| `a_out` | uint16 | GPIOA ODR |
| `b_out` | uint16 | GPIOB ODR |
| `chg_in` | uint8 | PA0 充电检测 (1=插入) |
| `sw_key` | uint8 | PA9 按键 (1=按下) |
| `mcu_wk` | uint8 | PB13 MCU唤醒 |
| `cmnt_en` | uint8 | PB4 CAN收发器供电 |
| `dc_en` | uint8 | PA10 DC使能 |
| `dbg_led` | uint8 | PB15 调试LED |
| `afe_ctlc` | uint8 | PB14 AFE CTLC |
| `afe_pro_en` | uint8 | PB0 AFE保护使能 |
| `m_stb` | uint8 | PA15 主电源待机 |
| `ad_en` | uint8 | PB3 AD使能 |
| `adc_bus_en` | uint8 | PB5 ADC总线使能 |
| `_2727_en` | uint8 | PA3 升压使能 |

### g_dbg.mos — MOS 状态 (4 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `sw_chg` | uint8 | 充电MOS 软件状态 |
| `sw_dsg` | uint8 | 放电MOS 软件状态 |
| `hw_dsg_fet` | uint8 | AFE DSG_FET 硬件实际 |
| `hw_chg_fet` | uint8 | AFE CHG_FET 硬件实际 |

> `sw_dsg != hw_dsg_fet` → 软件层和硬件不一致，排查保护逻辑

### g_dbg.sys — 系统状态 (4 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `status` | uint32 | System_Status 快照 |
| `feature` | uint32 | 功能开关掩码 |
| `err_lo` | uint16 | 错误标志前16字节 |
| `err_hi` | uint16 | 错误标志后16字节 |

### g_dbg.can — CAN 状态 (13 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `bus_active` | uint8 | 总线上有设备 |
| `power_on` | uint8 | 收发器供电 |
| `bus_off` | uint8 | BUS-OFF状态 |
| `no_ack_cnt` | uint8 | 连续无ACK数 |
| `tx_queue` | uint8 | TX队列长度 |
| `probe` | uint8 | 探测模式 |
| `rtc_svc` | uint8 | RTC唤醒服务中 |
| `esr` | uint16 | CAN ESR寄存器 |
| `tx_ok_cnt` | uint16 | 发送成功计数 |
| `tx_fail_cnt` | uint16 | 发送失败计数 |
| `busoff_in_cnt` | uint16 | BUS-OFF进入次数 |
| `busoff_out_cnt` | uint16 | BUS-OFF恢复次数 |
| `last_tx_id` | uint16 | 最后发送的CAN ID |

### g_dbg.lp — 低功耗 (8 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `mode` | uint8 | 0=NORMAL 1=HICCUP 2=DEEP 3=NO_SLP |
| `ready` | uint8 | 准备休眠 |
| `block_reason` | uint8 | 阻塞原因码 |
| `block_mask` | uint32 | 阻塞位掩码 |
| `sleep_sec` | uint32 | 上次休眠秒数 |
| `elapsed_sec` | uint32 | 累计休眠秒数 |
| `hiccup_cycles` | uint32 | HICCUP 唤醒轮次 |
| `last_wake_src` | uint8 | 上次唤醒源 |

### g_dbg.adc — ADC (6 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `mos_temp` | uint16 | MOS温度 |
| `typec_cur_ma` | uint16 | TypeC电流 mA |
| `vbat_mv` | uint32 | 电池总压 mV |
| `raw_vbus` | uint16 | VBUS DMA原始值 |
| `raw_cur` | uint16 | TypeC DMA原始值 |
| `raw_mos` | uint16 | MOS温 DMA原始值 |

### g_dbg.soc — SOC (26 fields)

**基础值**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `pct` | uint8 | SOC % |
| `soh` | uint8 | SOH % |
| `cap_now` | uint16 | 当前容量 Ah×100 |
| `vmax` | uint16 | 最高电芯 mV |
| `vmin` | uint16 | 最低电芯 mV |
| `ichg` | uint16 | 充电电流 A×10 |
| `idsg` | uint16 | 放电电流 A×10 |
| `init_over` | uint8 | 初始化完成 |
| `ocv_cali` | uint8 | OCV校准标志 |
| `vtotal` | uint16 | 总压 V×100 |

**校准内部状态**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `mode` | uint8 | CHG/DSG/RELAX |
| `last_mode` | uint8 | 上次模式 |
| `rest_ticks` | uint32 | 静置计数 |
| `stable_ticks` | uint32 | 稳定计数 |
| `full_ticks` | uint16 | 满电确认计数 |
| `empty_ticks` | uint16 | 尾端计数 |
| `mid_ticks` | uint16 | 中段计数 |
| `full_anchor` | uint8 | 已满电锚定 |
| `cal_allowed` | uint8 | 校准允许 |
| `sag_blocked` | uint8 | SAG阻塞 |
| `rest_stable` | uint8 | 电压稳定 |
| `low_tail` | uint8 | 低压尾部激活 |
| `mid_tail` | uint8 | 中段尾部激活 |
| `display_ticks` | uint16 | 显示平滑计数 |
| `ocv_target` | uint8 | OCV目标 SOC% |
| `last_calib_soc` | uint8 | 上次校准前 SOC% |

### g_dbg.afe — AFE (7 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `bstatus1` | uint8 | SH367309 BSTATUS1 |
| `bstatus3` | uint8 | SH367309 BSTATUS3 |
| `fault1` | uint8 | BSTATUS1副本 |
| `cur_raw` | uint16 | AFE电流 mA |
| `pec_err` | uint16 | PEC错误计数 |
| `cell_min_mv` | uint16 | 最低电芯mV |
| `cell_max_mv` | uint16 | 最高电芯mV |

### g_dbg.fault / aging / flash (9 fields)

| 组 | 字段 | 说明 |
|----|------|------|
| `fault.first` | uint16 | 一级故障 |
| `fault.third` | uint16 | 三级故障 |
| `fault.mdl1/mdl3` | uint16 | 模型故障 |
| `aging.state` | uint8 | 0=STOP 1=RUN 2=DONE |
| `aging.remain_sec` | uint32 | 老化剩余秒数 |
| `flash.update_flag` | uint8 | IAP请求标志 |
| `flash.e2prom_flag` | uint8 | Flash写请求 |
| `flash.busy` | uint8 | Flash忙 |

### g_dbg.led — LED显示 (10 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `sleep` | uint8 | 休眠态 |
| `blank` | uint8 | 空白 |
| `number` | uint8 | 当前显示数字 |
| `indicators` | uint8 | 图标掩码 |
| `disp_10ms` | uint16 | 显示剩余tick |
| `frame_len` | uint8 | 扫描帧长 |
| `scan_idx` | uint8 | 扫描位置 |
| `key_active` | uint8 | 按键有效 |
| `charge_icon` | uint8 | 充电图标 (1=亮) |
| `percent_icon` | uint8 | %图标 (1=亮) |

### g_dbg.timing / ctr (12 fields)

| 组 | 字段 | 说明 |
|----|------|------|
| `timing.loop_last_us` | uint32 | 上轮主循环 μs |
| `timing.loop_max_us` | uint32 | 历史最大 μs |
| `ctr.main_cycle` | uint32 | 主循环计数 |
| `ctr.afe_get_cnt` | uint32 | AFE获取次数 |
| `ctr.can_rcv_cnt` | uint32 | CAN接收帧数 |
| `ctr.rtc_sleep_cnt` | uint32 | RTC休眠次数 |
| `ctr.rtc_sec_cnt` | uint32 | RTC秒中断 |
| `ctr.rtc_alm_cnt` | uint32 | RTC闹钟 |
| `ctr.sci1_irq_cnt` | uint32 | 串口1中断 |
| `ctr.pa0/key_irq_cnt` | uint16 | 按键中断 |
| `ctr.tick_10ms` | uint32 | 10ms tick |

---

## 4. Printf 输出（`_DEBUG_` 宏控制，UART1 输出）

| 函数 | 触发 | 内容 |
|------|------|------|
| `DbgPrint_Summary()` | 每5秒自动 | 一行: 时间/SOC/V/I/MOS/LP/CAN/老化/耗时 |
| **`DbgPrint_All()`** | 手动 | 一次输出所有分组的完整信息 |
| `DbgPrint_IO()` | 手动 | GPIO + MOS 状态 |
| `DbgPrint_LP()` | 手动 | 低功耗阻塞原因逐位解析 + HICCUP轮次 |
| `DbgPrint_CAN()` | 手动 | CAN 状态 + 错误计数 + 最后发送ID |
| `DbgPrint_SOC()` | 手动 | SOC 校准全量 |
| **`DbgPrint_Wakeup()`** | 手动 | 唤醒源 / RTC状态 / HICCUP轮次 |
| `DbgPrint_EventRing()` | 手动 | 最近 16 条事件 |

### DbgPrint_Summary 输出示例

```
[DBG] 120s |SOC=45% V=3.6~3.9V I=0/5A |MOS=cd |LP=HICCUP blk=00 |CAN=A- |AGE=RUN |loop=850us max=1250us
```

---

## 5. 常用调试流程

### 不进低功耗

```
展开 g_dbg.lp → 看 mode/block_reason/block_mask
mode=3 (NO_SLP) 且 block_mask != 0 → DbgPrint_LP() 看逐位解析
```

### CAN 通信问题

```
展开 g_dbg.can → 看 bus_off/no_ack_cnt/tx_queue/power_on
bus_off=1 → 看 esr/busoff_in_cnt
power_on=0, tx_queue>0 → 收发器供电问题
last_tx_id=0 → 无任何发送
```

### SOC 不校准

```
展开 g_dbg.soc → 看 cal_allowed/sag_blocked/rest_stable/rest_ticks
cal_allowed=0 → 逐个检查阻塞条件
rest_ticks 很小 → 电池一直在充放电
sag_blocked=1 → 电压跌落抑制中
```

### LED 闪烁

```
展开 g_dbg.led → 看 scan_idx/frame_len/disp_10ms
frame_len=0 → 没在扫描
charge_icon/percent_icon → 图标是否正确
```

### 主循环耗时

```
g_dbg.timing.loop_max_us > 5000 → 有阻塞或计算过重
结合 g_dbg.ctr.afe_get_cnt 增长速率判断 AFE 采样频率
```

---

## 6. 文件清单

| 文件 | 作用 |
|------|------|
| `Source/SystemDebug.h` | 14个子结构体 + API声明 |
| `Source/SystemDebug.c` | 快照实现 + 事件环形缓冲 + printf |
| `Source/Runtime.c` | 调用点 + 自动事件检测 + 定时打印 |
| `Source/Can_HDX.c` | `Can_GetDebugSnapshot()` |
| `Source/LedBar.c` | `LedBar_GetDebugSnapshot()` |
| `Source/SocEnhance.c` | `SOC_GetDebugInternals()` |
| `conf/Project_Config.h` | `PROJECT_CFG_DEBUG_MONITOR_ENABLE` |

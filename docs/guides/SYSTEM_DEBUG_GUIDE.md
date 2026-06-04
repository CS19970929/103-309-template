# SystemDebug 调试监控使用指南 v2.2

> 功能: `SystemDebug` — Keil Watch 全局状态快照
> 版本: v2.2 2026-06-02 (增加模块健康总览)
> 控制宏: `PROJECT_CFG_DEBUG_MONITOR_ENABLE` (0=Release, 1=Debug)

## 1. 概述

`g_dbg` 是一个全局结构体，200ms 更新一次，把所有 IO 状态、外设状态、功能状态、运行计数器拍成快照。

**v2.2 改进**: 21 个子结构体替代平铺字段。Keil Watch 中按需展开目标分组，无需在百级字段中翻找。新增 `module`，用于观察各模块是否运行、是否 ready、是否 busy、是否 error、是否超过 2s 未刷新。

**零开销**: `PROJECT_CFG_DEBUG_MONITOR_ENABLE=0` 时，整个模块编译为空，不占 Flash/RAM。

## 2. 启用方法

1. `conf/Project_Config.h` → `PROJECT_CFG_DEBUG_MONITOR_ENABLE` → 设为 `1`
2. 重新编译，烧录
3. Keil → Debug → Watch 窗口 → 添加 `g_dbg`
4. 展开子结构体查看，例如 `g_dbg.gpio` → 展开看所有 IO

## 3. 结构体层级（21 组）

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

### g_dbg.module — 模块健康总览

| 字段 | 类型 | 说明 |
|------|------|------|
| `alive_mask` | uint32 | 已经打过心跳的模块 bit |
| `ready_mask` | uint32 | 当前 ready 的模块 bit |
| `busy_mask` | uint32 | 当前 busy 的模块 bit |
| `error_mask` | uint32 | 当前 error 的模块 bit |
| `stale_mask` | uint32 | 超过 200 个 10ms tick 未刷新的模块 bit |
| `last_id` | uint8 | 最近一次打心跳的模块 ID |
| `last_tick` | uint32 | 最近一次模块心跳 tick |

每个模块子项包含：

| 字段 | 类型 | 说明 |
|------|------|------|
| `last_tick` | uint32 | 最近一次运行 tick |
| `max_gap_ticks` | uint32 | 两次运行之间的最大间隔，单位 10ms |
| `run_cnt` | uint32 | 运行次数，饱和到 `0xFFFFFFFF` |

模块 ID / bit 对照：

| ID | bit | 子项 | 模块 |
|----|-----|------|------|
| 0 | `0x00000001` | `runtime` | 主循环 |
| 1 | `0x00000002` | `systime` | `SysTime_LatchTaskFlags()` |
| 2 | `0x00000004` | `aging` | 老化任务 |
| 3 | `0x00000008` | `led` | 灯板显示 |
| 4 | `0x00000010` | `afe` | AFE 采样/处理 |
| 5 | `0x00000020` | `snapshot` | `SystemDebug_Snapshot()` |
| 6 | `0x00000040` | `sci` | 上位机串口协议 |
| 7 | `0x00000080` | `adc` | MCU ADC 计算 |
| 8 | `0x00000100` | `low_power` | 低功耗任务 |
| 9 | `0x00000200` | `can` | CAN 任务 |
| 10 | `0x00000400` | `flash` | Flash 更新/存储 |
| 11 | `0x00000800` | `log` | 日志记录 |
| 12 | `0x00001000` | `proid` | 生产 ID |
| 13 | `0x00002000` | `watchdog` | IWDG 喂狗 |
| 14 | `0x00004000` | `debug_print` | Debug 串口打印 |
| 15 | `0x00008000` | `protect` | 保护/故障状态 |
| 16 | `0x00010000` | `soc` | SOC 计算 |

使用方法：

- 先看 `error_mask`：非 0 时按 bit 对照定位异常模块。
- 再看 `busy_mask`：判断是否卡在 CAN、Flash、老化、低功耗准备等状态。
- 再看 `stale_mask`：非 0 时说明对应模块超过约 2s 没有刷新。
- 展开对应子项，看 `last_tick/max_gap_ticks/run_cnt` 判断是偶发延迟还是任务彻底停跑。

### g_dbg.rcc — MCU 时钟资源 (11 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `cr` | uint32 | RCC CR 原始寄存器 |
| `cfgr` | uint32 | RCC CFGR 原始寄存器 |
| `ahbenr` | uint32 | AHB 外设时钟使能 |
| `apb1enr` | uint32 | APB1 外设时钟使能 |
| `apb2enr` | uint32 | APB2 外设时钟使能 |
| `bdcr` | uint32 | 备份域/RTC 时钟配置 |
| `csr` | uint32 | LSI 与复位标志 |
| `sysclk_src` | uint8 | 0=HSI 1=HSE 2=PLL |
| `hse_ready` | uint8 | HSE ready |
| `pll_ready` | uint8 | PLL ready |
| `lsi_ready` | uint8 | LSI ready |

用途：确认 STOP/RTC 唤醒后 CAN、ADC、USART、GPIO、AFIO、PWR、BKP、RTC 等资源时钟是否恢复。

### g_dbg.irq — MCU 中断资源 (9 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `iser0` | uint32 | NVIC IRQ0-31 使能位 |
| `ispr0` | uint32 | NVIC IRQ0-31 pending 位 |
| `iabr0` | uint32 | NVIC IRQ0-31 active 位 |
| `scb_icsr` | uint32 | 当前/挂起异常状态 |
| `scb_shcsr` | uint32 | 系统异常 active/pending/enable |
| `systick_ctrl` | uint32 | SysTick CTRL |
| `systick_val` | uint32 | SysTick 当前计数 |
| `exti_imr` | uint32 | EXTI 中断屏蔽状态 |
| `exti_pr` | uint32 | EXTI pending 状态 |

用途：确认 RTC/EXTI/CAN/USART/ADC/TIM 中断是否打开，是否存在 pending 未清或 active 卡住。

### g_dbg.periph — MCU 外设寄存器快照 (13 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `usart1_sr` | uint16 | USART1 SR |
| `usart2_sr` | uint16 | USART2 SR |
| `usart3_sr` | uint16 | USART3 SR |
| `can_msr` | uint16 | CAN MSR |
| `can_tsr` | uint32 | CAN TSR |
| `can_rf0r` | uint32 | CAN FIFO0 状态 |
| `can_esr` | uint32 | CAN 错误状态 |
| `adc1_sr` | uint16 | ADC1 SR |
| `dma1_isr` | uint32 | DMA1 ISR |
| `tim3_sr` | uint16 | TIM3 SR |
| `tim4_sr` | uint16 | TIM4 SR |
| `flash_sr` | uint16 | Flash SR |
| `pwr_csr` | uint16 | PWR CSR |

注意：这些字段只读快照；不会读 USART DR、CAN FIFO 数据寄存器，也不会清除 pending/reset/错误标志。对应外设时钟未使能时字段填 0。

### g_dbg.reset — MCU 复位来源 (7 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `rcc_csr` | uint32 | RCC CSR 原始寄存器 |
| `pin` | uint8 | PIN reset |
| `por` | uint8 | POR/PDR reset |
| `software` | uint8 | software reset |
| `iwdg` | uint8 | independent watchdog reset |
| `wwdg` | uint8 | window watchdog reset |
| `low_power` | uint8 | low-power reset |

用途：确认异常重启是看门狗、掉电、外部 NRST、软件复位还是低功耗复位导致。

### g_dbg.can — CAN 状态 (4 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `power_on` | uint8 | 收发器供电 |
| `bus_off` | uint8 | BUS-OFF状态 |
| `tx_queue` | uint8 | TX队列长度 |
| `esr` | uint16 | CAN ESR寄存器 |

### g_dbg.lp — 低功耗 (8 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `mode` | uint8 | 0=NORMAL 1=HICCUP 2=DEEP 3=NO_SLP |
| `ready` | uint8 | 准备休眠 |
| `block` | uint32 | `LP_BLOCK_*` 阻塞位掩码 |
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

### g_dbg.soc — SOC (18 fields)

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
| `full_anchor` | uint8 | 已满电锚定 |
| `display_ticks` | uint16 | 显示平滑计数 |

更细的 SOC 内部观察使用 `g_dbg_soc_watch`：

- `u8LastCalibSource/u8LastSocBefore/u8LastSocAfter`
- `u8LowTailActive`
- `u8InternalSoc/u8DisplaySoc`
- `u32RestTicks/u32StableRestTicks/u32LongRestDownTicks`
- `u8RestDownValid/u8RestDownTarget`
- `u8RestVoltageStable/u8SagHoldBlocksCalibration`

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

### g_dbg.profile — 主循环分段耗时 (5 groups)

每个分组包含 `last_us`、`max_us`、`call_cnt`：

| 分组 | 说明 |
|------|------|
| `loop` | 整轮 `Runtime_RunOnce()` 耗时 |
| `front` | `SysTime_LatchTaskFlags`、老化、LED、AFE、SystemDebug 快照 |
| `io_power` | SCI、ADC 计算、低功耗任务、CAN |
| `background` | Flash、日志、生产 ID、喂狗 |
| `debug_print` | Debug 构建下的周期串口打印 |

用途：当 `timing.loop_max_us` 偏大时，展开 `g_dbg.profile` 判断是 AFE、通信/低功耗、Flash/日志还是 Debug 打印拖慢主循环。

### g_dbg.watchdog — IWDG 喂狗监控 (9 fields)

| 字段 | 类型 | 说明 |
|------|------|------|
| `feed_cnt` | uint32 | `IWDG_Feed()` 调用次数 |
| `last_feed_tick` | uint32 | 最近喂狗 10ms tick |
| `last_gap_ticks` | uint32 | 最近两次喂狗间隔 |
| `max_gap_ticks` | uint32 | 历史最大喂狗间隔 |
| `pr` | uint16 | IWDG PR |
| `rlr` | uint16 | IWDG RLR |
| `sr` | uint16 | IWDG SR |
| `last_source` | uint8 | 2=普通喂狗 |
| `iwdg_reset` | uint8 | RCC CSR 中 IWDG reset 标志 |

用途：排查主循环或长阻塞流程是否导致喂狗间隔过大；`max_gap_ticks` 单位为 10ms。

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
展开 g_dbg.lp → 看 mode/block
mode=3 (NO_SLP) 且 block != 0 → DbgPrint_LP() 看逐位解析
```

### CAN 通信问题

```
展开 g_dbg.can → 看 bus_off/tx_queue/power_on/esr
bus_off=1 → 看 esr；恢复由 CAN_ABOM 自动处理
power_on=0, tx_queue>0 → 收发器供电问题
tx_queue 长时间不降 → 看总线 ACK、bus-off、主循环 App_Can() 是否运行
```

### SOC 不校准

```
展开 g_dbg.soc → 先看 mode/rest_ticks/stable_ticks/full_ticks/empty_ticks
展开 g_dbg_soc_watch → 再看 u8LastCalibSource/u8LowTailActive/u8RestVoltageStable/u8SagHoldBlocksCalibration
rest_ticks 很小 → 电池一直在充放电或被 low-tail/sag hold 打断
u8SagHoldBlocksCalibration=1 → 电压跌落抑制中
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

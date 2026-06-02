# 项目优化简化变更报告

> 日期: 2026-06-01
> 分支: `production-release`
> 提交: `88c43c9` — 24 文件, -323 行, 0 Error

---

## 2026-06-02 回归修复说明

`02bb091` 继续删除 init-done 类变量时，误删了 LedBar 必要的运行态保护。该保护不是死字段：`APP_LedBar()` 每轮主循环都会调用，必须防止重复执行 `LedBar_Init()`。

已恢复：

- `LedBarRuntime.initialized`
- `LedBarRuntime.scan_timer_initialized`
- `LedBarRuntime.key_filter_initialized`
- `LedBarRuntime.mcu_wk_filter_initialized`

这些字段用于保持显示窗口、扫描定时器和输入滤波状态，不属于可删除的无效变量。

---

## 删除清单

### 整文件删除 (6 个)

| 文件 | 原因 | 替代 |
|------|------|------|
| `bsp_power.c/h` | 纯 1:1 转发器，无逻辑 | `app_lowpower.c` 直接调用 `RtcSleep_Port*` |
| `bsp_clock.c/h` | 纯 1:1 转发器 | 直接调用 `cpu_frequency_conf()` |
| `bsp_rtc.c/h` | 7/8 个函数是纯转发器 | 内联到 `app_lowpower.c`，唯一有状态的 `BSP_RTC_SetWakeupPeriodSeconds` 直接合并 |

### 函数删除 (9 个)

| 函数 | 文件 | 原因 |
|------|------|------|
| `sleep()` | rtc_sleep.c | 无调用者 |
| `App_LowPowerProcess()` | rtc_sleep.c | 3 层包装，已直接调 `rtc_sleep()` |
| `entersleep(mode)` | rtc_sleep.c | 直接调 `LowPower_Request()` |
| `set_irq_wksource()` | rtc_sleep.c | 直接赋值 `g_irq_t` |
| `ReadEEPROM_Byte()` | EEPROM.c | 死存根，始终返回 0xFF |
| `WriteEEPROM_Byte()` | EEPROM.c | 死存根，空操作 |
| `ReadEEPROM_Word_NoZone()` | EEPROM.c | 死存根，始终返回 0xFFFF |
| `WriteEEPROM_Word_NoZone()` | EEPROM.c | 死存根，空操作 |
| `U16_SwapEndian()` | PubFunc.c | 改为 `main.h` 中宏 |

### 结构体字段删除 (~12 个)

`Time_T` (sys_time) 中删除:
- `test_main_cycle`, `App_AFEGet_cnt`, `App_SH367309_Monitor_cnt` — 从未被写入
- `sci2_irq_cnt`, `sci3_irq_cnt` — 从未使用 (SCI2/SCI3 禁用)
- `pec_err_cnt` — 从未被写入
- `CHG`, `DSG` — 仅 debug 用
- `cnt_enter_chg_open`, `cnt_enter_dsg_open` — 从未使用
- `wakeup_reason`, `power_on` — 未使用
- `enter_rtc_delay`, `rtc_irq_cnt`, `typc_curr` — 从未使用

### EEPROM.h 宏删除

- `sEEAddress`, `SDA_IN_SEE`, `SDA_OUT_SEE`, `SCL_IN_SEE`, `SCL_OUT_SEE` — 旧 I2C 硬件定义

### SystemDebug.c/h 字段删除

- `g_dbg.ctr.main_cycle`, `g_dbg.ctr.afe_get_cnt` — 对应 sys_time 死字段
- `g_dbg.afe.pec_err` — 对应 sys_time.pec_err_cnt

---

## 保留不变

- 所有功能代码、协议、保护逻辑、IAP
- app_lowpower.c 的核心阻塞检查逻辑不变，只是去掉 BSP 间接层
- FactoryAging.c 的行为不变，只是提取公共加载路径

---

## 编译验证

`FD_Release + DEBUG_MONITOR_ENABLE=1` — **0 Error(s), 6 Warning(s)**

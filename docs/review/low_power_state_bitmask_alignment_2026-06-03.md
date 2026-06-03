# 低功耗状态与阻塞位图收口说明

文档状态：已按源码部分验证
源码验证：已核对当前源码，未上板实测
日期：2026-06-03
参考源码：

- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep.h`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/SystemDebug.c`
- `103 + 309/Project/Source/SystemDebug.h`
- `tools/stlink_bms_monitor.ps1`
- `tools/project_check.py`

## 本次确认的设计

`g_stLowPowerRtcStatus` 是低功耗模块的统一状态结构，保存：

| 字段 | 含义 |
|---|---|
| `mode` | 当前睡眠请求模式 |
| `rtc` | RTC 唤醒标志 |
| `comm` | 上一次外部通信计数 |
| `idle` | 空闲进入 HICCUP 的累计秒数 |
| `idleMax` | 空闲进入 HICCUP 的目标秒数 |
| `force` | 2800mV 强制 deep 累计秒数 |
| `vlow` | 参数低压 deep 累计秒数 |
| `block` | `LP_BLOCK_*` 阻塞位图 |
| `sleep` | 当前 HICCUP STOP 累计睡眠秒数 |
| `last` | 最近一次完整睡眠秒数 |
| `cycles` | 当前 HICCUP STOP 周期次数 |

## 阻塞原因口径

源码只保留一套阻塞口径：`LP_BLOCK_*` bitmask。

已删除旧的 `LOW_POWER_RTC_BLOCK_*` 粗粒度枚举，避免 `block_mask -> block_reason` 二次映射。

当前阻塞位：

| 位 | 宏 | 含义 |
|---|---|---|
| bit0 | `LP_BLOCK_CHARGE` | 充电电流大于 10mA |
| bit1 | `LP_BLOCK_DISCHARGE` | 放电电流大于 10mA |
| bit2 | `LP_BLOCK_COMM` | SCI 或 CAN busy |
| bit3 | `LP_BLOCK_KEY` | MCU_WK active |
| bit4 | `LP_BLOCK_EXT_COMM` | 外部通信计数变化 |
| bit5 | `LP_BLOCK_FLASH_BUSY` | Flash busy 或 E2PROM 待写 |
| bit6 | `LP_BLOCK_UPGRADE` | IAP/升级 pending |
| bit7 | `LP_BLOCK_FAULT` | fault active |
| bit8 | `LP_BLOCK_LED_ACTIVE` | LedBar 显示活动 |
| bit9 | `LP_BLOCK_AGING` | 工厂老化 running |

`LP_GetBlockReason()` 是唯一阻塞判断入口。`lp_select()` 只读取该函数返回值并决定是否清空 `idle`。

## 低功耗选择流程

当前 `rtc_sleep()` 每秒调用 `lp_select()`。

```text
lp_select()
  lp_deep()
    强制低压 deep
    参数低压 deep
  g_stLowPowerRtcStatus.block = LP_GetBlockReason()
  if block != 0
    idle = 0
    return
  lp_idle()
    idle 达到 idleMax 后请求 HICCUP_MODE
```

## 保持不变

- 低压 deep 优先级仍高于普通阻塞。
- 工厂老化只阻塞 HICCUP RTC STOP，不阻塞低压 deep 或外部 `DEEP_MODE/NORMAL_MODE` reset sleep 请求。
- `HICCUP_MODE` 仍走 RTC STOP 周期循环。
- `NORMAL_MODE/DEEP_MODE` 仍走 `RtcSleep_PortCommitResetSleep()` 和 `SleepDeal_Continue()`。
- 不修改 CAN/Modbus/上位机协议字段、IAP/App 地址和烧录脚本。

## 调试观察

`SystemDebug` 不再调用 `LP_GetBlockReason()`，只读取 `g_stLowPowerRtcStatus.block`。这样可避免调试快照提前消费 `RTC_ExtComCnt` 边沿。

`tools/stlink_bms_monitor.ps1` 已按 `g_stLowPowerRtcStatus` 新 8 word 布局解析：

- word0：`mode/rtc/comm`
- word1：`idle/idleMax`
- word2：`force`
- word3：`vlow`
- word4：`block`
- word5：`sleep`
- word6：`last`
- word7：`cycles`

## 验证记录

- `git diff --check`：通过，仅有仓库既有 CRLF 换行提示。
- `py -3.9 tools/project_check.py --quiet`：`101 OK / 0 Warnings / 39 Errors`；剩余失败为仓库既有缺失文件、编码和配置门禁问题，本次新增低功耗状态收口门禁通过。
- `./tools/bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil 日志显示 `0 Error(s), 3 Warning(s)`，已生成 `FD_Release.axf/bin`。

仍需上板验证：

- 真板空闲进入 HICCUP STOP。
- 低压 deep 优先级。
- 通信、Flash、fault、LED、老化阻塞位图。
- ST-Link 监控脚本读取新布局。

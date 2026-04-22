# IsSleepStartUp 与休眠标志改为备份域寄存器说明

## 1. 背景
- 日期：2026-04-22
- 需求：休眠相关逻辑统一使用备份域寄存器，不再使用 Flash 存储睡眠标志。
- 现状问题：
1. 原 `IsSleepStartUp()` 来自 030 MCU 代码，访问了 `RTC->BKP1R/BKP2R`，不符合 STM32F103 标准库寄存器模型。
2. 工程中存在睡眠标志写 Flash 的路径，和备份域方案不一致。

## 2. 修复目标
1. 所有休眠标志读写统一到 F103 备份域寄存器。
2. 保留原有睡眠模式值（`NORMAL/DEEP/HICCUP/RESET`）和启动行为。
3. 避开 RTC 已占用的 `BKP_DR1`（RTC 初始化标志使用），避免冲突。

## 3. 实施方案
1. 在 `SleepDeal.c` 中实现统一的 `BootFlag_*` 接口，底层改为：
- `BKP_DR2`：标志值
- `BKP_DR3`：标志反码（`~flag`）
2. 写入前统一打开备份域访问：
- `RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE)`
- `PWR_BackupAccessCmd(ENABLE)`
3. 读取时做一致性校验：`flag ^ inverse_flag == 0xFFFF`，失败按 `BOOT_FLAG_RESET_VALUE` 处理。
4. `SleepDeal_Continue()` 与 `rtc_sleep.c` 中原先写 Flash 的睡眠标志逻辑，全部改为 `BootFlag_Write(...)`。

## 4. 改动文件
1. `103 + 309/Project/Source/SleepDeal.c`
2. `103 + 309/Project/Source/SleepDeal.h`
3. `103 + 309/Project/Source/rtc_sleep.c`

## 5. 关键行为变化
1. 休眠标志不再擦写 Flash 页面，改为备份域寄存器读写。
2. 上电启动 `IsSleepStartUp()` 读取来源与休眠写入来源完全一致（均为备份域）。
3. 备份域采用“值 + 反码”双寄存器机制，提高异常值检测能力。

## 6. 回归测试建议
1. 冷启动（无休眠标志）验证不会误入睡眠恢复分支。
2. 分别触发 `NORMAL/HICCUP/DEEP`，复位后验证 `IsSleepStartUp()` 能进入对应分支。
3. 唤醒后再次复位，验证 `BootFlag_Clear()` 生效，不重复进入同一睡眠恢复流程。
4. RTC 初始化后验证 `BKP_DR1`（RTC 标志）与 `BKP_DR2/DR3`（睡眠标志）互不干扰。

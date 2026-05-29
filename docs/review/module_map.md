# D009 CAN/RTC 休眠模块映射

状态：部分验证

参考源码：

- `103 + 309/Project/Source/main.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c`
- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/LowPowerSleep.c`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/bsp_power.c`
- `103 + 309/Project/Source/bsp_clock.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/LedBar.c`

| 模块 | D009 当前文件 | 当前职责 | 迁移注意 |
|---|---|---|---|
| 主循环 | `main.c` | `App_LowPowerProcess()` 在 `App_Can()` 前执行 | 顺序已符合参考分支策略，避免 CAN 先拉起发送窗口后阻塞入睡 |
| CAN 周期发送 | `Can_HDX.c` | 1s/5s 飞道广播、CAN App 命令、老化/IAP 命令 | 保留协议和帧内容，迁移调度与忙闲语义 |
| CAN 收发器供电 | `Can_HDX.c`, `conf/conf_gpio.h` | `GPIO_CMNT_EN/PB4` 控制 CAN transceiver | 保留 D009 引脚和极性，确认 RTC IO 模式后能恢复输出 |
| CAN RTC 服务 | `Can_HDX.c` | RTC 唤醒后按 active/idle 发送周期帧或探测帧 | 目标是有 ACK 设备时 1s 对外通信，无设备时回退低频探测 |
| RTC 周期 | `RTC.c`, `bsp_rtc.c` | `RTC_GetWakeupPeriodSeconds()` 从 CAN 获取 1s/10s 周期，BSP 层提供设置/安全窗口接口 | 已同步参考分支的 pending/中断清理和运行态秒中断恢复 |
| 低功耗框架 | `app_lowpower.c`, `LowPowerSleep.c`, `bsp_power.c`, `bsp_clock.c` | 汇总睡眠阻塞原因、保存核心状态、封装 STOP 前后动作 | D009 适配时移除当前分支不适用的 `StorageFlash_IsBusy()` 和数码管活跃阻塞 |
| 低功耗入口 | `rtc_sleep.c`, `rtc_sleep_port.c`, `rtc_sleep_afe_sh367309.c` | `HICCUP_MODE` STOP + RTC alarm 周期唤醒，硬件/AFE 操作通过 port 层隔离 | 保持 D009 AFE/SOC/老化/充电判断，迁移 CAN 服务窗口 |
| 复位式休眠 | `SleepDeal.c` | deep/normal sleep 前保存状态并 STOP/RESET | 保留，不把 RTC 通信窗口引入 deep sleep |
| 板级 IO | `conf/conf.c` | STOP 前关闭 ADC/CAN/LED，唤醒后恢复 | 保留 D009 普通 LED 和 socKey，不迁移数码管 |
| LED | `LedBar.c/.h` | 4 路普通 SOC LED，PA3/PA2/PA4/PA7 | 只确认不会阻塞 RTC；不迁移 Charlieplexing/74HC595 |

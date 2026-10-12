# 休眠逻辑修复与调用链收敛

日期：2026-10-12。修改基线：`00795f9`。适用目录：`103 + 309/Project`。

本次在当前实现上修复问题，没有导入 `E:\TODO\103 + 309 - 副本` 的休眠框架。保留当前低压策略、长按行为、协议字段、EEPROM 地址和 Flash 标志格式。

## 状态与调用链

`rtc_sleep.c` 是休眠请求、资格计数和 RTC STOP 循环的唯一所有者。主循环直接调用 `rtc_sleep()`；保护、按键和协议仍通过 `entersleep(mode)` 请求休眠。请求优先级为 `DEEP_MODE > NORMAL_MODE > HICCUP_MODE`，自动空闲判断不会覆盖显式请求；低压计时可以把等待中的 NORMAL 请求升级为 DEEP。

```text
main → rtc_sleep
       ├─ HICCUP → Init_RTC → IO/唤醒源 → RTC_WKTimeConfig → Sys_StopMode
       │                         ↑                       ↓
       │                         └── 无异常的 RTC 唤醒 ───┘
       │             异常/外部唤醒 → sleep_restore_run → InitRunAfterStop → RUN
       └─ NORMAL/DEEP → 等待通信/参数写入结束
                       → SOC_SaveBeforeSleep → 兼容 Flash 标志
                       → AFE_Sleep → 事件记录 → MCU_RESET
启动 → IsSleepStartUp → 识别并清除兼容标志 → 原 GPIO/唤醒源 → STOP → MCU_RESET
```

`SleepDeal.c` 只承担复位休眠事务和启动标志恢复。删除未启用的第二套 `App_SleepDeal` 状态机、`Sleep_Mode` 位域、跨日志模块释放休眠状态的机制，以及未调用的 LED 关机入口。有效的 `App_DI1_Switch` 保持原代码。

## 保留的产品策略

| 项目 | 保留行为 |
| --- | --- |
| 强制低压 | `Vmin <= 2600mV && u16Ichg == 0`，原 60 次一秒资格计数后请求 DEEP |
| 参数低压 | `Vmin <= OtherElement.u16Sleep_Vlow && u16Ichg == 0`，原 `u16Sleep_TimeVlow * 60` 次资格计数 |
| 空闲 RTC | 保留启动首个周期准备，再累计 `sys_time.time_enter_rtc = 30` 个空闲周期 |
| RTC 周期 | 保留 `ALARM_TIME_SEC = 10`；LSE 优先、首次初始化可回退 LSI |
| 长按 | PA9 低电平，原 `App_DI1_Switch` 连续 15 次判定后请求 DEEP；松开清除消抖计数 |
| 保护请求 | `DataDeal.c`、`ChargerLoadFunc.c` 原 NORMAL 请求条件和延时保持不变，修复其请求原本没有生效的问题 |
| HICCUP / NORMAL 唤醒 | 原 PA0 上升、PA9 下降、PB12 上升；HICCUP 加 RTC Alarm |
| DEEP 唤醒 | 原 PA0 上升、PA9 下降；没有新增通信唤醒源 |
| RTC 准入 | 保留充放电报告值大于 10、加热、实际均衡、充电器输入及 AFE 状态检查 |

本次没有把低压计数改成新的连续计时算法，也没有为 RTC 模式新增 AFE IDLE/SLEEP 命令。RTC 模式延续当前的 MCU STOP 加 AFE 定期监测方式；NORMAL/DEEP 延续 AFE SLEEP 加复位休眠方式。

## 修复内容与失败行为

- 移除从未置真的 `sys_time.power_on` 条件，RTC 唤醒后实际读取 AFE MOS/保护状态；电压读取、状态读取失败及 MOS 关闭均退出 RTC 循环，恢复主循环。
- RTC 初始化识别有效备份域，普通进入/唤醒不再 `BKP_DeInit()` 重置时间。首次设时使用初始日期的副本，避免换算函数修改模板影响失败重试。
- 闹钟、同步、振荡器和写完成等待使用有限轮询。初始化/装载失败不进入 STOP；RTC ISR 只清标志并记录唤醒，不等待同步或完成业务采样。
- 用整个 RTC 会话的无符号计数差补偿日志间隔，包含周期采样和 STOP 准备时间；支持提前外部唤醒和 32 位计数回绕，移除原每次只补 5 秒的错误。
- `RTC_ExtComCnt` 改为 `volatile UINT32`，避免恰好收到 256 字节被误判为无活动。进入前检查 UART 收发状态、TC 和持久化待办，清日志过程中也不复位休眠。
- EXTI/UART ISR 锁存外部唤醒原因。RTC 准备后再次检查活动和按键电平；WFI 前在短临界区检查锁存事件，恢复运行时钟后再恢复中断屏蔽状态。
- STOP 前暂停 TIM3 并清除其挂起中断。恢复当前目标原先直接使用的 HSE 时钟源，保留 CFGR 分频和 VTOR；HSE 启动/切换失败有界返回并复位。没有引入 PLL 配置。
- 退出 RTC 循环时一次恢复 GPIO、ADC、原缓存波特率的 UART、TIM3、检测和两路软件 I2C。保留 `InitIO()` 已有的 CTLC 启动与 STOP 恢复区别，不在 RTC 唤醒重新做零电流标定或重写 AFE 参数。
- Flash 单半字写入只尝试一次擦除和编程，核验读回并始终上锁；错误返回，不再无限擦除重试。正常复位休眠只写一次标志、启动清除一次，消除原重复写入。
- NORMAL/DEEP 前保存已有 SOC、放电 SOC 记录和循环次数三个 EEPROM 字，地址和 `/100` 缩放不变；保存失败不提交 Flash 标志。
- AFE 休眠写入失败会尝试回滚 Flash 标志，失败记录错误。本次不继续主动复位；失败请求和资格计数清零，重新等待原条件，避免每秒立即重试。只有 AFE 命令返回成功才记录本次休眠事件。

Flash 标志仍使用 `0x0801FC00`，NORMAL/DEEP/HICCUP 值分别为 `0x1234/0x1235/0x1236`，清除值 `0xFFFF`。启动仍识别旧 HICCUP 标志。未修改 `Flash.h`、EEPROM 布局、旧数据读取、协议地址、字节序、数据缩放及不存在串位的 `61001` 约定。

## 软件验证

下列命令均已通过，产物仅写入 `%LOCALAPPDATA%\CodexTemp\103-309-template`：

```powershell
python -B tests/test_sleep.py
python -B tests/test_current_calibration.py
python -B tests/test_startup_afe.py
python -B tests/check_armcc_no_bin.py
```

`test_sleep.py` 执行生产休眠、Flash、RTC、时钟恢复和按键函数，覆盖低压边界、空闲资格、请求升级、收发/存储忙、256 字节活动别名、均衡/加热阻塞、AFE 故障、RTC 装载失败、准备期唤醒、部分休眠和回绕、EEPROM/Flash/AFE 命令失败及原长按消抖。硬件访问被替换为可注入故障的桩。

已有电流回归通过 20 个命名场景、10000 个随机场景和全部 65536 个 CADC 原始值。启动回归通过两种化学体系、八类标定失败、CTLC 恢复/互锁、参数读回及均衡掩码场景。

ARMCC 5.06、`Target 1`、当前工程的 O0/microlib/宏及源清单：61 个编译单元与 AXF 链接成功，0 错误、11 条原有警告。用相同脚本验证 `00795f9` 加本轮开始前已有的三个用户修改，基线为 35 条警告；本次没有新增警告。

| 大小项 | 基线 | 修改后 |
| --- | ---: | ---: |
| Code | 56132 | 56708 |
| RO-data | 2932 | 2932 |
| RW-data | 1464 | 1376 |
| ZI-data | 6088 | 6088 |

修改后 MAP：加载区域起点 `0x08004800`，大小 `0xEE58`，运行 RAM 区域 `0x1D28`。这些是工程配置下的链接结果，不代表已确认实际芯片容量或实板功耗。

`check_armcc_no_bin.py` 直接调用 ARMCC/armasm/armlink，避开工程里已启用的 UV4 after-build `fromelf --bin`；没有生成 HEX/BIN、烧录或连接板卡。原有 BIN 的路径、大小和时间戳核对未变。

本轮开始前已有的 `DataDeal.h`、`Fault.h` 和 `.uvprojx` 修改原样保留，未纳入本次提交。编译验证使用的是当前工作树配置。

## 实板边界与后续验收

本次完成软件修复和回归，没有作出实板通过结论。需要实板确认 10 秒周期及 LSI 偏差、PA0/PA9/PB12 竞争唤醒、各模式 GPIO 电平/电流、UART 唤醒后重新发帧、CTLC/MOS 恢复、watchdog 和异常复位。

沿用旧单标志和三个独立 EEPROM 字，未新增事务格式或 CRC；掉电中断擦写、AFE 应答丢失及 Flash 回滚本身失败无法凭 host 桩证明安全。若回滚失败，旧休眠标志可能仍在，下一次复位仍会按兼容启动路径处理，需故障注入确认。原工程未消费的持久化标志若长期不清零，也会持续阻止休眠；本次没有改变这些协议写入或存储功能。

# 103-309 BMS Review 后测试计划

> 状态：测试设计。
> 源码验证：PARTIAL，测试项来自当前源码路径和需求确认表。
> 注意：本轮未执行硬件测试；所有硬件实测需在确认需求后进行。

## 1. 编译测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-BUILD-001 | Keil FD_Release 编译 | 使用现有 Keil/脚本入口编译 | 0 error，map/bin 生成 |
| T-BUILD-002 | App 链接地址检查 | 检查 map 中 vector/text 起始地址 | App 从 `0x08004800` 起，不覆盖 `0x08000000` IAP |
| T-BUILD-003 | Profile guard | 检查 `PROJECT_CFG_BUILD_PROFILE` 和 SOC 测试宏 | 量产为 profile 0，SOC test disabled |
| T-BUILD-004 | 后 64K 地址检查 | 读取 Flash size register 或脚本静态检查 | 若小于 128KB，不允许使用 `0x0801C000+` 存储 |

## 2. 上位机 Modbus 协议回归测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-MODBUS-001 | 实时状态读 | `COM4/19200/slave=1` 读 `0xD000` | 电压、电流、SOC、故障字字段稳定 |
| T-MODBUS-002 | 产品信息读 | 读 `0xC002` 48 个寄存器 | SN/HW/SW 各 16 bytes，原 UI 底栏可显示 |
| T-MODBUS-003 | SOC 测试状态 | 读 `0xD300` | 量产返回 `supported=0` |
| T-MODBUS-004 | 写保护参数 | 在工装条件下写保护阈值并读回 | 范围检查正确，写失败可回滚 |
| T-MODBUS-005 | 只读/非法地址 | 读写非法地址 | 返回正确异常码，不写 Flash |

## 3. CAN 协议回归测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-CAN-001 | 周期广播 | 抓 `0x14F80200+index` | 1000ms/5000ms 周期基本符合，字段单位正确 |
| T-CAN-002 | 老化时间广播 | 抓 `0x14F80208` | 字节 2 状态、字节 3-4 剩余分钟正确 |
| T-CAN-003 | CAN App 状态 | 发 `0x60 GET_STATUS` | `0x61` ack 返回 SOC/SOH |
| T-CAN-004 | READ_BLOCK | 通过 CAN App 读 `0xD000` 块 | 多帧 `0x86` 顺序返回，不丢帧 |
| T-CAN-005 | ENTER_IAP | 工装环境发进入 IAP | ack 后延迟复位，App 不覆盖 IAP |
| T-CAN-006 | BusOff 恢复 | 模拟 CAN 异常后恢复总线 | `CAN_ABOM` 自动恢复，恢复后周期帧继续发送；debug 可只读 ESR BOFF |

## 4. 参数读写测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-PARAM-001 | RW 参数默认加载 | 清空/破坏 RW_PARAM 区 | 加载默认并重新保存 |
| T-PARAM-002 | AFE 参数读写 | 写 AFE 参数，重启读回 | Flash 与 AFE 侧一致 |
| T-PARAM-003 | SOC snapshot | 改 SOC 后断电重启 | snapshot 有效且不会跳变 |
| T-PARAM-004 | 升级参数策略 | 改 policy version | 只执行预期 reset 项，不误清其他参数 |

## 5. Flash 掉电风险测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-FLASH-001 | 写中断电 | 写 RW/SOC/log 时断电 | 重启选择旧有效槽或新有效槽 |
| T-FLASH-002 | CRC 破坏 | 手动破坏 slot CRC | 识别无效，不使用坏数据 |
| T-FLASH-003 | Flash busy 阻塞 sleep | 参数写入期间观察低功耗 | 不进入 STOP |

## 6. SOC 测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-SOC-001 | 主机回放 | 使用 SOC host 测试脚本 | 满电/低压/静置/骑行用例通过 |
| T-SOC-002 | 真实电流积分 | 上板充放电 | SOC 方向、速率符合电流 |
| T-SOC-003 | 满电锚点 | 充至阈值并保持 | 逐步到 100%，未确认前不提前 100 |
| T-SOC-004 | 低压尾段 | 模拟 Vmin 靠近 V0 | SOC 不虚高，显示可快速下降 |
| T-SOC-005 | RTC 休眠补偿 | 空闲 STOP 后唤醒 | SOC 小步补偿，不大跳 |
| T-SOC-006 | 量产隔离 | 读写 `0xD300/0x2500` | 量产不支持注入式测试 |

## 7. ADC / AFE 通信测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-ADC-001 | ADC DMA 三通道 | 观察 PA1/PA2/PB1 采样 | 数据更新，无 DMA error |
| T-ADC-002 | Type-C 电流 | 9V/5V 输出负载 | PA2 电流和折算电流合理 |
| T-AFE-001 | AFE I2C 读 | 200ms 读 MTP_TEMP1 46 bytes | CRC 正确，电芯/温度更新 |
| T-AFE-002 | CADC 真实电流 | 充/放电方向测试 | `u16Ichg/u16IDischg` 方向正确 |
| T-AFE-003 | AFE 参数写 | 工装写保护参数 | 写入、verify、reset 后一致 |

## 8. 保护逻辑与 MOS 控制测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-PROT-001 | OVP/UVP | 电池模拟器触发 | fault bit、CAN 状态、MOS 行为正确 |
| T-PROT-002 | OCP/CBC | 可控电流触发 | AFE status 与 System_ErrFlag 一致 |
| T-MOS-001 | 启动 MOS | 冷上电、睡眠唤醒、5V 充电 | CHG/DSG 初始状态符合需求 |
| T-MOS-002 | 深睡/唤醒 MOS | DEEP/HICCUP/NORMAL | 进入/退出无异常导通 |

## 9. RTC 低功耗 / IWDG 测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-LP-001 | idle 进入 HICCUP | 无电流、无通信、无 fault | 达到 `time_enter_rtc` 后 STOP |
| T-LP-002 | 通信阻塞 | 连续 Modbus/CAN | 不进入 STOP |
| T-LP-003 | Flash busy 阻塞 | 写参数期间 | 不进入 STOP |
| T-LP-004 | IWDG 10s 限制 | 设置更长 wake period | 被限制或阻塞，系统不复位 |
| T-LP-005 | RTC 唤醒恢复 | STOP 周期唤醒 | 时钟、ADC、USART、CAN、AFE 恢复；周期唤醒中不主动广播 CAN |
| T-LP-006 | CMNT 睡眠电源 | 示波器/万用表测 `GPIO_CMNT_EN` | `Can_PrepareSleep()` 后关闭，唤醒恢复 `InitCan()` 后打开 |
| T-LP-007 | DBGMCU Release 低功耗位 | Release 运行后读 `DBGMCU->CR` | Release 不置位 DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP；Debug 可显式打开 |
| T-LP-008 | IWDG 宏与实际行为 | 分别构建 `PROJECT_CFG_WDOG_ENABLE=0/1` | 宏值、`Init_IWDG()` 行为、RTC wake 安全窗口一致 |
| T-LP-009 | 工厂老化阻塞 RTC 策略 | 老化 running 时观察低功耗状态，再触发低压或外部 `DEEP_MODE` | 老化 running 时不进入 HICCUP RTC STOP；低压 deep 和外部 deep/reset sleep 请求仍能执行 |
| T-LP-010 | AFE not idle 阻塞策略 | 构造 AFE fault/PCHG/MOS 状态 | 按用户确认：需要阻塞时必须不进入 HICCUP STOP |
| T-LP-011 | Sleep 参数有效性 | 通过上位机写 `0x2310-0x2317` 后观察策略 | 写入的有效参数必须改变对应行为；无效/占位参数必须文档化或拒绝 |
| T-LP-012 | HICCUP 前 AFE 功耗状态 | STOP 前后读 SH367309 状态并测整机电流 | 符合确认后的 AFE sleep/保持测量策略 |
| T-LP-013 | `app_lowpower` 主路径净删减 | `rg "LP_EnterStop|LP_BeforeSleep|LP_AfterWakeup|LP_SetWakeupPeriod|LP_Task|LP_STATE_"` | 源码无旧 wrapper/状态缓存引用；主路径为 `Runtime_RunOnce()->rtc_sleep()` |
| T-LP-014 | RTC sleep 变量净删减 | `rg "get_rtc_soc|set_rtc_soc|s_u8RtcSoc|s_lp_runtime|LP_CanSleep|low_power_cancel_rtc|low_power_is_idle_rtc_request"` | 源码无无消费者缓存/未调用 helper；SOC 休眠补偿仍调用 `RtcSleep_PortApplySocRtcRest()` |
| T-LP-015 | ST-Link 监控脚本兼容 | 用新 ELF 跑 `tools/stlink_bms_monitor.ps1 -Count 1` | 不要求 `s_lp_runtime` 符号，仍能读取 `g_stLowPowerRtcStatus`、LED、DBGMCU |
| T-LP-016 | CAN busy 查询副作用隔离 | 连续 CAN RX，同时观察 `SystemDebug_Snapshot()` 和 `rtc_sleep()` | debug/heartbeat 不更新 `last_ext_comm_cnt_can`；低功耗仍能因 CAN 活动阻塞 RTC STOP |
| T-IWDG-001 | 主循环喂狗 | 正常运行 8h | 无异常复位 |
| T-IWDG-002 | 阻塞等待喂狗 | CAN RTC service/延时 | 无 IWDG 复位 |

## 10. LED 显示测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-LED-001 | SOC 显示 | 0/1/9/10/99/100 | 数码显示正确，无串亮 |
| T-LED-002 | 启动显示窗口 | 上电观察 | 按配置显示窗口后熄屏 |
| T-LED-003 | 睡眠预览 | 睡眠中按键 | 显示保存 SOC，超时熄屏 |
| T-LED-004 | 长按休眠 | 长按 SW | 进入 DEEP_MODE，SOC 保存 |
| T-LED-005 | STOP GPIO 泄漏 | 低功耗电流测试 | LED 引脚无明显漏电 |

## 11. Bootloader / IAP 测试

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-IAP-001 | 安全脚本 dry-run | `tools/soc_flash_app_safe.ps1` 不加 `-Flash` | 输出地址 `0x08004800` |
| T-IAP-002 | 地址拒绝 | 指定 `0x08000000` | 脚本拒绝执行 |
| T-IAP-003 | CAN 进入 IAP | 发 App CAN 命令 | ack 后复位进 IAP |
| T-IAP-004 | 串口进入 IAP | 写 `0xFFFD` | ack 后复位进 IAP |

## 12. 硬件实测清单

- 真实 MCU 丝印、Flash size register、后 64K 可写性。
- CHG_IN、MCU_WK、SW、RS485/USART1 RX、INT_WK_CMNT 的实际电平和唤醒边沿。
- SH367309 CADC 零点、充放电方向、MOS driver 位。
- VBC 分压、Type-C PA2 电流换算、MOS NTC 温度换算。
- RTC LSE/LSI fallback，STOP 电流，IWDG 最坏情况。
- CAN 250k 位时序、周期广播、上位机实时监控和升级链路。

## 13. 状态变量净删减专项测试

专项文档：`docs/review/state_variable_audit.md`

| ID | 测试项 | 方法 | 通过标准 |
|---|---|---|---|
| T-SV-001 | 文档阶段一致性 | `rg "Q-SV-|REQ-SV-|SV-CLEAN" docs/review` | 状态变量专项在审计、确认、风险、计划和测试文档中都有入口 |
| T-SV-002 | 产品信息初始化收口 | 读 Modbus `0xC002` 48 个寄存器，并 `rg "su8_StartUpFlag|InitProID\\(|App_ProID_Deal"` | SN/HW/SW 默认或写入信息不丢失；源码中 `App_ProID_Deal()` 不再依赖一次性 flag |
| T-SV-003 | FactoryAging 结构体收口 | `rg "s_u8FactoryAging|s_u16FactoryAging|s_u32FactoryAging" "103 + 309/Project/Source/FactoryAging.c"`，并检查 `s_factory_aging` 字段 | 旧散落静态变量无残留；`state/elapsed10ms/lastTick/saveValid/retry/mosMode` 集中在 `s_factory_aging` |
| T-SV-004 | FactoryAging 老化功能回归 | 通过 CAN/上位机执行 start/stop/reset/set hours，抓 `0x14F80208` 老化状态和剩余分钟 | 结构体收口后老化状态、剩余时间、BKP/Flash 保存和 MOS 模式行为不变 |
| T-SV-005 | LogRecord 结构体收口 | `rg "BMS_LOG_POINT|BMS_LOG_RECORD|s_log_record_flag|s_u32_LogRecord|s_u8_LogRecord|su8_Event|su8_CBC_Temp" "103 + 309/Project/Source/LogRecord.c"`，并检查 `s_log_record` 字段 | 旧私有日志状态无残留；外部 `su32_Interval_S_Tcnt` 保留；日志格式和 Flash 保存接口不变 |
| T-SV-006 | LogRecord 日志功能回归 | 触发 startup/sleep/fault/CBC 日志，读事件记录并执行 reset event record | startup/sleep 日志、fault 边沿去重、CBC 变化记录、读取顺序和清空行为不变 |
| T-SV-007 | LedBar 显式初始化 | 源码阶段执行后观察上电启动显示、按键显示、`MCU_WK` 显示、TIM4 扫描 | 不重复初始化，不持续闪烁，显示窗口结束后熄屏并释放低功耗阻塞 |
| T-SV-008 | LedBar STOP 前 GPIO | 触发 `LedBar_PrepareForStop()` 后测 GPIO 和 STOP 电流 | LED 引脚进入低漏电安全态，STOP 前无残留扫描 |
| T-SV-009 | `readyToSleep` 收口 | 源码阶段执行后覆盖 HICCUP、NORMAL、DEEP 三类 sleep | sleep SOC 保存、`BMS_SLEEP` 日志、`SleepDeal_Continue()` 和 STOP 循环行为不变 |
| T-SV-010 | 低功耗 debug 快照 | 读取 `g_dbg.lp`、`g_stLowPowerRtcStatus` 或 ST-Link 监控输出 | 移出纯展示字段后，调试仍能看到 mode/block/elapsed 等必要信息 |
| T-SV-011 | 历史状态保留 | `rg` 确认按键/`MCU_WK` 防抖、SOC sample seq、AFE fault/recover 计数未被第一批删除 | 第一批净删减不碰真实历史状态 |
| T-SV-012 | DataDeal 客户逻辑隔离 | 文档阶段只确认需求，不改源码；源码阶段若拆分，先做等价调用链检查 | `charger_detect_and_keyLogi_200ms()` 和 `new_todo_logi()` 行为未在未确认前改变 |
| T-SV-013 | 静态检查 | 每批执行 `git diff --check`、`rg` 旧符号、可用时 `python3 tools/project_check.py --quiet` | 无新增 whitespace 错误；旧符号按预期消失；脚本结果与基线对比解释清楚 |
| T-SV-014 | 编译 | 可用 Keil 或等价静态检查时编译/检查涉及文件 | `FD_Release` 0 error；若缺 Keil/硬件，必须在结论中说明未验证 |

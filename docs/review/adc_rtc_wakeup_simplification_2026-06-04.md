# RTC 唤醒后 ADC 直接采样简化记录

文档状态：部分验证
源码验证：已按当前源码静态核对；Keil 编译和真板测试结果见 `docs/change_log.md`
最后更新时间：2026-06-04

## 参考源码

- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/ADC.h`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`

## 当前结论

用户已确认本轮按“去掉所有 MCU ADC 软件滤波，直接采样后计算结果”的方向修改。已实施范围只限 ADC 模块内部：

- 保留 STOP 前关闭 TIM2/ADC/DMA、唤醒后重新 `InitADC()` 的低功耗外设恢复路径。
- 删除 VBC、MOS 温度、Type-C 电流的软件累加平均和 IIR 滤波。
- `App_AnlogCal()` 改为 latest-sample 模式：有新的 10ms tick 时，只用当前 DMA raw 计算一次结果，不再补跑历史 tick。
- ADC 重新初始化后丢弃 1 个 10ms tick，避免刚恢复时第一组 raw 不稳定。
- Type-C 电流仍保留零点死区 `AD_CurZeroDeadband` 和换算限幅；这些属于最小保护，不属于运行态滤波。
- RF_EN 熔断保险丝逻辑使用 ADC Vbat 时，必须先确认 `ADC_IsReady()!=0`，避免 RTC 唤醒初期 ADC 未计算完成或异常首值参与不可逆动作。
- AFE 单体累加总压、AFE 电流 sample seq、SOC 主积分和 Modbus/CAN 协议字段含义保持不变。

## 当前采样时序

```text
RTC STOP 前:
  Conf_PrepareStopEntry()
    -> ADC_StopForLowPower()
       -> 停 TIM2 / ADC1 / DMA1

RTC STOP 唤醒后:
  InitRunAfterStopWakeup()
    -> ADC_StopForLowPower()
    -> InitADC()
       -> 清零 s_adc.raw/result/vbat/typec
       -> ADC_ResetAnlogCalSchedule()
          -> discard = 1 个 10ms tick
       -> InitADC_GPIO()
       -> InitADC_TIMER()
       -> InitADC_DMA()
       -> InitADC_ADC1()
    -> InitTimer()

运行态:
  Runtime_RunOnce()
    -> App_AFEGet()        // 200ms AFE/SOC 主链路
    -> App_AnlogCal()      // 最新 ADC raw 直接计算一次
```

## 已删除的延迟来源

| 数据 | 修改前 | 修改后 |
|---|---|---|
| ADC raw | TIM2 约 10ms 触发，DMA 写 `s_adc.raw[]` | 保持不变 |
| MOS 温度 | 每次查表后做 1/8 IIR，从 0 收敛 | 当前 raw 查表后直接写 `result[ADC_TEMP_MOS1]` |
| VBC/ADC 总压 | 8 点累加平均后再做 1/8 IIR | 当前 raw 直接换算分压和电池侧 mV |
| Type-C 电流 | 32 点累加平均，零点连续 3 次确认 | 当前 raw 直接换算 mV/mA，保留死区和最大值限幅 |
| `App_AnlogCal()` | 按 10ms tick catch-up，最多补跑 10 tick | latest-sample，一次调用最多计算一次 |

## 需求确认表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-ADC-WAKE-001 | RTC STOP 前必须关闭 ADC/TIM2/DMA，唤醒后重新初始化 ADC | `conf/conf.c`, `ADC_StopForLowPower()`, `InitRunAfterStopWakeup()` | 已保留 stop/reinit 路径 | 删除会影响 STOP 功耗和外设恢复确定性 | MUST_KEEP | 是否确认保留当前低功耗外设关闭/恢复路径？ | 保留 | 已确认并已执行 |
| REQ-ADC-WAKE-002 | RTC 唤醒后 ADC 最终值应尽快由有效样本恢复 | `InitADC()`, `App_AnlogCal()` | 已改为丢弃 1 个 tick 后直接计算 | 若首样本异常可能短时输出异常值 | CHANGE_NEEDED | 是否允许去掉从 0 慢收敛的滤波？ | 允许直接采样计算 | 已确认并已执行 |
| REQ-ADC-WAKE-003 | VBC/ADC 总压滤波可简化，但不能替代 AFE 单体累加总压 | `DataDeal.c`, `ADC_GetVbatMilliVolt()` | ADC VBC 只作为辅助值；AFE 总压主路径不变 | 误替代主总压会影响保护和上报 | MUST_KEEP | 是否确认 AFE 总压仍为主路径？ | 保持 AFE 总压主路径 | 已确认并已执行 |
| REQ-ADC-WAKE-004 | Type-C 电流可以从 32 点平均简化为直接计算 | `ADC_UpdateTypeCCurrent()`, `SOC.c` | 已改为 raw 直接换算，保留死区/限幅 | ADC 噪声可能更直接进入 Type-C 等效电流 | KEEP_BUT_REFACTOR | 是否允许 Type-C 直接按 raw/delta_mV 计算？ | 直接计算，保留最小保护 | 已确认并已执行 |
| REQ-ADC-WAKE-005 | ADC 重新初始化后应有首样本丢弃保护 | `ADC_ResetAnlogCalSchedule()` | 已增加 `discard`，丢弃 1 个 10ms tick | 丢弃过少需上板确认 | CHANGE_NEEDED | 是否允许增加少量内部状态做首样本保护？ | 只保留 `discard` 内部状态 | 已确认并已执行 |
| REQ-ADC-WAKE-006 | `App_AnlogCal()` 应降低对主循环调用频率的依赖 | `App_AnlogCal()` | 已改 latest-sample，不补跑滤波 tick | 若调用周期变低，结果刷新频率仍随调用降低，但不会积压滤波历史 | CHANGE_NEEDED | 是否确认改为最新 raw 更新一次？ | 改为 latest-sample | 已确认并已执行 |
| REQ-ADC-WAKE-007 | 简化 ADC 不应改变协议字段和 SOC 主电流样本序号 | `SOC.c`, `DataDeal.c`, `ADC_Get*()` | AFE/SOC 主链路不变 | 误改会导致积分或协议兼容问题 | MUST_KEEP | 是否确认只改 ADC VBC/Type-C/MOS 输出？ | 保持协议和 AFE/SOC 主路径不变 | 已确认并已执行 |

## 已实施方案

1. 删除 `ADC_RUNTIME` 中的 `filt[]`、Type-C/VBC 平滑计数和零点确认计数。
2. 删除 `ADC.h` 中已失效的 ADC 平均滤波宏，只保留 Type-C 死区和换算参数。
3. 用 `ADC_UpdateMosTemp()`、`ADC_UpdateVbc()`、`ADC_UpdateTypeCCurrent()` 替换旧的 `ADC_TTC()`、`ADC_Vbc()`、`ADC_Current_Smooth()`。
4. `App_AnlogCal()` 只在 tick 变化后处理一次最新 raw；ADC 恢复后的第一个 tick 只做丢弃，不生成最终值。
5. 不改 ADC 通道、DMA、TIM2 触发、GPIO、AFE 驱动、SOC 主采样序号和对外协议。

## 测试建议

| 测试项 | 方法 | 通过标准 |
|---|---|---|
| 冷启动 ADC | 上电后读取 `ADC_GetRaw()`、`ADC_GetVbatMilliVolt()`、`ADC_GetTypeCOutCurrentMilliAmp()` | raw 快速更新，最终值不再长时间从 0 慢收敛 |
| RTC STOP 唤醒 | 进入 HICCUP RTC STOP 后由 RTC 唤醒 | 唤醒后约 20ms 起可得到首个直接计算结果；200ms 内 VBC/MOS 温度应合理 |
| Type-C 插拔 | Type-C 负载接入/断开 | 接入响应不再有 32 点平均延迟；断开后死区内清零 |
| AFE 总压隔离 | 比较 `u16VCellTotle` 与 ADC VBC | `u16VCellTotle` 仍来自 AFE 单体累加，ADC VBC 不覆盖主总压 |
| SOC 积分 | Type-C 有/无输出，主回路充放电 | AFE sample seq 仍驱动主 SOC；Type-C 只作为附加等效放电 |
| 低功耗电流 | STOP 前后测功耗 | ADC 简化不破坏 STOP 前 TIM2/ADC/DMA 关闭 |
| RF_EN 熔断保护 | RTC STOP 唤醒后观察 `ADC_IsReady()`、`ADC_GetVbatMilliVolt()`、`GPIO_RF_EN` | ADC not ready 时 ADC Vbat 不参与 RF_EN 条件；AFE 错误分支计数必须连续满足条件才累计 |

## 剩余风险

- 直接采样会让 ADC 抖动更快反映到 VBC、MOS 温度和 Type-C 辅助电流，需要真板确认噪声是否可接受。
- Type-C 电流已保留死区和限幅，但不再做连续零点确认，需实测插拔和小电流边界。
- RF_EN 是不可逆安全动作，任何使用 ADC Vbat 的判断都必须带 `ADC_IsReady()` 或更强的连续有效判定；本轮已对当前 RF_EN 路径增加 ready 门控和连续计数清零。
- 本轮没有修改 AFE/SOC 主链路；若后续发现 SOC 仍异常，应单独排查 AFE CADC、Type-C 是否应计入 SOC、以及电流方向。

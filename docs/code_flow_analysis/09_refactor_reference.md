# 09 重构参考结论

状态：只给建议，不修改源码。所有判断基于当前源码链路和前面索引文档。

## 目录
- [复杂度排序](#复杂度排序)
- [高风险耦合](#高风险耦合)
- [适合优先处理的重构点](#适合优先处理的重构点)
- [不建议直接动的边界](#不建议直接动的边界)
- [人工确认清单](#人工确认清单)

## 复杂度排序

| 模块/文件 | 位置 | 复杂原因 | 建议 |
| --- | --- | --- | --- |
| 低功耗链路 | `rtc_sleep.c` + `SleepDeal.c` + `conf.c` + `RTC.c` + `rtc_sleep_port.c` | 同时存在运行期STOP/HICCUP和reset-sleep两条路径，blocker跨CAN/SCI/LED/Flash/故障/SOC | 先画状态机和blocker表，禁止一次性改行为 |
| SCI/Modbus寄存器桥 | `Sci_Upper.c` | 协议地址、读写窗口、参数保存、SOC/AFE/IAP副作用混在同一文件 | 先生成寄存器映射和副作用表，再做小步收口 |
| AFE/采样/保护/MOS | `DataDeal.c` + `SH367309_Func.c` + `SH367309_DataDeal.c` | 采样、错误恢复、保护、MOS、认证熔断和低功耗请求耦合 | 先重命名/拆分 `new_todo_logi()` 这类职责不清函数 |
| SOC状态机 | `SOC.c` + `SocEnhance.c` | 状态多，但顶层顺序已经比较清晰 | 保持 `command -> direction -> integrate -> sag/full/empty/rest -> save -> publish` 顺序 |
| CAN APP协议 | `Can_HDX.c` | TX队列、APP命令、读块流、老化/IAP和SCI桥合并 | 保留 s_tx/s_runtime/s_app 三个最小运行态，补协议表 |
| 全局数据报告 | `g_stCellInfoReport` / `OtherElement` / `System_ErrFlag` | 协议字段、运行状态、算法输入输出共用结构体 | 先文档化单位和字段消费者，不直接改布局 |


## 高风险耦合

- `new_todo_logi()`：名称不表达职责，却影响充电、MOS过温、认证熔断、AFE错误关断和低功耗请求。
- `Can_IsBusy()`：看似查询，实际会更新 `sys_time.last_ext_comm_cnt_can`。调试/快照用途应避免调用它。
- `OtherElement` 写入：部分offset会触发 `AFE_PARAM_WRITE_Flag`、`InitData_SOC()`、`SOC_RequestCapacityReset()` 和 `SeriesNum/g_u32CS_Res_AFE` 更新。
- `g_stCellInfoReport`：同时服务协议、SOC、日志、低功耗和LED；任何字段单位变化都会扩散。
- LED `s_ledbar`：TIM4 ISR和主循环共享，没有明显统一锁，后续改帧结构需小心。

## 适合优先处理的重构点

1. 文档先行：把 `Sci_Upper.c` 的寄存器范围、读写副作用和单位落成表。
2. 命名收口：把 `new_todo_logi()` 拆分/重命名为充电检测、MOS过温、认证熔断三个明确函数；先只移动代码，不改条件。
3. 数据单位注释：给 `g_stCellInfoReport`、`SOC_Enhance_Element`、`OtherElement` 关键字段补单位文档，避免协议层误改。
4. 低功耗状态机：把 `NO_SLEEP/HICCUP/NORMAL/DEEP`、blocker和唤醒源整理成一个权威文档，再决定是否净删减变量。
5. CAN APP协议：保留现有状态结构，补命令表和ACK状态表；后续只清理重复ACK/读块代码。

## 不建议直接动的边界

- 不直接调整 `App_AFEGet()` 内采样/SOC顺序。
- 不直接改 `SOC_IntEnhance_Ctrl()` 顶层顺序。
- 不直接改 Modbus寄存器地址、长度、字段含义。
- 不直接改 IAP地址和 `u8FlashUpdateFlag` 复位路径。
- 不直接删低功耗状态变量，如 `g_stLowPowerRtcStatus.readyToSleep/block/idle/cycles/sleep` 等，需先证明生命周期。

## 人工确认清单

- `Project_BuildGuard.h` 当前 release profile 与 debug宏默认/当前值是否符合预期。
- `Init_IWDG()` 被注释是否是临时状态，量产是否应启用硬件IWDG。
- `__EnableLowPowerDebug__` 在当前构建是否允许保留；若要测真实低功耗，必须关闭。
- `DISP_VBAT_AND_TEMP_` 把Type-C等效电流和ADC总压写入 `u16VCell[29/30]` 是否仍是客户可见协议需求。
- `new_todo_logi()` 中认证熔断 `_UL_RENZHENG_ENABLE_` 和阈值是否为当前量产需求。
- `firmware/comm_tool_f103ret6` 是否需要单独做通信工具固件流程分析；它不是BMS主App调用链的一部分。

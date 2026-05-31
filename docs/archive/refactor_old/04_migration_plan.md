# 迁移步骤

文档状态：部分验证  
目标：把当前工程迁移到更清晰的裸机 BMS 架构，同时保持功能、安全边界和协议兼容。  
硬性边界：用户确认前不修改固件源码；每一步必须小步、可验证、可回滚。

## 1. 迁移总原则

1. 先删死代码，再拆模块，最后才考虑优化算法。
2. 每次只改一个边界：例如只改 `system_time`，不要同时动 CAN 和低功耗。
3. 先保持函数签名和行为，再改目录结构。
4. 任何协议寄存器、CAN ID、Flash 地址、AFE 时序、保护阈值改动都必须单独确认。
5. 每个阶段必须有：
   - 修改范围
   - 禁止修改项
   - 编译/静态检查
   - 实机验证条件
   - 回滚方式
6. 迁移期间保留 `Project_BuildGuard.h`，不能绕过量产配置检查。

## 2. 迁移前基线

### M0：建立基线，不改源码

目标：

- 固化当前行为基准，作为后续重构对照。

建议动作：

- 记录当前 git commit 和分支。
- 运行 `python3 tools/project_check.py`。
- 在 Windows/Keil 环境编译 `FD_Release`。
- 生成并保存 map/bin/hex 产物信息。
- 确认 `FD_Release.sct` App 起始地址仍为 `0x08004800`。
- 若有硬件，读取：
  - `COM4/19200/slave=1`
  - `0xD000` 板端状态
  - `0xD300` SOC 测试支持状态
  - `0xC002` SN/硬件/软件版本
- 若有 CAN，记录飞道 CAN 周期帧和老化剩余时间帧。

禁止修改：

- 禁止改源码。
- 禁止烧录未知地址。
- 禁止写 `0x08000000`。

验证：

- `tools/project_check.py` 无 Error。
- Keil Release 编译通过。
- 实机基础通信可读。

回滚：

- 无源码修改，只需保留记录。

## 3. 第一批低风险净删减

### M1：删除已确认的死代码和历史注释

前置条件：

- 用户逐项确认 `02_delete_or_keep_list.md` 中 D/C 类候选。

候选范围：

- `App_WarnCtrl` 残留声明和注释调用。
- `test_Autocurrent_cycle()`。
- `SH367309_SC_DelayT_Set()`。
- `ShortFunc.c/h`。
- 伪 EEPROM API。
- 明确无用的 `#if 0` 旧实现。
- AFE I2C 旧包装分支。

禁止修改：

- 禁止改保护阈值。
- 禁止改 AFE MTP 默认参数。
- 禁止改 Modbus/CAN 协议。
- 禁止改 Flash 地址。
- 禁止改低功耗状态机。

验证：

- `git diff --check`
- Keil Release 编译。
- `tools/project_check.py`
- 若删除文件，确认 Keil 工程中不再引用。

回滚：

- 单独 commit；异常时 revert 本 commit。

## 4. 系统时基与调度整理

### M2：收口 `system_time`

目标：

- 把 `TIM3_IRQHandler`、task flags、10ms tick、period take API 作为 `system_time` 边界。
- 保留主循环任务顺序。

迁移范围：

- `System_Init.c/h`
- `Runtime.c`
- `conf.h:Time_T` 中时间相关字段

禁止修改：

- 禁止改 `b1Sys10msFlag/b1Sys50msFlag/b1Sys100msFlag/b1Sys200msFlag/b1Sys1000msFlag` 语义。
- 禁止改 `SysTime_Take200msTaskPeriod()` 行为。
- 禁止改 `App_AFEGet()` 调用周期。

验证：

- Keil 编译。
- 通过调试计数或日志确认 10ms/200ms/1000ms 任务节奏不变。

回滚：

- 单独 commit。

## 5. 存储边界整理

### M3：拆 `storage_flash` 和 `param_store`

目标：

- 把 Flash 物理擦写、typed storage、默认参数、升级策略分开。
- 保持 Flash 地址、magic、version、CRC 和双槽 fallback 完全不变。

迁移范围：

- `Flash.c/h`
- `EEPROM.c/h`

禁止修改：

- 禁止改 `FLASH_APP_ADDR`、storage page address。
- 禁止改 SOC/AFE/RW/log/aging 数据结构布局。
- 禁止改 `App_FlashUpdate()` 进入 IAP 前关 MOS 和 reset 行为。

验证：

- Keil 编译。
- 运行 `tools/project_check.py`。
- 有硬件时执行参数读写、断电恢复、SOC snapshot 保存/读取。

回滚：

- 单独 commit；保留原 Flash 文件对照。

## 6. AFE driver 边界整理

### M4：拆 SH367309 driver/config/monitor

目标：

- `afe_driver` 只负责读写 AFE。
- `protection` 负责 fault mapping。
- `mos_ctrl` 负责 MOS 状态输出。

迁移范围：

- `I2C_AFE1.c/h`
- `SH367309_Func.c/h`
- `SH367309_DataDeal.c`
- `rtc_sleep_afe_sh367309.c`

禁止修改：

- 禁止改 I2C bitbang 时序。
- 禁止改 MTP 写入顺序、延时和验证。
- 禁止改 `InitAFE1()` 中初始 MOS 策略。
- 禁止改 AFE fault 位到 BMS fault 位映射。

验证：

- Keil 编译。
- 读 AFE 电压/温度/电流正常。
- 人工触发 AFE 通信错误时系统错误位一致。
- 确认 STOP 唤醒后 AFE 重新初始化正常。

回滚：

- 单独 commit。

## 7. BMS 核心、保护、MOS 拆分

### M5：拆 `DataDeal.c`

目标：

- 从 `DataDeal.c` 拆出：
  - 采样数据转换：`bms_core`
  - 保护判断：`protection`
  - MOS/charger/认证策略：`mos_ctrl`
- 保持 `App_AFEGet()` 外部行为不变。

重点对象：

- `DataLoad_CellVolt`
- `DataLoad_Temperature`
- `DataLoad_Current`
- `MonitorAFE`
- `new_todo_logi`
- `open_ctlc/close_ctlc`

禁止修改：

- 禁止改 13 串映射逻辑。
- 禁止改温度、电流、低压、MOS 过温阈值。
- 禁止删除 `_UL_RENZHENG_ENABLE_` 行为，除非用户确认。
- 禁止改 `MCUO_RF_EN`、`GPIO_MCC_C` 输出时机。
- 禁止改 AFE error 下休眠策略。

验证：

- Keil 编译。
- 采样寄存器读数一致。
- MOS 启动状态一致。
- 充电器插拔行为一致。
- 低压/过温/AFE error 场景实机验证。

回滚：

- 分成多个小 commit：
  1. 只移动采样转换。
  2. 只移动保护映射。
  3. 只移动 MOS 策略。

## 8. SOC 边界整理

### M6：收口 SOC 输入输出

目标：

- SOC 算法保持不变。
- 输入改为明确样本结构。
- 输出通过 `SocOutput` 或 publish API 更新报告。

迁移范围：

- `SOC.c`
- `SocEnhance.c/h`

禁止修改：

- 禁止改 OCV 表。
- 禁止改 full/empty anchor。
- 禁止改 rest compensation。
- 禁止改 display smoothing。
- 禁止改 SOC snapshot 保存条件。

验证：

- host 侧 SOC 场景回放。
- 充电、放电、静置、RTC sleep rest compensation 对比。
- `0xD000/0xD300` 输出一致。

回滚：

- 单独 commit。

## 9. 通信协议整理

### M7：拆 Modbus register map

目标：

- 把寄存器范围、读写权限、handler 从 `Sci_Upper.c` 中抽出。
- 保留当前 Modbus RTU 行为。

禁止修改：

- 禁止改寄存器地址。
- 禁止改每个 block 的长度。
- 禁止改读写权限和错误码。
- 禁止改 `0xC002` 48 个寄存器来源。

验证：

- 上位机全功能读写。
- 脚本读取 `0xD000`、`0xD300`、`0xC002`。
- 写保护参数、写 SN/版本、IAP 请求行为一致。

### M8：拆 CAN service

目标：

- 把 CAN hardware、TX queue、周期帧、App 命令拆开。
- 保留 `Can_IsBusy()`、`Can_PrepareSleep()`、`Can_RtcWakeService()` 行为。

禁止修改：

- 禁止改 CAN 波特率配置。
- 禁止改周期帧 ID 和数据含义。
- 禁止改 App command CRC 和 ACK 格式。
- 禁止改老化命令 guard。

验证：

- CAN 周期帧抓包对照。
- CAN App read/write/IAP/aging 命令测试。
- bus-off 和 mailbox no-ack 场景观察。
- RTC sleep 周期唤醒 CAN 广播验证。

回滚：

- Modbus 和 CAN 分开 commit，不能混改。

## 10. 低功耗整理

### M9：拆低功耗状态机

目标：

- 把 blocker 判断、RTC hiccup sleep、reset sleep、wake restore 分开。
- 清楚定义每个 blocker 来源。

迁移范围：

- `app_lowpower.c`
- `rtc_sleep.c`
- `rtc_sleep_port.c`
- `SleepDeal.c`
- `LowPowerSleep.c`

禁止修改：

- 禁止改 BKP DR2/DR3 boot flag 含义。
- 禁止改 BKP DR4/DR5 LED sleep SOC。
- 禁止改 FactoryAging BKP DR6-DR10。
- 禁止改唤醒源判断顺序。
- 禁止改 `Can_RtcWakeService()` 超时窗口。
- 禁止改 `SleepDeal_Continue()` 中 AFE sleep 和 MCU reset 顺序。

验证：

- 普通休眠、深度休眠、RTC hiccup sleep。
- 按键唤醒、充电器唤醒、电流唤醒、AFE fault 唤醒。
- LED 睡眠 SOC 快显。
- SOC RTC rest compensation。
- CAN RTC 唤醒广播。

回滚：

- 低功耗必须单独分支和 commit，不与其他模块同时改。

## 11. LED、老化、日志收尾

### M10：LED 显示整理

目标：

- 扫描驱动和显示策略分开。
- 长按按键只发低功耗请求，不直接散落调用 sleep。

禁止修改：

- 禁止改 TIM4 扫描频率。
- 禁止改 Charlieplexing 路由。
- 禁止改 SOC 显示时间和睡眠快显。

### M11：老化和事件日志整理

目标：

- 老化状态机保留，抽出 storage/mos 依赖。
- 事件日志 ring buffer 和 Flash 节流保留。

禁止修改：

- 禁止删除老化剩余时间可见性。
- 禁止改 CAN `0x14F80208` 老化剩余时间广播含义。
- 禁止改事件记录读取顺序。

验证：

- 老化开始/停止/重置/设置时长。
- 断电/睡眠后老化进度恢复。
- 事件记录读写和清除。

## 12. 最终目录迁移

### M12：文件目录迁移

只有当前面模块边界已通过编译和实机验证后，才移动文件到目标目录。

动作：

- 更新 Keil 工程文件。
- 更新 include path。
- 删除旧路径中的空壳文件。
- 更新 `docs/change_log.md` 和测试记录。

禁止修改：

- 禁止在目录移动时顺手改逻辑。

验证：

- Release 编译。
- Debug/Factory 如仍保留也要编译。
- `tools/project_check.py`。
- 基础实机通信和 CAN/低功耗 smoke test。

## 13. 推荐提交粒度

推荐 commit 序列：

1. `docs: add BMS architecture refactor analysis`
2. `refactor: remove confirmed dead code`
3. `refactor: isolate system time services`
4. `refactor: split storage flash boundaries`
5. `refactor: split SH367309 driver boundary`
6. `refactor: isolate BMS sample pipeline`
7. `refactor: isolate MOS control policy`
8. `refactor: isolate SOC input output`
9. `refactor: extract Modbus register map`
10. `refactor: split CAN app command service`
11. `refactor: isolate low power state machine`
12. `refactor: split LED scan and display policy`
13. `refactor: reorganize source directories`

每个 commit 都应能独立编译，不能把多个高风险模块塞进同一个提交。

## 14. 当前阻塞条件

进入源码重构前需要用户确认：

- 删除清单中 C/D 类候选是否允许删除。
- 当前硬件是否只使用 SCI1。
- ENV2/ENV3 是否未贴片。
- `__VIRTURE_CURRENT__` 是否仍用于量产/现场调试。
- Flash64K 测试是否移出主业务工程。
- 低功耗强制深睡 2800mV/60s 是否已实机确认。
- 老化流程是否必须继续默认启用。

这些问题未确认前，只能继续做文档、依赖图、测试脚本和只读审查。

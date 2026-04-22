# STM32F1 BMS 数码管接入说明（74HC595，OE不可控）

## 1. 目标与约束
- 当前板卡使用 `74HC595`，`OE` 不可由 MCU 独立控制。
- 不能按理想 Charlieplexing 三态模型做每线高阻切换。
- 扫描顺序固定为：`全灭 -> 目标帧 -> 全灭 -> 下一目标帧`。
- 业务层与物理扫描层解耦：业务负责生成显示帧，扫描层负责时序输出。

## 2. 实际接入点
- 业务刷新入口：`E:\TODO\103 + 309 - 副本 - 副本\103 + 309\Project\Source\LedBar.c` 中 `APP_LedBar()`
- 1ms 扫描入口：`APP_LedBar()` 内 `g_st_SysTimeFlag.bits.b1Sys1msFlag` 条件下调用 `LedBar_Scan1ms()`
- 74HC595 GPIO 复用：`GPIO_LED595_DATA / GPIO_LED595_CLK / GPIO_LED595_LATCH`

## 3. 扫描状态机（OE不可控受限扫描）
`LedBar_Scan1ms()` 实现以下状态：
1. `LEDBAR_SCAN_STATE_OFF_PRE`：输出全灭帧
2. `LEDBAR_SCAN_STATE_OUTPUT_TARGET`：输出当前目标帧
3. `LEDBAR_SCAN_STATE_HOLD_TARGET`：保持目标帧（非阻塞 tick 计数）
4. `LEDBAR_SCAN_STATE_OFF_POST`：再次输出全灭帧
5. `LEDBAR_SCAN_STATE_NEXT_ITEM`：切换到下一个目标帧并在安全点处理前后缓冲切换

该状态机保证不会直接执行 `目标帧A -> 目标帧B`。

## 4. 业务层与扫描层分离方式
- 业务层（frame 生成）：
  - `LedBar_RebuildFrame()`
  - `LedBar_BuildTargetMask()`
  - `LedBar_CopyFramePatternsToBuffer()`
  - `LedBar_BuildGreedyFrameToBuffer()`
- 扫描层（物理输出）：
  - `LedBar_595WriteByte()`
  - `LedBar_OutputPattern()/LedBar_OutputOff()`
  - `LedBar_Scan1ms()`

## 5. 前后缓冲
- `front`：扫描层当前读取的帧列表
- `back`：业务层重建的新帧列表
- `pending`：待切换标志
- 在 `LEDBAR_SCAN_STATE_NEXT_ITEM` 安全点调用 `LedBar_CommitBackFrameIfPending()` 切换，避免扫描过程读取半更新数据。

## 6. 单段测试模式
新增接口：
- `LedBar_EnableSingleSegmentTest(uint8_t enable)`
- `LedBar_SetSingleSegmentIndex(uint8_t segment_id)`
- `segment_id` 范围由 `LEDBAR_SINGLE_SEG_ID_MIN` 到 `LEDBAR_SINGLE_SEG_ID_MAX`（当前 `0~17`）

开启后仅输出单个逻辑段，便于上板逐段核对。

## 7. 低功耗联动
- 新增接口：
  - `LedBar_SetSleep(uint8_t enable)`
  - `LedBar_Wakeup(void)`
- 当前在 `APP_LedBar()` 中使用 `Sleep_Mode.bits.b1_ToSleepFlag` 做显示熄屏联动。
- 熄屏时会重建空帧并输出全灭。

## 8. TODO（保留项）
- TODO：`Sleep_Mode.bits.b1_ToSleepFlag` 与真实系统进入/退出 sleep 的时序需要上板确认，当前只做显示联动，不反向影响系统休眠决策。
- TODO：单段 `segment_id` 与物理丝印段位的一一对应关系需要结合实物点灯结果固化到测试文档。

## 9. 本次最小编译验证
- 使用 `arm-none-eabi-gcc` 对以下文件完成语法级检查：
  - `Source/LedBar.c`
  - `Source/main.c`
- 命令带入工程 include 路径与 `STM32F10X_MD` / `USE_STDPERIPH_DRIVER` 宏，检查结果通过。

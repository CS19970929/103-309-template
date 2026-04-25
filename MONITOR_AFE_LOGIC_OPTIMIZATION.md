# MonitorAFE 逻辑优化说明

## 背景

`MonitorAFE()` 由 `App_AFEGet()` 在 200ms 任务周期内调用，负责 AFE 通信失败计数、恢复重试、系统错误标志维护，以及 AFE/EEPROM 长时间异常后的休眠保护。

原实现中 AFE1/AFE2 分支逻辑重复，阈值散落在函数内部；休眠注释为等待 5min，但实际使用 `5 * 60` 次调用计数，在 200ms 周期下约为 60s。

## 本次优化

1. 将 AFE 监控阈值集中为宏：
   - `MONITOR_AFE_FAIL_LIMIT`：连续异常超过 50 次后清空寄存器缓存并上报通信错误。
   - `MONITOR_AFE_RECOVER_TRIGGER`：异常计数到 30 次时触发一次 AFE 恢复动作。
   - `MONITOR_AFE_WAKE_RETRY_LIMIT`：恢复动作最多累计 20 次，避免异常状态下无限重初始化。
   - `MONITOR_AFE_SLEEP_DELAY_TICKS`：按 200ms 周期换算 5min，当前为 1500 tick。

2. 抽取公共处理函数：
   - `MonitorAFE_UpdateChannel()` 统一处理计数递增/递减、恢复重试、状态位和错误标志。
   - `MonitorAFE_UpdateSleepDelay()` 统一处理 AFE/EEPROM 持续异常后的休眠倒计时。
   - AFE1/AFE2 仅保留通道选择和各自恢复动作差异。

3. 修正行为细节：
   - 5min 休眠保护从 300 次调用修正为 1500 次 200ms 周期调用。
   - AFE2 成功恢复时也清除 `ERROR_REMOVE_AFE2`，与 AFE1 行为保持一致。
   - 恢复重试上限从原来的 `<= 20` 调整为 `< 20`，实际最多执行 20 次。

## 验证关注点

1. AFE 通信恢复后，`SystemStatus.bits.b1Status_AFE1` 应恢复为 1，`ERROR_STATUS_AFE1` 应清零。
2. AFE 连续异常到 30 次时应触发恢复动作；超过 50 次后应清空寄存器缓存并上报错误。
3. AFE 或 EEPROM 错误持续存在时，应约 5 分钟后进入 `NORMAL_MODE` 休眠流程。
4. 串口忙时 `App_AFEGet()` 会跳过 200ms 任务，因此休眠倒计时会随任务跳过而暂停。

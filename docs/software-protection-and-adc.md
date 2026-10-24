# 软件保护与 ADC

## ADC 与 RTC

- 当前只有 PB1 / ADC1 Channel 9 的 MOS NTC 输入。新增采样只扩展 `ADC_INPUT` 和 `s_adc_inputs`，每路软件触发单次转换，不使用 TIM2、DMA 或 ADC 中断。
- RUN 主循环先执行 `ADC_Task`，再消费完整的新采样组。启动配置和有界校准结束后等待稳定，丢弃一次真实转换，随后取 8 次转换、去掉最大最小值后平均。每轮采样完成后间隔 100 ms。
- 只有确认 EOC 且未超时的数据才能进入采样组；零值、满量程、NTC 表范围外和非法校准结果只表示采样不可用，不能直接视为 MOS 高温。上报保留最近有效温度，但保护不得拿这个缓存推进确认时间。
- `DataLoad_MosTemperature` 是 MOS NTC 查表、原有 K/B 校准和上报编码的唯一入口，保持 `(摄氏度 + 40) * 10` 编码和原校准含义。
- 进入 STOP 时停止 ADC、清除未完成组和待消费样本。RTC 周期检查只读取 AFE 温度。只有真正恢复 RUN，完成时钟、GPIO、通信和系统计时恢复后，才重新初始化 ADC；禁止在 RTC 检查阶段采样 MOS。

## 验证边界

`python tools/test_protection_refactor.py` 执行真实 C 源码的 host 回归，硬件接口使用 stub。`python tools/check_bms_no_bin.py --target FD_Release`（或 `FD_Debug`）在用户临时目录编译、链接，关闭用户构建钩子与 HEX 输出，不生成 BIN/HEX，不烧录。

host 和编译结果不代表板上验收。仍需用实际 NTC、供电和示波器确认冷启动与 STOP/RUN 恢复的采样稳定性，以及温度触发、恢复和物理 MOS 动作。

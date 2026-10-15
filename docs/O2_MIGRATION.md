# ARMCC O2 迁移说明

日期：2026-10-15
工程：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
目标：`Target 1`，ARM Compiler 5.06 update 7

## 配置结论

- 工程已固定为 Keil `Level 2 (-O2)`：`Optim=3`。
- `Optimize for Time` 保持关闭：`oTime=0`，即继续以 `Ospace` 为优化目标。
- `<default>` 在当前 ARMCC 5 工具链没有上级覆盖时通常落到编译器默认 `-O2`，但它表示继承/隐式默认，不是锁定的 O2。工程因此使用显式 `Level 2`。
- 任务开始时工作树为 `Optim=0`（`<default>`）；当前提交基线中的 `Optim=1` 对应 Keil `Level 0 (-O0)`。这与口头所述“原 O1”不一致，迁移以实际工程文件为准。

## 源码调整

1. 将空循环延时改为现有 SysTick 延时：
   - AFE 软件总线 `Delay4us()` 使用 `__delay_us(4U)`；
   - `Delay_Base10us()` 和 `Delay1ms()` 分别使用 `__delay_us()`、`__delay_ms()`。
   这样不依赖优化器是否保留无副作用循环。
2. 明确 ISR 与主循环共享状态：
   - 定时计数、SOC/RTC 周期事件、USART 解析状态与通信错误计数使用 `volatile`；
   - 周期事件通过短临界区原子取走，避免 ISR 置位与主循环清零相互覆盖；
   - CAN 接收中断只发布已接收的 ID 和 pending 状态，主循环在短临界区中取走。
3. 消除优化后可能暴露的未定义访问：
   - 不再把结构体的首个标量成员当数组跨成员递增；
   - EEPROM、协议参数、RTC、保护参数和故障计数统一按对象字节布局读写 `UINT16`；
   - 产品 ID 不再把字节数组强制转换成可能未对齐的 `UINT16 *`；
   - AFE 参数通过明确的 `AFE_Value_Typedef` 元素定位访问。
4. `Registers_AFE1` 初始化改为对完整对象 `memset()`，不再依赖相邻成员布局。

上述修改不改变 Modbus 地址、数据缩放、字节序、EEPROM 地址/布局、Bootloader/APP 边界或外部协议。

## 验证结果

- `python -B tests/test_o2_compatibility.py`：通过。
- `python -B tests/check_armcc_no_bin.py`：61 个对象以 ARMCC `-O2` 编译并链接 AXF 通过。
- 最终尺寸：`Code=42324`、`RO-data=2948`、`RW-data=1300`、`ZI-data=5776`。
- 电流校准：22 个命名用例、10000 个随机用例、65536 个 CADC 输入通过。
- RTC GPIO、SOC 容量、启动/AFE、Sleep/RTC/STOP host 回归全部通过。
- 反汇编确认 `Delay1ms` 跳转到 `__delay_ms`；AFE 延时代码在 O2 下调用 `__delay_us`。
- 仍有 11 条既有编译告警，均不是本次 O2 迁移引入；本次不扩大范围清理。

验证只生成用户临时目录中的 AXF/OBJ/日志，没有生成或覆盖 BIN/HEX，没有烧录或操作硬件。上板验证仍需覆盖 AFE 通信时序、UART/CAN 接收、SOC/RTC 周期任务、STOP 唤醒和 MOS/保护路径。

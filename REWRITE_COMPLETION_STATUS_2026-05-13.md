# 103 + 309 重写完成度与剩余边界

本文说明 `codex/full-rewrite-2026-05-12` 分支是否已经可以替代旧应用层，以及 `CI` / `port` 的含义。

## 1. 结论

当前已经完成：

- 旧 `103 + 309/Project/Source` 应用层 C/H 源码删除。
- 新业务代码集中在 `firmware_rewrite/src`。
- Keil 工程文件已经引用 rewrite core 和 STM32F1 port，不再引用旧 `..\Source`。
- Keil 工程文件已经替换旧 `stm32f10x_it.c` / `system_stm32f10x.c`，不再依赖旧 `main.h`。
- App 起始地址保持 `0x08004800`，不覆盖 `0x08000000` IAP。
- host 行为测试、CMake 构建、Keil 工程引用检查、ARM GCC Cortex-M3 只编译门禁可以通过。

当前还不能声明：

- 已经完美替代旧板端量产固件。
- 已经通过 Windows/Keil 实编译。
- 已经通过真实 103 + 309 板端 AFE、Flash、CAN、RTC、LED、RS485 上板验证。

原因是 `firmware_rewrite/ports/stm32f1_spl` 现在已经有默认 STM32F1 board 实现，但 AFE/ADC 标定、MOS 控制极性、LED Charlieplexing、RTC Alarm 唤醒电流与时钟恢复、CAN ACK 行为和 RS485 电气方向仍必须上板实测确认。没有实测前，不能把“可编译”当作“量产等价”。

## 2. “port” 是什么意思

`port` 是硬件适配层。

clean-room core 只处理业务逻辑：

- SOC 怎么积分、校准、显示。
- 保护状态怎么判断。
- CAN/RTC 策略怎么决策。
- 通信地址怎么读写。
- 量产 `0xD300 supported=0` 怎么保持测试入口隔离。
- IAP 请求怎么进入状态。

port 负责把真实硬件接到这些逻辑上：

- `bms_stm32f1_board_*` 是当前 STM32F1 port 的板级函数前缀。
- 从 AFE/ADC/DI1/`GPIO_MCU_WK` 生成 `bms_sample_t`。
- 把 SOC 快照写入 `0x0801E000` / `0x0801E800`。
- 把 core 的 MOS 状态输出到实际 GPIO 或 AFE 控制位。
- 把 CAN status/probe 发送到真实 CAN 外设。
- 按 core 给出的周期配置 `RTC Alarm + EXTI17` 并进入 STOP。
- 把显示状态刷新到真实 LED/数码管。
- 收到 `0xFFFD` IAP 请求后写标志并复位。

这样做的目的，是防止业务逻辑和硬件寄存器互相缠绕。以后换板、换 AFE、换显示，只改 port，不改 core。

## 3. “CI” 是什么意思

`CI` 在这里不是云服务本身，而是一组固定门禁命令。

本仓库当前的本地 CI 入口是：

```bash
python3 tools/run_rewrite_ci.py
```

它会依次执行：

1. `git diff --check`
2. `python3 tools/project_check.py --quiet`
3. `python3 tools/check_rewrite_keil_project.py`
4. `python3 tools/build_rewrite_arm_gcc.py`
5. `python3 tools/run_rewrite_host_tests.py`
6. `cmake -S firmware_rewrite -B build/firmware_rewrite_cmake`
7. `cmake --build build/firmware_rewrite_cmake`
8. `ctest --test-dir build/firmware_rewrite_cmake --output-on-failure`

这些命令保证：

- 旧 Source 不会重新混入。
- 关键地址和通信契约不丢。
- Keil 工程仍然指向 rewrite 文件。
- rewrite core + STM32F1 port 可以被 `arm-none-eabi-gcc` 按 Cortex-M3 目标编译成 object。
- host 行为测试通过。
- CMake 可构建。

Windows/Keil 实编译不属于 Mac 本地 CI，因为 Keil MDK 需要 Windows。Windows 上使用：

```powershell
.\tools\build_rewrite_keil.ps1 -Build
```

## 4. Keil 编译状态

仓库已完成 Keil 工程引用替换，并能通过 XML/路径/地址检查。
仓库也已经用 `arm-none-eabi-gcc` 对 rewrite core、STM32F1 board、system、interrupt 文件做了只编译门禁。

当前机器是 macOS，没有 `UV4.exe`、`ARMCC`、`ARMCLANG`、`fromelf`，所以不能在本机给出 Keil 实编译成功结论。

Windows 上的验收条件：

1. 打开或命令行构建 `103 + 309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx`。
2. 构建 `FD_Release`。
3. 确认无 error。
4. 确认 map/scatter 中 App 起点是 `0x08004800`。
5. 使用 `tools\soc_flash_app_safe.ps1` 烧录，禁止裸写 `0x08000000`。

## 5. 完美替代旧代码的剩余工作

要达到“板端完美替代”，还必须完成：

1. 上板校准 `bms_stm32f1_board_read_sample()` 的电压分压、电流零点、温度和 AFE 接入。
2. 上板确认 Flash 双槽保存和恢复。
3. 上板确认 RS485 `0x03/0x06/0x10` 读写 `bms_comm_*()`。
4. 上板确认 CAN status/probe 发送与 ACK 反馈。
5. 上板确认 `RTC Alarm + EXTI17` STOP 进入、周期唤醒和 `bms_app_apply_rtc_wake()`。
6. 上板确认 LED/DI1/`GPIO_MCU_WK` 刷新。
7. 上板确认 MOS 输出到真实硬件控制位的极性。
8. Windows/Keil 编译通过。
9. 安全脚本烧录 App 到 `0x08004800`。
10. 上板验证通信地址、SOC、保护、低功耗、IAP、显示体验。

这些不是“兜底写法”，而是真实硬件接线和量产验证。没有这些结果，不能把 host 通过等同于出货固件通过。

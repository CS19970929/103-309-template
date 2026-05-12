# BMS Clean-Room Rewrite

这是 `103 + 309` 项目的从零重写实验区。这里不复用旧应用层代码结构，只保留仓库文档中已经明确的产品需求、外部通信契约、SOC 用户体验和安全边界。

## 目标

- 裸机单主循环，不引入 RTOS。
- 一个 `bms_app_t` 上下文统一承载运行状态。
- SOC、通信、低功耗、CAN、存储各自内聚。
- 自动 SOC 校准每次最多 `1%`。
- 通信地址保持现有上位机兼容。
- host C 测试先行，硬件 port 后置。

## 当前目录

```text
include/bms_app.h              对外 API 和核心数据结构
CMakeLists.txt                 host/CI 构建入口
src/bms_app.c                  App 门面与统一入口
src/bms_soc.c                  SOC 策略和显示体验
src/bms_comm.c                 RS485/Modbus-like 地址兼容层
src/bms_power_can.c            低功耗判定和 CAN 有/无设备策略
src/bms_storage.c              SOC 双槽快照
tests/test_rewrite_core.c      host 行为测试
```

## 验证

从仓库根目录运行：

```bash
python3 tools/run_rewrite_host_tests.py
```

也可以直接使用 CMake：

```bash
cmake -S firmware_rewrite -B build/firmware_rewrite_cmake
cmake --build build/firmware_rewrite_cmake
ctest --test-dir build/firmware_rewrite_cmake --output-on-failure
```

当前 host 测试覆盖 `0x1005`、`0xD000`、满电到 `100%`、低压到 `0%`、静置 OCV deferred target、大电流 sag holdoff、CAN RTC 周期和 SOC 快照恢复。

## 接板原则

后续新增 `ports/stm32f1_spl/` 时，port 层只负责硬件输入输出：

- AFE/ADC 采样转成 `bms_sample_t`。
- RS485/CAN 协议解析后调用 `bms_comm_*()`。
- RTC 唤醒后调用 `bms_app_apply_rtc_wake()`。
- Flash driver 映射 `bms_storage_t` 双槽。
- LED/DI1 按体验契约接入，不改变 SOC core。

不要把旧 `main.c`、旧低功耗状态机或旧 CAN 调度写法搬进该目录。

## 旧代码边界

旧 `103 + 309/Project/Source` 应用层源码已退役删除。当前分支只保留厂商 SPL/启动文件、安全烧录脚本和历史文档；后续所有功能都应在 `firmware_rewrite` 或新 port 层补齐。

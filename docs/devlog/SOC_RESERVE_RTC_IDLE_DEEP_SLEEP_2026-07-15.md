# SOC 预留容量与 RTC 空闲超时深睡说明

## 1. 修改目标

本次修改实现两项功能：

1. SOC 按“当前满电容量 - 预留容量”计算，默认预留 1Ah，对外满电容量不变。
2. 电池连续处于无异常 RTC 低功耗状态达到配置时间后，自动转入深度休眠，默认时间为 48 小时。

本次不修改 Modbus/CAN 寄存器、CAN ID、通信帧格式、保护阈值和 App/IAP 地址。

## 2. 可配置参数

配置文件：`103 + 309/Project/Source/conf/Project_Config.h`

| 宏 | 默认值 | 单位 | 说明 |
| --- | ---: | --- | --- |
| `PROJECT_CFG_SOC_RESERVE_CAPACITY_AH10` | `10` | 0.1Ah | SOC 预留容量，`10` 表示 1Ah，`0` 表示不预留 |
| `PROJECT_CFG_RTC_IDLE_TO_DEEP_SLEEP_HOURS` | `48` | 小时 | 连续无异常 RTC 低功耗转深睡的时间 |

`Project_BuildGuard.h` 已增加范围检查：

- SOC 预留容量：`0..65000`，即 `0..6500Ah`。
- RTC 空闲转深睡时间：`1..65535` 小时。

## 3. SOC 预留容量逻辑

### 3.1 容量定义

- `cap_factory_as10`：工厂额定容量。
- `cap_full_as10`：按 SOH 修正后的当前满电容量，继续用于对外上报。
- `cap_usable_as10`：SOC 内部计算容量，按以下规则生成：

```text
cap_reserve = PROJECT_CFG_SOC_RESERVE_CAPACITY_AH10 * 0.1Ah
cap_usable  = cap_full - cap_reserve
```

如果运行时容量参数异常，导致预留容量大于或等于当前满电容量，代码会安全禁用本次预留，避免容量为 0 或除 0。

### 3.2 SOC 计算与容量上报

库仑积分、SOC 百分比、设置 SOC 和满电锚定全部使用 `cap_usable_as10`。

对外容量保持原有量级：

- `u16CapacityFactory` 仍上报工厂额定容量。
- `u16CapacityFull` 仍上报按 SOH 修正后的当前满电容量。
- `u16CapacityNow` 将内部可用容量等比映射回对外满电容量，保证 SOC=100% 时当前容量与满电容量一致，SOC=0% 时当前容量为 0。

27Ah、预留 1Ah 的典型行为：

| 状态 | SOC 内部可用容量 | 对外满电容量 | 对外当前容量 | SOC |
| --- | ---: | ---: | ---: | ---: |
| 充满 | 26Ah | 27Ah | 27Ah | 100% |
| 放出约 13Ah | 13Ah | 27Ah | 约 13.5Ah | 约 50% |
| 放出约 26Ah | 0Ah | 27Ah | 0Ah | 0% |

此时电芯仍保留约 1Ah 物理容量，不计入用户可见 SOC。

### 3.3 Flash 快照兼容

SOC Flash 结构格式版本保持 `FLASH_STORAGE_SOC_DATA_VERSION_V2`，没有改变 Flash 布局。

- 新快照的 `u32CapFull` 保存当前 SOC 计算基准 `cap_usable_as10`。
- 启动时如果快照计算基准与当前宏配置一致，精确恢复内部剩余容量。
- 旧快照或修改了预留容量宏后，按快照中已保存的 SOC 百分比重建剩余容量，避免启动时 SOC 跳变。

### 3.4 充电器连接但满电零电流时的 SOC

当电池和充电器同为约 42V，充电电流降为 0 时，SOC 不会因为板载自耗持续下降到 99%：

- 正常运行的库仑积分仍会按 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` 扣除板耗。
- 当单体满足现有满电电压、压差和有效性条件时，满电确认逻辑每 15 秒重新将 SOC 和内部容量锚定到 100%。
- host test 已验证单体 4200mV、充电电流 0、连续运行 1 小时后，SOC 仍为 100%，当前容量仍为 27Ah。

本次不会因为检测到充电器 5V 就全局关闭 SOC 板耗补偿。否则在只有 5V 识别但主充电回路异常、充电器故障或电池实际未获得充电能量时，会错误锁住 SOC。当满电电压条件不再成立时，保留板耗扣减可以反映真实的静态消耗；电压回落后充电器恢复充电，满电条件再次成立时 SOC 会重新锚定到 100%。

## 4. RTC 空闲超时转深睡

### 4.1 原有低功耗边界

原有逻辑保持不变：

- 充电电流小于 500mA、未检测到充电器 5V、无放电、无通信、无按键、无 Flash/升级占用、无故障时，达到 `sys_time.time_enter_rtc` 后进入 `HICCUP_MODE` RTC STOP。
- 优先判断充电器 5V 识别；识别脚为低电平有效，或者充电电流大于等于 500mA 时，持续阻止进入 RTC STOP。
- 低压深睡判定的阈值、计时和优先级不变。
- RTC 周期唤醒时继续执行 AFE 采样、SOC 静置补偿和老化时间累计。

### 4.2 新增计时规则

`g_stLowPowerRtcStatus.sleep` 用于累加本轮连续 RTC 低功耗时间。只有满足以下条件的 RTC Alarm 周期才累加：

1. RTC Alarm 正常唤醒。
2. AFE 数据更新成功。
3. 没有充放电电流唤醒。
4. 没有 AFE 异常唤醒。
5. 没有紧急低压唤醒。

任何外部唤醒或异常会结束当前 RTC 低功耗轮次；下次进入 RTC 时从 0 重新计时。

### 4.3 达到超时后的处理

连续 RTC 时间达到配置阈值后，依次执行：

1. 清除并关闭 RTC STOP 唤醒源。
2. 恢复运行态外设，保证 Flash、日志和低功耗保存路径可用。
3. 将本轮 RTC 时间补入系统运行时间。
4. 请求 `DEEP_MODE`。
5. 复用现有 `low_power_log_and_commit_sleep(DEEP_MODE)` 路径，保存 SOC 快照、老化进度和睡眠日志，写入深睡 BootFlag 后复位。

## 5. 修改文件

| 文件 | 修改内容 |
| --- | --- |
| `SocEnhance.c` | 新增可用容量计算、对外容量映射、Flash 快照迁移逻辑 |
| `Project_Config.h` | 新增 SOC 预留容量和 RTC 空闲转深睡宏 |
| `Project_BuildGuard.h` | 新增两个配置宏的范围检查 |
| `rtc_sleep.c` | 累加无异常 RTC 时间，达到阈值后转深睡 |
| `tools/soc_host_c_test.c` | 增加预留容量、对外容量和旧快照迁移测试 |

## 6. 验证结果

### 6.1 已通过

- `python3 tools/run_soc_host_c_test.py`
  - 板耗 `0mA`、`15mA`、`30mA`、`1000mA` 四种配置。
  - 每种配置 21 项真实 SOC C 源码 host test 全部通过。
  - 覆盖 27Ah 对外容量不变、默认 26Ah 内部可用容量放完后 SOC=0，以及旧快照迁移。
- `rtc_sleep.c` 通过 Clang C99 严格语法检查，无隐式函数声明错误。
- 满电单体 4200mV、充电电流为 0 的 host 用例连续运行 1 小时后 SOC 仍为 100%，当前容量仍为 27Ah。
- 本次相关文件通过 `git diff --check`。

### 6.2 未完成的验证

- 未在 Keil/ARMCC 环境执行完整 Release 编译。
- 未在实板等待 48 小时验证 RTC 转深睡；建议实板验证时临时将宏改为 1 小时或使用专用测试构建，验证后恢复 48。
- `tools/soc_replay_test.py` 当前在未修改的尾段模型解析处仍会查找源码中已不存在的 `s_empty_tail_table`，属于现有测试工具与当前源码不同步，与本次功能无关。

## 7. 实板建议验证项

1. 27Ah 电池充满后，确认通信中 `CapacityFull=27Ah`、`CapacityNow=27Ah`、`SOC=100%`。
2. 关闭低压尾段干扰或使用高于尾段校准阈值的可控放电工况，放出约 26Ah 后确认 SOC 达到 0%。
3. SOC=0% 后继续小电流放电，确认约 1Ah 物理预留容量可用，同时低压保护仍正常。
4. 无充放电、无通信、无故障时，确认电池先进入 RTC STOP，未达到阈值时继续 RTC 周期低功耗。
5. RTC 连续时间达到阈值后，确认 BootFlag 转为深睡标志，不再执行 RTC 周期唤醒。
6. RTC 计时期间分别测试充电、放电、通信和 AFE 异常唤醒，确认会退出当前 RTC 轮次，下次进入时从 0 计时。
7. 确认原有低压深睡、SOC Flash 恢复、老化时间累计和睡眠日志保存正常。

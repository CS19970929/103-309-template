# 新架构设计

文档状态：部分验证  
目标：裸机、STM32 标准外设库/寄存器级、无 HAL、无 RTOS、无 malloc，优先降低复杂度。  
设计原则：不是重写系统，而是把现有功能按真实边界拆清楚，让后续删除、移植、验证更容易。

## 1. 总体目录

建议目标目录：

```text
app/
  app_main.c
  app_boot.c
  app_runtime.c

bsp/
  bsp_clock.c
  bsp_gpio.c
  bsp_power.c
  bsp_rtc.c
  bsp_timer.c
  bsp_watchdog.c

drivers/
  afe/sh367309_driver.c
  afe/sh367309_config.c
  adc_hw.c
  can_hw.c
  flash_hw.c
  i2c_soft.c
  led_gpio_scan.c
  usart_hw.c

modules/
  bms_core.c
  protection.c
  mos_ctrl.c
  soc.c
  low_power.c
  led_display.c
  system_time.c
  event_log.c
  factory_aging.c
  product_info.c

protocol/
  comm_modbus.c
  modbus_register_map.c
  comm_can.c
  can_feidao_frames.c
  can_app_cmd.c
  iap_service.c

storage/
  storage_flash.c
  param_store.c
  upgrade_param_policy.c

docs/
  refactor/
  review/
  test_record/

tests_or_tools/
  flash64k_storage_test/
  soc_injection_test/
  host_protocol_check/
```

## 2. 依赖方向

建议依赖方向必须单向：

```text
app
  -> modules
  -> protocol
  -> storage
  -> drivers
  -> bsp

drivers -> bsp
storage -> drivers/flash_hw
protocol -> modules API + drivers/usart_hw/can_hw callback
modules -> drivers + storage + bsp
bsp -> StdPeriph/register
```

禁止方向：

- `drivers` 不调用 `modules` 业务逻辑。
- `storage` 不调用保护、SOC、MOS、CAN、Modbus。
- `protocol` 不直接改 AFE 寄存器和 GPIO，必须调用业务 API。
- `low_power` 不直接访问大量全局变量，必须通过端口 API 查询 blocker。
- `soc` 不直接写协议寄存器缓冲，输出由 `bms_core` 或 publish API 完成。

## 3. app 层

### app_boot

职责：

- 执行上电初始化顺序。
- 处理 sleep boot flag 和启动恢复。
- 初始化 BSP、driver、storage、modules、protocol。
- 保持现有 `AppInit_Boot()` 顺序语义。

迁移来源：

- `AppInit.c`
- `main.c`
- `SleepDeal.c:IsSleepStartUp`

保留边界：

- `InitDelay()` 必须早于休眠启动判断。
- `IsSleepStartUp()` 的 BKP flag 语义保持不变。
- `InitE2PROM -> InitAFE1 -> InitCan -> InitADC -> InitData_SOC -> InitTimer` 顺序变更必须单独验证。

### app_runtime

职责：

- 固定主循环任务顺序。
- 不包含业务细节，只调度模块。
- 根据 10ms/200ms/1000ms 标志决定任务执行。

目标流程：

```text
app_runtime_run_once()
  -> system_time_latch_flags()
  -> factory_aging_task()
  -> led_display_task()
  -> bms_core_sample_task()
  -> comm_modbus_task()
  -> adc_task()
  -> low_power_task()
  -> comm_can_task()
  -> storage_test_task_if_enabled()
  -> iap_service_task()
  -> event_log_task()
  -> product_info_task()
  -> watchdog_feed()
```

## 4. bsp 层

职责：

- 只处理 MCU 外设基础能力和板级 GPIO/电源定义。
- 不包含 BMS 策略。

模块建议：

| 模块 | 职责 | 迁移来源 |
|---|---|---|
| `bsp_clock` | `SystemInit` 后 STOP 恢复时钟 | `bsp_clock.c`、`conf.c:cpu_frequency_conf` |
| `bsp_gpio` | GPIO 初始化、JTAG 禁用、IO 模式切换 | `conf.c`、`conf_gpio.h` |
| `bsp_power` | STOP 前后电源 rail 和唤醒 IO | `bsp_power.c`、`conf.c` |
| `bsp_rtc` | RTC wakeup 周期、BKP access 基础接口 | `bsp_rtc.c`、`RTC.c` |
| `bsp_timer` | TIM3 10ms tick、TIM4 display scan timer 基础 | `System_Init.c`、`LedBar.c` |
| `bsp_watchdog` | IWDG 初始化和 feed | `System_Init.c`、宏 `Feed_IWatchDog` |

## 5. drivers 层

### afe_driver

职责：

- SH367309 读写寄存器、MTP、状态读取。
- bitbang I2C 时序。
- AFE 初始化、sleep/idle/reset 基础操作。
- 不直接做保护决策、不直接写 SOC、不直接写 Modbus 报告。

建议接口：

```c
uint8_t afe_driver_init(void);
uint8_t afe_driver_read_sample(AfeSample *sample);
uint8_t afe_driver_read_status(AfeStatus *status);
uint8_t afe_driver_write_config(const AfeConfig *config);
void afe_driver_enter_sleep(void);
void afe_driver_reset(void);
```

迁移来源：

- `I2C_AFE1.c/h`
- `SH367309_Func.c/h`
- `SH367309_DataDeal.c`
- `rtc_sleep_afe_sh367309.c`

关键要求：

- 保留当前 MTP 返回值约定，或在迁移阶段显式统一后全局替换。
- AFE 写 MTP 前后延时、reset、`MCUO_AFE_VPRO` 时序必须保留。
- 保护故障映射从 driver 中移出，由 `protection` 处理。

### adc_hw

职责：

- ADC/DMA/TIM2 初始化。
- VBAT、Type-C 电流、MOS 温度计算。
- STOP 前关闭 ADC/DMA/TIM2，STOP 后恢复。

迁移来源：

- `ADC.c`

## 6. modules 层

### bms_core

职责：

- 当前 BMS 主状态的聚合入口。
- 负责 200ms sample cycle。
- 按顺序调用 AFE sample、数据转换、保护、MOS、SOC publish。

建议 200ms 流程：

```text
bms_core_sample_task()
  -> if no 200ms period: return
  -> afe_driver_read_sample()
  -> bms_core_update_cell_voltage()
  -> bms_core_update_temperature()
  -> bms_core_update_current()
  -> protection_update()
  -> mos_ctrl_update()
  -> soc_update()
  -> bms_core_publish_report()
```

迁移来源：

- `DataDeal.c:App_AFEGet`
- `DataDeal.c:DataLoad_*`
- `SH367309_Func.c:App_SH367309`

边界：

- `bms_core` 可以读写 `BmsReport`，但不直接处理 Modbus/CAN 帧。
- `bms_core` 不直接写 GPIO MOS，由 `mos_ctrl` 执行。

### protection

职责：

- 保护阈值判断。
- 故障位、告警位、故障记录。
- AFE fault 到 MCU fault 的映射。
- 系统错误状态映射。

迁移来源：

- `Fault.c/h`
- `SH367309_Func.c:Fault_ChangeToMCU`
- `DataDeal.c` 中保护相关判断
- `System_Monitor.c/h`

建议接口：

```c
void protection_init(const ProtectConfig *cfg);
void protection_update(const BmsSample *sample, ProtectionResult *result);
void protection_apply_afe_status(const AfeStatus *status);
uint16_t protection_get_fault_bits(void);
```

### mos_ctrl

职责：

- 统一管理 charge MOS、discharge MOS、MCC、RF_EN、CTLC。
- 接收策略请求：正常启动、5V 充电、老化、保护断开、恢复。
- 封装 AFE MOS bit 和 MCU GPIO 操作。

迁移来源：

- `MosStartup.c`
- `DataDeal.c:open_ctlc/close_ctlc/new_todo_logi`
- `SH367309_Func.c:SH367309_DriverMos_Ctrl`
- `FactoryAging.c` 中 `enter_fac_mode` 相关调用

禁止事项：

- 不允许业务模块直接写 `GPIO_MCC_C`、`MCUO_RF_EN`、`SH367309_Reg_Store.REG_MTP_CONF.bits.*`。
- 任何 MOS 状态变化都应能追踪来源。

### soc

职责：

- SOC/SOH/cycle/capacity 计算。
- OCV、coulomb counting、rest compensation、display smoothing。
- 保存/加载 SOC snapshot。

迁移来源：

- `SOC.c`
- `SocEnhance.c/h`

建议边界：

```c
void soc_init(const SocConfig *cfg, const SocSnapshot *snapshot);
void soc_update(const BmsSample *sample, SocOutput *output);
void soc_apply_rtc_rest(uint32_t rest_seconds, uint16_t vmin, uint16_t vmax);
uint8_t soc_save_snapshot_if_needed(void);
```

第一阶段不改算法，只把输入样本和输出发布边界收紧。

### low_power

职责：

- 统一管理 RUN、IDLE_CHECK、PREPARE_SLEEP、STOP_SLEEP、WAKEUP_RESTORE。
- 管理 blocker：电流、通信、按键、Flash、升级、故障、LED、IWDG 安全窗口。
- 管理 HICCUP/NORMAL/DEEP 三种 sleep request。
- 调用端口层完成 STOP、BKP、AFE sleep、CAN RTC service。

迁移来源：

- `app_lowpower.c`
- `rtc_sleep.c`
- `rtc_sleep_port.c`
- `SleepDeal.c`
- `LowPowerSleep.c`

目标：

- 保留当前行为。
- 把“能不能睡”和“怎么睡”拆开。
- 把 reset sleep 与 RTC hiccup sleep 拆成两个清楚路径。

### led_display

职责：

- SOC 数码管显示策略。
- 唤醒快显。
- 按键滤波和长按关机请求。
- 不直接进入 deep sleep，只向 `low_power` 发请求。

迁移来源：

- `LedBar.c`

底层扫描应移动到 `drivers/led_gpio_scan.c`。

### system_time

职责：

- 10ms tick。
- 周期 task flag。
- 运行时间、RTC sleep 累计时间接口。

迁移来源：

- `System_Init.c`
- `conf.h:Time_T` 中时间相关字段

### event_log

职责：

- BMS 事件 ring buffer。
- 日志节流。
- Flash load/save。
- Modbus/CAN 只通过 API 查询。

迁移来源：

- `LogRecord.c/h`

## 7. protocol 层

### comm_modbus

职责：

- 串口收发状态机。
- Modbus RTU CRC、帧解析、ACK/NAK。
- 不直接包含业务判断。

迁移来源：

- `Sci_Upper.c` 中帧解析、串口服务、ISR。

### modbus_register_map

职责：

- 集中定义寄存器窗口、读写权限、handler。
- 对接 `bms_core/protection/soc/storage/product_info/factory_aging` API。

必须保留：

- `0xD000` 板端状态。
- `0xD300` SOC 测试/支持状态。
- `0xC002` 48 个寄存器的 SN、硬件版本、软件版本来源。
- 事件记录、参数写入、IAP 等现有地址和错误码。

### comm_can

职责：

- CAN 初始化、电源控制、TX queue、bus active timeout。
- 周期帧调度。
- RX App 命令队列。
- 低功耗 RTC 唤醒广播服务。

迁移来源：

- `Can_HDX.c`
- `CanFeidaoFrames.c`

建议拆分：

```text
can_hw.c              CAN1 init/filter/ISR mailbox
comm_can.c            queue, power, bus active, App_Can
can_feidao_frames.c   customer periodic frames
can_app_cmd.c         GET_STATUS/IAP/READ/WRITE/AGING commands
```

## 8. storage 层

职责：

- Flash 物理擦写和 verify 放 `drivers/flash_hw.c`。
- typed storage 放 `storage_flash.c`。
- 默认参数和升级策略放 `param_store.c`、`upgrade_param_policy.c`。

迁移来源：

- `Flash.c/h`
- `EEPROM.c/h`

保持：

- storage address 不变。
- magic/version/CRC 不变。
- SOC 双槽 fallback 不变。
- AFE/RW/log/aging 数据结构兼容。

## 9. 全局状态收敛建议

当前全局状态：

- `g_stCellInfoReport`
- `SH367309_Reg_Store`
- `OtherElement`
- `ProtectParameter`
- `SOC_Enhance_Element`
- `sys_time`
- `ProductionInfor`
- `System_ErrFlag`

目标不是一次性消灭所有全局变量，而是先定义所有权：

| 状态 | 目标所有者 | 访问方式 |
|---|---|---|
| `g_stCellInfoReport` | `bms_core` | protocol 只读 API，storage/soc 通过 publish 更新 |
| `SH367309_Reg_Store` | `afe_driver` | 其他模块不直接写寄存器缓存 |
| `OtherElement` | `param_store` | modules 通过 config snapshot 读取 |
| `ProtectParameter` | `protection` | 通过 protection config 读取 |
| `SOC_Enhance_Element` | `soc` | 对外只发布 SOC output |
| `sys_time` | `system_time` | debug counters 独立拆出 |
| `ProductionInfor` | `product_info` | protocol 通过 API 读取 |
| `System_ErrFlag` | `protection/system_monitor` | 显式 set/clear/get API |

## 10. 第一阶段目标文件映射

第一阶段不搬目录也可以先按文件内职责切分，降低风险：

| 当前文件 | 目标模块 | 第一阶段动作 |
|---|---|---|
| `Runtime.c` | `app_runtime` | 保留顺序，清理注释死调用。 |
| `System_Init.c` | `system_time` | 命名收敛，不改 TIM3 行为。 |
| `DataDeal.c` | `bms_core/protection/mos_ctrl` | 先提取函数块，不改判断。 |
| `I2C_AFE1.c` | `afe_driver/i2c_soft` | 先分清 bitbang 和 AFE 寄存器操作。 |
| `SH367309_*` | `afe_driver/protection` | fault mapping 移出 driver。 |
| `SOC.c/SocEnhance.c` | `soc` | 先统一输入输出结构。 |
| `Sci_Upper.c` | `comm_modbus/modbus_register_map` | 先提取 register map。 |
| `Can_HDX.c` | `comm_can/can_app_cmd` | 先拆 App 命令和周期调度。 |
| `Flash.c/EEPROM.c` | `storage/param_store` | 保持地址，拆 typed API。 |
| `LedBar.c` | `led_display/led_gpio_scan` | 扫描和策略拆开。 |
| `rtc_sleep*.c` | `low_power` | blocker、RTC cycle、reset sleep 分层。 |

## 11. 架构验收标准

重构完成后应满足：

1. 主循环仍是裸机轮询，无 RTOS。
2. 不引入 HAL，不引入 malloc。
3. Release 编译 guard 仍能阻止测试/调试开关进入量产。
4. App 地址仍是 `0x08004800`。
5. Modbus/CAN 协议地址、ID、数据含义不变。
6. AFE 采样周期和 SOC 更新节奏不变。
7. 低功耗进入、唤醒、BKP flag、LED 快显行为不变。
8. 老化剩余时间在上位机和 CAN 广播中仍可见。
9. 每一步都有独立 diff、编译、静态检查和可回滚点。

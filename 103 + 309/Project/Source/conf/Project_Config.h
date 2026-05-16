#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* Keil uVision: right click this file and select "Configuration Wizard". */

// <<< Use Configuration Wizard in Context Menu >>>

// <h>项目可视化配置

// <h>构建档位
// <o> 构建档位
//   <0=> 量产 Release
//   <1=> 调试 Debug
//   <2=> 工厂/测试 Factory/Test
// <i> 构建用途选择。量产必须使用 Release，SOC 注入测试必须使用 Factory/Test。
#ifndef PROJECT_CFG_BUILD_PROFILE
#define PROJECT_CFG_BUILD_PROFILE 0
#endif
// </h>

// <h>产品基础配置
// <o> EEPROM 初始化标记 <0x0000-0xFFFF>
// <i> Change this value only when field parameters must be reinitialized.
// <i> EEPROM 参数初始化标记。只有需要强制重置现场参数时才修改。
#define PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG 0x2445

// <o> 电池包类型
//   <0=> 主包 BAT_MASTER，20A
//   <1=> 从包 BAT_SLAVE，40A
// <i> 电池包类型。影响默认电流等级、协议身份和部分产品差异配置。
#define PROJECT_CFG_BAT_TYPE 1

// <o> 电芯体系
//   <0=> 三元锂 TERNARYLI
//   <1=> 磷酸铁锂 LIFEPO
// <i> 电芯体系。影响电压平台、SOC 表和保护参数选择。
#define PROJECT_CFG_BAT_CHEMISTRY 0

// <o> 固件年份 <0-99>
// <i> 固件年份，两位数显示。
#define PROJECT_CFG_FD_YEAR 26

// <o> 固件月份 <1-12>
// <i> 固件月份，用于版本信息上报。
#define PROJECT_CFG_FD_MONTH 5

// <o> 固件日期 <1-31>
// <i> 固件日期，用于版本信息上报。
#define PROJECT_CFG_FD_DAY 9

// <o> 固件版本号 <0-65535>
// <i> 固件版本号，用于上位机、升级策略和现场识别。
#define PROJECT_CFG_VERSION 5

// <o> 电流档位
//   <0=> 80A 档 CURR_80A
//   <1=> 100A 档 CURR_100A
//   <2=> 150A 档 CURR_150A
//   <3=> 200A 档 CURR_200A
//   <4=> 250A 档 CURR_250A
// <i> 额定电流档位。影响保护、电流显示和整机能力配置。
#define PROJECT_CFG_LEVEL_CURR 2

// <o> AFE 类型
//   <0=> bq76xx AFE
//   <1=> sh36xx AFE
// <i> AFE 芯片类型。必须与硬件实际焊接型号一致。
#define PROJECT_CFG_AFE_TYPE 1
// </h>

// <h>功能开关
// <q> 使能 IWDG 看门狗
// <i> 使能独立看门狗。量产建议开启，防止程序跑飞后长期无响应。
#define PROJECT_CFG_WDOG_ENABLE 1

// <q> 使能 RTC 功能
// <i> 使能 RTC。用于低功耗计时、休眠唤醒和 SOC 休眠修正。
#define PROJECT_CFG_RTC_ENABLE 1

// <q> 使能加热功能
// <i> 使能加热功能。仅硬件具备加热回路时开启。
#define PROJECT_CFG_HEAT_ENABLE 0

// <q> 使能负载移除短路恢复
// <i> 使能负载移除短路恢复逻辑。按产品保护策略决定。
#define PROJECT_CFG_LOAD_REMOVE_SHORT_ENABLE 0

// <q> 使能 UART1 唤醒
// <i> 允许 UART1 唤醒系统。上位机或外设接在 UART1 时开启。
#define PROJECT_CFG_UART1_WAKEUP_ENABLE 1

// <q> 使能 UART2 唤醒
// <i> 允许 UART2 唤醒系统。未使用 UART2 唤醒时保持关闭。
#define PROJECT_CFG_UART2_WAKEUP_ENABLE 0

// <q> 使能 RS485 唤醒
// <i> 允许 RS485 通信唤醒系统。需要休眠后上位机唤醒时开启。
#define PROJECT_CFG_RS485_WAKEUP_ENABLE 1

// <q> 使能第二路电流保护
// <i> 使能第二路电流保护。只有硬件和参数支持时开启。
#define PROJECT_CFG_SECOND_CURR_PROTECT_ENABLE 0

// <q> 使能虚拟电流补偿
// <i> 使能虚拟电流补偿。用于修正部分工况下的电流估算偏差。
#define PROJECT_CFG_VIRTUAL_CURRENT_ENABLE 1

// <q> 使能 DI 系统开关输入
// <i> 使能 DI 系统开关输入。需要外部系统开关时开启。
#define PROJECT_CFG_DI_SWITCH_SYS_ONOFF_ENABLE 0

// <q> 使能 DI 放电开关输入
// <i> 使能 DI 放电开关输入。需要外部控制放电时开启。
#define PROJECT_CFG_DI_SWITCH_DSG_ONOFF_ENABLE 0

// <q> 使能 DI 长按开关输入
// <i> 使能 DI 长按开关输入。用于按键长按开关机。
#define PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE 1

// <q> 使能旧 LED 功能标记
// <i> 使能旧 LED 功能标记。新 LedBar 方案通常不依赖此项。
#define PROJECT_CFG_LED_FUNC_ENABLE 0

// <q> 使能调试代码
// <i> 使能调试代码。量产必须关闭。
#define PROJECT_CFG_DEBUG_CODE_ENABLE 0

// <q> Export Keil Watch debug observation entries
// <i> Debug only. Release build must keep this disabled. Exports g_dbg_* watch pointers.
#ifndef PROJECT_CFG_DEBUG_WATCH_ENABLE
#define PROJECT_CFG_DEBUG_WATCH_ENABLE 0
#endif

// <q> 允许带电流休眠
// <i> 允许带电流进入休眠。通常保持关闭，避免带载误休眠。
#define PROJECT_CFG_SLEEP_WITH_CURRENT_ENABLE 0

// <q> 使能 IAP 跳转支持
// <i> 使能 IAP 跳转支持。带 bootloader 的 App 必须开启。
#define PROJECT_CFG_IAP_ENABLE 1

// <q> 使能出厂老化模式
// <i> 首次出厂运行期间打开充放电管；完成后写入 Flash 标志，后续不再自动进入老化。
#define PROJECT_CFG_FACTORY_AGING_ENABLE 1

// <o> 出厂老化运行时长，秒 <1-604800>
// <i> 默认 259200 秒，即 3 天。只累计 MCU 正常运行态 TIM3 tick，RTC/STOP 休眠时间不计入。
#define PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS 259200
// </h>

// <h>串口角色
// <o> SCI1 角色
//   <0=> 禁用
//   <1=> 通用上位机
//   <2=> 客户端
//   <3=> LCD
// <i> SCI1 通信角色。当前常用于通用上位机协议。
#define PROJECT_CFG_SCI1_ROLE 1

// <o> SCI2 角色
//   <0=> 禁用
//   <1=> 通用上位机
//   <2=> 客户端
//   <3=> LCD
// <i> SCI2 通信角色。未接外设时保持 Disabled。
#define PROJECT_CFG_SCI2_ROLE 0

// <o> SCI3 角色
//   <0=> 禁用
//   <1=> 通用上位机
//   <2=> 客户端
//   <3=> LCD
// <i> SCI3 通信角色。未接外设时保持 Disabled。
#define PROJECT_CFG_SCI3_ROLE 0
// </h>

// <h>Flash 64K 存储测试
// <q> 使能破坏性快速测试
// <i> 使能 Flash 后 64K 破坏性快速测试。只允许实验固件开启。
#define PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE 0

// <o> 快速测试循环次数 <1-65535>
// <i> 破坏性快速测试循环次数。仅测试模式使用。
#define PROJECT_CFG_FLASH64K_QUICK_TEST_CYCLES 96

// <q> 使能 App 运行存储测试
// <i> 使能 App 运行期间存储测试。用于验证 SOC/AFE 存储稳定性。
#define PROJECT_CFG_FLASH64K_USE_TEST_ENABLE 0

// <o> App 测试打印周期，秒 <1-65535>
// <i> App 运行存储测试日志打印周期，单位秒。
#define PROJECT_CFG_FLASH64K_USE_TEST_PRINT_PERIOD_SEC 10

// <q> 使能 App 测试加速
// <i> 使能运行存储测试加速。只用于缩短台架验证时间。
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE 0

// <o> 加速 SOC 保存周期，秒 <1-65535>
// <i> 加速测试时 SOC 快照保存周期，单位秒。
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_SOC_PERIOD_SEC 1

// <o> 加速 AFE 保存周期，秒 <1-65535>
// <i> 加速测试时 AFE 参数保存周期，单位秒。
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_AFE_PERIOD_SEC 30
// </h>

// <h>Flash 日志磨损保护
// <o> 同类事件重复保存最小间隔，秒 <0-86400>
// <i> 用于抑制故障抖动导致的事件日志反复写 Flash。默认 3600 秒；0 表示关闭限频；启动和睡眠事件不受限。
#define PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC 3600
// </h>

// <h>SOC 校准参数
// <o> 满电确认最小电压余量，mV <0-500>
// <i> 普通满电确认允许低于满电电压的余量。越大越容易到 100%。
#define PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV 80

// <o> 满电确认最大单体压差，mV <0-1000>
// <i> 满电确认允许的最大单体压差。越小越保守。
#define PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV 120

// <o> 普通满电确认时间，秒 <1-600>
// <i> 普通满电条件需要持续的时间。越长越稳，越短到 100% 越快。
#define PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS 15

// <o> 快速满电确认时间，秒 <1-600>
// <i> 快速满电条件需要持续的时间。必须不大于普通确认时间。
#define PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS 5

// <o> 普通满电确认最低 SOC，百分比 <0-100>
// <i> 普通满电确认要求的最低内部 SOC。用于避免中低电量误跳 100%。
#define PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT 95

// <o> 快速满电确认电压余量，mV <0-500>
// <i> 快速满电确认允许低于满电电压的余量。越大越容易快速到 100%。
#define PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV 30

// <o> SOC calibration min valid cell voltage, mV <1000-3500>
#define PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV 2000

// <o> 校准最大有效单体电压，mV <3600-6000>
// <i> SOC 校准允许使用的最高单体电压。高于该值认为采样异常。
#define PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV 5000

// <o> 校准最大单体压差，mV <0-3000>
// <i> SOC 校准允许的最大单体压差。越小越能过滤异常串。
#define PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV 1000

// <q> 三级保护故障时禁止 SOC 校准
// <i> 三级保护故障时是否禁止 SOC 自动校准。开启后更保守。
#define PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT 0

// <q> 系统故障时禁止 SOC 校准
// <i> AFE/ADC/均衡/温度等系统异常时是否禁止 SOC 自动校准。
#define PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT 0

// <o> SOC sag holdoff seconds <0-1800>
#define PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS 30

// <o> 压降延迟允许校准余量，mV <0-500>
// <i> 压降延迟期间，低于 V0 加该余量才允许末端校准。
#define PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV 50

// <o> 静置 OCV 修正等待时间，秒 <60-43200>
// <i> 静置多久后允许按 OCV 小步修正 SOC。三元锂通常建议 30 分钟起。
#define PROJECT_CFG_SOC_REST_OCV_SECONDS 1800

// <o> 稳定静置最短可信时间，秒 <60-7200>
// <i> 电压稳定至少多久后才允许记录 OCV deferred target。调小会更快修正，调大更稳。
#define PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS 300

// <o> 稳定静置/后续充放电消化 OCV 差值节拍，秒 <60-7200>
// <i> 达到稳定窗口后，每隔多久允许刷新或消化 1% OCV 修正。
#define PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS 600

// <o> 久置低 OCV 静置下修节拍，秒 <60-43200>
// <i> 长时间静置且 OCV 目标低于内部 SOC 时，每隔多久最多下修 1%。
#define PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS 1800

// <o> SOC auto calibration max step percent <1-10>
#define PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT 1

// <o> 板端自耗电流，mA <0-1000>
// <i> SOC 积分时额外扣除 BMS 板自身耗电。默认 30mA，后续按实测电流修改。
#define PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA 30

// <o> 低压尾段校准起始偏移，mV <0-1000>
// <i> VCellMin <= V0 + 该偏移时才允许进入低压尾段表。调小可减少最后 20% 掉电过快。
#define PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV 400

// <o> 低压软尾段目标抬高百分比 <0-30>
// <i> 只作用于 V0 以上的软尾段表项。调大可让低电区显示更不保守，V0 及以下仍强制向 0 收敛。
#define PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT 0

// <o> 低压软尾段下修速度比例，百分比 <25-400>
// <i> 只作用于 V0 以上的软尾段表项。100 为默认；调大变慢，调小变快。
#define PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT 100

// <o> 普通显示跟随速度，秒/1% <1-60>
#define PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS 5

// <o> 充电显示上升速度，秒/1% <1-60>
#define PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS 5

// <o> 低压显示下降速度，秒/1% <1-60>
#define PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS 1

// <o> 低压显示加速边界，mV <0-500>
// <i> VCellMin <= V0 + 该偏移时，显示下降使用低压速度。
#define PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV 50

// <o> 空电显示最快边界，低于 V0 的 mV <0-500>
// <i> VCellMin <= V0 - 该值时，显示每个 200ms tick 最多下降 1%。
#define PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV 50
// </h>

// <h>SOC 测试模式
// <q> 使能 SOC 注入测试模式
// <i> SOC 注入式测试入口。量产必须保持关闭，只允许 Factory/Test 固件开启。
#ifndef PROJECT_CFG_SOC_TEST_MODE_ENABLE
#define PROJECT_CFG_SOC_TEST_MODE_ENABLE 0
#endif

// <o> SOC 单次注入最大 tick 数 <1-1000>
// <i> 单次 0x2500 注入最多模拟多少个 200ms tick，用于限制测试加速强度。
#ifndef PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX
#define PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX 300
#endif
// </h>

// <h>灯条配置
// <q> 使用 GPIO Charlieplexing 灯条驱动
// <i> 使用 GPIO Charlieplexing 灯条驱动。需与硬件灯条连接方式一致。

// <q> 使能灯条休眠 SOC 备份
// <i> 休眠前备份灯条 SOC 显示状态，唤醒后用于恢复显示体验。
#define PROJECT_CFG_LEDBAR_SLEEP_ENABLE 1

// <q> 使能长按 GPIO 翻转测试
// <i> 长按 GPIO 翻转测试开关。仅调试硬件按键/灯条时开启。
#define PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST 0

// <q> LedBar test always-on display
// <i> Keeps SOC display on during Debug/Factory testing. Release build must keep this disabled.
#ifndef PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON
#define PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON 0
#endif

// <o> SOC 显示时间，10ms tick <1-65535>
// <i> SOC 显示保持时间，单位 10ms。500 表示 5 秒。
#define PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS 500

// <o> Sleep wakeup SOC display time, 10ms tick <1-65535>
// <i> After MCU reset from sleep wakeup, keep SOC display for 10s by default.
#define PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS 1000

// <o> LedBar scan timer period, 100kHz tick <1-1000>
// <i> GPIO Charlieplexing only. 50 means 0.5ms per route.
#define PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS 50

// <o> MCU_WK 有效滤波，10ms tick <0-255>
// <i> MCU_WK 有效滤波时间，单位 10ms。用于消抖。
#define PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS 3

// <o> MCU_WK 无效滤波，10ms tick <0-255>
// <i> MCU_WK 无效滤波时间，单位 10ms。用于消抖。
#define PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS 3

// <o> 充电接入滤波，100ms tick <0-255>
// <i> 充电插入有效滤波时间，单位 100ms。
#define PROJECT_CFG_LEDBAR_CHARGE_ON_FILTER_100MS 3

// <o> 充电断开滤波，100ms tick <0-255>
// <i> 充电拔出无效滤波时间，单位 100ms。
#define PROJECT_CFG_LEDBAR_CHARGE_OFF_FILTER_100MS 5
// </h>

// <h>升级参数策略
// <q> 使能升级参数策略
// <i> 使能升级参数策略。只有需要随固件升级重置现场参数时开启。
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE 1

// <o> 升级参数策略版本 <0x0000-0xFFFE>
// <i> 升级参数策略版本。策略内容变化时递增，避免重复或漏执行。
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION 0x0509

// <q> 重置 AFE 参数
// <i> 升级时重置 AFE 参数。只在 AFE 默认参数必须覆盖现场值时开启。
#define PROJECT_CFG_UPGRADE_PARAM_RESET_AFE 1

// <q> 重置保护参数
// <i> 升级时重置保护参数。会影响保护阈值，必须谨慎开启。
#define PROJECT_CFG_UPGRADE_PARAM_RESET_PROTECT 0

// <q> 重置均衡开启电压
// <i> 升级时重置均衡开启电压。用于修正现场旧版本均衡参数。
#define PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE 0

// <q> 重置 SOC 表
// <i> 升级时重置 SOC OCV 表。电芯曲线或默认表变化时开启。
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE 0

// <q> 重置 SOC 配置
// <i> 升级时重置 SOC 容量、电压锚点等配置。会影响用户可见电量。
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG 0

// <q> 重置 SOC 快照
// <i> 升级时清除 SOC 快照。开启后首次启动可能重新按 OCV 估算 SOC。
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT 0

// <q> 重置事件记录
// <i> 升级时清除事件记录。售后需要保留历史故障时必须关闭。
#define PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD 0

// <q> 强制重复执行策略，仅测试
// <i> 强制重复执行升级参数策略。只用于测试，量产必须关闭。
#define PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY 0
// </h>

// </h>

// <<< end of configuration section >>>

#endif

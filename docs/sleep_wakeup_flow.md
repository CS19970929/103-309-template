# 休眠、唤醒与 LED 链路说明

## 目标

系统默认保持深度休眠。只有两类来源允许唤醒并继续运行：

- 充电唤醒：PA0 / `PIN_INT_WK_MCU`。
- 开关唤醒：PA9 / `PIN_SOC_KEY` 或 PB5 / `PIN_KEY1`。

其它来源，包括上电复位、外部复位、软件复位、看门狗复位，以及未识别的 Stop 唤醒，均回到深度休眠。

## 模块职责

- `main.c`：启动入口。`InitDevice()` 最早执行快显预览和休眠启动分流。
- `SleepDeal.c`：运行时休眠判断、休眠模式保存、启动后进入 Stop 循环。
- `SleepWakeFastUi.c`：Stop 唤醒后的合法唤醒源判断，以及启动早期 SOC 快显和开机确认。
- `LedSnapshot.c`：把休眠前的 SOC/告警状态保存到 BKP，供快显使用。
- `LedBar.c`：正常运行后的 LED 状态机，包括待机预览、开机动画、工作显示、充电显示、关机确认。
- `PowerUi.c`：开机/关机确认状态，控制 MOS 是否允许恢复。

## 启动链路

`main()` 进入 `InitDevice()` 后先执行最小初始化：

1. `SystemInit()`。
2. `InitDelay()`。
3. `SleepWakeFastUi_ServiceStartupPreview()`。
4. 如果第 3 步没有确认合法唤醒，则执行 `IsSleepStartUp()`。

`SleepWakeFastUi_ServiceStartupPreview()` 优先读取 BKP 中的快显唤醒标志。没有快显标志时，会直接读取 PA0、PA9、PB5 的实时电平，用于覆盖“上电瞬间已经插入充电器”或“按住开关上电”的场景。

如果没有合法唤醒源，`IsSleepStartUp()` 会把 `FLASH_SLEEP_RESET_VALUE` 或无效休眠标志统一转成 `FLASH_DEEP_SLEEP_VALUE`，然后进入深度休眠 Stop 循环。因此 MCU 上电复位不会直接跑主循环。

## 运行时进入休眠

运行时休眠入口是 `App_SleepDeal()`：

1. `SleepDeal_Normal_Select()` 判断进入正常休眠 L2 还是低压深睡 L3。
2. `SleepDeal_Normal_L2()` / `SleepDeal_Normal_L3()` 做延时确认。
3. 满足条件后置 `Sleep_Mode.bits.b1_ToSleepFlag`。
4. `SleepDeal_Continue()` 保存 LED 快照，按 `Sleep_Mode` 选择休眠模式并写入 BKP。
5. AFE 进入 Sleep，随后 `MCU_RESET()`。
6. 复位后 `IsSleepStartUp()` 根据 BKP 休眠标志进入对应 Stop 循环。

`SleepDeal_Continue()` 现在只做两件核心事：

- `SleepDeal_SelectMode()`：把 `Sleep_Mode` 映射为 `NORMAL_MODE` / `HICCUP_MODE` / `DEEP_MODE`。
- `SleepDeal_ModeToFlag()`：把休眠模式映射为 BKP 中保存的 `FLASH_*_SLEEP_VALUE`。

## Stop 唤醒链路

`SleepDeal_RunStopWake()` 是唯一 Stop 循环：

1. 根据休眠模式配置低功耗 IO 和 EXTI 唤醒源。
2. 清 `g_irq_t`。
3. 先调用一次 `SleepWakeFastUi_ServiceAfterStop()` 检查当前电平，覆盖“电平已经有效但没有新边沿”的情况。
4. 执行 `Sys_StopMode()`。
5. Stop 返回后再次调用 `SleepWakeFastUi_ServiceAfterStop()`。

`SleepWakeFastUi_ServiceAfterStop()` 内部通过 `SleepWakeFastUi_DetectWakeReason()` 集中判断唤醒原因：

- 充电：`g_irq_t == CHG_IRQ` 或 PA0 为高。
- 开关：`g_irq_t == bms_keyirq`、`g_irq_t == soc_key`、PA9 为低或 PB5 为低。

合法唤醒会写入 BKP 快显标志并复位 MCU。非法唤醒不会清休眠标志，循环会再次进入 Stop。

## 快显与开机确认

启动早期的 SOC 快显由 `SleepWakeFastUi_ServiceStartupPreview()` 完成：

- 充电唤醒：直接播放开机动画，清快显标志和休眠标志，调用 `PowerUi_ConfirmPowerOn()`，后续完整初始化并允许运行。
- 开关唤醒：显示休眠前通过 `LedSnapshot_SaveRuntime()` 保存的 SOC；按住达到 `LED_BOOT_CONFIRM_HOLD_MS` 后确认开机。超时未确认则重新写回原休眠模式并复位，系统再次进入 Stop。

确认开机后才会调用 `SleepDeal_ClearSleepModeFlag()`，所以普通复位不会误清深睡状态。

## LED 运行状态机

`LedBar.c` 使用 `LED_UI_STATE` 维护正常运行后的 LED 行为：

- `LED_UI_OFF_IDLE`：未开机待机，LED 关闭；检测到按键下降沿后进入 `LED_UI_BOOT_PREVIEW`。
- `LED_UI_BOOT_PREVIEW`：显示当前 SOC；按键持续达到 `LED_BOOT_CONFIRM_HOLD_MS` 后进入开机动画；超时进入待机。
- `LED_UI_BOOT_ANIM`：播放 5 步开机动画；结束后调用 `PowerUi_ConfirmPowerOn()` 并进入工作态。
- `LED_UI_WORK`：显示当前 SOC/告警；按键下降沿进入关机确认。
- `LED_UI_CHARGE`：检测到充电电流或 PA0 充电信号后进入；未确认开机时会自动确认开机。
- `LED_UI_SHUTDOWN_CONFIRM`：闪烁显示关机确认；按住达到 `LED_SHUTDOWN_CONFIRM_HOLD_MS` 后进入关机动画；超时回工作态。
- `LED_UI_SHUTDOWN_ANIM`：播放关机动画；结束后调用 `PowerUi_RequestShutdown()`，由 `PowerUi_ProcessRequests()` 关闭 MOS 并请求深睡。

`APP_LedBar()` 的执行顺序是：刷新闪烁节拍、读取按键、处理充电优先级、按当前状态分发到对应 service 函数。

## 当前限制

SOC 快显数据保存在 STM32 BKP 寄存器中。正常 Stop 唤醒、软件复位通常能保留；如果整机断电导致备份域掉电，BKP 快照会丢失，开关唤醒预览可能显示默认值。当前版本不做 EEPROM/Flash 持久化兜底。

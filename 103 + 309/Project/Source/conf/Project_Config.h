#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* Keil uVision: right click this file and select "Configuration Wizard". */
/* 项目可视化配置: Keil Configuration Wizard marker. */

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Project Basic Configuration

// <o> Battery type
//   <0=> Master BAT_MASTER 20A
//   <1=> Slave  BAT_SLAVE  40A
#define PROJECT_CFG_BAT_TYPE 1

// <o> Battery chemistry
//   <0=> Ternary Lithium TERNARYLI
//   <1=> LiFePO4 LIFEPO
#define PROJECT_CFG_BAT_CHEMISTRY 0

// <q> Enable host write registers
#define PROJECT_CFG_HOST_WRITE_ENABLE 1

// <o> Firmware year <0-99>
#define PROJECT_CFG_FD_YEAR 26

// <o> Firmware month <1-12>
#define PROJECT_CFG_FD_MONTH 5

// <o> Firmware day <1-31>
#define PROJECT_CFG_FD_DAY 9

// <o> Firmware version <0-65535>
#define PROJECT_CFG_VERSION 5

// <o> Rated current
//   <0=> 80A
//   <1=> 100A
//   <2=> 150A
//   <3=> 200A
//   <4=> 250A
#define PROJECT_CFG_LEVEL_CURR 2

// <o> AFE type
//   <0=> bq76xx
//   <1=> sh36xx
#define PROJECT_CFG_AFE_TYPE 1

// <o> EEPROM init flag <0x0000-0xFFFF>
#define PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG 0x2445
// </h>

// <h>Feature Switches

// <q> Enable IWDG
#define PROJECT_CFG_WDOG_ENABLE 1

// <q> Enable RTC
#define PROJECT_CFG_RTC_ENABLE 1

// <q> Enable IAP
#define PROJECT_CFG_IAP_ENABLE 1

// <q> Enable factory aging
#define PROJECT_CFG_FACTORY_AGING_ENABLE 1

// <o> Factory aging duration seconds <1-604800>
#define PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS 259200

// <q> Enable virtual current
#define PROJECT_CFG_VIRTUAL_CURRENT_ENABLE 1

// <q> Enable long-key power switch
#define PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE 1

// <q> Enable debug system monitor
// <i> Exports g_dbg global struct with all IO/peripheral/function states for Keil watch.
// <i> Release must keep disabled.
#ifndef PROJECT_CFG_DEBUG_MONITOR_ENABLE
#define PROJECT_CFG_DEBUG_MONITOR_ENABLE 1
#endif

// <q> Enable IRQ debug counters
// <i> Keeps lightweight interrupt counters for Keil watch and STOP wakeup debug.
#ifndef PROJECT_CFG_IRQ_DEBUG_ENABLE
#define PROJECT_CFG_IRQ_DEBUG_ENABLE 1
#endif

// <q> Enable IRQ debug event ring
// <i> High-rate interrupts are counted but not pushed into the event ring.
#ifndef PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE
#define PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE 1
#endif
// </h>

// <h>Wakeup Sources

// <q> Enable UART1 wakeup
#define PROJECT_CFG_UART1_WAKEUP_ENABLE 1

// <q> Enable RS485 wakeup
#define PROJECT_CFG_RS485_WAKEUP_ENABLE 1
// </h>

// <h>Serial Port Roles

// <o> SCI1 role
//   <0=> Disabled
//   <1=> Host
#define PROJECT_CFG_SCI1_ROLE 1

// <o> SCI2 role
//   <0=> Disabled
#define PROJECT_CFG_SCI2_ROLE 0

// <o> SCI3 role
//   <0=> Disabled
#define PROJECT_CFG_SCI3_ROLE 0
// </h>

// <h>Flash Log Wear Protection

// <o> Minimum repeat interval for same event seconds <0-86400>
#define PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC 3600
// </h>

// <h>SOC Calibration
//
// Keep this section short. Only product tuning or field-debug values should be
// PROJECT_CFG_* macros. Algorithm-internal constants stay in SocEnhance.c.

// <o> Full confirm min cell margin mV <0-500>
// <i> Full-charge calibration requires max cell voltage near the compile-time full voltage.
#define PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV 80

// <o> Full confirm max cell delta mV <0-1000>
// <i> Blocks full-charge calibration when cell imbalance is larger than this value.
#define PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV 120

// <o> Full confirm time seconds <1-600>
// <i> Normal full-charge confirmation time before SOC can be raised to 100%.
#define PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS 15

// <o> Full confirm fast time seconds <1-600>
// <i> Shorter confirmation time used when voltage is closer to full voltage.
#define PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS 5

// <o> Full confirm min SOC percent <0-100>
// <i> Full-charge calibration only runs when internal SOC is at or above this value.
#define PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT 95

// <o> Full confirm fast margin mV <0-500>
// <i> Cell voltage margin that enables the fast full-charge confirmation path.
#define PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV 30

// <o> Calibration min valid cell voltage mV <1000-3500>
// <i> Rejects OCV/full/low-tail calibration when a cell voltage is below this range.
#define PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV 2000

// <o> Calibration max valid cell voltage mV <3600-6000>
// <i> Rejects OCV/full/low-tail calibration when a cell voltage is above this range.
#define PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV 5000

// <o> Calibration max cell delta mV <0-3000>
// <i> Rejects calibration when max-min cell voltage delta is too large.
#define PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV 1000

// <o> Sag holdoff seconds <0-1800>
// <i> Delays OCV/low-tail calibration after heavy discharge to avoid rebound miscalibration.
#define PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS 30

// <o> Sag allow offset mV <0-500>
// <i> Sag hold blocks calibration only while Vmin is above empty voltage plus this offset.
#define PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV 50

// <o> Rest OCV wait seconds <60-43200>
// <i> Minimum stable-rest time before rest OCV target can be trusted.
#define PROJECT_CFG_SOC_REST_OCV_SECONDS 1800

// <o> Rest down step seconds <60-43200>
// <i> Long-rest slow-down period; each period allows at most one SOC step toward OCV target.
#define PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS 1800

// <o> Auto calibration max step percent <1-10>
// <i> Maximum SOC change per automatic calibration step.
#define PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT 1

// <o> Board self consumption mA <0-1000>
// <i> Normal running self-consumption current included in coulomb counting.
#define PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA 30

// <o> Empty tail start offset mV <0-1000>
// <i> Enables low-tail downward calibration when Vmin is below empty voltage plus this offset.
#define PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV 400

// <o> Display normal seconds per 1% <1-60>
// <i> Normal display smoothing rate in seconds for each visible SOC percent.
#define PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS 5

// <o> Display charge seconds per 1% <1-60>
// <i> Visible SOC rising rate while charging.
#define PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS 5

// <o> Display low seconds per 1% <1-60>
// <i> Visible SOC falling rate near empty voltage.
#define PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS 1

// <o> Display low offset mV <0-500>
// <i> Uses low display fall rate when Vmin is below empty voltage plus this offset.
#define PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV 50

// <o> Display empty fast below V0 mV <0-500>
// <i> Drops visible SOC faster when Vmin is below empty voltage by this offset.
#define PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV 50
// </h>

// <h>LedBar Configuration

// <q> Enable sleep LED off
#define PROJECT_CFG_LEDBAR_SLEEP_ENABLE 1

// <o> SOC display time 10ms <1-65535>
#define PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS 500

// <o> Wakeup display time 10ms <1-65535>
#define PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS 1000

// <o> Scan timer period 100kHz <1-1000>
#define PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS 50

//todo 是否有用
// <o> MCU_WK on filter 10ms <0-255>
#define PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS 3

// <o> MCU_WK off filter 10ms <0-255>
#define PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS 3
// </h>

// <h>Upgrade Parameter Policy

// <q> Enable upgrade param policy
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE 1

// <o> Policy version <0x0000-0xFFFE>
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION 0x0603

// <q> Reset AFE params on upgrade
#define PROJECT_CFG_UPGRADE_PARAM_RESET_AFE 1

// <q> Reset protection params on upgrade
#define PROJECT_CFG_UPGRADE_PARAM_RESET_PROTECT 1

// <q> Reset balance open voltage on upgrade
#define PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE 1

// <q> Reset SOC config on upgrade
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG 1

// <q> Reset SOC snapshot on upgrade
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT 1

// <q> Reset event record on upgrade
#define PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD 1

// <q> Reset factory aging time on upgrade
#define PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME 0
// </h>

// <<< end of configuration section >>>

#endif

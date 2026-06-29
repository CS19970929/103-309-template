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
#define PROJECT_CFG_BAT_CHEMISTRY 1

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

// <o> Factory aging duration seconds <1-604800>
// #define PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS 259200
#define PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS 259200

// <q> Enable virtual current
#define PROJECT_CFG_VIRTUAL_CURRENT_ENABLE 1

// <q> Enable long-key power switch
#define PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE 1

// <q> Enable debug system monitor
// <i> Exports g_dbg global struct with all IO/peripheral/function states for Keil watch.
// <i> Release must keep disabled.
#ifndef PROJECT_CFG_DEBUG_MONITOR_ENABLE
#define PROJECT_CFG_DEBUG_MONITOR_ENABLE 0
#endif

// <q> Enable IRQ debug counters
// <i> Keeps lightweight interrupt counters for Keil watch and STOP wakeup debug.
#ifndef PROJECT_CFG_IRQ_DEBUG_ENABLE
#define PROJECT_CFG_IRQ_DEBUG_ENABLE 0
#endif

// <q> Enable IRQ debug event ring
// <i> High-rate interrupts are counted but not pushed into the event ring.
#ifndef PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE
#define PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE 0
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
// <i> Continuous full-charge confirmation time before SOC can be raised by one step.
#define PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS 15

// <o> Calibration min valid cell voltage mV <1000-3500>
// <i> Rejects OCV/full/low-tail calibration when a cell voltage is below this range.
#define PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV 2000

// <o> Calibration max valid cell voltage mV <3600-6000>
// <i> Rejects OCV/full/low-tail calibration when a cell voltage is above this range.
#define PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV 5000

// <o> Sag holdoff seconds <0-1800>
// <i> Delays OCV/low-tail calibration after heavy discharge to avoid rebound miscalibration.
#define PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS 30

// <o> Sag allow offset mV <0-500>
// <i> Sag hold blocks calibration only while Vmin is above empty voltage plus this offset.
#define PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV 50

// <q> Enable rest OCV slow downward calibration
// <i> When enabled, rest OCV triggers once after long stable rest to correct drift and self-discharge.
#define PROJECT_CFG_SOC_REST_OCV_ENABLE 1

// <o> Rest OCV wait seconds <60-43200>
// <i> Minimum stable-rest time before rest OCV target can be trusted. 3600 = 1 hour.
#define PROJECT_CFG_SOC_REST_OCV_SECONDS 3600

// <o> Rest down step seconds <60-43200>
// <i> Long-rest slow-down period; each period allows at most one SOC step toward OCV target.
#define PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS 1800

// <o> Auto calibration max step percent <1-10>
// <i> Maximum SOC change per automatic calibration step.
#define PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT 1

// <o> Board self consumption mA <0-1000>
// <i> Normal running self-consumption current included in coulomb counting.
#define PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA 15

// <o> Empty tail start offset mV <0-1000>
// <i> Enables low-tail downward calibration when Vmin is below empty voltage plus this offset.
#define PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV 400
// </h>

// <h>Upgrade Parameter Policy

// <o> Policy version <0x0000-0xFFFE>
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION 0x0005

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
#define PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME 1

// <q> Update all OtherElement words from Project_Config on upgrade
// <i> Raise policy version when enabling this action for a release package.
#define PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT 1

// <h>Upgrade OtherElement Values
// <i> Used only when PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT is 1.
// <i> These 32 words map to RS485 0x2300..0x231F without changing protocol order.

// <o> Balance open voltage mV <1000-5000>
#define PROJECT_CFG_UPGRADE_OTHER_BALANCE_OPEN_VOLTAGE 4160
// <o> Balance open window mV <1-2000>
#define PROJECT_CFG_UPGRADE_OTHER_BALANCE_OPEN_WINDOW 30
// <o> Balance close window mV <0-2000>
#define PROJECT_CFG_UPGRADE_OTHER_BALANCE_CLOSE_WINDOW 20
// <o> Balance reserved 1 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_BALANCE_RES1 0
// <o> Balance reserved 2 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_BALANCE_RES2 0
// <o> Balance reserved 3 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_BALANCE_RES3 0
// <o> Balance reserved 4 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_BALANCE_RES4 0
// <o> Balance reserved 5 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_BALANCE_RES5 0

// <o> Charge current max A*10 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_CS_CUR_CHGMAX 1865
// <o> Discharge current max A*10 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_CS_CUR_DSGMAX 1865
// <o> CBC delay time us*10 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_CBC_DELAY_T 2000
// <o> CBC discharge current A*10 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_CBC_CUR_DSG 3200

// <o> SOC table select <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_SOC_TABLE_SELECT 2
// <o> Password always <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_PASSWORD_ALWAYS 0
// <o> Current-limit voltage delta mV <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_CUR_LIMIT_VDELTA 1000
// <o> Current-limit current A*10 <0-65000>
#define PROJECT_CFG_UPGRADE_OTHER_CUR_LIMIT_CUR 30

// <o> Normal sleep voltage mV <1000-5000>
#define PROJECT_CFG_UPGRADE_OTHER_SLEEP_V_NORMAL 4200
// <o> Normal sleep time min <1-65000>
#define PROJECT_CFG_UPGRADE_OTHER_SLEEP_TIME_NORMAL 7200
// <o> Low-voltage sleep voltage mV <1000-5000>
#define PROJECT_CFG_UPGRADE_OTHER_SLEEP_V_LOW 3000
// <o> Low-voltage sleep time min <1-65000>
#define PROJECT_CFG_UPGRADE_OTHER_SLEEP_TIME_LOW 1
// <o> Sleep virtual charge current A*10 <0-50000>
#define PROJECT_CFG_UPGRADE_OTHER_SLEEP_VIR_CUR_CHG 10
// <o> Sleep virtual discharge current A*10 <0-50000>
#define PROJECT_CFG_UPGRADE_OTHER_SLEEP_VIR_CUR_DSG 10
// <o> Sleep RTC wakeup time min <0-50000>
#define PROJECT_CFG_UPGRADE_OTHER_SLEEP_RTC_WAKEUP_TIME 240
// <o> Sleep RTC time min <0-50000>
#define PROJECT_CFG_UPGRADE_OTHER_SLEEP_TIME_RTC 3

// <o> SOC capacity Ah*10 <1-65000>
#define PROJECT_CFG_UPGRADE_OTHER_SOC_AH 270
// <o> SOC cycle times <1-50000>
#define PROJECT_CFG_UPGRADE_OTHER_SOC_CYCLE_TIMES 1
// <o> SOC 100 percent voltage mV <1-50000>
#define PROJECT_CFG_UPGRADE_OTHER_SOC_V_100 4180
// <o> SOC 0 percent voltage mV <1-50000>
#define PROJECT_CFG_UPGRADE_OTHER_SOC_V_0 3000

// <o> System series number <3-32>
#define PROJECT_CFG_UPGRADE_OTHER_SYS_SERIES_NUM 7
// <o> System current-sense resistor mOhm <1-65000>
#define PROJECT_CFG_UPGRADE_OTHER_SYS_CS_RES 2
// <o> System current-sense resistor numerator <1-10000>
#define PROJECT_CFG_UPGRADE_OTHER_SYS_CS_RES_NUM 6
// <o> System precharge time s <0-50000>
#define PROJECT_CFG_UPGRADE_OTHER_SYS_PRECHG_TIME 10
// </h>
// </h>

// <<< end of configuration section >>>

#endif

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* Keil uVision: right click this file and select "Configuration Wizard". */
/* ÏîÄ¿¿ÉÊÓ»¯ÅäÖÃ: Keil Configuration Wizard marker. */

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Project Basic Configuration


// <o> Battery chemistry
//   <0=> Ternary Lithium TERNARYLI
//   <1=> LiFePO4 LIFEPO
#define PROJECT_CFG_BAT_CHEMISTRY 1

// <q> Enable host write registers
#define PROJECT_CFG_HOST_WRITE_ENABLE 1

// <h>Feature Switches

// <q> Enable IWDG
#define PROJECT_CFG_WDOG_ENABLE 1

// <q> Enable RTC
#define PROJECT_CFG_RTC_ENABLE 1

// </h>

// <h>Switch behavior (PA9, shorted = low)
#define PROJECT_SWITCH_MAINTAINED 0
#define PROJECT_SWITCH_PUSHBUTTON 1
// <o> Switch mode
//   <0=> Maintained: shorted RUN, open NORMAL (50 ms debounce)
//   <1=> Pushbutton: hold 3 s NORMAL; HICCUP IRQ wake/restart hold; NORMAL/DEEP 1 s wake/release
// <i> Both modes retain charger wake and protective DEEP priority.
// <i> Maintained mode stays RUN while charger input is active; opening takes effect after removal.
#ifndef PROJECT_CFG_SWITCH_MODE
#define PROJECT_CFG_SWITCH_MODE 1
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

// <h>Flash parameter updates after firmware upgrade
// <q> Reset ALL parameters once on each new firmware version
// <i> Factory reset takes priority over the module versions below. Statistics are always preserved.
// <i> Keep 0 for selective updates; 1 uses the complete BMS_SOFTWARE_VERDION_DEFAULT string.
#define PROJECT_CFG_RESET_PARAMS_ON_NEW_VERSION 1

// Each module: 0 = preserve; 1..65534 = apply compiled defaults once when the
// saved version differs. Increment only the modules to update. Reusing a
// version does not overwrite host changes or repeat on reboot/reflash.
// <o> AFE parameter version <0-65534>
#define PROJECT_CFG_PARAM_AFE_VERSION 8
// <o> Software protection parameter version <0-65534>
#define PROJECT_CFG_PARAM_PROTECT_VERSION 8
// <o> SOC configuration version <0-65534>
// <i> Table selection, rated capacity, initial cycle count and full/empty voltages; keeps SOC state.
#define PROJECT_CFG_PARAM_SOC_VERSION 8
// <o> Other parameter version <0-65534>
// <i> Excludes the SOC fields above; protocol addresses are unchanged.
#define PROJECT_CFG_PARAM_OTHER_VERSION 8
// <o> Current calibration version <0-65534>
#define PROJECT_CFG_PARAM_CALIB_VERSION 8
// <o> Heating/cooling parameter version <0-65534>
#define PROJECT_CFG_PARAM_HEAT_COOL_VERSION 8
// <o> SOC state reset version <0-65534>
// <i> Explicitly discards SOC/capacity/cycle snapshot; startup rebuilds it by the existing SOC rules.
#define PROJECT_CFG_PARAM_SOC_STATE_VERSION 8
// <o> Factory aging state reset version <0-65534>
#define PROJECT_CFG_PARAM_AGING_STATE_VERSION 8
// <o> Recent 100 logs clear version <0-65534>
// <i> Does not clear the customer's lifetime event counters.
#define PROJECT_CFG_PARAM_LOG_VERSION 8
// </h>

// <<< end of configuration section >>>

#endif

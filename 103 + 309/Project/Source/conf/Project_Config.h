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

// <h>Explicit Flash parameter update
// <o> Parameter update generation <0-65534>
// <i> 0 = preserve stored parameters. Increment only for an intentional update.
#define PROJECT_CFG_STORAGE_UPDATE_GENERATION 0
// <o> Parameter update mask <0-255>
// <i> Bits: 0 AFE, 1 protection, 2 SOC, 3 other, 4 current K/B,
// <i> 5 heat/cool, 6 SOC state, 7 recent log.
// <i> Keep mask 0 when generation 0. Lifetime statistics are preserved.
#define PROJECT_CFG_STORAGE_UPDATE_MASK 0
// </h>

// <<< end of configuration section >>>

#endif

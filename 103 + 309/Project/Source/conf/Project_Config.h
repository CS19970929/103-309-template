#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/*
 * Product-facing configuration.
 *
 * Keil uVision: right click this file and select "Configuration Wizard".
 * Keep only daily release/product decisions here. Test switches and algorithm
 * tuning defaults live in Project_AdvancedConfig.h.
 */

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Project Visual Configuration

// <h>Build Profile
// <o> Build profile
//   <0=> Release
//   <1=> Debug
//   <2=> Factory/Test
#ifndef PROJECT_CFG_BUILD_PROFILE
#define PROJECT_CFG_BUILD_PROFILE 0
#endif
// </h>

// <h>Product
// <o> EEPROM init flag <0x0000-0xFFFF>
// <i> Change this value only when field parameters must be reinitialized.
#define PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG 0x2445

// <o> Battery type
//   <0=> BAT_MASTER, 20A
//   <1=> BAT_SLAVE, 40A
#define PROJECT_CFG_BAT_TYPE 1

// <o> Battery chemistry
//   <0=> TERNARYLI
//   <1=> LIFEPO
#define PROJECT_CFG_BAT_CHEMISTRY 0

// <o> Firmware year <0-99>
#define PROJECT_CFG_FD_YEAR 26

// <o> Firmware month <1-12>
#define PROJECT_CFG_FD_MONTH 4

// <o> Firmware day <1-31>
#define PROJECT_CFG_FD_DAY 15

// <o> Firmware version <0-65535>
#define PROJECT_CFG_VERSION 5

// <o> Current level
//   <0=> CURR_80A
//   <1=> CURR_100A
//   <2=> CURR_150A
//   <3=> CURR_200A
//   <4=> CURR_250A
#define PROJECT_CFG_LEVEL_CURR 2

// <o> AFE type
//   <0=> bq76xx_afe
//   <1=> sh36xx
#define PROJECT_CFG_AFE_TYPE 1
// </h>

// <h>Feature Switches
// <q> Enable IWDG watchdog
#define PROJECT_CFG_WDOG_ENABLE 1

// <q> Enable RTC function
#define PROJECT_CFG_RTC_ENABLE 1

// <q> Enable heat function
#define PROJECT_CFG_HEAT_ENABLE 0

// <q> Enable load remove short function
#define PROJECT_CFG_LOAD_REMOVE_SHORT_ENABLE 0

// <q> Enable UART1 wakeup
#define PROJECT_CFG_UART1_WAKEUP_ENABLE 1

// <q> Enable UART2 wakeup
#define PROJECT_CFG_UART2_WAKEUP_ENABLE 0

// <q> Enable RS485 wakeup
#define PROJECT_CFG_RS485_WAKEUP_ENABLE 1

// <q> Enable second current protection
#define PROJECT_CFG_SECOND_CURR_PROTECT_ENABLE 0

// <q> Enable virtual current compensation
#define PROJECT_CFG_VIRTUAL_CURRENT_ENABLE 1

// <q> Enable DI system on/off switch
#define PROJECT_CFG_DI_SWITCH_SYS_ONOFF_ENABLE 0

// <q> Enable DI discharge on/off switch
#define PROJECT_CFG_DI_SWITCH_DSG_ONOFF_ENABLE 0

// <q> Enable DI long-key on/off switch
#define PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE 1

// <q> Enable LED function marker
#define PROJECT_CFG_LED_FUNC_ENABLE 0

// <q> Enable sleep with current
#define PROJECT_CFG_SLEEP_WITH_CURRENT_ENABLE 0

// <h>SCI Role
// <o> SCI1 role
//   <0=> Disabled
//   <1=> Common upper
//   <2=> Client
//   <3=> LCD
#define PROJECT_CFG_SCI1_ROLE 1

// <o> SCI2 role
//   <0=> Disabled
//   <1=> Common upper
//   <2=> Client
//   <3=> LCD
#define PROJECT_CFG_SCI2_ROLE 0

// <o> SCI3 role
//   <0=> Disabled
//   <1=> Common upper
//   <2=> Client
//   <3=> LCD
#define PROJECT_CFG_SCI3_ROLE 0
// </h>

// <h>LedBar
// <q> Use GPIO Charlieplexing driver
#define PROJECT_CFG_LEDBAR_DRIVER_GPIO_CHARLIE 1

// <q> Enable LedBar sleep SOC backup
#define PROJECT_CFG_LEDBAR_SLEEP_ENABLE 1
// </h>

// <h>Upgrade Parameter Policy
// <q> Enable upgrade parameter policy
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE 0

// <o> Upgrade parameter policy version <0x0000-0xFFFE>
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION 0x0429

// <q> Reset AFE parameters
#define PROJECT_CFG_UPGRADE_PARAM_RESET_AFE 1

// <q> Reset protect parameters
#define PROJECT_CFG_UPGRADE_PARAM_RESET_PROTECT 0

// Reset OtherElement balance open voltage
#define PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE 1

// <q> Reset SOC table
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE 0

// <q> Reset SOC config
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG 1

// <q> Reset SOC snapshot
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT 1

// <q> Reset event record
#define PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD 1

// <q> Force reapply policy, test only
#define PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY 0
// </h>

// </h>

// <<< end of configuration section >>>

#include "Project_AdvancedConfig.h"

#endif

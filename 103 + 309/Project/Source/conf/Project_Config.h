#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* Keil uVision: right click this file and select "Configuration Wizard". */

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Project Visual Configuration

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

// <q> Enable debug code
#define PROJECT_CFG_DEBUG_CODE_ENABLE 0

// <q> Enable sleep with current
#define PROJECT_CFG_SLEEP_WITH_CURRENT_ENABLE 0

// <q> Enable IAP jump support
#define PROJECT_CFG_IAP_ENABLE 1
// </h>

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

// <h>Flash 64K Storage Test
// <q> Enable destructive quick test
#define PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE 0

// <o> Quick test cycles <1-65535>
#define PROJECT_CFG_FLASH64K_QUICK_TEST_CYCLES 96

// <q> Enable app running storage test
#define PROJECT_CFG_FLASH64K_USE_TEST_ENABLE 0

// <o> App test print period, seconds <1-65535>
#define PROJECT_CFG_FLASH64K_USE_TEST_PRINT_PERIOD_SEC 10

// <q> Enable app test acceleration
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE 1

// <o> Accelerated SOC save period, seconds <1-65535>
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_SOC_PERIOD_SEC 1

// <o> Accelerated AFE save period, seconds <1-65535>
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_AFE_PERIOD_SEC 30
// </h>

// <h>LedBar
// <q> Use GPIO Charlieplexing driver
#define PROJECT_CFG_LEDBAR_DRIVER_GPIO_CHARLIE 1

// <q> Enable LedBar sleep SOC backup
#define PROJECT_CFG_LEDBAR_SLEEP_ENABLE 1

// <q> Enable long-press GPIO toggle test
#define PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST 0

// <o> SOC display time, 10ms ticks <1-65535>
#define PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS 500

// <q> Enable SOC display snap strategy
#define PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_ENABLE 0

// <o> SOC display snap window <0-20>
#define PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_WINDOW 2

// <o> SOC snap minimum extra segments <0-30>
#define PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_MIN_EXTRA 4

// <o> SOC snap minimum gain <0-30>
#define PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_MIN_GAIN 1

// <o> Scan timer period in 100kHz ticks <1-1000>
#define PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS 50

// <o> MCU_WK on filter, 10ms ticks <0-255>
#define PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS 3

// <o> MCU_WK off filter, 10ms ticks <0-255>
#define PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS 3

// <o> Charge on filter, 100ms ticks <0-255>
#define PROJECT_CFG_LEDBAR_CHARGE_ON_FILTER_100MS 1

// <o> Charge off filter, 100ms ticks <0-255>
#define PROJECT_CFG_LEDBAR_CHARGE_OFF_FILTER_100MS 5
// </h>

// <h>Upgrade Parameter Policy
// <q> Enable upgrade parameter policy
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE 1

// <o> Upgrade parameter policy version <0x0000-0xFFFE>
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION 0x0005

// <q> Reset AFE parameters
#define PROJECT_CFG_UPGRADE_PARAM_RESET_AFE 0

// <q> Reset protect parameters
#define PROJECT_CFG_UPGRADE_PARAM_RESET_PROTECT 0

// Reset OtherElement balance open voltage
#define PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE 1

// <q> Reset SOC table
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE 0

// <q> Reset SOC config
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG 0

// <q> Reset SOC snapshot
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT 0

// <q> Force reapply policy, test only
#define PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY 0
// </h>

// </h>

// <<< end of configuration section >>>

#endif

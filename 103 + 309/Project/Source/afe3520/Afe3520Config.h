#ifndef AFE3520_CONFIG_H
#define AFE3520_CONFIG_H

/* SH3673520 CV1.0A: defaults use physical units, never register bytes.
 * Runtime hardware profile: Bms3520_GetHardwareConfig/SetHardwareConfig.
 * Software protection remains independent in the existing 0x2400 area. */
#include <stdint.h>

/* Reference board: MOS thermistor is wired to TS4 (zero-based index 3). */
#define AFE3520_MOS_TEMP_INDEX 3U

// <<< Use Configuration Wizard in Context Menu >>>
// <h>Protection sources (service/MOS feedback always runs)
// <o> Protection mode <1=>Software only <2=>AFE hardware only <3=>Both
#ifndef BMS3520_CFG_PROTECTION_MODE
#define BMS3520_CFG_PROTECTION_MODE 3
#endif
#define BMS3520_CFG_SW_PROTECTION (BMS3520_CFG_PROTECTION_MODE != 2)
#define BMS3520_CFG_HW_PROTECTION (BMS3520_CFG_PROTECTION_MODE != 1)
#if (BMS3520_CFG_PROTECTION_MODE < 1) || (BMS3520_CFG_PROTECTION_MODE > 3)
#error "Protection mode must be 1, 2 or 3"
#endif
// </h>
// <h>SPI transport
// <q> Use STM32 SPI1 (0 = reference software SPI, 1 = hardware SPI)
#ifndef AFE3520_CFG_USE_HARDWARE_SPI
#define AFE3520_CFG_USE_HARDWARE_SPI 1
#endif
// <o> Maximum SPI clock, Hz (hardware divider selected from actual PCLK2)
#ifndef AFE3520_CFG_SPI_TARGET_HZ
#define AFE3520_CFG_SPI_TARGET_HZ 500000U
#endif
// <o> Hardware flag timeout, microseconds; TIM4 is reserved by this driver
#define AFE3520_CFG_SPI_TIMEOUT_US 1000U
// <o> Good 200ms protection cycles required after a communication fault
#define AFE3520_CFG_COMM_RECOVERY_TICKS 3U
// </h>
#if (AFE3520_CFG_USE_HARDWARE_SPI != 0) && (AFE3520_CFG_USE_HARDWARE_SPI != 1)
#error "AFE3520_CFG_USE_HARDWARE_SPI must be 0 or 1"
#endif
#if (AFE3520_CFG_SPI_TARGET_HZ < 10000U) || (AFE3520_CFG_SPI_TARGET_HZ > 500000U)
#error "SPI target must be 10000..500000 Hz"
#endif
#if (AFE3520_CFG_SPI_TIMEOUT_US < 1) || (AFE3520_CFG_SPI_TIMEOUT_US > 5000) || (AFE3520_CFG_COMM_RECOVERY_TICKS < 1)
#error "Polling and recovery limits must be nonzero"
#endif
// <h>Emergency battery preservation (overrides ordinary sleep blockers)
// <o> Persistent AFE/global fault timeout, seconds
#define AFE3520_CFG_EMERGENCY_FAULT_SECONDS 300U
// <o> Confirmed critical undervoltage timeout, seconds
#define AFE3520_CFG_EMERGENCY_LOW_SECONDS 60U
// <q> Fatal CPU exceptions reset directly into emergency sleep
#define AFE3520_CFG_FATAL_SLEEP_ENABLE 1
// <q> Allow PA0 charger/AFE wake in emergency sleep (default: key only)
#define AFE3520_CFG_EMERGENCY_CHARGER_WAKE 0
// </h>
#if (AFE3520_CFG_EMERGENCY_FAULT_SECONDS < 1) || (AFE3520_CFG_EMERGENCY_FAULT_SECONDS > 65535) || (AFE3520_CFG_EMERGENCY_LOW_SECONDS < 1) || (AFE3520_CFG_EMERGENCY_LOW_SECONDS > 65535)
#error "Emergency timeouts must be 1..65535 seconds"
#endif
// <h>AFE hardware defaults (CV1.0A, NORMAL mode delay units)
// Watchdog is a build policy; default OFF for debugger halt.
#ifndef AFE3520_CFG_WDT_ENABLE
#define AFE3520_CFG_WDT_ENABLE 0
#endif
// Allowed: 32340, 15680, 7840, 3920 ms.
#define AFE3520_CFG_WDT_TIMEOUT_MS 32340U
// Port topology: 1 common port (MOS_EN on), 0 separate ports (MOS_EN off).
// CV1.0A 7.10: common-port reverse current may turn a FET on autonomously.
#ifndef AFE3520_CFG_COMMON_PORT
#define AFE3520_CFG_COMMON_PORT 1U
#endif
#define AFE3520_CFG_PUMP_ENABLE 0U
#define AFE3520_CFG_AUTO_POWERDOWN_ENABLE 0U
#define AFE3520_CFG_CHARGER_WAKE_ENABLE 0U
// Load wake: 0 disabled, 1 connect, 2 disconnect.
#define AFE3520_CFG_LOAD_WAKE_MODE 0U
// C+ detection: 0 disabled, 1 voltage, 2 load status.
#define AFE3520_CFG_LOAD_DETECT_MODE 0U
#define AFE3520_CFG_OPEN_WIRE_ENABLE 0U
#define AFE3520_CFG_PRE_DISCHARGE_MS 210U
#define AFE3520_CFG_CADC_ENABLE 1U
#define AFE3520_CFG_CADC_IDLE_SECONDS 4U
#define AFE3520_CFG_LOAD_PULLUP_UA 60U
// Half-microvolt units allow exact 192.5 + code*137.5 uV.
#define AFE3520_CFG_CURRENT_STATE_UV_X2 1485U
#define AFE3520_CFG_OPEN_WIRE_MV 960U
// Alarm sources are named individually; 1 enables an ALARM pulse.
#define AFE3520_CFG_ALARM_LOAD_CONNECT 0U
#define AFE3520_CFG_ALARM_LOAD_DISCONNECT 1U
#define AFE3520_CFG_ALARM_VADC 1U
#define AFE3520_CFG_ALARM_CADC 1U
#define AFE3520_CFG_ALARM_WAKE 1U
#define AFE3520_CFG_ALARM_WDT 1U
#define AFE3520_CFG_ALARM_OPEN_WIRE 1U
#define AFE3520_CFG_ALARM_TEMPERATURE 1U
#define AFE3520_CFG_ALARM_OCC 1U
#define AFE3520_CFG_ALARM_OCD 1U
#define AFE3520_CFG_ALARM_UV 1U
#define AFE3520_CFG_ALARM_OV 1U

#define AFE3520_CFG_OV_ENABLE 1U
#define AFE3520_CFG_UV_ENABLE 1U
#define AFE3520_CFG_OCD_ENABLE 1U
#define AFE3520_CFG_SC_ENABLE 1U
#define AFE3520_CFG_OCC_ENABLE 1U
// TS protection enables do NOT disable temperature acquisition.
#define AFE3520_CFG_TS1_PROTECTION 1U
#define AFE3520_CFG_TS2_PROTECTION 1U
#define AFE3520_CFG_TS3_PROTECTION 1U
#define AFE3520_CFG_TS4_PROTECTION 0U
#define AFE3520_CFG_OV_MV 4250U
#define AFE3520_CFG_OV_DELAY_MS 140U
#define AFE3520_CFG_UV_MV 2650U
#define AFE3520_CFG_UV_DELAY_MS 490U
// Current thresholds are voltages across the actual shunt, NOT cell voltages.
// I[A] = threshold[mV] / shunt[mOhm]. Default shunt: 0.25 mOhm.
#define AFE3520_CFG_OCD1_SHUNT_MV 50U
#define AFE3520_CFG_OCD1_DELAY_MS 980U
#define AFE3520_CFG_OCD2_SHUNT_MV 40U
#define AFE3520_CFG_OCD2_DELAY_MS 25U
#define AFE3520_CFG_SC_OCD2_MULTIPLIER 2U
#define AFE3520_CFG_SC_DELAY_US 256U
#define AFE3520_CFG_OCC_SHUNT_UV 11000U
#define AFE3520_CFG_OCC_DELAY_MS 140U
// Exact NTC resistance in ohms preserves reference threshold encoding.
// Approximate temperatures depend on the installed 10K NTC curve.
#define AFE3520_CFG_CHG_OT_OHM 3550UL   // about +55 C
#define AFE3520_CFG_DSG_OT_OHM 1935UL   // about +75 C
#define AFE3520_CFG_CHG_UT_OHM 27513UL  // about 0 C
#define AFE3520_CFG_DSG_UT_OHM 116110UL // about -30 C
#define AFE3520_CFG_HW_OV_RECOVERY_MV 4150U
#define AFE3520_CFG_HW_UV_RECOVERY_MV 2900U
#define AFE3520_CFG_HW_CHG_OT_RECOVERY_C 50
#define AFE3520_CFG_HW_DSG_OT_RECOVERY_C 70
#define AFE3520_CFG_HW_CHG_UT_RECOVERY_C 5
#define AFE3520_CFG_HW_DSG_UT_RECOVERY_C (-15)
#define AFE3520_CFG_HW_OCP_RECOVERY_MA 1000U
#define AFE3520_CFG_HW_RECOVERY_MS 5000U
// </h>
// <<< end of configuration section >>>

/* Encoding is internal to the driver. Application settings above use units.
 * MOS_EN follows port topology; report actual FET feedback independently. */
#define AFE3520_CFG_WDT_CODE ((AFE3520_CFG_WDT_TIMEOUT_MS == 32340U) ? 0U : \
    (AFE3520_CFG_WDT_TIMEOUT_MS == 15680U) ? 1U : \
    (AFE3520_CFG_WDT_TIMEOUT_MS == 7840U) ? 2U : 3U)
#define AFE3520_CFG_EFFECTIVE_SCONF5 ( \
    (AFE3520_CFG_COMMON_PORT ? AFE3520_SCONF5_MOS_EN : 0U) | \
    (BMS3520_CFG_HW_PROTECTION && AFE3520_CFG_OCC_ENABLE ? AFE3520_SCONF5_OCC_EN : 0U) | \
    (AFE3520_CFG_CADC_ENABLE ? AFE3520_SCONF5_CADC_EN : 0U) | \
    (AFE3520_CFG_WDT_ENABLE ? AFE3520_SCONF5_WDT_EN : 0U) | AFE3520_CFG_WDT_CODE)
#if (AFE3520_CFG_WDT_ENABLE != 0) && (AFE3520_CFG_WDT_ENABLE != 1)
#error "WDT enable must be 0 or 1"
#endif
#if (AFE3520_CFG_WDT_TIMEOUT_MS != 32340U) && (AFE3520_CFG_WDT_TIMEOUT_MS != 15680U) && (AFE3520_CFG_WDT_TIMEOUT_MS != 7840U) && (AFE3520_CFG_WDT_TIMEOUT_MS != 3920U)
#error "Unsupported watchdog timeout"
#endif

#if (AFE3520_CFG_COMMON_PORT != 0) && (AFE3520_CFG_COMMON_PORT != 1)
#error "COMMON_PORT must be 0 or 1"
#endif
#if (AFE3520_CFG_PUMP_ENABLE != 0) && (AFE3520_CFG_PUMP_ENABLE != 1)
#error "PUMP_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_AUTO_POWERDOWN_ENABLE != 0) && (AFE3520_CFG_AUTO_POWERDOWN_ENABLE != 1)
#error "PRE_DISCHARGE_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_CHARGER_WAKE_ENABLE != 0) && (AFE3520_CFG_CHARGER_WAKE_ENABLE != 1)
#error "CHARGER_WAKE_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_OPEN_WIRE_ENABLE != 0) && (AFE3520_CFG_OPEN_WIRE_ENABLE != 1)
#error "OPEN_WIRE_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_CADC_ENABLE != 0) && (AFE3520_CFG_CADC_ENABLE != 1)
#error "CADC_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_OV_ENABLE != 0) && (AFE3520_CFG_OV_ENABLE != 1)
#error "OV_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_UV_ENABLE != 0) && (AFE3520_CFG_UV_ENABLE != 1)
#error "UV_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_OCD_ENABLE != 0) && (AFE3520_CFG_OCD_ENABLE != 1)
#error "OCD_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_SC_ENABLE != 0) && (AFE3520_CFG_SC_ENABLE != 1)
#error "SC_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_OCC_ENABLE != 0) && (AFE3520_CFG_OCC_ENABLE != 1)
#error "OCC_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_TS1_PROTECTION != 0) && (AFE3520_CFG_TS1_PROTECTION != 1)
#error "TS1_PROTECTION must be 0 or 1"
#endif
#if (AFE3520_CFG_TS2_PROTECTION != 0) && (AFE3520_CFG_TS2_PROTECTION != 1)
#error "TS2_PROTECTION must be 0 or 1"
#endif
#if (AFE3520_CFG_TS3_PROTECTION != 0) && (AFE3520_CFG_TS3_PROTECTION != 1)
#error "TS3_PROTECTION must be 0 or 1"
#endif
#if (AFE3520_CFG_TS4_PROTECTION != 0) && (AFE3520_CFG_TS4_PROTECTION != 1)
#error "TS4_PROTECTION must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_LOAD_CONNECT != 0) && (AFE3520_CFG_ALARM_LOAD_CONNECT != 1)
#error "ALARM_LOAD_CONNECT must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_LOAD_DISCONNECT != 0) && (AFE3520_CFG_ALARM_LOAD_DISCONNECT != 1)
#error "ALARM_LOAD_DISCONNECT must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_VADC != 0) && (AFE3520_CFG_ALARM_VADC != 1)
#error "ALARM_VADC must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_CADC != 0) && (AFE3520_CFG_ALARM_CADC != 1)
#error "ALARM_CADC must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_WAKE != 0) && (AFE3520_CFG_ALARM_WAKE != 1)
#error "ALARM_WAKE must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_WDT != 0) && (AFE3520_CFG_ALARM_WDT != 1)
#error "ALARM_WDT must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_OPEN_WIRE != 0) && (AFE3520_CFG_ALARM_OPEN_WIRE != 1)
#error "ALARM_OPEN_WIRE must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_TEMPERATURE != 0) && (AFE3520_CFG_ALARM_TEMPERATURE != 1)
#error "ALARM_TEMPERATURE must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_OCC != 0) && (AFE3520_CFG_ALARM_OCC != 1)
#error "ALARM_OCC must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_OCD != 0) && (AFE3520_CFG_ALARM_OCD != 1)
#error "ALARM_OCD must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_UV != 0) && (AFE3520_CFG_ALARM_UV != 1)
#error "ALARM_UV must be 0 or 1"
#endif
#if (AFE3520_CFG_ALARM_OV != 0) && (AFE3520_CFG_ALARM_OV != 1)
#error "ALARM_OV must be 0 or 1"
#endif

/* Stable API fields, NOT a wire/Flash layout. Never memcpy this struct to a
 * protocol frame: map fields explicitly and version any future persisted data.
 * enableMask uses named AFE3520_SCONF6_* bits; occEnable is separate. */
typedef struct
{
    uint16_t ovMv, ovDelayMs, uvMv, uvDelayMs;
    uint16_t ocd1ShuntMv, ocd1DelayMs, ocd2ShuntMv, ocd2DelayMs;
    uint16_t scOcd2Multiplier, scDelayUs, occShuntUv, occDelayMs;
    uint32_t chgOtOhm, dsgOtOhm, chgUtOhm, dsgUtOhm;
    uint16_t ovRecoveryMv, uvRecoveryMv;
    int16_t chgOtRecoveryC, dsgOtRecoveryC, chgUtRecoveryC, dsgUtRecoveryC;
    uint16_t ocpRecoveryMa, recoveryMs;
    uint16_t enableMask, occEnable;
} BMS3520_HARDWARE_CONFIG;

#endif

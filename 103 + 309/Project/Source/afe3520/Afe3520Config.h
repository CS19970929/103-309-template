#ifndef AFE3520_CONFIG_H
#define AFE3520_CONFIG_H

/* SH3673520 AFE 硬件配置唯一入口；基线 78b4f5f/main.c。
 * MCU 软件保护仍由 0x2400 参数区配置，不再反向覆盖本表。
 * 单位：电压 mV；其余 CODE/REG 是数据手册原始编码。
 * KEEP 表示参考分支不写该寄存器；可改成 0x00..0xFF 显式覆盖。
 * KEEP 不等于零，也不保证 MCU 单独复位后该寄存器恢复出厂值。
 * 修改本文件后重新编译；串数沿用持久化 SeriesNum (5..20)。 */
#define AFE3520_CONFIG_KEEP (-1)

// <<< Use Configuration Wizard in Context Menu >>>
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
// <h>SH3673520 AFE Configuration
// <q> Enable AFE watchdog (MCU halt does NOT stop this watchdog)
#ifndef AFE3520_CFG_WDT_ENABLE
#define AFE3520_CFG_WDT_ENABLE 0
#endif
// <o> SCONF2 base (MOS commands are controlled at runtime) <0-127>
// <i> Reference: PUMP_EN=0, PD_EN=0, PDSG_CTL=0.
#define AFE3520_CFG_SCONF2 0x00U
// <o> SCONF3 (OWD and wake control) <0-127>
#define AFE3520_CFG_SCONF3 0x00U
// <o> SCONF5 base (exclude WDT_EN bit 2) <0-255>
// <i> 0x18: OCC_EN=1, CADC_EN=1, MOS_EN=0; other bits=0.
#define AFE3520_CFG_SCONF5 0x18U
// <o> SCONF6 protection and TS enable mask <0-255>
// <i> 0x7F: OV/UV/OCD/SC and TS1/TS2/TS3 on, TS4 off.
#define AFE3520_CFG_SCONF6 0x7FU
// <o> OV threshold mV (5mV step) <0-5115:5>
#define AFE3520_CFG_OV_MV 4250U
// <o> OVT code <0-7>
// <i> 0..7: 140/280/490/988/2030/3010/4970/10010 ms.
#define AFE3520_CFG_OV_DELAY_CODE 0U
// <o> UV threshold mV (5mV step) <0-5115:5>
#define AFE3520_CFG_UV_MV 2650U
// <o> UVT code <0-7>
// <i> 0..7: 490/770/980/1470/2030/3010/4970/10010 ms.
#define AFE3520_CFG_UV_DELAY_CODE 0U
// <o> OCD2 raw register <0-255>
// <i> Low nibble: (code+1)*10mV; high nibble: (code+1)*25ms.
#define AFE3520_CFG_OCD2 0x03U
// <o> OCC raw register <0-255>
// <i> Low 5 bits: (code+1)*1.375mV; high 3 bits: delay code. 0x07=11mV/140ms.
#define AFE3520_CFG_OCC 0x07U
// <o> OTC raw NTC code <0-255>
// <i> Reference Rt=3.55k, Rref=10k: floor(Rt*512/(10+Rt)).
#define AFE3520_CFG_OTC 0x86U
// <o> OTD raw NTC code <0-255>
// <i> Reference Rt=1.935k.
#define AFE3520_CFG_OTD 0x53U
// <o> UTC raw NTC code <0-255>
// <i> Reference Rt=27.513k: floor(Rt*512/(10+Rt))-256.
#define AFE3520_CFG_UTC 0x77U
// <o> UTD raw NTC code <0-255>
// <i> Reference Rt=116.11k.
#define AFE3520_CFG_UTD 0xD7U
// </h>
// <<< end of configuration section >>>

/* 参考初始化未写的寄存器，默认保留；需要时直接填原始字节。 */
#define AFE3520_CFG_SCONF7 AFE3520_CONFIG_KEEP
#define AFE3520_CFG_OWV_ALARMH AFE3520_CONFIG_KEEP
#define AFE3520_CFG_ALARML AFE3520_CONFIG_KEEP
#define AFE3520_CFG_OCD1 AFE3520_CONFIG_KEEP
#define AFE3520_CFG_SC AFE3520_CONFIG_KEEP

#if (AFE3520_CFG_WDT_ENABLE != 0) && (AFE3520_CFG_WDT_ENABLE != 1)
#error "AFE3520_CFG_WDT_ENABLE must be 0 or 1"
#endif
#if (AFE3520_CFG_SCONF5 & 0x04U)
#error "Set watchdog only through AFE3520_CFG_WDT_ENABLE"
#endif
#if (AFE3520_CFG_SCONF2 & 0xA7U) || (AFE3520_CFG_SCONF3 & 0x81U)
#error "Mode commands, LTCLR, MOS commands and OWD_TRG are runtime-only"
#endif
#if (AFE3520_CFG_OV_MV > 5115U) || (AFE3520_CFG_UV_MV > 5115U) ||     (AFE3520_CFG_OV_MV <= AFE3520_CFG_UV_MV) ||     (AFE3520_CFG_OV_MV % 5U) || (AFE3520_CFG_UV_MV % 5U) ||     (AFE3520_CFG_OV_DELAY_CODE > 7U) || (AFE3520_CFG_UV_DELAY_CODE > 7U)
#error "Invalid SH3673520 OV/UV threshold or delay"
#endif

/* 0x40..0x54，顺序与物理地址一致；0x43 在构建时替换为 SeriesNum。 */
#define AFE3520_CONFIG_VALUES { \
    0x00U, /* 40 SCONF1: NORMAL */ \
    AFE3520_CFG_SCONF2 | 0x80U, /* 41: LTCLR */ \
    AFE3520_CFG_SCONF3, /* 42 */ \
    0U, /* 43: SeriesNum is filled at runtime */ \
    AFE3520_CFG_SCONF5 | (AFE3520_CFG_WDT_ENABLE << 2), /* 44 */ \
    AFE3520_CFG_SCONF6, /* 45 */ \
    AFE3520_CFG_SCONF7, /* 46 */ \
    AFE3520_CFG_OWV_ALARMH, /* 47 */ \
    AFE3520_CFG_ALARML, /* 48 */ \
    (AFE3520_CFG_OV_DELAY_CODE << 4) | ((AFE3520_CFG_OV_MV / 5U) >> 8), /* 49 */ \
    (AFE3520_CFG_OV_MV / 5U) & 0xFFU, /* 4A */ \
    (AFE3520_CFG_UV_DELAY_CODE << 4) | ((AFE3520_CFG_UV_MV / 5U) >> 8), /* 4B */ \
    (AFE3520_CFG_UV_MV / 5U) & 0xFFU, /* 4C */ \
    AFE3520_CFG_OCD1, /* 4D */ \
    AFE3520_CFG_OCD2, /* 4E */ \
    AFE3520_CFG_SC, /* 4F */ \
    AFE3520_CFG_OCC, /* 50 */ \
    AFE3520_CFG_OTC, /* 51 */ \
    AFE3520_CFG_OTD, /* 52 */ \
    AFE3520_CFG_UTC, /* 53 */ \
    AFE3520_CFG_UTD /* 54 */ \
}

#endif

#ifndef RTC_SLEEP_PORT_H
#define RTC_SLEEP_PORT_H

#include <stdint.h>
#include "rtc_sleep.h"

uint8_t RtcSleep_PortIsOneSecondTick(void);
uint16_t RtcSleep_PortGetIdleDelayTargetSeconds(void);

uint16_t RtcSleep_PortGetCellMinMv(void);
uint16_t RtcSleep_PortGetCellMaxMv(void);
uint16_t RtcSleep_PortGetChargeCurrentMa(void);
uint16_t RtcSleep_PortGetDischargeCurrentMa(void);
uint16_t RtcSleep_PortGetLowVoltageSleepMv(void);
uint16_t RtcSleep_PortGetLowVoltageSleepMinutes(void);

// uint8_t RtcSleep_PortIsMcuWakeActive(void);
uint8_t RtcSleep_PortIsChargerInputActive(void);
uint8_t RtcSleep_PortIsHeatActive(void);
uint8_t RtcSleep_PortIsFactoryAgingActive(void);
uint8_t RtcSleep_PortGetExternalCommCounter(void);
uint8_t RtcSleep_PortIsAfeSleepBlocked(void);

uint8_t RtcSleep_PortUpdateRtcData(void);
uint8_t RtcSleep_PortHasCurrentWake(enum irqWakeup *source);
uint8_t RtcSleep_PortHasAfeWake(enum irqWakeup *source);
uint8_t RtcSleep_PortIsEmergencyWakeVoltage(void);

void RtcSleep_PortRequestSleepLog(void);
void RtcSleep_PortClearLegacySleepRequest(void);
void RtcSleep_PortSelectLegacyResetSleep(uint8_t sleep_mode);
void RtcSleep_PortCommitResetSleep(uint8_t sleep_mode);
void RtcSleep_PortOnDeepSleepRequest(void);

void RtcSleep_PortPrepareRtcStop(uint32_t rtc_cycle_count);
void RtcSleep_PortEnterStop(void);
void RtcSleep_PortDisableStopWakeup(void);
void RtcSleep_PortRestoreAfterStop(void);
uint8_t RtcSleep_PortIsRtcWake(void);
void RtcSleep_PortClearRtcWake(void);
uint32_t RtcSleep_PortGetLastWakeupSeconds(void);

uint32_t RtcSleep_PortGetCanRtcPeriodSeconds(void);
uint8_t RtcSleep_PortIsCanBusActive(void);
void RtcSleep_PortRunCanRtcWakeService(uint32_t rtc_elapsed_seconds);

uint8_t RtcSleep_PortApplySocRtcRest(uint32_t rest_seconds);
void RtcSleep_PortAddRuntimeSeconds(uint32_t seconds);
enum irqWakeup RtcSleep_PortGuessWakeupSource(void);
void RtcSleep_PortOnWakeupSource(enum irqWakeup source);

void cpu_frequency_conf(void);

#endif

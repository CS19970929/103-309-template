#ifndef RTC_SLEEP_PORT_H
#define RTC_SLEEP_PORT_H

#include <stdint.h>
#include "rtc_sleep.h"

uint8_t RtcSleep_PortIsOneSecondTick(void);

uint16_t RtcSleep_PortGetCellMinMv(void);
uint16_t RtcSleep_PortGetChargeCurrentMa(void);
uint16_t RtcSleep_PortGetDischargeCurrentMa(void);
uint16_t RtcSleep_PortGetLowVoltageSleepMv(void);

uint8_t RtcSleep_PortIsMcuWakeActive(void);
uint8_t RtcSleep_PortGetExternalCommCounter(void);

uint8_t RtcSleep_PortUpdateRtcData(void);
uint8_t RtcSleep_PortHasCurrentWake(enum irqWakeup *source);
uint8_t RtcSleep_PortHasAfeWake(enum irqWakeup *source);
uint8_t RtcSleep_PortIsEmergencyWakeVoltage(void);

void RtcSleep_PortCommitResetSleep(uint8_t sleep_mode);

void RtcSleep_PortPrepareRtcStop(void);
void RtcSleep_PortEnterStop(void);
void RtcSleep_PortDisableStopWakeup(void);
void RtcSleep_PortRestoreAfterStop(void);
uint8_t RtcSleep_PortIsRtcWake(void);
uint32_t RtcSleep_PortGetLastWakeupSeconds(void);

void RtcSleep_PortApplySocRtcRest(uint32_t rest_seconds);
void RtcSleep_PortAddRuntimeSeconds(uint32_t seconds);
enum irqWakeup RtcSleep_PortGuessWakeupSource(void);
void RtcSleep_PortOnWakeupSource(enum irqWakeup source);

void cpu_frequency_conf(void);

#endif

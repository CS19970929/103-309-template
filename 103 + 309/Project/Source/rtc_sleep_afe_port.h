#ifndef RTC_SLEEP_AFE_PORT_H
#define RTC_SLEEP_AFE_PORT_H

#include <stdint.h>
#include "low_power.h"

uint8_t RtcSleep_AfePortIsSleepBlocked(void);
uint8_t RtcSleep_AfePortUpdateRtcData(void);
uint8_t RtcSleep_AfePortHasCurrentWake(enum irqWakeup *source);
uint8_t RtcSleep_AfePortHasAfeWake(enum irqWakeup *source);

#endif

#ifndef RTC_H
#define RTC_H

#include "conf.h"

#define RTC_BKP_DATA        0xA5A5
#define LSE_FREQUENT        32767
#define TIME_ZOOM           (8 * 60 * 60)
#define UNIX_TIME_YEAR      1970
#define SEC_DAY             86400L

#define Leapyear(year)      ((year) % 4 == 0)
#define Days_in_year(a)     (Leapyear(a) ? 366 : 365)

struct RTC_ELEMENT {
	UINT16 RTC_Time_Year;
	UINT16 RTC_Time_Month;
	UINT16 RTC_Time_Day;
	UINT16 RTC_Time_Hour;
	UINT16 RTC_Time_Minute;
	UINT16 RTC_Time_Second;
	UINT16 RTC_Alarm_Year;
	UINT16 RTC_Alarm_Month;
	UINT16 RTC_Alarm_Day;
	UINT16 RTC_Alarm_Hour;
	UINT16 RTC_Alarm_Minute;
	UINT16 RTC_Alarm_Second;
};

#define RTC_element_min     {0, 1, 1, 0, \
								0, 0, 0, 0, \
								0, 0, 0, 0}

#define RTC_element_default {18, 10, 31, 23, \
								59, 30, 0, 0, \
								0, 9, 1, 4}

#define RTC_element_max     {99, 12, 31, 23, \
								59, 59, 99, 12, \
								31, 23, 59, 59}

extern struct RTC_ELEMENT RTC_time;

void Init_RTC(void);
void App_RTC(void);

void RTC_SetWakeupPeriodSeconds(UINT32 seconds);
UINT32 RTC_GetWakeupPeriodSeconds(void);
UINT32 RTC_GetLastWakeupPeriodSeconds(void);
void RTC_WKTimeConfig(void);
void RTC_DisableStopWakeup(void);
void RTC_SyncAfterStop(void);
void RTC_RestoreRunInterrupts(void);
UINT8 RTC_IsStopWakeup(void);
void RTC_ClearStopWakeup(void);

#endif /* RTC_H */

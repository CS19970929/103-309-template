#ifndef RTC_H
#define RTC_H

#include "conf.h"


#define RTC_BKP_DATA        0xA5A5
#define LSE_START_TIMEOUT   ((uint16_t)0x5000) /*!< Time out for LSE start up */
#define LSE_FREQUENT		32767
#define TIME_ZOOM			(8*60*60)			//�����ʱ���ı���ʱ��

#define	Leapyear(year)		((year) % 4 == 0)	//�������ᳬ��2100��û����


struct RTC_ELEMENT {
	UINT16  RTC_Time_Year;
	UINT16  RTC_Time_Month;
	UINT16  RTC_Time_Day;
	UINT16  RTC_Time_Hour;
	
	UINT16  RTC_Time_Minute;
	UINT16  RTC_Time_Second;
	UINT16  RTC_Alarm_Year;
	UINT16  RTC_Alarm_Month;
	
	UINT16  RTC_Alarm_Day;
	UINT16  RTC_Alarm_Hour;
	UINT16  RTC_Alarm_Minute;
	UINT16  RTC_Alarm_Second;
};

extern struct RTC_ELEMENT RTC_time;
//extern UINT8 RTC_Faultcnt;

UINT32 RTC_GetWakeupPeriodSeconds(void);
UINT32 RTC_GetLastWakeupPeriodSeconds(void);
UINT8 RTC_IsStopWakeup(void);
void RTC_ClearStopWakeup(void);
UINT8 RTC_WKTimeConfig(void);
void RTC_DisableStopWakeup(void);
void RTC_RestoreRunInterrupts(void);
UINT8 Init_RTC(void);

#endif	/* RTC_H */


#ifndef RTC_H
#define RTC_H

#include "conf.h"


#define RTC_BKP_DATA        0xA5A5
#define LSE_START_TIMEOUT   ((uint16_t)0x5000) /*!< Time out for LSE start up */
#define LSE_FREQUENT		32767
#define TIME_ZOOM			(8*60*60)			//�����ʱ���ı���ʱ��
#define	UNIX_TIME_YEAR		1970				//��Ԫʱ��
#define SEC_DAY				86400L          	//һ���ж���s 

#define ALARM_TIME_SEC		(UINT32)10       //s�����߻���ʱ�� 

#define	Leapyear(year)		((year) % 4 == 0)	//�������ᳬ��2100��û����
#define	Days_in_year(a) 	(Leapyear(a) ? 366 : 365)

/*
struct RTC_TIME {
	UINT16 RTC_year;		//Ϊʲô���Ϊ16λ����Ϊ��������Ҫ16λ�����ϴ�����Ҫ
	UINT8 RTC_mon;
	UINT8 RTC_day;
	UINT8 RTC_hour;
	UINT8 RTC_min;
	UINT8 RTC_sec;
};
*/

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

#define RTC_element_min     {0, 1, 1, 0,\
								0, 0, 0, 0,\
								0, 0, 0, 0}

#define RTC_element_default {18, 10, 31, 23,\
								59, 30, 0, 0,\
								0, 9, 1, 4}

#define RTC_element_max     {99, 12, 31, 23,\
								59, 59, 99, 12,\
								31, 23, 59, 59}

extern struct RTC_ELEMENT RTC_time;
//extern UINT8 RTC_Faultcnt;
extern volatile bool is_rtc_wakekup;


UINT32 RTC_GetWakeupPeriodSeconds(void);
UINT32 RTC_GetLastWakeupPeriodSeconds(void);
void RTC_WKTimeConfig(void);
void Init_RTC(void);
void App_RTC(void);

#endif	/* RTC_H */


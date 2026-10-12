#include "main.h"

// UINT8 RTC_Faultcnt = 0;
__IO UINT8 TimeDisplay = 0; // 秒中断标志，进入秒中断时置1，当时间被刷新之后清0

struct RTC_ELEMENT RTC_time;

struct RTC_ELEMENT Systmtime = {2018, 12, 31, 23, 59, 30};

UINT8 month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
#define Days_in_month(a) (month_days[(a)-1])

#ifdef _WEEK
void GregorianDay(struct RTC_ELEMENT *tm)
{
	int leapsToDate;
	int lastYear;
	int day;
	int MonthOffset[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

	lastYear = tm->RTC_Time_Year - 1;
	leapsToDate = lastYear / 4 - lastYear / 100 + lastYear / 400; // 计算从公元元年到计数的前一年之中一共经历了多少个闰年

	if ((tm->RTC_Time_Year % 4 == 0) // 如若计数的这一年为闰年，且计数的月份在2月之后，则日数加1，否则不加1
		&& ((tm->RTC_Time_Year % 100 != 0) || (tm->RTC_Time_Year % 400 == 0)) && (tm->RTC_Time_Month > 2))
	{
		day = 1;
	}
	else
	{
		day = 0;
	}

	day += lastYear * 365 + leapsToDate + MonthOffset[tm->RTC_Time_Month - 1] + tm->RTC_Time_Day; // 计算从公元元年元旦到计数日期一共有多少天

	tm->tm_wday = day % 7;
}

#endif

// 此函数为——Linux源码中的mktime算法
UINT32 mktimev(struct RTC_ELEMENT *tm)
{
	if (0 >= (int)(tm->RTC_Time_Month -= 2))
	{							  /* 1..12 -> 11,12,1..10 */
		tm->RTC_Time_Month += 12; /* Puts Feb last since it has leap day */
		tm->RTC_Time_Year -= 1;
	}

	return (((
				 (UINT32)(tm->RTC_Time_Year / 4 - tm->RTC_Time_Year / 100 + tm->RTC_Time_Year / 400 + 367 * tm->RTC_Time_Month / 12 + tm->RTC_Time_Day) + tm->RTC_Time_Year * 365 - 719499) *
				 24 +
			 tm->RTC_Time_Hour /* now have hours */
			 ) * 60 +
			tm->RTC_Time_Minute /* now have minutes */
			) * 60 +
		   tm->RTC_Time_Second; /* finally seconds */
}

void Second_To_RTCtime(UINT32 AllSecond, struct RTC_ELEMENT *RTCtime)
{

	UINT32 i;
	UINT32 Second_res, Day;

	Day = AllSecond / SEC_DAY;		  /* 有多少天 */
	Second_res = AllSecond % SEC_DAY; /* 今天的时间，单位s */

	/* Hours, minutes, seconds are easy */
	RTCtime->RTC_Time_Hour = Second_res / 3600;
	RTCtime->RTC_Time_Minute = (Second_res % 3600) / 60;
	RTCtime->RTC_Time_Second = (Second_res % 3600) % 60;

	for (i = UNIX_TIME_YEAR; Day >= Days_in_year(i); i++)
	{ // 算出当前年份，起始的计数年份为1970年
		Day -= Days_in_year(i);
	}
	i %= 100; // 只保留后两位
	RTCtime->RTC_Time_Year = (UINT8)i;

	if (Leapyear(RTCtime->RTC_Time_Year))
	{ // 计算当前的月份
		Days_in_month(2) = 29;
	}
	for (i = 1; Day >= Days_in_month(i); i++)
	{
		Day -= Days_in_month(i);
	}
	Days_in_month(2) = 28;
	RTCtime->RTC_Time_Month = (UINT8)i;

	RTCtime->RTC_Time_Day = (UINT8)Day + 1; // 计算当前日期

	// GregorianDay(tm);						//计算星期几
}

void Get_RTC_Time(void)
{
	UINT32 u32BJ_SecondTimeVar;

	u32BJ_SecondTimeVar = RTC_GetCounter() + TIME_ZOOM;
	Second_To_RTCtime(u32BJ_SecondTimeVar, &RTC_time); // 把定时器的值转换为北京时间
}

// 此函数为——Linux源码中的mktime算法修改为——易读懂型
// 1970年1月1日0时为UNIX TIME的纪元时间
UINT32 Seccond_Cal(struct RTC_ELEMENT *RTC_t)
{
	UINT32 Y_day, M_day, D_day;
	UINT32 X_day, T_sec;

	if (0 >= (int)(RTC_t->RTC_Time_Month -= 2))
	{ // 前两个月被推到上一年去
		RTC_t->RTC_Time_Month += 12;
		RTC_t->RTC_Time_Year -= 1;
	}
	Y_day = (RTC_t->RTC_Time_Year - 1) * 365 + RTC_t->RTC_Time_Year / 4 - RTC_t->RTC_Time_Year / 100 + RTC_t->RTC_Time_Year / 400;
	M_day = 367 * RTC_t->RTC_Time_Month / 12 - 30 + 59;
	D_day = RTC_t->RTC_Time_Day - 1;
	X_day = (UINT32)(Y_day + M_day + D_day) - 719162; // 719162为0年0月0日0时到纪元时间的天数
	T_sec = ((X_day * 24 + RTC_t->RTC_Time_Hour) * 60 + RTC_t->RTC_Time_Minute) * 60 + RTC_t->RTC_Time_Second;

	return T_sec;
}

/* Poll budgets bound a missing oscillator or stalled backup-domain write. */
#define RTC_WAIT_POLL_LIMIT 1000000UL
static UINT8 s_rtc_ready;
volatile bool is_rtc_wakekup;
volatile uint16_t rtc_cnt;

static UINT8 RTC_WaitFlag(UINT16 flag)
{
    UINT32 polls = RTC_WAIT_POLL_LIMIT;
    while (RTC_GetFlagStatus(flag) == RESET)
    {
        if (--polls == 0U)
            return 0U;
    }
    return 1U;
}

static UINT8 RTC_Synchronize(void)
{
    RTC->CRL &= (UINT16)~RTC_FLAG_RSF;
    return RTC_WaitFlag(RTC_FLAG_RSF);
}

static UINT8 RTC_StartOscillator(UINT8 lse)
{
    UINT32 polls = RTC_WAIT_POLL_LIMIT;
    UINT8 flag = lse ? RCC_FLAG_LSERDY : RCC_FLAG_LSIRDY;
    if (lse)
        RCC_LSEConfig(RCC_LSE_ON);
    else
        RCC_LSICmd(ENABLE);
    while (RCC_GetFlagStatus(flag) == RESET)
    {
        if (--polls == 0U)
            return 0U;
    }
    return 1U;
}

static void RTC_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static void RTC_ConfigureInterrupts(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;
    exti.EXTI_Line = EXTI_Line17;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);
    nvic.NVIC_IRQChannel = RTCAlarm_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0U;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = RTC_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1U;
    NVIC_Init(&nvic);
}

UINT8 Init_RTC(void)
{
    UINT32 source;
    struct RTC_ELEMENT initial_time;
    UINT8 retained;
    RTC_EnableBackupAccess();
    s_rtc_ready = 0U;
    source = RCC->BDCR & RCC_BDCR_RTCSEL;
    retained = (UINT8)(BKP_ReadBackupRegister(BKP_DR1) == RTC_BKP_DATA &&
                      (RCC->BDCR & RCC_BDCR_RTCEN) != 0U &&
                      (source == RCC_BDCR_RTCSEL_LSE || source == RCC_BDCR_RTCSEL_LSI));
    if (!retained)
    {
        /* Only initialize an absent/invalid RTC; ordinary wake keeps its time. */
        BKP_DeInit();
        if (RTC_StartOscillator(1U))
            RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
        else if (RTC_StartOscillator(0U))
            RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
        else
            return 0U;
        RCC_RTCCLKCmd(ENABLE);
    }
    else if (!RTC_StartOscillator((UINT8)(source == RCC_BDCR_RTCSEL_LSE)))
        return 0U;
    if (!RTC_Synchronize() || !RTC_WaitFlag(RTC_FLAG_RTOFF))
        return 0U;
    if (!retained)
    {
        source = RCC->BDCR & RCC_BDCR_RTCSEL;
        RTC_SetPrescaler(source == RCC_BDCR_RTCSEL_LSE ? LSE_FREQUENT : 39999U);
        if (!RTC_WaitFlag(RTC_FLAG_RTOFF))
            return 0U;
        initial_time = Systmtime;
        RTC_SetCounter(Seccond_Cal(&initial_time) - TIME_ZOOM);
        if (!RTC_WaitFlag(RTC_FLAG_RTOFF))
            return 0U;
        BKP_WriteBackupRegister(BKP_DR1, RTC_BKP_DATA);
    }
    RTC_ITConfig(RTC_IT_ALR | RTC_IT_SEC, DISABLE);
    if (!RTC_WaitFlag(RTC_FLAG_RTOFF))
        return 0U;
    RTC_ClearITPendingBit(RTC_IT_ALR | RTC_IT_SEC);
    if (!RTC_WaitFlag(RTC_FLAG_RTOFF))
        return 0U;
    EXTI_ClearITPendingBit(EXTI_Line17);
    NVIC_ClearPendingIRQ(RTCAlarm_IRQn);
    NVIC_ClearPendingIRQ(RTC_IRQn);
    RTC_ConfigureInterrupts();
    s_rtc_ready = 1U;
    return 1U;
}

UINT8 RTC_WKTimeConfig(void)
{
    if (!s_rtc_ready || !RTC_WaitFlag(RTC_FLAG_RTOFF))
        return 0U;
    RTC_ITConfig(RTC_IT_SEC | RTC_IT_ALR, DISABLE);
    if (!RTC_WaitFlag(RTC_FLAG_RTOFF))
        return 0U;
    RTC_ClearITPendingBit(RTC_IT_ALR | RTC_IT_SEC);
    if (!RTC_WaitFlag(RTC_FLAG_RTOFF))
        return 0U;
    EXTI_ClearITPendingBit(EXTI_Line17);
    NVIC_ClearPendingIRQ(RTCAlarm_IRQn);
    NVIC_ClearPendingIRQ(RTC_IRQn);
    RTC_SetAlarm(RTC_GetCounter() + ALARM_TIME_SEC);
    if (!RTC_WaitFlag(RTC_FLAG_RTOFF))
        return 0U;
    RTC_ITConfig(RTC_IT_ALR, ENABLE);
    return RTC_WaitFlag(RTC_FLAG_RTOFF);
}

void RTC_DisableStopAlarm(void)
{
    if (!s_rtc_ready)
        return;
    RTC_ITConfig(RTC_IT_ALR, DISABLE);
    (void)RTC_WaitFlag(RTC_FLAG_RTOFF);
    RTC_ClearITPendingBit(RTC_IT_ALR);
    (void)RTC_WaitFlag(RTC_FLAG_RTOFF);
    EXTI_ClearITPendingBit(EXTI_Line17);
    NVIC_ClearPendingIRQ(RTCAlarm_IRQn);
    is_rtc_wakekup = false;
}

void App_RTC(void)
{
    if (TimeDisplay)
    {
        Get_RTC_Time();
        TimeDisplay = 0U;
    }
}

static void RTC_RecordAlarmWake(void)
{
    if (RTC_GetITStatus(RTC_IT_ALR) != RESET ||
        EXTI_GetITStatus(EXTI_Line17) != RESET)
    {
        RTC_ClearITPendingBit(RTC_IT_ALR);
        EXTI_ClearITPendingBit(EXTI_Line17);
        is_rtc_wakekup = true;
        ++rtc_cnt;
    }
}

void RTCAlarm_IRQHandler(void)
{
    RTC_RecordAlarmWake();
}

void RTC_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_SEC) != RESET)
    {
        RTC_ClearITPendingBit(RTC_IT_SEC);
        TimeDisplay = 1U;
    }
    RTC_RecordAlarmWake();
}

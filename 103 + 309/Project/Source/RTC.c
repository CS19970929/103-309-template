#include "main.h"
#include "IrqDebug.h"
#include "DebugWatch.h"

typedef struct RTC_RUNTIME_TAG
{
	__IO UINT8 disp;
	volatile bool wake;
	struct RTC_ELEMENT time;
} RTC_RUNTIME;

static RTC_RUNTIME s_rtc = {
	0U,
	false,
	{0}
};
struct RTC_ELEMENT RTC_time;

static const UINT8 month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#if DEBUG_WATCH_ENABLED
void RTC_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->runtime.rtc = &s_rtc;
	watch->public_data.rtc_time = &RTC_time;
	watch->tables.rtc_month_days = month_days;
	watch->tables.rtc_month_days_count = (uint16_t)(sizeof(month_days) / sizeof(month_days[0]));
}
#endif

#define RTC_CLOCK_OK             0U
#define RTC_CLOCK_USE_LSI        1U
#define RTC_CLOCK_NEED_REINIT    2U
#define RTC_CLOCK_INIT_FAILED    3U
#define RTC_WAIT_TIMEOUT         ((UINT32)0x00FFFFFFU)
#define RTC_WAKEUP_DEFAULT_SECONDS ((UINT32)10U)

static UINT8 RTC_GetMonthDays(UINT32 month, UINT8 is_leap_year)
{
	if ((month == 2U) && (is_leap_year != 0U))
	{
		return 29U;
	}

	return month_days[month - 1U];
}

static void RTC_EnableBackupAccess(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
}

static UINT8 RTC_WaitForLastTaskSafe(void)
{
	UINT32 timeout = RTC_WAIT_TIMEOUT;

	while ((RTC->CRL & RTC_FLAG_RTOFF) == (uint16_t)RESET)
	{
		if (timeout-- == 0U)
		{
			return 0U;
		}
	}

	return 1U;
}

static UINT8 RTC_WaitForSynchroSafe(void)
{
	UINT32 timeout = RTC_WAIT_TIMEOUT;

	RTC->CRL &= (uint16_t)~RTC_FLAG_RSF;
	while ((RTC->CRL & RTC_FLAG_RSF) == (uint16_t)RESET)
	{
		if (timeout-- == 0U)
		{
			return 0U;
		}
	}

	return 1U;
}

static UINT8 RTC_EnableLsiOscillator(void)
{
	UINT32 timeout = RTC_WAIT_TIMEOUT;

	RCC_LSICmd(ENABLE);
	while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
	{
		if (timeout-- == 0U)
		{
			return 0U;
		}
	}

	return 1U;
}

static UINT8 RTC_PrepareExistingClock(void)
{
	uint32_t bdcr = RCC->BDCR;
	uint32_t rtcsel = bdcr & RCC_BDCR_RTCSEL;

	if ((bdcr & RCC_BDCR_RTCEN) == 0U)
	{
		return 0U;
	}

	if (rtcsel == RCC_BDCR_RTCSEL_LSE)
	{
		return ((bdcr & RCC_BDCR_LSERDY) != 0U) ? 1U : 0U;
	}

	if (rtcsel == RCC_BDCR_RTCSEL_LSI)
	{
		return RTC_EnableLsiOscillator();
	}

	return 0U;
}

static UINT8 RTC_EnableLsiClock(void)
{
	if (!RTC_EnableLsiOscillator())
	{
		return 0U;
	}

	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
	RCC_RTCCLKCmd(ENABLE);
	if (!RTC_WaitForSynchroSafe())
	{
		return 0U;
	}
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}
	RTC_ITConfig(RTC_IT_SEC, ENABLE);
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}
	RTC_SetPrescaler(40000 - 1);
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}

	return 1U;
}

static UINT8 RTC_ReinitWithLsiClock(void)
{
	BKP_DeInit();
	PWR_BackupAccessCmd(ENABLE);

	if (!RTC_EnableLsiClock())
	{
		return RTC_CLOCK_INIT_FAILED;
	}

	return RTC_CLOCK_USE_LSI;
}

static UINT8 RTC_NormalizeYear(UINT16 input_year, UINT16 *full_year)
{
	if (full_year == 0)
	{
		return 0U;
	}

	if (input_year <= 99U)
	{
		*full_year = (UINT16)(2000U + input_year);
		return 1U;
	}

	if ((input_year >= 2000U) && (input_year <= 2099U))
	{
		*full_year = input_year;
		return 1U;
	}

	return 0U;
}

UINT8 RTC_IsCalendarTimeValid(const struct RTC_ELEMENT *time)
{
	UINT16 full_year;
	UINT8 is_leap_year;
	UINT8 month_days_this_month;

	if (time == 0)
	{
		return 0U;
	}
	if (RTC_NormalizeYear(time->RTC_Time_Year, &full_year) == 0U)
	{
		return 0U;
	}
	if ((time->RTC_Time_Month < 1U) || (time->RTC_Time_Month > 12U))
	{
		return 0U;
	}
	if ((time->RTC_Time_Hour > 23U) ||
		(time->RTC_Time_Minute > 59U) ||
		(time->RTC_Time_Second > 59U))
	{
		return 0U;
	}

	is_leap_year = (UINT8)Leapyear(full_year);
	month_days_this_month = RTC_GetMonthDays(time->RTC_Time_Month, is_leap_year);
	if ((time->RTC_Time_Day < 1U) ||
		(time->RTC_Time_Day > month_days_this_month))
	{
		return 0U;
	}

	return 1U;
}

UINT8 RTC_CalendarToEpochSeconds(const struct RTC_ELEMENT *time, UINT32 *epoch_seconds)
{
	UINT16 full_year;
	UINT32 calc_year;
	UINT32 calc_month;
	UINT32 y_day;
	UINT32 m_day;
	UINT32 d_day;
	UINT32 x_day;

	if ((time == 0) || (epoch_seconds == 0))
	{
		return 0U;
	}
	if (RTC_IsCalendarTimeValid(time) == 0U)
	{
		return 0U;
	}
	if (RTC_NormalizeYear(time->RTC_Time_Year, &full_year) == 0U)
	{
		return 0U;
	}

	calc_year = full_year;
	calc_month = time->RTC_Time_Month;
	if (calc_month <= 2U)
	{
		calc_month += 12U;
		calc_year -= 1U;
	}

	y_day = (calc_year - 1U) * 365U +
		calc_year / 4U - calc_year / 100U + calc_year / 400U;
	m_day = 367U * calc_month / 12U - 30U + 59U;
	d_day = time->RTC_Time_Day - 1U;
	x_day = (y_day + m_day + d_day) - 719162U;
	*epoch_seconds = ((x_day * 24U + time->RTC_Time_Hour) * 60U +
					  time->RTC_Time_Minute) * 60U +
		time->RTC_Time_Second;
	return 1U;
}

void RTC_EpochSecondsToCalendar(UINT32 epoch_seconds, struct RTC_ELEMENT *time)
{
	UINT32 i;
	UINT32 second_res;
	UINT32 day;
	UINT8 is_leap_year;

	if (time == 0)
	{
		return;
	}

	memset(time, 0, sizeof(*time));
	day = epoch_seconds / SEC_DAY;
	second_res = epoch_seconds % SEC_DAY;

	time->RTC_Time_Hour = (UINT16)(second_res / 3600U);
	time->RTC_Time_Minute = (UINT16)((second_res % 3600U) / 60U);
	time->RTC_Time_Second = (UINT16)(second_res % 60U);

	for (i = UNIX_TIME_YEAR; day >= Days_in_year(i); i++)
	{
		day -= Days_in_year(i);
	}
	time->RTC_Time_Year = (UINT16)(i % 100U);

	is_leap_year = (UINT8)Leapyear(i);
	for (i = 1U; day >= RTC_GetMonthDays(i, is_leap_year); i++)
	{
		day -= RTC_GetMonthDays(i, is_leap_year);
	}
	time->RTC_Time_Month = (UINT16)i;
	time->RTC_Time_Day = (UINT16)(day + 1U);
}

void Second_To_RTCtime(UINT32 AllSecond, struct RTC_ELEMENT *RTCtime)
{
	RTC_EpochSecondsToCalendar(AllSecond, RTCtime);
	return;
#if 0

	UINT32 i;
	UINT32 Second_res, Day;
	UINT8 is_leap_year;

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

	is_leap_year = Leapyear(RTCtime->RTC_Time_Year);
	for (i = 1; Day >= RTC_GetMonthDays(i, is_leap_year); i++)
	{
		Day -= RTC_GetMonthDays(i, is_leap_year);
	}
	RTCtime->RTC_Time_Month = (UINT8)i;

	RTCtime->RTC_Time_Day = (UINT8)Day + 1; // 计算当前日期

	// GregorianDay(tm);						//计算星期几
#endif
}

void Get_RTC_Time(void)
{
	UINT32 u32BJ_SecondTimeVar;

	u32BJ_SecondTimeVar = RTC_GetCounter() + TIME_ZOOM;
	Second_To_RTCtime(u32BJ_SecondTimeVar, &s_rtc.time); // 把定时器的值转换为北京时间
	RTC_time = s_rtc.time;
}

// 此函数为——Linux源码中的mktime算法修改为——易读懂型
// 1970年1月1日0时为UNIX TIME的纪元时间
UINT32 Seccond_Cal(struct RTC_ELEMENT *RTC_t)
{
	UINT32 epoch_seconds;

	if (RTC_CalendarToEpochSeconds(RTC_t, &epoch_seconds) != 0U)
	{
		return epoch_seconds;
	}

	return 0U;

#if 0
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
#endif
}

UINT8 RTC_GetCalendarTime(struct RTC_ELEMENT *time)
{
	if (time == 0)
	{
		return 0U;
	}

	Get_RTC_Time();
	*time = RTC_time;
	return RTC_IsCalendarTimeValid(time);
}

UINT8 RTC_SetCounterFromCalendar(const struct RTC_ELEMENT *time)
{
	UINT32 epoch_seconds;

	if (RTC_CalendarToEpochSeconds(time, &epoch_seconds) == 0U)
	{
		return 0U;
	}
	if (epoch_seconds < TIME_ZOOM)
	{
		return 0U;
	}

	RTC_EnableBackupAccess();
	RTC_SetCounter(epoch_seconds - TIME_ZOOM);
	if (RTC_WaitForLastTaskSafe() == 0U)
	{
		return 0U;
	}

	Get_RTC_Time();
	return 1U;
}

static UINT8 RTC_ClockConfig(UINT8 need_full_init)
{
	__IO UINT16 StartUpCounter = 0, HSEStatus = 0;
	UINT8 result = RTC_CLOCK_OK;
	PWR_BackupAccessCmd(ENABLE); // 允许访问RTC

	if (!need_full_init)
	{
		if (!RTC_PrepareExistingClock())
		{
			return RTC_CLOCK_NEED_REINIT;
		}
		RCC_RTCCLKCmd(ENABLE);
		if (!RTC_WaitForSynchroSafe())
		{
			return RTC_CLOCK_NEED_REINIT;
		}
		if (!RTC_WaitForLastTaskSafe())
		{
			return RTC_CLOCK_NEED_REINIT;
		}
		RTC_ITConfig(RTC_IT_SEC, ENABLE);
		if (!RTC_WaitForLastTaskSafe())
		{
			return RTC_CLOCK_NEED_REINIT;
		}
		return RTC_CLOCK_OK;
	}

	BKP_DeInit();				 // 仅首次初始化 RTC 时重置备份域
	RCC_LSEConfig(RCC_LSE_ON);	 // 使能外部LSE晶振，RCC_LSE_Bypass旁路的意思应该是使能这个LSE时钟，但是单片机不用，外围电路用?
	do
	{
		HSEStatus = RCC_GetFlagStatus(RCC_FLAG_LSERDY);
		StartUpCounter++;
	} while ((HSEStatus == RESET) && (StartUpCounter < LSE_START_TIMEOUT)); // 等待到 LSE 预备

	if (StartUpCounter < LSE_START_TIMEOUT)
	{
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE); // 把RTC 时钟源配置为LSE
		RCC_RTCCLKCmd(ENABLE);					// 使能RTC时钟
		if (!RTC_WaitForSynchroSafe())			// 等待 RTC APB 寄存器同步
		{
			return RTC_ReinitWithLsiClock();
		}
		if (!RTC_WaitForLastTaskSafe())			// 确保上一次 RTC 的操作完成
		{
			return RTC_ReinitWithLsiClock();
		}
		RTC_ITConfig(RTC_IT_SEC, ENABLE);		// 使能 RTC 秒中断
		if (!RTC_WaitForLastTaskSafe())			// 确保上一次 RTC 的操作完成
		{
			return RTC_ReinitWithLsiClock();
		}
		RTC_SetPrescaler(LSE_FREQUENT);			// 设置 RTC 分频: 使 RTC 周期为1s
										// RTC period = RTCCLK/RTC_PR = (32.768 KHz)/(32767+1) = 1HZ
		if (!RTC_WaitForLastTaskSafe()) // 确保上一次 RTC 的操作完成
		{
			return RTC_ReinitWithLsiClock();
		}
	}
	else
	{
		//++RTC_Faultcnt;											//RTC错误单数为LSE出错
		// System_ERROR_UserCallback(ERROR_LSE);
		if (!RTC_EnableLsiClock())
		{
			return RTC_CLOCK_INIT_FAILED;
		}
		result = RTC_CLOCK_USE_LSI;
	}

	return result;
}

static void RTC_ClearAlarmPending(void)
{
	RTC_ClearITPendingBit(RTC_IT_ALR);
	RTC_ClearFlag(RTC_FLAG_ALR);
	RTC_WaitForLastTaskSafe();
	EXTI_ClearITPendingBit(EXTI_Line17);
	NVIC_ClearPendingIRQ(RTCAlarm_IRQn);
}

static void RTC_DisableSecondInterrupt(void)
{
	RTC_ITConfig(RTC_IT_SEC, DISABLE);
	RTC_WaitForLastTaskSafe();
	RTC_ClearITPendingBit(RTC_IT_SEC);
	RTC_WaitForLastTaskSafe();
}

static void RTC_DisableAlarmInterrupt(void)
{
	RTC_ITConfig(RTC_IT_ALR, DISABLE);
	//todo ??
	RTC_WaitForLastTaskSafe();
	RTC_ClearAlarmPending();
}

static void RTC_EnableAlarmAfterSeconds(UINT32 wake_seconds)
{
	if (wake_seconds == 0U)
	{
		wake_seconds = 1U;
	}
	RTC_ClearAlarmPending();
	RTC_SetAlarm(RTC_GetCounter() + wake_seconds);
	RTC_WaitForLastTaskSafe();
	RTC_ITConfig(RTC_IT_ALR, ENABLE);
	RTC_WaitForLastTaskSafe();
	NVIC_ClearPendingIRQ(RTCAlarm_IRQn);
}

void RTC_TimeConfig(void)
{
	struct RTC_ELEMENT Systmtime = {2018, 12, 31, 23, 59, 30, 0, 0, 0, 0, 0, 0};

	// GregorianDay(tm);			//计算星期
	RTC_SetCounter(Seccond_Cal(&Systmtime) - TIME_ZOOM); // 由日期计算时间戳并写入到RTC计数寄存器
	RTC_WaitForLastTaskSafe();
}

void RTC_AlarmConfig(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;


	//------------EXTI17 配置 -------------------
	EXTI_InitStructure.EXTI_Line = EXTI_Line17;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);
	//------------设置 中断-------------------
	NVIC_InitStructure.NVIC_IRQChannel = RTCAlarm_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void RTC_NVIC_Config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

	NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

UINT32 RTC_GetWakeupPeriodSeconds(void)
{
	if(g_stLowPowerRtcStatus.mode == NORMAL_MODE)
	{
		return 20;
	}
	return RTC_WAKEUP_DEFAULT_SECONDS;
}

UINT32 RTC_GetLastWakeupPeriodSeconds(void)
{
	return RTC_WAKEUP_DEFAULT_SECONDS;
}

void RTC_WKTimeConfig(void)
{
	UINT32 wake_seconds;

	RTC_EnableBackupAccess();
	RTC_DisableSecondInterrupt();
	RTC_DisableAlarmInterrupt();
	wake_seconds = RTC_GetWakeupPeriodSeconds();
	RTC_EnableAlarmAfterSeconds(wake_seconds);
}

void RTC_DisableStopWakeup(void)
{
	RTC_EnableBackupAccess();
	RTC_DisableAlarmInterrupt();
}

void RTC_RestoreRunInterrupts(void)
{
	RTC_EnableBackupAccess();
	RTC_DisableAlarmInterrupt();
	RTC_ClearITPendingBit(RTC_IT_SEC);
	RTC_WaitForLastTaskSafe();
	NVIC_ClearPendingIRQ(RTC_IRQn);
	RTC_ITConfig(RTC_IT_SEC, ENABLE);
	RTC_WaitForLastTaskSafe();
}

void Init_RTC(void)
{ // 使能PWR外设时钟，待机模式，RTC，看门狗
	UINT8 need_full_init;
	UINT8 clock_status;

	RTC_EnableBackupAccess();

	need_full_init = (BKP_ReadBackupRegister(BKP_DR1) != RTC_BKP_DATA) ? 1U : 0U;
	clock_status = RTC_ClockConfig(need_full_init); // RTC时钟配置

	if ((!need_full_init) && (clock_status == RTC_CLOCK_NEED_REINIT))
	{
		need_full_init = 1U;
		clock_status = RTC_ClockConfig(need_full_init);
	}

	if (clock_status == RTC_CLOCK_INIT_FAILED)
	{
		return;
	}

	if (need_full_init)
	{ // 读取备份里面的值是否被写过。
		RTC_TimeConfig();
		BKP_WriteBackupRegister(BKP_DR1, RTC_BKP_DATA);
	}
	else
	{ // 以下这段话需要吗？
		if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
		{ // 这是啥
			//("\r\n Power On Reset occurred....\n\r");
			//++RTC_Faultcnt;
		}
		else if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
		{ // 这是啥
			// printf("\r\n External Reset occurred....\n\r");
		}
		RCC_ClearFlag();
		// RTC_TimeShow();				//Display the RTC Time and Alarm，这个后面会用到
		// RTC_AlarmShow();
	}
	RTC_ClearAlarmPending();
	RTC_AlarmConfig();
	RTC_NVIC_Config();
}
void App_RTC(void)
{
	if (0 == s_rtc.disp)
	{
		return;
	}
	Get_RTC_Time();
	s_rtc.disp = 0;
}

UINT8 RTC_IsStopWakeup(void)
{
	return s_rtc.wake ? 1U : 0U;
}

void RTC_ClearStopWakeup(void)
{
	s_rtc.wake = false;
}

static void RTC_HandleAlarmWakeup(void)
{
	UINT8 had_alarm = 0;

	if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
	{
		RTC_ClearITPendingBit(RTC_IT_ALR);
		RTC_WaitForLastTaskSafe();
		had_alarm = 1;
	}

	if (EXTI_GetITStatus(EXTI_Line17) != RESET)
	{
		EXTI_ClearITPendingBit(EXTI_Line17);
		had_alarm = 1;
	}

	if (had_alarm)
	{
		sys_time.rtc_alm_cnt++;
		s_rtc.wake = true;
	}
}

void RTCAlarm_IRQHandler(void)
{
	IrqDebug_Count((uint8_t)IRQDBG_RTC_ALARM);
	RTC_HandleAlarmWakeup();
}

void RTC_IRQHandler(void)
{
	if (RTC_GetITStatus(RTC_IT_SEC) != RESET)
	{
		IrqDebug_CountFast((uint8_t)IRQDBG_RTC_SEC);
		RTC_ClearITPendingBit(RTC_IT_SEC); // Clear the RTC Second interrupt
		sys_time.rtc_sec_cnt++;
		s_rtc.disp = 1;				   // Enable time update
		RTC_WaitForLastTaskSafe();		   // Wait until last write operation on RTC registers has finished
	}

	if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
	{
		IrqDebug_Count((uint8_t)IRQDBG_RTC_ALARM_IN_RTC_IRQ);
		RTC_HandleAlarmWakeup();
	}
}


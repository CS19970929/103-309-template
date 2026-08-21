#include "main.h"

typedef struct RTC_RUNTIME_TAG
{
	__IO UINT8 disp;
	volatile bool wake;
	struct RTC_ELEMENT time;
	UINT32 wakeup_seconds;
	UINT32 last_wakeup_seconds;
} RTC_RUNTIME;

static RTC_RUNTIME s_rtc = {
	0U,
	false,
	{0},
	1U,
	0U
};

struct RTC_ELEMENT RTC_time;

static const UINT8 month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define RTC_CLOCK_OK             0U
#define RTC_CLOCK_USE_LSI        1U
#define RTC_CLOCK_NEED_REINIT    2U
#define RTC_CLOCK_INIT_FAILED    3U
#define RTC_WAIT_TIMEOUT         ((UINT32)0x00FFFFFFU)
#define RTC_LSI_NOMINAL_HZ       ((UINT32)40000U)

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

static UINT8 RTC_WaitForLseReady(void)
{
	UINT32 elapsed_ms = 0U;

	RCC_LSEConfig(RCC_LSE_ON);
	while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
	{
		if (elapsed_ms >= (UINT32)PROJECT_CFG_RTC_LSE_STARTUP_TIMEOUT_MS)
		{
			return 0U;
		}
		__delay_ms(1U);
		elapsed_ms++;
	}

	return 1U;
}

static UINT8 RTC_SetSecondInterrupt(FunctionalState state)
{
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}
	RTC_ITConfig(RTC_IT_SEC, state);
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}

	return 1U;
}

static UINT8 RTC_SetPrescalerSafe(UINT32 prescaler)
{
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}
	RTC_SetPrescaler(prescaler);
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}

	return 1U;
}

static UINT8 RTC_StartFreshClock(UINT32 source, UINT32 prescaler)
{
	RCC_RTCCLKConfig(source);
	RCC_RTCCLKCmd(ENABLE);

	if (!RTC_WaitForSynchroSafe())
	{
		return 0U;
	}
	if (!RTC_SetPrescalerSafe(prescaler))
	{
		return 0U;
	}
	if (!RTC_SetSecondInterrupt(ENABLE))
	{
		return 0U;
	}

	return 1U;
}

static UINT8 RTC_StartFreshLsiClock(void)
{
	if (!RTC_EnableLsiOscillator())
	{
		return 0U;
	}

	return RTC_StartFreshClock(RCC_RTCCLKSource_LSI, RTC_LSI_NOMINAL_HZ - 1U);
}

static UINT8 RTC_ReinitWithLsiClock(void)
{
	BKP_DeInit();
	PWR_BackupAccessCmd(ENABLE);
	RCC_LSEConfig(RCC_LSE_OFF);

	if (!RTC_StartFreshLsiClock())
	{
		return RTC_CLOCK_INIT_FAILED;
	}

	return RTC_CLOCK_USE_LSI;
}

static UINT8 RTC_PrepareExistingClock(void)
{
	UINT32 bdcr = RCC->BDCR;
	UINT32 rtcsel = bdcr & RCC_BDCR_RTCSEL;

	if ((bdcr & RCC_BDCR_RTCEN) == 0U)
	{
		return 0U;
	}

	if (rtcsel == RCC_BDCR_RTCSEL_LSE)
	{
		if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
		{
			if (!RTC_WaitForLseReady())
			{
				return 0U;
			}
		}
	}
	else if (rtcsel == RCC_BDCR_RTCSEL_LSI)
	{
		if (!RTC_EnableLsiOscillator())
		{
			return 0U;
		}
	}
	else
	{
		return 0U;
	}

	RCC_RTCCLKCmd(ENABLE);
	if (!RTC_WaitForSynchroSafe())
	{
		return 0U;
	}
	if (!RTC_SetSecondInterrupt(ENABLE))
	{
		return 0U;
	}

	return 1U;
}

void Second_To_RTCtime(UINT32 AllSecond, struct RTC_ELEMENT *RTCtime)
{
	UINT32 i;
	UINT32 Second_res;
	UINT32 Day;
	UINT8 is_leap_year;

	Day = AllSecond / SEC_DAY;
	Second_res = AllSecond % SEC_DAY;

	RTCtime->RTC_Time_Hour = Second_res / 3600U;
	RTCtime->RTC_Time_Minute = (Second_res % 3600U) / 60U;
	RTCtime->RTC_Time_Second = (Second_res % 3600U) % 60U;

	for (i = UNIX_TIME_YEAR; Day >= Days_in_year(i); i++)
	{
		Day -= Days_in_year(i);
	}
	i %= 100U;
	RTCtime->RTC_Time_Year = (UINT8)i;

	is_leap_year = Leapyear(RTCtime->RTC_Time_Year);
	for (i = 1U; Day >= RTC_GetMonthDays(i, is_leap_year); i++)
	{
		Day -= RTC_GetMonthDays(i, is_leap_year);
	}
	RTCtime->RTC_Time_Month = (UINT8)i;
	RTCtime->RTC_Time_Day = (UINT8)Day + 1U;
}

void Get_RTC_Time(void)
{
	UINT32 u32BJ_SecondTimeVar;

	u32BJ_SecondTimeVar = RTC_GetCounter() + TIME_ZOOM;
	Second_To_RTCtime(u32BJ_SecondTimeVar, &s_rtc.time);
	RTC_time = s_rtc.time;
}

UINT32 Seccond_Cal(struct RTC_ELEMENT *RTC_t)
{
	UINT32 Y_day;
	UINT32 M_day;
	UINT32 D_day;
	UINT32 X_day;
	UINT32 T_sec;

	if (0 >= (int)(RTC_t->RTC_Time_Month -= 2U))
	{
		RTC_t->RTC_Time_Month += 12U;
		RTC_t->RTC_Time_Year -= 1U;
	}
	Y_day = (RTC_t->RTC_Time_Year - 1U) * 365U + RTC_t->RTC_Time_Year / 4U - RTC_t->RTC_Time_Year / 100U + RTC_t->RTC_Time_Year / 400U;
	M_day = 367U * RTC_t->RTC_Time_Month / 12U - 30U + 59U;
	D_day = RTC_t->RTC_Time_Day - 1U;
	X_day = (UINT32)(Y_day + M_day + D_day) - 719162U;
	T_sec = ((X_day * 24U + RTC_t->RTC_Time_Hour) * 60U + RTC_t->RTC_Time_Minute) * 60U + RTC_t->RTC_Time_Second;

	return T_sec;
}

static UINT8 RTC_ClockConfig(UINT8 need_full_init)
{
	if (!need_full_init)
	{
		return RTC_PrepareExistingClock() ? RTC_CLOCK_OK : RTC_CLOCK_NEED_REINIT;
	}

	BKP_DeInit();
	PWR_BackupAccessCmd(ENABLE);

	if (RTC_WaitForLseReady())
	{
		if (RTC_StartFreshClock(RCC_RTCCLKSource_LSE, LSE_FREQUENT))
		{
			return RTC_CLOCK_OK;
		}
		return RTC_ReinitWithLsiClock();
	}

	RCC_LSEConfig(RCC_LSE_OFF);
	if (!RTC_StartFreshLsiClock())
	{
		return RTC_CLOCK_INIT_FAILED;
	}

	return RTC_CLOCK_USE_LSI;
}

static void RTC_ClearAlarmPending(void)
{
	if (RTC_WaitForLastTaskSafe())
	{
		RTC_ClearITPendingBit(RTC_IT_ALR);
		(void)RTC_WaitForLastTaskSafe();
	}
	EXTI_ClearITPendingBit(EXTI_Line17);
	NVIC_ClearPendingIRQ(RTCAlarm_IRQn);
}

static void RTC_DisableSecondInterrupt(void)
{
	if (RTC_SetSecondInterrupt(DISABLE))
	{
		if (RTC_WaitForLastTaskSafe())
		{
			RTC_ClearITPendingBit(RTC_IT_SEC);
			(void)RTC_WaitForLastTaskSafe();
		}
	}
	NVIC_ClearPendingIRQ(RTC_IRQn);
}

static void RTC_DisableAlarmInterrupt(void)
{
	if (RTC_WaitForLastTaskSafe())
	{
		RTC_ITConfig(RTC_IT_ALR, DISABLE);
		(void)RTC_WaitForLastTaskSafe();
	}
	RTC_ClearAlarmPending();
}

static UINT8 RTC_EnableAlarmAfterSeconds(UINT32 wake_seconds)
{
	UINT32 alarm_value;

	if (wake_seconds == 0U)
	{
		wake_seconds = 1U;
	}

	RTC_ClearAlarmPending();
	alarm_value = RTC_GetCounter() + wake_seconds;
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}
	RTC_SetAlarm(alarm_value);
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}
	RTC_ITConfig(RTC_IT_ALR, ENABLE);
	if (!RTC_WaitForLastTaskSafe())
	{
		return 0U;
	}
	NVIC_ClearPendingIRQ(RTCAlarm_IRQn);

	return 1U;
}

void RTC_TimeConfig(void)
{
	struct RTC_ELEMENT Systmtime = {2018, 12, 31, 23, 59, 30, 0, 0, 0, 0, 0, 0};

	if (!RTC_WaitForLastTaskSafe())
	{
		return;
	}
	RTC_SetCounter(Seccond_Cal(&Systmtime) - TIME_ZOOM);
	(void)RTC_WaitForLastTaskSafe();
}

void RTC_AlarmConfig(void)
{
	NVIC_InitTypeDef nvic;
	EXTI_InitTypeDef exti;

	EXTI_ClearITPendingBit(EXTI_Line17);
	exti.EXTI_Line = EXTI_Line17;
	exti.EXTI_Mode = EXTI_Mode_Interrupt;
	exti.EXTI_Trigger = EXTI_Trigger_Rising;
	exti.EXTI_LineCmd = ENABLE;
	EXTI_Init(&exti);

	NVIC_ClearPendingIRQ(RTCAlarm_IRQn);
	nvic.NVIC_IRQChannel = RTCAlarm_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 0U;
	nvic.NVIC_IRQChannelSubPriority = 0U;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);
}

void RTC_NVIC_Config(void)
{
	NVIC_InitTypeDef nvic;

	nvic.NVIC_IRQChannel = RTC_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 1U;
	nvic.NVIC_IRQChannelSubPriority = 0U;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);
}

void RTC_SetWakeupPeriodSeconds(UINT32 seconds)
{
	s_rtc.wakeup_seconds = (seconds == 0U) ? 1U : seconds;
}

UINT32 RTC_GetWakeupPeriodSeconds(void)
{
	return s_rtc.wakeup_seconds;
}

UINT32 RTC_GetLastWakeupPeriodSeconds(void)
{
	return s_rtc.last_wakeup_seconds;
}

void RTC_SyncAfterStop(void)
{
	RTC_EnableBackupAccess();
	(void)RTC_WaitForSynchroSafe();
}

void RTC_WKTimeConfig(void)
{
	RTC_EnableBackupAccess();
	RTC_SyncAfterStop();
	RTC_DisableSecondInterrupt();
	RTC_DisableAlarmInterrupt();
	RTC_AlarmConfig();

	if (RTC_EnableAlarmAfterSeconds(s_rtc.wakeup_seconds))
	{
		s_rtc.last_wakeup_seconds = s_rtc.wakeup_seconds;
	}
	else
	{
		s_rtc.last_wakeup_seconds = 0U;
	}
}

void RTC_DisableStopWakeup(void)
{
	RTC_EnableBackupAccess();
	RTC_DisableAlarmInterrupt();
}

void RTC_RestoreRunInterrupts(void)
{
	RTC_EnableBackupAccess();
	RTC_SyncAfterStop();
	RTC_DisableAlarmInterrupt();

	if (RTC_WaitForLastTaskSafe())
	{
		RTC_ClearITPendingBit(RTC_IT_SEC);
		(void)RTC_WaitForLastTaskSafe();
	}
	NVIC_ClearPendingIRQ(RTC_IRQn);
	(void)RTC_SetSecondInterrupt(ENABLE);
}

void Init_RTC(void)
{
	UINT8 need_full_init;
	UINT8 clock_status;

	RTC_EnableBackupAccess();
	need_full_init = (BKP_ReadBackupRegister(BKP_DR1) != RTC_BKP_DATA) ? 1U : 0U;
	clock_status = RTC_ClockConfig(need_full_init);

	if ((!need_full_init) && (clock_status == RTC_CLOCK_NEED_REINIT))
	{
		need_full_init = 1U;
		clock_status = RTC_ClockConfig(1U);
	}

	if (clock_status == RTC_CLOCK_INIT_FAILED)
	{
		return;
	}

	if (need_full_init)
	{
		RTC_TimeConfig();
		BKP_WriteBackupRegister(BKP_DR1, RTC_BKP_DATA);
	}

	RTC_ClearAlarmPending();
	RTC_AlarmConfig();
	RTC_NVIC_Config();
}

void App_RTC(void)
{
	if (s_rtc.disp == 0U)
	{
		return;
	}
	Get_RTC_Time();
	s_rtc.disp = 0U;
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
	UINT8 had_alarm = 0U;

	if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
	{
		if (RTC_WaitForLastTaskSafe())
		{
			RTC_ClearITPendingBit(RTC_IT_ALR);
			(void)RTC_WaitForLastTaskSafe();
		}
		had_alarm = 1U;
	}

	if (EXTI_GetITStatus(EXTI_Line17) != RESET)
	{
		EXTI_ClearITPendingBit(EXTI_Line17);
		had_alarm = 1U;
	}

	if (had_alarm != 0U)
	{
		sys_time.rtc_alm_cnt++;
		s_rtc.wake = true;
	}
}

void RTCAlarm_IRQHandler(void)
{
	RTC_HandleAlarmWakeup();
}

void RTC_IRQHandler(void)
{
	if (RTC_GetITStatus(RTC_IT_SEC) != RESET)
	{
		if (RTC_WaitForLastTaskSafe())
		{
			RTC_ClearITPendingBit(RTC_IT_SEC);
			(void)RTC_WaitForLastTaskSafe();
		}
		s_rtc.disp = 1U;
	}

	if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
	{
		RTC_HandleAlarmWakeup();
	}
}

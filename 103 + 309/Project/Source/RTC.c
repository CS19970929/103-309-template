#include "main.h"

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

#define RTC_CLOCK_OK                    0U
#define RTC_CLOCK_USE_LSI               1U
#define RTC_CLOCK_NEED_REINIT           2U
#define RTC_CLOCK_INIT_FAILED           3U
#define RTC_WAIT_TIMEOUT                ((UINT32)0x00FFFFFFU)
#define RTC_WAKEUP_DEFAULT_SECONDS      ((UINT32)10U)
#define RTC_WAKEUP_NORMAL_SECONDS       ((UINT32)20U)

static UINT32 s_u32LastWakeupPeriodSeconds = RTC_WAKEUP_DEFAULT_SECONDS;

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

void Second_To_RTCtime(UINT32 AllSecond, struct RTC_ELEMENT *RTCtime)
{
	UINT32 i;
	UINT32 Second_res, Day;
	UINT8 is_leap_year;

	Day = AllSecond / SEC_DAY;
	Second_res = AllSecond % SEC_DAY;

	RTCtime->RTC_Time_Hour = Second_res / 3600;
	RTCtime->RTC_Time_Minute = (Second_res % 3600) / 60;
	RTCtime->RTC_Time_Second = (Second_res % 3600) % 60;

	for (i = UNIX_TIME_YEAR; Day >= Days_in_year(i); i++)
	{
		Day -= Days_in_year(i);
	}
	i %= 100;
	RTCtime->RTC_Time_Year = (UINT8)i;

	is_leap_year = Leapyear(RTCtime->RTC_Time_Year);
	for (i = 1; Day >= RTC_GetMonthDays(i, is_leap_year); i++)
	{
		Day -= RTC_GetMonthDays(i, is_leap_year);
	}
	RTCtime->RTC_Time_Month = (UINT8)i;
	RTCtime->RTC_Time_Day = (UINT8)Day + 1;
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
	UINT32 Y_day, M_day, D_day;
	UINT32 X_day, T_sec;

	if (0 >= (int)(RTC_t->RTC_Time_Month -= 2))
	{
		RTC_t->RTC_Time_Month += 12;
		RTC_t->RTC_Time_Year -= 1;
	}
	Y_day = (RTC_t->RTC_Time_Year - 1) * 365 + RTC_t->RTC_Time_Year / 4 - RTC_t->RTC_Time_Year / 100 + RTC_t->RTC_Time_Year / 400;
	M_day = 367 * RTC_t->RTC_Time_Month / 12 - 30 + 59;
	D_day = RTC_t->RTC_Time_Day - 1;
	X_day = (UINT32)(Y_day + M_day + D_day) - 719162;
	T_sec = ((X_day * 24 + RTC_t->RTC_Time_Hour) * 60 + RTC_t->RTC_Time_Minute) * 60 + RTC_t->RTC_Time_Second;

	return T_sec;
}

static UINT8 RTC_ClockConfig(UINT8 need_full_init)
{
	__IO UINT16 StartUpCounter = 0, HSEStatus = 0;
	UINT8 result = RTC_CLOCK_OK;
	PWR_BackupAccessCmd(ENABLE);

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

	BKP_DeInit();
	RCC_LSEConfig(RCC_LSE_ON);
	do
	{
		HSEStatus = RCC_GetFlagStatus(RCC_FLAG_LSERDY);
		StartUpCounter++;
	} while ((HSEStatus == RESET) && (StartUpCounter < LSE_START_TIMEOUT));

	if (StartUpCounter < LSE_START_TIMEOUT)
	{
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
		RCC_RTCCLKCmd(ENABLE);
		if (!RTC_WaitForSynchroSafe())
		{
			return RTC_ReinitWithLsiClock();
		}
		if (!RTC_WaitForLastTaskSafe())
		{
			return RTC_ReinitWithLsiClock();
		}
		RTC_ITConfig(RTC_IT_SEC, ENABLE);
		if (!RTC_WaitForLastTaskSafe())
		{
			return RTC_ReinitWithLsiClock();
		}
		RTC_SetPrescaler(LSE_FREQUENT);
		if (!RTC_WaitForLastTaskSafe())
		{
			return RTC_ReinitWithLsiClock();
		}
	}
	else
	{
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
	NVIC_ClearPendingIRQ(RTC_IRQn);
}

static void RTC_DisableAlarmInterrupt(void)
{
	RTC_ITConfig(RTC_IT_ALR, DISABLE);
	RTC_WaitForLastTaskSafe();
	RTC_ClearAlarmPending();
}

static void RTC_EnableAlarmAfterSeconds(UINT32 wake_seconds)
{
	if (wake_seconds == 0U)
	{
		wake_seconds = 1U;
	}

	s_u32LastWakeupPeriodSeconds = wake_seconds;
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

	RTC_SetCounter(Seccond_Cal(&Systmtime) - TIME_ZOOM);
	RTC_WaitForLastTaskSafe();
}

void RTC_AlarmConfig(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;

	EXTI_InitStructure.EXTI_Line = EXTI_Line17;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

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
	if (g_stLowPowerRtcStatus.mode == NORMAL_MODE)
	{
		return RTC_WAKEUP_NORMAL_SECONDS;
	}

	return RTC_WAKEUP_DEFAULT_SECONDS;
}

UINT32 RTC_GetLastWakeupPeriodSeconds(void)
{
	return s_u32LastWakeupPeriodSeconds;
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
{
	UINT8 need_full_init;
	UINT8 clock_status;

	RTC_EnableBackupAccess();

	need_full_init = (BKP_ReadBackupRegister(BKP_DR1) != RTC_BKP_DATA) ? 1U : 0U;
	clock_status = RTC_ClockConfig(need_full_init);

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
	{
		RTC_TimeConfig();
		BKP_WriteBackupRegister(BKP_DR1, RTC_BKP_DATA);
	}
	else
	{
		if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
		{
		}
		else if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
		{
		}
		RCC_ClearFlag();
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
		g_irq_t = rtc_alarm_irq;
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
		RTC_ClearITPendingBit(RTC_IT_SEC);
		s_rtc.disp = 1;
		RTC_WaitForLastTaskSafe();
	}

	if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
	{
		RTC_HandleAlarmWakeup();
	}
}

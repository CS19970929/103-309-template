#include "main.h"
#include "LowPowerSleep.h"

typedef struct SLEEP_RUNTIME_TAG
{
	volatile UINT8 ext_comm;
	UINT8 boot_sleep;
	UINT8 chg_wake;
	UINT8 key_state;
	UINT32 key_press_tick;
	UINT8 undervoltage_handled;
	UINT8 normal_idle_tracking;
	UINT32 normal_idle_since;
} SLEEP_RUNTIME;

static SLEEP_RUNTIME s_sleep;

enum { KEY_READY, KEY_TIMING, KEY_WAIT_RELEASE };
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_MAINTAINED
#define KEY_SLEEP_TICKS 5U
#define KEY_WAKE_STEPS 5U
#else
#define KEY_SLEEP_TICKS 300U
#define KEY_WAKE_STEPS 100U
#endif

static void SleepDeal_MarkBootFromSleepChargerWakeup(void);
static void SleepDeal_WaitStopWakeup(void);

static UINT8 SleepDeal_IsChargerWakeupActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_SET);
}

static UINT8 SleepDeal_IsKeyPressed(void)
{
	return (UINT8)(MCUI_ENI_DI1 == 0);
}

UINT8 SleepDeal_IsWakeupValid(void)
{
	UINT8 held;
	if (SleepDeal_IsChargerWakeupActive())
	{
		SleepDeal_MarkBootFromSleepChargerWakeup();
		SleepDeal_ResetKeyState(0U);
		return 1;
	}
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_PUSHBUTTON
	/* NORMAL accepts a pressed-switch IRQ without a hold check. Neither
	 * releasing after the sleep hold nor an old low level on RTC wakes RUN. */
	if (g_stLowPowerRtcStatus.mode==NORMAL_MODE)
		return (UINT8)(g_irq_t==soc_key && SleepDeal_IsKeyPressed());
#endif
	if (!SleepDeal_IsKeyPressed()) { s_sleep.key_state=KEY_READY; return 0U; }
	if (s_sleep.key_state==KEY_WAIT_RELEASE) return 0U;
	/* TIM3 is stopped here. Use bounded input qualification after the STOP
	 * clock restore, while ADC and normal business remain paused. */
	for (held=0U; held<KEY_WAKE_STEPS; ++held) {
		__delay_ms(10U);
		Feed_IWatchDog;
		if (SleepDeal_IsChargerWakeupActive()) {
			SleepDeal_MarkBootFromSleepChargerWakeup();
			SleepDeal_ResetKeyState(0U);
			return 1U;
		}
		if (!SleepDeal_IsKeyPressed()) { s_sleep.key_state=KEY_READY; return 0U; }
	}
	s_sleep.key_state=KEY_WAIT_RELEASE;
	return 1U;
}

void SleepDeal_ResetKeyState(UINT8 normal_key_wake)
{
	s_sleep.key_state=SleepDeal_IsKeyPressed() ? KEY_WAIT_RELEASE : KEY_READY;
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_PUSHBUTTON
	if (normal_key_wake && s_sleep.key_state==KEY_WAIT_RELEASE) {
		/* Start only after RUN restores its timer; InitTimer resets the tick. */
		s_sleep.key_press_tick=SysTime_Get10msTickCount();
		s_sleep.key_state=KEY_TIMING;
	}
#else
	(void)normal_key_wake;
#endif
}

UINT8 SleepDeal_TakeKeySleepRequest(void)
{
	UINT32 now=SysTime_Get10msTickCount();
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_MAINTAINED
	/* Charger wake is still allowed with an open switch. Re-arm the open
	 * request while charging, so unplugging returns to NORMAL after debounce. */
	if (SleepDeal_IsKeyPressed() || SleepDeal_IsChargerWakeupActive()) {
		s_sleep.key_state=KEY_READY;
		s_sleep.normal_idle_tracking=0U;
		return 0U;
	}
#else
	if (!SleepDeal_IsKeyPressed()) { s_sleep.key_state=KEY_READY; return 0U; }
#endif
	s_sleep.normal_idle_tracking=0U;
	if (s_sleep.key_state==KEY_WAIT_RELEASE) return 0U;
	if (s_sleep.key_state==KEY_READY) {
		s_sleep.key_press_tick=now;
		s_sleep.key_state=KEY_TIMING;
		return 0U;
	}
	if ((UINT32)(now-s_sleep.key_press_tick)<KEY_SLEEP_TICKS) return 0U;
	s_sleep.key_state=KEY_WAIT_RELEASE;
	return 1U;
}

void SleepDeal_Continue(UINT8 sleep_mode)
{
	UINT16 boot_flag = s_sleep.undervoltage_handled ? FLASH_UNDERVOLTAGE_SLEEP_VALUE : FLASH_DEEP_SLEEP_VALUE;

	LowPowerSleep_SaveResetState();
	BootFlag_Write(boot_flag);
	InitAFE1_Sleep(0);
	AFE_Sleep();
	MCU_RESET();
}

void SleepDeal_UpdateOverdischarge(UINT8 active)
{
	/* Call only after a successful AFE status read. Recovery re-arms the edge;
	 * a wake while UV remains active is allowed to display zero and recharge. */
	if (!active) { s_sleep.undervoltage_handled=0U; return; }
	if (s_sleep.undervoltage_handled) return;
	s_sleep.undervoltage_handled=1U;
	SOC_RequestSetOnce(0U);
	LowPower_Request(DEEP_MODE);
}

UINT8 SleepDeal_HasHandledOverdischarge(void)
{
	return s_sleep.undervoltage_handled;
}

UINT8 SleepDeal_NormalIdleExpired(UINT8 activity)
{
	UINT32 now;
	if (activity || OtherElement.u16Sleep_TimeNormal==0U) {
		s_sleep.normal_idle_tracking=0U;
		return 0U;
	}
	/* RTC continues through STOP. The 10 ms RUN tick is restarted on wake and
	 * cannot measure a multi-day idle interval. TimeNormal remains minutes. */
	now=RTC_GetCounter();
	if (!s_sleep.normal_idle_tracking) {
		s_sleep.normal_idle_since=now;
		s_sleep.normal_idle_tracking=1U;
		return 0U;
	}
	/* Backward RTC correction is not a multi-day idle interval. */
	if ((INT32)(now-s_sleep.normal_idle_since)<0) {
		s_sleep.normal_idle_since=now;
		return 0U;
	}
	if ((UINT32)(now-s_sleep.normal_idle_since)<(UINT32)OtherElement.u16Sleep_TimeNormal*60U)
		return 0U;
	s_sleep.normal_idle_tracking=0U;
	return 1U;
}

static void BootFlag_EnableAccess(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
}

#define SLEEP_BKP_FLAG_REG BKP_DR2
#define SLEEP_BKP_INV_REG BKP_DR3

void BootFlag_Write(UINT16 flag)
{
	BootFlag_EnableAccess();
	BKP_WriteBackupRegister(SLEEP_BKP_FLAG_REG, flag);
	BKP_WriteBackupRegister(SLEEP_BKP_INV_REG, (UINT16)(~flag));
}

static void SleepDeal_MarkBootFromSleepChargerWakeup(void)
{
	s_sleep.chg_wake = 1U;
	BootFlag_Write(FLASH_SLEEP_CHARGER_WAKE_VALUE);
}

void SleepDeal_RecordExternalComm(void)
{
	s_sleep.ext_comm++;
	/* Reset directly: 256 valid frames between policy polls must not hide
	 * activity when the existing small communication counter wraps. */
	s_sleep.normal_idle_tracking=0U;
}

UINT8 SleepDeal_GetExternalCommCounter(void)
{
	return s_sleep.ext_comm;
}

UINT16 BootFlag_Read(void)
{
	UINT16 flag;
	UINT16 inverse_flag;

	BootFlag_EnableAccess();
	flag = BKP_ReadBackupRegister(SLEEP_BKP_FLAG_REG);
	inverse_flag = BKP_ReadBackupRegister(SLEEP_BKP_INV_REG);
	if ((UINT16)(flag ^ inverse_flag) != 0xFFFF)
	{
		return BOOT_FLAG_RESET_VALUE;
	}

	switch (flag)
	{
	case FLASH_HICCUP_SLEEP_VALUE:
	case FLASH_NORMAL_SLEEP_VALUE:
	case FLASH_DEEP_SLEEP_VALUE:
	case FLASH_UNDERVOLTAGE_SLEEP_VALUE:
	case FLASH_SLEEP_CHARGER_WAKE_VALUE:
	case FLASH_SLEEP_RESET_VALUE:
		return flag;
	default:
		return BOOT_FLAG_RESET_VALUE;
	}
}

void BootFlag_Clear(void)
{
	BootFlag_Write(BOOT_FLAG_RESET_VALUE);
}

UINT8 SleepDeal_IsBootFromSleepStartup(void)
{
	return s_sleep.boot_sleep;
}

UINT8 SleepDeal_IsBootFromSleepChargerWakeup(void)
{
	if ((s_sleep.chg_wake == 0U) &&
		(BootFlag_Read() == FLASH_SLEEP_CHARGER_WAKE_VALUE))
	{
		s_sleep.chg_wake = 1U;
	}

	return s_sleep.chg_wake;
}

static void SleepDeal_WaitStopWakeup(void)
{
	do
	{
		Feed_IWatchDog;
		Sys_StopMode();
		Feed_IWatchDog;
	} while (!SleepDeal_IsWakeupValid());
}

void SleepDeal_HandleBootSleepStartup(void)
{
	UINT16 sleep_flag;

	sleep_flag = BootFlag_Read();
	s_sleep.boot_sleep = 0U;
	s_sleep.chg_wake = 0U;
	s_sleep.undervoltage_handled=(UINT8)(sleep_flag==FLASH_UNDERVOLTAGE_SLEEP_VALUE);
	switch (sleep_flag)
	{
	case FLASH_HICCUP_SLEEP_VALUE:
		break;
	case FLASH_NORMAL_SLEEP_VALUE:
		break;
	case FLASH_DEEP_SLEEP_VALUE:
	case FLASH_UNDERVOLTAGE_SLEEP_VALUE:
		s_sleep.boot_sleep = 1U;
		IOstatus_DeepMode();
		InitWakeUp_DeepMode();
		SleepDeal_ResetKeyState(0U);
		// Sys_StandbyMode();		//??????IO???
		SleepDeal_WaitStopWakeup();
		/* Keep the sleep reason across any reset while still waiting. */
		if (!s_sleep.chg_wake) BootFlag_Clear();
		break;
	case FLASH_SLEEP_CHARGER_WAKE_VALUE:
		s_sleep.boot_sleep = 1U;
		s_sleep.chg_wake = 1U;
		break;
	case FLASH_SLEEP_RESET_VALUE:
		// ????
		break;
	default:
		BootFlag_Clear();
		break;
	}
}

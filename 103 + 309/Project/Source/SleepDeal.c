#include "main.h"
#include "DebugWatch.h"
#include "LowPowerSleep.h"
#include "IrqDebug.h"

typedef struct SLEEP_RUNTIME_TAG
{
	UINT8 ext_comm;
	UINT8 boot_sleep;
	UINT8 chg_wake;
	UINT8 reserved;
} SLEEP_RUNTIME;

static SLEEP_RUNTIME s_sleep;

#if DEBUG_WATCH_ENABLED
void SleepDeal_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->sleep = &s_sleep;
}
#endif

static void SleepDeal_MarkBootFromSleepChargerWakeup(void);
static void SleepDeal_WaitStopWakeup(void);

#define DI1_LONG_PRESS_WAKE_10MS ((UINT16)50) // PC13����3��պϲ���Ϊ��Ч

static UINT8 SleepDeal_IsChargerWakeupActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET);
}

static UINT8 SleepDeal_IsKeyPressed(void)
{
	// PC13���رպ�Ϊ�͵�ƽ����EXTI13�½��ػ��ѱ���һ�£�
	return (UINT8)(MCUI_ENI_DI1 == 0);
}

static UINT8 SleepDeal_IsWakeupValid(void)
{
	UINT16 hold_cnt = 0;
	UINT16 display_cnt = 0;

	if (SleepDeal_IsChargerWakeupActive())
	{
		SleepDeal_MarkBootFromSleepChargerWakeup();
		return 1;
	}

	while (1)
	{
		if (!SleepDeal_IsKeyPressed())
		{
			LedBar_PrepareForStop();
			return 0;
		}

		LedBar_ShowSleepSocPreview();
		hold_cnt = 0;
		while (SleepDeal_IsKeyPressed())
		{
			if (SleepDeal_IsChargerWakeupActive())
			{
				SleepDeal_MarkBootFromSleepChargerWakeup();
				return 1;
			}

			__delay_ms(10);
			if (++hold_cnt >= DI1_LONG_PRESS_WAKE_10MS)
			{
				return 1;
			}
		}

		display_cnt = 0;
		while (display_cnt < LEDBAR_SOC_DISPLAY_10MS)
		{
			if (SleepDeal_IsChargerWakeupActive())
			{
				SleepDeal_MarkBootFromSleepChargerWakeup();
				return 1;
			}
			if (SleepDeal_IsKeyPressed())
			{
				break;
			}

			__delay_ms(10);
			display_cnt++;
		}

		if (display_cnt >= LEDBAR_SOC_DISPLAY_10MS)
		{
			LedBar_PrepareForStop();
			return 0;
		}
	}
}

void SleepDeal_Continue(UINT8 sleep_mode)
{
	UINT16 boot_flag;

	switch (sleep_mode)
	{
	case NORMAL_MODE:
		boot_flag = FLASH_NORMAL_SLEEP_VALUE;
		break;
	case HICCUP_MODE:
		boot_flag = FLASH_HICCUP_SLEEP_VALUE;
		break;
	case DEEP_MODE:
		boot_flag = FLASH_DEEP_SLEEP_VALUE;
		break;
	default:
		// 不调整引脚进入休眠，功耗会很大
		return;
	}

	IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_SLEEP_PREPARE);
	LowPowerSleep_SaveResetState();
	BootFlag_Write(boot_flag);
	InitAFE1_Sleep(0);
	AFE_Sleep();
	MCU_RESET();
}

static void BootFlag_EnableAccess(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
}

#define SLEEP_BKP_FLAG_REG BKP_DR2
#define SLEEP_BKP_INV_REG  BKP_DR3

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
		IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_RESET_SLEEP_WAIT);
		IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_STOP_WAIT);
		Sys_StopMode();
		IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_STOP_WAKE_RAW);
	} while (!SleepDeal_IsWakeupValid());
}

void SleepDeal_HandleBootSleepStartup(void)
{
	UINT16 sleep_flag;

	sleep_flag = BootFlag_Read();
	s_sleep.boot_sleep = 0U;
	s_sleep.chg_wake = 0U;
	switch (sleep_flag)
	{
	case FLASH_HICCUP_SLEEP_VALUE:
		s_sleep.boot_sleep = 1U;
		BootFlag_Clear();
		Init_RTC();

		IOstatus_RTCMode();
		InitWakeUp_RTCMode();
		SleepDeal_WaitStopWakeup();
		// Sys_StandbyMode();
		IORecover_RTCMode();
		break;
	case FLASH_NORMAL_SLEEP_VALUE:
		s_sleep.boot_sleep = 1U;
		BootFlag_Clear();
		IOstatus_NormalMode();
		InitWakeUp_NormalMode();
		SleepDeal_WaitStopWakeup();
		IORecover_NormalMode();
		break;
	case FLASH_DEEP_SLEEP_VALUE:
		s_sleep.boot_sleep = 1U;
		BootFlag_Clear();
		IOstatus_DeepMode();
		InitWakeUp_DeepMode();
		// Sys_StandbyMode();		//??????IO???
		SleepDeal_WaitStopWakeup();
		IORecover_DeepMode();
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

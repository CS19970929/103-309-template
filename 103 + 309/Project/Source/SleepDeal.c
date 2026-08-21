#include "main.h"
#include "LowPowerSleep.h"

typedef struct SLEEP_RUNTIME_TAG
{
	UINT8 ext_comm;
	UINT8 boot_sleep;
	UINT8 chg_wake;
	UINT8 reserved;
} SLEEP_RUNTIME;

static SLEEP_RUNTIME s_sleep;

static void SleepDeal_MarkBootFromSleepChargerWakeup(void);
static void SleepDeal_WaitStopWakeup(void);

static UINT8 SleepDeal_IsChargerWakeupActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_SET);
}

static UINT8 SleepDeal_IsKeyPressed(void)
{
#if PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE
	return (UINT8)(MCUI_ENI_DI1 == 0);
#else
	return 0U;
#endif
}

UINT8 SleepDeal_IsWakeupValid(void)
{
	if (SleepDeal_IsChargerWakeupActive())
	{
		SleepDeal_MarkBootFromSleepChargerWakeup();
		return 1U;
	}

	if (SleepDeal_IsKeyPressed())
	{
		return 1U;
	}

	return 0U;
}

void SleepDeal_Continue(UINT8 sleep_mode)
{
	UINT16 boot_flag;

	if (sleep_mode == NORMAL_MODE)
	{
		boot_flag = FLASH_NORMAL_SLEEP_VALUE;
	}
	else
	{
		boot_flag = FLASH_DEEP_SLEEP_VALUE;
	}

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
	if ((UINT16)(flag ^ inverse_flag) != 0xFFFFU)
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
	while (!SleepDeal_IsWakeupValid())
	{
		/* Clear only stale pending state before sleeping. Recheck level-based
		   wake conditions so an already-present charger/key cannot be missed. */
		LowPower_ClearWakeupPending();
		if (SleepDeal_IsWakeupValid())
		{
			break;
		}
		Sys_StopMode();
	}
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
		break;

	case FLASH_NORMAL_SLEEP_VALUE:
		break;

	case FLASH_DEEP_SLEEP_VALUE:
		s_sleep.boot_sleep = 1U;
		BootFlag_Clear();
		IOstatus_DeepMode();
		InitWakeUp_DeepMode();
		SleepDeal_WaitStopWakeup();
		break;

	case FLASH_SLEEP_CHARGER_WAKE_VALUE:
		s_sleep.boot_sleep = 1U;
		s_sleep.chg_wake = 1U;
		break;

	case FLASH_SLEEP_RESET_VALUE:
		break;

	default:
		BootFlag_Clear();
		break;
	}
}

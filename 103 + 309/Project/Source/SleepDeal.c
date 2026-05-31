#include "main.h"

static UINT8 s_u8BootFromSleepStartup = 0U;
static UINT8 s_u8BootFromSleepChargerWakeup = 0U;
static void SleepDeal_MarkBootFromSleepChargerWakeup(void);

#define DI1_LONG_PRESS_WAKE_10MS ((UINT16)50) // PC13����3��պϲ���Ϊ��Ч

static UINT8 IsChargerWakeupActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET);
}

static UINT8 IsKeyPressed(void)
{
	// PC13���رպ�Ϊ�͵�ƽ����EXTI13�½��ػ��ѱ���һ�£�
	return (UINT8)(MCUI_ENI_DI1 == 0);
}

static UINT8 IsSleepWakeupValid(void)
{
	UINT16 hold_cnt = 0;
	UINT16 display_cnt = 0;

	if (IsChargerWakeupActive())
	{
		SleepDeal_MarkBootFromSleepChargerWakeup();
		return 1;
	}

	while (1)
	{
		if (!IsKeyPressed())
		{
			LedBar_PrepareForStop();
			return 0;
		}

		LedBar_ShowSleepSocPreview();
		hold_cnt = 0;
		while (IsKeyPressed())
		{
			if (IsChargerWakeupActive())
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
			if (IsChargerWakeupActive())
			{
				SleepDeal_MarkBootFromSleepChargerWakeup();
				return 1;
			}
			if (IsKeyPressed())
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
	UINT8 u8FlashWriteOK_flag = 0;

	Can_PrepareSleep();
	SOC_SaveSnapshotBeforeSleep();
	FactoryAging_SaveProgressBeforeSleep();
	LedBar_SaveSleepSoc();

	switch (sleep_mode)
	{
	case NORMAL_MODE:
		BootFlag_Write(FLASH_NORMAL_SLEEP_VALUE);
		u8FlashWriteOK_flag = 1;
		break;
	case HICCUP_MODE:
		BootFlag_Write(FLASH_HICCUP_SLEEP_VALUE);
		u8FlashWriteOK_flag = 1;

		break;
	case DEEP_MODE:
		BootFlag_Write(FLASH_DEEP_SLEEP_VALUE);
		u8FlashWriteOK_flag = 1;
		break;
	default:
		// 不调整引脚进入休眠，功耗会很大
		break;
	}

	if (u8FlashWriteOK_flag)
	{
		InitAFE1_Sleep(0);
		AFE_Sleep();
		MCU_RESET();
	}
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
	s_u8BootFromSleepChargerWakeup = 1U;
	BootFlag_Write(FLASH_SLEEP_CHARGER_WAKE_VALUE);
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
	return s_u8BootFromSleepStartup;
}

UINT8 SleepDeal_IsBootFromSleepChargerWakeup(void)
{
	if ((s_u8BootFromSleepChargerWakeup == 0U) &&
		(BootFlag_Read() == FLASH_SLEEP_CHARGER_WAKE_VALUE))
	{
		s_u8BootFromSleepChargerWakeup = 1U;
	}

	return s_u8BootFromSleepChargerWakeup;
}

void IsSleepStartUp(void)
{
	UINT16 sleep_flag;

	sleep_flag = BootFlag_Read();
	s_u8BootFromSleepStartup = 0U;
	s_u8BootFromSleepChargerWakeup = 0U;
	switch (sleep_flag)
	{
	case FLASH_HICCUP_SLEEP_VALUE:
		s_u8BootFromSleepStartup = 1U;
		BootFlag_Clear();
		Init_RTC();

		IOstatus_RTCMode();
		InitWakeUp_RTCMode();
		do
		{
			Sys_StopMode();
		} while (!IsSleepWakeupValid());
		// Sys_StandbyMode();
		IORecover_RTCMode();
		break;
	case FLASH_NORMAL_SLEEP_VALUE:
		s_u8BootFromSleepStartup = 1U;
		BootFlag_Clear();
		IOstatus_NormalMode();
		InitWakeUp_NormalMode();
		do
		{
			Sys_StopMode();
		} while (!IsSleepWakeupValid());
		IORecover_NormalMode();
		break;
	case FLASH_DEEP_SLEEP_VALUE:
		s_u8BootFromSleepStartup = 1U;
		BootFlag_Clear();
		IOstatus_DeepMode();
		InitWakeUp_DeepMode();
		// Sys_StandbyMode();		//??????IO???
		do
		{
			Sys_StopMode();
		} while (!IsSleepWakeupValid());
		IORecover_DeepMode();
		break;
	case FLASH_SLEEP_CHARGER_WAKE_VALUE:
		s_u8BootFromSleepStartup = 1U;
		s_u8BootFromSleepChargerWakeup = 1U;
		break;
	case FLASH_SLEEP_RESET_VALUE:
		// ????
		break;
	default:
		BootFlag_Clear();
		break;
	}
}

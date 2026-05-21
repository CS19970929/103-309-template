#include "main.h"
#include "LowPowerSleep.h"

UINT8 RTC_ExtComCnt = 0;
static UINT8 s_u8BootFromSleepStartup = 0U;
static UINT8 s_u8BootFromSleepChargerWakeup = 0U;
static void SleepDeal_MarkBootFromSleepChargerWakeup(void);
static void BootFlag_EnableAccess(void);
static UINT16 SleepDeal_DeepSleepSocCrc(UINT32 elapsed_seconds);
static UINT8 SleepDeal_LoadDeepSleepSocSeconds(UINT32 *elapsed_seconds);
static void SleepDeal_WriteDeepSleepSocSeconds(UINT32 elapsed_seconds);
static void SleepDeal_ClearDeepSleepSocSeconds(void);
static void SleepDeal_AddDeepSleepSocSeconds(UINT32 add_seconds);
static void SleepDeal_PrepareDeepSleepRtcWake(void);
static void SleepDeal_RecordDeepSleepRtcWake(void);

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

	LowPowerSleep_SaveResetState();

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

#define DEEP_SLEEP_SOC_BKP_MAGIC     ((UINT16)0xD53CU)
#define DEEP_SLEEP_SOC_BKP_MAGIC_REG BKP_DR13
#define DEEP_SLEEP_SOC_BKP_INV_REG   BKP_DR14
#define DEEP_SLEEP_SOC_BKP_LO_REG    BKP_DR15
#define DEEP_SLEEP_SOC_BKP_HI_REG    BKP_DR16
#define DEEP_SLEEP_SOC_BKP_CRC_REG   BKP_DR17

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

static UINT16 SleepDeal_DeepSleepSocCrc(UINT32 elapsed_seconds)
{
	UINT16 lo = (UINT16)(elapsed_seconds & 0xFFFFU);
	UINT16 hi = (UINT16)((elapsed_seconds >> 16) & 0xFFFFU);

	return (UINT16)(lo ^ hi ^ DEEP_SLEEP_SOC_BKP_MAGIC ^ 0x6C3AU);
}

static UINT8 SleepDeal_LoadDeepSleepSocSeconds(UINT32 *elapsed_seconds)
{
	UINT16 magic;
	UINT16 inverse_magic;
	UINT16 lo;
	UINT16 hi;
	UINT32 seconds;

	if (elapsed_seconds == 0)
	{
		return 0U;
	}

	BootFlag_EnableAccess();
	magic = BKP_ReadBackupRegister(DEEP_SLEEP_SOC_BKP_MAGIC_REG);
	inverse_magic = BKP_ReadBackupRegister(DEEP_SLEEP_SOC_BKP_INV_REG);
	if ((magic != DEEP_SLEEP_SOC_BKP_MAGIC) ||
		((UINT16)(magic ^ inverse_magic) != 0xFFFFU))
	{
		return 0U;
	}

	lo = BKP_ReadBackupRegister(DEEP_SLEEP_SOC_BKP_LO_REG);
	hi = BKP_ReadBackupRegister(DEEP_SLEEP_SOC_BKP_HI_REG);
	seconds = ((UINT32)hi << 16) | (UINT32)lo;
	if (BKP_ReadBackupRegister(DEEP_SLEEP_SOC_BKP_CRC_REG) != SleepDeal_DeepSleepSocCrc(seconds))
	{
		return 0U;
	}

	*elapsed_seconds = seconds;
	return 1U;
}

static void SleepDeal_WriteDeepSleepSocSeconds(UINT32 elapsed_seconds)
{
	BootFlag_EnableAccess();
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_MAGIC_REG, DEEP_SLEEP_SOC_BKP_MAGIC);
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_INV_REG, (UINT16)(~DEEP_SLEEP_SOC_BKP_MAGIC));
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_LO_REG, (UINT16)(elapsed_seconds & 0xFFFFU));
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_HI_REG, (UINT16)((elapsed_seconds >> 16) & 0xFFFFU));
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_CRC_REG, SleepDeal_DeepSleepSocCrc(elapsed_seconds));
}

static void SleepDeal_ClearDeepSleepSocSeconds(void)
{
	BootFlag_EnableAccess();
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_MAGIC_REG, 0U);
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_INV_REG, 0U);
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_LO_REG, 0U);
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_HI_REG, 0U);
	BKP_WriteBackupRegister(DEEP_SLEEP_SOC_BKP_CRC_REG, 0U);
}

static void SleepDeal_AddDeepSleepSocSeconds(UINT32 add_seconds)
{
	UINT32 elapsed_seconds;

	if (add_seconds == 0U)
	{
		return;
	}
	if (SleepDeal_LoadDeepSleepSocSeconds(&elapsed_seconds) == 0U)
	{
		elapsed_seconds = 0U;
	}
	if (elapsed_seconds > (0xFFFFFFFFU - add_seconds))
	{
		elapsed_seconds = 0xFFFFFFFFU;
	}
	else
	{
		elapsed_seconds += add_seconds;
	}
	SleepDeal_WriteDeepSleepSocSeconds(elapsed_seconds);
}

static void SleepDeal_PrepareDeepSleepRtcWake(void)
{
	is_rtc_wakekup = false;
	RTC_WKTimeConfig();
}

static void SleepDeal_RecordDeepSleepRtcWake(void)
{
	UINT32 wake_seconds;

	if (!is_rtc_wakekup)
	{
		return;
	}
	wake_seconds = RTC_GetLastWakeupPeriodSeconds();
	if (wake_seconds == 0U)
	{
		wake_seconds = RTC_GetWakeupPeriodSeconds();
	}
	SleepDeal_AddDeepSleepSocSeconds(wake_seconds);
	is_rtc_wakekup = false;
}

UINT8 SleepDeal_TryApplyDeepSleepSocCalibration(void)
{
	UINT32 elapsed_seconds;

	if (SleepDeal_LoadDeepSleepSocSeconds(&elapsed_seconds) == 0U)
	{
		return 0U;
	}
	if (elapsed_seconds < (UINT32)PROJECT_CFG_SOC_RTC_CALIBRATION_MIN_SECONDS)
	{
		return 0U;
	}
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return 0U;
	}
	if ((g_stCellInfoReport.u16VCellMin == 0U) ||
		(g_stCellInfoReport.u16VCellMax == 0U) ||
		(g_stCellInfoReport.u16VCellMax < g_stCellInfoReport.u16VCellMin))
	{
		return 0U;
	}

	(void)SOC_ApplyDeepSleepRtcCompensation(elapsed_seconds,
		g_stCellInfoReport.u16VCellMin,
		g_stCellInfoReport.u16VCellMax);
	SleepDeal_ClearDeepSleepSocSeconds();
	log_w("deep sleep rtc rest %lu s, vmin %u, soc %u",
		(unsigned long)elapsed_seconds,
		(unsigned int)g_stCellInfoReport.u16VCellMin,
		(unsigned int)SOC_Enhance_Element.u8_SOC);
	return 1U;
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
		Init_RTC();
		IOstatus_DeepMode();
		InitWakeUp_DeepMode();
		// Sys_StandbyMode();		//??????IO???
		do
		{
			SleepDeal_PrepareDeepSleepRtcWake();
			Sys_StopMode();
			SleepDeal_RecordDeepSleepRtcWake();
		} while (!IsSleepWakeupValid());
		RTC_DisableStopWakeup();
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
void IOstatus_TestMode(void)
{
	IOstatus_NormalMode();
}

void InitWakeUp_TestMode(void)
{
	InitWakeUp_NormalMode();
}

void IORecover_TestMode(void)
{
	MCU_RESET();
}

void Sys_SleepOnExitMode(void)
{
	NVIC_SystemLPConfig(NVIC_LP_SLEEPONEXIT, ENABLE); // 库函数版�?，�?�置SLEEP ON EXIT位为1
	// SCB->SCR|=1<<1;//寄存器版�?，�?�置SLEEP ON EXIT位为1
	__ASM volatile("wfi");
}

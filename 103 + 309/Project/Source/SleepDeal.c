#include "main.h"
#include "LowPowerSleep.h"
#include "afe3520/Afe3520Config.h"

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
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU) == Bit_SET);
}

static UINT8 SleepDeal_IsKeyPressed(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_KEY1, PIN_KEY1) == 0);
}

UINT8 SleepDeal_IsWakeupValid(void)
{

	if (SleepDeal_IsChargerWakeupActive())
	{
		SleepDeal_MarkBootFromSleepChargerWakeup();
		return 1;
	}
	if (SleepDeal_IsKeyPressed())
	{
		return 1;
	}

    return 0U;

}

void SleepDeal_Continue(UINT8 sleep_mode)
{
    const BMS3520_PROTECTION_STATUS *status;
    if ((sleep_mode != NORMAL_MODE && sleep_mode != DEEP_MODE) ||
        AFE3520_CFG_WDT_ENABLE || !Afe3520_GetSnapshot()->valid ||
        (Bms3520_GetBlockMask() & (AFE3520_BLOCK_GLOBAL_AFE_COMM | AFE3520_BLOCK_GLOBAL_AFE_CONFIG)))
    {
        LowPower_Request(NO_SLEEP);
        return;
    }
    Bms3520_SetSystemBlock(1U);
    status = Bms3520_GetProtectionStatus();
    if (!status->mosFeedbackValid || status->actualCharge || status->actualDischarge ||
        Afe3520_EnterSleep() != AFE3520_OK)
    {
        Bms3520_HandleCommFault();
        Bms3520_SetSystemBlock(0U);
        LowPower_Request(NO_SLEEP);
        return;
    }
    /* Commit the boot marker only after MOS-off feedback and sleep ACK. */
    LowPowerSleep_SaveResetState();
    BootFlag_Write(sleep_mode == DEEP_MODE ? FLASH_DEEP_SLEEP_VALUE : FLASH_NORMAL_SLEEP_VALUE);
    MCU_RESET();
}

/* No EEPROM/Flash persistence or readiness prerequisite on this last-resort path. */
void SleepDeal_CommitEmergencyBoot(void)
{
    BootFlag_Write(FLASH_EMERGENCY_SLEEP_VALUE);
    MCU_RESET();
}

void SleepDeal_EmergencySleep(void)
{
    GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
    /* Every operation has bounded transport retries. Failure never vetoes sleep. */
    (void)Afe3520_SetMos(0U, 0U, 0U);
    (void)Afe3520_Write(AFE3520_REG_SCONF5, AFE3520_CFG_SCONF5); /* WDT off if reachable. */
    (void)Afe3520_EnterSleep(); /* Includes best-effort balance off. */
    SleepDeal_CommitEmergencyBoot();
}

static void SleepDeal_WaitEmergencyWake(void)
{
    GPIO_InitTypeDef gpio;
    /* Boot reaches here BEFORE RTC, UART/CAN, AFE and application startup. */
    IOstatus_DeepMode();
    InitWakeUp_DeepMode();
#if !AFE3520_CFG_EMERGENCY_CHARGER_WAKE
    NVIC_DisableIRQ(EXTI0_IRQn); /* AFE WDT/held PA0 cannot start a retry/reset loop. */
#endif
    NVIC_DisableIRQ(RTC_IRQn);
    NVIC_DisableIRQ(RTCAlarm_IRQn);
    RTC_ITConfig(RTC_IT_SEC | RTC_IT_ALR | RTC_IT_OW, DISABLE);
    SysTick->CTRL = 0U;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
    gpio.GPIO_Pin = PIN_M_CCC; GPIO_Init(GPIO_M_CCC, &gpio);
    GPIO_ResetBits(GPIO_M_STB, PIN_M_STB);
    gpio.GPIO_Pin = PIN_M_STB; GPIO_Init(GPIO_M_STB, &gpio);
    GPIO_ResetBits(GPIO_AD_EN, PIN_AD_EN);
    gpio.GPIO_Pin = PIN_AD_EN; GPIO_Init(GPIO_AD_EN, &gpio);
    GPIO_ResetBits(GPIO_CMNT_EN, PIN_CMNT_EN);
    gpio.GPIO_Pin = PIN_CMNT_EN; GPIO_Init(GPIO_CMNT_EN, &gpio);
    GPIO_SetBits(GPIO_CS_SPI, PIN_CS_SPI);
    gpio.GPIO_Pin = PIN_CS_SPI; GPIO_Init(GPIO_CS_SPI, &gpio);
    /* Discard only boot-time pending flags, then require a NEW permitted edge. */
    LowPower_ClearWakeupPending();
    g_irq_t = NO_IRQ;
    for (;;)
    {
        __disable_irq();
        if (g_irq_t == soc_key
#if AFE3520_CFG_EMERGENCY_CHARGER_WAKE
            || g_irq_t == PA0_irq
#endif
           )
        {
            __enable_irq();
            break;
        }
        g_irq_t = NO_IRQ;
        /* No level test: a stuck input must not keep the CPU running. */
        PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
        __enable_irq();
    }
    /* Retain the emergency marker across watchdog/brownout resets until this edge. */
    BootFlag_Clear();
    cpu_frequency_conf();
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
    case FLASH_EMERGENCY_SLEEP_VALUE:
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
        g_irq_t = NO_IRQ;
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
    case FLASH_EMERGENCY_SLEEP_VALUE:
        s_sleep.boot_sleep = 1U;
        SleepDeal_WaitEmergencyWake();
        break;
	case FLASH_HICCUP_SLEEP_VALUE:
		break;
	case FLASH_NORMAL_SLEEP_VALUE:
	case FLASH_DEEP_SLEEP_VALUE:
		s_sleep.boot_sleep = 1U;
		BootFlag_Clear();
		IOstatus_DeepMode();
		InitWakeUp_DeepMode();
		// Sys_StandbyMode();		//??????IO???
		SleepDeal_WaitStopWakeup();
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

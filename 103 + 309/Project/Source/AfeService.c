#include "AfeService.h"

#include "BoardControl.h"
#include "DataDeal.h"
#include "FactoryAging.h"
#include "I2C_AFE1.h"
#include "SleepDeal.h"

static UINT8 AfeService_ShouldRunStartupZero(void)
{
	return ((g_u32AfeCurrentSampleSeq == 0U) && (AfeCurrent_IsStartupZeroDone() == 0U)) ? 1U : 0U;
}

static void AfeService_PrepareStartupZero(UINT8 do_startup_zero)
{
	if (do_startup_zero == 0U)
	{
		return;
	}

	AfeCurrent_SetStartupColdBoot((SleepDeal_IsBootFromSleepStartup() != 0U) ? 0U : 1U);
	AfeCurrent_PrepareStartupZero();
}

static void AfeService_ApplyStartupMosState(void)
{
	if (FactoryAging_IsActive() != 0U)
	{
		enter_fac_mode(true);
	}
	else
	{
		open_dsg_close_chg();
	}
}

static void AfeService_FinishStartupZero(UINT8 do_startup_zero)
{
	if (do_startup_zero != 0U)
	{
		AfeCurrent_StartupZeroCal();
	}
	else
	{
		open_ctlc();
	}
}

void AfeService_Init(void)
{
	UINT8 do_startup_zero;

	do_startup_zero = AfeService_ShouldRunStartupZero();
	AfeService_PrepareStartupZero(do_startup_zero);
	InitAFE1();
	AfeService_ApplyStartupMosState();
	AfeService_FinishStartupZero(do_startup_zero);
}

void AfeService_Recover(void)
{
	AfeService_Init();
}

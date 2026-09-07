#include "main.h"
#include "MosStartup.h"

static void MosStartup_WriteMosState(UINT8 charge_on, UINT8 discharge_on)
{
    /* Never send SH367309 CONF bits to SH3673520 SCONF1 (mode command). */
    SH367309_DriverMos_Ctrl(GPIO_CHG, charge_on);
    SH367309_DriverMos_Ctrl(GPIO_DSG, discharge_on);
}

UINT8 IsChargeActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU) == Bit_SET);
}

void MosStartup_OpenChargeCloseDischarge(void)
{
	MosStartup_WriteMosState(1U, 0U);
}

void MosStartup_OpenDischargeCloseCharge(void)
{
	MosStartup_WriteMosState(0U, 1U);
}

void MosStartup_EnterFactoryMode(bool on)
{
	MosStartup_WriteMosState(1U, 1U);
}

void MosStartup_ApplyInitialState(void)
{
	MosStartup_EnterFactoryMode(true);
}

void Mos_OpenAll(void)
{
	MosStartup_WriteMosState(1U, 1U);
}

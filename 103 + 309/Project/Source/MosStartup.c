#include "main.h"
#include "MosStartup.h"

static UINT8 MosStartup_WriteMosState(UINT8 charge_on, UINT8 discharge_on, BitAction mcc_level)
{
	MTP_REG_CONF requested = SH367309_Reg_Store.REG_MTP_CONF;

	(void)mcc_level;
	requested.bits.CADCON = 1U;
	requested.bits.CHGMOS = (charge_on != 0U) ? 1U : 0U;
	requested.bits.DSGMOS = (discharge_on != 0U) ? 1U : 0U;

	if (!MTPWrite(MTP_CONF, 1, &requested.all))
	{
		return 0U;
	}

	SH367309_Reg_Store.REG_MTP_CONF = requested;
	return 1U;
}

UINT8 IsChargeActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_SET);
}

void MosStartup_OpenChargeCloseDischarge(void)
{
	(void)MosStartup_WriteMosState(1U, 0U, Bit_SET);
}

void MosStartup_OpenDischargeCloseCharge(void)
{
	(void)MosStartup_WriteMosState(0U, 1U, Bit_RESET);
}

void MosStartup_EnterFactoryMode(bool on)
{
	(void)on;
	(void)MosStartup_WriteMosState(1U, 1U, Bit_SET);
}

void MosStartup_ApplyInitialState(void)
{
	MosStartup_EnterFactoryMode(true);
}

void Mos_OpenAll(void)
{
	(void)MosStartup_WriteMosState(1U, 1U, Bit_RESET);
}

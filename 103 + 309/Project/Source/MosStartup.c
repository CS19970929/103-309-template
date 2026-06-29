#include "main.h"
#include "FactoryAging.h"
#include "MosStartup.h"

static void MosStartup_WriteMosState(UINT8 charge_on, UINT8 discharge_on, BitAction mcc_level)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1;
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = charge_on;
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = discharge_on;
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	// GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, mcc_level);
}

UINT8 MosStartup_Is5vChargeActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET);
}

void MosStartup_OpenChargeCloseDischarge(void)
{
	MosStartup_WriteMosState(1U, 0U, Bit_SET);
}

void MosStartup_OpenDischargeCloseCharge(void)
{
	MosStartup_WriteMosState(0U, 1U, Bit_RESET);
}

void MosStartup_EnterFactoryMode(bool on)
{
	MosStartup_WriteMosState(1U, 1U, Bit_SET);
}

void MosStartup_ApplyInitialState(void)
{
	MosStartup_EnterFactoryMode(true);
}

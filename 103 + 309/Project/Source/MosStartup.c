#include "main.h"
#include "FactoryAging.h"
#include "MosStartup.h"

static UINT8 MosControl_IsCtlcEnabled(void)
{
	return (MCUO_AFE_CTLC != 0U) ? 1U : 0U;
}

static void MosControl_HoldBlockedState(void)
{
	if (GPIO_ReadOutputDataBit(GPIO_MCC_C, PIN_MCC_C) != (uint8_t)Bit_RESET)
	{
		GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_RESET);
	}
}

static void MosControl_WriteMosState(UINT8 charge_on, UINT8 discharge_on, BitAction mcc_level)
{
	UINT8 need_mtp_write = 0U;

	if (SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON != 1U)
	{
		SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1U;
		need_mtp_write = 1U;
	}
	if (SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS != charge_on)
	{
		SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = charge_on;
		need_mtp_write = 1U;
	}
	if (SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS != discharge_on)
	{
		SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = discharge_on;
		need_mtp_write = 1U;
	}

	if (need_mtp_write != 0U)
	{
		MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	}

	if (GPIO_ReadOutputDataBit(GPIO_MCC_C, PIN_MCC_C) != (uint8_t)mcc_level)
	{
		GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, mcc_level);
	}
}

static void MosControl_ApplyState(UINT8 charge_on, UINT8 discharge_on, BitAction mcc_level)
{
	/*
	 * CTLC is the MCU-side hard gate. While it is low, another protection
	 * path owns the MOS shutdown. Do not let normal mode switching undo it.
	 * The normal target will be restored automatically after CTLC is released.
	 */
	if (MosControl_IsCtlcEnabled() == 0U)
	{
		MosControl_HoldBlockedState();
		return;
	}

	MosControl_WriteMosState(charge_on, discharge_on, mcc_level);
}

UINT8 MosStartup_Is5vChargeActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET);
}

void MosStartup_OpenChargeCloseDischarge(void)
{
	MosControl_ApplyState(1U, 0U, Bit_SET);
}

void MosStartup_OpenDischargeCloseCharge(void)
{
	MosControl_ApplyState(0U, 1U, Bit_RESET);
}

void MosStartup_EnterFactoryMode(bool on)
{
	if (MosStartup_Is5vChargeActive() != 0U)
	{
		MosStartup_OpenChargeCloseDischarge();
		return;
	}

	if (on)
	{
		MosControl_ApplyState(1U, 1U, Bit_SET);
	}
	else
	{
		MosStartup_OpenDischargeCloseCharge();
	}
}

void MosStartup_ApplyInitialState(void)
{
	if (MosStartup_Is5vChargeActive() != 0U)
	{
		MosStartup_OpenChargeCloseDischarge();
	}
	else if (FactoryAging_ShouldStartOnBoot() != 0U)
	{
		MosStartup_EnterFactoryMode(true);
	}
	else
	{
		MosStartup_OpenDischargeCloseCharge();
	}
}

void MosControl_Update(void)
{
	/*
	 * Runtime MOS state is derived from current facts instead of a remembered
	 * charger edge. This makes boot, wakeup and runtime converge to one target
	 * even if CHG_IN changes while initialization is still running.
	 */
	if (MosControl_IsCtlcEnabled() == 0U)
	{
		MosControl_HoldBlockedState();
		return;
	}

	if (MosStartup_Is5vChargeActive() != 0U)
	{
		MosControl_WriteMosState(1U, 0U, Bit_SET);
	}
	else if (FactoryAging_IsActive() != 0U)
	{
		MosControl_WriteMosState(1U, 1U, Bit_SET);
	}
	else
	{
		MosControl_WriteMosState(0U, 1U, Bit_RESET);
	}
}

#include "BoardControl.h"

#include "I2C_AFE1.h"
#include "SH367309_Func.h"
#include "conf.h"

void open_chg_close_dsg(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1;
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 1;
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 0;
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_SET);
}

void open_dsg_close_chg(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1;
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 0;
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1;
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_RESET);
}

void enter_fac_mode(bool on)
{
	if (on)
	{
		SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1;
		SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 1;
		SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1;
		MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
		GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_SET);
	}
	else
	{
		open_dsg_close_chg();
	}
}

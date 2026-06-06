#include "main.h"
#include <stdint.h>

static UINT16 SOC_LimitA10(UINT32 current_a10)
{
	return (current_a10 > (UINT32)0xFFFFU) ? (UINT16)0xFFFFU : (UINT16)current_a10;
}

static UINT32 SOC_GetPackVoltageForTypeCMv(void)
{
	UINT32 pack_mv = (UINT32)g_stCellInfoReport.u16VCellTotle * 10U;

	if (pack_mv != 0U)
	{
		return pack_mv;
	}
	return ADC_GetVbatMilliVolt();
}

UINT16 SOC_GetTypeCBatEquivCurrentA10(void)
{
	// return ADC_GetTypeCOutCurrentMilliAmp();
	uint64_t numerator;
	uint64_t denominator;
	uint64_t current_mA;
	UINT32 pack_mv = SOC_GetPackVoltageForTypeCMv();
	UINT16 typec_out_current_mA = ADC_GetTypeCOutCurrentMilliAmp();

	if ((typec_out_current_mA == 0U) ||
		(pack_mv == 0U) ||
		(TYPEC_OUT_VOLTAGE_MV == 0U) ||
		(TYPEC_DCDC_EFFICIENCY_PERMILLE == 0U))
	{
		return 0U;
	}

	numerator = (uint64_t)typec_out_current_mA *
		(uint64_t)TYPEC_OUT_VOLTAGE_MV * 1000ULL;
	denominator = (uint64_t)pack_mv *
		(uint64_t)TYPEC_DCDC_EFFICIENCY_PERMILLE;
	current_mA = (numerator + (denominator / 2ULL)) / denominator;
	if (current_mA > 0xFFFFULL)
	{
		current_mA = 0xFFFFULL;
	}

	return SOC_LimitA10((UINT32)((current_mA + 50ULL) / 100ULL));
}

static int32_t SOC_GetNetCurrentMilliAmp(UINT16 report_ichg, UINT16 report_idsg)
{
	UINT32 chg_a10 = report_ichg;
	UINT32 dsg_a10 = (UINT32)report_idsg + (UINT32)SOC_GetTypeCBatEquivCurrentA10();

	if (chg_a10 >= dsg_a10)
	{
		return (int32_t)SOC_LimitA10(chg_a10 - dsg_a10) * 100;
	}
	return 0 - ((int32_t)SOC_LimitA10(dsg_a10 - chg_a10) * 100);
}

void InitData_SOC(void)
{
	soc_param_lib_init();
}

void App_SOC(void)
{
	static UINT32 s_u32LastAfeCurrentSampleSeq = 0U;
	UINT32 u32AfeCurrentSeq;
	UINT8 u8HasNewAfeSample;

	u32AfeCurrentSeq = AfeCurrent_GetSeq();
	u8HasNewAfeSample = (u32AfeCurrentSeq != s_u32LastAfeCurrentSampleSeq) ? 1U : 0U;
	if (u8HasNewAfeSample)
	{
		s_u32LastAfeCurrentSampleSeq = u32AfeCurrentSeq;
		SOC_IntEnhance_Ctrl(SOC_GetNetCurrentMilliAmp(g_stCellInfoReport.u16Ichg,
		                                               g_stCellInfoReport.u16IDischg));
	}
	else
	{
		SOC_PublishReportData();
	}

}

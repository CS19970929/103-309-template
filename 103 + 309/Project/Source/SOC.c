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

static UINT16 SOC_GetCompileTimeTableSelect(void)
{
#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
	return (UINT16)SOC_TABLE_LIFEPO;
#else
	return (UINT16)SOC_TABLE_TERNARYLI;
#endif
}

static UINT16 SOC_GetEffectiveTableSelect(UINT16 table_select)
{
#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
	return table_select;
#else
	(void)table_select;
	return SOC_GetCompileTimeTableSelect();
#endif
}

static void SOC_GetNetCurrentForCalc(UINT16 report_ichg, UINT16 report_idsg,
									 UINT16 *soc_ichg, UINT16 *soc_idsg)
{
	UINT32 chg_a10 = report_ichg;
	UINT32 dsg_a10 = (UINT32)report_idsg + (UINT32)SOC_GetTypeCBatEquivCurrentA10();

	if (chg_a10 >= dsg_a10)
	{
		*soc_ichg = SOC_LimitA10(chg_a10 - dsg_a10);
		*soc_idsg = 0U;
	}
	else
	{
		*soc_ichg = 0U;
		*soc_idsg = SOC_LimitA10(dsg_a10 - chg_a10);
	}
}

static void SOC_LoadConfigData(void)
{
	UINT16 i;

	SOC_Enhance_Element.u16_SOC_Ah = OtherElement.u16Soc_Ah;
	SOC_Enhance_Element.u16_SOC_CycleT_Ever = OtherElement.u16Soc_Cycle_times;
	SOC_Enhance_Element.u16_SOC_CycleT_Limit = 5000;
	SOC_Enhance_Element.u16_SOC_TableSelect =
		SOC_GetEffectiveTableSelect(OtherElement.u16Soc_TableSelect);
	SOC_Enhance_Element.u16_SOC_100_Vol = OtherElement.u16Soc_V_100;
	SOC_Enhance_Element.u16_SOC_0_Vol = OtherElement.u16Soc_V_0;

#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
	for (i = 0; i < SOC_Size_TableCanSet; ++i)
	{
		SOC_Enhance_Element.SOC_Table_CanSet[i] = SOC_Table_Set[i];
	}
#else
	(void)i;
#endif
}

void InitData_SOC(void)
{
	SOC_LoadConfigData();
	soc_param_lib_init();
}

void App_SOC(void)
{
	static UINT32 s_u32LastAfeCurrentSampleSeq = 0U;
	UINT32 u32AfeCurrentSeq;
	UINT8 u8HasNewAfeSample;
	UINT16 soc_ichg;
	UINT16 soc_idsg;

	u32AfeCurrentSeq = AfeCurrent_GetSeq();
	u8HasNewAfeSample = (u32AfeCurrentSeq != s_u32LastAfeCurrentSampleSeq) ? 1U : 0U;
	if (u8HasNewAfeSample)
	{
		s_u32LastAfeCurrentSampleSeq = u32AfeCurrentSeq;
		SOC_GetNetCurrentForCalc(g_stCellInfoReport.u16Ichg,
								 g_stCellInfoReport.u16IDischg,
								 &soc_ichg,
								 &soc_idsg);
		SOC_UpdateSampleData(g_stCellInfoReport.u16VCellMax,
							 g_stCellInfoReport.u16VCellMin,
							 soc_ichg,
							 soc_idsg);
		SOC_IntEnhance_Ctrl();
	}
	else
	{
		SOC_PublishReportData();
	}

}

#if 0
UINT8 SOC_TestMode_RunSample(UINT8 enable, UINT16 vcell_max, UINT16 vcell_min,
							 UINT16 ichg, UINT16 idsg, UINT16 ticks)
{
	(void)enable;
	(void)vcell_max;
	(void)vcell_min;
	(void)ichg;
	(void)idsg;
	(void)ticks;
	return 0U;
}

void SOC_TestMode_ReadStatus(UINT16 status_words[], UINT16 word_count)
{
	(void)status_words;
	(void)word_count;
}
#endif

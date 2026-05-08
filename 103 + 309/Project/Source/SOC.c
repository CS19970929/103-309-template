#include "main.h"

UINT16 SOC_Table_Set[SOC_TABLE_SIZE];

const UINT16 SOC_Table_Default[SOC_TABLE_SIZE] = {
	4160, 100,
	4100, 95,
	4050, 90,
	3995, 85,
	3935, 80,
	3880, 75,
	3835, 70,
	3795, 65,
	3760, 60,
	3725, 55,
	3695, 50,
	3670, 45,
	3645, 40,
	3615, 35,
	3585, 30,
	3555, 25,
	3525, 20,
	3480, 15,
	3400, 10,
	3250, 5,
	3000, 0,
};

static void SOC_LoadConfigData(void)
{
	UINT16 i;

	SOC_Enhance_Element.u16_SOC_Ah = OtherElement.u16Soc_Ah;
	SOC_Enhance_Element.u16_SOC_CycleT_Ever = OtherElement.u16Soc_Cycle_times;
	SOC_Enhance_Element.u16_SOC_CycleT_Limit = 5000;
	SOC_Enhance_Element.u16_SOC_TableSelect = OtherElement.u16Soc_TableSelect;
	SOC_Enhance_Element.u16_SOC_100_Vol = OtherElement.u16Soc_V_100;
	SOC_Enhance_Element.u16_SOC_0_Vol = OtherElement.u16Soc_V_0;

	for (i = 0; i < SOC_Size_TableCanSet; ++i)
	{
		SOC_Enhance_Element.SOC_Table_CanSet[i] = SOC_Table_Set[i];
	}
}

void InitData_SOC(void)
{
	SOC_LoadConfigData();
	soc_param_lib_init();
	SOC_PublishReportData();
}

void App_SOC(void)
{
	static UINT32 s_u32LastAfeCurrentSampleSeq = 0U;
	UINT8 u8HasNewAfeSample;

	u8HasNewAfeSample = (g_u32AfeCurrentSampleSeq != s_u32LastAfeCurrentSampleSeq) ? 1U : 0U;
	if (u8HasNewAfeSample)
	{
		s_u32LastAfeCurrentSampleSeq = g_u32AfeCurrentSampleSeq;
		SOC_UpdateSampleData(g_stCellInfoReport.u16VCellMax,
							 g_stCellInfoReport.u16VCellMin,
							 g_stCellInfoReport.u16Ichg,
							 g_stCellInfoReport.u16IDischg);
		SOC_IntEnhance_Ctrl();
	}
	else
	{
		SOC_PublishReportData();
	}

	if (SOC_Enhance_Element.u16_SOC_InitOver)
	{
		System_Func_StartUp.bits.b1StartUpFlag_SOC = 0;
	}
}

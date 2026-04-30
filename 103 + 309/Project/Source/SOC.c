#include "main.h"
#include "SocAutoTest.h"

UINT16 SOC_Table_Set[SOC_TABLE_SIZE];

const UINT16 SOC_Table_Default[SOC_TABLE_SIZE] = {
	3336,
	100,
	3332,
	90,
	3330,
	80,
	3327,
	75,
	3316,
	70,
	3301,
	65,
	3294,
	60,
	3291,
	55,
	3290,
	50,
	3288,
	45,
	3286,
	40,
	3279,
	35,
	3266,
	30,
	3254,
	25,
	3236,
	20,
	3212,
	15,
	3198,
	10,
	3112,
	5,
	2526,
	0,
	1000,
	0,
	1000,
	0,
};

// 一次性赋值
static void SOC_LoadConfigData(void)
{
	UINT16 i;

	SOC_Enhance_Element.u16_SOC_Ah = OtherElement.u16Soc_Ah;
	SOC_Enhance_Element.u16_SOC_CycleT_Ever = OtherElement.u16Soc_Cycle_times;
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
#if !PROJECT_CFG_SOC_AUTO_TEST_ENABLE
	static UINT32 s_u32LastAfeCurrentSampleSeq = 0U;
	UINT8 u8HasNewAfeSample;
#endif

	if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag)
	{
		return;
	}

#if !LEDBAR_DRIVER_GPIO_CHARLIE
	MCUO_DEBUG_LED1 = !MCUO_DEBUG_LED1;
#endif

#if PROJECT_CFG_SOC_AUTO_TEST_ENABLE
	SocAutoTest_Task();
#else
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
#endif

	if (SOC_Enhance_Element.u16_SOC_InitOver)
	{
		System_Func_StartUp.bits.b1StartUpFlag_SOC = 0; // 初始化完毕
	}
}

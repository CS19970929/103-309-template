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

#define SOC_TEST_MODE_STATUS_WORDS       ((UINT16)16U)
#define SOC_TEST_MODE_TICK_MS            ((UINT16)200U)
#define SOC_TEST_MODE_TICKS_PER_SECOND   ((UINT16)5U)
#define SOC_TEST_MODE_RESULT_OK          ((UINT16)0U)
#define SOC_TEST_MODE_RESULT_UNSUPPORTED ((UINT16)1U)
#define SOC_TEST_MODE_RESULT_DISABLED    ((UINT16)2U)
#define SOC_TEST_MODE_RESULT_INVALID     ((UINT16)3U)

#if PROJECT_CFG_SOC_TEST_MODE_ENABLE
typedef struct
{
	UINT8 u8Enabled;
	UINT16 u16LastResult;
	UINT16 u16LastVCellMax;
	UINT16 u16LastVCellMin;
	UINT16 u16LastIchg;
	UINT16 u16LastIdsg;
	UINT16 u16LastTicks;
	UINT32 u32TotalTicks;
} SOC_TEST_MODE_STATE;

static SOC_TEST_MODE_STATE s_stSocTestMode;

static UINT8 SOC_TestMode_InputValid(UINT16 vcell_max, UINT16 vcell_min,
									 UINT16 ichg, UINT16 idsg, UINT16 ticks)
{
	if ((ticks == 0U) || (ticks > (UINT16)PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX))
	{
		return 0U;
	}
	if ((vcell_min < 1000U) || (vcell_max > 5000U) || (vcell_min > vcell_max))
	{
		return 0U;
	}
	if ((ichg > 5000U) || (idsg > 5000U))
	{
		return 0U;
	}
	return 1U;
}

static void SOC_TestMode_ApplyReportSample(UINT16 vcell_max, UINT16 vcell_min,
										   UINT16 ichg, UINT16 idsg)
{
	UINT32 total_mv;

	g_stCellInfoReport.u16VCellMax = vcell_max;
	g_stCellInfoReport.u16VCellMin = vcell_min;
	g_stCellInfoReport.u16VCellDelta = (UINT16)(vcell_max - vcell_min);
	total_mv = (UINT32)vcell_min * (UINT32)SeriesNum;
	g_stCellInfoReport.u16VCellTotle = (UINT16)(total_mv / 10U);
	g_stCellInfoReport.u16Ichg = ichg;
	g_stCellInfoReport.u16IDischg = idsg;
}
#endif

static UINT16 SOC_LimitA10(UINT32 current_a10)
{
	return (current_a10 > (UINT32)0xFFFFU) ? (UINT16)0xFFFFU : (UINT16)current_a10;
}

static void SOC_GetNetCurrentForCalc(UINT16 report_ichg, UINT16 report_idsg,
									 UINT16 *soc_ichg, UINT16 *soc_idsg)
{
	UINT32 chg_a10 = report_ichg;
	UINT32 dsg_a10 = report_idsg;

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
	UINT16 soc_ichg;
	UINT16 soc_idsg;

#if PROJECT_CFG_SOC_TEST_MODE_ENABLE
	if (s_stSocTestMode.u8Enabled)
	{
		SOC_PublishReportData();
		if (SOC_Enhance_Element.u16_SOC_InitOver)
		{
			System_Func_StartUp.bits.b1StartUpFlag_SOC = 0;
		}
		return;
	}
#endif

	u8HasNewAfeSample = (g_u32AfeCurrentSampleSeq != s_u32LastAfeCurrentSampleSeq) ? 1U : 0U;
	if (u8HasNewAfeSample)
	{
		s_u32LastAfeCurrentSampleSeq = g_u32AfeCurrentSampleSeq;
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

	if (SOC_Enhance_Element.u16_SOC_InitOver)
	{
		System_Func_StartUp.bits.b1StartUpFlag_SOC = 0;
	}
}

UINT8 SOC_TestMode_RunSample(UINT8 enable, UINT16 vcell_max, UINT16 vcell_min,
							 UINT16 ichg, UINT16 idsg, UINT16 ticks)
{
#if PROJECT_CFG_SOC_TEST_MODE_ENABLE
	UINT16 i;

	if (enable == 0U)
	{
		s_stSocTestMode.u8Enabled = 0U;
		s_stSocTestMode.u16LastResult = SOC_TEST_MODE_RESULT_OK;
		return 1U;
	}
	if (!SOC_TestMode_InputValid(vcell_max, vcell_min, ichg, idsg, ticks))
	{
		s_stSocTestMode.u16LastResult = SOC_TEST_MODE_RESULT_INVALID;
		return 0U;
	}
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		s_stSocTestMode.u16LastResult = SOC_TEST_MODE_RESULT_DISABLED;
		return 0U;
	}

	s_stSocTestMode.u8Enabled = 1U;
	s_stSocTestMode.u16LastVCellMax = vcell_max;
	s_stSocTestMode.u16LastVCellMin = vcell_min;
	s_stSocTestMode.u16LastIchg = ichg;
	s_stSocTestMode.u16LastIdsg = idsg;
	s_stSocTestMode.u16LastTicks = ticks;
	for (i = 0U; i < ticks; ++i)
	{
		SOC_TestMode_ApplyReportSample(vcell_max, vcell_min, ichg, idsg);
		SOC_UpdateSampleData(vcell_max, vcell_min, ichg, idsg);
		SOC_IntEnhance_Ctrl();
		++s_stSocTestMode.u32TotalTicks;
	}
	s_stSocTestMode.u16LastResult = SOC_TEST_MODE_RESULT_OK;
	return 1U;
#else
	(void)enable;
	(void)vcell_max;
	(void)vcell_min;
	(void)ichg;
	(void)idsg;
	(void)ticks;
	return 0U;
#endif
}

void SOC_TestMode_ReadStatus(UINT16 status_words[], UINT16 word_count)
{
	UINT16 i;

	if (status_words == 0)
	{
		return;
	}
	for (i = 0U; i < word_count; ++i)
	{
		status_words[i] = 0U;
	}
	if (word_count < SOC_TEST_MODE_STATUS_WORDS)
	{
		return;
	}
	status_words[0] = (UINT16)PROJECT_CFG_SOC_TEST_MODE_ENABLE;
	status_words[2] = SOC_TEST_MODE_TICK_MS;
	status_words[3] = SOC_TEST_MODE_TICKS_PER_SECOND;
	status_words[4] = (UINT16)PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX;
#if PROJECT_CFG_SOC_TEST_MODE_ENABLE
	status_words[1] = (UINT16)s_stSocTestMode.u8Enabled;
	status_words[5] = s_stSocTestMode.u16LastVCellMax;
	status_words[6] = s_stSocTestMode.u16LastVCellMin;
	status_words[7] = s_stSocTestMode.u16LastIchg;
	status_words[8] = s_stSocTestMode.u16LastIdsg;
	status_words[9] = s_stSocTestMode.u16LastTicks;
	status_words[10] = (UINT16)(s_stSocTestMode.u32TotalTicks >> 16);
	status_words[11] = (UINT16)(s_stSocTestMode.u32TotalTicks & 0xFFFFU);
	status_words[12] = (UINT16)g_stCellInfoReport.SocElement.u16Soc;
	status_words[13] = (UINT16)g_stCellInfoReport.SocElement.u16Soh;
	status_words[14] = (UINT16)g_stCellInfoReport.SocElement.u16CapacityNow;
	status_words[15] = s_stSocTestMode.u16LastResult;
#else
	status_words[15] = SOC_TEST_MODE_RESULT_UNSUPPORTED;
#endif
}

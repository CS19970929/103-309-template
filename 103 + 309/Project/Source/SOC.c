#include "main.h"
#include <stdint.h>

static UINT16 SOC_LimitA10(UINT32 current_a10)
{
	return (current_a10 > (UINT32)0xFFFFU) ? (UINT16)0xFFFFU : (UINT16)current_a10;
}

static int32_t SOC_GetNetCurrentMilliAmp(UINT16 report_ichg, UINT16 report_idsg)
{
	UINT32 chg_a10 = report_ichg;
	UINT32 dsg_a10 = report_idsg;

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

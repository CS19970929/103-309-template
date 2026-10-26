#include "main.h"
#include <stdint.h>

#define SOC_TIME_CURRENT_MIN_MA 200L

UINT16 SOC_GetTypeCBatEquivCurrentA10(void)
{
	return 0;
}

static int32_t SOC_GetNetCurrentMilliAmp(void)
{
	int64_t net_current_mA;

	/*
	 * AFE current is already signed and calibrated in mA. Keep the existing
	 * Type-C equivalent-current meaning: it is an additional discharge load.
	 */
	net_current_mA = (int64_t)AfeCurrent_GetCurrent_mA() -
	                 ((int64_t)SOC_GetTypeCBatEquivCurrentA10() * 100LL);

	if (net_current_mA > (int64_t)0x7FFFFFFF)
	{
		return (int32_t)0x7FFFFFFF;
	}
	if (net_current_mA < -((int64_t)0x7FFFFFFF) - 1LL)
	{
		return (int32_t)(-2147483647L - 1L);
	}
	return (int32_t)net_current_mA;
}

void InitData_SOC(void)
{
	soc_param_lib_init();
}

void App_SOC(void)
{
	SOC_IntEnhance_Ctrl(SOC_GetNetCurrentMilliAmp());
}

UINT16 SOC_GetRemainingMinutes(UINT8 charging)
{
    UINT32 capacity, minutes;
    int64_t current_ma;
    INT32 measured_ma=SOC_GetNetCurrentMilliAmp();
    UINT16 full=g_stCellInfoReport.SocElement.u16CapacityFull;
    UINT16 now=g_stCellInfoReport.SocElement.u16CapacityNow;

    if (!Fault_IsCurrentSampleFresh() || full==0U) return 0U;
    if (now>full) now=full;
    /* Use the same calibrated net current and board load as coulomb counting.
     * No extrapolation through idle, invalid samples or STOP. */
    current_ma=(int64_t)measured_ma-PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA;
    if (charging) {
        if (measured_ma<SOC_TIME_CURRENT_MIN_MA || current_ma<=0) return 0U;
        capacity=(UINT32)(full-now);
    }
    else {
        if (measured_ma>-SOC_TIME_CURRENT_MIN_MA) return 0U;
        capacity=now;
        current_ma=-current_ma;
    }
    /* Capacity is Ah*100: one unit is 10 mAh, or 600 mA*min.
     * Round up a nonzero remainder; the largest numerator is 39,321,000. */
    minutes=(UINT32)(((int64_t)capacity*600+current_ma-1)/current_ma);
    return (UINT16)(minutes>0xFFFFU ? 0xFFFFU : minutes);
}

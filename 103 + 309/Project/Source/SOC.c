#include "main.h"
#include <stdint.h>

#define SOC_TIME_CURRENT_MIN_MA 200L
#define SOC_TIME_UNKNOWN_MINUTES 0xFFFFU
#define SOC_TIME_MAX_MINUTES 0xFFFEU

void InitData_SOC(void)
{
	soc_param_lib_init();
}

void App_SOC(void)
{
	if (!Fault_IsCurrentSampleFresh())
        return;
    SOC_IntEnhance_Ctrl(AfeCurrent_GetCurrent_mA());
}

UINT16 SOC_GetRemainingMinutes(UINT8 charging)
{
    UINT32 capacity, minutes;
    int64_t current_ma;
    INT32 measured_ma=AfeCurrent_GetCurrent_mA();
    UINT16 full=g_stCellInfoReport.SocElement.u16CapacityFull;
    UINT16 now=g_stCellInfoReport.SocElement.u16CapacityNow;

    if (!Fault_IsCurrentSampleFresh() || full==0U)
        return SOC_TIME_UNKNOWN_MINUTES;
    if (measured_ma>-SOC_TIME_CURRENT_MIN_MA && measured_ma<SOC_TIME_CURRENT_MIN_MA)
        return SOC_TIME_UNKNOWN_MINUTES;
    if (now>full) now=full;
    /* Use the same measured current as coulomb counting. */
    current_ma=(int64_t)measured_ma;
    if (charging) {
        if (measured_ma<SOC_TIME_CURRENT_MIN_MA) return 0U;
        if (current_ma<=0) return SOC_TIME_UNKNOWN_MINUTES;
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
    /* Reserve 0xFFFF for unavailable estimates on the customer wire format. */
    return (UINT16)(minutes>SOC_TIME_MAX_MINUTES ? SOC_TIME_MAX_MINUTES : minutes);
}

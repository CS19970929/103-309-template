#include "main.h"
#include <stdint.h>

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

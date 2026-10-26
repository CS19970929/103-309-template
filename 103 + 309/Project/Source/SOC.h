#ifndef SOC_H
#define SOC_H

#include "SocEnhance.h"

void InitData_SOC(void);
void App_SOC(void);
/* 0xFFFF: unavailable; 0..0xFFFE: minutes (opposite direction returns zero). */
UINT16 SOC_GetRemainingMinutes(UINT8 charging);

#endif	/* SOC_H */

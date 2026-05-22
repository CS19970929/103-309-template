#ifndef SOC_H
#define SOC_H

#include "SocEnhance.h"

#define SOC_TABLE_SIZE 		42
#define SOC_VOL_MIN   		((UINT16)0)
#define SOC_VOL_MAX   		((UINT16)5000)		//mV
#define SOC_VALUE_MIN   	((UINT16)0)
#define SOC_VALUE_MAX   	((UINT16)100)		//%

#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
extern UINT16 SOC_Table_Set[SOC_TABLE_SIZE];
extern const UINT16 SOC_Table_Default[SOC_TABLE_SIZE];
#endif

void InitData_SOC(void);
void App_SOC(void);
UINT8 SOC_TestMode_RunSample(UINT8 enable, UINT16 vcell_max, UINT16 vcell_min,
							 UINT16 ichg, UINT16 idsg, UINT16 ticks);
void SOC_TestMode_ReadStatus(UINT16 status_words[], UINT16 word_count);

#endif	/* SOC_H */


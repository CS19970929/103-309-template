#ifndef SOCENHANCE_H
#define SOCENHANCE_H

#include "stm32f10x.h"
#include "Project_Config.h"
#include <stdint.h>
//#include "stm32f0xx.h"

#define SOC_TABLE_SIZE 		42

enum SOC_TABLE_SELECT {
	SOC_TABLE_LIFEPO = 1,
	SOC_TABLE_TERNARYLI = 2
};

#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
extern const UINT16 SOC_Table_LiFePO[SOC_TABLE_SIZE];
#endif
#if (PROJECT_CFG_BAT_CHEMISTRY == 0)
extern const UINT16 SocTable_TernaryLi[SOC_TABLE_SIZE];
#endif

void SOC_IntEnhance_Ctrl(int32_t net_current_ma);
void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max);
void SOC_SaveSnapshotBeforeSleep(void);
void SOC_PublishReportData(void);
void SOC_RequestCapacityReset(void);
void SOC_RequestSetOnce(UINT8 soc);

void soc_param_lib_init(void);
UINT8 SOC_ResetStoredSnapshotToDefault(void);

#endif	/* SOCENHANCE_H */

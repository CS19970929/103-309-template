#ifndef SOCENHANCE_H
#define SOCENHANCE_H

#include "stm32f10x.h"
//#include "stm32f0xx.h"


#define SOC_Size_TableCanSet 	(UINT16)42
#define SOC_Size_LiFePO 		(UINT16)42
#define SOC_Size_TernaryLi 		(UINT16)42
#define SOC_Size_LiFePO2 		(UINT16)42
#define SOC_DEFAULT_STARTUP_PERCENT ((UINT8)60)

enum SOC_TABLE_SELECT {
	SOC_TABLE_TEST = 0,
	SOC_TABLE_LIFEPO,
	SOC_TABLE_TERNARYLI,
	SOC_TABLE_LIFEPO2
};

extern const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO];
extern const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi];
extern const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2];

struct SOC_ENHANCE_ELEMENT {
	UINT16 u16_SOC_Ah;                 // 10 * Ah
	UINT16 u16_SOC_CycleT_Ever;        // cycle count loaded from config
	UINT16 u16_SOC_CycleT_Limit;       // cycle limit
	UINT16 u16_SOC_TableSelect;        // enum SOC_TABLE_SELECT
	UINT16 u16_SOC_0_Vol;              // mV at SOC 0%
	UINT16 u16_SOC_100_Vol;            // mV at SOC 100%
	UINT16 SOC_Table_CanSet[SOC_Size_TableCanSet];
	UINT8 u8_SetSocOnce;

	UINT16 u16_VCellMax;               // mV
	UINT16 u16_VCellMin;               // mV
	UINT16 u16_Ichg;                   // A * 10
	UINT16 u16_Idsg;                   // A * 10

	UINT16 u16_SOC_InitOver;
	UINT8 u8_SOC;
	UINT8 u8_SOH;
	UINT16 u16_CapacityNow;            // Ah * 100
	UINT16 u16_CapacityFull;           // Ah * 100
	UINT16 u16_CapacityFactory;        // Ah * 100
	UINT16 u16_Cycle_times;
	UINT8 u8_SOC_OCV_Cali;

	UINT16 u16_RefreshData_Flag;       // 1: OCV refresh, 2: capacity reset, 3: set SOC once
};

extern struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;

void SOC_IntEnhance_Ctrl(void);
void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max);
void SOC_UpdateSampleData(UINT16 vcell_max, UINT16 vcell_min, UINT16 ichg, UINT16 idsg);
void SOC_PublishReportData(void);

void soc_param_lib_init(void);
UINT8 SOC_ResetStoredSnapshotToDefault(void);
void SOC_Test_SetKernelSoc(UINT8 soc);
UINT8 SOC_Test_GetKernelSoc(void);
#endif	/* SOCENHANCE_H */

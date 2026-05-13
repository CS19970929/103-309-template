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

enum SOC_WATCH_CALIB_SOURCE {
	SOC_WATCH_CALIB_NONE = 0,
	SOC_WATCH_CALIB_INTEGRATE_CHG,
	SOC_WATCH_CALIB_INTEGRATE_DSG,
	SOC_WATCH_CALIB_FULL_ANCHOR,
	SOC_WATCH_CALIB_EMPTY_TAIL,
	SOC_WATCH_CALIB_MID_TAIL,
	SOC_WATCH_CALIB_REST_TARGET,
	SOC_WATCH_CALIB_DEFERRED_OCV,
	SOC_WATCH_CALIB_LONG_REST_DOWN,
	SOC_WATCH_CALIB_MANUAL_OCV,
	SOC_WATCH_CALIB_PARAM_RESET,
	SOC_WATCH_CALIB_SET_ONCE,
	SOC_WATCH_CALIB_STARTUP_SNAPSHOT,
	SOC_WATCH_CALIB_STARTUP_OCV,
	SOC_WATCH_CALIB_STARTUP_DEFAULT,
	SOC_WATCH_CALIB_RTC_REST,
	SOC_WATCH_CALIB_BOARD_SELF_CONSUMPTION
};

enum SOC_WATCH_BLOCK_REASON {
	SOC_WATCH_BLOCK_NONE = 0,
	SOC_WATCH_BLOCK_VOLTAGE_INVALID,
	SOC_WATCH_BLOCK_CELL_DELTA,
	SOC_WATCH_BLOCK_PROTECTION_FAULT,
	SOC_WATCH_BLOCK_SYSTEM_FAULT,
	SOC_WATCH_BLOCK_SAG_HOLD,
	SOC_WATCH_BLOCK_DIRECTION,
	SOC_WATCH_BLOCK_NOT_RELAX,
	SOC_WATCH_BLOCK_LOW_TAIL,
	SOC_WATCH_BLOCK_REST_UNSTABLE,
	SOC_WATCH_BLOCK_REFRESH_FIXED,
	SOC_WATCH_BLOCK_REFRESH_ZERO
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

struct SOC_DEBUG_WATCH {
	UINT32 u32CapFactoryAs10;
	UINT32 u32CapFullAs10;
	UINT32 u32CapNowAs10;
	UINT32 u32CycleX100;
	UINT32 u32DsgAccAs10;
	UINT32 u32RestTicks;
	UINT32 u32StableRestTicks;
	UINT32 u32ShortRestTicks;
	UINT32 u32LongRestDownTicks;
	UINT16 u16VCellMax;
	UINT16 u16VCellMin;
	UINT16 u16CellDelta;
	UINT16 u16Ichg;
	UINT16 u16Idsg;
	UINT16 u16FullTicks;
	UINT16 u16EmptyTicks;
	UINT16 u16MidTicks;
	UINT16 u16DisplayTicks;
	UINT16 u16SagHoldTicks;
	UINT16 u16DeferredOcvTicks;
	UINT16 u16RestRefVmin;
	UINT16 u16RestRefVmax;
	UINT16 u16EmptyTailTarget;
	UINT16 u16EmptyTailTicks;
	UINT16 u16MidTailTarget;
	UINT16 u16MidTailTicks;
	UINT16 u16SnapshotFlags;
	UINT8 u8Mode;
	UINT8 u8LastMode;
	UINT8 u8InternalSoc;
	UINT8 u8DisplaySoc;
	UINT8 u8Soh;
	UINT8 u8DeferredOcvValid;
	UINT8 u8DeferredOcvTarget;
	UINT8 u8FullAnchor;
	UINT8 u8LowTailActive;
	UINT8 u8MidTailActive;
	UINT8 u8CalibrationAllowed;
	UINT8 u8SagHoldBlocksCalibration;
	UINT8 u8RestVoltageStable;
	UINT8 u8LastCalibSource;
	UINT8 u8LastBlockReason;
	UINT8 u8LastSocBefore;
	UINT8 u8LastSocAfter;
	UINT8 u8LastPublishForce;
};

extern struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;
extern struct SOC_DEBUG_WATCH * const g_dbg_soc_watch;

void SOC_IntEnhance_Ctrl(void);
void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max);
void SOC_SaveSnapshotBeforeSleep(void);
void SOC_UpdateSampleData(UINT16 vcell_max, UINT16 vcell_min, UINT16 ichg, UINT16 idsg);
void SOC_PublishReportData(void);

void soc_param_lib_init(void);
UINT8 SOC_ResetStoredSnapshotToDefault(void);
#endif	/* SOCENHANCE_H */

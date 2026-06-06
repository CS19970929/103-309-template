#ifndef SOCENHANCE_H
#define SOCENHANCE_H

#include "stm32f10x.h"
#include "Project_Config.h"
#include <stdint.h>
//#include "stm32f0xx.h"

#define SOC_Size_LiFePO 		(UINT16)42
#define SOC_Size_TernaryLi 		(UINT16)42
#define SOC_DEFAULT_STARTUP_PERCENT ((UINT8)60)

enum SOC_TABLE_SELECT {
	SOC_TABLE_LIFEPO = 1,
	SOC_TABLE_TERNARYLI = 2
};

enum SOC_WATCH_CALIB_SOURCE {
	SOC_WATCH_CALIB_NONE = 0,
	SOC_WATCH_CALIB_INTEGRATE_CHG,
	SOC_WATCH_CALIB_INTEGRATE_DSG,
	SOC_WATCH_CALIB_FULL_ANCHOR,
	SOC_WATCH_CALIB_EMPTY_TAIL,
	SOC_WATCH_CALIB_LONG_REST_DOWN,
	SOC_WATCH_CALIB_PARAM_RESET,
	SOC_WATCH_CALIB_SET_ONCE,
	SOC_WATCH_CALIB_STARTUP_SNAPSHOT,
	SOC_WATCH_CALIB_STARTUP_OCV,
	SOC_WATCH_CALIB_STARTUP_DEFAULT,
	SOC_WATCH_CALIB_RTC_REST,
	SOC_WATCH_CALIB_BOARD_SELF_CONSUMPTION
};

#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
extern const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO];
#endif
#if (PROJECT_CFG_BAT_CHEMISTRY == 0)
extern const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi];
#endif

struct SOC_DEBUG_WATCH {
	UINT32 u32CapFactoryAs10;
	UINT32 u32CapFullAs10;
	UINT32 u32CapNowAs10;
	UINT32 u32CycleX100;
	UINT32 u32DsgAccAs10;
	UINT32 u32RestTicks;
	UINT32 u32StableRestTicks;
	UINT32 u32LongRestDownTicks;
	UINT16 u16VCellMax;
	UINT16 u16VCellMin;
	UINT16 u16CellDelta;
	UINT16 u16Ichg;
	UINT16 u16Idsg;
	UINT16 u16FullTicks;
	UINT16 u16EmptyTicks;
	UINT16 u16SagHoldTicks;
	UINT16 u16RestRefVmin;
	UINT16 u16RestRefVmax;
	UINT16 u16EmptyTailTarget;
	UINT16 u16EmptyTailTicks;
	UINT16 u16SnapshotFlags;
	UINT8 u8Mode;
	UINT8 u8LastMode;
	UINT8 u8InternalSoc;
	UINT8 u8Soh;
	UINT8 u8RestDownValid;
	UINT8 u8RestDownTarget;
	UINT8 u8LowTailActive;
	UINT8 u8CalibrationAllowed;
	UINT8 u8SagHoldBlocksCalibration;
	UINT8 u8RestVoltageStable;
	UINT8 u8LastCalibSource;
	UINT8 u8LastSocBefore;
	UINT8 u8LastSocAfter;
	UINT8 u8LastPublishForce;
};

void SOC_IntEnhance_Ctrl(int32_t net_current_ma);
void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max);
void SOC_SaveSnapshotBeforeSleep(void);
void SOC_PublishReportData(void);
void SOC_RequestCapacityReset(void);
void SOC_RequestSetOnce(UINT8 soc);

void soc_param_lib_init(void);
UINT8 SOC_ResetStoredSnapshotToDefault(void);

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE
void SOC_GetDebugInternals(uint8_t *mode, uint8_t *last_mode,
                           uint32_t *rest_soc_ticks, uint32_t *stable_soc_ticks,
                           uint16_t *full_ticks, uint16_t *empty_ticks);
#endif

#endif	/* SOCENHANCE_H */

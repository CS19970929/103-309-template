#ifndef SOC_H
#define SOC_H

#include "stm32f10x.h"
#include "Project_Config.h"
#include <stdint.h>

/* ── Sizing ── */
#define SOC_TABLE_SIZE            ((UINT16)42)
#define SOC_Size_TableCanSet      ((UINT16)42)
#define SOC_Size_LiFePO           ((UINT16)42)
#define SOC_Size_TernaryLi        ((UINT16)42)
#define SOC_Size_LiFePO2          ((UINT16)42)
#define SOC_DEFAULT_STARTUP_PERCENT ((UINT8)60)

#ifndef PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
#define PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE 0
#endif

/* ── Table enum ── */
enum SOC_TABLE_SELECT {
    SOC_TABLE_TEST = 0,
    SOC_TABLE_LIFEPO,
    SOC_TABLE_TERNARYLI,
    SOC_TABLE_LIFEPO2
};

/* ── Calibration source ── */
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

/* ── Block reason ── */
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

/* ── SOC tables ── */
#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE || (PROJECT_CFG_BAT_CHEMISTRY == 1)
extern const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO];
#endif
#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE || (PROJECT_CFG_BAT_CHEMISTRY == 0)
extern const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi];
#endif
#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
extern const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2];
#endif

#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
extern UINT16 SOC_Table_Set[SOC_TABLE_SIZE];
extern const UINT16 SOC_Table_Default[SOC_TABLE_SIZE];
#endif

/* ── Runtime data ── */
struct SOC_ENHANCE_ELEMENT {
    UINT16 u16_SOC_Ah;
    UINT16 u16_SOC_CycleT_Ever;
    UINT16 u16_SOC_CycleT_Limit;
    UINT16 u16_SOC_TableSelect;
    UINT16 u16_SOC_0_Vol;
    UINT16 u16_SOC_100_Vol;
#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
    UINT16 SOC_Table_CanSet[SOC_Size_TableCanSet];
#endif
    UINT8  u8_SetSocOnce;
    UINT16 u16_VCellMax;
    UINT16 u16_VCellMin;
    UINT16 u16_Ichg;
    UINT16 u16_Idsg;
    UINT16 u16_SOC_InitOver;
    UINT8  u8_SOC;
    UINT8  u8_SOH;
    UINT16 u16_CapacityNow;
    UINT16 u16_CapacityFull;
    UINT16 u16_CapacityFactory;
    UINT16 u16_Cycle_times;
    UINT8  u8_SOC_OCV_Cali;
    UINT16 u16_RefreshData_Flag;
};

extern struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;

/* ── Debug watch ── */
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
    UINT8  u8Mode;
    UINT8  u8LastMode;
    UINT8  u8InternalSoc;
    UINT8  u8DisplaySoc;
    UINT8  u8Soh;
    UINT8  u8DeferredOcvValid;
    UINT8  u8DeferredOcvTarget;
    UINT8  u8FullAnchor;
    UINT8  u8LowTailActive;
    UINT8  u8MidTailActive;
    UINT8  u8CalibrationAllowed;
    UINT8  u8SagHoldBlocksCalibration;
    UINT8  u8RestVoltageStable;
    UINT8  u8LastCalibSource;
    UINT8  u8LastBlockReason;
    UINT8  u8LastSocBefore;
    UINT8  u8LastSocAfter;
    UINT8  u8LastPublishForce;
};

extern struct SOC_DEBUG_WATCH * const g_dbg_soc_watch;

/* ── Public API ── */
void     InitData_SOC(void);
void     App_SOC(void);
UINT8    SOC_TestMode_RunSample(UINT8 enable, UINT16 vcell_max, UINT16 vcell_min,
                                UINT16 ichg, UINT16 idsg, UINT16 ticks);
void     SOC_TestMode_ReadStatus(UINT16 status_words[], UINT16 word_count);
void     SOC_IntEnhance_Ctrl(void);
void     SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max);
void     SOC_SaveSnapshotBeforeSleep(void);
void     SOC_UpdateSampleData(UINT16 vcell_max, UINT16 vcell_min, UINT16 ichg, UINT16 idsg);
void     SOC_PublishReportData(void);
void     soc_param_lib_init(void);

#endif  /* SOC_H */
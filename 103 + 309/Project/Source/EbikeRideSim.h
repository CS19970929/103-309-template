#ifndef EBIKE_RIDE_SIM_H
#define EBIKE_RIDE_SIM_H

#include "stm32f10x.h"

typedef struct _EBIKE_RIDE_SIM_OBSERVE {
    UINT8 u8Enabled;
    UINT8 u8Profile;
    UINT8 u8TruthSoc;
    UINT8 u8SegmentIndex;
    UINT16 u16CellOcv_mV;
    UINT16 u16CellLoad_mV;
    UINT16 u16VCellMin_mV;
    UINT16 u16VCellMax_mV;
    UINT16 u16Ichg_A10;
    UINT16 u16IDsg_A10;
    UINT32 u32ElapsedMs;
    UINT32 u32SegmentElapsedMs;
    UINT32 u32TruthCap_As10;
} EBIKE_RIDE_SIM_OBSERVE;

extern EBIKE_RIDE_SIM_OBSERVE g_stEbikeRideSimObserve;

void EbikeRideSim_Update(UINT16 period_ms);
void EbikeRideSim_Reset(UINT8 initial_soc_percent);
void EbikeRideSim_SetManualSample(UINT16 cell_mv, UINT16 ichg_a10, UINT16 idsg_a10);

#endif

#include "main.h"
#include "EbikeRideSim.h"

#define EBIKE_RIDE_SIM_PROFILE_AUTO      ((UINT8)0U)
#define EBIKE_RIDE_SIM_PROFILE_CONSTANT  ((UINT8)1U)
#define EBIKE_RIDE_SIM_PROFILE_MANUAL    ((UINT8)2U)
#define EBIKE_RIDE_SIM_UNUSED_CELL_MV    ((UINT16)61001U)

typedef struct _EBIKE_RIDE_SIM_SEGMENT {
    UINT16 u16Seconds;
    UINT16 u16Ichg_A10;
    UINT16 u16IDsg_A10;
    INT16 i16ExtraCellSag_mV;
} EBIKE_RIDE_SIM_SEGMENT;

typedef struct _EBIKE_RIDE_SIM_OCV_POINT {
    UINT8 u8Soc;
    UINT16 u16CellMv;
} EBIKE_RIDE_SIM_OCV_POINT;

EBIKE_RIDE_SIM_OBSERVE g_stEbikeRideSimObserve;

#if PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE

static const EBIKE_RIDE_SIM_SEGMENT s_stAutoProfile[] = {
    { 60U, 0U,   0U,   0 },
    { 10U, 0U, 300U, -40 },
    {300U, 0U,  80U, -10 },
    { 60U, 0U, 250U, -35 },
    {120U, 0U,  25U,   8 },
    { 90U, 0U,   0U,  20 },
    { 60U, 30U,  0U,  15 },
};

static const EBIKE_RIDE_SIM_SEGMENT s_stConstantProfile[] = {
    {600U, 0U, 80U, -10 },
};

#ifndef LIFEPO
static const EBIKE_RIDE_SIM_OCV_POINT s_stTernaryOcv[] = {
    {  0U, 3000U },
    {  5U, 3300U },
    { 10U, 3450U },
    { 20U, 3600U },
    { 30U, 3660U },
    { 40U, 3710U },
    { 50U, 3760U },
    { 60U, 3820U },
    { 70U, 3890U },
    { 80U, 3970U },
    { 90U, 4070U },
    {100U, 4180U },
};
#endif

#ifdef LIFEPO
static const EBIKE_RIDE_SIM_OCV_POINT s_stLfpOcv[] = {
    {  0U, 2500U },
    {  5U, 3000U },
    { 10U, 3150U },
    { 20U, 3240U },
    { 30U, 3290U },
    { 40U, 3310U },
    { 50U, 3320U },
    { 60U, 3330U },
    { 70U, 3340U },
    { 80U, 3380U },
    { 90U, 3450U },
    {100U, 3600U },
};
#endif

static UINT8 s_u8Initialized;
static UINT16 s_u16ManualCellMv;
static UINT16 s_u16ManualIchgA10;
static UINT16 s_u16ManualIDsgA10;

static UINT32 EbikeRideSim_GetCapacityFullAs10(void)
{
    UINT32 cap_full_as10;

    cap_full_as10 = (UINT32)OtherElement.u16Soc_Ah * 3600U;
    if (cap_full_as10 == 0U)
    {
        cap_full_as10 = 1U;
    }

    return cap_full_as10;
}

static UINT16 EbikeRideSim_LimitCellMv(INT32 cell_mv)
{
    if (cell_mv < 0)
    {
        return 0U;
    }
    if (cell_mv > 5000)
    {
        return 5000U;
    }

    return (UINT16)cell_mv;
}

static UINT16 EbikeRideSim_InterpolateOcv(const EBIKE_RIDE_SIM_OCV_POINT *table,
                                          UINT8 count,
                                          UINT8 soc)
{
    UINT8 i;
    UINT8 soc_low;
    UINT8 soc_high;
    UINT16 mv_low;
    UINT16 mv_high;
    UINT32 span_soc;
    UINT32 span_mv;

    if (soc <= table[0].u8Soc)
    {
        return table[0].u16CellMv;
    }

    for (i = 1U; i < count; ++i)
    {
        if (soc <= table[i].u8Soc)
        {
            soc_low = table[i - 1U].u8Soc;
            soc_high = table[i].u8Soc;
            mv_low = table[i - 1U].u16CellMv;
            mv_high = table[i].u16CellMv;
            span_soc = (UINT32)(soc_high - soc_low);
            span_mv = (UINT32)(mv_high - mv_low);
            return (UINT16)(mv_low + ((span_mv * (UINT32)(soc - soc_low)) / span_soc));
        }
    }

    return table[count - 1U].u16CellMv;
}

static UINT16 EbikeRideSim_GetOcvMv(UINT8 soc)
{
#ifdef LIFEPO
    return EbikeRideSim_InterpolateOcv(s_stLfpOcv,
        (UINT8)(sizeof(s_stLfpOcv) / sizeof(s_stLfpOcv[0])), soc);
#else
    return EbikeRideSim_InterpolateOcv(s_stTernaryOcv,
        (UINT8)(sizeof(s_stTernaryOcv) / sizeof(s_stTernaryOcv[0])), soc);
#endif
}

static const EBIKE_RIDE_SIM_SEGMENT *EbikeRideSim_GetProfile(UINT8 *count)
{
    switch ((UINT8)PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE)
    {
    case EBIKE_RIDE_SIM_PROFILE_CONSTANT:
        *count = (UINT8)(sizeof(s_stConstantProfile) / sizeof(s_stConstantProfile[0]));
        return s_stConstantProfile;
    case EBIKE_RIDE_SIM_PROFILE_AUTO:
    default:
        *count = (UINT8)(sizeof(s_stAutoProfile) / sizeof(s_stAutoProfile[0]));
        return s_stAutoProfile;
    }
}

static UINT8 EbikeRideSim_CalcTruthSoc(UINT32 cap_now_as10, UINT32 cap_full_as10)
{
    UINT32 soc;

    soc = ((cap_now_as10 * 100U) + (cap_full_as10 / 2U)) / cap_full_as10;
    if (soc > 100U)
    {
        soc = 100U;
    }

    return (UINT8)soc;
}

static void EbikeRideSim_ApplyTruthCapacity(UINT16 period_ms, UINT16 ichg_a10, UINT16 idsg_a10)
{
    UINT32 cap_full_as10;
    UINT32 delta_as10;

    cap_full_as10 = EbikeRideSim_GetCapacityFullAs10();
    delta_as10 = ((UINT32)((ichg_a10 != 0U) ? ichg_a10 : idsg_a10) * (UINT32)period_ms) / 1000U;

    if (ichg_a10 != 0U)
    {
        if ((delta_as10 >= cap_full_as10) ||
            (g_stEbikeRideSimObserve.u32TruthCap_As10 > (cap_full_as10 - delta_as10)))
        {
            g_stEbikeRideSimObserve.u32TruthCap_As10 = cap_full_as10;
        }
        else
        {
            g_stEbikeRideSimObserve.u32TruthCap_As10 += delta_as10;
        }
    }
    else if (idsg_a10 != 0U)
    {
        if (g_stEbikeRideSimObserve.u32TruthCap_As10 > delta_as10)
        {
            g_stEbikeRideSimObserve.u32TruthCap_As10 -= delta_as10;
        }
        else
        {
            g_stEbikeRideSimObserve.u32TruthCap_As10 = 0U;
        }
    }

    g_stEbikeRideSimObserve.u8TruthSoc =
        EbikeRideSim_CalcTruthSoc(g_stEbikeRideSimObserve.u32TruthCap_As10, cap_full_as10);
}

static INT16 EbikeRideSim_GetCellSpread(UINT8 cell_index)
{
    UINT16 window;
    UINT16 range;
    UINT16 phase;

    window = (UINT16)PROJECT_CFG_EBIKE_RIDE_SIM_CELL_IMBALANCE_MV;
    if (window == 0U)
    {
        return 0;
    }

    range = (UINT16)((window * 2U) + 1U);
    phase = (UINT16)((((UINT16)cell_index * 7U) +
        (UINT16)(g_stEbikeRideSimObserve.u32ElapsedMs / 1000U)) % range);
    return (INT16)((INT16)phase - (INT16)window);
}

static void EbikeRideSim_UpdateObserveCurrent(UINT16 ichg_a10, UINT16 idsg_a10)
{
    g_stAfeCurrentObserve.u16Ichg_A10 = ichg_a10;
    g_stAfeCurrentObserve.u16IDsg_A10 = idsg_a10;
    g_stAfeCurrentObserve.u32ChgCurrent_mA = (UINT32)ichg_a10 * 100U;
    g_stAfeCurrentObserve.u32DsgCurrent_mA = (UINT32)idsg_a10 * 100U;
    g_stAfeCurrentObserve.u32Current_mA =
        (ichg_a10 != 0U) ? g_stAfeCurrentObserve.u32ChgCurrent_mA : g_stAfeCurrentObserve.u32DsgCurrent_mA;

    if (ichg_a10 != 0U)
    {
        g_stAfeCurrentObserve.u8Direction = (UINT8)AFE_CURRENT_DIR_CHG;
    }
    else if (idsg_a10 != 0U)
    {
        g_stAfeCurrentObserve.u8Direction = (UINT8)AFE_CURRENT_DIR_DSG;
    }
    else
    {
        g_stAfeCurrentObserve.u8Direction = (UINT8)AFE_CURRENT_DIR_ZERO;
    }
}

static void EbikeRideSim_ApplyCellPack(UINT16 cell_load_mv, UINT16 ichg_a10, UINT16 idsg_a10)
{
    UINT8 i;
    UINT16 cell_mv;
    UINT16 vcell_min;
    UINT16 vcell_max;
    UINT8 vcell_min_pos;
    UINT8 vcell_max_pos;
    UINT32 total_mv;
    INT32 cell_calc_mv;

    vcell_min = 0xFFFFU;
    vcell_max = 0U;
    vcell_min_pos = 0U;
    vcell_max_pos = 0U;
    total_mv = 0U;

    for (i = 0U; i < SeriesNum; ++i)
    {
        cell_calc_mv = (INT32)cell_load_mv + (INT32)EbikeRideSim_GetCellSpread(i);
        cell_mv = EbikeRideSim_LimitCellMv(cell_calc_mv);
        g_stCellInfoReport.u16VCell[i] = cell_mv;
        total_mv += cell_mv;

        if (cell_mv > vcell_max)
        {
            vcell_max = cell_mv;
            vcell_max_pos = i;
        }
        if (cell_mv < vcell_min)
        {
            vcell_min = cell_mv;
            vcell_min_pos = i;
        }
    }

    for (i = SeriesNum; i < 31U; ++i)
    {
        g_stCellInfoReport.u16VCell[i] = EBIKE_RIDE_SIM_UNUSED_CELL_MV;
    }

    g_stCellInfoReport.u16VCellMax = vcell_max;
    g_stCellInfoReport.u16VCellMin = vcell_min;
    g_stCellInfoReport.u16VCellDelta = (UINT16)(vcell_max - vcell_min);
    g_stCellInfoReport.u16VCellMaxPosition = (UINT16)(vcell_max_pos + 1U);
    g_stCellInfoReport.u16VCellMinPosition = (UINT16)(vcell_min_pos + 1U);
    g_stCellInfoReport.u16VCellTotle = (UINT16)((total_mv + 5U) / 10U);
    g_stCellInfoReport.u16Ichg = ichg_a10;
    g_stCellInfoReport.u16IDischg = idsg_a10;

    g_stEbikeRideSimObserve.u16VCellMin_mV = vcell_min;
    g_stEbikeRideSimObserve.u16VCellMax_mV = vcell_max;
    g_stEbikeRideSimObserve.u16CellLoad_mV = cell_load_mv;
    g_stEbikeRideSimObserve.u16Ichg_A10 = ichg_a10;
    g_stEbikeRideSimObserve.u16IDsg_A10 = idsg_a10;

    EbikeRideSim_UpdateObserveCurrent(ichg_a10, idsg_a10);
}

static void EbikeRideSim_ApplyManualProfile(UINT16 period_ms)
{
    UINT16 ichg_a10;
    UINT16 idsg_a10;

    ichg_a10 = s_u16ManualIchgA10;
    idsg_a10 = s_u16ManualIDsgA10;
    if ((ichg_a10 != 0U) && (idsg_a10 != 0U))
    {
        idsg_a10 = 0U;
    }

    EbikeRideSim_ApplyTruthCapacity(period_ms, ichg_a10, idsg_a10);
    g_stEbikeRideSimObserve.u16CellOcv_mV = EbikeRideSim_GetOcvMv(g_stEbikeRideSimObserve.u8TruthSoc);
    EbikeRideSim_ApplyCellPack(s_u16ManualCellMv, ichg_a10, idsg_a10);
}

static void EbikeRideSim_SelectNextSegment(UINT16 period_ms)
{
    UINT8 profile_count;
    const EBIKE_RIDE_SIM_SEGMENT *profile;
    UINT32 segment_ms;

    profile = EbikeRideSim_GetProfile(&profile_count);
    g_stEbikeRideSimObserve.u32SegmentElapsedMs += period_ms;
    segment_ms = (UINT32)profile[g_stEbikeRideSimObserve.u8SegmentIndex].u16Seconds * 1000U;

    if ((segment_ms == 0U) || (g_stEbikeRideSimObserve.u32SegmentElapsedMs < segment_ms))
    {
        return;
    }

    g_stEbikeRideSimObserve.u32SegmentElapsedMs = 0U;
    ++g_stEbikeRideSimObserve.u8SegmentIndex;
    if (g_stEbikeRideSimObserve.u8SegmentIndex >= profile_count)
    {
        g_stEbikeRideSimObserve.u8SegmentIndex = 0U;
    }
}

static void EbikeRideSim_ApplyAutoProfile(UINT16 period_ms)
{
    UINT8 profile_count;
    const EBIKE_RIDE_SIM_SEGMENT *profile;
    const EBIKE_RIDE_SIM_SEGMENT *segment;
    UINT16 ocv_mv;
    UINT16 cell_load_mv;
    INT32 load_mv;
    INT32 sag_mv;

    profile = EbikeRideSim_GetProfile(&profile_count);
    if (g_stEbikeRideSimObserve.u8SegmentIndex >= profile_count)
    {
        g_stEbikeRideSimObserve.u8SegmentIndex = 0U;
    }

    segment = &profile[g_stEbikeRideSimObserve.u8SegmentIndex];
    EbikeRideSim_ApplyTruthCapacity(period_ms, segment->u16Ichg_A10, segment->u16IDsg_A10);

    ocv_mv = EbikeRideSim_GetOcvMv(g_stEbikeRideSimObserve.u8TruthSoc);
    sag_mv = ((INT32)segment->u16IDsg_A10 - (INT32)segment->u16Ichg_A10) *
        (INT32)PROJECT_CFG_EBIKE_RIDE_SIM_CELL_RES_MOHM / 10;
    load_mv = (INT32)ocv_mv - sag_mv + (INT32)segment->i16ExtraCellSag_mV;
    cell_load_mv = EbikeRideSim_LimitCellMv(load_mv);

    g_stEbikeRideSimObserve.u16CellOcv_mV = ocv_mv;
    EbikeRideSim_ApplyCellPack(cell_load_mv, segment->u16Ichg_A10, segment->u16IDsg_A10);
    EbikeRideSim_SelectNextSegment(period_ms);
}

void EbikeRideSim_Reset(UINT8 initial_soc_percent)
{
    UINT32 cap_full_as10;

    if (initial_soc_percent > 100U)
    {
        initial_soc_percent = 100U;
    }

    cap_full_as10 = EbikeRideSim_GetCapacityFullAs10();
    memset(&g_stEbikeRideSimObserve, 0, sizeof(g_stEbikeRideSimObserve));
    g_stEbikeRideSimObserve.u8Enabled = 1U;
    g_stEbikeRideSimObserve.u8Profile = (UINT8)PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE;
    g_stEbikeRideSimObserve.u8TruthSoc = initial_soc_percent;
    g_stEbikeRideSimObserve.u32TruthCap_As10 = ((UINT32)initial_soc_percent * cap_full_as10) / 100U;

    s_u16ManualCellMv = EbikeRideSim_GetOcvMv(initial_soc_percent);
    s_u16ManualIchgA10 = 0U;
    s_u16ManualIDsgA10 = 0U;
    s_u8Initialized = 1U;
}

void EbikeRideSim_SetManualSample(UINT16 cell_mv, UINT16 ichg_a10, UINT16 idsg_a10)
{
    if ((ichg_a10 != 0U) && (idsg_a10 != 0U))
    {
        idsg_a10 = 0U;
    }

    s_u16ManualCellMv = EbikeRideSim_LimitCellMv((INT32)cell_mv);
    s_u16ManualIchgA10 = ichg_a10;
    s_u16ManualIDsgA10 = idsg_a10;
}

void EbikeRideSim_Update(UINT16 period_ms)
{
    if (period_ms == 0U)
    {
        return;
    }

    if (!s_u8Initialized)
    {
        EbikeRideSim_Reset((UINT8)PROJECT_CFG_EBIKE_RIDE_SIM_INITIAL_SOC_PERCENT);
    }

    g_stEbikeRideSimObserve.u8Enabled = 1U;
    g_stEbikeRideSimObserve.u8Profile = (UINT8)PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE;
    g_stEbikeRideSimObserve.u32ElapsedMs += period_ms;

    if ((UINT8)PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE == EBIKE_RIDE_SIM_PROFILE_MANUAL)
    {
        EbikeRideSim_ApplyManualProfile(period_ms);
    }
    else
    {
        EbikeRideSim_ApplyAutoProfile(period_ms);
    }
}

#else

void EbikeRideSim_Reset(UINT8 initial_soc_percent)
{
    (void)initial_soc_percent;
    memset(&g_stEbikeRideSimObserve, 0, sizeof(g_stEbikeRideSimObserve));
}

void EbikeRideSim_SetManualSample(UINT16 cell_mv, UINT16 ichg_a10, UINT16 idsg_a10)
{
    (void)cell_mv;
    (void)ichg_a10;
    (void)idsg_a10;
}

void EbikeRideSim_Update(UINT16 period_ms)
{
    (void)period_ms;
}

#endif

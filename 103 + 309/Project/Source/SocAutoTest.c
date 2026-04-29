#include "main.h"
#include "SocAutoTest.h"
#include "EbikeRideSim.h"

#define SOC_AUTO_TEST_PERIOD_MS             ((UINT16)200U)
#define SOC_AUTO_TEST_REAL_RIDE_TICKS       ((UINT16)4500U)
#define SOC_AUTO_TEST_PASS                  ((UINT8)1U)
#define SOC_AUTO_TEST_FAIL                  ((UINT8)0U)
#define SOC_AUTO_TEST_FAIL_CODE(case_id, reason) ((UINT16)(((UINT16)(case_id) * 100U) + (UINT16)(reason)))

enum SOC_AUTO_TEST_CASE_ID {
    SOC_AUTO_TEST_CASE_GATE_IDLE = 0,
    SOC_AUTO_TEST_CASE_REAL_RIDE_DSG,
    SOC_AUTO_TEST_CASE_DSG_COULOMB,
    SOC_AUTO_TEST_CASE_CHG_CLAMP_99,
    SOC_AUTO_TEST_CASE_FULL_CONFIRM,
    SOC_AUTO_TEST_CASE_LOW_GUARD,
    SOC_AUTO_TEST_CASE_ONLINE_OCV,
    SOC_AUTO_TEST_CASE_RTC_RELAX,
    SOC_AUTO_TEST_CASE_TOTAL
};

SOC_AUTO_TEST_REPORT g_stSocAutoTestReport;

#if PROJECT_CFG_SOC_AUTO_TEST_ENABLE

static UINT8 s_u8Initialized;

static void SocAutoTest_BeginCase(UINT8 soc)
{
    SOC_Test_SetKernelSoc(soc);
    EbikeRideSim_Reset(soc);
    g_stSocAutoTestReport.u16Step = 0U;
    g_stSocAutoTestReport.u8ActualSoc = SOC_Test_GetKernelSoc();
    g_stSocAutoTestReport.u8TruthSoc = soc;
    g_stSocAutoTestReport.u16ObservedVMin_mV = 0xFFFFU;
    g_stSocAutoTestReport.u16ObservedVMax_mV = 0U;
    g_stSocAutoTestReport.u16ObservedIDsgMin_A10 = 0xFFFFU;
    g_stSocAutoTestReport.u16ObservedIDsgMax_A10 = 0U;
}

static UINT16 SocAutoTest_GetV100(void)
{
    return OtherElement.u16Soc_V_100;
}

static UINT16 SocAutoTest_GetV0(void)
{
    return OtherElement.u16Soc_V_0;
}

static UINT16 SocAutoTest_GetOnlineHighOcv(void)
{
#ifdef LIFEPO
    return 3332U;
#else
    return 3888U;
#endif
}

static UINT16 SocAutoTest_GetRelaxLowOcv(void)
{
#ifdef LIFEPO
    return 3240U;
#else
    return 3600U;
#endif
}

static void SocAutoTest_Feed(UINT16 vmin_mv, UINT16 vmax_mv, UINT16 ichg_a10, UINT16 idsg_a10)
{
    g_stSocAutoTestReport.u16LastVMin_mV = vmin_mv;
    g_stSocAutoTestReport.u16LastVMax_mV = vmax_mv;
    g_stSocAutoTestReport.u16LastIchg_A10 = ichg_a10;
    g_stSocAutoTestReport.u16LastIDsg_A10 = idsg_a10;
    g_stSocAutoTestReport.u8TruthSoc = g_stEbikeRideSimObserve.u8TruthSoc;

    if (vmin_mv < g_stSocAutoTestReport.u16ObservedVMin_mV)
    {
        g_stSocAutoTestReport.u16ObservedVMin_mV = vmin_mv;
    }
    if (vmax_mv > g_stSocAutoTestReport.u16ObservedVMax_mV)
    {
        g_stSocAutoTestReport.u16ObservedVMax_mV = vmax_mv;
    }
    if (idsg_a10 < g_stSocAutoTestReport.u16ObservedIDsgMin_A10)
    {
        g_stSocAutoTestReport.u16ObservedIDsgMin_A10 = idsg_a10;
    }
    if (idsg_a10 > g_stSocAutoTestReport.u16ObservedIDsgMax_A10)
    {
        g_stSocAutoTestReport.u16ObservedIDsgMax_A10 = idsg_a10;
    }

    EbikeRideSim_SetManualSample(vmin_mv, ichg_a10, idsg_a10);
    SOC_UpdateSampleData(vmax_mv, vmin_mv, ichg_a10, idsg_a10);
    SOC_IntEnhance_Ctrl();
    g_stSocAutoTestReport.u8ActualSoc = SOC_Test_GetKernelSoc();
}

static void SocAutoTest_FeedRideSim(void)
{
    EbikeRideSim_Update(SOC_AUTO_TEST_PERIOD_MS);
    SocAutoTest_Feed(g_stCellInfoReport.u16VCellMin,
                     g_stCellInfoReport.u16VCellMax,
                     g_stCellInfoReport.u16Ichg,
                     g_stCellInfoReport.u16IDischg);
}

static UINT8 SocAutoTest_ExpectSocRange(UINT8 min_soc, UINT8 max_soc, UINT16 fail_code)
{
    UINT8 soc;

    soc = SOC_Test_GetKernelSoc();
    g_stSocAutoTestReport.u8ActualSoc = soc;
    g_stSocAutoTestReport.u8ExpectedMinSoc = min_soc;
    g_stSocAutoTestReport.u8ExpectedMaxSoc = max_soc;

    if ((soc < min_soc) || (soc > max_soc))
    {
        g_stSocAutoTestReport.u16FailCode = fail_code;
        return SOC_AUTO_TEST_FAIL;
    }

    return SOC_AUTO_TEST_PASS;
}

static UINT8 SocAutoTest_ExpectNearTruth(UINT8 max_error, UINT16 fail_code)
{
    UINT8 truth_soc;
    UINT8 min_soc;
    UINT8 max_soc;

    truth_soc = g_stEbikeRideSimObserve.u8TruthSoc;
    min_soc = (truth_soc > max_error) ? (UINT8)(truth_soc - max_error) : 0U;
    max_soc = ((UINT8)(100U - truth_soc) > max_error) ? (UINT8)(truth_soc + max_error) : 100U;
    return SocAutoTest_ExpectSocRange(min_soc, max_soc, fail_code);
}

static void SocAutoTest_RecordResult(UINT8 passed)
{
    if (passed)
    {
        ++g_stSocAutoTestReport.u16CasePassed;
    }
    else
    {
        ++g_stSocAutoTestReport.u16CaseFailed;
    }

    ++g_stSocAutoTestReport.u16CaseIndex;
    g_stSocAutoTestReport.u16Step = 0U;
}

static UINT8 SocAutoTest_RunGateIdle(void)
{
    if (g_stSocAutoTestReport.u16Step == 0U)
    {
        SocAutoTest_BeginCase(50U);
    }

    SocAutoTest_Feed(3700U, 3706U, 0U, 3U);
    ++g_stSocAutoTestReport.u16Step;

    if (g_stSocAutoTestReport.u16Step >= 20U)
    {
        return SocAutoTest_ExpectSocRange(50U, 50U,
            SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_GATE_IDLE, 1U));
    }

    return SOC_AUTO_TEST_PASS;
}

static UINT8 SocAutoTest_RunDsgCoulomb(void)
{
    if (g_stSocAutoTestReport.u16Step == 0U)
    {
        SocAutoTest_BeginCase(80U);
    }

    SocAutoTest_Feed(3700U, 3708U, 0U, 2700U);
    ++g_stSocAutoTestReport.u16Step;

    if (g_stSocAutoTestReport.u16Step >= 100U)
    {
        return SocAutoTest_ExpectSocRange(74U, 76U,
            SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_DSG_COULOMB, 1U));
    }

    return SOC_AUTO_TEST_PASS;
}

static UINT8 SocAutoTest_RunRealRideDischarge(void)
{
    if (g_stSocAutoTestReport.u16Step == 0U)
    {
        SocAutoTest_BeginCase(80U);
    }

    SocAutoTest_FeedRideSim();
    ++g_stSocAutoTestReport.u16Step;

    if (g_stSocAutoTestReport.u16Step >= SOC_AUTO_TEST_REAL_RIDE_TICKS)
    {
        if ((UINT16)(g_stSocAutoTestReport.u16ObservedIDsgMax_A10 -
            g_stSocAutoTestReport.u16ObservedIDsgMin_A10) < 80U)
        {
            g_stSocAutoTestReport.u16FailCode =
                SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_REAL_RIDE_DSG, 1U);
            return SOC_AUTO_TEST_FAIL;
        }
        if ((UINT16)(g_stSocAutoTestReport.u16ObservedVMax_mV -
            g_stSocAutoTestReport.u16ObservedVMin_mV) < 60U)
        {
            g_stSocAutoTestReport.u16FailCode =
                SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_REAL_RIDE_DSG, 2U);
            return SOC_AUTO_TEST_FAIL;
        }
        if (g_stEbikeRideSimObserve.u8TruthSoc >= 78U)
        {
            g_stSocAutoTestReport.u16FailCode =
                SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_REAL_RIDE_DSG, 3U);
            return SOC_AUTO_TEST_FAIL;
        }

        return SocAutoTest_ExpectNearTruth(3U,
            SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_REAL_RIDE_DSG, 4U));
    }

    return SOC_AUTO_TEST_PASS;
}

static UINT8 SocAutoTest_RunChgClamp99(void)
{
    if (g_stSocAutoTestReport.u16Step == 0U)
    {
        SocAutoTest_BeginCase(98U);
    }

    SocAutoTest_Feed(3700U, 3708U, 2700U, 0U);
    ++g_stSocAutoTestReport.u16Step;

    if (g_stSocAutoTestReport.u16Step >= 50U)
    {
        return SocAutoTest_ExpectSocRange(99U, 99U,
            SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_CHG_CLAMP_99, 1U));
    }

    return SOC_AUTO_TEST_PASS;
}

static UINT8 SocAutoTest_RunFullConfirm(void)
{
    UINT16 v100;
    UINT16 vmin;

    if (g_stSocAutoTestReport.u16Step == 0U)
    {
        SocAutoTest_BeginCase(99U);
    }

    v100 = SocAutoTest_GetV100();
    vmin = (v100 > 80U) ? (UINT16)(v100 - 70U) : v100;
    SocAutoTest_Feed(vmin, (UINT16)(v100 + 10U), 10U, 0U);
    ++g_stSocAutoTestReport.u16Step;

    if (g_stSocAutoTestReport.u16Step >= 310U)
    {
        return SocAutoTest_ExpectSocRange(100U, 100U,
            SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_FULL_CONFIRM, 1U));
    }

    return SOC_AUTO_TEST_PASS;
}

static UINT8 SocAutoTest_RunLowGuard(void)
{
    UINT16 v0;

    if (g_stSocAutoTestReport.u16Step == 0U)
    {
        SocAutoTest_BeginCase(60U);
    }

    v0 = SocAutoTest_GetV0();
    SocAutoTest_Feed(v0, (UINT16)(v0 + 10U), 0U, 0U);
    ++g_stSocAutoTestReport.u16Step;

    if (g_stSocAutoTestReport.u16Step >= 15U)
    {
        return SocAutoTest_ExpectSocRange(0U, 0U,
            SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_LOW_GUARD, 1U));
    }

    return SOC_AUTO_TEST_PASS;
}

static UINT8 SocAutoTest_RunOnlineOcv(void)
{
    UINT16 vcell;

    if (g_stSocAutoTestReport.u16Step == 0U)
    {
        SocAutoTest_BeginCase(50U);
    }

    vcell = SocAutoTest_GetOnlineHighOcv();
    SocAutoTest_Feed(vcell, (UINT16)(vcell + 6U), 10U, 0U);
    ++g_stSocAutoTestReport.u16Step;

    if (g_stSocAutoTestReport.u16Step >= 270U)
    {
        return SocAutoTest_ExpectSocRange(51U, 55U,
            SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_ONLINE_OCV, 1U));
    }

    return SOC_AUTO_TEST_PASS;
}

static UINT8 SocAutoTest_RunRtcRelax(void)
{
    UINT16 vcell;

    if (g_stSocAutoTestReport.u16Step == 0U)
    {
        SocAutoTest_BeginCase(80U);
        vcell = SocAutoTest_GetRelaxLowOcv();
        SOC_ApplyRtcRelaxationCompensation(3600U, vcell, (UINT16)(vcell + 6U));
        ++g_stSocAutoTestReport.u16Step;
    }

    return SocAutoTest_ExpectSocRange(78U, 79U,
        SOC_AUTO_TEST_FAIL_CODE(SOC_AUTO_TEST_CASE_RTC_RELAX, 1U));
}

static UINT8 SocAutoTest_RunCurrentCase(void)
{
    switch (g_stSocAutoTestReport.u16CaseIndex)
    {
    case SOC_AUTO_TEST_CASE_GATE_IDLE:
        return SocAutoTest_RunGateIdle();
    case SOC_AUTO_TEST_CASE_REAL_RIDE_DSG:
        return SocAutoTest_RunRealRideDischarge();
    case SOC_AUTO_TEST_CASE_DSG_COULOMB:
        return SocAutoTest_RunDsgCoulomb();
    case SOC_AUTO_TEST_CASE_CHG_CLAMP_99:
        return SocAutoTest_RunChgClamp99();
    case SOC_AUTO_TEST_CASE_FULL_CONFIRM:
        return SocAutoTest_RunFullConfirm();
    case SOC_AUTO_TEST_CASE_LOW_GUARD:
        return SocAutoTest_RunLowGuard();
    case SOC_AUTO_TEST_CASE_ONLINE_OCV:
        return SocAutoTest_RunOnlineOcv();
    case SOC_AUTO_TEST_CASE_RTC_RELAX:
        return SocAutoTest_RunRtcRelax();
    default:
        return SOC_AUTO_TEST_PASS;
    }
}

void SocAutoTest_Reset(void)
{
    memset(&g_stSocAutoTestReport, 0, sizeof(g_stSocAutoTestReport));
    g_stSocAutoTestReport.u8Enabled = 1U;
    g_stSocAutoTestReport.u8Running = 1U;
    g_stSocAutoTestReport.u16CaseTotal = (UINT16)SOC_AUTO_TEST_CASE_TOTAL;
    s_u8Initialized = 1U;
}

void SocAutoTest_Task(void)
{
    UINT8 result;
    UINT16 ticks_to_run;

    if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag)
    {
        return;
    }

    if (!s_u8Initialized)
    {
        SocAutoTest_Reset();
    }

    if (g_stSocAutoTestReport.u8Done)
    {
        return;
    }

    ticks_to_run = (UINT16)PROJECT_CFG_SOC_AUTO_TEST_TICKS_PER_CALL;

    while ((ticks_to_run != 0U) && (!g_stSocAutoTestReport.u8Done))
    {
        --ticks_to_run;

        if (g_stSocAutoTestReport.u16CaseIndex >= (UINT16)SOC_AUTO_TEST_CASE_TOTAL)
        {
            g_stSocAutoTestReport.u8Running = 0U;
            g_stSocAutoTestReport.u8Done = 1U;
            g_stSocAutoTestReport.u8Passed =
                (g_stSocAutoTestReport.u16CaseFailed == 0U) ? 1U : 0U;
            break;
        }

        ++g_stSocAutoTestReport.u32TickTotal;
        result = SocAutoTest_RunCurrentCase();
        if (result == SOC_AUTO_TEST_FAIL)
        {
            SocAutoTest_RecordResult(0U);
        }
        else
        {
            switch (g_stSocAutoTestReport.u16CaseIndex)
            {
            case SOC_AUTO_TEST_CASE_GATE_IDLE:
                if (g_stSocAutoTestReport.u16Step >= 20U) { SocAutoTest_RecordResult(1U); }
                break;
            case SOC_AUTO_TEST_CASE_REAL_RIDE_DSG:
                if (g_stSocAutoTestReport.u16Step >= SOC_AUTO_TEST_REAL_RIDE_TICKS) { SocAutoTest_RecordResult(1U); }
                break;
            case SOC_AUTO_TEST_CASE_DSG_COULOMB:
                if (g_stSocAutoTestReport.u16Step >= 100U) { SocAutoTest_RecordResult(1U); }
                break;
            case SOC_AUTO_TEST_CASE_CHG_CLAMP_99:
                if (g_stSocAutoTestReport.u16Step >= 50U) { SocAutoTest_RecordResult(1U); }
                break;
            case SOC_AUTO_TEST_CASE_FULL_CONFIRM:
                if (g_stSocAutoTestReport.u16Step >= 310U) { SocAutoTest_RecordResult(1U); }
                break;
            case SOC_AUTO_TEST_CASE_LOW_GUARD:
                if (g_stSocAutoTestReport.u16Step >= 15U) { SocAutoTest_RecordResult(1U); }
                break;
            case SOC_AUTO_TEST_CASE_ONLINE_OCV:
                if (g_stSocAutoTestReport.u16Step >= 270U) { SocAutoTest_RecordResult(1U); }
                break;
            case SOC_AUTO_TEST_CASE_RTC_RELAX:
                if (g_stSocAutoTestReport.u16Step >= 1U) { SocAutoTest_RecordResult(1U); }
                break;
            default:
                break;
            }
        }
    }
}

#else

void SocAutoTest_Reset(void)
{
    memset(&g_stSocAutoTestReport, 0, sizeof(g_stSocAutoTestReport));
}

void SocAutoTest_Task(void)
{
}

#endif

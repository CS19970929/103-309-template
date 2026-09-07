#include "main.h"
#include "afe3520/BmsProtection3520.h"

struct Afe3520Measurements g_afe3520Measurements;

static INT16 Afe3520_NativeToCalibratedCode(INT16 nativeRaw)
{
    /*
     * Generic DataDeal.c still applies the historical current calibration:
     * I = raw * 20 * (CS_Res_Num*1000/CS_Res) / 2147.
     * Reference board CADC conversion:
     * I_mA = native * 100 * (CS_Res_Num*1000/CS_Res) / 29127.
     * Equating both gives legacyRaw = nativeRaw * 10735 / 29127.
     * This adapter lets the existing calibrated SOC/current pipeline continue
     * while native SH3673520 CADC remains available inside Afe3520_SNAPSHOT.
     */
    return (INT16)(((INT32)nativeRaw * 10735L) / 29127L);
}

UINT8 Afe3520_ReadCalibratedCurrentCode(UINT16 *code)
{
    const AFE3520_SNAPSHOT *snap = Afe3520_GetSnapshot();
    if (code == 0) return 0U;
    if (Afe3520_Service() != AFE3520_OK) return 0U;
    snap = Afe3520_GetSnapshot();
    *code = (UINT16)Afe3520_NativeToCalibratedCode((INT16)snap->cadcRaw);
    return 1U;
}

void Afe3520_RestorePort(void)
{
    /* STOP entry converts most GPIO to analog. Rebuild SPI pins on every wake.
     * Mark config dirty because a concurrent AFE reset/WDT recovery may also
     * have restored RAM defaults while the MCU was asleep. */
    Afe3520_PortInit();
    Afe3520_Invalidate();
}

void Afe3520_AppInit(void)
{
    Bms3520_ProtectionInit();

    if (Afe3520_Init() != AFE3520_OK)
    {
        SystemRuntime_SetAfeStatus(0U, 0U);
        System_ERROR_UserCallback(ERROR_AFE1);
        return;
    }

    if (!Bms3520_ApplyAndVerifyAfeConfig())
    {
        SystemRuntime_SetAfeStatus(0U, 0U);
        System_ERROR_UserCallback(ERROR_AFE1);
        return;
    }

    SystemRuntime_SetAfeStatus(0U, 1U);
    System_ERROR_UserCallback(ERROR_REMOVE_AFE1);
}

UINT8 Afe3520_UpdateMeasurements(void)
{
    /* MonitorAFE and the RTC adapter use 0=success, nonzero=failure. */
    const AFE3520_SNAPSHOT *snap;
    UINT8 i;
    INT32 tempEncoded;
    INT16 proxy;

    if (Afe3520_Service() != AFE3520_OK)
    {
        SystemRuntime_SetAfeStatus(0U, 0U);
        return 1U;
    }

    snap = Afe3520_GetSnapshot();
    if (!snap->valid) return 1U;

    for (i = 0U; i < AFE3520_CELL_MAX; ++i)
    {
        g_afe3520Measurements.u16VCell[i] = snap->cellMv[i];
    }

    for (i = 0U; i < AFE3520_TEMP_MAX; ++i)
    {
        /* Generic report convention: (degC + 40) * 10. */
        tempEncoded = (INT32)snap->tempDeciC[i] + 400L;
        if (tempEncoded < 0L) tempEncoded = 0L;
        if (tempEncoded > 2000L) tempEncoded = 2000L;
        g_afe3520Measurements.u16TempBat[i] = (UINT16)tempEncoded;
    }

    proxy = Afe3520_NativeToCalibratedCode((INT16)snap->cadcRaw);
    g_afe3520Measurements.u16Current = (UINT16)proxy;

    return 0U;
}

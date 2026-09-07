#include "main.h"
#include "rtc_sleep_afe_port.h"

void DataLoad_CellVolt(void);
void DataLoad_CellVoltMaxMinFind(void);
void DataLoad_Temperature(void);
void DataLoad_TemperatureMaxMinFind(void);
void DataLoad_Current(void);

UINT8 RtcSleep_AfePortUpdateRtcData(void)
{
    if (Afe3520_UpdateMeasurements())
    {
        Bms3520_HandleCommFault();
        return 0U;
    }
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    return 1U;
}

UINT8 RtcSleep_AfePortHasCurrentWake(enum irqWakeup *source)
{
    UINT8 result;

    if (source != 0)
    {
        *source = NO_IRQ;
    }

    DataLoad_Current();

    result = (UINT8)((g_stCellInfoReport.u16Ichg != 0U) ||
                     (g_stCellInfoReport.u16IDischg != 0U));
    if (result != 0U)
    {
        if (source != 0)
        {
            *source = current_wake;
        }
    }

    return result;
}

UINT8 RtcSleep_AfePortHasAfeWake(enum irqWakeup *source)
{
    if (source != 0) *source = NO_IRQ;
    /* Read native SH3673520 flags and actual MOS feedback. */
    Bms3520_Service200ms();
    if (!Afe3520_GetSnapshot()->valid || !Bms3520_GetProtectionStatus()->mosFeedbackValid ||
        Bms3520_GetBlockMask() != 0U)
    {
        if (source != 0) *source = error_wake;
        return 1U;
    }
    return 0U;
}

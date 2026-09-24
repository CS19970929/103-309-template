#include "main.h"
#include "rtc_sleep_afe_port.h"
#include "rtc_sleep_port.h"

#ifndef RTC_SLEEP_CURRENT_WAKE_THRESHOLD_MA
#define RTC_SLEEP_CURRENT_WAKE_THRESHOLD_MA 500U
#endif

#if (RTC_SLEEP_CURRENT_WAKE_THRESHOLD_MA == 0U) || (RTC_SLEEP_CURRENT_WAKE_THRESHOLD_MA > 6553500U)
#error RTC_SLEEP_CURRENT_WAKE_THRESHOLD_MA must be within 1..6553500 mA
#endif

/* Public current reports use 100 mA per count; round the threshold up. */
#define RTC_SLEEP_CURRENT_WAKE_THRESHOLD_A10 \
    ((RTC_SLEEP_CURRENT_WAKE_THRESHOLD_MA + 99U) / 100U)

void Fault_ChangeToMCU(void);
void DataLoad_CellVolt(void);
void DataLoad_CellVoltMaxMinFind(void);
void DataLoad_Temperature(void);
void DataLoad_TemperatureMaxMinFind(void);
void DataLoad_Current(void);

UINT8 RtcSleep_AfePortUpdateRtcData(void)
{
    if (UpdateVoltageFromBqMaximo())
    {
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

    result = (UINT8)((RtcSleep_PortGetChargeCurrentMa() >= RTC_SLEEP_CURRENT_WAKE_THRESHOLD_A10) ||
                     (RtcSleep_PortGetDischargeCurrentMa() >= RTC_SLEEP_CURRENT_WAKE_THRESHOLD_A10));
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
    if (source != 0)
    {
        *source = NO_IRQ;
    }

    if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
    {
        SystemRuntime_SetMosStatus(SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET,
                                   SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET);
        Fault_ChangeToMCU();

        // if (!SystemRuntime_IsDischargeMosOpen())
        // {
        //     if (source != 0)
        //     {
        //         *source = chg_dsg_close;
        //     }
        //     return 1U;
        // }

        // if (g_stCellInfoReport.unMdlFault_Third.all != 0U)
        // {
        //     if (source != 0)
        //     {
        //         *source = error_wake;
        //     }
        //     return 1U;
        // }
    }
    return 0U;
}

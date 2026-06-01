#include "main.h"
#include "rtc_sleep_afe_port.h"

void Fault_ChangeToMCU(void);
void DataLoad_CellVolt(void);
void DataLoad_CellVoltMaxMinFind(void);
void DataLoad_Temperature(void);
void DataLoad_TemperatureMaxMinFind(void);
void DataLoad_Current(void);

UINT8 RtcSleep_AfePortIsSleepBlocked(void)
{
    if (MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all))
    {
        if (SH367309_Reg_Store.REG_BSTATUS1.all ||
            SH367309_Reg_Store.REG_BSTATUS2.all ||
            SH367309_Reg_Store.REG_BSTATUS3.bits.L0V ||
            SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET)
        {
            return 1U;
        }
        return 0U;
    }

    return 2U;
}

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
    if (source != 0)
    {
        *source = NO_IRQ;
    }

    if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
    {
        SystemRuntime_SetMosStatus(SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET,
                                   SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET);
        Fault_ChangeToMCU();

        if (!SystemRuntime_IsDischargeMosOpen())
        {
            if (source != 0)
            {
                *source = chg_dsg_close;
            }
            return 1U;
        }

        if (g_stCellInfoReport.unMdlFault_Third.all != 0U)
        {
            if (source != 0)
            {
                *source = error_wake;
            }
            return 1U;
        }
    }
    return 0U;
}

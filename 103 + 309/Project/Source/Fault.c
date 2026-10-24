#include "main.h"

struct PRT_E2ROM_PARAS PRT_E2ROMParas;

union FAULT_FLAG_FIRST Fault_Flag_Fisrt;
union FAULT_FLAG_SECOND Fault_Flag_Second;
union FAULT_FLAG_THIRD Fault_Flag_Third;

UINT16 Fault_record_Third[Record_len];

UINT16 Fault_record_Third2[Record_len];

UINT8 FaultPoint_Third;

UINT8 FaultPoint_Third2;

void FaultWarnRecord2(enum FaultFlag num);

void FaultWarnRecord2(enum FaultFlag num)
{
	if (FaultPoint_Third2 >= Record_len)
	{
		FaultPoint_Third2 = 0;
	}
	Fault_record_Third2[FaultPoint_Third2++] = num;
}

/* Only this module changes the software overcurrent latches and recovery time. */
#define FAULT_REVERSE_CURRENT_A10 10U
#define FAULT_CURRENT_RECOVERY_TICKS 3000U
#define FAULT_CURRENT_MAX_GAP_TICKS 50U
static UINT32 s_current_tick;
static UINT16 s_charge_recovery_ticks;
static UINT16 s_discharge_recovery_ticks;
static UINT8 s_current_valid;

void Fault_PauseMeasurements(void)
{
    /* STOP, reset and failed reads do not acknowledge any confirmed fault. */
    s_current_valid = 0U;
}

void Fault_UpdateCurrentProtection(UINT8 sample_valid)
{
    UINT32 now = SysTime_Get10msTickCount();
    UINT32 elapsed = now - s_current_tick;

    if (!sample_valid)
    {
        s_current_valid = 0U;
        return;
    }
    if (!s_current_valid || elapsed > FAULT_CURRENT_MAX_GAP_TICKS)
        elapsed = 0U;
    s_current_tick = now;
    s_current_valid = 1U;

    if (g_stCellInfoReport.u16Ichg >= AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue)
    {
        if (!g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp)
            FaultWarnRecord2(IchgOcp_Second);
        g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = 1U;
        s_charge_recovery_ticks = 0U;
    }
    else if (g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp &&
             g_stCellInfoReport.u16IDischg < FAULT_REVERSE_CURRENT_A10)
    {
        s_charge_recovery_ticks += (UINT16)elapsed;
        if (s_charge_recovery_ticks >= FAULT_CURRENT_RECOVERY_TICKS)
        {
            g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = 0U;
            s_charge_recovery_ticks = 0U;
        }
    }

    if (g_stCellInfoReport.u16IDischg >= AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue)
    {
        if (!g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp)
            FaultWarnRecord2(IdischgOcp_Second);
        g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = 1U;
        s_discharge_recovery_ticks = 0U;
    }
    else if (g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp &&
             g_stCellInfoReport.u16Ichg < FAULT_REVERSE_CURRENT_A10)
    {
        s_discharge_recovery_ticks += (UINT16)elapsed;
        if (s_discharge_recovery_ticks >= FAULT_CURRENT_RECOVERY_TICKS)
        {
            g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = 0U;
            s_discharge_recovery_ticks = 0U;
        }
    }

#ifdef __SOC_5_PROTECT_
    {
        static UINT32 soc_low_ticks;
        if (g_stCellInfoReport.SocElement.u16Soc > 5U ||
            g_stCellInfoReport.u16Ichg >= FAULT_REVERSE_CURRENT_A10)
        {
            soc_low_ticks = 0U;
            g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow =
                (g_stCellInfoReport.SocElement.u16Soc <= 5U);
        }
        else
        {
            g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = 1U;
            soc_low_ticks += elapsed;
            if (soc_low_ticks >= 360000U)
            {
                soc_low_ticks = 0U;
                LowPower_Request(DEEP_MODE);
            }
        }
    }
#endif
}

UINT8 Fault_ApplyMosPermits(UINT8 charge_on, UINT8 discharge_on)
{
    UINT8 reverse_valid = (UINT8)(s_current_valid &&
        (UINT32)(SysTime_Get10msTickCount() - s_current_tick) <= FAULT_CURRENT_MAX_GAP_TICKS);
    UINT8 charge_block;
    UINT8 discharge_block;

    /* Recovery exceptions belong to these directional faults only. */
    charge_block = (UINT8)((g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp) &&
        !(reverse_valid && g_stCellInfoReport.u16IDischg >= FAULT_REVERSE_CURRENT_A10));
    discharge_block = (UINT8)((g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp ||
        SH367309_Reg_Store.REG_BSTATUS1.bits.SC) &&
        !(reverse_valid && g_stCellInfoReport.u16Ichg >= FAULT_REVERSE_CURRENT_A10));
#ifdef __SOC_5_PROTECT_
    discharge_block |= (UINT8)(g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow &&
        !(reverse_valid && g_stCellInfoReport.u16Ichg >= FAULT_REVERSE_CURRENT_A10));
#endif
    /* A common fault can only add a block; directional recovery cannot erase it. */
    charge_block |= g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp;
    discharge_block |= g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp;
    return SH367309_SetMosPermits((UINT8)(charge_on && !charge_block),
                                 (UINT8)(discharge_on && !discharge_block));
}

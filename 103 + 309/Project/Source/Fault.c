#include "main.h"

struct PRT_E2ROM_PARAS PRT_E2ROMParas;

union FAULT_FLAG_FIRST Fault_Flag_Fisrt;
union FAULT_FLAG_SECOND Fault_Flag_Second;
union FAULT_FLAG_THIRD Fault_Flag_Third;

UINT16 Fault_record_Third[Record_len];

UINT16 Fault_record_Third2[Record_len];

UINT8 FaultPoint_Third;

UINT8 FaultPoint_Third2;

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

#define FAULT_MOS_MAX_GAP_TICKS 30U
#define FAULT_MOS_RECOVERY_TICKS 100U
#define FAULT_MOS_CONFIRM_IDLE 0xFFFFU
#define FAULT_MOS_SENSOR_TIMEOUT_TICKS 500U
#define FAULT_MOS_SENSOR_RECOVER_SAMPLES 3U
static UINT32 s_mos_tick;
static UINT16 s_mos_confirm_ticks = FAULT_MOS_CONFIRM_IDLE;
static UINT8 s_host_mos_off;
static UINT8 s_mos_sensor_missing;
static UINT8 s_mos_sensor_ok_samples;
static UINT32 s_mos_sensor_missing_tick;
static UINT8 s_mos_sensor_fault;
static UINT8 s_pack_sensor_fault;
static UINT8 s_pack_sensor_ok_count;

static void Fault_UpdateTemperatureDiagnostic(void)
{
    SystemError_SetActive(ERROR_TEMP_BREAK,
        (UINT8)(s_mos_sensor_fault || s_pack_sensor_fault));
}

/* 0: valid AFE pack NTCs; 1: I2C failure; 2: invalid pack NTCs. */
void Fault_UpdatePackTemperatureResult(UINT8 sample_result)
{
    if (sample_result == 1U) return; /* No new sensor evidence. */
    if (sample_result == 2U)
    {
        s_pack_sensor_fault = 1U;
        s_pack_sensor_ok_count = 0U;
        close_ctlc();
    }
    else
    {
        if (s_pack_sensor_ok_count < 3U) ++s_pack_sensor_ok_count;
        if (s_pack_sensor_ok_count >= 3U) s_pack_sensor_fault = 0U;
    }
    Fault_UpdateTemperatureDiagnostic();
}

UINT8 Fault_SetHostMosOff(UINT8 mask)
{
    if (mask > 3U)
        return 0U;
    s_host_mos_off = mask;
    /* Keep the requested close on an I2C failure; normal arbitration retries.
     * Releasing the host request never releases a protection latch. */
    if (AFE_PARAM_WRITE_Flag)
        return 0U;
    return Fault_ApplyMosPermits(1U, 1U);
}

void Fault_PauseMeasurements(void)
{
    /* STOP, reset and failed reads do not acknowledge any confirmed fault. */
    s_current_valid = 0U;
    SOC_PauseCalibration();
    Fault_ResetMosConfirmation();
    s_mos_sensor_missing = 0U;
    s_mos_sensor_ok_samples = 0U;
}

UINT8 Fault_IsCurrentSampleFresh(void)
{
    return (UINT8)(s_current_valid &&
        (UINT32)(SysTime_Get10msTickCount()-s_current_tick)<=FAULT_CURRENT_MAX_GAP_TICKS);
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

}

/* All software MOS requests reach this readback-checked hardware writer.
 * Reserved error 21 is used only as an internal MOS-apply diagnostic. */
static UINT8 Fault_WriteMos(UINT8 charge_on, UINT8 discharge_on)
{
    UINT8 ok = SH367309_SetMosPermits(charge_on, discharge_on);
    SystemError_SetActive(ERROR_RESERVED_21, (UINT8)!ok);
    if (!ok && (!charge_on || !discharge_on))
        close_ctlc(); /* CTLC=3 safety fallback, subject to board validation. */
    return ok;
}

UINT8 Fault_CanOpenCtl(void)
{
    return (UINT8)(s_host_mos_off != 3U &&
        !g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp &&
        !SystemError_IsActive(ERROR_STATUS_TEMP_BREAK) &&
        !SystemError_IsActive(ERROR_STATUS_AFE1) &&
        !SH367309_Reg_Store.REG_BSTATUS1.bits.PF);
}

UINT8 Fault_ApplyMosPermits(UINT8 charge_on, UINT8 discharge_on)
{
    UINT8 reverse_valid = (UINT8)(s_current_valid &&
        (UINT32)(SysTime_Get10msTickCount() - s_current_tick) <= FAULT_CURRENT_MAX_GAP_TICKS);
    UINT8 charge_block;
    UINT8 discharge_block;

    if (SystemError_IsActive(ERROR_STATUS_AFE1))
    {
        close_ctlc();
        return 0U;
    }

    /* Common shutdowns must never be released by reverse current. */
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp ||
        SystemError_IsActive(ERROR_STATUS_TEMP_BREAK) ||
        s_host_mos_off == 3U || (!charge_on && !discharge_on))
        return Fault_WriteMos(0U, 0U);

    /* Common-port reverse conduction releases a directional request temporarily;
     * keep the request/latch so it applies again when reverse current stops. */
    charge_block = (UINT8)((!charge_on || (s_host_mos_off & 1U) ||
        g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp) &&
        !(reverse_valid && g_stCellInfoReport.u16IDischg >= FAULT_REVERSE_CURRENT_A10));
    discharge_block = (UINT8)((!discharge_on || (s_host_mos_off & 2U) ||
        g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp ||
        SH367309_Reg_Store.REG_BSTATUS1.bits.SC) &&
        !(reverse_valid && g_stCellInfoReport.u16Ichg >= FAULT_REVERSE_CURRENT_A10));
    return Fault_WriteMos((UINT8)!charge_block, (UINT8)!discharge_block);
}

UINT8 Fault_MosParametersValid(const struct PRT_E2ROM_PARAS *parameters)
{
    return (UINT8)(parameters != 0 &&
        parameters->u16TmosOTp_Third >= 400U && parameters->u16TmosOTp_Third <= 1450U &&
        parameters->u16TmosOTp_Rcv >= 400U &&
        parameters->u16TmosOTp_Rcv < parameters->u16TmosOTp_Third &&
        parameters->u16TmosOTp_Filter >= 1U && parameters->u16TmosOTp_Filter <= 50000U);
}

void Fault_ResetMosConfirmation(void)
{
    s_mos_confirm_ticks = FAULT_MOS_CONFIRM_IDLE;
}

/* Called when no fresh NTC sample is available. Does not reset the
   temperature protection confirmation on normal ADC sampling gaps. */
void Fault_MosSensorWatchdog(void)
{
    UINT32 now = SysTime_Get10msTickCount();
    if (!s_mos_sensor_missing)
    {
        s_mos_sensor_missing = 1U;
        s_mos_sensor_missing_tick = now;
    }
    if ((UINT32)(now - s_mos_sensor_missing_tick) >= FAULT_MOS_SENSOR_TIMEOUT_TICKS)
    {
        s_mos_sensor_fault = 1U;
        Fault_UpdateTemperatureDiagnostic();
        close_ctlc();
    }
}

void Fault_UpdateMosTemperature(UINT8 sample_valid, UINT16 temperature)
{
    UINT32 now = SysTime_Get10msTickCount();
    UINT32 elapsed = now - s_mos_tick;
    UINT16 trip = PRT_E2ROMParas.u16TmosOTp_Third;
    UINT16 recover = PRT_E2ROMParas.u16TmosOTp_Rcv;
    UINT16 delay = PRT_E2ROMParas.u16TmosOTp_Filter;
    UINT8 active = g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp;

    if (!sample_valid)
    {
        s_mos_sensor_ok_samples = 0U;
        Fault_MosSensorWatchdog();
        Fault_ResetMosConfirmation();
        return;
    }
    s_mos_sensor_missing = 0U;
    if (s_mos_sensor_ok_samples < FAULT_MOS_SENSOR_RECOVER_SAMPLES)
        ++s_mos_sensor_ok_samples;
    if (s_mos_sensor_ok_samples >= FAULT_MOS_SENSOR_RECOVER_SAMPLES)
    {
        s_mos_sensor_fault = 0U;
        Fault_UpdateTemperatureDiagnostic();
    }
    /* Keep legacy Flash records intact. Invalid old MOS settings use the
       existing factory trip/recovery/delay locally, without rewriting storage. */
    if (!Fault_MosParametersValid(&PRT_E2ROMParas))
    {
        trip = mos_3;
        recover = mos_recover;
        delay = mos_filter3;
    }
    if (elapsed > FAULT_MOS_MAX_GAP_TICKS)
        Fault_ResetMosConfirmation();
    s_mos_tick = now;

    if ((!active && temperature >= trip) || (active && temperature <= recover))
    {
        if (s_mos_confirm_ticks == FAULT_MOS_CONFIRM_IDLE)
            s_mos_confirm_ticks = 0U; /* First qualifying sample only starts timing. */
        else
            s_mos_confirm_ticks += (UINT16)elapsed;
        if (s_mos_confirm_ticks >= (active ? FAULT_MOS_RECOVERY_TICKS : delay))
        {
            g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp = (UINT8)!active;
            if (!active)
                FaultWarnRecord2(MosOTp_Third);
            Fault_ResetMosConfirmation();
            Fault_ApplyMosPermits(1U, 1U);
        }
    }
    else
    {
        Fault_ResetMosConfirmation();
    }
}

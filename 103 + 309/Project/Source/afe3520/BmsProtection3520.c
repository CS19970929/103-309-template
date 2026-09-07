#include "main.h"
#include "afe3520/BmsProtection3520.h"
#include "SH367309_Func.h"
#include "afe3520/Afe3520Config.h"
#include <string.h>

static BMS3520_PROTECTION_STATUS s_prot;
static uint16_t s_swCnt[8];
static uint16_t s_swRcvCnt[8];
static uint16_t s_hwStableCnt;
static uint8_t s_systemBlock;

SH367309_REG_STORE SH367309_Reg_Store;

static uint16_t Bms3520_MaxCell(void)
{
    uint8_t i;
    uint16_t maxv = 0U;
    for (i = 0U; i < SeriesNum && i < AFE3520_CELL_MAX; ++i)
        if (g_stCellInfoReport.u16VCell[i] > maxv) maxv = g_stCellInfoReport.u16VCell[i];
    return maxv;
}

static uint16_t Bms3520_MinCell(void)
{
    uint8_t i;
    uint16_t minv = 0xFFFFU;
    for (i = 0U; i < SeriesNum && i < AFE3520_CELL_MAX; ++i)
        if (g_stCellInfoReport.u16VCell[i] < minv) minv = g_stCellInfoReport.u16VCell[i];
    return (minv == 0xFFFFU) ? 0U : minv;
}

static uint16_t Bms3520_MaxTempEncoded(void)
{
    uint8_t i;
    uint16_t v = 0U;
    for (i = 0U; i < 2U; ++i)
        if (g_stCellInfoReport.u16Temperature[i] > v) v = g_stCellInfoReport.u16Temperature[i];
    return v;
}

static uint16_t Bms3520_MinTempEncoded(void)
{
    uint8_t i;
    uint16_t v = 0xFFFFU;
    for (i = 0U; i < 2U; ++i)
    {
        uint16_t t = g_stCellInfoReport.u16Temperature[i];
        if ((t != 0U) && (t < v)) v = t;
    }
    return (v == 0xFFFFU) ? 0U : v;
}

static uint16_t Bms3520_FilterTicks(uint16_t filter10ms)
{
    uint32_t ms = (uint32_t)filter10ms * 10UL;
    uint16_t ticks = (uint16_t)((ms + BMS3520_PROTECTION_PERIOD_MS - 1U) / BMS3520_PROTECTION_PERIOD_MS);
    return (ticks == 0U) ? 1U : ticks;
}

uint8_t Bms3520_BuildAfeConfig(AFE3520_REG_CONFIG *cfg)
{
    static const int16_t values[AFE3520_CONFIG_LENGTH] = AFE3520_CONFIG_VALUES;
    uint8_t i;
    if ((cfg == 0) || (SeriesNum < 5U) || (SeriesNum > AFE3520_CELL_MAX)) return 0U;
    memset(cfg, 0, sizeof(*cfg));
    for (i = 0U; i < AFE3520_CONFIG_LENGTH; ++i)
    {
        if (values[i] == AFE3520_CONFIG_KEEP) continue;
        if ((values[i] < 0) || (values[i] > 255)) return 0U;
        cfg->value[i] = (uint8_t)values[i];
        cfg->writeMask |= 1UL << i;
    }
    cfg->value[AFE3520_REG_SCONF4 - AFE3520_REG_SCONF1] = SeriesNum;
    return 1U;
}

uint8_t Bms3520_ApplyAndVerifyAfeConfig(void)
{
    AFE3520_REG_CONFIG cfg;
    if (!Bms3520_BuildAfeConfig(&cfg))
    {
        s_prot.configValid = 0U;
        return 0U;
    }
    GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
    if (Afe3520_ApplyConfig(&cfg) != AFE3520_OK)
    {
        s_prot.configValid = 0U;
        System_ERROR_UserCallback(ERROR_AFE1);
        return 0U;
    }
    s_prot.configValid = 1U;
    System_ERROR_UserCallback(ERROR_REMOVE_AFE1);
    return 1U;
}

static uint8_t Bms3520_FilterHigh(uint16_t value, uint16_t threshold, uint16_t filter10ms,
                                  uint16_t *activeCnt, uint16_t *recoverCnt, uint8_t active,
                                  uint16_t recoverThreshold)
{
    if (!active)
    {
        if (value >= threshold)
        {
            if (++(*activeCnt) >= Bms3520_FilterTicks(filter10ms))
            {
                *activeCnt = 0U;
                *recoverCnt = 0U;
                return 1U;
            }
        }
        else *activeCnt = 0U;
        return 0U;
    }
    if (value <= recoverThreshold)
    {
        if (++(*recoverCnt) >= BMS3520_SW_RECOVERY_STABLE_TICKS)
        {
            *recoverCnt = 0U;
            return 0U;
        }
    }
    else *recoverCnt = 0U;
    return 1U;
}

static uint8_t Bms3520_FilterLow(uint16_t value, uint16_t threshold, uint16_t filter10ms,
                                 uint16_t *activeCnt, uint16_t *recoverCnt, uint8_t active,
                                 uint16_t recoverThreshold)
{
    if (!active)
    {
        if (value <= threshold)
        {
            if (++(*activeCnt) >= Bms3520_FilterTicks(filter10ms))
            {
                *activeCnt = 0U;
                *recoverCnt = 0U;
                return 1U;
            }
        }
        else *activeCnt = 0U;
        return 0U;
    }
    if (value >= recoverThreshold)
    {
        if (++(*recoverCnt) >= BMS3520_SW_RECOVERY_STABLE_TICKS)
        {
            *recoverCnt = 0U;
            return 0U;
        }
    }
    else *recoverCnt = 0U;
    return 1U;
}

static void Bms3520_UpdateSoftwareProtection(void)
{
    uint16_t maxCell = Bms3520_MaxCell();
    uint16_t minCell = Bms3520_MinCell();
    uint16_t maxTemp = Bms3520_MaxTempEncoded();
    uint16_t minTemp = Bms3520_MinTempEncoded();
    uint8_t active;

    active = (s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OV) ? 1U : 0U;
    if (Bms3520_FilterHigh(maxCell, AFE_Parameters_RS485_Struction.u16VcellOvp.curValue,
                           AFE_Parameters_RS485_Struction.u16VcellOvp_Filter.curValue,
                           &s_swCnt[0], &s_swRcvCnt[0], active,
                           AFE_Parameters_RS485_Struction.u16VcellOvp_Rcv.curValue))
        s_prot.chargeBlocks |= AFE3520_BLOCK_CHG_SW_OV;
    else s_prot.chargeBlocks &= ~AFE3520_BLOCK_CHG_SW_OV;

    active = (s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_UV) ? 1U : 0U;
    if (Bms3520_FilterLow(minCell, AFE_Parameters_RS485_Struction.u16VcellUvp.curValue,
                          AFE_Parameters_RS485_Struction.u16VcellUvp_Filter.curValue,
                          &s_swCnt[1], &s_swRcvCnt[1], active,
                          AFE_Parameters_RS485_Struction.u16VcellUvp_Rcv.curValue))
        s_prot.dischargeBlocks |= AFE3520_BLOCK_DSG_SW_UV;
    else s_prot.dischargeBlocks &= ~AFE3520_BLOCK_DSG_SW_UV;

    active = (s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OCP) ? 1U : 0U;
    if (Bms3520_FilterHigh(g_stCellInfoReport.u16Ichg,
                           AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue,
                           AFE_Parameters_RS485_Struction.u16IchgOcp_Filter_First.curValue,
                           &s_swCnt[2], &s_swRcvCnt[2], active,
                           (uint16_t)(AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue * 8U / 10U)))
        s_prot.chargeBlocks |= AFE3520_BLOCK_CHG_SW_OCP;
    else s_prot.chargeBlocks &= ~AFE3520_BLOCK_CHG_SW_OCP;

    active = (s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_OCP) ? 1U : 0U;
    if (Bms3520_FilterHigh(g_stCellInfoReport.u16IDischg,
                           AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue,
                           AFE_Parameters_RS485_Struction.u16IdsgOcp_Filter_First.curValue,
                           &s_swCnt[3], &s_swRcvCnt[3], active,
                           (uint16_t)(AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue * 8U / 10U)))
        s_prot.dischargeBlocks |= AFE3520_BLOCK_DSG_SW_OCP;
    else s_prot.dischargeBlocks &= ~AFE3520_BLOCK_DSG_SW_OCP;

    active = (s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_TEMP) ? 1U : 0U;
    if (Bms3520_FilterHigh(maxTemp, AFE_Parameters_RS485_Struction.u16TChgOTp.curValue, 1U,
                           &s_swCnt[4], &s_swRcvCnt[4], active,
                           AFE_Parameters_RS485_Struction.u16TChgOTp_Rcv.curValue) ||
        Bms3520_FilterLow(minTemp, AFE_Parameters_RS485_Struction.u16TchgUTp.curValue, 1U,
                          &s_swCnt[5], &s_swRcvCnt[5], active,
                          AFE_Parameters_RS485_Struction.u16TchgUTp_Rcv.curValue))
        s_prot.chargeBlocks |= AFE3520_BLOCK_CHG_SW_TEMP;
    else s_prot.chargeBlocks &= ~AFE3520_BLOCK_CHG_SW_TEMP;

    active = (s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_TEMP) ? 1U : 0U;
    if (Bms3520_FilterHigh(maxTemp, AFE_Parameters_RS485_Struction.u16TdischgOTp.curValue, 1U,
                           &s_swCnt[6], &s_swRcvCnt[6], active,
                           AFE_Parameters_RS485_Struction.u16TdischgOTp_Rcv.curValue) ||
        Bms3520_FilterLow(minTemp, AFE_Parameters_RS485_Struction.u16TdischgUTp.curValue, 1U,
                          &s_swCnt[7], &s_swRcvCnt[7], active,
                          AFE_Parameters_RS485_Struction.u16TdischgUTp_Rcv.curValue))
        s_prot.dischargeBlocks |= AFE3520_BLOCK_DSG_SW_TEMP;
    else s_prot.dischargeBlocks &= ~AFE3520_BLOCK_DSG_SW_TEMP;
}

static void Bms3520_UpdateHardwareProtection(const AFE3520_SNAPSHOT *snap)
{
    uint32_t chg = 0U, dsg = 0U, global = 0U;
    if (!snap->valid)
    {
        s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_COMM;
        return;
    }

    if (snap->flag1 & AFE3520_FLAG1_OV) chg |= AFE3520_BLOCK_CHG_HW_OV;
    if (snap->flag1 & AFE3520_FLAG1_OCC) chg |= AFE3520_BLOCK_CHG_HW_OCC;
    if (snap->flag2 & (AFE3520_FLAG2_UTC | AFE3520_FLAG2_OTC)) chg |= AFE3520_BLOCK_CHG_HW_TEMP;
    if (snap->flag1 & AFE3520_FLAG1_UV) dsg |= AFE3520_BLOCK_DSG_HW_UV;
    if (snap->flag1 & (AFE3520_FLAG1_OCD1 | AFE3520_FLAG1_OCD2)) dsg |= AFE3520_BLOCK_DSG_HW_OCD;
    if (snap->flag2 & (AFE3520_FLAG2_UTD | AFE3520_FLAG2_OTD)) dsg |= AFE3520_BLOCK_DSG_HW_TEMP;
    if (snap->flag1 & AFE3520_FLAG1_SC) global |= AFE3520_BLOCK_GLOBAL_SHORT;
    if (snap->flag2 & AFE3520_FLAG2_WDT) global |= AFE3520_BLOCK_GLOBAL_WDT;
    if (snap->internalTempDeciC >= 1050) global |= AFE3520_BLOCK_GLOBAL_INTERNAL_TEMP;

    s_prot.chargeBlocks = (s_prot.chargeBlocks & (AFE3520_BLOCK_CHG_SW_OV | AFE3520_BLOCK_CHG_SW_OCP | AFE3520_BLOCK_CHG_SW_TEMP)) | chg;
    s_prot.dischargeBlocks = (s_prot.dischargeBlocks & (AFE3520_BLOCK_DSG_SW_UV | AFE3520_BLOCK_DSG_SW_OCP | AFE3520_BLOCK_DSG_SW_TEMP)) | dsg;
    s_prot.globalBlocks &= ~(AFE3520_BLOCK_GLOBAL_SHORT | AFE3520_BLOCK_GLOBAL_WDT |
                             AFE3520_BLOCK_GLOBAL_OPEN_WIRE | AFE3520_BLOCK_GLOBAL_INTERNAL_TEMP |
                             AFE3520_BLOCK_GLOBAL_AFE_COMM);
    s_prot.globalBlocks |= global;
    if (s_systemBlock) s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_SYSTEM;
    else s_prot.globalBlocks &= ~AFE3520_BLOCK_GLOBAL_SYSTEM;
    if (!s_prot.configValid) s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
    else s_prot.globalBlocks &= ~AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
    s_prot.latchedHardware = chg | dsg | global;
}

static uint8_t Bms3520_HardwareRecoverySafe(const AFE3520_SNAPSHOT *snap)
{
    uint16_t maxCell = Bms3520_MaxCell();
    uint16_t minCell = Bms3520_MinCell();
    uint16_t maxTemp = Bms3520_MaxTempEncoded();
    uint16_t minTemp = Bms3520_MinTempEncoded();
    (void)snap;
    if (maxCell > AFE_Parameters_RS485_Struction.u16VcellOvp_Rcv.curValue) return 0U;
    if ((minCell != 0U) && (minCell < AFE_Parameters_RS485_Struction.u16VcellUvp_Rcv.curValue)) return 0U;
    if (g_stCellInfoReport.u16Ichg >= BMS3520_REVERSE_CURRENT_A10) return 0U;
    if (g_stCellInfoReport.u16IDischg >= BMS3520_REVERSE_CURRENT_A10) return 0U;
    if (maxTemp > AFE_Parameters_RS485_Struction.u16TdischgOTp_Rcv.curValue) return 0U;
    if ((minTemp != 0U) && (minTemp < AFE_Parameters_RS485_Struction.u16TdischgUTp_Rcv.curValue)) return 0U;
    return 1U;
}

static void Bms3520_TryRecoverHardware(const AFE3520_SNAPSHOT *snap)
{
    uint8_t clear1 = 0U;
    uint8_t clear2 = 0U;
    if ((snap->flag1 & 0x3FU) == 0U && (snap->flag2 & 0xFCU) == 0U) { s_hwStableCnt = 0U; return; }
    if (!Bms3520_HardwareRecoverySafe(snap)) { s_hwStableCnt = 0U; return; }
    if (++s_hwStableCnt < BMS3520_HW_RECOVERY_STABLE_TICKS) return;
    s_hwStableCnt = 0U;

    if (snap->flag1 & AFE3520_FLAG1_OV) clear1 |= AFE3520_FLAG1_OV;
    if (snap->flag1 & AFE3520_FLAG1_UV) clear1 |= AFE3520_FLAG1_UV;
    if (snap->flag1 & AFE3520_FLAG1_OCD1) clear1 |= AFE3520_FLAG1_OCD1;
    if (snap->flag1 & AFE3520_FLAG1_OCD2) clear1 |= AFE3520_FLAG1_OCD2;
    if (snap->flag1 & AFE3520_FLAG1_SC) clear1 |= AFE3520_FLAG1_SC;
    if (snap->flag1 & AFE3520_FLAG1_OCC) clear1 |= AFE3520_FLAG1_OCC;
    if (snap->flag2 & AFE3520_FLAG2_WDT) clear2 |= AFE3520_FLAG2_WDT;
    if (snap->flag2 & (AFE3520_FLAG2_UTC | AFE3520_FLAG2_OTC | AFE3520_FLAG2_UTD | AFE3520_FLAG2_OTD))
        clear2 |= (uint8_t)(snap->flag2 & (AFE3520_FLAG2_UTC | AFE3520_FLAG2_OTC | AFE3520_FLAG2_UTD | AFE3520_FLAG2_OTD));
    (void)Afe3520_ClearFlags(clear1, clear2);
}

static void Bms3520_PublishLegacyFaultView(const AFE3520_SNAPSHOT *snap)
{
    uint8_t chgOvp = ((s_prot.chargeBlocks & (AFE3520_BLOCK_CHG_HW_OV | AFE3520_BLOCK_CHG_SW_OV)) != 0U);
    uint8_t uvp = ((s_prot.dischargeBlocks & (AFE3520_BLOCK_DSG_HW_UV | AFE3520_BLOCK_DSG_SW_UV)) != 0U);
    uint8_t chgOcp = ((s_prot.chargeBlocks & (AFE3520_BLOCK_CHG_HW_OCC | AFE3520_BLOCK_CHG_SW_OCP)) != 0U);
    uint8_t dsgOcp = ((s_prot.dischargeBlocks & (AFE3520_BLOCK_DSG_HW_OCD | AFE3520_BLOCK_DSG_SW_OCP)) != 0U);

    memset(&SH367309_Reg_Store, 0, sizeof(SH367309_Reg_Store));
    SH367309_Reg_Store.REG_BSTATUS1.bits.OV = chgOvp;
    SH367309_Reg_Store.REG_BSTATUS1.bits.UV = uvp;
    SH367309_Reg_Store.REG_BSTATUS1.bits.OCD1 = dsgOcp;
    SH367309_Reg_Store.REG_BSTATUS1.bits.OCD2 = (snap->flag1 & AFE3520_FLAG1_OCD2) ? 1U : 0U;
    SH367309_Reg_Store.REG_BSTATUS1.bits.OCC = chgOcp;
    SH367309_Reg_Store.REG_BSTATUS1.bits.SC = (snap->flag1 & AFE3520_FLAG1_SC) ? 1U : 0U;
    SH367309_Reg_Store.REG_BSTATUS1.bits.WDT = (snap->flag2 & AFE3520_FLAG2_WDT) ? 1U : 0U;
    SH367309_Reg_Store.REG_BSTATUS2.bits.UTC = (s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_TEMP) || (snap->flag2 & AFE3520_FLAG2_UTC);
    SH367309_Reg_Store.REG_BSTATUS2.bits.OTC = (s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_TEMP) || (snap->flag2 & AFE3520_FLAG2_OTC);
    SH367309_Reg_Store.REG_BSTATUS2.bits.UTD = (s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_TEMP) || (snap->flag2 & AFE3520_FLAG2_UTD);
    SH367309_Reg_Store.REG_BSTATUS2.bits.OTD = (s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_TEMP) || (snap->flag2 & AFE3520_FLAG2_OTD);

    g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp = chgOvp;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp = uvp;
    g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp = chgOcp;
    g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp = dsgOcp;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp = SH367309_Reg_Store.REG_BSTATUS2.bits.UTC;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp = SH367309_Reg_Store.REG_BSTATUS2.bits.OTC;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp = SH367309_Reg_Store.REG_BSTATUS2.bits.UTD;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp = SH367309_Reg_Store.REG_BSTATUS2.bits.OTD;
    System_ErrFlag.u8ErrFlag_CBC_DSG = SH367309_Reg_Store.REG_BSTATUS1.bits.SC;
}

static void Bms3520_ApplyMosArbitration(void)
{
    uint8_t charge = s_prot.requestedCharge;
    uint8_t discharge = s_prot.requestedDischarge;
    const AFE3520_SNAPSHOT *snap;
    uint8_t reverseDischarge = (g_stCellInfoReport.u16IDischg >= BMS3520_REVERSE_CURRENT_A10);
    uint8_t reverseCharge = (g_stCellInfoReport.u16Ichg >= BMS3520_REVERSE_CURRENT_A10);

    if ((s_prot.globalBlocks != 0U) || !s_prot.configValid)
    {
        charge = 0U;
        discharge = 0U;
    }
    else
    {
        if ((s_prot.chargeBlocks != 0U) && !reverseDischarge) charge = 0U;
        if ((s_prot.dischargeBlocks != 0U) && !reverseCharge) discharge = 0U;
    }

    /* Reissue commands after every sample/reconfiguration; a successful old
     * command is not proof that hardware still has that state after reset.
     * PB14 follows the charge command, exactly as reference GPIO_M_CCC. */
    if (!charge) GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
    if (Afe3520_SetMos(charge, discharge, 0U) != AFE3520_OK)
    {
        s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_COMM;
        GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
        System_ERROR_UserCallback(ERROR_AFE1);
        return;
    }
    GPIO_WriteBit(GPIO_M_CCC, PIN_M_CCC, charge ? Bit_SET : Bit_RESET);
    snap = Afe3520_GetSnapshot();
    s_prot.actualCharge = ((snap->bstatus1 & AFE3520_BSTATUS1_CHG_FET) &&
                           GPIO_ReadOutputDataBit(GPIO_M_CCC, PIN_M_CCC)) ? 1U : 0U;
    s_prot.actualDischarge = (snap->bstatus1 & AFE3520_BSTATUS1_DSG_FET) ? 1U : 0U;
    SystemRuntime_SetMosStatus(s_prot.actualCharge, s_prot.actualDischarge);
}

void Bms3520_ProtectionInit(void)
{
    memset(&s_prot, 0, sizeof(s_prot));
    memset(s_swCnt, 0, sizeof(s_swCnt));
    memset(s_swRcvCnt, 0, sizeof(s_swRcvCnt));
    s_hwStableCnt = 0U;
    s_prot.requestedCharge = 0U;
    s_prot.requestedDischarge = 0U;
    s_prot.globalBlocks = AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
    GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
}

void Bms3520_ProtectionService(void)
{
    const AFE3520_SNAPSHOT *snap;
    AFE3520_RESULT result;

    result = Afe3520_Service();
    snap = Afe3520_GetSnapshot();
    if (result != AFE3520_OK)
    {
        s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_COMM;
        System_ERROR_UserCallback(ERROR_AFE1);
        Bms3520_ApplyMosArbitration();
        return;
    }
    s_prot.globalBlocks &= ~AFE3520_BLOCK_GLOBAL_AFE_COMM;
    System_ERROR_UserCallback(ERROR_REMOVE_AFE1);

    if (Afe3520_ConfigDirty() || !s_prot.configValid)
    {
        if (!Bms3520_ApplyAndVerifyAfeConfig())
        {
            s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
            Bms3520_ApplyMosArbitration();
            return;
        }
        if (Afe3520_Service() != AFE3520_OK)
        {
            s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_COMM;
            System_ERROR_UserCallback(ERROR_AFE1);
            Bms3520_ApplyMosArbitration();
            return;
        }
    }

    Bms3520_UpdateSoftwareProtection();
    Bms3520_UpdateHardwareProtection(snap);
    Bms3520_TryRecoverHardware(snap);
    Bms3520_PublishLegacyFaultView(snap);

    s_prot.activeAll = s_prot.chargeBlocks | s_prot.dischargeBlocks | s_prot.globalBlocks;
    Bms3520_ApplyMosArbitration();
}

void Bms3520_RequestMos(GPIO_Type type, uint8_t on)
{
    switch (type)
    {
    case GPIO_CHG: s_prot.requestedCharge = on ? 1U : 0U; break;
    case GPIO_DSG: s_prot.requestedDischarge = on ? 1U : 0U; break;
    case GPIO_PreCHG:
    case GPIO_MAIN:
    default: break;
    }
    Bms3520_ApplyMosArbitration();
}

const BMS3520_PROTECTION_STATUS *Bms3520_GetProtectionStatus(void) { return &s_prot; }
uint32_t Bms3520_GetBlockMask(void) { return s_prot.activeAll; }
void Bms3520_SetSystemBlock(uint8_t blocked) { s_systemBlock = blocked ? 1U : 0U; }

/* ===== Compatibility API: old filenames remain build slots only. ===== */
UINT8 AFE_CheckStatus(void) { return Afe3520_IsReady(); }
UINT8 AFE_IsReady(void) { return (Afe3520_Service() == AFE3520_OK) ? 1U : 0U; }
void AFE_Reset(void) { (void)Afe3520_SoftReset(); }
void AFE_Sleep(void) { (void)Afe3520_EnterSleep(); }
void AFE_IDLE(void) { (void)Afe3520_EnterIdle(); }
void AFE_SHIP(void)
{
    /* Reference board has no software-driven SHIP GPIO. */
}
UINT32 AFE_CalcuVbat(void)
{
    const AFE3520_SNAPSHOT *s = Afe3520_GetSnapshot();
    uint8_t i;
    uint32_t total = 0U;
    for (i = 0U; i < SeriesNum && i < AFE3520_CELL_MAX; ++i) total += s->cellMv[i];
    return total;
}
UINT8 SH367309_SC_DelayT_Set(void) { return Bms3520_ApplyAndVerifyAfeConfig(); }
void SH367309_DriverMos_Ctrl(GPIO_Type Type, UINT8 OnOFF) { Bms3520_RequestMos(Type, OnOFF); }
bool SH367309_UpdataAfeConfig(void) { return Bms3520_ApplyAndVerifyAfeConfig() ? true : false; }
void SH367309_Enable_AFE_Wdt_Cadc_Drivers(void) { (void)Bms3520_ApplyAndVerifyAfeConfig(); }
void App_SH367309(void) { Bms3520_ProtectionService(); }

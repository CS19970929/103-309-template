#include "main.h"
#include "afe3520/BmsProtection3520.h"
#include "afe3520/Afe3520Config.h"
#include <string.h>

static BMS3520_PROTECTION_STATUS s_prot;
#if BMS3520_CFG_SW_PROTECTION
static uint16_t s_swCnt[8];
static uint16_t s_swRcvCnt[8];
#ifdef __SOC_5_PROTECT_
static uint16_t s_socLowCnt;
#endif
#endif
static uint16_t s_hwStableCnt[11];
static uint8_t s_systemBlock;
static uint8_t s_tempActive[4];

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

#if BMS3520_CFG_SW_PROTECTION
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
        if (t < v) v = t;
    }
    return (v == 0xFFFFU) ? 0U : v;
}

static uint16_t Bms3520_FilterTicks(uint16_t filter10ms)
{
    uint32_t ms = (uint32_t)filter10ms * 10UL;
    uint16_t ticks = (uint16_t)((ms + BMS3520_PROTECTION_PERIOD_MS - 1U) / BMS3520_PROTECTION_PERIOD_MS);
    return (ticks == 0U) ? 1U : ticks;
}

#endif

/* Tables are register encodings from CV1.0A pp.33-38, not editable defaults. */
static const uint16_t s_normalDelayMs[8] = {140,280,490,980,2030,3010,4970,10010};
static const uint16_t s_uvDelayMs[8] = {490,770,980,1470,2030,3010,4970,10010};
static const uint16_t s_scDelayUs[16] = {0,32,64,96,128,192,224,256,288,320,384,448,480,512,544,576};
static const uint16_t s_scMultiplier[4] = {2,3,4,6};
static const uint16_t s_preDischargeMs[8] = {210,280,420,490,630,980,2030,3010};
static const uint16_t s_cadcIdleSeconds[4] = {4,32,64,256};
extern const UINT16 iSheldTemp_10K_NTC[141];
static BMS3520_HARDWARE_CONFIG s_hwConfig = {
    AFE3520_CFG_OV_MV, AFE3520_CFG_OV_DELAY_MS, AFE3520_CFG_UV_MV, AFE3520_CFG_UV_DELAY_MS,
    AFE3520_CFG_OCD1_SHUNT_MV, AFE3520_CFG_OCD1_DELAY_MS,
    AFE3520_CFG_OCD2_SHUNT_MV, AFE3520_CFG_OCD2_DELAY_MS,
    AFE3520_CFG_SC_OCD2_MULTIPLIER, AFE3520_CFG_SC_DELAY_US,
    AFE3520_CFG_OCC_SHUNT_UV, AFE3520_CFG_OCC_DELAY_MS,
    AFE3520_CFG_CHG_OT_OHM, AFE3520_CFG_DSG_OT_OHM, AFE3520_CFG_CHG_UT_OHM, AFE3520_CFG_DSG_UT_OHM,
    AFE3520_CFG_HW_OV_RECOVERY_MV, AFE3520_CFG_HW_UV_RECOVERY_MV,
    AFE3520_CFG_HW_CHG_OT_RECOVERY_C, AFE3520_CFG_HW_DSG_OT_RECOVERY_C,
    AFE3520_CFG_HW_CHG_UT_RECOVERY_C, AFE3520_CFG_HW_DSG_UT_RECOVERY_C,
    AFE3520_CFG_HW_OCP_RECOVERY_MA, AFE3520_CFG_HW_RECOVERY_MS,
    (AFE3520_CFG_OV_ENABLE ? AFE3520_SCONF6_OV_EN : 0U) |
    (AFE3520_CFG_UV_ENABLE ? AFE3520_SCONF6_UV_EN : 0U) |
    (AFE3520_CFG_OCD_ENABLE ? AFE3520_SCONF6_OCD_EN : 0U) |
    (AFE3520_CFG_SC_ENABLE ? AFE3520_SCONF6_SC_EN : 0U) |
    (AFE3520_CFG_TS1_PROTECTION ? AFE3520_SCONF6_TS1_EN : 0U) |
    (AFE3520_CFG_TS2_PROTECTION ? AFE3520_SCONF6_TS2_EN : 0U) |
    (AFE3520_CFG_TS3_PROTECTION ? AFE3520_SCONF6_TS3_EN : 0U) |
    (AFE3520_CFG_TS4_PROTECTION ? AFE3520_SCONF6_TS4_EN : 0U),
    AFE3520_CFG_OCC_ENABLE
};

static uint8_t Bms3520_TableCode(uint16_t value, const uint16_t *table, uint8_t count)
{
    uint8_t i;
    for (i=0U; i<count; ++i) if (value == table[i]) return i;
    return 255U; /* Unsupported physical value; never silently round a delay. */
}

uint8_t Bms3520_ValidateHardwareConfig(const BMS3520_HARDWARE_CONFIG *p)
{
    if (!p || p->enableMask > 255U || p->occEnable > 1U) return 0U;
    if (!p->uvMv || p->ovMv > 5115U || p->uvMv >= p->ovMv ||
        p->ovMv % 5U || p->uvMv % 5U ||
        p->ovRecoveryMv >= p->ovMv || p->ovRecoveryMv <= p->uvMv ||
        p->uvRecoveryMv <= p->uvMv || p->uvRecoveryMv >= p->ovRecoveryMv) return 0U;
    if (Bms3520_TableCode(p->ovDelayMs,s_normalDelayMs,8U)==255U ||
        Bms3520_TableCode(p->uvDelayMs,s_uvDelayMs,8U)==255U ||
        Bms3520_TableCode(p->ocd1DelayMs,s_normalDelayMs,8U)==255U ||
        Bms3520_TableCode(p->occDelayMs,s_normalDelayMs,8U)==255U ||
        Bms3520_TableCode(p->scDelayUs,s_scDelayUs,16U)==255U ||
        Bms3520_TableCode(p->scOcd2Multiplier,s_scMultiplier,4U)==255U) return 0U;
    if (p->ocd1ShuntMv < 5U || p->ocd1ShuntMv > 80U || p->ocd1ShuntMv % 5U ||
        p->ocd2ShuntMv < 10U || p->ocd2ShuntMv > 160U || p->ocd2ShuntMv % 10U ||
        p->ocd2DelayMs < 25U || p->ocd2DelayMs > 400U || p->ocd2DelayMs % 25U ||
        p->occShuntUv < 1375U || p->occShuntUv > 44000U || p->occShuntUv % 1375U) return 0U;
    /* Rref=10kOhm: high-temp byte <256, low-temp byte omits bit8. */
    if (p->chgOtOhm < 980UL || p->chgOtOhm >= 10000UL ||
        p->dsgOtOhm < 980UL || p->dsgOtOhm >= 10000UL ||
        p->chgUtOhm < 10000UL || p->chgUtOhm > 203750UL ||
        p->dsgUtOhm < 10000UL || p->dsgUtOhm > 203750UL ||
        p->chgOtRecoveryC > 100 || p->dsgOtRecoveryC > 100 ||
        p->chgUtRecoveryC < -40 || p->dsgUtRecoveryC < -40 ||
        p->chgUtRecoveryC >= p->chgOtRecoveryC || p->dsgUtRecoveryC >= p->dsgOtRecoveryC) return 0U;
    if ((uint32_t)iSheldTemp_10K_NTC[p->chgOtRecoveryC+40]*10UL <= p->chgOtOhm ||
        (uint32_t)iSheldTemp_10K_NTC[p->dsgOtRecoveryC+40]*10UL <= p->dsgOtOhm ||
        (uint32_t)iSheldTemp_10K_NTC[p->chgUtRecoveryC+40]*10UL >= p->chgUtOhm ||
        (uint32_t)iSheldTemp_10K_NTC[p->dsgUtRecoveryC+40]*10UL >= p->dsgUtOhm) return 0U;
    if (!p->ocpRecoveryMa || p->ocpRecoveryMa % 100U ||
        p->recoveryMs < BMS3520_PROTECTION_PERIOD_MS || p->recoveryMs % BMS3520_PROTECTION_PERIOD_MS) return 0U;
    return 1U;
}

static uint8_t Bms3520_TemperatureCode(uint32_t resistanceOhm)
{
    /* CV1.0A: 512-step divider, 10kOhm reference; low-temp byte omits bit8. */
    return (uint8_t)(resistanceOhm * 512UL / (10000UL + resistanceOhm));
}

uint8_t Bms3520_BuildAfeConfig(AFE3520_REG_CONFIG *cfg)
{
    const BMS3520_HARDWARE_CONFIG *p = &s_hwConfig;
    uint8_t pre, idle;
    if (!cfg || SeriesNum < 5U || SeriesNum > AFE3520_CELL_MAX || !Bms3520_ValidateHardwareConfig(p)) return 0U;
    pre=Bms3520_TableCode(AFE3520_CFG_PRE_DISCHARGE_MS,s_preDischargeMs,8U);
    idle=Bms3520_TableCode(AFE3520_CFG_CADC_IDLE_SECONDS,s_cadcIdleSeconds,4U);
    if (pre==255U || idle==255U || AFE3520_CFG_LOAD_WAKE_MODE>2U || AFE3520_CFG_LOAD_DETECT_MODE>2U ||
        (AFE3520_CFG_LOAD_PULLUP_UA!=60U && AFE3520_CFG_LOAD_PULLUP_UA!=500U) ||
        AFE3520_CFG_CURRENT_STATE_UV_X2<385U || AFE3520_CFG_CURRENT_STATE_UV_X2>2310U ||
        (AFE3520_CFG_CURRENT_STATE_UV_X2-385U)%275U ||
        AFE3520_CFG_OPEN_WIRE_MV<160U || AFE3520_CFG_OPEN_WIRE_MV>2560U || AFE3520_CFG_OPEN_WIRE_MV%160U) return 0U;
    memset(cfg,0,sizeof(*cfg));
    cfg->writeMask=(1UL << AFE3520_CONFIG_LENGTH)-1UL;
#define SET_REG(reg, value_) cfg->value[(reg)-AFE3520_REG_SCONF1]=(uint8_t)(value_)
    SET_REG(AFE3520_REG_SCONF1,AFE3520_MODE_NORMAL);
    SET_REG(AFE3520_REG_SCONF2, AFE3520_SCONF2_LTCLR |
        (AFE3520_CFG_PUMP_ENABLE ? AFE3520_SCONF2_PUMP_EN : 0U) |
        (AFE3520_CFG_AUTO_POWERDOWN_ENABLE ? AFE3520_SCONF2_PD_EN : 0U));
    SET_REG(AFE3520_REG_SCONF3,(AFE3520_CFG_CHARGER_WAKE_ENABLE << 6) |
        (AFE3520_CFG_LOAD_WAKE_MODE << 4) | (AFE3520_CFG_LOAD_DETECT_MODE << 2) | (AFE3520_CFG_OPEN_WIRE_ENABLE << 1));
    SET_REG(AFE3520_REG_SCONF4,(pre << 5) | SeriesNum);
    SET_REG(AFE3520_REG_SCONF5,(AFE3520_CFG_EFFECTIVE_SCONF5 & ~AFE3520_SCONF5_OCC_EN) |
        (BMS3520_CFG_HW_PROTECTION && p->occEnable ? AFE3520_SCONF5_OCC_EN : 0U));
    SET_REG(AFE3520_REG_SCONF6,BMS3520_CFG_HW_PROTECTION ? p->enableMask : 0U);
    SET_REG(AFE3520_REG_SCONF7,((AFE3520_CFG_LOAD_PULLUP_UA==500U) << 6) | (idle << 4) |
        ((AFE3520_CFG_CURRENT_STATE_UV_X2-385U)/275U));
    SET_REG(AFE3520_REG_OWV_ALARMH,((AFE3520_CFG_OPEN_WIRE_MV/160U-1U) << 4) |
        (AFE3520_CFG_ALARM_LOAD_CONNECT << 3) | (AFE3520_CFG_ALARM_LOAD_DISCONNECT << 2) |
        (AFE3520_CFG_ALARM_VADC << 1) | AFE3520_CFG_ALARM_CADC);
    SET_REG(AFE3520_REG_ALARML,(AFE3520_CFG_ALARM_WAKE << 7) | (AFE3520_CFG_ALARM_WDT << 6) |
        (AFE3520_CFG_ALARM_OPEN_WIRE << 5) | (AFE3520_CFG_ALARM_TEMPERATURE << 4) |
        (AFE3520_CFG_ALARM_OCC << 3) | (AFE3520_CFG_ALARM_OCD << 2) |
        (AFE3520_CFG_ALARM_UV << 1) | AFE3520_CFG_ALARM_OV);
    SET_REG(AFE3520_REG_OVT_OVH,(Bms3520_TableCode(p->ovDelayMs,s_normalDelayMs,8U) << 4) | ((p->ovMv/5U) >> 8));
    SET_REG(AFE3520_REG_OVL,p->ovMv/5U);
    SET_REG(AFE3520_REG_UVT_UVH,(Bms3520_TableCode(p->uvDelayMs,s_uvDelayMs,8U) << 4) | ((p->uvMv/5U) >> 8));
    SET_REG(AFE3520_REG_UVL,p->uvMv/5U);
    SET_REG(AFE3520_REG_OCD1V_OCD1T,(Bms3520_TableCode(p->ocd1DelayMs,s_normalDelayMs,8U) << 4) | (p->ocd1ShuntMv/5U-1U));
    SET_REG(AFE3520_REG_OCD2V_OCD2T,((p->ocd2DelayMs/25U-1U) << 4) | (p->ocd2ShuntMv/10U-1U));
    SET_REG(AFE3520_REG_SCV_SCT,(Bms3520_TableCode(p->scOcd2Multiplier,s_scMultiplier,4U) << 4) | Bms3520_TableCode(p->scDelayUs,s_scDelayUs,16U));
    SET_REG(AFE3520_REG_OCCV_OCCT,(Bms3520_TableCode(p->occDelayMs,s_normalDelayMs,8U) << 5) | (p->occShuntUv/1375U-1U));
    SET_REG(AFE3520_REG_OTC,Bms3520_TemperatureCode(p->chgOtOhm));
    SET_REG(AFE3520_REG_OTD,Bms3520_TemperatureCode(p->dsgOtOhm));
    SET_REG(AFE3520_REG_UTC,Bms3520_TemperatureCode(p->chgUtOhm));
    SET_REG(AFE3520_REG_UTD,Bms3520_TemperatureCode(p->dsgUtOhm));
#undef SET_REG
    return 1U;
}

const BMS3520_HARDWARE_CONFIG *Bms3520_GetHardwareConfig(void) { return &s_hwConfig; }

BMS3520_CONFIG_RESULT Bms3520_SetHardwareConfig(const BMS3520_HARDWARE_CONFIG *cfg)
{
    if (!Bms3520_ValidateHardwareConfig(cfg)) return BMS3520_CONFIG_INVALID;
    /* Main-loop only. Reject before any mutation; close both paths before write.
     * Failed transfer remains pending and inhibits MOS until verified recovery. */
    GPIO_ResetBits(GPIO_M_CCC,PIN_M_CCC);
    s_prot.configValid=0U;
    s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
    s_hwConfig=*cfg;
    memset(s_hwStableCnt,0,sizeof(s_hwStableCnt));
    Afe3520_MarkConfigDirty();
    if (Afe3520_SetMos(0U,0U,0U)!=AFE3520_OK || !Bms3520_ApplyAndVerifyAfeConfig())
    {
        Bms3520_HandleCommFault();
        return BMS3520_CONFIG_PENDING;
    }
    /* Service must take fresh measurements and arbitrate before MOS can reopen. */
    return BMS3520_CONFIG_VERIFIED;
}

static uint8_t Bms3520_HardwareFlag1Mask(void)
{
    uint8_t mask=0U;
    if (!BMS3520_CFG_HW_PROTECTION) return 0U;
    if (s_hwConfig.enableMask & AFE3520_SCONF6_OV_EN) mask |= AFE3520_FLAG1_OV;
    if (s_hwConfig.enableMask & AFE3520_SCONF6_UV_EN) mask |= AFE3520_FLAG1_UV;
    if (s_hwConfig.enableMask & AFE3520_SCONF6_OCD_EN) mask |= AFE3520_FLAG1_OCD1 | AFE3520_FLAG1_OCD2;
    if (s_hwConfig.enableMask & AFE3520_SCONF6_SC_EN) mask |= AFE3520_FLAG1_SC;
    if (s_hwConfig.occEnable) mask |= AFE3520_FLAG1_OCC;
    return mask;
}

static uint8_t Bms3520_HardwareFlag2Mask(void)
{
    return (BMS3520_CFG_HW_PROTECTION && (s_hwConfig.enableMask &
        (AFE3520_SCONF6_TS1_EN | AFE3520_SCONF6_TS2_EN | AFE3520_SCONF6_TS3_EN | AFE3520_SCONF6_TS4_EN))) ?
        (AFE3520_FLAG2_UTC | AFE3520_FLAG2_OTC | AFE3520_FLAG2_UTD | AFE3520_FLAG2_OTD) : 0U;
}

uint8_t Bms3520_ApplyAndVerifyAfeConfig(void)
{
    AFE3520_REG_CONFIG cfg;
    uint8_t clear1, clear2;
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
    /* Disable first, verify, then remove only latches belonging to disabled sources. */
    clear1=(AFE3520_FLAG1_OV | AFE3520_FLAG1_UV | AFE3520_FLAG1_OCD1 |
        AFE3520_FLAG1_OCD2 | AFE3520_FLAG1_SC | AFE3520_FLAG1_OCC) & ~Bms3520_HardwareFlag1Mask();
    clear2=(AFE3520_FLAG2_UTC | AFE3520_FLAG2_OTC | AFE3520_FLAG2_UTD |
        AFE3520_FLAG2_OTD) & ~Bms3520_HardwareFlag2Mask();
    if ((clear1 || clear2) && Afe3520_ClearFlags(clear1,clear2) != AFE3520_OK)
    {
        s_prot.configValid=0U;
        return 0U;
    }
    s_prot.configValid = 1U;
    return 1U;
}

#if BMS3520_CFG_SW_PROTECTION
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
#ifdef __SOC_5_PROTECT_
    if (g_stCellInfoReport.SocElement.u16Soc <= 5U &&
        g_stCellInfoReport.u16Ichg < BMS3520_REVERSE_CURRENT_A10)
    {
        s_prot.dischargeBlocks |= AFE3520_BLOCK_DSG_SW_SOC;
        g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = 1U;
        if (++s_socLowCnt >= 18000U) { s_socLowCnt=0U; LowPower_Request(DEEP_MODE); }
    }
    else
    {
        s_socLowCnt=0U; s_prot.dischargeBlocks &= ~AFE3520_BLOCK_DSG_SW_SOC;
        g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = 0U;
    }
#endif

    active = (s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OV) ? 1U : 0U;
    if (Bms3520_FilterHigh(maxCell, g_bmsParameters.u16VcellOvp.curValue,
                           g_bmsParameters.u16VcellOvp_Filter.curValue,
                           &s_swCnt[0], &s_swRcvCnt[0], active,
                           g_bmsParameters.u16VcellOvp_Rcv.curValue))
        s_prot.chargeBlocks |= AFE3520_BLOCK_CHG_SW_OV;
    else s_prot.chargeBlocks &= ~AFE3520_BLOCK_CHG_SW_OV;

    active = (s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_UV) ? 1U : 0U;
    if (Bms3520_FilterLow(minCell, g_bmsParameters.u16VcellUvp.curValue,
                          g_bmsParameters.u16VcellUvp_Filter.curValue,
                          &s_swCnt[1], &s_swRcvCnt[1], active,
                          g_bmsParameters.u16VcellUvp_Rcv.curValue))
        s_prot.dischargeBlocks |= AFE3520_BLOCK_DSG_SW_UV;
    else s_prot.dischargeBlocks &= ~AFE3520_BLOCK_DSG_SW_UV;

    active = (s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OCP) ? 1U : 0U;
    if (Bms3520_FilterHigh(g_stCellInfoReport.u16Ichg,
                           g_bmsParameters.u16IchgOcp_First.curValue,
                           g_bmsParameters.u16IchgOcp_Filter_First.curValue,
                           &s_swCnt[2], &s_swRcvCnt[2], active,
                           (uint16_t)(g_bmsParameters.u16IchgOcp_First.curValue * 8U / 10U)))
        s_prot.chargeBlocks |= AFE3520_BLOCK_CHG_SW_OCP;
    else s_prot.chargeBlocks &= ~AFE3520_BLOCK_CHG_SW_OCP;

    active = (s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_OCP) ? 1U : 0U;
    if (Bms3520_FilterHigh(g_stCellInfoReport.u16IDischg,
                           g_bmsParameters.u16IdsgOcp_First.curValue,
                           g_bmsParameters.u16IdsgOcp_Filter_First.curValue,
                           &s_swCnt[3], &s_swRcvCnt[3], active,
                           (uint16_t)(g_bmsParameters.u16IdsgOcp_First.curValue * 8U / 10U)))
        s_prot.dischargeBlocks |= AFE3520_BLOCK_DSG_SW_OCP;
    else s_prot.dischargeBlocks &= ~AFE3520_BLOCK_DSG_SW_OCP;

    s_tempActive[0] = Bms3520_FilterHigh(maxTemp, g_bmsParameters.u16TChgOTp.curValue, 1U,
                           &s_swCnt[4], &s_swRcvCnt[4], s_tempActive[0],
                           g_bmsParameters.u16TChgOTp_Rcv.curValue);
    s_tempActive[1] = Bms3520_FilterLow(minTemp, g_bmsParameters.u16TchgUTp.curValue, 1U,
                           &s_swCnt[5], &s_swRcvCnt[5], s_tempActive[1],
                           g_bmsParameters.u16TchgUTp_Rcv.curValue);
    if (s_tempActive[0] || s_tempActive[1]) s_prot.chargeBlocks |= AFE3520_BLOCK_CHG_SW_TEMP;
    else s_prot.chargeBlocks &= ~AFE3520_BLOCK_CHG_SW_TEMP;

    s_tempActive[2] = Bms3520_FilterHigh(maxTemp, g_bmsParameters.u16TdischgOTp.curValue, 1U,
                           &s_swCnt[6], &s_swRcvCnt[6], s_tempActive[2],
                           g_bmsParameters.u16TdischgOTp_Rcv.curValue);
    s_tempActive[3] = Bms3520_FilterLow(minTemp, g_bmsParameters.u16TdischgUTp.curValue, 1U,
                           &s_swCnt[7], &s_swRcvCnt[7], s_tempActive[3],
                           g_bmsParameters.u16TdischgUTp_Rcv.curValue);
    if (s_tempActive[2] || s_tempActive[3]) s_prot.dischargeBlocks |= AFE3520_BLOCK_DSG_SW_TEMP;
    else s_prot.dischargeBlocks &= ~AFE3520_BLOCK_DSG_SW_TEMP;

}

#endif

static void Bms3520_UpdateHardwareProtection(const AFE3520_SNAPSHOT *snap)
{
    uint32_t chg = 0U, dsg = 0U, global = 0U;
    uint8_t flag1 = snap->flag1 & Bms3520_HardwareFlag1Mask();
    uint8_t flag2 = snap->flag2 & Bms3520_HardwareFlag2Mask();
    if (!snap->valid)
    {
        s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_COMM;
        return;
    }

    if (flag1 & AFE3520_FLAG1_OV) chg |= AFE3520_BLOCK_CHG_HW_OV;
    if (flag1 & AFE3520_FLAG1_OCC) chg |= AFE3520_BLOCK_CHG_HW_OCC;
    if (flag2 & (AFE3520_FLAG2_UTC | AFE3520_FLAG2_OTC)) chg |= AFE3520_BLOCK_CHG_HW_TEMP;
    if (flag1 & AFE3520_FLAG1_UV) dsg |= AFE3520_BLOCK_DSG_HW_UV;
    if (flag1 & (AFE3520_FLAG1_OCD1 | AFE3520_FLAG1_OCD2)) dsg |= AFE3520_BLOCK_DSG_HW_OCD;
    if (flag2 & (AFE3520_FLAG2_UTD | AFE3520_FLAG2_OTD)) dsg |= AFE3520_BLOCK_DSG_HW_TEMP;
    if (flag1 & AFE3520_FLAG1_SC) global |= AFE3520_BLOCK_GLOBAL_SHORT;
    if (snap->flag2 & AFE3520_FLAG2_WDT) global |= AFE3520_BLOCK_GLOBAL_WDT;
    if (BMS3520_CFG_SW_PROTECTION && snap->internalTempDeciC >= 1050) global |= AFE3520_BLOCK_GLOBAL_INTERNAL_TEMP;

    s_prot.chargeBlocks = (s_prot.chargeBlocks & (AFE3520_BLOCK_CHG_SW_OV | AFE3520_BLOCK_CHG_SW_OCP | AFE3520_BLOCK_CHG_SW_TEMP)) | chg;
    s_prot.dischargeBlocks = (s_prot.dischargeBlocks & (AFE3520_BLOCK_DSG_SW_UV | AFE3520_BLOCK_DSG_SW_OCP | AFE3520_BLOCK_DSG_SW_TEMP | AFE3520_BLOCK_DSG_SW_SOC)) | dsg;
    s_prot.globalBlocks &= ~(AFE3520_BLOCK_GLOBAL_SHORT | AFE3520_BLOCK_GLOBAL_WDT |
                             AFE3520_BLOCK_GLOBAL_OPEN_WIRE | AFE3520_BLOCK_GLOBAL_INTERNAL_TEMP |
                             0U);
    s_prot.globalBlocks |= global;
    if (s_systemBlock) s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_SYSTEM;
    else s_prot.globalBlocks &= ~AFE3520_BLOCK_GLOBAL_SYSTEM;
    if (!s_prot.configValid) s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
    else s_prot.globalBlocks &= ~AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
    s_prot.latchedHardware = chg | dsg | global;
}

static void Bms3520_TryRecoverHardware(const AFE3520_SNAPSHOT *snap)
{
    static const uint8_t masks[11] = {AFE3520_FLAG1_OV, AFE3520_FLAG1_UV,
        AFE3520_FLAG1_OCD1, AFE3520_FLAG1_OCD2, AFE3520_FLAG1_SC, AFE3520_FLAG1_OCC,
        AFE3520_FLAG2_WDT, AFE3520_FLAG2_UTC, AFE3520_FLAG2_OTC,
        AFE3520_FLAG2_UTD, AFE3520_FLAG2_OTD};
    uint8_t i, active, safe, clear1=0U, clear2=0U;
    uint16_t maxCell=Bms3520_MaxCell(), minCell=Bms3520_MinCell();
    int16_t maxTemp=-32768, minTemp=32767;
    for (i=0U; i<AFE3520_TEMP_MAX; ++i)
    {
        if (!(s_hwConfig.enableMask & (AFE3520_SCONF6_TS1_EN << i))) continue;
        if (snap->tempDeciC[i] > maxTemp) maxTemp=snap->tempDeciC[i];
        if (snap->tempDeciC[i] < minTemp) minTemp=snap->tempDeciC[i];
    }
    for (i=0U; i<11U; ++i)
    {
        active = (i<6U ? snap->flag1 : snap->flag2) & masks[i];
        safe = 0U;
        if (snap->valid && active)
        {
            switch (i)
            {
            case 0: safe = maxCell <= s_hwConfig.ovRecoveryMv; break;
            case 1: safe = minCell > 0U && minCell >= s_hwConfig.uvRecoveryMv; break;
            case 2: case 3: case 4: case 5:
                safe = g_stCellInfoReport.u16Ichg < (s_hwConfig.ocpRecoveryMa / 100U) &&
                       g_stCellInfoReport.u16IDischg < (s_hwConfig.ocpRecoveryMa / 100U); break;
            case 6: safe = 1U; break; /* Valid communication can clear WDT independently. */
            case 7: safe = minTemp >= s_hwConfig.chgUtRecoveryC*10; break;
            case 8: safe = maxTemp <= s_hwConfig.chgOtRecoveryC*10; break;
            case 9: safe = minTemp >= s_hwConfig.dsgUtRecoveryC*10; break;
            case 10: safe = maxTemp <= s_hwConfig.dsgOtRecoveryC*10; break;
            }
        }
        if (!safe) { s_hwStableCnt[i]=0U; continue; }
        /* WDT recovery must not inherit a long user-selected protection delay. */
        if (++s_hwStableCnt[i] < (i==6U ? AFE3520_CFG_COMM_RECOVERY_TICKS :
            (s_hwConfig.recoveryMs / BMS3520_PROTECTION_PERIOD_MS))) continue;
        s_hwStableCnt[i]=0U;
        if (i<6U) clear1 |= masks[i]; else clear2 |= masks[i];
    }
    if ((clear1 || clear2) && Afe3520_ClearFlags(clear1,clear2)!=AFE3520_OK)
        Bms3520_HandleCommFault();
}

static void Bms3520_PublishFaults(const AFE3520_SNAPSHOT *snap)
{
    uint8_t chgOvp = ((s_prot.chargeBlocks & (AFE3520_BLOCK_CHG_HW_OV | AFE3520_BLOCK_CHG_SW_OV)) != 0U);
    uint8_t uvp = ((s_prot.dischargeBlocks & (AFE3520_BLOCK_DSG_HW_UV | AFE3520_BLOCK_DSG_SW_UV)) != 0U);
    uint8_t chgOcp = ((s_prot.chargeBlocks & (AFE3520_BLOCK_CHG_HW_OCC | AFE3520_BLOCK_CHG_SW_OCP)) != 0U);
    uint8_t dsgOcp = ((s_prot.dischargeBlocks & (AFE3520_BLOCK_DSG_HW_OCD | AFE3520_BLOCK_DSG_SW_OCP)) != 0U);


    /* Compatibility reporting only: no second MOS authority or recovery timer. */
    if (!g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp &&
        (s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OCP)) FaultWarnRecord2(IchgOcp_Second);
    if (!g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp &&
        (s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_OCP)) FaultWarnRecord2(IdischgOcp_Second);
    g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = !!(s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OCP);
    g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = !!(s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_SW_OCP);
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp = chgOvp;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp = uvp;
    g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp = chgOcp;
    g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp = dsgOcp;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp = s_tempActive[1] || (BMS3520_CFG_HW_PROTECTION && (snap->flag2 & AFE3520_FLAG2_UTC));
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp = s_tempActive[0] || (BMS3520_CFG_HW_PROTECTION && (snap->flag2 & AFE3520_FLAG2_OTC));
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp = s_tempActive[3] || (BMS3520_CFG_HW_PROTECTION && (snap->flag2 & AFE3520_FLAG2_UTD));
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp = s_tempActive[2] || (BMS3520_CFG_HW_PROTECTION && (snap->flag2 & AFE3520_FLAG2_OTD));
    System_ErrFlag.u8ErrFlag_CBC_DSG = (BMS3520_CFG_HW_PROTECTION && (snap->flag1 & AFE3520_FLAG1_SC)) ? 1U : 0U;
}

void Bms3520_HandleCommFault(void)
{
    s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_AFE_COMM | AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
    s_prot.configValid = 0U;
    s_prot.recoveryCounter = 0U;
    memset(s_hwStableCnt, 0, sizeof(s_hwStableCnt));
    s_prot.mosFeedbackValid = 0U;
    Afe3520_Invalidate();
    GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
    s_prot.actualCharge = 0U; /* External charge gate is physically low. */
    /* Best effort only: a broken bus cannot guarantee discharge FET shutdown. */
    if (Afe3520_SetMos(0U, 0U, 0U) == AFE3520_OK)
    {
        s_prot.actualDischarge = (Afe3520_GetSnapshot()->bstatus1 & AFE3520_BSTATUS1_DSG_FET) ? 1U : 0U;
        s_prot.mosFeedbackValid = 1U;
    }
    SystemRuntime_SetMosStatus(s_prot.actualCharge, s_prot.actualDischarge);
    SystemRuntime_SetAfeStatus(0U, 0U);
    System_ERROR_UserCallback(ERROR_AFE1);
    s_prot.activeAll = s_prot.chargeBlocks | s_prot.dischargeBlocks | s_prot.globalBlocks;
}

static void Bms3520_ApplyMosArbitration(void)
{
    uint8_t charge = s_prot.requestedCharge;
    uint8_t discharge = s_prot.requestedDischarge;
    const AFE3520_SNAPSHOT *snap;

    if ((s_prot.globalBlocks != 0U) || !s_prot.configValid || s_systemBlock || !Afe3520_GetSnapshot()->valid)
    {
        charge = 0U;
        discharge = 0U;
    }
    else
    {
        if (s_prot.chargeBlocks != 0U) charge = 0U;
        if (s_prot.dischargeBlocks != 0U) discharge = 0U;
    }

    /* Reissue commands after every sample/reconfiguration; a successful old
     * command is not proof that hardware still has that state after reset.
     * PB14 follows the charge command, exactly as reference GPIO_M_CCC. */
    if (!charge) GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
    if (Afe3520_SetMos(charge, discharge, 0U) != AFE3520_OK)
    {
        Bms3520_HandleCommFault();
        return;
    }
    GPIO_WriteBit(GPIO_M_CCC, PIN_M_CCC, charge ? Bit_SET : Bit_RESET);
    snap = Afe3520_GetSnapshot();
    s_prot.actualCharge = ((snap->bstatus1 & AFE3520_BSTATUS1_CHG_FET) &&
                           GPIO_ReadOutputDataBit(GPIO_M_CCC, PIN_M_CCC)) ? 1U : 0U;
    s_prot.actualDischarge = (snap->bstatus1 & AFE3520_BSTATUS1_DSG_FET) ? 1U : 0U;
    s_prot.mosFeedbackValid = 1U;
    SystemRuntime_SetMosStatus(s_prot.actualCharge, s_prot.actualDischarge);
}

void Bms3520_ProtectionInit(void)
{
    memset(&s_prot, 0, sizeof(s_prot));
    s_systemBlock = 0U;
    memset(s_tempActive, 0, sizeof(s_tempActive));
#if BMS3520_CFG_SW_PROTECTION
#ifdef __SOC_5_PROTECT_
    s_socLowCnt = 0U;
#endif
    memset(s_swCnt, 0, sizeof(s_swCnt));
    memset(s_swRcvCnt, 0, sizeof(s_swRcvCnt));
#endif
    memset(s_hwStableCnt, 0, sizeof(s_hwStableCnt));
    s_prot.requestedCharge = 0U;
    s_prot.requestedDischarge = 0U;
    s_prot.globalBlocks = AFE3520_BLOCK_GLOBAL_AFE_CONFIG;
    GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
}

void Bms3520_Service200ms(void)
{
    const AFE3520_SNAPSHOT *snap;
    if (Afe3520_Service() != AFE3520_OK)
    {
        Bms3520_HandleCommFault();
        return;
    }
    if (Afe3520_ConfigDirty() || !s_prot.configValid)
    {
        if (!Bms3520_ApplyAndVerifyAfeConfig() || Afe3520_Service() != AFE3520_OK)
        {
            Bms3520_HandleCommFault();
            return;
        }
    }
    snap = Afe3520_GetSnapshot();
#if BMS3520_CFG_SW_PROTECTION
    Bms3520_UpdateSoftwareProtection();
#endif
    Bms3520_UpdateHardwareProtection(snap);
    Bms3520_TryRecoverHardware(snap);
    if (!snap->valid) return; /* Flag-clear failure has already latched a comm fault. */
    Bms3520_PublishFaults(snap);

    if (s_prot.globalBlocks & AFE3520_BLOCK_GLOBAL_AFE_COMM)
    {
        if (++s_prot.recoveryCounter >= AFE3520_CFG_COMM_RECOVERY_TICKS)
        {
            s_prot.globalBlocks &= ~AFE3520_BLOCK_GLOBAL_AFE_COMM;
            s_prot.recoveryCounter = 0U;
        }
    }
    Bms3520_ApplyMosArbitration();
    s_prot.activeAll = s_prot.chargeBlocks | s_prot.dischargeBlocks | s_prot.globalBlocks;
    if (!(s_prot.globalBlocks & (AFE3520_BLOCK_GLOBAL_AFE_COMM | AFE3520_BLOCK_GLOBAL_AFE_CONFIG)))
    {
        SystemRuntime_SetAfeStatus(0U, 1U);
        System_ERROR_UserCallback(ERROR_REMOVE_AFE1);
    }
    else
    {
        SystemRuntime_SetAfeStatus(0U, 0U);
        System_ERROR_UserCallback(ERROR_AFE1);
    }
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
uint32_t Bms3520_GetBlockMask(void) { return s_prot.chargeBlocks | s_prot.dischargeBlocks | s_prot.globalBlocks; }
void Bms3520_SetSystemBlock(uint8_t blocked)
{
    s_systemBlock = blocked ? 1U : 0U;
    if (blocked) s_prot.globalBlocks |= AFE3520_BLOCK_GLOBAL_SYSTEM;
    else s_prot.globalBlocks &= ~AFE3520_BLOCK_GLOBAL_SYSTEM;
    if (blocked) Bms3520_ApplyMosArbitration();
    s_prot.activeAll = Bms3520_GetBlockMask();
}

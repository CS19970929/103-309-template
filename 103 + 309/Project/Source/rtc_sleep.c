#include "main.h"
#include "low_power_port.h"

/* Product policy. STOP sequencing and board pins have separate owners. */
extern void DataLoad_CellVolt(void);
extern void DataLoad_CellVoltMaxMinFind(void);
extern void DataLoad_Temperature(void);
extern void DataLoad_TemperatureMaxMinFind(void);

#define SLEEP_FORCE_LOW_MV 2600U
#define SLEEP_FORCE_LOW_SECONDS 60U
static enum _SLEEP_MODE s_sleep_mode = NO_SLEEP;
static uint32_t s_low_seconds, s_idle_seconds, s_last_comm;
static uint16_t s_force_seconds;
static uint8_t s_started;
volatile UINT32 RTC_ExtComCnt;
volatile enum irqWakeup g_irq_t = NO_IRQ;

void rtc_sleep_record_wakeup(enum irqWakeup source)
{
    uint32_t wake;
    uint32_t mask = BoardLowPower_Lock();
    if (g_irq_t == NO_IRQ)
        g_irq_t = source;
    switch (source)
    {
    case PA0_irq: wake = LP_WAKE_PA0; break;
    case bms_keyirq: case soc_key: wake = LP_WAKE_KEY; break;
    case rs485_irq: wake = LP_WAKE_COMM; break;
    case uart1_irq: wake = LP_WAKE_UART1; break;
    case uart2_irq: wake = LP_WAKE_UART2; break;
    case uart3_irq: wake = LP_WAKE_UART3; break;
    default: wake = LP_WAKE_OTHER; break;
    }
    LowPower_NotifyWake(wake);
    BoardLowPower_Unlock(mask);
}

void entersleep(enum _SLEEP_MODE mode)
{
    if (mode == NO_SLEEP)
        s_sleep_mode = NO_SLEEP;
    else if (mode == DEEP_MODE ||
             (mode == NORMAL_MODE && s_sleep_mode != DEEP_MODE) ||
             (mode == HICCUP_MODE && s_sleep_mode == NO_SLEEP))
    {
        s_sleep_mode = mode;
#ifdef __FUNC__LED__
        if (mode == DEEP_MODE)
            set_LED_state(LED_BAR_NORMAL, 4);
#endif
    }
}

void BmsSleep_Request(LowPowerTarget target)
{
    switch (target)
    {
    case LP_TARGET_RUN: entersleep(NO_SLEEP); break;
    case LP_TARGET_RTC: entersleep(HICCUP_MODE); break;
    case LP_TARGET_NORMAL_RESET: entersleep(NORMAL_MODE); break;
    case LP_TARGET_DEEP_RESET: entersleep(DEEP_MODE); break;
    default: break;
    }
}

LowPowerTarget BmsSleep_GetTarget(void)
{
    switch (s_sleep_mode)
    {
    case HICCUP_MODE: return LP_TARGET_RTC;
    case NORMAL_MODE: return LP_TARGET_NORMAL_RESET;
    case DEEP_MODE: return LP_TARGET_DEEP_RESET;
    default: return LP_TARGET_RUN;
    }
}

uint32_t BmsSleep_BlockMask(uint8_t refresh_afe)
{
    uint32_t blocks = 0U;
    uint8_t balance[2];
    if (u8FlashUpdateFlag || u8FlashUpdateE2PROM || gu8_Reset_EventRecord ||
        u32E2P_Pro_VolCur_WriteFlag || u32E2P_Pro_Temp_WriteFlag ||
        u32E2P_Pro_Other_WriteFlag || u32E2P_OtherElement1_WriteFlag ||
        u32E2P_HeatCool_WriteFlag || u32E2P_RTC_Element_WriteFlag ||
        u8E2P_SocTable_WriteFlag || u8E2P_CopperLoss_WriteFlag || u8E2P_KB_WriteFlag)
        blocks |= LP_BLOCK_STORAGE;
    if (Sci_CommonUpperHasPendingWork())
        blocks |= LP_BLOCK_COMM;
    if (RTC_ExtComCnt != s_last_comm)
        blocks |= LP_BLOCK_COMM_CHANGED;
    if (g_stCellInfoReport.u16Ichg > 10U || g_stCellInfoReport.u16IDischg > 10U)
        blocks |= LP_BLOCK_CURRENT;
    if (SystemStatus.bits.b1Status_Heat)
        blocks |= LP_BLOCK_HEAT;
    if (OtherElement.u16Sys_SeriesNum == 0U || OtherElement.u16Sys_SeriesNum > SNum)
        blocks |= LP_BLOCK_CONFIG;
    if (refresh_afe)
    {
        if (!MTPRead(MTP_BALANCEH, 2U, balance))
            blocks |= LP_BLOCK_AFE;
        else
            CellBalance_UpdateReport(((UINT16)balance[0] << 8) | balance[1]);
        if (!MTPRead(MTP_BSTATUS1, 3U, &SH367309_Reg_Store.REG_BSTATUS1.all))
            blocks |= LP_BLOCK_AFE;
    }
    if (g_stCellInfoReport.u16BalanceFlag1 != 0U)
        blocks |= LP_BLOCK_BALANCE;
    if (SH367309_Reg_Store.REG_BSTATUS1.all || SH367309_Reg_Store.REG_BSTATUS2.all ||
        SH367309_Reg_Store.REG_BSTATUS3.bits.L0V || SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET)
        blocks |= LP_BLOCK_AFE;
#ifdef __same_door__
    if (!SystemStatus.bits.b1Status_MOS_CHG || !SystemStatus.bits.b1Status_MOS_DSG)
        blocks |= LP_BLOCK_MOS;
#endif
    if (BoardLowPower_ActiveWake() != 0U)
        blocks |= LP_BLOCK_WAKE_PIN;
    return blocks;
}

void BmsSleep_ResetQualification(void)
{
    s_idle_seconds = s_low_seconds = 0U;
    s_force_seconds = 0U;
    s_last_comm = RTC_ExtComCnt;
    entersleep(NO_SLEEP);
    g_low_power_diag.idle_seconds = 0U;
}

void BmsSleep_PollRequest(void)
{
    uint32_t blocks;
    if (!TakePendingFlag(&gu8_1000msAccClock_Flag))
        return;
    if (s_sleep_mode == DEEP_MODE)
        return;
    if (g_stCellInfoReport.u16VCellMin <= SLEEP_FORCE_LOW_MV &&
        g_stCellInfoReport.u16Ichg == 0U)
    {
        s_idle_seconds = 0U;
        if (s_force_seconds < SLEEP_FORCE_LOW_SECONDS)
            ++s_force_seconds;
        if (s_force_seconds >= SLEEP_FORCE_LOW_SECONDS)
            entersleep(DEEP_MODE);
        return;
    }
    if (g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_Vlow &&
        g_stCellInfoReport.u16Ichg == 0U)
    {
        s_idle_seconds = 0U;
        if (s_low_seconds < (uint32_t)OtherElement.u16Sleep_TimeVlow * 60U)
            ++s_low_seconds;
        if (s_low_seconds >= (uint32_t)OtherElement.u16Sleep_TimeVlow * 60U)
            entersleep(DEEP_MODE);
        return;
    }
    s_low_seconds = 0U;
    s_force_seconds = 0U;
    if (s_sleep_mode != NO_SLEEP)
        return;
    if (!s_started)
    {
        s_started = 1U;
        s_last_comm = RTC_ExtComCnt;
        return;
    }
    blocks = BmsSleep_BlockMask(1U);
    g_low_power_diag.block_mask = blocks;
    s_last_comm = RTC_ExtComCnt;
    if (blocks != 0U)
        s_idle_seconds = 0U;
    else
    {
        if (s_idle_seconds < sys_time.time_enter_rtc)
            ++s_idle_seconds;
        if (s_idle_seconds >= sys_time.time_enter_rtc)
            entersleep(HICCUP_MODE);
    }
    g_low_power_diag.idle_seconds = s_idle_seconds;
    BoardLowPower_Snapshot();
}

uint32_t BmsSleep_SampleExit(void)
{
    uint8_t i;
    uint32_t exits = 0U;
    if (OtherElement.u16Sys_SeriesNum == 0U || OtherElement.u16Sys_SeriesNum > SNum)
        return LP_EXIT_CONFIG;
    if (UpdateVoltageFromBqMaximo())
        return LP_EXIT_AFE;
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();
    if (g_stCellInfoReport.u16Ichg != 0U || g_stCellInfoReport.u16IDischg != 0U)
        exits |= LP_EXIT_CURRENT;
    if (!MTPRead(MTP_BALANCEH, 5U, &SH367309_Reg_Store.u8_MTP_BALANCEH))
        return exits | LP_EXIT_AFE;
    CellBalance_UpdateReport(((UINT16)SH367309_Reg_Store.u8_MTP_BALANCEH << 8) |
                            SH367309_Reg_Store.u8_MTP_BALANCEL);
    SystemStatus.bits.b1Status_MOS_CHG = SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;
    SystemStatus.bits.b1Status_MOS_DSG = SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;
    if (!SystemStatus.bits.b1Status_MOS_CHG || !SystemStatus.bits.b1Status_MOS_DSG)
        exits |= LP_EXIT_MOS;
    if (SH367309_Reg_Store.REG_BSTATUS1.all || SH367309_Reg_Store.REG_BSTATUS2.all ||
        SH367309_Reg_Store.REG_BSTATUS3.bits.L0V || SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET ||
        g_stCellInfoReport.u16BalanceFlag1 != 0U)
        exits |= LP_EXIT_AFE;
    for (i = 0U; i < OtherElement.u16Sys_SeriesNum; ++i)
    {
        if (g_stCellInfoReport.u16VCell[i] < PRT_E2ROMParas.u16VcellUvp_Third)
            exits |= LP_EXIT_UV;
        if (g_stCellInfoReport.u16VCell[i] > PRT_E2ROMParas.u16VcellOvp_Third)
            exits |= LP_EXIT_OV;
    }
    if (g_stCellInfoReport.u16VCellMin <= SLEEP_FORCE_LOW_MV)
        exits |= LP_EXIT_UV;
    return exits;
}

uint8_t BmsSleep_CommitReset(void)
{
    return SleepDeal_CommitResetSleep((UINT8)s_sleep_mode);
}

void rtc_sleep(void)
{
    (void)LowPower_Poll();
}

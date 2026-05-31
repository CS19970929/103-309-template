#include "main.h"
#include "led_scan.h"

/* ── 7-segment digit map (A..G bits) ── */
#define LEDBAR_DIGIT_BIT_A (1u << 0)
#define LEDBAR_DIGIT_BIT_B (1u << 1)
#define LEDBAR_DIGIT_BIT_C (1u << 2)
#define LEDBAR_DIGIT_BIT_D (1u << 3)
#define LEDBAR_DIGIT_BIT_E (1u << 4)
#define LEDBAR_DIGIT_BIT_F (1u << 5)
#define LEDBAR_DIGIT_BIT_G (1u << 6)

/* ── Key / MCU_WK filter constants ── */
#define LEDBAR_KEY_ON_FILTER_10MS  3u
#define LEDBAR_KEY_OFF_FILTER_10MS 3u
#define LEDBAR_KEY_LONG_PRESS_10MS 50u
#define LEDBAR_MCU_WK_ON_FILTER_10MS  PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS
#define LEDBAR_MCU_WK_OFF_FILTER_10MS PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS

/* ── Sleep SOC stored in backup registers ── */
#define LEDBAR_SLEEP_SOC_MAGIC       0x5A00u
#define LEDBAR_SLEEP_SOC_MAGIC_MASK  0xFF00u
#define LEDBAR_SLEEP_SOC_VALUE_MASK  0x00FFu
#define LEDBAR_SLEEP_SOC_REG         BKP_DR4
#define LEDBAR_SLEEP_SOC_INV_REG     BKP_DR5

/* ── Runtime state ── */
typedef struct
{
    uint8_t  initialized;
    uint8_t  sleep;
    uint8_t  blank;
    uint8_t  number;
    uint8_t  indicator_mask;
    volatile uint32_t frame_mask;
    volatile uint8_t  scan_route;
    volatile uint8_t  scan_timer_enabled;
    uint16_t soc_display_10ms;
    uint8_t  startup_display_armed;
    uint32_t key_hold_10ms;
    uint32_t key_press_start_10ms;
    uint8_t  key_long_handled;
    uint8_t  key_filter_initialized;
    uint8_t  key_active;
    uint8_t  key_on_10ms;
    uint8_t  key_off_10ms;
    uint8_t  mcu_wk_filter_initialized;
    uint8_t  mcu_wk_active;
    uint8_t  mcu_wk_on_10ms;
    uint8_t  mcu_wk_off_10ms;
} LedBarRuntime;

static const uint8_t s_ledbar_digit_map[10] =
{
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C |
        LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F,           /* 0 */
    LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C,                                    /* 1 */
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_D |
        LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_G,                                 /* 2 */
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C |
        LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_G,                                 /* 3 */
    LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_F |
        LEDBAR_DIGIT_BIT_G,                                                      /* 4 */
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D |
        LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,                                 /* 5 */
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D |
        LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,           /* 6 */
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C,               /* 7 */
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C |
        LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F |
        LEDBAR_DIGIT_BIT_G,                                                      /* 8 */
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C |
        LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,           /* 9 */
};

static LedBarRuntime s_ledbar =
{
    0u,
    0u,
    1u,
    0u,
    LEDBAR_ICON_PERCENT_MASK,
};

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
LedBarRuntime * const g_dbg_ledbar_runtime = &s_ledbar;
#endif

/* ── Forward decls ── */
static void LedBar_RefreshOutput(void);

/* ── Helpers ── */

static void LedBar_EnsureInit(void)
{
    if (s_ledbar.initialized == 0u)
    {
        LedBar_Init();
    }
}

static void LedBar_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static uint8_t LedBar_LimitSoc(uint16_t soc)
{
    if (soc > 100u)
    {
        soc = 100u;
    }
    return (uint8_t)soc;
}

static uint8_t LedBar_GetRuntimeSoc(void)
{
    return LedBar_LimitSoc(g_stCellInfoReport.SocElement.u16Soc);
}

static uint8_t LedBar_ReadMcuWakeRaw(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK) != Bit_RESET);
}

static uint8_t LedBar_IsDischargeMosOpen(void)
{
    return (uint8_t)(SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET != 0u);
}

static uint8_t LedBar_ReadSwitchRaw(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIO_SW, PIN_SW) == Bit_RESET);
}

/* ── Binary filter (shared by key and MCU_WK) ── */

static void LedBar_PrimeBinaryFilter(uint8_t raw_active,
                                     uint8_t *active,
                                     uint8_t *on_count,
                                     uint8_t *off_count,
                                     uint8_t on_limit,
                                     uint8_t off_limit)
{
    *active    = raw_active;
    *on_count  = (raw_active != 0u) ? on_limit  : 0u;
    *off_count = (raw_active == 0u) ? off_limit : 0u;
}

static void LedBar_UpdateBinaryFilter(uint8_t raw_active,
                                      uint8_t *active,
                                      uint8_t *on_count,
                                      uint8_t *off_count,
                                      uint8_t on_limit,
                                      uint8_t off_limit)
{
    if (raw_active != 0u)
    {
        *off_count = 0u;
        if (*on_count < on_limit) { (*on_count)++; }
        if (*on_count >= on_limit) { *active = 1u; }
    }
    else
    {
        *on_count = 0u;
        if (*off_count < off_limit) { (*off_count)++; }
        if (*off_count >= off_limit) { *active = 0u; }
    }
}

/* ── Frame mask builder ── */

static void LedBar_AddDigitRoutes(uint32_t *target_mask,
                                  uint8_t digit,
                                  uint8_t route_a)
{
    uint8_t digit_mask = s_ledbar_digit_map[digit % 10u];
    uint8_t segment;

    for (segment = 0u; segment < 7u; ++segment)
    {
        if ((digit_mask & (uint8_t)(1u << segment)) != 0u)
        {
            *target_mask |= (1UL << (route_a + segment));
        }
    }
}

static uint32_t LedBar_BuildTargetMask(uint8_t value, uint8_t indicator_mask)
{
    uint8_t tens = (uint8_t)((value / 10u) % 10u);
    uint8_t ones = (uint8_t)(value % 10u);
    uint32_t target_mask = 0u;

    if (value >= 100u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_HUNDREDS_1_UPPER);
        target_mask |= (1UL << LEDBAR_ROUTE_HUNDREDS_1_LOWER);
    }

    if (value >= 10u)
    {
        LedBar_AddDigitRoutes(&target_mask, tens, LEDBAR_ROUTE_TENS_A);
    }

    LedBar_AddDigitRoutes(&target_mask, ones, LEDBAR_ROUTE_ONES_A);

    if ((indicator_mask & LEDBAR_ICON_CHARGE_MASK) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ICON_CHARGE);
    }
    if ((indicator_mask & LEDBAR_ICON_PERCENT_MASK) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ICON_PERCENT);
    }

    return target_mask;
}

static uint8_t LedBar_NextRouteFromMask(uint32_t mask, uint8_t start_route)
{
    uint8_t route_id;

    for (route_id = start_route; route_id < (uint8_t)LEDBAR_ROUTE_COUNT; ++route_id)
    {
        if ((mask & (1UL << route_id)) != 0u)
        {
            return route_id;
        }
    }
    for (route_id = 0u; route_id < start_route; ++route_id)
    {
        if ((mask & (1UL << route_id)) != 0u)
        {
            return route_id;
        }
    }
    return 0u;
}

static uint32_t LedBar_BuildCurrentFrameMask(void)
{
    if ((s_ledbar.blank != 0u) || (s_ledbar.sleep != 0u))
    {
        return 0u;
    }

    return LedBar_BuildTargetMask(s_ledbar.number,
                                  (uint8_t)(s_ledbar.indicator_mask &
                                            (LEDBAR_ICON_CHARGE_MASK |
                                             LEDBAR_ICON_PERCENT_MASK)));
}

/* ── Output control ── */

static void LedBar_ApplyFrameMask(uint32_t frame_mask)
{
    uint8_t same_frame = (s_ledbar.frame_mask == frame_mask) ? 1u : 0u;
    uint8_t scan_was_enabled = s_ledbar.scan_timer_enabled;
    uint8_t route_id;

    if ((same_frame != 0u) &&
        (((frame_mask == 0u) && (scan_was_enabled == 0u)) ||
         ((frame_mask != 0u) && (scan_was_enabled != 0u))))
    {
        return;
    }

    if (scan_was_enabled != 0u)
    {
        TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
        NVIC_DisableIRQ(TIM4_IRQn);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }

    s_ledbar.frame_mask = frame_mask;
    s_ledbar.scan_route  = 0u;

    if (s_ledbar.frame_mask == 0u)
    {
        LedScan_StopTimer();
        LedScan_OutputOff();
        s_ledbar.scan_timer_enabled = 0u;
        return;
    }

    if (scan_was_enabled == 0u)
    {
        LedScan_StartTimer();
        s_ledbar.scan_timer_enabled = 1u;
    }

    route_id = LedBar_NextRouteFromMask(s_ledbar.frame_mask, 0u);
    LedScan_OutputRoute((uint8_t)route_id);
    s_ledbar.scan_route = LedBar_NextRouteFromMask(s_ledbar.frame_mask,
                                                   (uint8_t)(route_id + 1u));

    if (scan_was_enabled != 0u)
    {
        TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
        NVIC_EnableIRQ(TIM4_IRQn);
    }
}

static void LedBar_RefreshOutput(void)
{
    LedBar_ApplyFrameMask(LedBar_BuildCurrentFrameMask());
}

/* ── Display window management ── */

static void LedBar_RequestSocDisplayWindow(void)
{
    if (s_ledbar.soc_display_10ms < LEDBAR_SOC_DISPLAY_10MS)
    {
        s_ledbar.soc_display_10ms = LEDBAR_SOC_DISPLAY_10MS;
    }
}

static void LedBar_RequestStartupDisplayWindow(void)
{
    if (s_ledbar.soc_display_10ms < LEDBAR_STARTUP_DISPLAY_10MS)
    {
        s_ledbar.soc_display_10ms = LEDBAR_STARTUP_DISPLAY_10MS;
    }
}

static void LedBar_ServiceStartupDisplayWindow(void)
{
    if (s_ledbar.startup_display_armed == 0u)
    {
        s_ledbar.startup_display_armed = 1u;
        LedBar_RequestStartupDisplayWindow();
    }
}

/* ── MCU_WK filtering ── */

static void LedBar_ServiceMcuWakeFilter(void)
{
    uint8_t raw_active = LedBar_ReadMcuWakeRaw();
    uint8_t was_active;

    if (s_ledbar.mcu_wk_filter_initialized == 0u)
    {
        s_ledbar.mcu_wk_filter_initialized = 1u;
        LedBar_PrimeBinaryFilter(raw_active,
                                 &s_ledbar.mcu_wk_active,
                                 &s_ledbar.mcu_wk_on_10ms,
                                 &s_ledbar.mcu_wk_off_10ms,
                                 LEDBAR_MCU_WK_ON_FILTER_10MS,
                                 LEDBAR_MCU_WK_OFF_FILTER_10MS);
        return;
    }

    if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
    {
        return;
    }

    was_active = s_ledbar.mcu_wk_active;
    LedBar_UpdateBinaryFilter(raw_active,
                              &s_ledbar.mcu_wk_active,
                              &s_ledbar.mcu_wk_on_10ms,
                              &s_ledbar.mcu_wk_off_10ms,
                              LEDBAR_MCU_WK_ON_FILTER_10MS,
                              LEDBAR_MCU_WK_OFF_FILTER_10MS);
    if ((was_active == 0u) && (s_ledbar.mcu_wk_active != 0u))
    {
        LedBar_RequestSocDisplayWindow();
    }
}

/* ── Key switch filtering ── */

static void LedBar_ServiceSwitch(void)
{
    uint8_t raw_pressed = LedBar_ReadSwitchRaw();
    uint8_t was_pressed;
    uint32_t now_10ms = SysTime_Get10msTickCount();

    if (s_ledbar.key_filter_initialized == 0u)
    {
        s_ledbar.key_filter_initialized = 1u;
        LedBar_PrimeBinaryFilter(raw_pressed,
                                 &s_ledbar.key_active,
                                 &s_ledbar.key_on_10ms,
                                 &s_ledbar.key_off_10ms,
                                 LEDBAR_KEY_ON_FILTER_10MS,
                                 LEDBAR_KEY_OFF_FILTER_10MS);
        return;
    }

    if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
    {
        return;
    }

    was_pressed = s_ledbar.key_active;
    LedBar_UpdateBinaryFilter(raw_pressed,
                              &s_ledbar.key_active,
                              &s_ledbar.key_on_10ms,
                              &s_ledbar.key_off_10ms,
                              LEDBAR_KEY_ON_FILTER_10MS,
                              LEDBAR_KEY_OFF_FILTER_10MS);

    if ((was_pressed == 0u) && (s_ledbar.key_active != 0u))
    {
        LedBar_RequestSocDisplayWindow();
        s_ledbar.key_press_start_10ms = now_10ms;
        s_ledbar.key_hold_10ms = 0u;
        s_ledbar.key_long_handled = 0u;
    }

    if (s_ledbar.key_active != 0u)
    {
        s_ledbar.key_hold_10ms = now_10ms - s_ledbar.key_press_start_10ms;

#ifdef _DI_SWITCH_longKEY_ONOFF
        if ((s_ledbar.key_hold_10ms >= LEDBAR_KEY_LONG_PRESS_10MS) &&
            (s_ledbar.key_long_handled == 0u))
        {
            s_ledbar.key_long_handled = 1u;
            LedBar_SaveSleepSoc();
            LP_RequestSleep(DEEP_MODE);
            SleepDeal_Continue((UINT8)DEEP_MODE);
        }
#endif
    }
    else
    {
        s_ledbar.key_hold_10ms = 0u;
        s_ledbar.key_press_start_10ms = now_10ms;
        s_ledbar.key_long_handled = 0u;
    }

    if (s_ledbar.soc_display_10ms != 0u)
    {
        s_ledbar.soc_display_10ms--;
    }
}

/* ── Fault check ── */

static uint8_t LedBar_IsFaultActive(void)
{
    if ((g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBu) != 0u)
    {
        return 1u;
    }
    if (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) != 0u)
    {
        return 1u;
    }
    if (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) != 0u)
    {
        return 1u;
    }
    return 0u;
}

/* ═══════════════════════════════════════════════════════════════════
   Public API
   ═══════════════════════════════════════════════════════════════════ */

void LedBar_Init(void)
{
    if (s_ledbar.initialized != 0u)
    {
        return;
    }

    s_ledbar.sleep                   = 0u;
    s_ledbar.blank                   = 1u;
    s_ledbar.number                  = 0u;
    s_ledbar.indicator_mask          = LEDBAR_ICON_PERCENT_MASK;
    s_ledbar.frame_mask              = 0u;
    s_ledbar.scan_route              = 0u;
    s_ledbar.scan_timer_enabled      = 0u;
    s_ledbar.soc_display_10ms        = 0u;
    s_ledbar.startup_display_armed   = 0u;
    s_ledbar.key_hold_10ms           = 0u;
    s_ledbar.key_press_start_10ms    = 0u;
    s_ledbar.key_long_handled        = 0u;
    s_ledbar.key_filter_initialized  = 0u;
    s_ledbar.key_active              = 0u;
    s_ledbar.key_on_10ms             = 0u;
    s_ledbar.key_off_10ms            = 0u;
    s_ledbar.mcu_wk_filter_initialized = 0u;
    s_ledbar.mcu_wk_active           = 0u;
    s_ledbar.mcu_wk_on_10ms          = 0u;
    s_ledbar.mcu_wk_off_10ms         = 0u;

    LedScan_Init();
    LedScan_OutputOff();
    LedScan_PrepareForStop();

    s_ledbar.initialized = 1u;
}

void LedBar_Clear(void)
{
    LedBar_EnsureInit();

    s_ledbar.blank = 1u;
    LedBar_RefreshOutput();
}

void LedBar_SetSleep(uint8_t enable)
{
    LedBar_EnsureInit();

#if !LEDBAR_SLEEP_ENABLE
    enable = 0u;
#else
    enable = (enable != 0u) ? 1u : 0u;
#endif

    if (s_ledbar.sleep == enable)
    {
        return;
    }

    s_ledbar.sleep = enable;
    LedBar_RefreshOutput();
    if (enable != 0u)
    {
        LedScan_PrepareForStop();
    }
}

void LedBar_Wakeup(void)
{
    LedBar_SetSleep(0u);
}

void LedBar_SetNumber(uint8_t value)
{
    LedBar_EnsureInit();

    value = LedBar_LimitSoc(value);
    if ((s_ledbar.number == value) && (s_ledbar.blank == 0u))
    {
        return;
    }

    s_ledbar.number = value;
    s_ledbar.blank  = 0u;
    LedBar_RefreshOutput();
}

void LedBar_SetIndicators(uint8_t indicator_mask)
{
    LedBar_EnsureInit();

    indicator_mask = (uint8_t)(indicator_mask &
                               (LEDBAR_ICON_CHARGE_MASK |
                                LEDBAR_ICON_PERCENT_MASK));
    if ((s_ledbar.indicator_mask == indicator_mask) && (s_ledbar.blank == 0u))
    {
        return;
    }

    s_ledbar.indicator_mask = indicator_mask;
    s_ledbar.blank = 0u;
    LedBar_RefreshOutput();
}

void LedBar_SetIndicatorState(uint8_t indicator_mask, uint8_t enable)
{
    uint8_t new_mask;

    LedBar_EnsureInit();

    indicator_mask = (uint8_t)(indicator_mask &
                               (LEDBAR_ICON_CHARGE_MASK |
                                LEDBAR_ICON_PERCENT_MASK));
    if (enable != 0u)
    {
        new_mask = (uint8_t)(s_ledbar.indicator_mask | indicator_mask);
    }
    else
    {
        new_mask = (uint8_t)(s_ledbar.indicator_mask & (uint8_t)(~indicator_mask));
    }

    LedBar_SetIndicators(new_mask);
}

void LedBar_SaveSleepSoc(void)
{
    uint16_t value = (uint16_t)(LEDBAR_SLEEP_SOC_MAGIC | LedBar_GetRuntimeSoc());

    LedBar_EnableBackupAccess();
    BKP_WriteBackupRegister(LEDBAR_SLEEP_SOC_REG, value);
    BKP_WriteBackupRegister(LEDBAR_SLEEP_SOC_INV_REG, (uint16_t)(~value));
}

uint8_t LedBar_LoadSleepSoc(void)
{
    uint16_t value;
    uint16_t value_inv;
    uint8_t  soc;

    LedBar_EnableBackupAccess();
    value     = BKP_ReadBackupRegister(LEDBAR_SLEEP_SOC_REG);
    value_inv = BKP_ReadBackupRegister(LEDBAR_SLEEP_SOC_INV_REG);

    if ((uint16_t)(value ^ value_inv) != 0xFFFFu)
    {
        return 0u;
    }

    if ((value & LEDBAR_SLEEP_SOC_MAGIC_MASK) != LEDBAR_SLEEP_SOC_MAGIC)
    {
        return 0u;
    }

    soc = (uint8_t)(value & LEDBAR_SLEEP_SOC_VALUE_MASK);
    return LedBar_LimitSoc(soc);
}

void LedBar_ShowSleepSocPreview(void)
{
    uint8_t soc;

    LedBar_EnsureInit();

    soc = LedBar_LoadSleepSoc();
    s_ledbar.number = soc;
    s_ledbar.blank  = 0u;

    LedBar_ApplyFrameMask(LedBar_BuildTargetMask(soc, LEDBAR_ICON_PERCENT_MASK));
}

void LedBar_RequestSocDisplay(void)
{
    LedBar_EnsureInit();
    LedBar_RequestSocDisplayWindow();
}

void LedBar_PrepareForStop(void)
{
    LedBar_EnsureInit();

    if (s_ledbar.scan_timer_enabled != 0u)
    {
        LedScan_StopTimer();
        s_ledbar.scan_timer_enabled = 0u;
    }
    LedScan_OutputOff();
    LedScan_PrepareForStop();
}

uint8_t LedBar_IsActiveForLowPower(void)
{
    if (s_ledbar.initialized == 0u)
    {
        return 0u;
    }

    if (s_ledbar.sleep != 0u)
    {
        return 0u;
    }

    if ((s_ledbar.soc_display_10ms != 0u) ||
        (s_ledbar.frame_mask != 0u) ||
        (s_ledbar.scan_timer_enabled != 0u))
    {
        return 1u;
    }

    return 0u;
}

/* ── TIM4 scan ISR ── */

static void LedBar_Scan1ms(void)
{
    if (s_ledbar.initialized == 0u)
    {
        return;
    }

    if ((s_ledbar.sleep != 0u) || (s_ledbar.frame_mask == 0u))
    {
        LedScan_OutputOff();
        s_ledbar.scan_route = 0u;
        return;
    }

    LedScan_OutputRoute(s_ledbar.scan_route);
    s_ledbar.scan_route = LedBar_NextRouteFromMask(s_ledbar.frame_mask,
                                                   (uint8_t)(s_ledbar.scan_route + 1u));
}

void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        LedBar_Scan1ms();
    }
}

/* ── Main display task (called from Runtime) ── */

void APP_LedBar(void)
{
    uint8_t display_value;
    uint8_t indicator_mask   = LEDBAR_ICON_PERCENT_MASK;
    uint8_t display_requested;
    uint8_t mcu_wk_active;
    uint8_t discharge_mos_open;

    LedBar_EnsureInit();

    LedBar_ServiceMcuWakeFilter();
    LedBar_ServiceSwitch();
    mcu_wk_active = (s_ledbar.mcu_wk_active != 0u) ? 1u : 0u;

#if LEDBAR_SLEEP_ENABLE
    if ((LowPower_IsToSleepPending() != 0u) && (mcu_wk_active == 0u))
    {
        LedBar_SaveSleepSoc();
        LedBar_SetSleep(1u);
        return;
    }
#endif

    LedBar_ServiceStartupDisplayWindow();

    display_requested =
        (s_ledbar.soc_display_10ms != 0u) ? 1u : 0u;
#if LEDBAR_TEST_ALWAYS_ON
    display_requested = 1u;
#elif !LEDBAR_SLEEP_ENABLE
    display_requested = 1u;
#endif

    if (display_requested == 0u)
    {
        if ((s_ledbar.blank == 0u) ||
            (s_ledbar.frame_mask != 0u) ||
            (s_ledbar.scan_timer_enabled != 0u))
        {
            LedBar_Clear();
        }
        return;
    }

    if (s_ledbar.sleep != 0u)
    {
        LedBar_Wakeup();
    }

    if ((g_st_SysTimeFlag.bits.b1Sys100msFlag == 0u) &&
        (s_ledbar.blank == 0u))
    {
        return;
    }

    display_value    = LedBar_GetRuntimeSoc();
    discharge_mos_open = LedBar_IsDischargeMosOpen();

    if (discharge_mos_open != 0u)
    {
        indicator_mask |= LEDBAR_ICON_CHARGE_MASK;
    }

    if (LedBar_IsFaultActive() != 0u)
    {
    }

    if ((s_ledbar.number != display_value) ||
        (s_ledbar.indicator_mask != indicator_mask) ||
        (s_ledbar.blank != 0u))
    {
        s_ledbar.number         = display_value;
        s_ledbar.indicator_mask = indicator_mask;
        s_ledbar.blank          = 0u;
        LedBar_RefreshOutput();
    }
}
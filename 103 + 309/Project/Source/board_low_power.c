#include "main.h"
#include "low_power_port.h"

/* HS-C073: PB12 is always powered; USART RX belongs to the switched rail. */
#define BOARD_RTC_EXTI_MASK (EXTI_Line0 | EXTI_Line3 | EXTI_Line7 | EXTI_Line9 | EXTI_Line12)
#define BOARD_COMM_QUIET_MS 5U
#define BOARD_PREPARE_TIMEOUT_MS 100U
static uint32_t s_prepare_started_ms;
static uint32_t s_quiet_started_ms;
static uint32_t s_prepare_started_rtc;
static uint8_t s_comm_was_active;

uint32_t BoardLowPower_Lock(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    return mask;
}

void BoardLowPower_Unlock(uint32_t mask)
{
    __set_PRIMASK(mask);
    __ISB();
}

void BoardLowPower_Init(void)
{
    g_low_power_diag.rcc_reset_flags = RCC->CSR;
#ifdef wdog_enable
    g_low_power_diag.iwdg_compiled = 1U;
#endif
#if LOW_POWER_DEBUG_PROFILE == LP_DEBUG_REAL_STOP
    DBGMCU->CR |= DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_IWDG_STOP;
#else
    DBGMCU->CR &= ~(DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_IWDG_STOP);
#endif
}

uint32_t BoardLowPower_NowMs(void) { return System_GetUptimeMs(); }
uint32_t BoardLowPower_RtcCounter(void) { return RTC_ReadCounterCoherent(); }

void BoardLowPower_Snapshot(void)
{
    g_low_power_diag.uptime_ms = System_GetUptimeMs();
    g_low_power_diag.rtc_counter = RTC_ReadCounterCoherent();
    g_low_power_diag.gpioa_idr = GPIOA->IDR;
    g_low_power_diag.gpiob_idr = GPIOB->IDR;
    g_low_power_diag.exti_imr = EXTI->IMR;
    g_low_power_diag.exti_pr = EXTI->PR;
    g_low_power_diag.nvic_pending[0] = NVIC->ISPR[0];
    g_low_power_diag.nvic_pending[1] = NVIC->ISPR[1];
    g_low_power_diag.rtc_crl = RTC->CRL;
    g_low_power_diag.rcc_cfgr = RCC->CFGR;
    g_low_power_diag.scb_icsr = SCB->ICSR;
    g_low_power_diag.primask = __get_PRIMASK();
    g_low_power_diag.comm_pending_mask = Sci_CommonUpperPendingMask();
    g_low_power_diag.uart_partial_timeouts = g_sci_partial_timeouts;
    g_low_power_diag.uart1_errors = gu16_CommuErrCnt_SCI1;
    g_low_power_diag.uart2_errors = gu16_CommuErrCnt_SCI2;
}

uint32_t BoardLowPower_ActiveWake(void)
{
    uint32_t sources = 0U;
    uint32_t pending = EXTI->PR & EXTI->IMR;
    if (GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU) || (pending & EXTI_Line0))
        sources |= LP_WAKE_PA0;
    if (!GPIO_ReadInputDataBit(GPIO_KEY1, PIN_KEY1) || (pending & EXTI_Line9))
        sources |= LP_WAKE_KEY;
    if (GPIO_ReadInputDataBit(GPIO_INT_WK_CMNT, PIN_INT_WK_CMNT) || (pending & EXTI_Line12))
        sources |= LP_WAKE_COMM;
    return sources;
}

uint8_t BoardLowPower_Begin(void)
{
    GPIO_InitTypeDef gpio;
    uint32_t mask = BoardLowPower_Lock();
    if (!Sci_CommonUpperPrepareStop())
    {
        BoardLowPower_Unlock(mask);
        return 0U;
    }
    g_irq_t = NO_IRQ;
    is_rtc_wakekup = false;
    /* Independent PA0/key sources remain armed during the PB12 quarantine.
       PB12 still latches rising edges while its interrupt mask is clear. */
    EXTI->IMR &= ~BOARD_RTC_EXTI_MASK;
    EXTI_ClearITPendingBit(BOARD_RTC_EXTI_MASK);
    InitWakeUp_Base();
    NVIC_DisableIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    NVIC_DisableIRQ(EXTI15_10_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    gpio.GPIO_Pin = PIN_INT_WK_CMNT;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_INT_WK_CMNT, &gpio);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);
    EXTI->RTSR |= EXTI_Line12;
    EXTI->FTSR &= ~EXTI_Line12;
    EXTI_ClearITPendingBit(EXTI_Line12);
    s_prepare_started_ms = System_GetUptimeMs();
    s_prepare_started_rtc = RTC_ReadCounterCoherent();
    s_quiet_started_ms = s_prepare_started_ms;
    s_comm_was_active = 0U;
    IOstatus_RTCMode();
    BoardLowPower_Unlock(mask);
    return 1U;
}

uint8_t BoardLowPower_WaitQuiet(void)
{
    uint32_t now = System_GetUptimeMs();
    uint32_t mask = BoardLowPower_Lock();
    uint8_t active = GPIO_ReadInputDataBit(GPIO_INT_WK_CMNT, PIN_INT_WK_CMNT);
    uint8_t edge = (uint8_t)((EXTI->PR & EXTI_Line12) != 0U);
    if (edge)
    {
        ++g_low_power_diag.quarantine_edges;
        EXTI_ClearITPendingBit(EXTI_Line12);
    }
    if (active || edge || s_comm_was_active)
        s_quiet_started_ms = now;
    s_comm_was_active = active;
    BoardLowPower_Unlock(mask);
    if ((uint32_t)(RTC_ReadCounterCoherent() - s_prepare_started_rtc) >= 2U)
    {
        LowPower_RecordFault(LP_FAULT_NO_TIMEBASE);
        return 2U;
    }
    if ((uint32_t)(now - s_prepare_started_ms) >= BOARD_PREPARE_TIMEOUT_MS)
        return 2U;
    if (!active && (uint32_t)(now - s_quiet_started_ms) >= BOARD_COMM_QUIET_MS)
        return 1U;
    return 0U;
}

void BoardLowPower_ArmWake(void)
{
    NVIC_InitTypeDef nvic;
    /* Preserve pending edges after the quiet test. */
    EXTI->IMR |= EXTI_Line12;
    nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1U;
    nvic.NVIC_IRQChannelSubPriority = 1U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void BoardLowPower_PrepareNextStop(void) { IOstatus_RTCMode(); }

void BoardLowPower_Stop(void)
{
    Feed_IWatchDog;
#if LOW_POWER_DEBUG_PROFILE != LP_DEBUG_SIMULATE
    Sys_StopMode();
#else
    LowPower_NotifyWake(LP_WAKE_RTC);
#endif
}

uint8_t BoardLowPower_RtcInit(void) { return Init_RTC(); }
uint8_t BoardLowPower_RtcArm(void)
{
    is_rtc_wakekup = false;
    return RTC_WKTimeConfig();
}

uint8_t BoardLowPower_CaptureWake(void)
{
    /* Synchronize before reading RTC alarm flags after APB1 stopped. */
    uint8_t ready = RTC_SynchronizeAfterStop();
    RTC_LatchAlarmWake();
    LowPower_NotifyWake(BoardLowPower_ActiveWake());
    return ready;
}

void BoardLowPower_BeginSample(void)
{
    Feed_IWatchDog;
    initAFE1_IIC();
}

void BoardLowPower_Restore(void)
{
    uint32_t mask = BoardLowPower_Lock();
    RTC_DisableStopAlarm();
    EXTI->IMR &= ~BOARD_RTC_EXTI_MASK;
    EXTI_ClearITPendingBit(BOARD_RTC_EXTI_MASK);
    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    BoardLowPower_Unlock(mask);
    InitRunAfterStop();
}

void BoardLowPower_AccountSeconds(uint32_t seconds)
{
    su32_Interval_S_Tcnt += seconds;
}

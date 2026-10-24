/* Host stubs around the real candidate-wake and STOP-cycle functions. */
volatile enum irqWakeup g_irq_t;
volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus;
static struct { UINT32 rtc_sec_cnt; } sys_time;
static int adc_on, gpio_led, stop_rtc, wake_external, inject_frame, returned_to_sleep;
static int wake_afe, deep_requests;
static enum irqWakeup stop_source;
static UINT8 injected[9];
static UINT16 injected_count;
#define MCUO_DEBUG_LED1 gpio_led
#define Feed_IWatchDog ((void)0)
#define LP_UART_VERIFY_TIMEOUT_US 300000U
static void LowPower_DisableUartWakeup(void) { assert(!adc_on); }
static void InitTimer(void) { assert(!adc_on); clock_us=0; }
static void IOstatus_RTCMode(void) { assert(!adc_on); Sci_StopForLowPower(); ++returned_to_sleep; }
static void InitWakeUp_RTCMode(void) {}
static void InitWakeUp_Base(void) {}
static UINT8 SleepDeal_IsWakeupValid(void);
static void RTC_ClearStopWakeup(void) {}
static void RTC_WKTimeConfig(void) {}
static UINT32 RTC_GetCounter(void) { return 1; }
static void RtcSleep_PortEnterStop(void) { assert(!adc_on); g_irq_t=stop_source; }
static void initAFE1_IIC(void) { assert(!adc_on); }
static UINT8 RTC_IsStopWakeup(void) { return (UINT8)stop_rtc; }
static bool rtc_sleep_has_wakeup_exception(void) { assert(!adc_on); return wake_afe != 0; }
static UINT32 RtcSleep_PortGetLastWakeupSeconds(void) { return 1; }
static void RtcSleep_PortApplySocRtcRest(UINT32 seconds) {}
static void lp_refresh_status(void) {}
static UINT8 SystemRuntime_IsDischargeMosOpen(void) { return 1; }
static void low_power_log_and_commit_sleep(UINT8 mode) { assert(mode==DEEP_MODE); ++deep_requests; }

/* REAL_RTC_FUNCTIONS */

static void receive(struct SCI_PORT *port,UINT8 byte);
static UINT8 SleepDeal_IsWakeupValid(void)
{
    if (inject_frame)
    {
        UINT16 i;
        struct SCI_PORT *port=(stop_source == uart1_irq) ? &s_port1 : &s_port2;
        inject_frame=0;
        for (i=0;i<injected_count;++i) receive(port,injected[i]);
    }
    return (UINT8)wake_external;
}

static void test_wake_paths(void)
{
    int i, before, activity;
    UINT8 b[251];
    g_stLowPowerRtcStatus.mode=HICCUP_MODE;
    clock_step=100U;
    for (i=0;i<2;++i)
    {
        stop_source=i ? uart2_irq : uart1_irq;
        g_irq_t=stop_source;
        memcpy(injected,data1_request,7); injected_count=7; inject_frame=1;
        before=writes; activity=comm_activity;
        assert(!rtc_sleep_run_hiccup_cycle());
        assert(!adc_on && writes==before && comm_activity==activity);
        assert((i ? s_port2.frame_pending : s_port1.frame_pending)==2U);
        clock_step=0; InitUSART_CommonUpper(); App_CommonUpper();
        assert(transmit(i ? &s_port2 : &s_port1,b)==43);
        clock_step=100U;
        /* Bad checksum and foreign Modbus address cannot restore RUN. */
        memcpy(injected,data1_request,7); injected[5]^=1; inject_frame=1;
        before=returned_to_sleep;
        assert(rtc_sleep_run_hiccup_cycle() && returned_to_sleep==before+1);
        assert(clock_us >= LP_UART_VERIFY_TIMEOUT_US && !adc_on && !Sci_IsAnyPortBusy());
        modbus_request(injected,2,3,0xD000,1); injected_count=8; inject_frame=1;
        assert(rtc_sleep_run_hiccup_cycle() && !adc_on && !Sci_IsAnyPortBusy());
        memcpy(injected,data1_request,3); injected_count=3; inject_frame=1;
        assert(rtc_sleep_run_hiccup_cycle() && !adc_on && !Sci_IsAnyPortBusy());
    }
    /* Real external wake remains effective even without UART validation. */
    stop_source=uart1_irq; wake_external=1;
    assert(!rtc_sleep_run_hiccup_cycle() && !adc_on); wake_external=0;
    wake_afe=1;
    assert(!rtc_sleep_run_hiccup_cycle() && !adc_on); wake_afe=0;
    g_stCellInfoReport.u16VCellMin=2500;
    assert(rtc_sleep_run_hiccup_cycle() && deep_requests==1 && !adc_on);
    stop_source=NO_IRQ; stop_rtc=1;
    g_stCellInfoReport.u16VCellMin=3300;
    assert(rtc_sleep_run_hiccup_cycle() && !adc_on);
    stop_rtc=0; clock_step=0;
    puts("PASS STOP: both UART candidates, valid-frame hold, bad checksum/address/partial frame back to sleep, ADC stays off");
}

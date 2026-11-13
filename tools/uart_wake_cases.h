/* Real STOP cycle and communication EXTI handlers, no physical board I/O. */
volatile enum irqWakeup g_irq_t;
volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus;
static int adc_on,gpio_led,stop_rtc,wake_external,wake_afe,deep_requests,gpio_ctlc;
static int simultaneous_key;
static enum irqWakeup stop_source;
static UINT32 exti_pending;
#define MCUO_AFE_CTLC gpio_ctlc
#define MCUO_DEBUG_LED1 gpio_led
#define Feed_IWatchDog ((void)0)
#define RESET 0
#define EXTI_Line3 (1U<<3)
#define EXTI_Line5 (1U<<5)
#define EXTI_Line6 (1U<<6)
#define EXTI_Line7 (1U<<7)
#define EXTI_Line9 (1U<<9)
#define EXTI_Line12 (1U<<12)
#define EXTI_Line13 (1U<<13)
static int EXTI_GetITStatus(UINT32 line) { return (exti_pending & line)!=0U; }
static void EXTI_ClearITPendingBit(UINT32 line) { exti_pending &= ~line; }
/* REAL_WAKE_IRQS */
static UINT8 CheckChargeMa(void) { return 0; }
static UINT8 CheckDischargeMa(void) { return 0; }
static UINT8 CheckKeyActive(void) { return 0; }
static UINT8 SleepDeal_NormalIdleExpired(UINT8 activity) { return 0; }
static void LowPower_Request(enum _SLEEP_MODE mode) { g_stLowPowerRtcStatus.mode=(UINT8)mode; }
static void LowPower_DisableWakeupExti(void) {}
static void Init_RTC(void) {}
static void IOstatus_RTCMode(void) { assert(!adc_on); }
static void InitWakeUp_Base(void) {}
static UINT8 SleepDeal_IsWakeupValid(void) { return (UINT8)wake_external; }
static void RTC_ClearStopWakeup(void) {}
static void RTC_WKTimeConfig(void) {}
static UINT32 RTC_GetCounter(void) { return 1; }
static void RtcSleep_PortEnterStop(void) {
    assert(!adc_on);
    g_stLowPowerRtcStatus.comm_wakeup=0U;
    g_irq_t=NO_IRQ;
    if(stop_source==uart1_irq) { exti_pending=EXTI_Line7; EXTI9_5_IRQHandler(); }
    else if(stop_source==uart2_irq) { exti_pending=EXTI_Line3; EXTI3_IRQHandler(); }
    else if(stop_source==rs485_irq) { exti_pending=EXTI_Line12; EXTI15_10_IRQHandler(); }
    else g_irq_t=stop_source;
    if(simultaneous_key) { exti_pending=EXTI_Line9; EXTI9_5_IRQHandler(); }
}
static void initAFE1_IIC(void) { assert(!adc_on); }
static UINT8 RTC_IsStopWakeup(void) { return (UINT8)stop_rtc; }
static bool rtc_sleep_has_wakeup_exception(void) {
    assert(!adc_on);
    if(wake_afe==2) g_stLowPowerRtcStatus.mode=DEEP_MODE;
    return wake_afe != 0 || wake_external;
}
static UINT32 RtcSleep_PortGetLastWakeupSeconds(void) { return 1; }
static void RtcSleep_PortAddRuntimeSeconds(UINT32 seconds) {}
static void RtcSleep_PortApplySocRtcRest(UINT32 seconds) {}
static void low_power_log_and_commit_sleep(UINT8 mode) { assert(mode==DEEP_MODE); ++deep_requests; }
/* REAL_RTC_FUNCTIONS */
static void test_wake_paths(void) {
    int i,mode,before,activity;
    UINT8 b[251],bad[8];
    g_stLowPowerRtcStatus.mode=HICCUP_MODE;
    clock_step=0U;
    for(mode=1;mode<=3;++mode) {
        assert(StorageFlash_SetBaudMode((UINT16)mode)); InitUSART_CommonUpper();
        for(i=0;i<3;++i) {
            stop_source=i==0 ? uart1_irq : (i==1 ? uart2_irq : rs485_irq);
            before=writes; activity=comm_activity;
            assert(!rtc_sleep_run_hiccup_cycle());
            assert(g_stLowPowerRtcStatus.comm_wakeup && !adc_on && writes==before && comm_activity==activity);
            /* RTC plus a switch-release IRQ must not hide the communication edge. */
            simultaneous_key=stop_rtc=1;
            assert(!rtc_sleep_run_hiccup_cycle() && g_irq_t==soc_key && !adc_on);
            simultaneous_key=stop_rtc=0;
            InitUSART_CommonUpper(); App_CommonUpper();
            assert(writes==before && !s_port1.tx_length && !s_port2.tx_length);
            if(i<2) {
                struct SCI_PORT *port=i ? &s_port2 : &s_port1;
                /* RUN accepts a later retry, with no 300 ms wake window. */
                clock_us+=1000000U;
                request(port,data1_request,7); App_CommonUpper(); assert(transmit(port,b)==43);
                modbus_request(bad,1,6,0x1102,7); bad[7]^=1U;
                request(port,bad,8); clock_us+=Sci_FrameGapUs(port)+SCI_CLOCK_STEP_US;
                App_CommonUpper(); assert(writes==before && !port->tx_length);
                quiet(port);
            }
        }
    }
    stop_source=uart1_irq; wake_external=1;
    assert(!rtc_sleep_run_hiccup_cycle() && !adc_on); wake_external=0;
    wake_afe=1; assert(!rtc_sleep_run_hiccup_cycle() && !adc_on); wake_afe=0;
    wake_afe=2; assert(!rtc_sleep_run_hiccup_cycle() && deep_requests==1 && !adc_on);
    wake_afe=0; g_stLowPowerRtcStatus.mode=HICCUP_MODE;
    stop_source=NO_IRQ; stop_rtc=1;
    assert(rtc_sleep_run_hiccup_cycle() && !adc_on);
    stop_rtc=0;
    puts("PASS STOP edges: USART1/USART2/PB12, all baud modes, no frame/timer required, RTC+key concurrency, late RUN retry, bad CRC rejected, ADC/deep priority");
}

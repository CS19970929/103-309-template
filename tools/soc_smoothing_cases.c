/* Included after real SOC.c, upper protocol and low-power code by the host runner. */
static void smooth_tick(INT32 ma,UINT16 minimum,UINT16 maximum,UINT8 valid,UINT8 faults)
{
    host_current_ma=ma;set_voltages(minimum,maximum);host_tick+=20U;
    App_SOC(valid,faults);
}
static void smooth_seconds(UINT32 seconds,INT32 ma,UINT16 minimum,UINT16 maximum)
{
    UINT32 i;
    for(i=0;i<seconds*5U;i++) smooth_tick(ma,minimum,maximum,1U,1U);
}

static void test_smooth_full(void)
{
    UINT32 before,previous;UINT16 last;UINT32 i;
    reset_case(245,20);before=s_soc.now_as10;
    /* High voltage alone at rest is not proof of a just-completed charge. */
    smooth_seconds(20,0,3300,3550);
    assert(s_soc.now_as10==before && !SOC_HasPendingFull());
    smooth_seconds(12,200,3300,3550);
    assert(SOC_HasPendingFull() && s_soc.now_as10>before);
    previous=s_soc.now_as10;last=g_stCellInfoReport.SocElement.u16Soc;
    /* Charger stops and high cell relaxes: committed target still converges. */
    for(i=0;i<500;i++) {
        smooth_tick(0,3300,3400,1U,1U);
        assert(s_soc.now_as10>=previous && s_soc.now_as10-previous<=1765U);
        assert(g_stCellInfoReport.SocElement.u16Soc-last<=1U);
        previous=s_soc.now_as10;last=g_stCellInfoReport.SocElement.u16Soc;
    }
    assert(s_soc.now_as10==s_soc.full_as10 && last==100U && !SOC_HasPendingFull());
    assert(s_soc.cycle_x100==300U && !s_soc.passed_as10);
    reset_case(245,50);smooth_seconds(12,200,3300,3550);
    assert(SOC_HasPendingFull());before=s_soc.now_as10;
    smooth_seconds(2,-500,3300,3400);
    assert(!SOC_HasPendingFull() && s_soc.now_as10<before);
    puts("PASS full: prior charge evidence, no idle false-positive, latched until100 after voltage relaxation, bounded non-jumping correction, discharge cancel");
}

static void test_smooth_low_window(void)
{
    UINT32 entry,previous,i;UINT16 last;
    reset_case(245,50);smooth_seconds(60,-100,3051,3300);
    expect_capacity(245,expected_capacity(245,50,-100,60));
    reset_case(245,50);smooth_seconds(60,-100,3050,3300);
    expect_capacity(245,expected_capacity(245,50,-100,60));
    entry=s_soc.low_entry_as10;assert(entry!=0U);
    previous=s_soc.now_as10;last=g_stCellInfoReport.SocElement.u16Soc;
    for(i=0;i<650;i++) {
        smooth_tick(-100,3040,3300,1,1);
        assert(previous>=s_soc.now_as10 && previous-s_soc.now_as10<=178U);
        assert(last-g_stCellInfoReport.SocElement.u16Soc<=1U);
        previous=s_soc.now_as10;last=g_stCellInfoReport.SocElement.u16Soc;
    }
    assert(s_soc.now_as10<=entry*4U/5U && s_soc.now_as10>entry*4U/5U-1000U);
    for(i=0;i<4;i++) {
        UINT32 v=s_soc.now_as10;
        smooth_seconds(11,-100,3051,3300);smooth_seconds(11,-100,3040,3300);
        assert(s_soc.low_entry_as10==entry && v-s_soc.now_as10==22U);
    }
    previous=s_soc.now_as10;smooth_seconds(30,0,3020,3300);
    assert(s_soc.now_as10==previous);
    smooth_seconds(30,100,3020,3300);
    assert(s_soc.now_as10>=previous+29U && s_soc.now_as10<=previous+30U &&
           !s_soc.low_entry_as10);
    puts("PASS low: configurable 3050/AFE 3000 window, no rebound compounding, rest/charge pause and reference clear");
}

static void test_smooth_manual_bounds(void)
{
    const UINT8 targets[]={0U,100U,35U};unsigned i;
    for(i=0;i<sizeof(targets)/sizeof(targets[0]);++i) {
        UINT32 expected;UINT8 target=targets[i];
        reset_case(245,50);assert(SOC_RequestSetOnce(target));
        expected=(UINT32)((uint64_t)s_soc.full_as10*target/100U);
        assert(s_soc.now_as10==expected && stored.u32CapNow==expected &&
               stored.u16LearnAnchorSoc==SOC_NO_MANUAL_TARGET);
        smooth_seconds(20,0,3300,3300);
        assert(s_soc.now_as10==expected);
        InitData_SOC();assert(s_soc.now_as10==expected);
    }
    reset_case(245,50);assert(SOC_RequestSetOnce(100));
    smooth_seconds(60,-100,3300,3300);
    assert(s_soc.now_as10==s_soc.full_as10-60U);
    puts("PASS manual: immediate zero/full/arbitrary changes, durable ACK and no later hidden target");
}

static void test_smooth_timing_and_invalid(void)
{
    UINT32 before;unsigned i;
    reset_case(245,50);smooth_seconds(2,200,3300,3400);
    smooth_seconds(9,0,3300,3550);assert(!SOC_HasPendingFull());
    SOC_PauseCalibration();smooth_seconds(2,0,3300,3550);
    assert(!SOC_HasPendingFull());
    smooth_seconds(9,0,3300,3550);assert(SOC_HasPendingFull());
    before=s_soc.now_as10;
    for(i=0;i<1000;i++) App_SOC(1,1);
    assert(s_soc.now_as10==before);
    host_tick+=100000U;App_SOC(1,1);assert(s_soc.now_as10==before);
    for(i=0;i<30;i++) smooth_tick(0,3300,3550,0,1);
    assert(s_soc.now_as10==before);
    for(i=0;i<30;i++) smooth_tick(0,3300,3550,1,0);
    assert(s_soc.now_as10==before);
    AFE_PARAM_WRITE_Flag=1;smooth_seconds(10,0,3300,3550);
    assert(s_soc.now_as10==before);AFE_PARAM_WRITE_Flag=0;
    set_voltages(0,3550);host_tick+=20;App_SOC(1,1);assert(s_soc.now_as10==before);
    SeriesNum=17;set_voltages(3300,3550);host_tick+=20;App_SOC(1,1);
    assert(s_soc.now_as10==before);
    SeriesNum=7;host_tick=UINT32_MAX-9U;SOC_PauseCalibration();
    smooth_tick(0,3300,3300,1,1);assert(s_soc.now_as10==before);
    smooth_tick(0,3300,3300,1,1);assert(s_soc.now_as10>before);
    puts("PASS timing: no elapsed-time catch-up, STOP/invalid sample freeze, 32-bit wrap and valid restored full target");
}

static void test_smooth_restore_and_sleep(void)
{
    UINT32 entry,before;unsigned attempts;
    reset_case(245,50);smooth_seconds(150,-100,3040,3300);
    entry=s_soc.low_entry_as10;assert(entry!=0U);
    assert(SOC_SaveSnapshot());before=s_soc.now_as10;InitData_SOC();
    assert(s_soc.now_as10==before && s_soc.low_entry_as10==entry);
    SOC_NotifyOverdischarge();
    assert(s_soc.now_as10==0U && !s_soc.low_entry_as10 &&
           g_stCellInfoReport.SocElement.u16Soc==0U);
    attempts=save_attempts;g_stLowPowerRtcStatus.mode=DEEP_MODE;
    assert(LowPowerSleep_SaveResetState());
    assert(save_attempts==attempts+1 && stored.u32CapNow==0U);
    InitData_SOC();assert(s_soc.now_as10==0U);
    g_stLowPowerRtcStatus.mode=NO_SLEEP;run_seconds(20,100,3300);
    assert(s_soc.now_as10==20U);
    puts("PASS empty: trusted UV is immediate zero, saved before DEEP, reboot zero and normal charging from zero");
}

static void test_smooth_tag_and_parameter_reset(void)
{
    struct RS485MSG msg;UINT32 before;
    reset_case(245,50);stored.u16LearnState=1;InitData_SOC();
    assert(soc_percent()==60U);
    reset_case(245,50);assert(SOC_RequestSetOnce(35));InitData_SOC();
    assert(soc_percent()==35U && stored.u16LearnAnchorSoc==SOC_NO_MANUAL_TARGET);
    stored.u16LearnState=0x534DU;InitData_SOC();assert(soc_percent()==60U);
    reset_case(245,50);smooth_seconds(12,200,3300,3550);
    assert(SOC_HasPendingFull());before=s_soc.now_as10;
    write_parameter(0x231A,3600,&msg);
    assert(msg.AckType==RS485_ACK_POS && s_soc.now_as10==before &&
           !SOC_HasPendingFull());
    smooth_seconds(20,0,3300,3550);assert(s_soc.now_as10==before);
    write_parameter(0x231B,3100,&msg);
    assert(msg.AckType==RS485_ACK_POS && !s_soc.calibration_flags);
    assert(SOC_SaveSnapshot());InitData_SOC();
    assert(!SOC_HasPendingFull() && s_soc.now_as10==before);
    puts("PASS reset: tag guard, immediate host override and SOC100/SOC0 parameter invalidation");
}

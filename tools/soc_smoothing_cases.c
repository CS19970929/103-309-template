/* Included after the real SOC C and protocol/low-power code by the host runner. */
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
    UINT32 before,previous,i;UINT16 last;
    reset_case(245,20);before=s_soc.now_as10;
    smooth_seconds(10,0,3100,3550);
    assert(s_soc.now_as10==before); /* The first sample starts the clock. */
    previous=before;last=g_stCellInfoReport.SocElement.u16Soc;
    for(i=0;i<4001;i++) {
        smooth_tick(0,3100,3550,1,1);
        assert(s_soc.now_as10>=previous && s_soc.now_as10-previous<=177U);
        assert(g_stCellInfoReport.SocElement.u16Soc-last<=1U);
        previous=s_soc.now_as10;last=g_stCellInfoReport.SocElement.u16Soc;
    }
    assert(s_soc.now_as10==s_soc.full_as10 && last==100U);
    assert(s_soc.cycle_x100==300U && !s_soc.passed_as10);
    /* Neither tail current nor minimum-cell voltage is a full qualifier. */
    reset_case(245,50);smooth_seconds(30,1,3100,3550);assert(s_soc.now_as10>441000U);
    reset_case(245,50);smooth_seconds(30,500,3100,3550);assert(s_soc.now_as10>441150U);
    reset_case(245,50);smooth_seconds(60,-100,3100,3550);
    expect_capacity(245,expected_capacity(245,50,-100,60));
    before=s_soc.now_as10;smooth_seconds(2,0,3100,3550);
    assert(s_soc.now_as10>before); /* A negative pulse cannot permanently disarm voltage full. */
    reset_case(245,20);g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp=1;
    smooth_tick(0,3100,3550,1,1);assert(s_soc.now_as10==176400U);
    smooth_tick(0,3100,3550,1,1);assert(s_soc.now_as10>176400U && s_soc.now_as10<176578U);
    puts("PASS full smoothing: maximum cell only, zero/tiny current, no discharge rise, OV never jumps, 200ms/whole-ramp bounds");
}

static void test_smooth_low_window(void)
{
    UINT32 entry,previous,i;UINT16 last;
    reset_case(245,50);smooth_seconds(600,-100,3051,3300);
    expect_capacity(245,expected_capacity(245,50,-100,600));
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
    smooth_tick(1,3040,3300,1,1);assert(s_soc.low_entry_as10==entry);
    smooth_seconds(11,-100,3040,3300);assert(s_soc.low_entry_as10==entry);
    /* Rebounds and new 10s confirmations keep the original reference. */
    for(i=0;i<4;i++) {
        UINT32 before=s_soc.now_as10;
        smooth_seconds(11,-100,3051,3300);smooth_seconds(11,-100,3040,3300);
        assert(s_soc.low_entry_as10==entry && before-s_soc.now_as10==22U);
    }
    previous=s_soc.now_as10;smooth_seconds(30,0,3020,3300);assert(s_soc.now_as10==previous);
    smooth_seconds(30,100,3020,3300);
    assert(s_soc.now_as10>=previous+29U && s_soc.now_as10<=previous+30U && !s_soc.low_entry_as10);
    assert(s_soc.cycle_x100==300U && s_soc.passed_as10<1000U);
    puts("PASS low window:3051mV integral only,3050mV entry,3040mV bounded descent, no rebound compounding, idle/charge stop, real-current cycles only");
}

static void test_smooth_manual_bounds(void)
{
    UINT32 i,before;int64_t delta;
    const UINT8 targets[]={0,100,35};unsigned t;
    for(t=0;t<3;t++) {
        reset_case(245,50);assert(SOC_RequestSetOnce(targets[t]));
        assert(s_soc.now_as10==441000U);set_voltages(3300,3300);
        for(i=0;i<1000;i++) {
            before=s_soc.now_as10;smooth_tick(0,3300,3300,1,1);
            delta=(int64_t)s_soc.now_as10-before;assert(delta>=-177 && delta<=177);
        }
        assert(s_soc.now_as10>264000U && s_soc.now_as10<618000U);
    }
    reset_case(245,50);assert(SOC_RequestSetOnce(0));smooth_seconds(60,-100,3051,3300);
    expect_capacity(245,expected_capacity(245,50,-100,60));
    reset_case(245,50);assert(SOC_RequestSetOnce(100));smooth_seconds(60,-100,3040,3300);
    expect_capacity(245,expected_capacity(245,50,-100,60));
    puts("PASS manual smoothing: both directions limited, no command jump, discharge obeys3050mV and never calibrates upward");
}

static void test_smooth_timing_and_invalid(void)
{
    UINT32 before;unsigned i;
    reset_case(245,50);assert(SOC_RequestSetOnce(0));smooth_seconds(1,0,3300,3300);
    before=s_soc.now_as10;
    for(i=0;i<10000;i++) App_SOC(1,1);
    assert(s_soc.now_as10==before); /* No time, no correction budget. */
    host_tick+=100000U;App_SOC(1,1);assert(s_soc.now_as10==before);
    SOC_PauseCalibration();host_tick+=20U;App_SOC(1,1);assert(s_soc.now_as10==before);
    for(i=0;i<40;i++) smooth_tick(0,3300,3300,0,1);
    assert(s_soc.now_as10==before);
    for(i=0;i<40;i++) smooth_tick(0,3300,3300,1,0);
    assert(s_soc.now_as10==before);
    AFE_PARAM_WRITE_Flag=1;smooth_seconds(20,0,3300,3300);assert(s_soc.now_as10==before);
    AFE_PARAM_WRITE_Flag=0;set_voltages(3300,3300);g_stCellInfoReport.u16VCell[2]=61001;
    for(i=0;i<40;i++) {host_tick+=20U;App_SOC(1,1);} assert(s_soc.now_as10==before);
    set_voltages(0,3550);for(i=0;i<40;i++) {host_tick+=20U;App_SOC(1,1);} assert(s_soc.now_as10==before);
    SeriesNum=17;set_voltages(3300,3300);host_tick+=20U;App_SOC(1,1);assert(s_soc.now_as10==before);
    SeriesNum=7;host_tick=UINT32_MAX-9U;SOC_PauseCalibration();
    smooth_tick(0,3300,3300,1,1);assert(s_soc.now_as10==before);
    smooth_tick(0,3300,3300,1,1);assert(before-s_soc.now_as10==176U);
    /* No stale high-voltage confirmation can survive STOP or a failed read. */
    reset_case(245,50);smooth_seconds(9,0,3100,3550);SOC_PauseCalibration();
    before=s_soc.now_as10;smooth_seconds(2,0,3100,3550);assert(s_soc.now_as10==before);
    smooth_tick(0,3100,3550,0,1);smooth_seconds(9,0,3100,3550);assert(s_soc.now_as10==before);
    puts("PASS smoothing timing: real RUN time, backlog/STOP/read gap/wrap, full reconfirmation, invalid cells/status/AFE parameters");
}

static void test_smooth_restore_and_sleep(void)
{
    UINT32 before,entry;unsigned attempts;
    reset_case(245,50);smooth_seconds(150,-100,3040,3300);entry=s_soc.low_entry_as10;
    assert(SOC_SaveSnapshot());before=s_soc.now_as10;InitData_SOC();
    assert(s_soc.now_as10==before && s_soc.low_entry_as10==entry);
    smooth_seconds(30,-100,3040,3300);assert(s_soc.now_as10==before-30U);
    SOC_NotifyOverdischarge();assert(s_soc.now_as10==before-30U);
    before=s_soc.now_as10;attempts=save_attempts;g_stLowPowerRtcStatus.mode=DEEP_MODE;
    assert(LowPowerSleep_SaveResetState());assert(save_attempts==attempts+1 && stored.u32CapNow==before);
    InitData_SOC();assert(s_soc.now_as10==before && (s_soc.calibration_flags&SOC_TARGET_EMPTY));
    smooth_seconds(20,-100,3020,3300);assert(s_soc.now_as10==before); /* DEEP: no business. */
    g_stLowPowerRtcStatus.mode=NO_SLEEP;g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp=1;
    smooth_seconds(20,100,3020,3300);
    assert(s_soc.now_as10==before+20U && !(s_soc.calibration_flags&SOC_TARGET_EMPTY) && !s_soc.low_entry_as10);
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp=0;
    before=s_soc.now_as10;run_seconds(20,100,3300);assert(s_soc.now_as10==before+20U);
    /* A persisted zero target also remains inert above3050mV after wake. */
    SOC_NotifyOverdischarge();assert(SOC_SaveSnapshot());InitData_SOC();before=s_soc.now_as10;
    run_seconds(60,-100,3051);assert(s_soc.now_as10==before-60U);
    puts("PASS smoothing resume: exact capacity/reference restore, UV target only, immediate DEEP save, no reboot zero, charge cancels old UV target");
}

static void test_smooth_tag_and_parameter_reset(void)
{
    struct RS485MSG msg;UINT32 before;
    reset_case(245,50);stored.u16Flags=0xFFFF;stored.u16LearnAnchorSoc=0;
    stored.u16LearnState=1;InitData_SOC();
    assert(s_soc.manual_target==SOC_NO_MANUAL_TARGET && soc_percent()==60U);
    reset_case(245,50);
    assert(SOC_RequestSetOnce(35));run_seconds(30,0,3300);assert(SOC_SaveSnapshot());
    before=s_soc.now_as10;InitData_SOC();
    assert(s_soc.now_as10==before && s_soc.manual_target==35U);
    /* Unsupported historical tags are ignored rather than decoded. */
    stored.u16LearnState=0x534DU;InitData_SOC();
    assert(soc_percent()==60U && s_soc.manual_target==SOC_NO_MANUAL_TARGET);
    reset_case(245,50);smooth_seconds(9,0,3100,3550);before=s_soc.now_as10;
    write_parameter(0x231A,3600,&msg);assert(msg.AckType==RS485_ACK_POS && s_soc.now_as10==before);
    smooth_seconds(20,0,3100,3550);assert(s_soc.now_as10==before);
    write_parameter(0x231A,3550,&msg);smooth_seconds(9,0,3100,3550);assert(s_soc.now_as10==before);
    smooth_seconds(2,0,3100,3550);assert(s_soc.now_as10>before);
    assert(SOC_SaveSnapshot());before=s_soc.now_as10;
    OtherElement.u16Soc_V_100=3600;InitData_SOC();
    assert(s_soc.now_as10==before && !(s_soc.calibration_flags&(SOC_TARGET_FULL|SOC_FULL_LATCHED)));
    assert(stored.u16MaxErrorPercent==3600 && !(stored.u16Flags&(SOC_TARGET_FULL|SOC_FULL_LATCHED)));
    smooth_seconds(20,0,3100,3550);assert(s_soc.now_as10==before);
    puts("PASS smoothing restore: current tag, unsupported history discarded, full-voltage change");
}

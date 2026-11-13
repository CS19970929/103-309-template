/* Real SOC, parameter writer and Flash journal in the same power-cut fixture. */
static INT32 persistence_current_ma;
static UINT8 persistence_current_fresh=1U;
INT32 AfeCurrent_GetCurrent_mA(void) { return persistence_current_ma; }
UINT8 Fault_IsCurrentSampleFresh(void) { return persistence_current_fresh; }
struct {UINT16 u16VCellMin,u16VCellMax,u16VCell[32];
    struct {struct {UINT8 b1CellUvp,b1CellOvp;} bits;} unMdlFault_Third;
    struct {UINT16 u16Soc,u16Soh,u16CapacityNow,u16CapacityFull,u16CapacityFactory,u16Cycle_times;} SocElement;
} g_stCellInfoReport;
enum {NORMAL_MODE,HICCUP_MODE,DEEP_MODE,NO_SLEEP};
static struct {UINT8 mode;} g_stLowPowerRtcStatus;
#define RS485_CMD_ADDR_BALANCE_OV 0x2300U
#define RS485_ACK_POS 0U
#define RS485_ACK_NEG 1U
#define RS485_ERROR_CMD_INVALID 4U
#define RS485_ERROR_DATA_INVALID 3U
struct RS485MSG {UINT8 u16Buffer[80],AckType,ErrorType;};
int AFE_PARAM_WRITE_Flag;
/* SOC_AND_PARAMETER_SOURCE */

static void persistence_boot(void)
{
    unsigned i;
    memset(&s_flash,0,sizeof(s_flash));
    persistence_current_ma=0;tick=0;
    persistence_current_fresh=1U;g_stLowPowerRtcStatus.mode=NO_SLEEP;
    assert(InitE2PROM());
    InitData_SOC();
    AFE_PARAM_WRITE_Flag=0;
    g_stCellInfoReport.u16VCellMin=g_stCellInfoReport.u16VCellMax=3300U;
    for(i=0;i<32;i++) g_stCellInfoReport.u16VCell[i]=i<SeriesNum ? 3300U : 61001U;
}

static void persistence_prepare(void)
{
    STORAGE_FLASH_SOC_DATA data;
    STORAGE_FLASH_FACTORY_AGING_DATA aging={1234,FLASH_FACTORY_AGING_STATE_RUNNING,12};
    memset((void *)FLASH_PARAMETER_RESET_BEGIN,0xFF,16384);
    memset((void *)FLASH_STATS_A,0x5A,2048);
    memset(s_param_versions,0,sizeof(s_param_versions));reset_on_version=0;
    cut_after=-1;error_after_commit=0;
    persistence_boot();
    assert(StorageFlash_LoadSocData(&data));
    data.u16SocNow=80U;data.u32CapNow=data.u32CapFull*80U/100U;
    data.u32CycleTimes=12345;data.u32LearnPassedAs10=1234;
    assert(StorageFlash_SaveSocData(&data));InitData_SOC();
    assert(test_save_aging(&aging));
}

static void test_soc_smooth_persistence(void)
{
    STORAGE_FLASH_SOC_DATA old,data;
    int total,cut,ack;
    unsigned i;
    persistence_prepare();assert(StorageFlash_LoadSocData(&old));
    /* Start from a shipped V2 smooth snapshot; reads must not force a rewrite. */
    old.u16LearnState=STORAGE_SOC_SMOOTH_TAG;old.u16SocNow=80U;
    old.u16Reserved[STORAGE_SOC_SMOOTH_INV_INDEX]=(UINT16)(STORAGE_SOC_SMOOTH_TAG^0xFFFFU);
    assert(StorageFlash_SaveSocData(&old));mutations=0;persistence_boot();assert(!mutations);
    assert(sizeof(old)==40U && offsetof(STORAGE_FLASH_SOC_DATA,u32CapNow)==12U &&
        offsetof(STORAGE_FLASH_SOC_DATA,u16Reserved)==30U);
    save_flash();mutations=0;
    assert(SOC_RequestSetOnce(35));total=mutations;
    assert(StorageFlash_LoadSocData(&data));
    assert(data.u16LearnState==STORAGE_SOC_STATE_TAG && !data.u16SocNow &&
        !data.u16DsgSocInt && !data.u16Reserved[STORAGE_SOC_SMOOTH_INV_INDEX]);
    /* Without an inverse word, CRC still rejects a torn/altered target. */
    {
        UINT32 address=Storage_StateLatest(FLASH_STATE_A)+sizeof(STORAGE_RECORD)+
            offsetof(STORAGE_STATE,soc)+offsetof(STORAGE_FLASH_SOC_DATA,u16LearnAnchorSoc);
        *(UINT8 *)address^=1U;
        persistence_boot();assert(s_soc.manual_target==SOC_NO_MANUAL_TARGET);
        assert(s_soc.now_as10==old.u32CapNow && StorageFlash_LoadSocData(&data));
        assert(data.u16LearnState==STORAGE_SOC_SMOOTH_TAG);
    }
    for(cut=0;cut<=total;cut++) {
        restore_flash();persistence_boot();mutations=0;cut_after=cut;
        ack=SOC_RequestSetOnce(35);cut_after=-1;persistence_boot();
        assert(StorageFlash_LoadSocData(&data));
        assert(data.u32CapNow==old.u32CapNow && s_soc.now_as10==old.u32CapNow);
        assert((s_soc.manual_target==35U)==(ack!=0));
    }
    printf("PASS smooth target transaction: %d half-word cuts, legacy/new layout, CRC rejection, ACK iff target committed, request/reboot capacity unchanged\n",total+1);
    persistence_prepare();assert(SOC_RequestSetOnce(35));
    AFE_PARAM_WRITE_Flag=0;
    g_stCellInfoReport.u16VCellMin=g_stCellInfoReport.u16VCellMax=3300U;
    for(i=0;i<32;i++) g_stCellInfoReport.u16VCell[i]=i<SeriesNum ? 3300U : 61001U;
    for(i=0;i<150;i++) {tick+=20U;App_SOC(1,1);}
    assert(StorageFlash_LoadSocData(&old));save_flash();mutations=0;
    assert(SOC_SaveSnapshot());total=mutations;
    assert(StorageFlash_LoadSocData(&data));
    for(cut=0;cut<=total;cut++) {
        UINT32 before;
        restore_flash();persistence_boot();mutations=0;cut_after=cut;
        /* Restart the interrupted final save with exactly the same payload. */
        (void)StorageFlash_SaveSocData(&data);cut_after=-1;persistence_boot();
        assert(s_soc.now_as10==old.u32CapNow || s_soc.now_as10==data.u32CapNow);
        assert(s_soc.manual_target==35U);before=s_soc.now_as10;
        tick+=100000U;App_SOC(1,1);assert(s_soc.now_as10==before);
        tick+=20U;App_SOC(1,1);assert(before-s_soc.now_as10>0U && before-s_soc.now_as10<=177U);
    }
    printf("PASS smooth progress journal: %d half-word cuts, old/new complete capacity and target retained, no boot catch-up\n",total+1);
    persistence_prepare();persistence_current_ma=-100;
    g_stCellInfoReport.u16VCellMin=g_stCellInfoReport.u16VCell[1]=3040U;
    for(i=0;i<150;i++) {tick+=20U;App_SOC(1,1);}
    {
        UINT32 entry=s_soc.low_entry_as10;
        assert(entry!=0U && StorageFlash_LoadSocData(&old));save_flash();mutations=0;
        assert(SOC_SaveSnapshot());total=mutations;assert(StorageFlash_LoadSocData(&data));
        for(cut=0;cut<=total;cut++) {
            restore_flash();persistence_boot();mutations=0;cut_after=cut;
            (void)StorageFlash_SaveSocData(&data);cut_after=-1;persistence_boot();
            assert(s_soc.now_as10==old.u32CapNow || s_soc.now_as10==data.u32CapNow);
            assert(s_soc.low_entry_as10==entry);
            persistence_current_ma=-100;g_stCellInfoReport.u16VCellMin=g_stCellInfoReport.u16VCell[1]=3040U;
            for(i=0;i<60;i++) {tick+=20U;App_SOC(1,1);}
            assert(s_soc.low_entry_as10==entry); /* Never rebuild from a lower restored capacity. */
        }
    }
    printf("PASS low reference journal: %d half-word cuts, exact entry/capacity restore, no compounded tail correction\n",total+1);
}

static void persistence_write(UINT16 value,UINT8 with_capacity,struct RS485MSG *msg)
{
    memset(msg,0,sizeof(*msg));
    msg->u16Buffer[5]=(UINT8)(with_capacity ? 2 : 1);
    msg->u16Buffer[6]=(UINT8)(with_capacity ? 4 : 2);
    if(with_capacity) {
        msg->u16Buffer[7]=1;msg->u16Buffer[8]=0xEA; /* 490:49.0 Ah */
        msg->u16Buffer[9]=(UINT8)(value>>8);msg->u16Buffer[10]=(UINT8)value;
    } else {
        msg->u16Buffer[7]=(UINT8)(value>>8);msg->u16Buffer[8]=(UINT8)value;
    }
    Sci_WrRegs_0x10_OtherElement((UINT16)(with_capacity ? 0x2318 : 0x2319),msg);
}

static void persistence_cycle_cuts(UINT8 rotation,UINT8 with_capacity,UINT8 same_value)
{
    STORAGE_FLASH_SOC_DATA data;
    STORAGE_FLASH_CONFIG_DATA config;
    STORAGE_FLASH_FACTORY_AGING_DATA aging;
    struct RS485MSG msg;
    int total,cut,new_config=0;
    UINT16 target;
    persistence_prepare();target=same_value ? OtherElement.u16Soc_Cycle_times : 1U;
    if(rotation) {
        unsigned guard;
        for(guard=0;guard<30;guard++) {
            if((Storage_StateLatest(FLASH_STATE_A)&1023U)==14U*STORAGE_STATE_SPAN) break;
            assert(test_load_aging(&aging));++aging.u32Elapsed10ms;
            assert(test_save_aging(&aging));
        }
        assert(guard<30);
    }
    save_flash();mutations=0;persistence_write(target,with_capacity,&msg);
    assert(msg.AckType==RS485_ACK_POS);total=mutations;
    for(cut=0;cut<=total;cut++) {
        UINT8 committed;
        restore_flash();persistence_boot();mutations=0;cut_after=cut;
        persistence_write(target,with_capacity,&msg);
        assert(StorageFlash_LoadConfig(&config));
        committed=(UINT8)(config.rw.reserved[STORAGE_SOC_CYCLE_GENERATION_INDEX]!=STORAGE_SOC_CYCLE_GENERATION_NONE);
        assert((msg.AckType==RS485_ACK_POS)==committed);
        cut_after=-1;persistence_boot();assert(StorageFlash_LoadSocData(&data));
        assert(g_stCellInfoReport.SocElement.u16Soc==80);
        if(committed) {
            ++new_config;
            assert(data.u32CycleTimes==(UINT32)target*100U && !data.u32LearnPassedAs10 && !data.u16DsgSocInt);
            assert(data.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX]==config.rw.reserved[STORAGE_SOC_CYCLE_GENERATION_INDEX]);
            assert(OtherElement.u16Soc_Ah==(with_capacity ? 490U : 245U));
        } else {
            assert(data.u32CycleTimes==12345 && data.u32LearnPassedAs10==1234);
            assert(OtherElement.u16Soc_Ah==245U);
        }
        assert(g_stCellInfoReport.SocElement.u16Cycle_times==(committed ? target : 123U));
        assert(g_stCellInfoReport.SocElement.u16CapacityNow==(UINT16)(OtherElement.u16Soc_Ah*8U));
        assert(!memcmp((void *)FLASH_LOG_A,saved+(FLASH_LOG_A-FLASH_PARAMETER_RESET_BEGIN),4096));
        mutations=0;persistence_boot();assert(!mutations); /* Recovery is idempotent. */
    }
    printf("PASS SOC cycle transaction: %d cuts, rotation%u mixed-capacity%u same-value%u; committed=%d, no stale cycle/SOH\n",total+1,rotation,with_capacity,same_value,new_config);
}

static void test_soc_cycle_persistence(void)
{
    STORAGE_FLASH_CONFIG_DATA config;
    STORAGE_FLASH_SOC_DATA data;
    struct RS485MSG msg;
    unsigned recovery_cuts=0;int total,cut;
    persistence_cycle_cuts(0,0,0);persistence_cycle_cuts(1,0,0);
    persistence_cycle_cuts(0,1,0);persistence_cycle_cuts(0,0,1);
    persistence_prepare();OtherElement.u16Soc_Cycle_times=42;
    assert(EEPROM_SaveRWParametersToFlash(1U));save_flash();mutations=0;
    assert(InitE2PROM());total=mutations;
    for(cut=0;cut<=total;cut++) {
        restore_flash();mutations=0;cut_after=cut;
        if(!InitE2PROM()) assert(errors); /* Boot must not run with incomplete recovery. */
        cut_after=-1;persistence_boot();assert(StorageFlash_LoadSocData(&data));
        assert(data.u32CycleTimes==4200 && !data.u32LearnPassedAs10);
        mutations=0;persistence_boot();assert(!mutations);++recovery_cuts;
    }
    /* Complete a committed cycle command before a SOC module default update. */
    restore_flash();s_param_versions[STORAGE_PARAM_SOC]=123;
    persistence_boot();assert(StorageFlash_LoadSocData(&data));
    assert(data.u32CycleTimes==4200 && g_stCellInfoReport.SocElement.u16Cycle_times==42);
    assert(OtherElement.u16Soc_Cycle_times!=42); /* New config default did not cancel the command. */
    /* After completion, normal accumulated cycles must not be reset on boot. */
    data.u32CycleTimes=4321;data.u32LearnPassedAs10=432;
    assert(StorageFlash_SaveSocData(&data));persistence_boot();assert(StorageFlash_LoadSocData(&data));
    assert(data.u32CycleTimes==4321 && data.u32LearnPassedAs10==432);
    /* Generation wrap skips a stale snapshot tag, so it cannot look applied. */
    persistence_prepare();assert(StorageFlash_LoadConfig(&config));
    config.rw.reserved[STORAGE_SOC_CYCLE_GENERATION_INDEX]=0xFFFE;
    assert(StorageFlash_SaveConfig(&config));assert(StorageFlash_LoadSocData(&data));
    data.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX]=1;
    assert(StorageFlash_SaveSocData(&data));OtherElement.u16Soc_Cycle_times=7;
    assert(EEPROM_SaveRWParametersToFlash(1U));assert(StorageFlash_GetSocCycleGeneration()==2);
    persistence_boot();assert(g_stCellInfoReport.SocElement.u16Cycle_times==7);
    /* Whole-Other defaults are also explicit cycle writes; failures have no SOC side effects. */
    persistence_prepare();cut_after=0;mutations=0;memset(&msg,0,sizeof(msg));msg.u16Buffer[5]=1;
    Sci_WrReg_0x06_Reset_OtherCanAdd(&msg);
    assert(msg.AckType==RS485_ACK_NEG && g_stCellInfoReport.SocElement.u16Cycle_times==123);
    cut_after=-1;memset(&msg,0,sizeof(msg));msg.u16Buffer[5]=1;
    Sci_WrReg_0x06_Reset_OtherCanAdd(&msg);assert(msg.AckType==RS485_ACK_POS);
    persistence_boot();assert(g_stCellInfoReport.SocElement.u16Cycle_times==OtherElement.u16Soc_Cycle_times);
    printf("PASS SOC cycle recovery: %u interrupted boot retries; module update, accumulated cycles, tag wrap, defaults failure and legacy snapshots\n",recovery_cuts);
}

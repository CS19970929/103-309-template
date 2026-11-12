#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int32_t INT32; typedef int FLASH_Status;
#include "conf/Project_Config.h"
#include "Flash.h"
#define SOC_TABLE_SIZE 42
/* PARAMETER_TYPES */
struct OTHER_ELEMENT OtherElement;
struct { UINT16 u16VCellMin,u16VCellMax; struct {
    UINT16 u16Soc,u16Soh,u16CapacityNow,u16CapacityFull,u16CapacityFactory,u16Cycle_times;
} SocElement; } g_stCellInfoReport;
static STORAGE_FLASH_SOC_DATA stored;
static UINT8 stored_valid,write_failure,current_fresh=1U;
static unsigned transient_save_failures, sleep_save_errors, sleep_watchdog_feeds;
#define ERROR_EEPROM_STORE 1
#define Feed_IWatchDog (++sleep_watchdog_feeds)
static void System_ERROR_UserCallback(int error) { assert(error==ERROR_EEPROM_STORE); ++sleep_save_errors; }
static INT32 host_current_ma;
static unsigned save_attempts;
static UINT32 host_tick;
static UINT32 SysTime_Get10msTickCount(void) { return host_tick; }
UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data) {
    if(!stored_valid) return 0U;
    *data=stored; return 1U;
}
UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data) {
    ++save_attempts;
    if(write_failure) return 0U;
    if(transient_save_failures) { --transient_save_failures; return 0U; }
    stored=*data; stored_valid=1U; return 1U;
}
INT32 AfeCurrent_GetCurrent_mA(void) { return host_current_ma; }
UINT8 Fault_IsCurrentSampleFresh(void) { return current_fresh; }

#define AFE_TYPE 1
#define sh36xx 1
#define bq76xx_afe 0
#define E2P_PARA_NUM_OTHER_ELEMENT1 32
#define RS485_CMD_ADDR_BALANCE_OV 0x2300U
#define RS485_ACK_POS 0U
#define RS485_ACK_NEG 1U
#define RS485_ERROR_CMD_INVALID 4U
#define RS485_ERROR_DATA_INVALID 3U
struct RS485MSG { UINT8 u16Buffer[80],AckType,ErrorType; };
static UINT8 AFE_PARAM_WRITE_Flag,SeriesNum,parameter_failure;
static UINT32 g_u32CS_Res_AFE;
static UINT16 cycle_generation=STORAGE_SOC_CYCLE_GENERATION_NONE;
UINT16 StorageFlash_GetSocCycleGeneration(void) { return cycle_generation; }
static UINT8 EEPROM_SaveRWParametersToFlash(UINT8 cycle_count_written) {
    if(parameter_failure) return 0U;
    if(cycle_count_written) cycle_generation=cycle_generation>=0xFFFEU ? 1U : (UINT16)(cycle_generation+1U);
    return 1U;
}
enum {NORMAL_MODE,HICCUP_MODE,DEEP_MODE,NO_SLEEP};
static struct {UINT8 mode;} g_stLowPowerRtcStatus;
static unsigned can_sleeps,statistics_flushes;
static void Can_PrepareSleep(void) { ++can_sleeps; }
void StorageFlash_FlushStatistics(void) { ++statistics_flushes; }
/* PRODUCTION_SOURCE */
static void reset_case(UINT16 capacity_a10,UINT8 soc) {
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    memset(&stored,0,sizeof(stored));
    OtherElement.u16Soc_Ah=capacity_a10; OtherElement.u16Soc_Cycle_times=3U;
    OtherElement.u16Soc_V_100=3550U; OtherElement.u16Soc_V_0=3050U;
    transient_save_failures=sleep_save_errors=sleep_watchdog_feeds=0;
    cycle_generation=STORAGE_SOC_CYCLE_GENERATION_NONE;
    stored_valid=write_failure=parameter_failure=0U;current_fresh=1U;host_current_ma=0;save_attempts=0U;host_tick=0;
    InitData_SOC();SOC_RequestSetOnce(soc);
}
static void run_seconds(UINT32 seconds,INT32 measured_ma,UINT16 voltage_mv) {
    UINT32 ticks=seconds*5U;
    host_current_ma=measured_ma;
    g_stCellInfoReport.u16VCellMin=g_stCellInfoReport.u16VCellMax=voltage_mv;
    while(ticks-- != 0U) { host_tick+=20U; App_SOC(); }
}
static UINT32 expected_capacity(UINT16 capacity_a10,UINT8 start,INT32 ma,UINT32 seconds) {
    int64_t full=(int64_t)capacity_a10*3600;
    int64_t cap=full*start/100+((int64_t)ma)*seconds/100;
    if(cap<0) cap=0;
    if(cap>full) cap=full;
    return (UINT32)cap;
}
static void expect_capacity(UINT16 capacity_a10,UINT32 cap) {
    UINT32 full=(UINT32)capacity_a10*3600U;
    UINT32 soc=(UINT32)(((uint64_t)cap*100U+full/2U)/full);
    assert(g_stCellInfoReport.SocElement.u16Soc==soc);
    assert(g_stCellInfoReport.SocElement.u16CapacityNow==((cap+180U)/360U>65535U ? 65535U : (cap+180U)/360U));
}
static void test_startup(void) {
    const UINT16 volts[]={0,2000,3300,3400,3550,4200,61001};unsigned i;
    for(i=0;i<sizeof(volts)/sizeof(volts[0]);++i) {
        reset_case(245,80);stored_valid=0;
        g_stCellInfoReport.u16VCellMin=g_stCellInfoReport.u16VCellMax=volts[i];
        InitData_SOC();assert(g_stCellInfoReport.SocElement.u16Soc==60);
        assert(stored_valid && stored.u16SocNow==60);
    }
    puts("PASS startup: no snapshot starts at60 independent of cell voltage");
}
static void test_voltage_independence(void) {
    const UINT16 volts[]={0,2000,2850,3000,3050,3100,3300,3400,3450,3451,3550,4180,5000,61001};unsigned i;
    for(i=0;i<sizeof(volts)/sizeof(volts[0]);++i) {
        reset_case(245,80);run_seconds(1800,-1100,volts[i]);
        expect_capacity(245,expected_capacity(245,80,-1100,1800));
        assert(stored.u16Flags==0);
    }
    reset_case(245,68);run_seconds(1800,0,3550);
    expect_capacity(245,expected_capacity(245,68,0,1800));
    puts("PASS voltage: no full/empty/OCV correction across14 voltage inputs");
}
static void test_current_resolution(void) {
    const INT32 currents[]={-6000,-1100,-200,-199,-101,-1,0,1,101,199,200,1100,6000};unsigned i;
    for(i=0;i<sizeof(currents)/sizeof(currents[0]);++i) {
        reset_case(245,50);run_seconds(3600,currents[i],3300);
        expect_capacity(245,expected_capacity(245,50,currents[i],3600));
    }
    puts("PASS current: signed milliamp integration including sub200mA and remainder");
}
static void test_customer_case(void) {
    UINT32 cap;
    reset_case(245,100);run_seconds(1800,-1100,3400);
    cap=expected_capacity(245,100,-1100,1800);expect_capacity(245,cap);
    assert(g_stCellInfoReport.SocElement.u16Soc==98);
    run_seconds(7200,-6000,3300);
    cap-=((UINT32)6000)*7200U/100U;expect_capacity(245,cap);
    assert(g_stCellInfoReport.SocElement.u16Soc==49);
    printf("PASS customer: 1.1A30min ->98; then6A2h ->49; remaining=%u*10mAh\n",g_stCellInfoReport.SocElement.u16CapacityNow);
}
static void test_bounds(void) {
    reset_case(245,99);run_seconds(3600,6000,3400);assert(g_stCellInfoReport.SocElement.u16Soc==100);
    run_seconds(3600,6000,3400);assert(g_stCellInfoReport.SocElement.u16Soc==100);
    reset_case(245,1);run_seconds(3600,-6000,3400);assert(!g_stCellInfoReport.SocElement.u16Soc);
    run_seconds(3600,-6000,3400);assert(!g_stCellInfoReport.SocElement.u16CapacityNow);
    puts("PASS bounds: charge reaches100 without voltage anchor; saturated endpoints");
}
static void test_manual_and_restore(void) {
    const UINT8 values[]={0,1,35,60,99,100};unsigned i;
    for(i=0;i<sizeof(values)/sizeof(values[0]);++i) {
        reset_case(245,80);SOC_RequestSetOnce(values[i]);
        assert(stored.u16SocNow==values[i]);InitData_SOC();
        assert(g_stCellInfoReport.SocElement.u16Soc==values[i]);
    }
    puts("PASS manual SOC: endpoints, immediate snapshot, reload");
}

static void test_no_self_consumption_and_invalid_samples(void) {
    UINT16 cap_before;
    reset_case(245,80);cap_before=g_stCellInfoReport.SocElement.u16CapacityNow;
    run_seconds(72U*3600U,0,3300);
    assert(g_stCellInfoReport.SocElement.u16CapacityNow==cap_before);
    current_fresh=0;run_seconds(3600,-6000,3000);
    assert(g_stCellInfoReport.SocElement.u16CapacityNow==cap_before);
    current_fresh=1;run_seconds(3600,-6000,3000);
    expect_capacity(245,expected_capacity(245,80,-6000,3600));
    puts("PASS no self consumption:72h zero current; invalid reads pause and recovery resumes");
}
static void test_no_soh_derating(void) {
    reset_case(245,35);
    stored.u32CycleTimes=200000U;
    stored.u32CapFull=245U*3600U*80U/100U;
    stored.u32CapNow=stored.u32CapFull*35U/100U;
    InitData_SOC();
    assert(g_stCellInfoReport.SocElement.u16Soc==35);
    assert(g_stCellInfoReport.SocElement.u16Soh==80);
    assert(g_stCellInfoReport.SocElement.u16CapacityFull==2450);
    assert(g_stCellInfoReport.SocElement.u16Cycle_times==2000);
    run_seconds(3600,-1100,3400);
    expect_capacity(245,expected_capacity(245,35,-1100,3600));
    puts("PASS independent SOH: cycle estimate retained; preserves old snapshot SOC ratio");
}


static void write_parameter(UINT16 address,UINT16 value,struct RS485MSG *msg) {
    memset(msg,0,sizeof(*msg));msg->u16Buffer[5]=1;msg->u16Buffer[6]=2;
    msg->u16Buffer[7]=(UINT8)(value>>8);msg->u16Buffer[8]=(UINT8)value;
    Sci_WrRegs_0x10_OtherElement(address,msg);
}
static void test_parameter_side_effects(void) {
    struct RS485MSG msg;UINT32 old_now;
    reset_case(245,80);stored.u32CycleTimes=12345;InitData_SOC();old_now=stored.u32CapNow;
    write_parameter(0x231B,3000,&msg);
    assert(msg.AckType==RS485_ACK_POS && stored.u32CycleTimes==12345 && stored.u32CapNow==old_now);
    write_parameter(0x231A,3500,&msg);assert(stored.u32CycleTimes==12345);
    write_parameter(0x2318,490,&msg);
    assert(msg.AckType==RS485_ACK_POS && stored.u32CycleTimes==12345);
    assert(stored.u32CapNow==old_now*2 && g_stCellInfoReport.SocElement.u16Soc==80);
    assert(g_stCellInfoReport.SocElement.u16CapacityFull==4900);
    parameter_failure=1;write_parameter(0x2319,1,&msg);
    assert(msg.AckType==RS485_ACK_NEG && stored.u32CycleTimes==12345 && OtherElement.u16Soc_Cycle_times==3);
    parameter_failure=0;write_parameter(0x2319,42,&msg);
    assert(msg.AckType==RS485_ACK_POS && stored.u32CycleTimes==4200);
    puts("PASS real parameter writes: V0/V100 preserve state; capacity keeps ratio; cycles explicit; failed write rolls back");
}
static void test_cycle_write_recovery(void) {
    struct RS485MSG msg;UINT32 capacity;
    reset_case(245,80);stored.u32CycleTimes=12345;InitData_SOC();capacity=stored.u32CapNow;
    write_failure=1;write_parameter(0x2319,1,&msg);
    assert(msg.AckType==RS485_ACK_POS && stored.u32CycleTimes==12345);
    assert(g_stCellInfoReport.SocElement.u16Cycle_times==1);
    InitData_SOC();assert(g_stCellInfoReport.SocElement.u16Cycle_times==1);
    write_failure=0;run_seconds(1,0,3400);
    assert(stored.u32CycleTimes==100 && stored.u32CapNow==capacity && !stored.u32LearnPassedAs10);
    stored.u32CycleTimes=12345;InitData_SOC();write_failure=1;
    write_parameter(0x2319,1,&msg);write_failure=0;InitData_SOC();
    assert(stored.u32CycleTimes==100); /* An explicit same-value write is a new command. */
    puts("PASS cycle command recovery: durable generation, failed snapshot/reboot, retry and same-value reset");
}
static void test_manual_storage_failure(void) {
    struct RS485MSG msg;
    reset_case(245,80);write_failure=1;
    assert(!SOC_RequestSetOnce(35));assert(stored.u16SocNow==80 && g_stCellInfoReport.SocElement.u16Soc==80);
    memset(&msg,0,sizeof(msg));msg.u16Buffer[5]=35;Sci_WrReg_0x06_SetSocOnce(&msg);
    assert(msg.AckType==RS485_ACK_NEG && msg.ErrorType==RS485_ERROR_CMD_INVALID);
    write_failure=0;memset(&msg,0,sizeof(msg));msg.u16Buffer[5]=35;Sci_WrReg_0x06_SetSocOnce(&msg);
    assert(msg.AckType==RS485_ACK_POS && stored.u16SocNow==35);
    assert(!SOC_RequestSetOnce(101));assert(stored.u16SocNow==35);
    puts("PASS real manual command: persistence failure rejects and preserves previous SOC; valid retry succeeds");
}
static void test_save_retry(void) {
    unsigned attempts;
    reset_case(245,80);stored_valid=0;write_failure=1;save_attempts=0;InitData_SOC();
    assert(save_attempts==1 && !stored_valid);write_failure=0;current_fresh=0;run_seconds(1,0,3400);
    assert(stored_valid && stored.u16SocNow==60);
    reset_case(245,80);write_failure=1;attempts=save_attempts;run_seconds(600,-6000,3400);
    assert(stored.u16SocNow==80 && save_attempts-attempts<=600);
    write_failure=0;run_seconds(1,0,3400);
    assert(stored.u16SocNow==g_stCellInfoReport.SocElement.u16Soc);
    reset_case(245,80);stored_valid=0;write_failure=1;save_attempts=0;
    host_tick=UINT32_MAX-50U;InitData_SOC();write_failure=0;
    {unsigned i;for(i=0;i<1000;i++) App_SOC();assert(save_attempts==1);}
    host_tick=30;App_SOC();assert(save_attempts==1);
    host_tick=49;App_SOC();assert(save_attempts==2 && stored_valid);
    puts("PASS snapshot retry: first-write recovery, real1Hz timeout, backlog and32-bit tick wrap");
}
static void test_sleep_and_reset(void) {
    UINT32 exact;unsigned attempts;
    reset_case(245,100);run_seconds(179,-1100,3400);exact=expected_capacity(245,100,-1100,179);attempts=save_attempts;
    g_stLowPowerRtcStatus.mode=HICCUP_MODE;LowPowerSleep_SaveCoreState();assert(save_attempts==attempts);
    g_stLowPowerRtcStatus.mode=NORMAL_MODE;LowPowerSleep_SaveCoreState();assert(stored.u32CapNow==exact);
    InitData_SOC();expect_capacity(245,exact);
    run_seconds(123,-6000,3300);exact-=6000U*123U/100U;
    g_stLowPowerRtcStatus.mode=DEEP_MODE;LowPowerSleep_SaveResetState();assert(stored.u32CapNow==exact);
    InitData_SOC();expect_capacity(245,exact);assert(can_sleeps && statistics_flushes);
    puts("PASS sleep: HICCUP keeps RAM without forced writes; NORMAL/DEEP save exact Ah and restore");
}
static void test_sleep_save_failure(void) {
    unsigned attempts,feeds,flushes;UINT32 exact;
    reset_case(245,80);run_seconds(60,-1100,3400);exact=expected_capacity(245,80,-1100,60);
    attempts=save_attempts;feeds=sleep_watchdog_feeds;flushes=statistics_flushes;
    transient_save_failures=1;g_stLowPowerRtcStatus.mode=DEEP_MODE;
    assert(LowPowerSleep_SaveResetState());
    assert(save_attempts==attempts+2 && stored.u32CapNow==exact && !sleep_save_errors);
    assert(sleep_watchdog_feeds==feeds+2 && statistics_flushes==flushes+1);
    write_failure=1;attempts=save_attempts;flushes=statistics_flushes;
    assert(!LowPowerSleep_SaveResetState());
    assert(save_attempts==attempts+2 && sleep_save_errors==1 && statistics_flushes==flushes+1);
    attempts=save_attempts;g_stLowPowerRtcStatus.mode=HICCUP_MODE;
    assert(LowPowerSleep_SaveCoreState() && save_attempts==attempts);
    assert(!LowPowerSleep_SaveResetState() && save_attempts==attempts+2);
    assert(sleep_save_errors==2); /* Reset saving cannot be skipped by a stale HICCUP mode. */
    write_failure=0;g_stLowPowerRtcStatus.mode=NORMAL_MODE;attempts=save_attempts;
    assert(LowPowerSleep_SaveCoreState() && save_attempts==attempts+1);
    puts("PASS sleep failures: transient retry, persistent two-attempt bound, watchdog, statistics and reset always saves");
}
static void test_soh_boundaries(void) {
    const UINT32 cycles[]={0,99,100,199,200,1999,2000,10000,42949672};unsigned i;
    for(i=0;i<sizeof(cycles)/sizeof(cycles[0]);++i) {
        UINT16 soh=cycles[i]/100U>=20U ? 80U : (UINT16)(100U-cycles[i]/100U);
        reset_case(245,50);stored.u32CycleTimes=cycles[i]*100U;InitData_SOC();
        assert(g_stCellInfoReport.SocElement.u16Soh==soh);
        assert(g_stCellInfoReport.SocElement.u16CapacityFull==2450);
        assert(g_stCellInfoReport.SocElement.u16Soc==50);
    }
    reset_case(245,50);stored.u32CycleTimes=UINT32_MAX-1U;InitData_SOC();run_seconds(1,INT32_MIN,3300);
    assert(SOC_SaveSnapshot() && stored.u32CycleTimes==UINT32_MAX);
    assert(g_stCellInfoReport.SocElement.u16Cycle_times==65535);
    puts("PASS SOH:100-cycle step,80% floor, independent rated capacity, cycle/report saturation");
}
static void test_sizes_and_invalid_capacity(void) {
    const UINT16 caps[]={1,245,270,6553,65000};unsigned i;
    for(i=0;i<sizeof(caps)/sizeof(caps[0]);++i) {
        reset_case(caps[i],50);run_seconds(60,-1100,3000);
        expect_capacity(caps[i],expected_capacity(caps[i],50,-1100,60));
    }
    reset_case(0,50);run_seconds(60,INT32_MIN,3000);
    assert(!g_stCellInfoReport.SocElement.u16CapacityNow && !SOC_RequestSetOnce(50));
    assert(SOC_GetRemainingMinutes(0)==0xFFFF);
    reset_case(245,50);run_seconds(1,INT32_MAX,4200);assert(g_stCellInfoReport.SocElement.u16Soc==100);
    run_seconds(1,INT32_MIN,2000);assert(!g_stCellInfoReport.SocElement.u16Soc);
    puts("PASS ranges: min/max capacity, zero-capacity guard, INT32 current extremes and no arithmetic overflow");
}
static void test_random_profile(void) {
    UINT32 random=0x13579BDFU,n;int64_t reference=245LL*3600*50/100*100000;
    const int64_t full=245LL*3600*100000;
    reset_case(245,50);
    for(n=0;n<200000;n++) {
        UINT32 expected;int64_t error;
        random=random*1664525U+1013904223U;
        host_current_ma=(INT32)(random%120001U)-60000;
        current_fresh=(UINT8)(n%17U!=0U);
        g_stCellInfoReport.u16VCellMin=(UINT16)(2000U+random%2500U);
        if(current_fresh) reference+=(int64_t)host_current_ma*200;
        if(reference<0) reference=0;if(reference>full) reference=full;
        host_tick+=20U;App_SOC();
        if(n%997U==0U) {
            assert(SOC_SaveSnapshot());expected=(UINT32)(reference/100000);
            error=(int64_t)stored.u32CapNow-expected;assert(error>=-1 && error<=1);
        }
    }
    puts("PASS randomized:200000 ticks, signed pulses, invalid samples, saturation and independent mA*ms oracle");
}
static void test_snapshot_semantics(void) {
    reset_case(245,80);stored.u16Flags=0xFFFF;stored.u32CapFull=0;stored.u32CapNow=0;InitData_SOC();
    assert(g_stCellInfoReport.SocElement.u16Soc==80);
    stored.u16SocNow=101;InitData_SOC();assert(g_stCellInfoReport.SocElement.u16Soc==60);
    stored.u16FormatVersion=1;InitData_SOC();assert(g_stCellInfoReport.SocElement.u16Soc==60);
    reset_case(245,100);save_attempts=0;run_seconds(14664,-6000,3400);run_seconds(14664,6000,3400);
    assert(save_attempts<=202);
    printf("PASS snapshot semantics: invalid data fallback, retired flags ignored, discharge/recharge saves=%u\n",save_attempts);
}

int main(void) {
    test_startup();test_voltage_independence();test_current_resolution();
    test_customer_case();test_bounds();test_manual_and_restore();
    test_no_self_consumption_and_invalid_samples();test_no_soh_derating();
    test_parameter_side_effects();test_cycle_write_recovery();test_manual_storage_failure();test_save_retry();
    test_sleep_and_reset();test_sleep_save_failure();test_soh_boundaries();test_sizes_and_invalid_capacity();
    test_random_profile();test_snapshot_semantics();
    puts("PASS SOC production C:18 groups");return 0;
}

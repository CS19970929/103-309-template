#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int32_t INT32; typedef int FLASH_Status;
#include "conf/Project_Config.h"
#include "Flash.h"
#define SOC_TABLE_SIZE 42
struct { UINT16 u16Soc_Ah,u16Soc_Cycle_times,u16Soc_V_100,u16Soc_V_0; } OtherElement;
struct { UINT16 u16VCellMin,u16VCellMax; struct {
    UINT16 u16Soc,u16Soh,u16CapacityNow,u16CapacityFull,u16CapacityFactory,u16Cycle_times;
} SocElement; } g_stCellInfoReport;
static STORAGE_FLASH_SOC_DATA stored;
static UINT8 stored_valid,write_failure,current_fresh=1U;
static INT32 host_current_ma;
static unsigned save_attempts;
UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data) {
    if(!stored_valid) return 0U;
    *data=stored; return 1U;
}
UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data) {
    ++save_attempts;
    if(write_failure) return 0U;
    stored=*data; stored_valid=1U; return 1U;
}
INT32 AfeCurrent_GetCurrent_mA(void) { return host_current_ma; }
UINT8 Fault_IsCurrentSampleFresh(void) { return current_fresh; }
/* PRODUCTION_SOURCE */
static void reset_case(UINT16 capacity_a10,UINT8 soc) {
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    memset(&stored,0,sizeof(stored));
    OtherElement.u16Soc_Ah=capacity_a10; OtherElement.u16Soc_Cycle_times=3U;
    OtherElement.u16Soc_V_100=3550U; OtherElement.u16Soc_V_0=3050U;
    stored_valid=write_failure=0U;current_fresh=1U;host_current_ma=0;save_attempts=0U;
    InitData_SOC();SOC_RequestSetOnce(soc);
}
static void run_seconds(UINT32 seconds,INT32 measured_ma,UINT16 voltage_mv) {
    UINT32 ticks=seconds*5U;
    host_current_ma=measured_ma;
    g_stCellInfoReport.u16VCellMin=g_stCellInfoReport.u16VCellMax=voltage_mv;
    while(ticks-- != 0U) App_SOC();
}
static UINT32 expected_capacity(UINT16 capacity_a10,UINT8 start,INT32 ma,UINT32 seconds) {
    int64_t full=(int64_t)capacity_a10*3600;
    int64_t cap=full*start/100+((int64_t)ma-HOST_SELF_MA)*seconds/100;
    if(cap<0) cap=0;
    if(cap>full) cap=full;
    return (UINT32)cap;
}
static void expect_capacity(UINT16 capacity_a10,UINT32 cap) {
    UINT32 full=(UINT32)capacity_a10*3600U;
    UINT32 soc=(UINT32)(((uint64_t)cap*100U+full/2U)/full);
    assert(g_stCellInfoReport.SocElement.u16Soc==soc);
    assert(g_stCellInfoReport.SocElement.u16CapacityNow==(cap+180U)/360U);
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
    cap-=((UINT32)6000+HOST_SELF_MA)*7200U/100U;expect_capacity(245,cap);
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
int main(void) {
    test_startup();test_voltage_independence();test_current_resolution();
    test_customer_case();test_bounds();test_manual_and_restore();
    puts("PASS SOC production C:6 groups");return 0;
}

/* Execute the real App_AFEGet() with independent ADC, fault and PF outcomes. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UINT8;
static int led,period,sample_result,fault_valid,pf,updated,mos_applied,soc_processed,uv_updates,ctl_closed;
static struct { struct { struct { UINT8 b1CellUvp; } bits; } unMdlFault_Third; } g_stCellInfoReport;
static struct { struct { struct { UINT8 PF; } bits; } REG_BSTATUS1; } SH367309_Reg_Store;
#define MCUO_DEBUG_LED1 led
static int SysTime_Take200msTaskPeriod(void) { return period; }
static UINT8 UpdateVoltageFromBqMaximo(void) { return (UINT8)sample_result; }
static void AFE_MonitorUpdate(UINT8 data_ok,UINT8 status_ok,UINT8 wire_ok) {
    UINT8 expected=(UINT8)(sample_result==0 && fault_valid && !pf);
    assert(data_ok==expected && status_ok==(UINT8)fault_valid && wire_ok==expected);
}
static void Fault_UpdatePackTemperatureResult(UINT8 result) {
    assert(result==(UINT8)sample_result);
}
static UINT8 Fault_CanOpenCtl(void) { return 1U; }
static UINT8 AFE_IsHealthy(void) { return 0U; }
static void close_ctlc(void) { ++ctl_closed; }
static void open_ctlc(void) { assert(!pf); }
static void AFE_ApplyPendingConfiguration(UINT8 data_ok,UINT8 status_ok) {
    assert(data_ok==(UINT8)(sample_result==0 && fault_valid && !pf));
    assert(status_ok==(UINT8)fault_valid);
}
static void DataLoad_CellVolt(void) {}
static void DataLoad_CellVoltMaxMinFind(void) {}
static void DataLoad_Temperature(void) {}
static void DataLoad_TemperatureMaxMinFind(void) {}
static void DataLoad_Current(void) {}
static void Fault_UpdateCurrentProtection(UINT8 valid) {
    assert(valid==(UINT8)(sample_result==0 && fault_valid && !pf));
}
static UINT8 App_SH367309(void) { return (UINT8)fault_valid; }
static UINT8 Fault_ApplyMosPermits(UINT8 a,UINT8 b) { ++mos_applied; return 1U; }
static void Sci_CustomerUpdateCounters(void) { assert(mos_applied); ++updated; }
static void App_SOC(UINT8 valid,UINT8 status) {
    assert(valid==(UINT8)(sample_result==0 && fault_valid && !pf));
    assert(status==(UINT8)fault_valid);
    soc_processed=1;
}
static void SleepDeal_UpdateOverdischarge(UINT8 active,UINT8 voltage_fresh) {
    assert(soc_processed && voltage_fresh==(UINT8)(sample_result==0 && fault_valid && !pf));
    ++uv_updates;
}
static UINT8 SleepDeal_NormalIdleExpired(UINT8 activity) { assert(activity && !fault_valid); return 0; }
/* AFE_TASK_SOURCE */
int main(void) {
    int a,b,c;
    for(a=0;a<4;++a) for(b=0;b<2;++b) for(c=0;c<2;++c) {
        period=1; sample_result=a; fault_valid=b; pf=c;
        SH367309_Reg_Store.REG_BSTATUS1.bits.PF=(UINT8)pf;
        updated=mos_applied=soc_processed=uv_updates=ctl_closed=0;
        App_AFEGet();
        assert(mos_applied==1 && updated==(a==0 && b && !c));
        assert(uv_updates==b);
        assert(ctl_closed==(b && c));
    }
    period=0; updated=mos_applied=0; App_AFEGet(); assert(!updated && !mos_applied);
    puts("PASS AFE sampling: 0/1/2/3 results, status failure and PF reject SOC and statistics before MOS arbitration");
    return 0;
}

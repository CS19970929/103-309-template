/* Execute the real AFE task with independent success/failure controls. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UINT8;
static int led,period,voltage_failed,fault_valid,updated,mos_applied,soc_processed,uv_updates;
static struct { struct { struct { UINT8 b1CellUvp; } bits; } unMdlFault_Third; } g_stCellInfoReport;
#define MCUO_DEBUG_LED1 led
static int SysTime_Take200msTaskPeriod(void) { return period; }
static UINT8 UpdateVoltageFromBqMaximo(void) { return (UINT8)voltage_failed; }
static void AFE_MonitorUpdate(UINT8 data_ok,UINT8 status_ok) {
    assert(data_ok==(UINT8)!voltage_failed && status_ok==(UINT8)fault_valid);
}
static UINT8 AFE_IsHealthy(void) { return 0U; }
static void open_ctlc(void) {}
static void AFE_ApplyPendingConfiguration(UINT8 data_ok,UINT8 status_ok) {
    assert(data_ok==(UINT8)!voltage_failed && status_ok==(UINT8)fault_valid);
}
static void DataLoad_CellVolt(void) {}
static void DataLoad_CellVoltMaxMinFind(void) {}
static void DataLoad_Temperature(void) {}
static void DataLoad_TemperatureMaxMinFind(void) {}
static void DataLoad_Current(void) {}
static void Fault_UpdateCurrentProtection(UINT8 valid) { assert(valid==!voltage_failed); }
static UINT8 App_SH367309(void) { return (UINT8)fault_valid; }
static void charger_detect_and_keyLogi_200ms(void) {}
static UINT8 Fault_ApplyMosPermits(UINT8 a,UINT8 b) { ++mos_applied; return 1U; }
static void Sci_CustomerUpdateCounters(void) { assert(mos_applied); ++updated; }
static void App_SOC(UINT8 sample_valid,UINT8 fault_valid) { soc_processed=1; }
static void SleepDeal_UpdateOverdischarge(UINT8 active) { assert(soc_processed); ++uv_updates; }
static UINT8 SleepDeal_NormalIdleExpired(UINT8 activity) { assert(activity && !fault_valid); return 0; }
/* AFE_TASK_SOURCE */
int main(void) {
    int a,b;
    for(a=0;a<2;++a) for(b=0;b<2;++b) {
        period=1; voltage_failed=a; fault_valid=b; updated=mos_applied=soc_processed=uv_updates=0;
        App_AFEGet(); assert(mos_applied==1 && updated==(!a && b));
        assert(uv_updates==b);
    }
    period=0; updated=mos_applied=0; App_AFEGet(); assert(!updated && !mos_applied);
    puts("PASS statistics sampling: only complete fresh cycles, after final MOS arbitration; no UART-loop or stale-cache edges");
    return 0;
}

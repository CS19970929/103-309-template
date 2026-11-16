"""Run real AFE monitor, MTP retries and MOS arbitration C with host bus stubs.

No board I/O or firmware images; all generated files stay in user temp.
"""
import os
from pathlib import Path
import subprocess
import tempfile

from test_protection_refactor import ADC_STUBS, FAULT_STUBS, function, run_c

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"

BUS_STUBS = r'''
#define AFE_ID 0x34
#define Feed_IWatchDog ((void)0)
static int read_failures, write_failures, status_fail;
static void Delay1ms(int ms) { (void)ms; }
static UINT8 TwiRead(UINT8 id, UINT8 addr, UINT8 len, UINT8 *p) {
    (void)id; (void)len; ++reads;
    if (read_failures) { --read_failures; return 0U; }
    if (status_fail && addr==0x41U) return 0U;
    *p=addr==MTP_CONF ? hardware_conf : 0U;
    if (bad_verify && addr==MTP_CONF && writes) *p^=0x10U;
    return 1U;
}
static UINT8 TwiWrite(UINT8 id, UINT8 addr, UINT8 len, UINT8 *p) {
    (void)id; (void)len; ++writes;
    if (write_failures) { --write_failures; return 0U; }
    if (fail_write) return 0U;
    if (addr==MTP_CONF) hardware_conf=*p;
    return 1U;
}
'''

APP_STUBS = r'''
#define DEEP_MODE 2U
static UINT8 ctl, afe_status, sleep_mode, input_sample, task_pending=1U;
static UINT8 MCUO_DEBUG_LED1, soc_data_ok, published_comm_fault;
static unsigned published, config_calls, counter_calls;
static void open_ctlc(void) { ctl=1U; }
static void SystemRuntime_SetAfeStatus(UINT8 index, UINT8 ok) {
    assert(index==0U); afe_status=ok;
}
static void LowPower_Request(UINT8 mode) { sleep_mode=mode; }
static void AfeCell_InvalidatePairHistory(void) {}
static UINT8 SysTime_Take200msTaskPeriod(void) { return task_pending; }
static UINT8 UpdateVoltageFromBqMaximo(void) {
    UINT8 ignored;
    if (!MTPRead(0x46U, 42U, &ignored)) return 1U;
    return input_sample;
}
static UINT8 App_SH367309(void) {
    UINT8 ignored;
    return MTPRead(0x41U, 5U, &ignored);
}
static void DataLoad_CellVolt(void) {
    ++published; published_comm_fault=SystemError_IsActive(ERROR_STATUS_AFE1);
}
static void DataLoad_CellVoltMaxMinFind(void) {}
static void DataLoad_Temperature(void) {}
static void DataLoad_TemperatureMaxMinFind(void) {}
static void DataLoad_Current(void) {}
static void App_SOC(UINT8 data_ok, UINT8 status_ok) {
    (void)status_ok; soc_data_ok=data_ok;
}
static void SleepDeal_UpdateOverdischarge(UINT8 uv, UINT8 data_ok) {
    (void)uv; (void)data_ok;
}
static UINT8 SleepDeal_NormalIdleExpired(UINT8 blocked) {
    assert(blocked); return 0U;
}
static void Sci_CustomerUpdateCounters(void) { ++counter_calls; }
static UINT8 SH367309_UpdataAfeConfig(void) {
    ++config_calls; AFE_PARAM_WRITE_Flag=0U; return 1U;
}
'''

CASES = r'''
static void reset_monitor(void) {
    memset(&s_data,0,sizeof(s_data));
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    memset(&SH367309_Reg_Store,0,sizeof(SH367309_Reg_Store));
    memset(test_error_state,0,sizeof(test_error_state));
    s_host_mos_off=0U; AFE_PARAM_WRITE_Flag=0U; s_afe_comm_failed=0U;
    read_failures=write_failures=status_fail=fail_write=bad_verify=0;
    reads=writes=forced_ctl_close=0; input_sample=ctl=afe_status=sleep_mode=0U;
    published=config_calls=counter_calls=0U; tick=0U; task_pending=1U;
    hardware_conf=0x38U; s_pack_sensor_fault=s_pack_sensor_ok_count=s_mos_sensor_fault=0U;
    AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue=120U;
    AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue=150U;
    Fault_PauseMeasurements();
}
static void cycles(unsigned n) {
    while(n--) { tick+=20U; App_AFEGet(); }
}
int main(void) {
    unsigned i; UINT32 since; UINT8 value=0U;
    /* The exact board failure: disconnected CELL at boot, healthy I2C. */
    reset_monitor(); input_sample=3U; cycles(3U);
    assert(!SystemError_IsActive(ERROR_STATUS_AFE1) && afe_status);
    assert(AFE_HasDataFault() && !AFE_IsHealthy() && !AFE_IsDataFresh());
    assert(!ctl && !(hardware_conf&0x30U) && published==3U && !soc_data_ok);
    assert(!Fault_CanOpenCtl());
    for(i=0;i<4U;i++) {
        g_stCellInfoReport.u16Ichg=20U; g_stCellInfoReport.u16IDischg=20U;
        Fault_UpdateCurrentProtection(1U);
        assert(Fault_SetHostMosOff((UINT8)i));
        assert(!(hardware_conf&0x30U) && !ctl && !Fault_CanOpenCtl());
    }
    /* Recovery cannot erase the host request or a separate protection. */
    input_sample=0U; cycles(2U); assert(AFE_HasDataFault() && !ctl);
    cycles(1U); assert(!AFE_HasDataFault() && !ctl && s_host_mos_off==3U);
    assert(Fault_SetHostMosOff(0U)); cycles(1U); assert(ctl && AFE_IsHealthy());
    assert((hardware_conf&0x30U)==0x30U && published==7U && soc_data_ok);

    reset_monitor(); cycles(3U); assert(ctl);
    input_sample=3U; cycles(2U); assert(!AFE_HasDataFault());
    cycles(1U); assert(AFE_HasDataFault() && !ctl && !(hardware_conf&0x30U));
    input_sample=0U; g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp=1U;
    cycles(3U); assert(!AFE_HasDataFault() && !ctl && !(hardware_conf&0x30U));

    /* NTC invalid and PF are unusable data, with successful communication. */
    reset_monitor(); input_sample=2U; cycles(3U);
    assert(afe_status && !SystemError_IsActive(ERROR_STATUS_AFE1));
    assert(SystemError_IsActive(ERROR_STATUS_TEMP_BREAK) && !ctl);
    reset_monitor(); SH367309_Reg_Store.REG_BSTATUS1.bits.PF=1U; cycles(3U);
    assert(afe_status && !SystemError_IsActive(ERROR_STATUS_AFE1));
    assert(!soc_data_ok && !ctl && AFE_HasDataFault());

    /* Missing status and a failed MTP read are actual comm failures. */
    reset_monitor(); status_fail=1; cycles(2U);
    assert(!SystemError_IsActive(ERROR_STATUS_AFE1)); cycles(1U);
    assert(SystemError_IsActive(ERROR_STATUS_AFE1) && !afe_status && !ctl);
    status_fail=0; input_sample=3U; cycles(3U);
    assert(!SystemError_IsActive(ERROR_STATUS_AFE1) && AFE_HasDataFault());
    reset_monitor();
    for(i=0;i<3U;i++) { read_failures=2; cycles(1U); }
    assert(SystemError_IsActive(ERROR_STATUS_AFE1) && published_comm_fault);
    cycles(2U); assert(SystemError_IsActive(ERROR_STATUS_AFE1) && !soc_data_ok);
    assert(published_comm_fault && !Fault_IsCurrentSampleFresh());
    cycles(1U); assert(!SystemError_IsActive(ERROR_STATUS_AFE1) && ctl);
    assert(!published_comm_fault && soc_data_ok);

    /* MTP retries: only final failure is latched, and success cannot erase it. */
    reset_monitor(); read_failures=1; assert(MTPRead(0x46U,1U,&value));
    assert(!AFE_TakeCommFailure());
    read_failures=2; assert(!MTPRead(0x46U,1U,&value));
    assert(MTPRead(0x46U,1U,&value)); assert(AFE_TakeCommFailure());
    assert(!AFE_TakeCommFailure());
    write_failures=1; assert(MTPWrite(MTP_CONF,1U,&value));
    assert(!AFE_TakeCommFailure());
    write_failures=2; assert(!MTPWrite(MTP_CONF,1U,&value));
    assert(AFE_TakeCommFailure());
    write_failures=2; assert(!MTPWriteROM(0U,1U,&value));
    assert(AFE_TakeCommFailure());

    /* Real MOS write errors reach comm monitoring on the next RUN cycle.
       Successful I2C with mismatched CONF remains a control error only. */
    reset_monitor(); fail_write=1; input_sample=3U; cycles(6U);
    assert(SystemError_IsActive(ERROR_STATUS_AFE1));
    reset_monitor(); bad_verify=1; input_sample=3U; cycles(6U);
    assert(!SystemError_IsActive(ERROR_STATUS_AFE1));
    assert(test_error_state[ERROR_RESERVED_21] && !ctl);

    /* Long disconnection still sleeps; no false comm error or timer restart. */
    reset_monitor(); input_sample=3U; cycles(3U);
    since=s_data.mon.sample.fault_tick;
    tick=since+29980U; App_AFEGet(); assert(!sleep_mode);
    tick=since+30000U; App_AFEGet();
    assert(sleep_mode==DEEP_MODE && !SystemError_IsActive(ERROR_STATUS_AFE1));
    reset_monitor(); status_fail=1; cycles(3U); since=s_data.mon.comm.fault_tick;
    tick=since+30000U; App_AFEGet(); assert(sleep_mode==DEEP_MODE);

    /* Independent clocks: a new comm fault must not restart the sampling timer. */
    reset_monitor(); input_sample=3U; cycles(3U); since=s_data.mon.sample.fault_tick;
    tick+=1000U; status_fail=1; cycles(3U);
    assert(s_data.mon.comm.fault_tick!=since && s_data.mon.sample.fault_tick==since);
    status_fail=0; cycles(3U); assert(!s_data.mon.comm.fault);
    tick=since+30000U; App_AFEGet(); assert(sleep_mode==DEEP_MODE);

    /* STOP retains faults but neither offline time nor queued periods count. */
    reset_monitor(); input_sample=3U; cycles(3U); tick=0U;
    AFE_MonitorResumeAfterStop();
    assert(AFE_HasDataFault() && s_data.mon.sample.fault_tick==0U);
    cycles(3U); assert(!SystemError_IsActive(ERROR_STATUS_AFE1) && AFE_HasDataFault());
    tick=30000U; App_AFEGet(); assert(sleep_mode==DEEP_MODE);
    reset_monitor(); status_fail=1; cycles(3U); tick=0U;
    AFE_MonitorResumeAfterStop();
    assert(s_data.mon.comm.fault && s_data.mon.comm.fault_tick==0U);
    cycles(3U); assert(SystemError_IsActive(ERROR_STATUS_AFE1));
    tick=30000U; App_AFEGet(); assert(sleep_mode==DEEP_MODE);
    reset_monitor(); input_sample=3U;
    for(i=0;i<100U;i++) App_AFEGet();
    assert(!AFE_HasDataFault() && s_data.mon.sample.fail_count==1U);
    reset_monitor(); cycles(3U); read_failures=2; App_AFEGet();
    assert(s_afe_comm_failed); /* Same-tick calls cannot lose a bus failure. */
    cycles(1U); assert(!s_afe_comm_failed && s_data.mon.comm.fail_count==1U);
    cycles(1U); assert(s_data.mon.comm.fail_count==0U);
    reset_monitor(); tick=UINT32_MAX-40U; input_sample=3U; cycles(3U);
    assert(AFE_HasDataFault()); tick=s_data.mon.sample.fault_tick+30000U;
    App_AFEGet(); assert(sleep_mode==DEEP_MODE);

    reset_monitor(); AFE_PARAM_WRITE_Flag=1U; cycles(1U);
    assert(config_calls==1U && !ctl);
    cycles(3U); assert(ctl && !SystemError_IsActive(ERROR_STATUS_AFE1));
    puts("PASS AFE classification: boot/wake open wire, I2C/CRC retries, NTC/PF,");
    puts("     MOS/host/reverse arbitration, recovery, independent 5-minute sleep, STOP/wrap");
    return 0;
}
'''


def main():
    data = (SOURCE / "DataDeal.c").read_text(encoding="utf-8")
    i2c = (SOURCE / "I2C_AFE1.c").read_text(encoding="utf-8")
    upper = (SOURCE / "Sci_Upper.h").read_text(encoding="utf-8")
    bits = upper[upper.index("struct MDLCHGFAULT_BITS"):
                 upper.index("};", upper.index("union  MDLCHGFAULT_REG")) + 2]
    stubs = FAULT_STUBS.replace(
        "static UINT8 AFE_HasDataFault(void) { return 0U; }",
        "UINT8 AFE_HasDataFault(void);")
    for signature in ("static int MTPRead(", "static int MTPWrite("):
        stubs = stubs.replace(function(stubs, signature), "")
    stubs = stubs.replace("static int forced_ctl_close;", "static int forced_ctl_close;\nstatic UINT8 ctl;")
    stubs = stubs.replace("++forced_ctl_close;", "++forced_ctl_close; ctl=0U;")
    stubs = stubs.replace("struct { UINT8 SC, PF; }", "struct { UINT8 SC, PF, UV; }")
    wrappers = "static UINT8 s_afe_comm_failed;\n" + "".join(function(i2c, sig) for sig in (
        "UINT8 AFE_TakeCommFailure(", "UINT8 MTPRead(", "UINT8 MTPWrite(", "UINT8 MTPWriteROM("))
    driver = (SOURCE / "SH367309_Func.c").read_text(encoding="utf-8")
    driver = "".join(function(driver, sig) for sig in (
        "static UINT8 SH367309_WriteConfBits(", "UINT8 SH367309_SetMosPermits("))
    defines = "\n".join(line for line in data.splitlines() if line.startswith(
        ("#define AFE_MONITOR_", "#define AFE_CONFIG_"))) + "\n"
    types = data[data.index("typedef struct _AFE_CURRENT_RUNTIME"):data.index("UINT16 g_u16CalibCoefK")]
    monitor = "".join(function(data, sig) for sig in (
        "void AFE_MonitorInvalidate(", "void AFE_MonitorResumeAfterStop(",
        "UINT8 AFE_IsDataFresh(", "UINT8 AFE_IsHealthy(", "UINT8 AFE_HasDataFault(",
        "static void AFE_UpdateFault(", "static void AFE_MonitorUpdate(",
        "static void AFE_ApplyPendingConfiguration(", "void App_AFEGet("))
    fault = (SOURCE / "Fault.c").read_text(encoding="utf-8").replace('#include "main.h"', "")
    # The same CTL variable is shared by real arbitration and App_AFEGet.
    app = APP_STUBS.replace("static UINT8 ctl,", "static UINT8")
    code = (ADC_STUBS + (SOURCE / "ADC.h").read_text(encoding="utf-8") +
            (SOURCE / "Fault.h").read_text(encoding="utf-8") + bits + stubs +
            BUS_STUBS + wrappers + driver + fault + app + defines + types + monitor + CASES)
    output = Path(tempfile.mkdtemp(prefix="bms-afe-classification-"))
    vs = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community")
    setup = output / "compiler-env.bat"
    setup.write_text('@echo off\ncall "' + str(vs / "Common7/Tools/VsDevCmd.bat") +
                     '" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n', encoding="utf-8")
    env = os.environ.copy()
    for line in subprocess.check_output(["cmd.exe", "/d", "/c", str(setup)],
                                        text=True, errors="replace").splitlines():
        key, sep, value = line.partition("=")
        if sep and key:
            env[key] = value
    compiler = sorted((vs / "VC/Tools/MSVC").glob("*/bin/Hostx64/x64/cl.exe"))[-1]
    run_c("afe_classification", code, output, env, compiler)
    print("Host-only evidence:", output)


if __name__ == "__main__":
    main()

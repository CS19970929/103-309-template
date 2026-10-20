"""Compile the production core with deterministic fake ports. No board writes."""
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile

from test_current_calibration import msvc_environment

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "low_power_port.h"
static uint32_t irq_mask, now_ms, rtc_now, blocks, active, sample_exit;
static uint32_t stop_wakes, arm_wakes, restore_wakes, elapsed;
static uint32_t stops, restores, samples;
static uint8_t quiet_result, begin_ok, init_ok, arm_ok, sync_ok, reset_ok;
static uint8_t uart_result;
static LowPowerTarget target;
uint32_t BoardLowPower_Lock(void) { uint32_t old=irq_mask; irq_mask=1; return old; }
void BoardLowPower_Unlock(uint32_t value) { irq_mask=value; }
void BoardLowPower_Init(void) {}
uint32_t BoardLowPower_NowMs(void) { return now_ms; }
uint32_t BoardLowPower_RtcCounter(void) { return rtc_now; }
void BoardLowPower_Snapshot(void) { g_low_power_diag.rtc_counter=rtc_now; }
uint32_t BoardLowPower_ActiveWake(void) { return active; }
uint8_t BoardLowPower_Begin(void) { return begin_ok; }
uint8_t BoardLowPower_WaitQuiet(void) { assert(!irq_mask); return quiet_result; }
void BoardLowPower_ArmWake(void) { assert(irq_mask); LowPower_NotifyWake(arm_wakes); }
void BoardLowPower_PrepareNextStop(void) { assert(irq_mask); }
void BoardLowPower_Stop(void) {
    assert(irq_mask); ++stops; rtc_now += 3U; LowPower_NotifyWake(stop_wakes);
}
uint8_t BoardLowPower_RtcInit(void) { return init_ok; }
uint8_t BoardLowPower_RtcArm(void) {
    /* Alarm can occur during arming; the core must not erase it afterward. */
    if (arm_wakes==LP_WAKE_RTC) LowPower_NotifyWake(LP_WAKE_RTC);
    return arm_ok;
}
uint8_t BoardLowPower_CaptureWake(void) { assert(irq_mask); return sync_ok; }
void BoardLowPower_BeginSample(void) { assert(!irq_mask); ++samples; }
uint8_t BoardLowPower_PollSample(void) { return 1U; }
void BoardLowPower_BeginUart1Check(void) { assert(!irq_mask); }
uint8_t BoardLowPower_PollUart1Check(void) { return uart_result; }
void BoardLowPower_EndUart1Check(uint8_t accepted) { (void)accepted; }
void BmsSleep_ConsumeSleepUartActivity(void) {}
void BoardLowPower_Restore(void) { ++restores; LowPower_NotifyWake(restore_wakes); }
void BoardLowPower_AccountSeconds(uint32_t seconds) { elapsed+=seconds; }
void BmsSleep_PollRequest(void) {}
LowPowerTarget BmsSleep_GetTarget(void) { return target; }
void BmsSleep_Request(LowPowerTarget value) { target=value; }
uint32_t BmsSleep_BlockMask(uint8_t refresh) { (void)refresh; return blocks; }
uint32_t BmsSleep_SampleExit(void) { return sample_exit; }
uint8_t BmsSleep_CommitReset(void) { return reset_ok; }
void BmsSleep_ResetQualification(void) { target=LP_TARGET_RUN; }
static void defaults(void) {
    memset((void*)&g_low_power_diag,0,sizeof(g_low_power_diag));
    irq_mask=now_ms=blocks=active=sample_exit=elapsed=stops=restores=samples=0U;
    stop_wakes=LP_WAKE_RTC; arm_wakes=restore_wakes=0U;
    quiet_result=begin_ok=init_ok=arm_ok=sync_ok=reset_ok=1U;
    uart_result=0U;
    rtc_now=100U; target=LP_TARGET_RUN;
    LowPower_Init();
}
static void prepare(void) {
    LowPower_Request(LP_TARGET_RTC,42U);
    assert(LowPower_Poll()==LP_OWNS_CPU && g_low_power_diag.state==LP_PREPARE);
    assert(LowPower_Poll()==LP_OWNS_CPU && g_low_power_diag.state==LP_WAIT_QUIET);
}
static void commit(void) {
    assert(LowPower_Poll()==LP_OWNS_CPU && g_low_power_diag.state==LP_COMMIT);
}
int main(void) {
    defaults(); LowPower_NotifyWake(LP_WAKE_KEY); assert(!g_low_power_diag.wake_mask);
    blocks=LP_BLOCK_STORAGE; target=LP_TARGET_RTC;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && !stops);
    defaults(); prepare(); quiet_result=0U;
    for(int i=0;i<100;i++) assert(LowPower_Poll()==LP_OWNS_CPU && !stops && !irq_mask);
    quiet_result=2U; assert(LowPower_Poll()==LP_RUN_APPLICATION);
    assert(restores==1 && (g_low_power_diag.fault_mask&LP_FAULT_PREPARE_TIMEOUT));
    defaults(); prepare(); active=LP_WAKE_KEY;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && !stops && restores==1);
    defaults(); prepare(); commit(); arm_wakes=LP_WAKE_COMM;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && !stops && restores==1);
    assert(g_low_power_diag.first_wake==LP_WAKE_COMM);
    defaults(); prepare(); commit(); stop_wakes=LP_WAKE_RTC|LP_WAKE_COMM;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && samples==0 && restores==1);
    assert(g_low_power_diag.wake_mask==(LP_WAKE_RTC|LP_WAKE_COMM));
    defaults(); prepare(); commit(); stop_wakes=LP_WAKE_UART1;
    assert(LowPower_Poll()==LP_OWNS_CPU && g_low_power_diag.state==LP_UART_VERIFY && restores==0);
    for(int i=0;i<10;i++) assert(LowPower_Poll()==LP_OWNS_CPU && restores==0);
    uart_result=2U;
    assert(LowPower_Poll()==LP_OWNS_CPU && g_low_power_diag.state==LP_RTC_SAMPLE);
    assert(g_low_power_diag.uart1_rejected==1U && restores==0);
    assert(LowPower_Poll()==LP_OWNS_CPU && samples==1U);
    defaults(); prepare(); commit(); stop_wakes=LP_WAKE_UART1;
    assert(LowPower_Poll()==LP_OWNS_CPU); uart_result=1U;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && restores==1U);
    assert(g_low_power_diag.uart1_valid_frames==1U && g_low_power_diag.external_wake_count==1U);
    defaults(); prepare(); commit(); stop_wakes=LP_WAKE_UART1;
    assert(LowPower_Poll()==LP_OWNS_CPU); LowPower_Request(LP_TARGET_RUN,0U);
    assert(LowPower_Poll()==LP_RUN_APPLICATION && restores==1U);
    defaults(); prepare(); commit(); stop_wakes=LP_WAKE_UART1;
    assert(LowPower_Poll()==LP_OWNS_CPU); LowPower_NotifyWake(LP_WAKE_KEY);
    assert(LowPower_Poll()==LP_RUN_APPLICATION && restores==1U);
    defaults(); prepare(); commit();
    assert(LowPower_Poll()==LP_OWNS_CPU && g_low_power_diag.state==LP_RTC_SAMPLE);
    assert(LowPower_Poll()==LP_OWNS_CPU && samples==1 && !restores);
    assert(LowPower_Poll()==LP_OWNS_CPU && stops==2);
    sample_exit=LP_EXIT_CURRENT;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && restores==1 && elapsed==6);
    defaults(); prepare(); commit(); stop_wakes=0U; arm_wakes=LP_WAKE_RTC;
    assert(LowPower_Poll()==LP_OWNS_CPU && g_low_power_diag.state==LP_RTC_SAMPLE);
    defaults(); prepare(); commit(); stop_wakes=0U;
    for(int i=0;i<7;i++) assert(LowPower_Poll()==LP_OWNS_CPU);
    assert(LowPower_Poll()==LP_RUN_APPLICATION);
    assert(stops==8 && (g_low_power_diag.fault_mask&LP_FAULT_SPURIOUS_LIMIT));
    defaults(); init_ok=0U; target=LP_TARGET_RTC;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && !restores);
    assert(g_low_power_diag.fault_mask&LP_FAULT_RTC_INIT);
    defaults(); prepare(); commit(); arm_ok=0U;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && !stops && restores==1);
    assert(g_low_power_diag.fault_mask&LP_FAULT_RTC_ARM);
    defaults(); prepare(); commit(); sync_ok=0U;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && restores==1);
    assert(g_low_power_diag.fault_mask&LP_FAULT_RTC_SYNC);
    defaults(); rtc_now=UINT32_MAX-1U; prepare(); commit(); stop_wakes=LP_WAKE_KEY;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && elapsed==3U && !irq_mask);
    defaults(); prepare(); LowPower_NotifyWake(LP_WAKE_KEY); LowPower_NotifyWake(LP_WAKE_COMM);
    assert(g_low_power_diag.first_wake==LP_WAKE_KEY);
    assert(g_low_power_diag.wake_mask==(LP_WAKE_KEY|LP_WAKE_COMM));
    LowPower_Request(LP_TARGET_RUN,0U); assert(LowPower_Poll()==LP_RUN_APPLICATION);
    defaults(); blocks=LP_BLOCK_MOS|LP_BLOCK_CURRENT; target=LP_TARGET_DEEP_RESET; reset_ok=0U;
    assert(LowPower_Poll()==LP_RUN_APPLICATION && (g_low_power_diag.fault_mask&LP_FAULT_RESET_SLEEP));
    puts("PASS: nonblocking prepare, abort, wake races, RTC sampling, bounded spurious returns, failures, wrap");
    return 0;
}
'''


def main():
    base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/framework-tests"
    base.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    source = folder / "harness.c"
    source.write_text(HARNESS, encoding="ascii")
    env = msvc_environment()
    result = subprocess.run(
        [shutil.which("cl.exe", path=env["PATH"]), "/nologo", "/TC", "/W4", "/O2",
         f"/I{SOURCE}", str(source), str(SOURCE / "low_power.c"),
         f"/Fe{folder / 'framework.exe'}"], cwd=folder, env=env, capture_output=True)
    log = (result.stdout + result.stderr).decode("gbk", errors="replace")
    (folder / "build.log").write_text(log, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(log)
    subprocess.run([folder / "framework.exe"], check=True, timeout=10)
    core = (SOURCE / "low_power.c").read_text()
    core = re.sub(r"/\*.*?\*/|//[^\n]*", "", core, flags=re.DOTALL)
    for token in ("GPIO", "USART", "EXTI", "__delay", "malloc"):
        assert token not in core, token
    print(f"Temporary outputs: {folder}")


if __name__ == "__main__":
    main()

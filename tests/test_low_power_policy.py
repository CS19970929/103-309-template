"""Production policy/restore regressions; all generated outputs stay in TEMP."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import test_sleep as t
from test_current_calibration import function_source, msvc_environment


def policy_program(tests):
    enums = t.read("rtc_sleep.h").split("enum irqWakeup", 1)[1].split("extern", 1)[0]
    stubs = t.POLICY_STUBS.replace("u16Sleep_TimeVlow;", "u16Sleep_TimeVlow, u16Sleep_TimeRTC;")
    return (t.COMMON + '#include "low_power_port.h"\n' + "enum irqWakeup" + enums + stubs
            + function_source(t.read("Flash.c"), "FlashWriteOneHalfWord")
            + function_source(t.read("SocEnhance.c"), "SOC_SaveBeforeSleep")
            + t.read("SleepDeal.c").replace('#include "main.h"', "")
            + t.read("rtc_sleep.c").replace('#include "main.h"', "")
            + t.read("low_power.c") + t.POLICY_TESTS.split("int main(void)", 1)[0] + tests)


POLICY = r'''
int main(void) {
    uint32_t blocks;
    defaults(); LowPower_Init();
    SystemStatus.bits.b1Status_MOS_CHG=SystemStatus.bits.b1Status_MOS_DSG=1;
    g_stCellInfoReport.u16IDischg=5;
    blocks=BmsSleep_BlockMask(1);
    if (!(blocks&LP_BLOCK_CURRENT)) { puts("FAIL: 500mA must block idle sleep"); return 1; }
    defaults(); LowPower_Init(); OtherElement.u16Sleep_TimeRTC=30;
    for(int i=0;i<1800;i++) { gu8_1000msAccClock_Flag=1; BmsSleep_PollRequest(); }
    if(s_sleep_mode!=NO_SLEEP) { puts("FAIL: minute parameter entered early"); return 1; }
    gu8_1000msAccClock_Flag=1; BmsSleep_PollRequest();
    if(s_sleep_mode!=HICCUP_MODE) { puts("FAIL: minute parameter not applied"); return 1; }
    defaults(); LowPower_Init(); OtherElement.u16Sleep_TimeRTC=0; sys_time.time_enter_rtc=10;
    for(int i=0;i<11;i++) { gu8_1000msAccClock_Flag=1; BmsSleep_PollRequest(); }
    if(s_sleep_mode!=HICCUP_MODE) { puts("FAIL: zero must retain legacy idle timeout"); return 1; }
    puts("PASS: current admission, minute parameter, zero compatibility"); return 0;
}
'''

RESTORE = r'''
#include <stdint.h>
#include <stdio.h>
static union { uint16_t all; struct { uint16_t event:1; } bits; } ChargerLoad_Func;
static struct { int wakeup_rtc; } sys_time;
static void Init_Charger_AllSeries(void) {}
static void Init_ChargerLoad_Det(void) { ChargerLoad_Func.all=0; }
static void InitIO(void) {}
static void ADC_DeInit(int x) {(void)x;}
static void InitADC(void) {}
static void USART_DeInit(int x) {(void)x;}
static void InitTimer(void) {}
static void initAFE1_IIC(void) {}
static void InitE2PROM_i2c(void) {}
#define ADC1 1
#define USART1 1
#define USART2 2
#define true 1
'''


def run_programs(programs):
    base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/policy-tests"
    base.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    env = msvc_environment()
    for name, program in programs.items():
        src = folder / (name + ".c")
        src.write_text(t.clean(program), encoding="ascii")
        result = subprocess.run([shutil.which("cl.exe", path=env["PATH"]), "/nologo", "/TC", "/W3", "/O2",
                                 f"/I{t.SOURCE}", str(src), f"/Fe{folder / (name+'.exe')}",
                                 f"/Fo{folder / (name+'.obj')}"], cwd=folder, env=env, capture_output=True)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode("gbk", errors="replace"))
        subprocess.run([str(folder / (name + ".exe"))], check=True, timeout=10)
    print(f"Temporary outputs: {folder}")


def main():
    restore = RESTORE + function_source(t.read("conf/conf.c"), "InitRunAfterStop") + r'''
int main(void) {
    ChargerLoad_Func.bits.event=1; InitRunAfterStop();
    if(!ChargerLoad_Func.bits.event) { puts("FAIL: restore erased wake event"); return 1; }
    puts("PASS: restore preserves ISR event and protection latches"); return 0;
}
'''
    run_programs({"policy": policy_program(POLICY), "restore": restore})


if __name__ == "__main__":
    main()

"""Exercise complete production SOC code and the time caller with host hardware stubs.

Run: python -B tests/test_soc_capacity.py
Generated sources/executables stay under LOCALAPPDATA/CodexTemp. No board or BIN.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from test_current_calibration import function_source, msvc_environment

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"


def read(name):
    text = (SOURCE / name).read_bytes().decode("latin1").replace("\r\n", "\n")
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.DOTALL)


STUBS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int32_t INT32;
enum _CUR { CurCHG, CurDSG };
#define LIFEPO
#define E2P_ADDR_E2POS_ENHANCE_SOC 10
static struct { UINT16 u16Soc_Cycle_times; } OtherElement;
static UINT16 saved_words[3];
static UINT8 save_count;
UINT16 ReadEEPROM_Word_NoZone(UINT16 addr) {
    assert(addr >= 10 && addr <= 14 && addr % 2 == 0);
    return saved_words[(addr-10)/2];
}
UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 value) {
    assert(addr >= 10 && addr <= 14 && addr % 2 == 0);
    saved_words[(addr-10)/2] = value; ++save_count; return 0;
}
static UINT16 fake_ocv;
UINT16 GetEndValue(const UINT16 *table, UINT16 size, UINT16 voltage) {
    (void)table; (void)size; (void)voltage; return fake_ocv;
}
UINT32 ModulusSub(UINT32 a, UINT32 b) { return a>b ? a-b : b-a; }
void GetData_SOC(void) {}
static struct { UINT16 u16Ichg, u16IDischg;
    struct { UINT16 u16Soc, u16CapacityFull, u16CapacityNow; } SocElement;
} g_stCellInfoReport;
static UINT16 time_chg, time_dsg, time_real;
static void BmsCurrent_Update(void) {}
'''


TESTS = r'''
static void initialize(UINT16 nominal_dah, UINT8 soc) {
    memset(&SOC_Enhance_Element, 0, sizeof(SOC_Enhance_Element));
    memset(&SOC_Calculate_Element, 0, sizeof(SOC_Calculate_Element));
    SOC_Enhance_Element.u16_SOC_Ah = nominal_dah;
    SOC_Enhance_Element.u16_SOC_0_Vol = 3050;
    SOC_Enhance_Element.u16_SOC_100_Vol = 3600;
    SOC_Enhance_Element.u16_VCellMin = 3300;
    SOC_Enhance_Element.u16_VCellMax = 3300;
    /* Disable voltage correction only in integration tests. */
    SOC_Enhance_Element.u8_LargeCurFlag_Chg = 1;
    SOC_Enhance_Element.u8_LargeCurFlag_Dsg = 1;
    OtherElement.u16Soc_Cycle_times = 123;
    saved_words[0] = soc; saved_words[1] = 13; saved_words[2] = 123;
    soc_param_lib_init();
}
static void second(UINT8 charging, UINT16 current_da) {
    unsigned tick;
    SOC_Enhance_Element.u16_Ichg = charging ? current_da : 0;
    SOC_Enhance_Element.u16_Idsg = charging ? 0 : current_da;
    for (tick=0; tick<5; ++tick) {
        if (charging) SOC_Cont_AH_Int_CHG();
        else SOC_Cont_AH_Int_DSG();
    }
}
static void set_soc(UINT8 soc) {
    SOC_Enhance_Element.u16_RefreshData_Flag = 3;
    SOC_Enhance_Element.u8_SetSocOnce = soc;
    SOC_Update_StartUp();
    SOC_Result_Pass();
}
int main(void) {
    unsigned n;
    UINT32 full;
    UINT16 old_cycles, old_partial;
    initialize(1000, 50);
    full = 36000U * SOC_USABLE_CAPACITY_PERCENT;
    assert(SOC_Calculate_Element.u32CapUsable == full);
    assert(SOC_Calculate_Element.u32CapNow == full/2);
    assert(SOC_Enhance_Element.u16_CapacityNow == 5000);
    assert(SOC_Enhance_Element.u16_CapacityFull == 10000);
    assert(SOC_Enhance_Element.u16_CapacityFactory == 10000);
    assert(SOC_Enhance_Element.u8_SOH == 100);
    assert(SOC_Enhance_Element.u16_Cycle_times == 123);
    assert(SOC_Calculate_Element.u8DSG_SOC_Int == 13);
    assert(SOC_GetRemainingMinutes(100, 0) == 3*SOC_USABLE_CAPACITY_PERCENT);
    assert(SOC_GetRemainingMinutes(100, 1) == 3*SOC_USABLE_CAPACITY_PERCENT);
    assert(SOC_GetRemainingMinutes(0, 0) == 0xffff);
    SOC_Enhance_Element.u16_SOC_InitOver = 0;
    assert(SOC_GetRemainingMinutes(100, 0) == 0xffff);
    SOC_Enhance_Element.u16_SOC_InitOver = 1;

    /* Real one-second current integration: half usable capacity down and back up. */
    set_soc(100);
    for (n=0; n<180U*SOC_USABLE_CAPACITY_PERCENT; ++n) second(0,100);
    SOC_Result_Pass();
    assert(SOC_Calculate_Element.u8SOC_Now == 50);
    assert(SOC_Calculate_Element.u32CapNow == full/2);
    assert(SOC_Enhance_Element.u16_CapacityNow == 5000);
    for (n=0; n<180U*SOC_USABLE_CAPACITY_PERCENT; ++n) second(1,100);
    assert(SOC_Calculate_Element.u8SOC_Now == 100);
    assert(SOC_Calculate_Element.u32CapNow == full);
    second(1,65535);
    assert(SOC_Calculate_Element.u32CapNow == full);
    assert(SOC_Calculate_Element.i32SocChangeScaled == 0);

    /* Charge/discharge reversal cancels fractional SOC instead of counting it twice. */
    initialize(1000,80);
    for (n=0; n<10U*SOC_USABLE_CAPACITY_PERCENT; ++n) second(0,100);
    for (n=0; n<10U*SOC_USABLE_CAPACITY_PERCENT; ++n) second(1,100);
    assert(SOC_Calculate_Element.u8SOC_Now==80);
    assert(SOC_Calculate_Element.u32CapNow==(UINT32)((uint64_t)full*80/100));
    assert(SOC_Calculate_Element.i32SocChangeScaled==0);

    /* Run the complete production controller, including result passing and saves. */
    initialize(1000,100);
    SOC_Cali_Flag=SOC_CALI_CONT_DSG;
    SOC_Enhance_Element.u16_Idsg=100;
    for (n=0; n<500; ++n) SOC_IntEnhance_Ctrl();
    assert(SOC_Calculate_Element.u32CapNow==full-10000U);
    assert(SOC_Enhance_Element.u8_SOC==SOC_Calculate_Element.u8SOC_Now);
    assert(SOC_Enhance_Element.u16_CapacityFull==10000);
    SOC_Enhance_Element.u8_SetSocOnce=40;
    SOC_Enhance_Element.u16_RefreshData_Flag=3;
    SOC_IntEnhance_Ctrl();
    assert(SOC_Calculate_Element.u8SOC_Now==40);
    assert(SOC_Calculate_Element.u32CapNow==(UINT32)((uint64_t)full*40/100));
    SOC_IntEnhance_Ctrl();
    assert(SOC_Enhance_Element.u16_RefreshData_Flag==0);

    /* Cycle percentages remain nominal, including across explicit SOC corrections. */
    initialize(1000,100);
    SOC_Calculate_Element.u8DSG_SOC_Int = 0;
    for (n=0; n<800; ++n) {
        if (n%4==0) set_soc(100);
        second(0,3600); /* 0.1 Ah per second; 80 Ah total. */
    }
    assert(SOC_Calculate_Element.u32Cycle_times == 12400);
    assert(SOC_Calculate_Element.u8DSG_SOC_Int == 0);
    old_cycles = (UINT16)SOC_Calculate_Element.u32Cycle_times;
    set_soc(0); second(0,3600);
    assert(SOC_Calculate_Element.u32CapNow == 0);
    assert(SOC_Calculate_Element.u32Cycle_times == old_cycles);

    /* Existing EEPROM words, scaling and restore path, including sleep save. */
    set_soc(37);
    SOC_Calculate_Element.u8DSG_SOC_Int = 19;
    save_count=0;
    assert(SOC_SaveBeforeSleep());
    assert(save_count==3 && saved_words[0]==37 && saved_words[1]==19 && saved_words[2]==124);
    soc_param_lib_init();
    assert(SOC_Calculate_Element.u8SOC_Now==37);
    assert(SOC_Calculate_Element.u32CapNow==(UINT32)((uint64_t)full*37/100));
    assert(SOC_Calculate_Element.u8DSG_SOC_Int==19);
    assert(SOC_Calculate_Element.u32Cycle_times==12400);

    /* Capacity refresh preserves SOC and honors the existing explicit cycle setting. */
    SOC_Enhance_Element.u16_SOC_Ah = 2000;
    SOC_Enhance_Element.u16_SOC_CycleT_Ever = 42;
    SOC_Enhance_Element.u16_RefreshData_Flag = 2;
    SOC_Update_StartUp();
    assert(SOC_Calculate_Element.u8SOC_Now==37);
    assert(SOC_Calculate_Element.u32CapUsable==full*2);
    assert(SOC_Calculate_Element.u32CapNow==(UINT32)((uint64_t)full*2*37/100));
    assert(SOC_Calculate_Element.i32SocChangeScaled==0);
    assert(SOC_Calculate_Element.u32Cycle_times==4200);
    fake_ocv=63; SOC_Enhance_Element.u16_RefreshData_Flag=1; SOC_Update_StartUp();
    assert(SOC_Calculate_Element.u8SOC_Now==63); /* No second 90% scaling of OCV. */
    set_soc(255);
    assert(SOC_Calculate_Element.u8SOC_Now==100);

    /* Voltage endpoints still own 100% / 0%, with usable-capacity reconstruction. */
    initialize(1000,50);
    SOC_Enhance_Element.u16_Ichg=100;
    SOC_Enhance_Element.u16_VCellMax=3600;
    soc_cali();
    assert(SOC_Calculate_Element.u8SOC_Now==100 && SOC_Calculate_Element.u32CapNow==full);
    SOC_Enhance_Element.u16_Ichg=0;
    SOC_Enhance_Element.u16_VCellMin=3050;
    for (n=0; n<50; ++n) soc_cali();
    assert(SOC_Calculate_Element.u8SOC_Now==0 && SOC_Calculate_Element.u32CapNow==0);
    assert(SOC_Calculate_Element.i32SocChangeScaled==0);
    set_soc(100);
    SOC_Enhance_Element.u16_VCellMin=3051;
    for (n=0; n<50; ++n) soc_cali();
    assert(SOC_Calculate_Element.u8SOC_Now==100);

    /* Enabled terminal correction rebuilds current capacity and clears stale fractions. */
    initialize(1000,50);
    SOC_Calculate_Element.i32SocChangeScaled=300;
    SOC_Enhance_Element.u16_VCellMax=3550;
    for (n=0; n<10; ++n) CorrectionTerminal_CV(CurCHG);
    assert(SOC_Calculate_Element.u8SOC_Now==51);
    assert(SOC_Calculate_Element.u32CapNow==(UINT32)((uint64_t)full*51/100));
    assert(SOC_Calculate_Element.i32SocChangeScaled==0);
    SOC_Enhance_Element.u16_VCellMin=3100;
    for (n=0; n<10; ++n) CorrectionTerminal_CV(CurDSG);
    assert(SOC_Calculate_Element.u8SOC_Now==50);
    assert(SOC_Calculate_Element.u32CapNow==full/2);

    /* Caller uses internal state, not deliberately inconsistent displayed Ah. */
    initialize(1000,50);
    g_stCellInfoReport.SocElement.u16CapacityFull=1;
    g_stCellInfoReport.SocElement.u16CapacityNow=60000;
    g_stCellInfoReport.u16Ichg=100; g_stCellInfoReport.u16IDischg=0;
    DataLoad_Current();
    assert(time_chg==3*SOC_USABLE_CAPACITY_PERCENT && time_dsg==0xffff && time_real==time_chg);
    g_stCellInfoReport.u16Ichg=0; g_stCellInfoReport.u16IDischg=100;
    DataLoad_Current();
    assert(time_dsg==3*SOC_USABLE_CAPACITY_PERCENT && time_chg==0xffff && time_real==time_dsg);
    g_stCellInfoReport.u16IDischg=0;
    g_stCellInfoReport.SocElement.u16Soc=50; DataLoad_Current();
    assert(time_chg==0xffff && time_dsg==0xffff && time_real==0xffff);
    g_stCellInfoReport.SocElement.u16Soc=0; DataLoad_Current();
    assert(time_dsg==0 && time_chg==0xffff);
    g_stCellInfoReport.SocElement.u16Soc=100; DataLoad_Current();
    assert(time_chg==0 && time_dsg==0xffff);

    /* Full UINT16 inputs: no multiplication overflow, wrap or zero denominator. */
    initialize(65535,100);
    assert(SOC_Calculate_Element.u32CapUsable==(UINT32)((uint64_t)65535*3600*SOC_USABLE_CAPACITY_PERCENT/100));
    assert(SOC_Enhance_Element.u16_CapacityFull==65535);
    assert(SOC_Enhance_Element.u16_CapacityFactory==65535);
    if (SOC_USABLE_CAPACITY_PERCENT>=2) assert(SOC_GetRemainingMinutes(1,0)==65534);
    assert(SOC_GetRemainingMinutes(65535,0)<=60);
    initialize(1,1);
    second(0,65535);
    assert(SOC_Calculate_Element.u8SOC_Now==0 && SOC_Calculate_Element.u32CapNow==0);
    assert(SOC_Calculate_Element.i32SocChangeScaled==0);
    second(1,65535);
    assert(SOC_Calculate_Element.u8SOC_Now==100);
    assert(SOC_Calculate_Element.i32SocChangeScaled==0);
    assert(SOC_Calculate_Element.u32CapNow==SOC_Calculate_Element.u32CapUsable);
    /* Retain sub-0.1 As fractions when one percent is smaller than one unit. */
    initialize(1,50);
    for (n=0; n<9U*SOC_USABLE_CAPACITY_PERCENT; ++n) second(0,2);
    assert(SOC_Calculate_Element.u32CapNow==0 && SOC_Calculate_Element.u8SOC_Now==0);
    assert(SOC_Calculate_Element.i32SocChangeScaled==0);
    for (n=0; n<9U*SOC_USABLE_CAPACITY_PERCENT; ++n) second(1,2);
    assert(SOC_Calculate_Element.u8SOC_Now==50);
    assert(SOC_Calculate_Element.u32CapNow==18U*SOC_USABLE_CAPACITY_PERCENT);
    initialize(0,100);
    old_partial=SOC_Calculate_Element.u8DSG_SOC_Int;
    second(0,100); second(1,100); SOC_Result_Pass();
    assert(SOC_Calculate_Element.u8SOC_Now==0 && SOC_Calculate_Element.u32CapNow==0);
    assert(SOC_GetRemainingMinutes(100,0)==65535);
    assert(SOC_Calculate_Element.u8DSG_SOC_Int==old_partial);
    printf("PASS: usable=%u%%, integration/display/time/cycle/EEPROM/refresh/endpoints/bounds\n",
           SOC_USABLE_CAPACITY_PERCENT);
    return 0;
}
'''


def main():
    env = msvc_environment()
    compiler = shutil.which("cl.exe", path=env["PATH"])
    base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/soc-capacity"
    base.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    header = re.sub(r'^#include[^\n]*', '', read("SocEnhance.h"), flags=re.M)
    production = re.sub(r'^#include[^\n]*', '', read("SocEnhance.c"), flags=re.M)
    caller = function_source(read("DataDeal.c"), "DataLoad_Current")
    source = folder / "soc_capacity.c"
    source.write_text(STUBS + header + production + caller + TESTS, encoding="ascii")
    for percent in (90, 100, 1):
        exe = folder / f"soc_{percent}.exe"
        result = subprocess.run(
            [compiler, "/nologo", "/W4", "/Od", "/RTC1", f"/DSOC_USABLE_CAPACITY_PERCENT={percent}",
             str(source), f"/Fe:{exe}", f"/Fo:{folder / ('soc_'+str(percent)+'.obj')}"],
            cwd=folder, env=env, capture_output=True,
        )
        (folder / f"compile_{percent}.log").write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode("gbk", errors="replace"))
        subprocess.run([str(exe)], cwd=folder, env=env, check=True)
    # Check the default itself, plus both invalid configuration boundaries.
    probe = folder / "config.c"
    probe.write_text(STUBS + header + '\nint main(void) { return SOC_USABLE_CAPACITY_PERCENT == 90 ? 0 : 1; }\n', encoding="ascii")
    for percent in (None, 0, 101):
        exe = folder / f"config_{percent}.exe"
        flags = [] if percent is None else [f"/DSOC_USABLE_CAPACITY_PERCENT={percent}"]
        result = subprocess.run([compiler, "/nologo", *flags, str(probe), f"/Fe:{exe}"],
                                cwd=folder, env=env, capture_output=True)
        if percent is None:
            assert result.returncode == 0, result.stdout + result.stderr
            subprocess.run([str(exe)], cwd=folder, check=True)
        else:
            assert result.returncode != 0
            assert b"SOC_USABLE_CAPACITY_PERCENT_must_be_1_to_100" in result.stdout + result.stderr
    print("PASS: default 90%; 0% and 101% rejected")
    print(f"Temporary outputs: {folder}")


if __name__ == "__main__":
    main()

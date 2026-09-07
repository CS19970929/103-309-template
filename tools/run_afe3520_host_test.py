"""Compile real AFE sources against a GPIO-level peripheral; all outputs are user-temp."""
import os
from pathlib import Path
import shutil
import subprocess
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "103 + 309/Project/Source"
OUT = Path(os.environ.get("LOCALAPPDATA", os.environ.get("TMPDIR", "/tmp"))) / "CodexTemp/afe-reference-align/host"


def main():
    project = ROOT / "103 + 309/Project/Users/BMS_SH3673520.uvprojx"
    expected = {"Afe3520.c", "Afe3520App.c", "BmsProtection3520.c", "BmsParameters.c", "rtc_sleep_afe3520.c"}
    for target in ET.parse(project).findall(".//Target"):
        entries = target.findall("./Groups/Group/Files/File")
        names = [e.findtext("FileName") for e in entries]
        for name in expected:
            assert names.count(name) == 1, (target.findtext("TargetName"), name)
        for entry in entries:
            path = (project.parent / entry.findtext("FilePath").replace("\\", "/")).resolve()
            assert path.is_file(), path
            assert not re.search(r"SH367309|I2C_AFE|ShortFunc", entry.findtext("FileName"))
    for path in SRC.rglob("*"):
        if path.suffix not in (".c", ".h") or "c073-cppcheck" in str(path):
            continue
        content = path.read_bytes().decode("latin1")
        assert not re.search(r"SH367309|I2C_AFE|MTPRead|MTPWrite|bq76xx|AFE_TYPE", content), path
        assert not re.search(r'#include\s+"[^"\n]+\.c"', content), path
    print("PASS: both Keil targets directly compile unique 3520 sources; obsolete sources absent")
    runtime = (SRC / "Runtime.c").read_bytes().decode("latin1")
    boot, loop = runtime.split("void Runtime_RunOnce", 1)
    assert boot.count("MosStartup_ApplyInitialState();") == 1
    assert boot.index("Afe3520_AppInit();") < boot.index("MosStartup_ApplyInitialState();") < boot.index("__enable_irq();")
    assert "MosStartup_ApplyInitialState" not in loop
    OUT.mkdir(parents=True, exist_ok=True)
    cc = shutil.which("cl") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        raise SystemExit("Run from a Visual Studio developer shell, or put gcc/clang on PATH.")
    msvc = Path(cc).name.lower() == "cl.exe"
    for mode, hardware, watchdog, port in ((m,h,w,p) for m in (1,2,3) for h,w in ((0,0),(0,1),(1,0),(1,1)) for p in (0,1)):
        exe = OUT / f"afe3520_mode{mode}_spi{hardware}_wdt{watchdog}_port{port}.exe"
        includes = [ROOT / "tools/afe3520_test_stubs", SRC]
        if msvc:
            cmd = [cc, "/nologo", "/std:c11", "/utf-8", "/W3", "/wd4819",
                   f"/DAFE3520_CFG_COMMON_PORT={port}", f"/DBMS3520_CFG_PROTECTION_MODE={mode}", f"/DAFE3520_CFG_WDT_ENABLE={watchdog}", f"/DAFE3520_CFG_USE_HARDWARE_SPI={hardware}", *[f"/I{p}" for p in includes],
                   str(ROOT / "tools/afe3520_host_test.c"), f"/Fe:{exe}", f"/Fo:{OUT}/"]
        else:
            cmd = [cc, "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
                   f"-DAFE3520_CFG_COMMON_PORT={port}", f"-DBMS3520_CFG_PROTECTION_MODE={mode}", f"-DAFE3520_CFG_WDT_ENABLE={watchdog}", f"-DAFE3520_CFG_USE_HARDWARE_SPI={hardware}", *[f"-I{p}" for p in includes],
                   str(ROOT / "tools/afe3520_host_test.c"), "-o", str(exe)]
        subprocess.run(cmd, cwd=OUT, check=True)
        subprocess.run([str(exe)], cwd=OUT, check=True)

    # Selected production bodies keep this state-machine harness focused.
    def function_body(source, name):
        text = (SRC / source).read_bytes().decode("latin1").replace("\r\n", "\n")
        match = re.search(r"^(?:static )?[\w]+ " + name + r"\([^)]*\)\s*\{.*?^\}", text, re.S | re.M)
        assert match, name
        # Source comments may be GBK; generated C contains only ASCII code.
        return re.sub(r"/\*.*?\*/|//[^\n]*", "", match.group(0), flags=re.S)
    functions = ["lp_refresh_status", "LowPower_Request", "LP_GetBlockReason",
                 "low_power_log_and_commit_sleep", "lp_select_deep_if_low_voltage",
                 "lp_update_sleep_request", "rtc_sleep_has_wakeup_exception", "lp_emergency_sleep_due"]
    text = "\n\n".join(function_body("rtc_sleep.c", name) for name in functions)
    text += "\n\n" + function_body("SleepDeal.c", "SleepDeal_Continue")
    for name in ("SleepDeal_CommitEmergencyBoot", "SleepDeal_EmergencySleep", "SleepDeal_WaitEmergencyWake"):
        text += "\n\n" + function_body("SleepDeal.c", name)
    text += "\n\n" + function_body("rtc_sleep.c", "LowPower_RequestCommandSleep")
    text += "\n\n" + function_body("rtc_sleep.c", "lp_process_command_sleep")
    text += "\n\n" + function_body("System_Monitor.c", "System_ERROR_UserCallback")
    text += "\n\n" + function_body("conf/conf.c", "Sys_StopMode")
    (OUT / "afe3520_sleep_functions.inc").write_text(text, encoding="ascii")
    for sleep_wdt in (0, 1):
        sleep_exe = OUT / f"sleep_wdt{sleep_wdt}.exe"
        sleep_cmd = [arg.replace(str(ROOT / "tools/afe3520_host_test.c"), str(ROOT / "tools/afe3520_recovery_sleep_test.c"))
                     .replace(str(exe), str(sleep_exe)).replace("WDT_ENABLE=1", f"WDT_ENABLE={sleep_wdt}") for arg in cmd]
        sleep_cmd.append(f"/I{OUT}" if msvc else f"-I{OUT}")
        subprocess.run(sleep_cmd, cwd=OUT, check=True)
        subprocess.run([str(sleep_exe)], cwd=OUT, check=True)

    # Boot migration must not reset calibration/SOC or unrelated shunt settings.
    migration = OUT / "shunt_migration.c"
    migration.write_text("""#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t UINT8;
typedef struct { uint16_t other[3]; } BMS_CONFIG;
#define CS_Res 2
#define CS_Res_Num 8
#define u16Sys_CS_Res 0
#define u16Sys_CS_Res_Num 1
#define BMS_OTHER_PARAM_WORD_INDEX(x) (x)
#define ERROR_REMOVE_EEPROM_STORE 0
#define ERROR_EEPROM_STORE 1
static BMS_CONFIG s_stConfigScratch, stored, applied;
static unsigned saves, fault, save_ok=1;
static int StorageFlash_LoadConfigData(BMS_CONFIG *p) { *p=stored; return 1; }
static int EEPROM_ConfigIsValid(const BMS_CONFIG *p) { (void)p; return 1; }
static void EEPROM_ApplyConfig(const BMS_CONFIG *p) { applied=*p; }
static int EEPROM_SaveConfigToFlash(void) { ++saves; return save_ok; }
static void System_ERROR_UserCallback(unsigned code) { fault=code; }
""" + function_body("EEPROM.c", "EEPROM_LoadConfigFromFlash") + """
int main(void) {
    stored.other[0]=2; stored.other[1]=2; stored.other[2]=1234;
    EEPROM_LoadConfigFromFlash();
    assert(saves==1 && applied.other[0]==2 && applied.other[1]==8 && applied.other[2]==1234 && !fault);
    saves=0; stored.other[1]=8; EEPROM_LoadConfigFromFlash(); assert(!saves);
    stored.other[0]=3; stored.other[1]=2; EEPROM_LoadConfigFromFlash();
    assert(!saves && applied.other[0]==3 && applied.other[1]==2);
    stored.other[0]=2; save_ok=0; EEPROM_LoadConfigFromFlash(); assert(fault);
    puts("PASS: only obsolete 2/2 shunt migrates to 2/8; other values survive; save error reported");
    return 0;
}
""", encoding="ascii")
    migration_exe = OUT / "shunt_migration.exe"
    migration_cmd = [arg.replace(str(ROOT / "tools/afe3520_host_test.c"), str(migration))
                     .replace(str(exe), str(migration_exe)) for arg in cmd]
    subprocess.run(migration_cmd, cwd=OUT, check=True)
    subprocess.run([str(migration_exe)], cwd=OUT, check=True)

    # Exercise the actual report-copy function, including its unused-channel sentinel.
    data = (SRC / "DataDeal.c").read_bytes().decode("latin1")
    function = re.search(r"void DataLoad_CellVolt\(void\)\s*\{.*?^\}", data, re.S | re.M).group(0)
    cell_source = OUT / "cell_mapping.c"
    cell_source.write_text("""#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t UINT8;
static UINT8 SeriesNum;
static struct { uint16_t u16VCell[20]; } g_afe3520Measurements;
static struct { uint16_t u16VCell[32]; } g_stCellInfoReport;
""" + function + """
int main(void) {
    unsigned n, i; const UINT8 counts[]={5,13,19,20};
    for(i=0;i<20;i++) g_afe3520Measurements.u16VCell[i]=(uint16_t)(3100+i*7);
    for(n=0;n<4;n++) {
        SeriesNum=counts[n]; DataLoad_CellVolt();
        for(i=0;i<SeriesNum;i++) assert(g_stCellInfoReport.u16VCell[i]==3100+i*7);
        for(;i<32;i++) assert(g_stCellInfoReport.u16VCell[i]==61001);
    }
    puts("PASS: 5/13/19/20-series report mapping and unused-channel sentinel");
    return 0;
}
""", encoding="ascii")
    cell_exe = OUT / "cell_mapping.exe"
    cmd = [arg.replace(str(ROOT / "tools/afe3520_host_test.c"), str(cell_source))
           .replace(str(exe), str(cell_exe)) for arg in cmd]
    subprocess.run(cmd, cwd=OUT, check=True)
    subprocess.run([str(cell_exe)], cwd=OUT, check=True)


if __name__ == "__main__":
    main()

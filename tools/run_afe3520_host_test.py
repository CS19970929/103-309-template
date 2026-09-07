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
    OUT.mkdir(parents=True, exist_ok=True)
    cc = shutil.which("cl") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        raise SystemExit("Run from a Visual Studio developer shell, or put gcc/clang on PATH.")
    msvc = Path(cc).name.lower() == "cl.exe"
    for watchdog in (0, 1):
        exe = OUT / f"afe3520_wdt{watchdog}.exe"
        includes = [ROOT / "tools/afe3520_test_stubs", SRC]
        if msvc:
            cmd = [cc, "/nologo", "/std:c11", "/utf-8", "/W3", "/wd4819",
                   f"/DAFE3520_CFG_WDT_ENABLE={watchdog}", *[f"/I{p}" for p in includes],
                   str(ROOT / "tools/afe3520_host_test.c"), f"/Fe:{exe}", f"/Fo:{OUT}/"]
        else:
            cmd = [cc, "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
                   f"-DAFE3520_CFG_WDT_ENABLE={watchdog}", *[f"-I{p}" for p in includes],
                   str(ROOT / "tools/afe3520_host_test.c"), "-o", str(exe)]
        subprocess.run(cmd, cwd=OUT, check=True)
        subprocess.run([str(exe)], cwd=OUT, check=True)

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

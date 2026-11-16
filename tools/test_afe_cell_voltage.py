"""Exercise real signed-CELL decoding and adjacent-pair sanity helpers.

No board I/O, no BIN/HEX, no MCU flash writes. Compiles extracted production C.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        depth += (source[pos] == "{") - (source[pos] == "}")
        if depth == 0:
            return source[start:pos + 1] + "\n"
    raise ValueError(signature)


def run() -> None:
    driver = (SOURCE / "I2C_AFE1.c").read_text(encoding="utf-8")
    mapping = (SOURCE / "DataDeal.c").read_text(encoding="utf-8")
    tables = mapping[mapping.index("const unsigned char SeriesSelect_AFE1[16][16]"):mapping.index("};", mapping.index("const unsigned char SeriesSelect_AFE1[16][16]")) + 2]
    defines = "\n".join(line for line in driver.splitlines() if
                         line.startswith("#define AFE_CELL_"))
    production = (defines + "\n" +
                  function(driver, "static UINT8 AfeCell_DecodeMv(") +
                  function(driver, "static UINT8 AfeCell_SuspiciousPair(") +
                  tables)
    prefix = """#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef int32_t INT32;
#define U16_SwapEndian(x) ((UINT16)((((UINT16)(x)) << 8) | (((UINT16)(x)) >> 8)))
static UINT16 from_be(UINT16 v) { return U16_SwapEndian(v); }
"""
    cases = """
int main(void) {
    UINT16 mv=0;unsigned n,i;UINT16 mask;UINT8 channel;
    assert(AfeCell_DecodeMv(from_be(0x5000U),&mv) && mv==3200U);
    assert(AfeCell_DecodeMv(from_be(0x7D00U),&mv) && mv==5000U);
    assert(!AfeCell_DecodeMv(from_be(0xE000U),&mv)); /* unsigned used to show 8960mV */
    assert(!AfeCell_DecodeMv(from_be(0xF000U),&mv)); /* unsigned used to show 9600mV */
    assert(!AfeCell_DecodeMv(from_be(0xFA00U),&mv)); /* unsigned used to show 10000mV */
    assert(!AfeCell_DecodeMv(from_be(0x0000U),&mv));
    assert(!AfeCell_DecodeMv(from_be(0x7FFFU),&mv));
    assert(AfeCell_SuspiciousPair(3300,3300,2200,4400));
    assert(AfeCell_SuspiciousPair(3300,3300,4400,2200));
    assert(!AfeCell_SuspiciousPair(3300,3300,3200,3400));
    assert(!AfeCell_SuspiciousPair(3300,3300,2200,3900));
    for(n=5;n<=16;n++) {
        mask=0U;
        for(i=0;i<n;i++) {
            channel=SeriesSelect_AFE1[n-1U][i];
            assert(channel<16U && !(mask & (1U<<channel)));
            mask|=(UINT16)(1U<<channel);
        }
    }
    assert(SeriesSelect_AFE1[12][8]==8 && SeriesSelect_AFE1[12][9]==9);
    puts("PASS signed CELL conversion, 9-10V anomaly rejection, inverse neighbor steps and 5..16 cell mapping");
    return 0;
}
"""
    output = Path(tempfile.mkdtemp(prefix="bms-cell-voltage-", dir=tempfile.gettempdir()))
    target = output / "cell_voltage.c"
    target.write_text(prefix + production + cases, encoding="utf-8")
    vs = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community")
    env = os.environ.copy()
    if os.name == "nt" and (vs / "Common7/Tools/VsDevCmd.bat").exists():
        setup = output / "compiler-env.bat"
        setup.write_text('@echo off\ncall "' + str(vs / "Common7/Tools/VsDevCmd.bat") +
                         '" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n', encoding="utf-8")
        for line in subprocess.check_output(["cmd.exe", "/d", "/c", str(setup)],
                                            text=True, errors="replace").splitlines():
            key, sep, value = line.partition("=")
            if sep and key:
                env[key] = value
        cc = sorted((vs / "VC/Tools/MSVC").glob("*/bin/Hostx64/x64/cl.exe"))[-1]
        exe = output / "cell_voltage.exe"
        command = [str(cc), "/nologo", "/W4", "/WX", "/utf-8", "/TC", "/std:c11",
                   str(target), "/Fe:" + str(exe), "/Fo:" + str(output / "cell_voltage.obj")]
    else:
        cc = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
        if not cc:
            raise RuntimeError("C compiler not found")
        exe = output / "cell_voltage"
        command = [str(cc), "-std=c99", "-Wall", "-Wextra", "-Werror",
                   str(target), "-o", str(exe)]
    subprocess.run(command, cwd=output, env=env, check=True)
    subprocess.run([str(exe)], cwd=output, check=True)
    print("Host-only evidence:", output)


if __name__ == "__main__":
    run()

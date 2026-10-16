"""Compile the actual current functions on Windows; keep all outputs in TEMP.

Run: python tests/test_current_calibration.py
Requires Visual Studio C++ Build Tools. This checks software arithmetic only.
"""

import ctypes
import os
from pathlib import Path
import random
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source/DataDeal.c"
HEADER = SOURCE.with_suffix(".h")


def msvc_environment():
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / (
        "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    install = subprocess.check_output(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         "-property", "installationPath"], text=True
    ).strip()
    if not install:
        raise RuntimeError("Visual Studio C++ tools not found")
    setup = Path(install) / "VC/Auxiliary/Build/vcvarsall.bat"
    result = subprocess.check_output(
        f'cmd.exe /d /s /c ""{setup}" x64 >nul && set"',
        text=True, errors="replace"
    )
    env = {key.upper(): value for key, value in os.environ.items()}
    for line in result.splitlines():
        key, separator, value = line.partition("=")
        if separator and key:
            env[key.upper()] = value
    return env


def function_source(source, name):
    match = re.search(
        r"^(?:static )?\w+ " + re.escape(name) + r"\([^;]*?\)\s*\{",
        source, re.MULTILINE
    )
    if not match:
        raise RuntimeError(f"Production function missing: {name}")
    depth = 1
    end = match.end()
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[match.start():end]


def build_library(folder):
    # Extract complete production function bodies, not a second implementation.
    source = SOURCE.read_bytes().decode("latin1")
    constants = "\n".join(re.findall(
        r"^#define (?:SH309_CADC|BOOT_CURRENT|CURRENT_)\w+[^\n]*",
        source, re.MULTILINE
    ))
    state = "\n".join(re.findall(
        r"^static (?:INT32|UINT8) g_(?:i32BootCurrentZeroRawX4|"
        r"u8BootCurrentZeroStatus|i32Current_mA) = [^;]+;",
        source, re.MULTILINE
    ))
    names = [
        "BmsCurrent_RawToSigned", "BmsCurrent_AbsI32",
        "SH309_CurrentRawX4To_mAX4", "BmsCurrent_mAX4To_mA",
        "BmsCurrent_Deadband_mA", "BmsCurrent_mAX4ToReport",
        "BmsCurrent_ApplyBootZeroX4", "BmsCurrent_IsBootZeroValid",
        "BmsCurrent_GetBootZeroRawX4", "BmsCurrent_GetCurrent_mA",
    ]
    calibration = "BmsCurrent_ApplyCalibration_mAX4"
    if re.search(r"\b" + calibration + r"\([^;]*?\)\s*\{", source):
        names.append(calibration)
    names.append("BmsCurrent_Update")
    bodies = "\n\n".join(function_source(source, name) for name in names)
    harness = r'''
#include <stdint.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int16_t INT16;
typedef int32_t INT32;
#include "HEADER_PATH"
#define log_i(...) ((void)0)
#define LP_RUN 0U
static struct { UINT32 state; } g_low_power_diag;
UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];
struct OTHER_ELEMENT OtherElement;
struct { UINT16 Cadc; } Registers_AFE1;
struct { UINT16 u16Ichg; UINT16 u16IDischg; } g_stCellInfoReport;
static UINT16 U16_SwapEndian(UINT16 value) {
    return (UINT16)((value << 8) | (value >> 8));
}
CONSTANTS
STATE
BODIES
__declspec(dllexport) void run_case(
    UINT16 raw, INT32 zero_x4, UINT8 zero_valid, UINT16 resistance,
    UINT16 count, UINT16 chg_k, INT16 chg_b, UINT16 dsg_k, INT16 dsg_b) {
    Registers_AFE1.Cadc = U16_SwapEndian(raw);
    OtherElement.u16Sys_CS_Res = resistance;
    OtherElement.u16Sys_CS_Res_Num = count;
    g_i32BootCurrentZeroRawX4 = zero_x4;
    g_u8BootCurrentZeroStatus = zero_valid ? BOOT_CURRENT_ZERO_VALID
                                         : BOOT_CURRENT_ZERO_NOT_RUN;
    g_u16CalibCoefK[MDL_ICHG] = chg_k;
    g_i16CalibCoefB[MDL_ICHG] = chg_b;
    g_u16CalibCoefK[MDL_IDSG] = dsg_k;
    g_i16CalibCoefB[MDL_IDSG] = dsg_b;
    BmsCurrent_Update();
}
__declspec(dllexport) INT32 current_ma(void) { return BmsCurrent_GetCurrent_mA(); }
__declspec(dllexport) UINT16 charge_report(void) { return g_stCellInfoReport.u16Ichg; }
__declspec(dllexport) UINT16 discharge_report(void) { return g_stCellInfoReport.u16IDischg; }
'''
    for token, value in {
        "HEADER_PATH": HEADER.as_posix(), "CONSTANTS": constants,
        "STATE": state, "BODIES": bodies,
    }.items():
        harness = harness.replace(token, value)
    path = folder / "current_harness.c"
    path.write_text(harness, encoding="latin1")
    dll = folder / "current_harness.dll"
    env = msvc_environment()
    compiler = shutil.which("cl.exe", path=env["PATH"])
    if not compiler:
        raise RuntimeError("MSVC compiler not found after vcvarsall")
    result = subprocess.run(
        # Existing header comments mix encodings; keep its definitions unchanged.
        [compiler, "/nologo", "/W4", "/WX", "/wd4819", "/O2", "/LD", str(path),
         f"/Fe:{dll}", f"/Fo:{folder / 'current_harness.obj'}"],
        cwd=folder, env=env, capture_output=True,
        text=True, errors="replace"
    )
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    lib = ctypes.CDLL(str(dll))
    lib.run_case.argtypes = [ctypes.c_uint16, ctypes.c_int32, ctypes.c_uint8,
                            ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16,
                            ctypes.c_int16, ctypes.c_uint16, ctypes.c_int16]
    lib.run_case.restype = None
    lib.current_ma.restype = ctypes.c_int32
    lib.charge_report.restype = ctypes.c_uint16
    lib.discharge_report.restype = ctypes.c_uint16
    return lib


def reference(case):
    raw, zero, valid, resistance, count, chg_k, chg_b, dsg_k, dsg_b = case
    signed_raw = raw if raw < 32768 else raw - 65536
    corrected = signed_raw * 4 - (zero if valid else 0)
    resistance = resistance or 2
    count = count or 2
    denominator = 2147 * resistance
    magnitude = (abs(corrected) * 20000 * count + denominator // 2) // denominator
    if not valid and magnitude < 500 * 4:
        magnitude = 0
    # K remains full-range; B is magnitude-limited near zero to prevent
    # a single nonzero CADC count creating an ampere-scale jump.
    if magnitude > 0:
        k, b = (chg_k, chg_b) if corrected > 0 else (dsg_k, dsg_b)
        k = k if 512 <= k <= 1536 else 1024
        b = b if -30000 <= b <= 30000 else 0
        offset = b * 1000 * 4
        offset = max(-magnitude * 1024, min(magnitude * 1024, offset))
        magnitude = max(0, magnitude * k + offset)
        magnitude = min(0x7FFFFFFF, (magnitude + 512) // 1024)
    current = (magnitude + 2) // 4
    current = current if corrected > 0 else -current
    report = min(65535, (magnitude + 200) // 400)
    return current, report if current > 0 else 0, report if current < 0 else 0


def check(lib, name, case):
    lib.run_case(*case)
    actual = lib.current_ma(), lib.charge_report(), lib.discharge_report()
    expected = reference(case)
    if actual != expected:
        raise AssertionError(f"{name}: case={case}, expected={expected}, actual={actual}")


def main():
    base = Path(os.environ.get("LOCALAPPDATA", tempfile.gettempdir())) / (
        "CodexTemp/103-309-template/current-calibration-tests"
    )
    base.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    lib = build_library(folder)
    cases = [
        ("default charge", (1074, 0, 1, 2, 2, 1024, 0, 1024, 0)),
        ("charge K", (1074, 0, 1, 2, 2, 1536, 0, 512, 0)),
        ("discharge K", (65536-1074, 0, 1, 2, 2, 1536, 0, 512, 0)),
        ("positive B is Q10 ampere", (1074, 0, 1, 2, 2, 1024, 1024, 1024, 0)),
        ("negative B", (65536-1074, 0, 1, 2, 2, 1024, 0, 1024, -1024)),
        ("negative result clamps to zero", (300, 0, 1, 2, 2, 512, -30000, 1536, 30000)),
        ("zero is not created by B", (4, 16, 1, 2, 2, 1536, 30000, 1536, 30000)),
        ("below former 2A boundary uses K/B", (214, 0, 1, 2, 2, 1536, 30000, 512, -30000)),
        ("above former 2A boundary uses K/B", (215, 0, 1, 2, 2, 1536, 30000, 512, -30000)),
        ("one positive count bounded B", (1, 0, 1, 2, 2, 1536, 1024, 512, -1024)),
        ("one negative count bounded B", (65535, 0, 1, 2, 2, 1536, -1024, 512, 1024)),
        ("large positive B cannot amplify one count", (1, 0, 1, 2, 2, 1536, 30000, 1536, 30000)),
        ("large discharge B cannot amplify one count", (65535, 0, 1, 2, 2, 1536, 30000, 1536, 30000)),
        ("small current reduced by negative B", (10, 0, 1, 2, 2, 1024, -1024, 1024, 0)),
        ("raw zero has no active calibration direction", (0, 0, 1, 2, 2, 1536, 30000, 1536, 30000)),
        ("fallback deadband", (53, 0, 0, 2, 2, 1536, 30000, 512, -30000)),
        ("fallback calibrates above 2A", (1074, 160, 0, 2, 2, 1536, 1024, 512, -1024)),
        ("direction after zero correction", (1, 160, 1, 2, 2, 1536, 30000, 512, -30000)),
        ("extreme shunt ratio", (32768, 160, 1, 1, 10000, 1536, 30000, 1536, 30000)),
        ("fractional shunt ratio", (32767, -160, 1, 65000, 9999, 1536, 30000, 512, -30000)),
        ("zero shunt parameters use defaults", (1074, 0, 1, 0, 0, 1536, 1024, 512, -1024)),
        ("invalid EEPROM coefficients use defaults", (1074, 0, 1, 2, 2, 65535, -32768, 0, 32767)),
    ]
    for name, case in cases:
        check(lib, name, case)
    # Independent behavioral bound, not just equality with the Python model.
    for raw in (1, 65535):
        for offset_b in (1024, 30000):
            case = (raw, 0, 1, 2, 2, 1536, offset_b, 1536, offset_b)
            lib.run_case(*case)
            assert abs(lib.current_ma()) < 30, ("one-count spike", case)
            assert lib.charge_report() == 0 and lib.discharge_report() == 0
    rng = random.Random(20261012)
    for index in range(10000):
        case = (rng.randrange(65536), rng.randrange(-160, 161), rng.randrange(2),
                rng.choice([1, 2, 3, 65000]), rng.choice([1, 2, 3, 10000]),
                rng.randint(512, 1536), rng.randint(-30000, 30000),
                rng.randint(512, 1536), rng.randint(-30000, 30000))
        check(lib, f"random {index}", case)
    for raw in range(65536):
        check(lib, "all signed CADC values, default K/B", (raw, 0, 1, 2, 2, 1024, 0, 1024, 0))
    print(f"PASS: {len(cases)} named cases, 10000 random cases, 65536 CADC values")
    print(f"Temporary outputs: {folder}")


if __name__ == "__main__":
    main()

"""Compile real AFE sources against a GPIO-level peripheral; all outputs are user-temp."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "103 + 309/Project/Source"
OUT = Path(os.environ.get("LOCALAPPDATA", os.environ.get("TMPDIR", "/tmp"))) / "CodexTemp/afe-reference-align/host"


def main():
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


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Build and run host tests for the production current K/B math."""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = Path(tempfile.gettempdir()) / "CodexTemp/103-309-copy/current-calibration-host"
SOURCE_DIR = ROOT / "103 + 309" / "Project" / "Source"


def compiler() -> str:
    env_cc = os.environ.get("CC")
    if env_cc:
        return env_cc
    for candidate in ("clang", "cc", "gcc"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("no C compiler found; install clang/gcc or set CC")


def main() -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    vs = Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community')
    if os.name == 'nt' and (vs / 'Common7/Tools/VsDevCmd.bat').exists():
        setup = BUILD_DIR / 'compiler-env.bat'
        setup.write_text('@echo off\ncall "' + str(vs / 'Common7/Tools/VsDevCmd.bat') +
                         '" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n')
        for line in subprocess.check_output(['cmd.exe', '/d', '/c', str(setup)], text=True, errors='replace').splitlines():
            key, sep, value = line.partition('=')
            if sep and key:
                env[key] = value
        cc = sorted((vs / 'VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))[-1]
        exe = BUILD_DIR / 'current_calibration_host_c_test.exe'
        cmd = [str(cc), '/nologo', '/W4', '/WX', '/utf-8', '/TC', '/std:c11',
               '/I' + str(SOURCE_DIR), str(ROOT / 'tools/current_calibration_host_c_test.c'),
               '/Fe:' + str(exe), '/Fo:' + str(BUILD_DIR / 'current.obj')]
    else:
        cc = compiler()
        exe = BUILD_DIR / 'current_calibration_host_c_test'
        cmd = [cc, '-std=c99', '-Wall', '-Wextra', '-Werror', '-I' + str(SOURCE_DIR),
               str(ROOT / 'tools/current_calibration_host_c_test.c'), '-o', str(exe)]
    print("Building current calibration host C test with:", cc, flush=True)
    subprocess.run(cmd, cwd=BUILD_DIR, env=env, check=True)
    print("Running", exe, flush=True)
    subprocess.run([str(exe)], cwd=BUILD_DIR, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

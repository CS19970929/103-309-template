#!/usr/bin/env python3
"""Build and run host tests for the production current K/B math."""

import os
import re
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
    source = (SOURCE_DIR / 'DataDeal.c').read_text(encoding='utf-8')
    header = (SOURCE_DIR / 'DataDeal.h').read_text(encoding='utf-8')
    conf = (SOURCE_DIR / 'SH367309_Func.h').read_text(encoding='utf-8', errors='replace')
    def function(name):
        match = re.search(r'^.*\b'+name+r'\([^;{}]*\)\s*\{',source,re.M)
        start=match.start(); brace=source.index('{',start);depth=1;end=brace+1
        while depth:
            depth+=(source[end]=='{')-(source[end]=='}');end+=1
        return source[start:end]+'\n'
    constants='\n'.join(line for line in source.splitlines() if line.startswith((
        '#define AFE_CURRENT_', '#define BOOT_CURRENT_', '#define CURRENT_')))+'\n'
    i2c=(SOURCE_DIR/'I2C_AFE1.h').read_text(encoding='utf-8',errors='replace')
    constants+='\n'.join(line for line in i2c.splitlines() if re.match(r'#define MTP_(CONF|BSTATUS3|ADC2)\b',line))+'\n'
    types=source[source.index('typedef struct _AFE_CURRENT_RUNTIME'):source.index('} AFE_CURRENT_RUNTIME;')+len('} AFE_CURRENT_RUNTIME;')]
    types+=header[header.index('enum AFE_CURRENT_ZERO_STATUS_E'):header.index('};',header.index('enum AFE_CURRENT_ZERO_STATUS_E'))+2]
    types+=conf[conf.index('typedef union __MTP_REG_CONF'):conf.index('}MTP_REG_CONF;')+len('}MTP_REG_CONF;')]
    names=('DataLoad_CurrentRawToSigned','DataLoad_CurrentAbsI32','DataLoad_CurrentRawX4ToMilliAmpX4',
           'DataLoad_CurrentMilliAmpX4ToMilliAmp','DataLoad_CurrentReadCadcRaw','AfeCurrent_BootZeroFail',
           'AfeCurrent_WaitFreshSample','AfeCurrent_PrepareBootZero','AfeCurrent_CheckBootZeroSafe',
           'AfeCurrent_StartupZeroCal','AfeCurrent_GetCurrent_mA','DataLoad_CurrentApplyCalibrationX4',
           'DataLoad_CurrentMilliAmpX4ToA10','DataLoad_Current')
    cases=(ROOT/'tools/current_sampling_cases.c').read_text(encoding='utf-8-sig')
    cases=cases.replace('/* CURRENT_TYPES */',constants+types)
    cases=cases.replace('/* CURRENT_FUNCTIONS */','\n'.join(function(name) for name in names))
    fixture=BUILD_DIR/'current_sampling.c';fixture.write_text(cases,encoding='utf-8')
    for virtual in (False,True):
        target=BUILD_DIR/('current_sampling_virtual.exe' if virtual else 'current_sampling.exe')
        if Path(cc).name.lower()=='cl.exe':
            build=[str(cc),'/nologo','/W3','/utf-8','/TC','/std:c11','/I'+str(SOURCE_DIR),
                   str(fixture),'/Fe:'+str(target),'/Fo:'+str(target.with_suffix('.obj'))]
            if virtual:build.append('/D__VIRTURE_CURRENT__')
        else:
            build=[str(cc),'-std=c99','-I'+str(SOURCE_DIR),str(fixture),'-o',str(target)]
            if virtual:build.append('-D__VIRTURE_CURRENT__')
        subprocess.run(build,cwd=BUILD_DIR,env=env,check=True)
        subprocess.run([str(target)],cwd=BUILD_DIR,check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

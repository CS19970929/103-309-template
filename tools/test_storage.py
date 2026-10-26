"""Run the real Flash storage code with half-word power-cut injection (no board writes)."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / '103 + 309/Project/Source'


def main():
    temp_root = Path(os.environ['LOCALAPPDATA']) / 'CodexTemp/103-309-copy'
    temp_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='storage-host-', dir=temp_root))
    vs = Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community')
    setup = output / 'compiler-env.bat'
    setup.write_text('@echo off\ncall "' + str(vs / 'Common7/Tools/VsDevCmd.bat') +
                     '" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n')
    env = os.environ.copy()
    for line in subprocess.check_output(['cmd.exe', '/d', '/c', str(setup)],
                                        text=True, errors='replace').splitlines():
        key, sep, value = line.partition('=')
        if sep and key:
            env[key] = value
    compiler = sorted((vs / 'VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))[-1]
    flash = (SOURCE / 'Flash.c').read_text(encoding='utf-8')
    # Exercise the production record/config implementation, without the old
    # storage engine or the unrelated SRAM IAP mailbox and MCU-reset routines.
    flash = flash[:flash.index('static UINT16 StorageFlash_RecordSpan(')]
    flash = flash.replace('#include "main.h"', '')
    code = (ROOT / 'tools/storage_host_cases.c').read_text(encoding='utf-8')
    code = code.replace('/* PRODUCTION_SOURCE */', flash)
    path = output / 'storage.c'
    path.write_text(code, encoding='utf-8')
    exe = output / 'storage.exe'
    result = subprocess.run([str(compiler), '/nologo', '/W3', '/utf-8', '/TC', '/std:c11',
                             '/I' + str(SOURCE), str(path), '/Fe:' + str(exe),
                             '/Fo:' + str(output / 'storage.obj')], cwd=output, env=env,
                            capture_output=True, text=True, errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    subprocess.run([str(exe)], cwd=output, check=True)
    print(f'Host evidence only; files: {output}')


if __name__ == '__main__':
    main()

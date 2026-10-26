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
    # Omit only the unrelated SRAM IAP mailbox and MCU-reset routines.
    flash = flash[:flash.index('UINT16 FlashReadOneHalfWord(UINT32 faddr)')]
    flash = flash.replace('#include "main.h"', '')
    code = (ROOT / 'tools/storage_host_cases.c').read_text(encoding='utf-8')
    code = code.replace('/* PRODUCTION_SOURCE */', flash)
    eeprom = (SOURCE / 'EEPROM.c').read_text(encoding='utf-8')
    initializer = eeprom[eeprom.index('UINT8 InitE2PROM(void)'):]
    # Simulate reflashing several firmware versions in one host process.
    initializer = initializer.replace('static const UINT8 version[32]=BMS_SOFTWARE_VERDION_DEFAULT;',
                                      'UINT8 version[32]; memcpy(version,firmware_version,32);')
    code = code.replace('/* CONFIG_INIT_SOURCE */', initializer)
    log = (SOURCE / 'LogRecord.c').read_text(encoding='utf-8')
    log_code = log[:log.index('void App_LogRecord(void)')]
    log_code += log[log.index('void Sci_ACK_0x03_ReadRegs_EventRecord('):log.index('void Sci_WrReg_0x06_Reset_EventRecord(')]
    log_code += log[log.index('UINT8 EEPROM_ResetData_EventRecord_ToDefault(void)'):]
    code = code.replace('/* LOG_SOURCE */', log_code.replace('#include "main.h"', ''))
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

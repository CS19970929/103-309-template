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
    vendor = (ROOT / '103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h').read_text(encoding='utf-8', errors='replace')
    code = code.replace('/* RESET_FLAGS */', '\n'.join(line for line in vendor.splitlines()
                        if line.lstrip().startswith('#define') and 'RCC_CSR_' in line))
    code = code.replace('/* PRODUCTION_SOURCE */', flash)
    eeprom = (SOURCE / 'EEPROM.c').read_text(encoding='utf-8')
    initializer = '\n'.join(line for line in eeprom.splitlines() if not line.startswith('#include'))
    # Simulate reflashing several firmware versions in one host process.
    initializer = initializer.replace('static const UINT8 version[32]=BMS_SOFTWARE_VERDION_DEFAULT;',
                                      'UINT8 version[32]; memcpy(version,firmware_version,32);')
    code = code.replace('/* CONFIG_INIT_SOURCE */', initializer)
    conf = (SOURCE / 'conf/conf.h').read_text(encoding='utf-8')
    data = (SOURCE / 'DataDeal.h').read_text(encoding='utf-8')
    headers = (SOURCE / 'conf/Project_Config.h').read_text(encoding='utf-8') + '\n'
    headers += '#if PROJECT_CFG_BAT_CHEMISTRY == 1\n#define LIFEPO\n#else\n#define TERNARYLI\n#endif\n'
    headers += 'enum { SOC_TABLE_LIFEPO=1, SOC_TABLE_TERNARYLI=2 };\n'
    headers += conf[conf.index('#define   CURR_10A'):conf.index('typedef enum GPIO_TYPE')]
    headers += data[data.index('enum tagInfoForKBArray {'):data.index('#define AFE_CURRENT_DIAG_VERSION')]
    for name in ('Fault.h','Heat_Cool.h','SH367309_DataDeal.h'):
        text = (SOURCE / name).read_text(encoding='utf-8',errors='replace')
        headers += '\n'.join(line for line in text.splitlines() if not line.startswith('#include'))+'\n'
    code = code.replace('/* PARAMETER_HEADERS */',headers)
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
    task = (SOURCE / 'DataDeal.c').read_text(encoding='utf-8')
    task = task[task.index('void App_AFEGet(void)'):]
    # App_AFEGet is currently the last definition; use its balanced body only.
    depth = 0
    for index in range(task.index('{'), len(task)):
        depth += (task[index]=='{')-(task[index]=='}')
        if depth==0:
            task=task[:index+1]
            break
    sampling = (ROOT / 'tools/storage_sampling_cases.c').read_text(encoding='utf-8')
    path = output / 'sampling.c'
    path.write_text(sampling.replace('/* AFE_TASK_SOURCE */',task), encoding='utf-8')
    exe = output / 'sampling.exe'
    result = subprocess.run([str(compiler), '/nologo', '/W3', '/utf-8', '/TC', '/std:c11',
                             str(path), '/Fe:' + str(exe), '/Fo:' + str(output / 'sampling.obj')],
                            cwd=output, env=env, capture_output=True, text=True, errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    subprocess.run([str(exe)], cwd=output, check=True)
    print(f'Host evidence only; files: {output}')


if __name__ == '__main__':
    main()

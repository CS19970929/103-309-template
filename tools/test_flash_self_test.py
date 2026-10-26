"""Exercise actual background Flash self-test code; all files stay in user temp."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'103 + 309/Project/Source'

def main():
    parent=Path(os.environ['LOCALAPPDATA'])/'CodexTemp/103-309-copy'
    parent.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix='flash-self-test-',dir=parent))
    vs=Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community')
    setup=output/'compiler-env.bat'
    setup.write_text('@echo off\ncall "'+str(vs/'Common7/Tools/VsDevCmd.bat')+'" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n')
    env=os.environ.copy()
    for line in subprocess.check_output(['cmd.exe','/d','/c',str(setup)],text=True,errors='replace').splitlines():
        key,sep,value=line.partition('=')
        if sep and key: env[key]=value
    compiler=sorted((vs/'VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))[-1]
    prefix=(ROOT/'tools/storage_host_cases.c').read_text(encoding='utf-8').split('/* PRODUCTION_SOURCE */')[0]
    prefix=prefix.replace('static void System_ERROR_UserCallback(int error) { ++errors; }',
        'static UINT8 System_ERROR_UserCallback(int error) { if(error==999) return (UINT8)afe_error; ++errors; return 0; }')
    prefix=prefix.replace('static int mutations,','static int afe_error;\nstatic int mutations,')
    prefix=prefix.replace('address>=FLASH_STORAGE_BEGIN && address<FLASH_STORAGE_END',
                          'address>=FLASH_PARAMETER_RESET_BEGIN && address<FLASH_STORAGE_END')
    prefix=prefix.replace('static int mutations,', 'static UINT32 hw_erases[8],hw_halfwords;\nstatic int mutations,')
    prefix=prefix.replace('++mutations; memset((void *)(uintptr_t)address,0xFF,1024);',
        '++mutations; if(address>=FLASH_SELF_TEST_BEGIN && address<FLASH_SELF_TEST_END) ++hw_erases[(address-FLASH_SELF_TEST_BEGIN)/1024]; memset((void *)(uintptr_t)address,0xFF,1024);')
    prefix=prefix.replace('++mutations; *p=value;',
        '++mutations; if(address>=FLASH_SELF_TEST_BEGIN && address<FLASH_SELF_TEST_END) ++hw_halfwords; *p=value;')
    prefix=prefix.replace('#define PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC 0','')
    prefix='#include "FlashSelfTest.h"\n'+prefix
    vendor=(ROOT/'103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h').read_text(encoding='utf-8',errors='replace')
    prefix=prefix.replace('/* RESET_FLAGS */','\n'.join(line for line in vendor.splitlines()
        if line.lstrip().startswith('#define') and 'RCC_CSR_' in line))
    flash=(SOURCE/'Flash.c').read_text(encoding='utf-8').split('UINT16 FlashReadOneHalfWord(UINT32 faddr)')[0]
    flash=flash.replace('#include "main.h"','')
    cases=(ROOT/'tools/flash_self_test_cases.c').read_text(encoding='utf-8')
    cases=cases.replace('sci_busy,can_busy,pending,afe_error','sci_busy,can_busy,pending')
    path=output/'selftest.c'
    path.write_text(prefix+cases.replace('/* PRODUCTION_SOURCE */',flash),encoding='utf-8')
    defines=['/DPROJECT_CFG_STORAGE_SELF_TEST_ENABLE=1','/DPROJECT_CFG_IRQ_DEBUG_ENABLE=1',
             '/DPROJECT_CFG_STORAGE_SELF_TEST_LOG_WRITES=2400','/DPROJECT_CFG_STORAGE_SELF_TEST_STATS_WRITES=1200',
             '/DPROJECT_CFG_STORAGE_SELF_TEST_CONFIG_WRITES=128']
    for name,soc in [('run',1200),('wide-soc-token',66000),('budget',1000000)]:
        exe=output/(name+'.exe')
        # The emulator reserves the MCU's 32-bit addresses in a 64-bit process.
        result=subprocess.run([str(compiler),'/nologo','/W3','/wd4312','/O2','/utf-8','/TC','/std:c11','/I'+str(SOURCE),
            *defines,f'/DPROJECT_CFG_STORAGE_SELF_TEST_SOC_WRITES={soc}',str(path),'/Fe:'+str(exe),
            '/Fo:'+str(output/(name+'.obj'))],cwd=output,env=env,capture_output=True,text=True,errors='replace')
        if result.returncode: raise RuntimeError(result.stdout+result.stderr)
        if re.search(r'warning C\d+',result.stdout+result.stderr):
            raise RuntimeError(result.stdout+result.stderr)
        subprocess.run([str(exe)],cwd=output,check=True)
    guard=output/'guard.c'
    guard.write_text('#include "conf/Project_BuildGuard.h"\n')
    checks=[('PROJECT_CFG_STORAGE_SELF_TEST_ENABLE',-1),('PROJECT_CFG_STORAGE_SELF_TEST_ENABLE',2)]
    for domain,minimum in [('CONFIG',100),('SOC',1000),('LOG',1000),('STATS',1000)]:
        checks += [(f'PROJECT_CFG_STORAGE_SELF_TEST_{domain}_WRITES',minimum-1),
                   (f'PROJECT_CFG_STORAGE_SELF_TEST_{domain}_WRITES',1000000001)]
    for key,value in checks:
        result=subprocess.run([str(compiler),'/nologo','/EP','/TC','/I'+str(SOURCE),
            f'/D{key}={value}',str(guard)],cwd=output,env=env,capture_output=True)
        assert result.returncode!=0,(key,value)
    print('PASS self-test build guard: rejects both invalid switches and all eight invalid targets')
    print(f'Host evidence only; files: {output}')

if __name__=='__main__': main()

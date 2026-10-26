"""Exercise the real switch and low-power policy; no firmware images or board I/O."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'103 + 309/Project/Source'

def main():
    parent=Path(os.environ['LOCALAPPDATA'])/'CodexTemp/103-309-copy'
    parent.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix='sleep-host-',dir=parent))
    vs=Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community')
    setup=output/'compiler-env.bat'
    setup.write_text('@echo off\ncall "'+str(vs/'Common7/Tools/VsDevCmd.bat')+'" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n')
    env=os.environ.copy()
    for line in subprocess.check_output(['cmd.exe','/d','/c',str(setup)],text=True,errors='replace').splitlines():
        key,sep,value=line.partition('=')
        if sep and key: env[key]=value
    compiler=sorted((vs/'VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))[-1]
    code=(ROOT/'tools/sleep_policy_cases.c').read_text(encoding='utf-8')
    for marker,name in (('SLEEP_SOURCE','SleepDeal.c'),('RTC_SOURCE','rtc_sleep.c')):
        text=(SOURCE/name).read_text(encoding='utf-8')
        code=code.replace('/* '+marker+' */','\n'.join(line for line in text.splitlines() if not line.startswith('#include')))
    afe=(SOURCE/'rtc_sleep_afe_sh367309.c').read_text(encoding='utf-8')
    code=code.replace('/* AFE_WAKE_SOURCE */',afe[afe.index('UINT8 RtcSleep_AfePortHasAfeWake('):])
    path=output/'sleep.c'; path.write_text(code,encoding='utf-8')
    exe=output/'sleep.exe'
    result=subprocess.run([str(compiler),'/nologo','/W3','/utf-8','/TC','/std:c11','/I'+str(SOURCE),
        str(path),'/Fe:'+str(exe),'/Fo:'+str(output/'sleep.obj')],cwd=output,env=env,capture_output=True,text=True,errors='replace')
    if result.returncode: raise RuntimeError(result.stdout+result.stderr)
    subprocess.run([str(exe)],cwd=output,check=True)
    print(f'Host evidence only; files: {output}')

if __name__=='__main__': main()

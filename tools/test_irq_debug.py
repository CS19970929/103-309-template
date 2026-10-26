"""Run actual ISR/debug/STOP code against fake registers; no device or image I/O."""
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / '103 + 309/Project/Source'


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    for pos in range(brace, len(text)):
        depth += (text[pos] == '{') - (text[pos] == '}')
        if not depth:
            return text[start:pos+1] + '\n'
    raise ValueError(signature)


def main():
    project = SOURCE.parent / 'Users/CommomSH367309_16series_103RCT6_C.uvprojx'
    for target in ET.parse(project).getroot().findall('Targets/Target'):
        paths = [node.text for node in target.findall('.//FilePath')]
        assert paths.count(r'..\Source\IrqDebug.c') == 1, target.findtext('TargetName')
        defines = target.findtext('TargetOption/TargetArmAds/Cads/VariousControls/Define') or ''
        assert 'PROJECT_CFG_IRQ_DEBUG_ENABLE=' not in defines
    parent = Path(os.environ['LOCALAPPDATA']) / 'CodexTemp/103-309-copy'
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='irq-host-', dir=parent))
    vs = Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community')
    setup = out / 'compiler-env.bat'
    setup.write_text('@echo off\ncall "' + str(vs / 'Common7/Tools/VsDevCmd.bat') +
                     '" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n')
    env = os.environ.copy()
    for line in subprocess.check_output(['cmd.exe', '/d', '/c', str(setup)], text=True, errors='replace').splitlines():
        key, sep, value = line.partition('=')
        if sep and key:
            env[key] = value
    compiler = sorted((vs / 'VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))[-1]
    body = (SOURCE / 'IrqDebug.c').read_text(encoding='utf-8').replace('#include "main.h"', '')
    interrupts = (SOURCE.parent / 'STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c').read_text(encoding='utf-8')
    names = ['NMI_Handler', 'HardFault_Handler', 'MemManage_Handler', 'BusFault_Handler',
             'UsageFault_Handler', 'SVC_Handler', 'DebugMon_Handler', 'PendSV_Handler', 'SysTick_Handler',
             'EXTI0_IRQHandler', 'EXTI2_IRQHandler', 'EXTI3_IRQHandler', 'EXTI9_5_IRQHandler',
             'EXTI15_10_IRQHandler', 'USART1_IRQHandler', 'USART2_IRQHandler', 'USART3_IRQHandler']
    body += ''.join(function(interrupts, 'void ' + name + '(') for name in names)
    for name, signatures in (
        ('RTC.c', ['static void RTC_HandleAlarmWakeup(', 'void RTCAlarm_IRQHandler(', 'void RTC_IRQHandler(']),
        ('System_Init.c', ['void TIM3_IRQHandler(']),
        ('Can_HDX.c', ['void USB_LP_CAN1_RX0_IRQHandler(']),
        ('conf/conf.c', ['void Sys_StopMode(']),
        ('StartupDefaultHandler.c', ['void IrqDebug_RecordUnhandledVector('])):
        text = (SOURCE / name).read_text(encoding='utf-8')
        body += ''.join(function(text, sig) for sig in signatures)
    fixture = (ROOT / 'tools/irq_debug_cases.c').read_text(encoding='utf-8')
    path = out / 'irq.c'
    path.write_text(fixture.replace('/* REAL_SOURCE */', body), encoding='utf-8')
    for enabled in (0, 1):
        exe = out / f'irq-{enabled}.exe'
        result = subprocess.run([str(compiler), '/nologo', '/W3', '/utf-8', '/TC', '/std:c11',
                                 '/I'+str(SOURCE), f'/DPROJECT_CFG_IRQ_DEBUG_ENABLE={enabled}',
                                 str(path), '/Fe:'+str(exe), '/Fo:'+str(out/f'irq-{enabled}.obj')],
                                cwd=out, env=env, capture_output=True, text=True, errors='replace')
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        subprocess.run([str(exe)], cwd=out, check=True)
    print(f'Host evidence only; files: {out}')


if __name__ == '__main__':
    main()

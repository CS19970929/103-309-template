"""Exercise real board tables/GPIO initialization; all host files stay in user temp."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / '103 + 309/Project/Source'


def without_includes(text):
    return '\n'.join(line for line in text.splitlines() if not line.startswith('#include'))


def main():
    parent = Path(os.environ['LOCALAPPDATA']) / 'CodexTemp/103-309-copy'
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='board-host-', dir=parent))
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
    code = (ROOT / 'tools/board_config_cases.c').read_text(encoding='utf-8')
    conf = (SOURCE / 'conf/conf.c').read_text(encoding='utf-8')
    code = code.replace('/* BOARD_DECLARATIONS */',
                        without_includes((SOURCE / 'ADC.h').read_text()) + '\n' +
                        without_includes((SOURCE / 'conf/Board_Config.h').read_text()))
    code = code.replace('/* BOARD_TABLES */',
                        without_includes((SOURCE / 'conf/Board_Config.c').read_text(encoding='utf-8')))
    code = code.replace('/* BOARD_INITIALIZATION */',
                        without_includes(conf[:conf.index('void Sys_StopMode(void)')]))
    code = code.replace('/* I2C_PIN_OPERATIONS */',
                        without_includes((SOURCE / 'I2C_AFE1.h').read_text(encoding='utf-8')))
    path = output / 'board.c'
    path.write_text(code, encoding='utf-8')
    # Wake flag AND serial role are required, independently for each UART.
    variants = [(1, 1, 1, 1, 1), (0, 1, 1, 1, 1), (1, 0, 1, 1, 1),
                (1, 1, 0, 1, 1), (1, 1, 1, 0, 1), (0, 0, 0, 0, 0)]
    for index, values in enumerate(variants):
        exe = output / f'board-{index}.exe'
        defines = [f'/D{name}={value}' for name, value in zip(
            ('TEST_UART1_WAKE', 'TEST_UART1_ROLE', 'TEST_UART2_WAKE', 'TEST_UART2_ROLE', 'TEST_RTC'), values)]
        result = subprocess.run([str(compiler), '/nologo', '/W3', '/wd4311', '/utf-8', '/TC', '/std:c11',
                                 *defines, str(path), '/Fe:' + str(exe),
                                 '/Fo:' + str(output / f'board-{index}.obj')],
                                cwd=output, env=env, capture_output=True, text=True, errors='replace')
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        subprocess.run([str(exe)], cwd=output, check=True)
    print(f'Host evidence only; files: {output}')


if __name__ == '__main__':
    main()

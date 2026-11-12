"""Compile and exercise the real SOC C implementation; all outputs stay in user temp."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"


def run():
    parent = Path(tempfile.gettempdir()) / "CodexTemp/103-309-copy"
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="soc-host-", dir=parent))
    def function(text, signature):
        start = text.index(signature)
        brace = text.index('{', start)
        while ';' in text[start:brace]:
            start = text.index(signature, start + len(signature))
            brace = text.index('{', start)
        depth = 0
        for pos in range(brace, len(text)):
            depth += (text[pos] == '{') - (text[pos] == '}')
            if depth == 0:
                return text[start:pos+1] + '\n'
        raise ValueError(signature)

    production = '\n'.join(line for line in (SOURCE / 'SOC.c').read_text(encoding='utf-8').splitlines()
                           if not line.startswith('#include'))
    data = (SOURCE / 'DataDeal.h').read_text(encoding='utf-8')
    begin = data.index('struct OTHER_ELEMENT {')
    parameters = data[begin:data.index('};', begin)+2] + '\n'
    for name in ('OtherElement_min', 'OtherElement_max'):
        lines = data[re.search(r'^#define\s+' + name, data, re.M).start():].splitlines()
        selected = []
        for line in lines:
            selected.append(line)
            if not line.rstrip().endswith('\\'):
                break
        parameters += '\n'.join(selected) + '\n'
    sci = (SOURCE / 'Sci_Upper.c').read_text(encoding='utf-8')
    signatures = ('static void Sci_SetWrError(', 'static UINT16 Sci_GetWrRegNum(',
                  'static UINT16 Sci_GetWrValue(', 'static UINT8 Sci_WrRegsByteCountValid(',
                  'static UINT8 Sci_RangeFits(', 'static UINT8 Sci_RangeOverlaps(',
                  'static void Sci_CopyWords(', 'static UINT8 Sci_WrValuesInRange(',
                  'static void Sci_WriteWordsFromRequest(', 'static void Sci_ApplyOtherElementSideEffects(',
                  'void Sci_WrRegs_0x10_OtherElement(', 'void Sci_WrReg_0x06_SetSocOnce(')
    production += '\n'.join(function(sci, signature) for signature in signatures)
    production += '\n'.join(line for line in (SOURCE / 'LowPowerSleep.c').read_text(encoding='utf-8').splitlines()
                            if not line.startswith('#include'))
    cases = (ROOT / 'tools/soc_host_c_test.c').read_text(encoding='utf-8')
    cases = cases.replace('/* SMOOTHING_TESTS */', (ROOT / 'tools/soc_smoothing_cases.c').read_text(encoding='utf-8'))
    code = cases.replace('/* PARAMETER_TYPES */', parameters).replace('/* PRODUCTION_SOURCE */', production)
    test_source = output / "soc.c"
    test_source.write_text(code, encoding="utf-8")
    env = os.environ.copy()
    vs = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community")
    if os.name == "nt" and (vs / "Common7/Tools/VsDevCmd.bat").exists():
        setup = output / "compiler-env.bat"
        setup.write_text('@echo off\ncall "' + str(vs / "Common7/Tools/VsDevCmd.bat") +
                         '" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n')
        for line in subprocess.check_output(["cmd.exe", "/d", "/c", str(setup)], text=True, errors="replace").splitlines():
            key, sep, value = line.partition("=")
            if sep and key:
                env[key] = value
        cc = sorted((vs / "VC/Tools/MSVC").glob("*/bin/Hostx64/x64/cl.exe"))[-1]
        exe = output / "soc.exe"
        cmd = [str(cc), "/nologo", "/W4", "/WX", "/utf-8", "/TC", "/std:c11",
               "/I" + str(SOURCE), str(test_source), "/Fe:" + str(exe), "/Fo:" + str(output / "soc.obj")]
    else:
        cc = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
        if not cc:
            raise RuntimeError("A host C compiler is required")
        exe = output / "soc"
        cmd = [str(cc), "-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + str(SOURCE),
               str(test_source), "-o", str(exe)]
    result = subprocess.run(cmd, cwd=output, env=env, capture_output=True, text=True, errors="replace")
    (output / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    result = subprocess.run([str(exe)], cwd=output, capture_output=True, text=True)
    (output / "results.txt").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="")
    print("SOC host evidence:", output)
    if result.returncode:
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    run()

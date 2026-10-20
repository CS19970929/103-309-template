"""Compile/link the configured Keil target without UV4 hooks or BIN conversion.

Run: python -B tests/check_armcc_no_bin.py
All generated files are written below LOCALAPPDATA/CodexTemp.
"""

import os
import argparse
from pathlib import Path
import re
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configured-optimization", action="store_true",
                        help="Use current Keil optimization; do not require the legacy O2 audit profile")
    args = parser.parse_args()
    target = ET.parse(PROJECT).find("./Targets/Target")
    ads = target.find("./TargetOption/TargetArmAds")
    ldads = ads.find("LDads")
    if ldads.findtext("useFile") != "0":
        raise RuntimeError("External linker input file is not supported by this validator")
    cads = ads.find("Cads")
    controls = cads.find("VariousControls")
    optim = int(cads.findtext("Optim"))
    if optim != 3 and not args.configured_optimization:
        raise RuntimeError(f"Expected explicit Keil Level 2 (-O2), got Optim={optim}")
    if cads.findtext("oTime") != "0":
        raise RuntimeError("Expected Optimize for Time to remain disabled (Ospace)")
    if not 0 <= optim <= 4:
        raise RuntimeError(f"Unsupported Keil optimization: {optim}")
    optimization = "" if optim == 0 else f"-O{optim - 1}"
    compiler = Path(os.environ.get("ARMCC_BIN", r"C:\Keil_v5\ARM\ARMCC\bin"))
    base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/armcc-no-bin"
    base.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    # Reject unsupported per-file overrides rather than silently validating another build.
    for element in target.findall("./Groups/Group") + target.findall("./Groups/Group/Files/File"):
        override = element.find(".//Cads")
        if override is not None:
            for setting in override:
                if setting.tag == "VariousControls":
                    if any((item.text or "").strip() for item in setting):
                        raise RuntimeError("Nonempty per-file compiler controls")
                elif setting.text not in ("0", "2"):
                    raise RuntimeError(f"Non-inherited per-file setting: {setting.tag}")
    flags = ["--cpu=Cortex-M3", "--apcs=interwork", "--library_type=microlib",
             "--split_sections", "-g", "-D__MICROLIB"]
    if optimization:
        flags.append(optimization)
    if cads.findtext("uC99") == "1":
        flags.append("--c99")
    if cads.findtext("uGnu") == "1":
        flags.append("--gnu")
    for define in controls.findtext("Define", "").split(","):
        if define.strip():
            flags += ["-D", define.strip()]
    for include in controls.findtext("IncludePath", "").split(";"):
        if include:
            flags += ["-I", str((PROJECT.parent / include).resolve())]
    if controls.findtext("MiscControls", "").strip():
        raise RuntimeError("Unexpected compiler MiscControls")
    logs = []
    objects = []
    started = time.monotonic()

    def run(args):
        result = subprocess.run([str(x) for x in args], cwd=output, capture_output=True)
        message = (result.stdout + result.stderr).decode("gbk", errors="replace")
        logs.append(subprocess.list2cmdline([str(x) for x in args]) + "\n" + message)
        (output / "build.log").write_text("\n".join(logs), encoding="utf-8")
        if result.returncode:
            raise RuntimeError(message + f"\nLog: {output / 'build.log'}")
        return message

    for index, entry in enumerate(target.findall("./Groups/Group/Files/File")):
        if entry.findtext("./FileOption/CommonProperty/IncludeInBuild") == "0":
            continue
        kind = entry.findtext("FileType")
        if kind not in ("1", "2"):
            continue
        source = (PROJECT.parent / entry.findtext("FilePath")).resolve()
        obj = output / f"{index}_{source.stem}.o"
        if kind == "1":
            run([compiler / "armcc.exe", *flags, "-c", source, "-o", obj])
        else:
            run([compiler / "armasm.exe", "--cpu=Cortex-M3", "--apcs=interwork",
                 "--pd", "__MICROLIB SETA 1", "-g", source, "-o", obj])
        objects.append(obj)
    # Keil OCR_RVCT4 / OCR_RVCT5 hold the selected IROM1 / IRAM1 ranges.
    memories = ads.find("./ArmAdsMisc/OnChipMemories")
    rom = memories.find("OCR_RVCT4")
    ram = memories.find("OCR_RVCT5")
    if int(ram.findtext("Size"), 0) == 0:
        ram = memories.find("IRAM")
    scatter = output / "target.sct"
    if ldads.findtext("umfTarg") == "0":
        configured_scatter = (PROJECT.parent / ldads.findtext("ScatterFile", "")).resolve()
        if not configured_scatter.is_file():
            raise RuntimeError(f"Missing configured scatter file: {configured_scatter}")
        scatter.write_bytes(configured_scatter.read_bytes())
    else:
        scatter.write_text(
            f"LR_IROM1 {rom.findtext('StartAddress')} {rom.findtext('Size')} {{\n"
            f" ER_IROM1 {rom.findtext('StartAddress')} {rom.findtext('Size')} {{\n"
            "  *.o (RESET, +First)\n  *(InRoot$$Sections)\n  .ANY (+RO)\n }\n"
            f" RW_IRAM1 {ram.findtext('StartAddress')} {ram.findtext('Size')} {{\n"
            "  .ANY (+RW +ZI)\n }\n}\n", encoding="ascii")
    message = run([compiler / "armlink.exe", *objects, "--library_type=microlib",
                   "--strict", "--scatter", scatter, "--summary_stderr", "--info=summarysizes",
                   "--map", "--list", output / "target.map", "-o", output / "target.axf"])
    stop_section = re.search(r"^\s*(0x[0-9a-fA-F]+).*\bi\.PWR_EnterSTOPMode\b",
                             (output / "target.map").read_text(), re.MULTILINE)
    if stop_section is None or not 0x20000000 <= int(stop_section[1], 16) < 0x20005000:
        raise RuntimeError("PWR_EnterSTOPMode must execute from RAM on this target")
    disassembly_result = subprocess.run(
        [str(compiler / "fromelf.exe"), "--text", "-c", str(output / "target.axf")],
        cwd=output, capture_output=True)
    disassembly = (disassembly_result.stdout + disassembly_result.stderr).decode(
        "gbk", errors="replace").replace("\r\n", "\n")
    if disassembly_result.returncode:
        raise RuntimeError(f"fromelf disassembly failed:\n{disassembly}")
    delay_start = disassembly.find("\n    i.Delay1ms\n")
    delay_end = disassembly.find("\n    i.", delay_start + 1)
    delay = disassembly[delay_start:delay_end if delay_end >= 0 else None]
    if delay_start < 0 or "__delay_ms" not in delay:
        raise RuntimeError("Disassembly does not preserve Delay1ms -> __delay_ms")
    assert not list(output.rglob("*.bin"))
    warnings = len(re.findall(r"\bWarning:", "\n".join(logs)))
    print(f"PASS: {target.findtext('TargetName')}, {len(objects)} objects, ARMCC {optimization or 'default'} compile + AXF link")
    print(message.strip())
    print("Disassembly: Delay1ms -> __delay_ms")
    print(f"Warnings: {warnings}; elapsed: {time.monotonic() - started:.1f}s")
    print(f"Temporary outputs: {output}")
    print("No UV4, HEX/BIN conversion or board operation executed; fromelf was disassembly-only.")


if __name__ == "__main__":
    main()

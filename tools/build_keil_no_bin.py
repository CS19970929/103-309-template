"""Build an isolated copy of the actual Keil project, without BIN hooks/board writes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROJECT = ROOT / "103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--uv4", type=Path, default=Path(r"C:\Keil_v5\UV4\UV4.exe"))
    args = parser.parse_args()
    project = args.project.resolve()
    if not project.is_file() or not args.uv4.is_file():
        parser.error("Keil executable or project missing")
    base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/keil-no-bin"
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    tree = ET.parse(project)
    target = tree.find("./Targets/Target")
    common = target.find("./TargetOption/TargetCommonOption")
    for name in ("OutputDirectory", "ListingPath"):
        common.find(name).text = str(out) + "\\"
    common.find("CreateHexFile").text = "0"
    for hook in ("BeforeMake", "AfterMake", "BeforeCompile"):
        node = common.find(hook)
        if node is not None:
            for flag in node:
                if flag.tag.startswith("RunUserProg"):
                    flag.text = "0"
    for item in target.findall("./Groups/Group/Files/File/FilePath"):
        item.text = str((project.parent / item.text).resolve())
    for controls in target.findall(".//VariousControls"):
        node = controls.find("IncludePath")
        if node is not None and node.text:
            node.text = ";".join(str((project.parent / p).resolve()) for p in node.text.split(";") if p)
    scatter = target.find("./TargetOption/TargetArmAds/LDads/ScatterFile")
    if scatter is not None and scatter.text:
        scatter.text = str((project.parent / scatter.text).resolve())
    copy = out / project.name
    tree.write(copy, encoding="utf-8", xml_declaration=True)
    log = out / "keil-build.log"
    command = [str(args.uv4), "-r", str(copy), "-t", target.findtext("TargetName"), "-j0", "-o", str(log)]
    result = subprocess.run(command, cwd=out, capture_output=True, timeout=300)
    text = log.read_bytes().decode("gbk", errors="replace") if log.exists() else ""
    size = re.search(r"Program Size:.*", text)
    verdict = re.search(r"\b0 Error\(s\)", text)
    bins = list(out.rglob("*.bin"))
    artifacts = list(out.glob("*.axf"))
    print(text[-5000:])
    if not verdict or not artifacts or bins:
        raise RuntimeError(f"Keil build failed or unexpected BIN (exit={result.returncode}); inspect {out}")
    manifest = {
        "project": str(project), "target": target.findtext("TargetName"),
        "device": common.findtext("Device"),
        "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "dirty_status": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
        "size": size[0] if size else "unavailable", "log": str(log),
        "artifacts": [{"path": str(a), "sha256": hashlib.sha256(a.read_bytes()).hexdigest()} for a in artifacts],
        "bin_generated": False, "board_operated": False,
    }
    regions = []
    for map_file in out.glob("*.map"):
        content = map_file.read_bytes().decode("ascii", errors="replace")
        for match in re.finditer(r"Load Region\s+(\S+)\s+\(Base:\s*(0x[0-9a-fA-F]+),\s*Size:\s*(0x[0-9a-fA-F]+)", content):
            regions.append({"name": match[1], "start": int(match[2], 16), "size": int(match[3], 16)})
    manifest["load_regions"] = regions
    if "STM32F103C8" in common.findtext("Device", ""):
        manifest["fits_declared_64k_flash"] = bool(regions) and all(
            r["start"] >= 0x08000000 and r["start"] + r["size"] <= 0x08010000 for r in regions)
        if not manifest["fits_declared_64k_flash"]:
            print("WARNING: image does not fit the declared C8 64-KiB Flash; board/BOM/layout sign-off required before flashing.")
        print("WARNING: existing Flash.h sleep/update addresses also exceed 64 KiB; unchanged by this repair.")
    (out / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"PASS: native Keil compile/link; no BIN; evidence: {out}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compile-check C translation units from a Keil .uvprojx with GNU Arm Embedded GCC.

This intentionally does not perform the final link. The production project remains
Keil/ARMCC5; the goal is to catch syntax, include, type, macro and translation-unit
regressions in GitHub-hosted CI before the Windows production build runs.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path
import xml.etree.ElementTree as ET


def split_semicolon(value: str | None) -> list[str]:
    if not value:
        return []
    return [item.strip() for item in value.split(";") if item.strip()]


def split_defines(value: str | None) -> list[str]:
    if not value:
        return []
    return [item.strip() for item in re.split(r"[;,]", value) if item.strip()]


def resolve_uv_path(base: Path, value: str) -> Path:
    path = Path(value.replace("\\", "/"))
    if not path.is_absolute():
        path = base / path
    return path.resolve()


def detect_cpu(target: ET.Element) -> str:
    cpu_text = target.findtext("./TargetOption/TargetCommonOption/Cpu", default="")
    match = re.search(r'CPUTYPE\("([^\"]+)"\)', cpu_text)
    cpu_name = match.group(1) if match else "Cortex-M3"
    mapping = {
        "Cortex-M0": "cortex-m0",
        "Cortex-M0+": "cortex-m0plus",
        "Cortex-M3": "cortex-m3",
        "Cortex-M4": "cortex-m4",
        "Cortex-M7": "cortex-m7",
    }
    if cpu_name not in mapping:
        raise RuntimeError(f"Unsupported/unknown CPU in uvprojx: {cpu_name}")
    return mapping[cpu_name]


def collect_project_settings(project: Path):
    root = ET.parse(project).getroot()
    target = root.find("./Targets/Target")
    if target is None:
        raise RuntimeError("No <Target> found in uvprojx")

    cpu = detect_cpu(target)
    controls = target.find("./TargetOption/TargetArmAds/Cads/VariousControls")
    if controls is None:
        raise RuntimeError("No ARM C compiler controls found in uvprojx")

    defines = split_defines(controls.findtext("Define"))
    include_values = split_semicolon(controls.findtext("IncludePath"))
    project_dir = project.parent
    include_dirs = [resolve_uv_path(project_dir, item) for item in include_values]

    c_sources: list[Path] = []
    non_gnu_link_inputs: list[Path] = []
    for file_node in target.findall("./Groups/Group/Files/File"):
        file_path = file_node.findtext("FilePath")
        if not file_path:
            continue
        resolved = resolve_uv_path(project_dir, file_path)
        suffix = resolved.suffix.lower()
        if suffix == ".c":
            c_sources.append(resolved)
        elif suffix in {".s", ".asm", ".lib", ".a"}:
            non_gnu_link_inputs.append(resolved)

    if not c_sources:
        raise RuntimeError("No C source files found in uvprojx")

    return cpu, defines, include_dirs, c_sources, non_gnu_link_inputs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", required=True, help="Path to Keil .uvprojx")
    parser.add_argument("--out", default="build/ci/gcc-source-check", help="Output directory")
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    args = parser.parse_args()

    project = Path(args.project).resolve()
    out_dir = Path(args.out).resolve()

    if not project.is_file():
        print(f"error: project not found: {project}", file=sys.stderr)
        return 2

    compiler = shutil.which(args.compiler)
    if not compiler:
        print(f"error: compiler not found in PATH: {args.compiler}", file=sys.stderr)
        return 2

    try:
        cpu, defines, include_dirs, sources, non_gnu_link_inputs = collect_project_settings(project)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    missing_sources = [str(path) for path in sources if not path.exists()]
    if missing_sources:
        print("error: uvprojx references missing source files:", file=sys.stderr)
        for item in missing_sources:
            print(f"  - {item}", file=sys.stderr)
        return 2

    missing_include_dirs = [path for path in include_dirs if not path.exists()]
    for path in missing_include_dirs:
        print(f"warning: skipping missing include directory from legacy uvprojx: {path}")
    include_dirs = [path for path in include_dirs if path.exists()]

    out_dir.mkdir(parents=True, exist_ok=True)

    common_flags = [
        f"-mcpu={cpu}",
        "-mthumb",
        "-std=gnu11",
        "-ffreestanding",
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-common",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-unused-function",
    ]
    for define in defines:
        common_flags.append(f"-D{define}")
    for include_dir in include_dirs:
        common_flags.extend(["-I", str(include_dir)])

    print(f"Project : {project}")
    print(f"Compiler: {compiler}")
    print(f"CPU     : {cpu}")
    print(f"Defines : {', '.join(defines) if defines else '(none)'}")
    print(f"C files : {len(sources)}")
    if non_gnu_link_inputs:
        print("Note    : final link is intentionally skipped; legacy Keil inputs remain production-owned.")

    failures: list[Path] = []
    for index, source in enumerate(sources, start=1):
        object_path = out_dir / f"{index:03d}_{source.stem}.o"
        command = [compiler, *common_flags, "-c", str(source), "-o", str(object_path)]
        print(f"[{index:02d}/{len(sources):02d}] {source}")
        result = subprocess.run(command, text=True)
        if result.returncode != 0:
            failures.append(source)

    if failures:
        print("\nCompile-check failed for:", file=sys.stderr)
        for source in failures:
            print(f"  - {source}", file=sys.stderr)
        return 1

    print(f"\nPASS: compiled {len(sources)} C translation units.")
    print("Full production link must still pass the Windows Keil/ARMCC5 job.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Compile-check C translation units from a Keil .uvprojx with GNU Arm Embedded GCC.

This intentionally does not perform the final link. The legacy project links ARMCC5
.lib archives, which are not assumed to be GNU ld compatible. The goal of this
check is to catch include, syntax, type, macro, and per-translation-unit compile
errors in cloud CI without changing the existing Keil build.
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
    """Split Keil macro definitions separated by comma or semicolon."""
    if not value:
        return []
    return [item.strip() for item in re.split(r"[;,]", value) if item.strip()]


def resolve_uv_path(base: Path, value: str) -> Path:
    normalized = value.replace("\\", "/")
    path = Path(normalized)
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


def collect_project_settings(project: Path) -> tuple[str, list[str], list[Path], list[Path], list[Path]]:
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
    legacy_libs: list[Path] = []
    asm_sources: list[Path] = []
    for file_node in target.findall("./Groups/Group/Files/File"):
        file_path = file_node.findtext("FilePath")
        if not file_path:
            continue
        resolved = resolve_uv_path(project_dir, file_path)
        suffix = resolved.suffix.lower()
        if suffix == ".c":
            c_sources.append(resolved)
        elif suffix in {".s", ".asm"}:
            asm_sources.append(resolved)
        elif suffix in {".lib", ".a"}:
            legacy_libs.append(resolved)

    if not c_sources:
        raise RuntimeError("No C source files found in uvprojx")

    return cpu, defines, include_dirs, c_sources, legacy_libs + asm_sources


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", required=True, help="Path to Keil .uvprojx")
    parser.add_argument("--out", default="build/ci/gcc-source-check", help="Output directory")
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument(
        "--exclude-basename",
        action="append",
        default=[],
        help="Skip a compiler-specific source basename (repeatable)",
    )
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

    missing = [str(path) for path in [*include_dirs, *sources] if not path.exists()]
    if missing:
        print("error: uvprojx references missing paths:", file=sys.stderr)
        for item in missing:
            print(f"  - {item}", file=sys.stderr)
        return 2

    excluded_names = set(args.exclude_basename)
    skipped_sources = [source for source in sources if source.name in excluded_names]
    sources = [source for source in sources if source.name not in excluded_names]
    if not sources:
        print("error: no C sources remain after exclusions", file=sys.stderr)
        return 2

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
    for source in skipped_sources:
        print(f"Skip    : {source} (compiler-specific compatibility unit)")
    if non_gnu_link_inputs:
        print("Note    : final link is intentionally skipped; legacy .lib/ASM inputs remain Keil-owned.")

    failures: list[Path] = []
    for index, source in enumerate(sources, start=1):
        object_name = f"{index:03d}_{source.stem}.o"
        object_path = out_dir / object_name
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
    if non_gnu_link_inputs:
        print("Full firmware link must still be validated by the Keil ARMCC5 CI job.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# Firmware CI

This repository has two verification levels.

## 1. GitHub-hosted GNU Arm source compile check

Runs automatically on `ubuntu-latest` for pushes and pull requests.

It parses the existing Keil `.uvprojx`, reuses its C source list, include paths and preprocessor defines, and compiles every C translation unit with `arm-none-eabi-gcc`.

This catches common AI/code-change regressions such as:

- missing headers or include-path errors;
- syntax errors;
- invalid types or declarations;
- broken preprocessor branches;
- many target-specific compile errors.

It deliberately does **not** perform the final firmware link because the production project links legacy ARMCC5 `.lib` archives.

Local/cloud command:

```bash
python3 tools/ci/stm32_source_check.py \
  --project "C030v1.0/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx" \
  --out build/ci/gcc-source-check
```

## 2. Keil ARMCC5 production build

The workflow already contains an optional full-build job matching the current production toolchain.

Requirements for the runner:

- Windows;
- Keil MDK with ARMCC 5.06 update 7 compatible support;
- a valid license for the project;
- GitHub Actions self-hosted runner;
- custom runner label: `keil-armcc5`;
- `UV4.exe` at `C:\Keil_v5\UV4\UV4.exe`, `C:\Keil\UV4\UV4.exe`, or pointed to by environment variable `KEIL_UV4`.

After the runner is online, set repository Actions variable:

```text
KEIL_CI_ENABLED=1
```

The workflow then invokes:

```powershell
tools/ci/keil_build.ps1 -Project "C030v1.0/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx"
```

A production build is considered successful only when the Keil log contains `0 Error(s)`.

## cppcheck

`cppcheck` runs in CI and uploads its diagnostics. It is intentionally non-blocking for the first baseline because this is an existing legacy codebase. After existing findings are triaged, the workflow can be tightened to reject newly introduced warnings.

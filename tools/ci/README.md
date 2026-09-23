# STM32F103 CI / Windows Keil Runner

Production project:

`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`

The Keil project is the production source of truth. It is configured for ARM Compiler 5.06 update 7.

## Verification levels

### 1. GitHub-hosted source compile gate

`.github/workflows/stm32-f103-ci.yml` runs on GitHub-hosted Ubuntu for pushes and pull requests.

It parses the existing Keil project and compiles the portable C translation units with GNU Arm GCC. This catches syntax, include, type, declaration and many preprocessor regressions without changing the production toolchain.

Legacy ARMCC-only CMSIS/interrupt shim sources are intentionally excluded from this compatibility gate and remain covered by the production Keil build.

### 2. Windows Keil production build

The real firmware build runs on a Windows self-hosted GitHub Actions runner with label:

`stm32-keil`

The build calls:

```powershell
.\tools\ci\keil_build.ps1
```

It:

- opens the existing `.uvprojx` through uVision command line;
- builds `Target 1`;
- requires zero Keil errors;
- stores the uVision log;
- collects AXF/MAP/listing outputs;
- generates BIN/HEX with ARMCC5 `fromelf.exe` when available;
- uploads `build/keil` as a GitHub Actions artifact.

## One-time Windows setup

Use the same Windows computer that already hosts other build runners. The STM32 runner uses its own directory/service and does not replace another runner.

Requirements:

- Keil MDK installed;
- ARM Compiler 5 available for this project;
- valid Keil license;
- administrator PowerShell for service installation.

From a checkout of this branch/repository, open **Administrator PowerShell**:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\ci\setup_stm32_keil_runner.ps1
```

If GitHub CLI `gh` is installed and authenticated as the repository owner/admin, the script obtains the short-lived registration token and enables:

`STM32_KEIL_CI_ENABLED=1`

automatically.

If `gh` is not available, obtain a registration token from:

Repository Settings -> Actions -> Runners -> New self-hosted runner

then run locally:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\ci\setup_stm32_keil_runner.ps1 -RegistrationToken "<token>"
```

Do not store the registration token in the repository.

## Triggering builds

After the Windows runner is online:

- a push runs the Keil build when repository variable `STM32_KEIL_CI_ENABLED=1`;
- Actions -> STM32F103 CI -> Run workflow can run it manually;
- pull requests run only the GitHub-hosted source gate. The self-hosted Windows job is intentionally not exposed to pull-request code.

## Useful local commands

Keil production build:

```powershell
.\tools\ci\keil_build.ps1
```

GitHub-hosted-compatible source check (when GNU Arm GCC is installed):

```bash
python3 tools/ci/stm32_source_check.py \
  --project "103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx" \
  --out build/ci/gcc-source-check
```

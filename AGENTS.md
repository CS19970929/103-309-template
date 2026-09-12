# Repository instructions for Codex

## Project facts

- MCU target: STM32F103RC (Cortex-M3).
- Existing production build: Keil uVision / ARMCC 5.06 update 7.
- Peripheral library: STM32F10x StdPeriph Library V3.5.0.
- The existing Keil project is the source of truth for the production image.
- The project links legacy ARMCC `.lib` files. GNU Arm GCC source compilation is therefore a compile gate, not proof that the final firmware can be linked identically with GNU ld.

## Engineering constraints

- Keep the existing Keil project usable.
- Do not replace StdPeriph with HAL.
- Do not modify vendor/StdPeriph code unless the task explicitly requires it.
- Do not modify or regenerate legacy `.lib` files unless explicitly requested.
- Prefer small, reviewable changes. Do not mass-format unrelated files.
- Preserve existing protocol and persistent-data compatibility unless the task explicitly changes them.

## Required verification

Before completing a code change, run the cloud-compatible source compile check when `arm-none-eabi-gcc` is available:

```bash
python3 tools/ci/stm32_source_check.py \
  --project "C030v1.0/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx" \
  --out build/ci/gcc-source-check
```

This must compile all C translation units without errors.

When a Keil ARMCC5 environment is available, also run the production build:

```powershell
tools/ci/keil_build.ps1 -Project "C030v1.0/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx"
```

Do not claim full firmware-build success unless the Keil ARMCC5 build has passed. If only the GNU Arm source compile check ran, state that explicitly.

## CI behavior

GitHub Actions runs the GNU Arm source compile check on pushes and pull requests. `cppcheck` is currently informational while a baseline is established.

The full Keil build is enabled only when the repository variable `KEIL_CI_ENABLED` is set to `1` and a Windows self-hosted runner with label `keil-armcc5` is online.

# Flash Layout Build Guard

## Purpose

This project now reserves the internal flash pages from `0x0801C000` upward for
persistent data and boot flags. Application code must stay below that address.

## Reserved Pages

The build guard reads these addresses directly from
[`Flash.h`](/E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Source/Flash.h):

- `FLASH_ADDR_STORAGE_AFE_SLOT_A`: `0x0801C000`
- `FLASH_ADDR_STORAGE_AFE_SLOT_B`: `0x0801C800`
- `FLASH_ADDR_STORAGE_LOG_SLOT_A`: `0x0801D000`
- `FLASH_ADDR_STORAGE_LOG_SLOT_B`: `0x0801D800`
- `FLASH_ADDR_SH367309_VALUE`: `0x0801E000`
- `FLASH_ADDR_SH367309_FLAG`: `0x0801E800`
- `FLASH_ADDR_UPDATE_FLAG`: `0x0801F800`
- `FLASH_ADDR_SLEEP_FLAG`: `0x0801FC00`

The earliest reserved page is `0x0801C000`, so the application ROM window is:

- app start: `0x08004800`
- app max size: `0x00017800`
- app end limit: `0x0801C000`

## Build-Time Protection

Two checks now protect the layout:

1. Keil `OCR_RVCT4` is limited to `0x00017800`, so normal code growth should hit
   a linker limit before touching reserved pages.
2. `After Build` runs
   [`check_flash_layout.ps1`](/E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Users/check_flash_layout.ps1),
   which parses the generated map file and fails the build if:
   - `ER_IROM1` max size is not aligned with the first reserved page
   - the linked application end address crosses into any reserved flash page

## What To Change Later

If the reserved flash layout changes, update the addresses in
[`Flash.h`](/E:/TODO/103%20+%20309%20-%20副本%20-%20副本/103%20+%20309/Project/Source/Flash.h).
The guard script reads the macros from that file, so it will automatically use
the new layout on the next build.

If application code really needs more ROM, do not just increase Keil IROM size.
Move or redesign the reserved flash layout first, then update both the storage
implementation and the build guard expectations.

# Persistent Storage Architecture

## 1. Design goal

Persistent storage is split into three responsibilities:

1. **Business state** — SOC, protection parameters, AFE parameters, event log, factory aging.
2. **Storage API** — `Storage.h`; the only interface new business code should depend on.
3. **MCU backend** — `Flash.c` / `Flash.h`; owns STM32F1 addresses, page geometry, erase/program, CRC, A/B and journal algorithms.

Business code must not know Flash addresses or page size.

```text
Business modules
    |
    | Storage_Load*/Storage_Save*
    v
Storage.h (portable public API)
    |
    v
Flash.c object backend
    |-- A/B object
    |-- journal pair
    |-- journal single page
    |-- CRC / sequence / read-back verify
    v
STM32F1 internal Flash
```

`StorageFlash_*` functions are retained only as a migration compatibility layer. New code should not add new calls to them.

## 2. Current object policy

| Object | Update frequency | Policy | Reason |
|---|---:|---|---|
| AFE parameters | very low | A/B | atomic configuration update |
| Protect + Other parameters | very low | A/B | atomic configuration update |
| SOC snapshot | medium/high | two-page journal | endurance + power-loss recovery |
| Event log | medium | two-page journal snapshot (legacy) | compatible with current data |
| Factory aging | high counter / low Flash | BKP + Flash journal | endurance |
| Sleep reason | high | BKP | no Flash wear |
| App -> IAP request | very low | SRAM mailbox | no Flash erase |
| Upgrade parameter version | very low | dedicated Flash page | one-shot migration marker |

The next log-format revision should change event logging from a 100-entry snapshot to append-only event records. It is intentionally not mixed into the first storage refactor because that changes on-Flash schema and requires migration testing.

## 3. STM32F103C8 capacity warning

The current persistent region is `0x0801C000..0x0801FFFF`, which requires a 128-KB addressable Flash array.

The Keil target is STM32F103C8, whose official Flash capacity is 64 KB. Existing hardware may expose a rear 64-KB region, but that is not an ST-guaranteed product contract.

The backend therefore reports one of:

- `STORAGE_STATE_READY` — reported physical Flash is at least 128 KB.
- `STORAGE_STATE_READY_UNVERIFIED_CAPACITY` — legacy C8 compatibility mode permits the rear 64 KB even though the device reports less than 128 KB.
- `STORAGE_STATE_UNSUPPORTED_FLASH` — persistence is blocked.

For production products that require guaranteed persistence, use a device with officially specified >=128 KB Flash and set `FLASH_STORAGE_ALLOW_UNVERIFIED_REAR64` to `0`.

## 4. On-Flash record contract

Existing records keep the same envelope so deployed data remains readable:

```text
magic
record version
payload length
sequence
CRC16
reserved
payload
```

Rules:

- A record is valid only when magic/version/length/CRC all match.
- The newest valid `sequence` wins.
- Writes are read back and compared before success is returned.
- A/B writes always update the non-current copy first.
- Journal writes append until a page is full, then rotate/erase the other page.
- CRC calculation accepts a 16-bit payload length; do not reintroduce the old 8-bit length truncation.

## 5. SOC endurance policy

Runtime SOC persistence is deliberately different from display update frequency.

Current policy:

- save after >=2% SOC delta from the last successful snapshot;
- do not save for every `cycle_x100` increment; standalone cycle delta threshold is 1.00 equivalent cycle;
- save immediately for capacity-base or rebound-hold flag changes;
- dirty data is forced within 30 minutes;
- reset-based sleep forces the latest dirty snapshot;
- manual SOC/capacity commands save immediately;
- the saved marker is updated only after a verified successful Flash write.

This bounds recovery error while reducing normal full-cycle Flash traffic from roughly 300 snapshots toward about 100 snapshots.

## 6. Adding a new persistent object

Do not allocate an address directly in a business module.

1. Define the portable payload in `Storage.h` if it is a cross-platform object.
2. Select persistence semantics: A/B, journal pair, BKP+Flash, or another explicit policy.
3. Allocate pages only in the backend layout (`Flash.h`).
4. Add a backend object descriptor in `Flash.c`.
5. Expose `Storage_LoadXxx` / `Storage_SaveXxx`.
6. Add payload validation in the owning business module.
7. Define schema migration before changing an existing payload size/layout.
8. Test power loss during erase, header write, payload write, and page rotation.

## 7. Porting to another MCU

When moving to another MCU/SoC, keep `Storage.h` and business payloads stable. Replace only the backend implementation.

The new backend must provide:

- initialization/capability detection;
- `Storage_IsReady` / `Storage_IsBusy`;
- load/save functions with equivalent atomicity guarantees;
- CRC/integrity validation;
- wear strategy appropriate for the NVM technology.

Examples:

- STM32F0/F1 internal Flash: A/B + journal.
- EFR32: NVM3 adapter.
- Telink: sector/page Flash journal adapter.
- external EEPROM/FRAM: object adapter without exposing bus details to business code.

## 8. Rules for future maintenance

- New business code must include `Storage.h`, not physical Flash addresses.
- Do not call `FLASH_ErasePage` / `FLASH_ProgramHalfWord` outside the backend.
- Do not use Flash for transient flags that fit BKP/SRAM.
- Do not persist on every scheduler tick; define a durability requirement first.
- A save API returns success only after integrity/read-back verification.
- Failed saves must not update the in-RAM "saved" marker.
- Flash schema changes require an explicit migration path.

# Persistent Storage Architecture

## 1. Design goal

Persistent storage is split into three responsibilities:

1. **Business state** — SOC, protection parameters, AFE parameters, calibration, product identity, event log and factory aging.
2. **Storage API** — `Storage.h`; the only persistent-storage interface business code should depend on.
3. **MCU backend** — `Flash.c` / `Flash.h` plus storage-specific backend code; owns STM32F1 addresses, page geometry, erase/program, CRC, A/B and journal algorithms.

Business code must not know Flash addresses or page size.

```text
Business modules
    |
    | Storage_*
    v
Storage.h (portable public API)
    |
    v
STM32F1 storage backend
    |-- A/B object
    |-- journal pair
    |-- event-log delta journal
    |-- CRC / sequence / read-back verify
    v
STM32F1 internal Flash
```

`StorageFlash_*` functions exist only inside the STM32F1 backend/legacy compatibility surface. New business code must use `Storage_*` APIs.

## 2. Current object policy

| Object | Update frequency | Policy | Reason |
|---|---:|---|---|
| AFE parameters | very low | A/B | atomic configuration update |
| Protect + Other parameters | very low | A/B | atomic configuration update |
| SOC snapshot | medium/high | two-page journal | endurance + power-loss recovery |
| Event log | medium | A/B base snapshot + alternating delta page | low write amplification + backward compatibility |
| Calibration K/B | very low | Factory Data A/B | production calibration must be atomic |
| Product ID / HW / SW version | very low | Factory Data A/B | identity survives reboot and failed writes do not change RAM |
| Factory aging | high counter / low Flash | BKP + Flash journal | endurance |
| Sleep reason | high | BKP | no Flash wear |
| App -> IAP request | very low | SRAM mailbox | no Flash erase |
| Upgrade parameter version | very low | dedicated Flash page | one-shot migration marker |

## 3. Physical page layout

The persistent area uses 16 x 1-KB pages:

```text
0x0801C000  Page 0   AFE A
0x0801C400  Page 1   Protect/Other A
0x0801C800  Page 2   AFE B
0x0801CC00  Page 3   Protect/Other B
0x0801D000  Page 4   Event snapshot A
0x0801D400  Page 5   Event delta A
0x0801D800  Page 6   Event snapshot B
0x0801DC00  Page 7   Event delta B
0x0801E000  Page 8   SOC journal A
0x0801E400  Page 9   Factory Data A
0x0801E800  Page 10  SOC journal B
0x0801EC00  Page 11  Factory Data B
0x0801F000  Page 12  Upgrade parameter version
0x0801F400  Page 13  Factory aging journal
0x0801F800  Page 14  Legacy update flag
0x0801FC00  Page 15  Legacy sleep flag
```

`Factory Data` is one A/B transaction envelope containing independently valid subobjects:

```text
format version
valid flags
47 x calibration K
47 x calibration B
32-byte BMS serial number
32-byte hardware version
32-byte software version
three stored lengths
```

Updating calibration preserves Product ID; updating Product ID preserves calibration. The complete Factory Data payload is committed as one A/B snapshot, so a power loss cannot leave a half-updated factory object.

## 4. Calibration transaction rules

There are 47 K/B pairs. Current accepted ranges are owned by the measurement/business layer:

```text
K: 512 .. 1536, default 1024
B: -30000 .. 30000, default 0
```

Boot behavior:

1. initialize RAM to default K/B;
2. load the Calibration subobject;
3. if it exists and all 47 pairs are valid, apply it;
4. if it is missing, persist the defaults as first-boot migration without reporting a storage fault;
5. if a stored object exists but contains invalid K/B data, report a storage fault and replace it with defaults.

Host write behavior is transactional:

1. a calibration write must contain exactly one K/B pair (two registers);
2. validate address, K and signed B before persistence;
3. build a complete candidate snapshot from the current 47 pairs;
4. save and read-back verify through `Storage_SaveCalibrationData()`;
5. update live RAM only after save success.

The reset command follows the same rule: persist all-default K/B first, then apply them to RAM. A Flash failure therefore leaves the previously active calibration unchanged.

## 5. Product identity transaction rules

BMS serial number, hardware version and software version each have a 32-byte field plus stored length.

At boot, `InitProID()` loads the Product ID subobject. If no valid record exists, compile-time defaults are loaded and initialized into Factory Data.

Host writes no longer modify `ProductionInfor` first. A candidate field is built and persisted with `Storage_SaveProductIdData()`; live identity is changed only after verified save success. Existing Modbus addresses and field byte layout are unchanged.

## 6. Event-log persistence

The customer-visible 100-entry event ring and the existing snapshot payload are unchanged.

Physical layout:

```text
Page 4  log snapshot A
Page 5  log delta A
Page 6  log snapshot B
Page 7  log delta B
```

Normal event persistence does **not** rewrite the 100-entry snapshot. One event is written as a small delta record containing:

```text
base snapshot sequence
event
time delta
```

The active delta page is selected from the base snapshot sequence, so page 5 and page 7 alternate across compactions.

When the active delta page fills:

1. load the newest valid A/B snapshot;
2. replay deltas that belong to that snapshot sequence;
3. append the new event in RAM;
4. write and verify a new full snapshot to the non-current A/B page;
5. only then erase the old delta page.

The `base snapshot sequence` field is the power-loss transaction boundary. If power fails after step 4 but before step 5, the new snapshot wins after reboot and the old deltas are ignored because their base sequence no longer matches. This prevents duplicate event replay without changing the legacy snapshot schema.

A 1-KB delta page holds about 42 current delta records. Compared with full-snapshot-on-every-event behavior, this cuts event-log erase frequency by roughly an order of magnitude while keeping every successfully returned event durable immediately.

The RAM event ring is updated only after `Storage_LogAppend()` succeeds. A failed Flash write therefore leaves the event latch retryable instead of making RAM newer than persistent storage.

## 7. STM32F103C8 capacity warning

The persistent region is `0x0801C000..0x0801FFFF`, which requires a 128-KB addressable Flash array.

The Keil target is STM32F103C8, whose official Flash capacity is 64 KB. Existing hardware may expose a rear 64-KB region, but that is not an ST-guaranteed product contract.

The backend therefore reports one of:

- `STORAGE_STATE_READY` — reported physical Flash is at least 128 KB.
- `STORAGE_STATE_READY_UNVERIFIED_CAPACITY` — legacy C8 compatibility mode permits the rear 64 KB even though the device reports less than 128 KB.
- `STORAGE_STATE_UNSUPPORTED_FLASH` — persistence is blocked.

For production products that require guaranteed persistence, use a device with officially specified >=128 KB Flash and set `FLASH_STORAGE_ALLOW_UNVERIFIED_REAR64` to `0`.

## 8. On-Flash record contract

Persistent objects use this envelope unless an explicitly documented legacy format applies:

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
- Journal writes append until a page is full, then rotate/erase according to the object's policy.
- CRC calculation accepts a 16-bit payload length.
- Cleanup erase after an already committed transaction must not determine logical success; stale data must be distinguishable by generation/sequence.
- Business RAM/state is committed only after the persistent transaction succeeds.

## 9. SOC endurance policy

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

## 10. Adding a new persistent object

Do not allocate an address directly in a business module.

1. Define the portable payload/API in `Storage.h` if it is a cross-platform object.
2. Select persistence semantics: A/B, journal, delta journal, BKP+Flash, or another explicit policy.
3. Allocate pages only in the backend layout (`Flash.h`).
4. Keep erase/program/CRC/sequence handling in the storage backend.
5. Expose `Storage_*` APIs to the business module.
6. Add payload validation in the owning business module.
7. Define schema migration before changing an existing payload size/layout.
8. Test power loss during erase, header write, payload write, compaction and cleanup.

## 11. Porting to another MCU

When moving to another MCU/SoC, keep `Storage.h` and business payloads stable. Replace only the backend implementation.

The new backend must provide:

- initialization/capability detection;
- `Storage_IsReady` / `Storage_IsBusy`;
- load/save/append functions with equivalent atomicity guarantees;
- CRC/integrity validation;
- wear strategy appropriate for the NVM technology.

Examples:

- STM32F0/F1 internal Flash: A/B + journal/delta journal.
- EFR32: NVM3 adapter.
- Telink: sector/page Flash journal adapter.
- external EEPROM/FRAM: object adapter without exposing bus details to business code.

## 12. Rules for future maintenance

- New business code must include `Storage.h`, not physical Flash addresses.
- Do not call `StorageFlash_*`, `FLASH_ErasePage` or `FLASH_ProgramHalfWord` from business modules.
- Do not use Flash for transient flags that fit BKP/SRAM.
- Do not persist on every scheduler tick; define a durability requirement first.
- A save/append API returns success only after integrity/read-back verification.
- Failed saves must not update in-RAM saved markers, event latches, calibration or production identity.
- Flash schema changes require an explicit migration path.

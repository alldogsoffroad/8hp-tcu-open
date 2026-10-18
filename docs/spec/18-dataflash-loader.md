# Spec 18 — Data-flash application loader (Teensy updates without the ACDP-2)

Status: **implemented and host-tested (2026-10-18); not yet run on hardware.** Markers: **[M]** manual (SH7254R HW
manual R01UH0480EJ0400 §26), **[O]** observed, **[B]** bench, **[I]** inference.

## 1. Why

- FWE is low whenever an application runs, so code flash cannot be programmed from our firmware
  (spec 16 §10). Writing code flash through the boot MAT's plant session needs its SecurityAccess
  algorithm, which this project does not pursue (spec 17).
- FWE protects **code flash only** [M §25.8.1]. The **data flash** (data MAT, 128 KB) can be
  programmed with FWE low; it needs on-chip ROM enabled, FRDY = 1, the FENTRYR key and the
  per-block enables EEPWE0/1, all set by software [M §26.3]. It is rated for **50 000** P/E cycles
  (code flash: 1 000) [M §32].
- Data flash cannot execute code (instruction fetch is blocked) [M §26.1].

So: a small permanent **loader** in code flash (installed once with the ACDP-2) keeps the
**application in data flash**, copies it to RAM at power-up and runs it there. The Teensy updates
the application through the loader, with our own protocol.

## 2. Data flash facts [M §26]

| Item | Value |
|---|---|
| Address (read and P/E) | `0x80100000`–`0x8011FFFF`, read over the P bus (3–7 Pφ per access) |
| Blocks | 16 × 8 KB, DB00–DB15 |
| Program | 8 bytes (`E8 04` + 4 words + `D0`) or 128 bytes (`E8 40` + 64 words + `D0`) |
| Erase | `20` `D0` on any address of the block |
| Blank check | `71` `D0` with FMODR.FRDMD = 1, range in EEPBCCNT, result in EEPBCSTAT |
| P/E mode | FENTRYR = `0xAA80` (FENTRYD); same FCU, FCU firmware and status handling as code flash |
| Read enable | EEPRE0 (DB00–07, key `0x2D`), EEPRE1 (DB08–15, key `0xD2`) |
| P/E enable | EEPWE0 (DB00–07, key `0x1E`), EEPWE1 (DB08–15, key `0xE1`) |
| Erased content | **undefined** (not `0xFF`); the ACDP-2 shows erased blocks as `MMSBLANK` |

The boot code writes EEPRE0/1 = `0x2D00`/`0xD200` (reads disabled) and EEPWE0/1 at start-up;
the loader sets the bits it needs.

## 3. Stock use of the data flash [O]

The stock application keeps its EEPROM records (spec 06 §6, `1034420267_eeprom_records.csv`) in
a **ring of four 32 KB slots**: DB00–03, DB04–07, DB08–11, DB12–15. Each slot holds one section
(header block + data block, about 12 KB) with header `40 01 00 7C`, length, sequence number and
the application CRC (`29B578DF`). Each new section goes to the next slot and the oldest slot is
erased:

| Read-out | DB00–03 | DB04–07 | DB08–11 | DB12–15 |
|---|---|---|---|---|
| 2026-09-24 | seq `3061` | seq `3062` | seq `3063` | erased |
| 2026-10-17 | erased | seq `3062` | seq `3063` | seq `3064` |

The Bosch boot software reads records too (`0x0111`, `0x0112`, identification records
`0x0F10`/`0F11`/`0F15`/`0F20` behind its `F1xx` identifiers). Our firmware never runs the stock
EEPROM manager, so the ring is frozen.

**Allocation:** the application uses **DB00–DB07 (64 KB)**; **DB08–DB15 stay untouched** (the two
newest stock sections, for the Bosch boot). The loader refuses any erase/program outside
DB00–DB07. The overwritten stock section (`3062`) is recoverable from the ACDP-2 EEPROM read-out.

## 4. Image format in data flash

| Offset | Content |
|---|---|
| `+0x00` | magic `"8HPAPP01"` |
| `+0x08` | image length (bytes, multiple of 128) |
| `+0x0C` | RAM load address (`0xFFF84100`) |
| `+0x10` | entry address |
| `+0x14` | CRC-32 (zlib) of the payload |
| `+0x18` | version string (16 bytes) |
| `+0x80` | payload: the application linked for RAM (code, rodata, initialised data) |

The **header block (first 128 bytes) is programmed last**; erasing DB00 first invalidates the old
image. So a power loss during an update leaves "no valid image" — the loader then waits for the
Teensy (§5) — never a half-written image that runs.

Size budget: payload ≤ 64 KB − 128 B. Today: ~23 KB code + initialised data (the RAM build drops
the code-flash flasher blob and the 32 KB boot-MAT dump buffer); bss ~4 KB, stack 4 KB. RAM
available from `0xFFF84100` to `0xFFFA0000`: ~112 KB.

## 5. Loader (code flash, installed once with the ACDP-2)

Keeps the stock identification fields (`build_image.py --stock-ident`), header and markers as
today. At start:

1. Watchdog, supervisor service (outputs off), CAN bus 1 at 500 kbit/s.
2. **Listen window**: for 100 ms, answer only the loader's own request to stay (`31 01 FE40`
   on `0x7E1`). Seen → stay in loader mode.
3. Otherwise check the image (magic, length, CRC over data flash). Valid → copy payload to RAM,
   jump to its entry with interrupts masked. Invalid → loader mode.
4. **Loader mode** (UDS on `0x7E1`/`0x7E9`, the spec 16 protocol with a data-flash back end):
   - `22 F1 95`: loader version, image status, FPMON/FSTATR;
   - `31 01 FF01 addr len`: CRC-32 of data flash;
   - `31 01 FF00 addr len`: erase whole blocks, DB00–DB07 only;
   - `34`/`36`/`37`: program in 128-byte units, read back;
   - `11 01`: reset (runs the image if valid).
5. The running application can also return to loader mode: routine FE30 (today's flasher entry)
   writes a "stay in loader" word to the boot-shared RAM record area and resets.

Supervisor, watchdog and CAN polling run in the loader exactly as in the RAM flasher; the FCU driver
(`fl_fcu.c`) gains the data-flash entry (`0xAA80`), 128-byte programming and EEPWE/EEPRE handling.

## 6. Host tool and tests

- `tcu_bench.py app-flash IMAGE`: stay-in-loader request during power-up (or FE30 from a running
  application), erase DB00–DB07, program payload, then header last; verify by CRC; reset.
- Simulator: the flash-controller model gains a data MAT (undefined erased content, 128-byte
  programming, block erase, EEPWE gating) and the power-cut sweep is rerun for the new write order.

## 7. Bench sequence

1. Build loader + RAM application; host tests pass.
2. Install the loader with the ACDP-2 (one write).
3. Power up: loader mode (no image yet); `22 F1 95`.
4. Data-flash self-test on DB07: erase, program, read back, erase.
5. Write the application; power-cycle; heartbeat `0x7F0` must come from the RAM application.
6. Power-cut test during an application write: loader must come up in loader mode.
7. ACDP-2 read of the TCU still works (recovery path unaffected).

## 7a. Implementation (2026-10-18)

| Part | Files |
|---|---|
| Loader (code flash, 9.8 KB code, RAM `0xFFF9C000`–`0xFFF9C2B0`, stack at the top) | `firmware/loader/ld_main.c`, `ld_image.c`, `ld_link.ld`; reuses `platform/sh7254r` start-up/init and `firmware/flasher` (`fl_core`, `fl_fcu`, `fl_sh7254r`) |
| Data-flash driver | `fl_fcu.c`: `fl_fcu_df_enable/erase/program` (FENTRYR `0xAA80`, 128-byte units, EEPRE/EEPWE keys) |
| Protocol | `fl_core.c` now runs over a target description: `fl_target_rom` (spec 16) and `fl_target_df` (ident `LD`, window `0x80100000`–`0x8010FFFF`, 128-byte page, no "erased = 0xFF" assumption) |
| RAM application | target `tcu_app_ram.elf` (`TCU_RAM_APP`, `platform/sh7254r/linker_ram.ld`): 22.6 KB payload, entry `0xFFF84900`, VBR `0xFFF84100`; FE30 hands back to the loader (stay word `LOAD` at `0xFFF800D0`, then the code-flash header entry) |
| Image | `tools/build_app_image.py` → `build/sh2a/tcu_app_df.bin` (built automatically); loader for the ACDP-2: `build_image.py --app build/sh2a/tcu_loader.hex --stock-ident` |
| Host tool | `tcu_bench.py app-flash [IMAGE]`, `app-selftest`; `tcu_flash.df_write` (DB00 erased first, payload, verify, header last) |
| Tests | simulator data MAT (undefined erased content, EEPWE/EEPRE gates, 8/128-byte units, no FWE); `test_flasher` data-flash cases (run with FWE low); `test_loader` (header checks, CRC); `test_tcu_flash` data-flash write + power cut during each of 181 operations: a valid header always means the complete new image, DB08–DB15 never change, a rewrite completes the image |

## 8. Open items

1. Whether the Bosch boot software depends on any record in DB00–DB07 (it reads the newest
   sections; DB08–15 are kept).
5. The stay word at `0xFFF800D0` (boot-preserved RAM between the boot request record and the
   boot MAT's bytes at `0xEC`–`0xF8`): no Bosch use found, but not proven unused. It is only
   written immediately before a jump to the loader, which clears it first thing.
2. Behaviour of EEPRE/EEPWE after the boot code's writes (re-enable with the keys).
3. The 100 ms listen window adds to start-up time in a vehicle; FE30 avoids needing it.
4. Execution speed from RAM versus code flash (expected equal or faster; measure heartbeat timing).

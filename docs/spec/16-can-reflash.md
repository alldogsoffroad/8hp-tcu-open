# Spec 16 — Project CAN reflash (application reflash over the project UDS session)

Status: **implemented and host-tested (2026-10-17); not usable on this unit — FWE is low at boot
hand-over (§10). Code-flash duty is handled by the boot MAT plant path (spec 17) and the
[8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher).** Markers: **[M]** manual (SH7254R HW manual
R01UH0480EJ0400 §25, §32), **[O]** observed, **[I]** inference, **[D]** derived.

## 1. Goal and boundaries

The project application carries its own flasher. The Teensy (or any SocketCAN tool) rewrites the
application and calibration blocks over CAN bus 1 with a plain UDS-style protocol. ~~The ACDP-2 is
needed once, to install the first project image, and stays the recovery path.~~ **Superseded
(2026-10-30): the Teensy flasher installs project images and is the recovery path on its own
(spec 17); no proprietary tool is needed.**

- **Writable window: `0x040000–0x1FFFFF` only** (application + calibration, spec 06 §1). The
  flasher refuses any address below `0x40000` or above `0x1FFFFF` (erase blocks EB00–EB10 hold block 0 /
  Auth_TSW, boot data, bootloader and boot manager).
- The user boot MAT is never selected (FENTRYD never set). Lock-bit commands (`0x71`/`0x77`) are
  never issued and FPROTR is left alone.
- So the Bosch power-up path (user boot MAT + bootloader, spec 06 §5b) is never modified. Whatever
  happens to the application, the boot-MAT power-up entry still works — and the Teensy flasher
  drives that entry directly (spec 17), so recovery needs no other tool.

## 2. Facts the design rests on

- **Stock boot software contains no P/E routines [O].**
  - The bootloader (`0x109F4`) and boot manager (`0x224D4`) only set FCURAME = `0xC401` and
    march-test FCU RAM `0x80FF8000` (`0x10C9C`, length `0x2000`, or `0x1800` on the other variant).
  - The boot manager's ROM→RAM copy table (`0x20138`) holds only initialised data.
  - [I] The flash driver is downloaded by the tester, which fits the encrypted ACDP-2 transfer.
    There is nothing to mirror; the flasher follows the manual's procedures.
- **No content check at power-on [O]** (bench item 12). An image with a valid header and markers
  starts.
- **Markers decide validity [O/I]** (spec 06 §3):
  - Application: marker 1 = `0x17FE00` (256 × `0x00`); marker 2 = `0x17FF00` (256 × `0xFF`).
  - Calibration: marker 1 = `0x180000`; marker 2 = `0x180100`.
- **Flash:**
  - Programming unit is 256 bytes. P/E address = `0x80800000` + ROM address [M].
  - FENTRYR = `0xAA01` (FENTRY0, `0x000000–0x0FFFFF`) or `0xAA02` (FENTRY1,
    `0x100000–0x1FFFFF`); `0xAA00` returns to ROM read mode [M].
  - ROM cannot be read while in P/E mode [M].
- **Erase blocks in the window [M]:**

  | ROM range | Block size | Blocks | Notes |
  |---|---|---|---|
  | `0x040000–0x09FFFF` | 64 KB | EB11–EB16 | header at `0x40000` |
  | `0x0A0000–0x17FFFF` | 128 KB | EB17–EB23 | EB23 = `0x160000` holds both application markers |
  | `0x180000–0x1FFFFF` | 128 KB | EB24–EB27 | EB24 = `0x180000` holds both calibration markers |

- **Timing (max, ≤100 cycles) [M]:**
  - 256 B program 12 ms (typical 2 ms).
  - 64 KB erase 875 ms; 128 KB erase 1.75 s (typical 0.4 / 0.8 s).
  - Wait budget = max × 1.1, as the manual's flow charts use.
- **Endurance: 1000 P/E cycles, timing guaranteed to 100 [M].** Each block is erased only when its
  content differs (§5).
- **FWE pin:** P/E mode can be entered only with FPMON bit 7 (FWE) = 1 [M].
  - [I] It is high, since the stock bootloader programs flash in place.
  - Checked on the bench before anything else (§8 step 1).

## 3. Architecture

- **Separate RAM-linked program.** The flasher is its own small C program (`firmware/flasher/`).
  - It is linked to RAM (`0xFFF98000–0xFFF9FFFF`, 32 KB including its stack) with `-nostdlib`, no
    libgcc and no references to the application, so it cannot call into ROM by construction. A
    link error replaces a run-time crash.
  - Its binary is embedded in the application image as a constant blob.
- **Entry from the application:** RoutineControl `31 01 FE30`, extended session.
  - Same interlocks as FE10: engine not running, and solenoid outputs forced off.
  - The application replies, then copies the blob to RAM, masks interrupts (SR.IMASK = 15), points
    VBR at a RAM vector table (NMI / errors → reset) and jumps.
  - From then on nothing in ROM is executed.
- **The flasher owns the hardware it needs, by polling:**
  - **RCAN-A mailboxes:** bus 1, `0x7E1` → `0x7E9`, bit timing left as the application set it, 0xAA
    padding; minimal ISO-TP with BS = 0 and STmin = 0.
  - **Internal WDT:** serviced at least every 10 ms (it resets after about 30 ms).
  - **External supervisor (RSPI-B 0x8E):** bit 2 toggled every 5 ms, bit 0 (output enable) = 0.
  - The busy-wait loops (program and erase) do all three, so a 1.9 s erase never starves the
    watchdog or CAN.
- **FCU set-up [M]:**
  1. FCURAME = `0xC401`.
  2. Copy FCU firmware `0x00402000–0x00403FFF` → `0x80FF8000–0x80FF9FFF`. [I] A CPU copy loop is
     used in place of the manual's DMAC; if the FCU rejects it (ILGLERR at the first command), use a
     DMAC transfer.
  3. Before each command: check FPMON.FWE.
  4. Enter P/E mode for the area's FENTRY bit, then issue `0x50` (status clear).

## 4. Protocol (UDS subset inside the flasher)

| Request | Meaning | Reply |
|---|---|---|
| `31 01 FE30` (application) | enter flasher | `71 01 FE30`, then the flasher runs |
| `3E 00` | keep alive | `7E 00` |
| `22 F1 95` | flasher identification | `62 F1 95 'F' 'L' ver FPMON FSTATR0 FSTATR1 FASTAT FPESTAT last-error` |
| `31 01 FF01 aaaaaaaa llllllll` | CRC-32 of a ROM range (read mode) | `71 01 FF01 cccccccc` |
| `31 01 FF00 aaaaaaaa llllllll` | erase; range must be whole erase blocks inside the window | `7F 31 78` every 1 s, then `71 01 FF00` |
| `34 00 44 aaaaaaaa llllllll` | open a download; address 256-aligned, inside the window, all target pages blank | `74 20 01 02` (blocks of 2 + 256 bytes) |
| `36 nn <256 B>` | one page; blank-checked, programmed and read back before the reply. A page of all `0xFF` is only blank-checked. Repeating the previous counter is acknowledged without programming. | `76 nn` |
| `37` | close the download | `77` |
| `11 01` | reset | `51 01`, then reset |

Negative responses:
- `0x13`: wrong length, or a page beyond the opened download.
- `0x31`: address or length outside the window, or misaligned.
- `0x70`: page not blank.
- `0x72`: FCU error. The reply carries FSTATR0/1 and FASTAT; the FCU is then reset via FRESETR.
- `0x73`: wrong block counter.
- `0x24`: sequence error.

There is no time-out back to the application: an idle flasher stays in its loop until `11 01` or
power-off.

## 5. Write order and power-loss behaviour

Host tool (`tcu_bench.py flash <image>`):

1. **Compare.** For every erase block in the window, compare the image's CRC-32 with `FF01` and keep
   only the blocks that differ. Nothing to do → stop (no wear).
2. **Invalidate.** If any application block differs, erase **EB23** (`0x160000`) first. This clears
   marker 1. Same for calibration with **EB24**.
3. **Write.** Erase and program the other differing blocks; verify each block with `FF01`.
4. **Commit.**
   - Program EB23 (EB24) except marker 1's page, then verify the whole block range.
   - Program **marker 1 last** (`0x17FE00` / `0x180000` = 256 × `0x00`); marker 2 stays erased.
5. **Reset** with `11 01`.

| Power lost during | State at next power-up | Recovery |
|---|---|---|
| step 1 | unchanged | rerun |
| steps 2–4, before marker 1 | marker 1 erased → [I] boot rejects the block and stays in the Bosch bootloader | ACDP-2 write (power-up path); our flasher is not available |
| after marker 1 | new image valid | — |

[I] The invalid-application case relies on the Bosch path accepting the ACDP-2 power-up entry with
no valid application. This is checked deliberately once (§8 step 4) before the flasher is used for
anything that matters.

## 6. Interlocks and limits

- **Entry:** extended session, engine not running (no engine-speed / ignition-run signal on CAN, as
  for FE10). Outputs off.
- **Bench version:** no SecurityAccess. A seed/key is added before any in-vehicle use (open item).
- **Address window:** checked in the flasher itself, not only in the host tool, on erase, download
  and every page.
- **Erase:** only whole blocks; a range touching EB00–EB10 or above `0x1FFFFF` is refused before
  P/E mode is entered.

## 7. Tests (host, before any hardware use)

- The flasher's protocol, window checks and write sequencing are built for the host against a
  simulated FCU.
- The simulated FCU is a 2 MB array with real flash rules: program only onto blank pages, erase per
  block, P/E mode required, ROM unreadable in P/E mode, injected FRDY time-outs and PRGERR/ERSERR.
- Cases:
  - boot-area refusal;
  - misalignment;
  - marker ordering;
  - power loss injected after every step, checked against §5 (marker 1 never valid with
    incomplete data);
  - CRC compare skips identical blocks.
- Host tool planning is tested against the same simulator through a fake CAN link.

## 8. Bench sequence

> Historical plan (2026-10-17), written while the ACDP-2 was still the installer/recovery tool
> and before §10's FWE verdict closed this path. Never executed. The roles it assigns to the
> ACDP-2 now belong to the Teensy flasher ([8hp-teensy-flasher](
> https://github.com/alldogsoffroad/8hp-teensy-flasher), spec 17 path).

1. Install the project image with the ACDP-2 (with `FD03`, a read-only DID reporting FPMON).
   Confirm FWE = 1. If FWE = 0, stop: plan #3 is not possible without hardware changes.
2. Enter the flasher and read `F195`. Run `FF01` CRCs and compare them with the image (read path
   only, no P/E).
3. Erase and program one **unused** 128 KB block inside the application area (blank in our image,
   e.g. EB20 `0x100000`); verify, then erase it again. The markers are untouched, so the application
   stays valid throughout.
4. **Safety-net check:**
   - Erase EB23 on purpose (application now invalid) and power-cycle.
   - Confirm the TCU stays silent and the ACDP-2 can still write.
   - Restore with the ACDP-2.
5. First real reflash with the Teensy (a one-line version-string change), then routine use.

## 9. Implementation (2026-10-17)

| Part | Files |
|---|---|
| Flasher program (RAM `0xFFF90000`, about 5 KB + 2.6 KB RAM) | `firmware/flasher/`: `fl_core.c` (ISO-TP, services, window), `fl_fcu.c` (FCU driver), `fl_sh7254r.c` (CAN / WDT / supervisor / CMT polling), `fl_entry.c`, `fl_start.S`, `fl_link.ld` |
| Embedding and entry | `platform/sh7254r/fl_blob.S` (`.incbin`), `plat_flasher_start()` + register-only copy `plat_jump_ram` (`start.S`), `app/flash_entry.c` (FE30, FD03) |
| Host tool | `tools/bench/tcu_flash.py` (image check, plan, write order), `tcu_bench.py flash / flash-selftest / flash-invalidate`; the UDS client now sends multi-frame requests |
| Tests | `tests/unit/fl_sim.c` (flash-controller model with the manual's rules), `test_flasher.c`, `tcu_flasher_sim` + `tools/bench/test_tcu_flash.py` (Python tool against the C flasher; full write, no-op rerun, power cut at 40+ points with validity check and retry) |

Implementation notes:
- **Bus:** the flasher listens on bus 1 only, one receive mailbox for `0x7E1` and transmit mailboxes 1–3.
  Pending application transmissions are cancelled (TXCR) before the mailboxes are reconfigured.
- **ISO-TP:** our flow control is BS = 0, STmin = 0. CAN frames on the bus are at least about 230 µs
  apart, and the polling loop takes microseconds.
- **Exceptions:** NMI or any exception goes to a RAM vector table. It runs an FCU reset (FRESET
  held for about 1 ms), then a watchdog reset.
- **Interrupted operations:** a program/erase time-out triggers an FCU reset. ILGLERR, PRGERR or
  ERSERR are handled with FASTAT = `0x10` plus status clear (figure 25.16). FCUERR or an FCU RAM
  ECC error causes a reset and a firmware reload before the next command.
- **Link:** the flasher links without libgcc and libc, and its literal pool holds only register,
  FCU and RAM addresses (checked after the build).

## 10. Open items

1. **FWE level: LOW with the application running on the bench supply** (2026-10-17, `22 FD 03` →
   `00`). P/E mode cannot be entered, so the flasher cannot write as things stand. No code in the
   dumped flash reads FPMON; the flash driver is downloaded by the tool. [I] FWE is raised by
   hardware the tool's driver controls (a port pin or the system chip) or by the ACDP-2 harness.
   User boot MAT dumped (FE20, 2026-10-17; private, not in the repository). It drives PC12 = PC13
   = 1, PC15 = 0 at reset (PC14 read back) — **tested: FWE stays low with these lines driven** (m4d,
   FE31). Ignition (pin 9) was already on. In its programming loop (state `0x19`, after the RAM
   test) it sends the supervisor word `0x8E80 | (bit 4 toggling)` instead of the normal
   `0x8E00 | (bit 2 toggling)`. **Tested (m4e): FWE stays low in that mode too.** The same
   supervisor routine is shared by the boot MAT, bootloader (`0x1D0B4`) and boot manager
   (`0x30E40`); states `0xE8` normal, `0x06` (`0x8E40`), `0x19` (bit 7, bit 4 toggling), `0x31`
   (bit 4 toggling).
   Manual table 3.2: FWE is a mode pin; normal operation is MCU single-chip mode (MD = 000, FWE =
   0), in which the user boot MAT does not run; raising the FWE pin at runtime enters user program
   mode. So a board signal must drive FWE. m4f adds routine FE32 (arbitrary supervisor word
   pattern, reply read-back) to test supervisor commands interactively.
   **Boot hand-over (bench 2026-10-18):** the data-flash loader latches FPMON in
   `entry_snapshot()`, the first statement of `plat_init()`, and reports it as flasher ident byte
   12 (commit 28bc6c2, loader reflashed with the ACDP-2). Result: **FPMON `0x00`, FWE low at the
   loader's first instruction**; `start.S` before it touches no port, supervisor or flash
   register. So the Bosch boot drops FWE before handing over to any application; our own port
   set-up is not the cause, and leaving pins as the boot left them cannot keep FWE high. With the
   connector check (spec 01 §2.9: no case pin controls FWE), the route "raise or hold FWE from our
   code" is closed. FWE is high only inside the boot MAT window (spec 17), which is gated by
   SecurityAccess. ~~Code-flash writes therefore stay with the ACDP-2; firmware updates use the
   data-flash loader (spec 18).~~ (Void — see the 2026-10-30 supersession note at the end of
   this item.)
   **Bench 2026-10-26 (plant-session engine, spec 17 §5k): FWE is low for the plant session's
   entire life** — FPMON bit 7 = 0 at ~4 min after arming and across 12 samples spanning the
   first 35 s of a second fresh session. The boot MAT's supervisor service (0x244C) never fires
   its body in this session: its 5 ms gate reads the FRC at 0xFFFFF220, **which does not run in
   this context** (state 0x19, port 0xFFFE3902 = 0x7F with bits 4–5 set, 120000-fire counter
   still 0 after minutes — and bits 4/5 set exclude that port as the FWE gate). No timer we can
   find runs there (CMT0 and FRC both dead — any flasher timing loop would hang). Forcing P/E
   entry anyway (engine-side skip of the FPMON check) is **refused by the FCU itself**: the
   FENTRYR 0xAA0x write does not stick, synchronous failure. FWE is therefore a hard hardware
   gate on user-MAT program/erase, and **the armed plant session can read but never write code
   flash**. The ACDP-2's code-flash writes must happen in an earlier window (its D7656915
   500 kbit/s responder answers in 350 µs — plausibly while FWE is still at its reset level).
   **Superseded (bench 2026-10-30): the plant path writes code flash after all — the verdict
   above was measured on our own minimal plant session.** The full plant flow downloads the
   vendor's *session engine* (the "run hook", spec 17 §5e) which then serves an encrypted
   500 kbit/s session; in that session FWE is latched (high from at latest the session's
   attestation exchange onward) and the engine's flash-op grammar — erase / ARM / program /
   verify, CRC-16/CCITT-FALSE framed — writes code flash. Proven end-to-end by the Teensy alone
   with no proprietary tool: [8hp-teensy-flasher](
   https://github.com/alldogsoffroad/8hp-teensy-flasher) (full write-up in spec 17 §5l). "Code-flash writes therefore stay with the ACDP-2" above is void: nothing
   stays with the ACDP-2.
2. Whether a CPU copy of the FCU firmware is accepted (§3).
3. Whether the user boot MAT also checks the calibration markers before starting the application
   (affects only which block must be valid; the order in §5 covers both).
4. Supervisor time-out (spec 03 §4.4). The flasher services it anyway.
5. SecurityAccess for in-vehicle use.
6. Lock bits: if Bosch locked the application blocks, erase fails with ERSERR and FPESTAT `0x11`
   (reported by `22 F1 95`). Overriding them (FPROTR) is deliberately not implemented; decide if
   it happens.

## 11. The FENTRY1 split and the 2026-10-31 full-stock restore [B]

The round-55 "silent TCU after pristine restore" had a second cause beyond the data flash:
**the ACDP-2 loader install (2026-10-17) had erased the whole application body, 0x60000–0x160000**,
rewriting only its own first 128 KB plus the marker islands (0x17FBFC end word, 0x17FE00 marker 1).
Every marker/validity probe therefore kept passing while the app CRC range (0x40080–0x17FB6F,
spec 06 §2) was mostly blank — the boot manager refused to start the application, and the TCU sat
silent at ~0.163 A. Found by engine op-01 default-view pointer reads against the pristine ELF.

Restoring the body surfaced the **1 MB MAT split** (already implied by §3's FENTRYR table, now
bench-proven): user MAT 0 is programmed under `FENTRYR = 0xAA01` (FENTRY0, addresses 0x0–0xFFFFF,
P/E window 0x80800000–0x808FFFFF) and **user MAT 1 only under `FENTRYR = 0xAA02` (FENTRY1,
0x100000–0x1FFFFF, window 0x80900000–0x809FFFFF)**. With key 1 at ≥0x100000 the FCU rejects
program *and* erase as illegal (engine error class 2 / 0x22, sub 0x32) from a clean boot — not
lock bits (FPROTR's FPROTCN read 1 = protection cancelled), not FCU degradation. The driver fix
is one byte: the ARM's key field = 2 for offsets ≥ 0x100000.

Also observed: after ~2560 consecutive program units the FCU latches an ILGL state that survives
engine ARM cycles; a power cycle clears it (resume the remaining units in a fresh session). And
the engine's op-13 staged-read path stalls deterministically at the ~18th read of a session —
verify with spot reads across fresh handshakes instead.

Result (2026-10-31): code flash 0x40000–0x160000 pristine + markers committed + the data-flash
ring restored (spec 18 §8c) → **the stock application boots and runs**: solenoid drive from
power-up (1.73 A transient, 1.79 A steady vs 0.166 A for our outputs-off firmware) and the full
bus-1 periodic set live (0x037/0x2F9, 0x144, 0x170, 0x1A0, 0x2A8, 0x178, 0x3D1) — matching the
2026-10-17 stock capture. Driver: `/tmp/restore_body.py` pattern (blank-region program, no
erases; resume mode; ~5 units/s ⇒ 0x100000 bytes ≈ 11 min).

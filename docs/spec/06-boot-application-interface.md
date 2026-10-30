# Spec 06 — Boot-to-application interface

Status: **draft.** The block layout, headers, CRCs, validity markers and the application entry
contract are established. The very first reset stage (selection between bootloader and
application) runs from the 32 KB user boot MAT, which is **not in the available dump**, so the
exact start conditions are partly inferred.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Context: the project writes the controller with its own Teensy 4.1 flasher
([8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher)) through the stock
Bosch plant software (spec 17), so the Bosch boot blocks stay in place and the project replaces
the **application** and **calibration** blocks only. (§5b's bench observations below were
recorded with the proprietary ACDP-2 that the work started around; it is retired from the
toolchain.)

## 1. Flash partitioning

| Range | Content | Header |
|---|---|---|
| user boot MAT (32 KB, separate array) | reset entry, boot selection [I] | not dumped |
| `0x000000–0x00FFFF` | boot data: CAN profiles for the bootloader, marker list, block descriptor at `0x6000` | `0x4000`, `0x6000` |
| `0x010000–0x01FFFF` | **bootloader** (diagnostic/flash session, block ID `bhjd0701`) | `0x10000` |
| `0x020000–0x03FFFF` | **boot manager / flash services** (block ID `bjix2201`); protection record at `0x3D12C` (`bhlc0505`) | `0x20000`, `0x3D12C` |
| `0x040000–0x17FFFF` | **application** (block ID `bhjc2222`) — replaced by the project | `0x40000` |
| `0x180000–0x1FFFFF` | **calibration** (block ID `bhjc2222`) — replaced by the project | `0x180200` |
| `0x200000–0x27FFFF` | not used (constant fill pattern) | — |

The boot side describes these blocks in a logical block table (0x44-byte entries at `0x60B4`,
served to the bootloader by a "get block info" function) [O]:

| Block | Full range | Checked/erased range | Marker 1 | Marker 2 |
|---|---|---|---|---|
| Application | `0x040000–0x17FFFF` | `0x040000–0x17FDFF` | `0x17FE00` (256 B) | `0x17FF00` (256 B) |
| Calibration | `0x180000–0x1FFFFF` | `0x180200–0x1FFFFF` | `0x180000` (256 B) | `0x180100` (256 B) |

(Two further entries describe ranges inside the calibration area that carry no markers in this
calibration [O].)

## 2. Block header format (`0x876543xx` headers)

Used by the application (`0x40000`), calibration (`0x180200`), boot manager (`0x20000`) and boot
descriptor (`0x6000`) headers [O]. All values big-endian 32-bit unless noted.

| Offset | Application value | Meaning |
|---|---|---|
| +0x00 | `0x876543C1` | header marker; low byte = block type (`30` boot, `31` protection, `C1` application, `83` calibration) |
| +0x08 | `0x00180200` | address of the next header in the chain (application → calibration; 0 = end) |
| +0x0C | `0x0017FBFC` | [I] end of the header-described area |
| +0x10 | `0x00040054` | pointer to the marker list (§3) |
| +0x18 | `0x0B00` + `"TYPTEILENR"` + `"bhjc2222"` | identification string (part-number type and block ID) |
| +0x38 | `0x00040080` | **CRC range start** |
| +0x3C | `0x0017FB6F` | **CRC range end** (inclusive) |
| +0x44 | `0x29B578DF` | **CRC-32 of the range** [O][D] |
| +0x54 … +0x60 | `0x17FE00, 0x100, 0x17FF00, 0x100` | marker list (address, size) ×2 |
| +0x74 | `0x0017FA70` | [I] application-defined pointer |
| +0x7C | `0x00066CA8` | **application entry point** (§4) |
| +0x80 … | handler addresses | **exception/interrupt vector table**, vector 4 onward (§4) |

**CRC algorithm** [D]: standard CRC-32 (IEEE 802.3 polynomial 0x04C11DB7, reflected, initial
value 0xFFFFFFFF, final XOR 0xFFFFFFFF — identical to zlib `crc32`) over the bytes from +0x38 to
+0x3C inclusive. Verified on all four headers:

| Header | Range | Stored CRC |
|---|---|---|
| `0x6000` | `0x6054–0x7C6F` | `0xA29A8493` |
| `0x20000` | `0x20054–0x3F9FF` | `0xD01E59F6` |
| `0x40000` (application) | `0x40080–0x17FB6F` | `0x29B578DF` |
| `0x180200` (calibration) | `0x190000–0x1FFD5F` | `0x33569BA2` |

The calibration header has the same layout with its own range (`0x190000–0x1FFD5F`) and markers
(`0x180000`, `0x180100`).

**Header check in the boot manager** [O] (`0x24FD0`): a header is accepted only if the word at
+0x00 has the form `0x876543xx`, the end pointer at **+0x0C** lies inside `0x6000–0x1FFFFF` and is
4-byte aligned, and the word at that end address is **`0x12345678`**. Every stock block carries
this end word (application `0x17FBFC`, calibration `0x1FFDEC`, boot blocks `0x7CFC`, `0x3FDFC`);
it lies outside the CRC range. The application tail before it (`0x17FB70`) holds an
identification string (`0JIXC224` in the stock image), and header +0x74 points to a table of
calibration pointers that is not referenced by the boot side [O].

## 3. Validity markers

Each replaceable block has two 256-byte marker areas listed in its header and in the boot's block
table [O]. State in the stock image:

| Block | Marker 1 | Marker 2 |
|---|---|---|
| Application | `0x17FE00`: 256 × `0x00` (programmed) | `0x17FF00`: 256 × `0xFF` (erased) |
| Calibration | `0x180000`: 256 × `0x00` | `0x180100`: 256 × `0xFF` |

[I] Marker 1 programmed = "block completely programmed and verified"; marker 2 erased = "block
not invalidated". The bootloader programs marker 1 last after a successful download; writing
marker 2 is the usual way to invalidate a block (for example to request reprogramming). A new
image must carry markers in this state, unless the ACDP-2 writes them itself (to be confirmed in
its documentation).

The application keeps its own copy of the block list (header CRC descriptor, marker 1, marker 2)
for each of its two blocks [O] — [I] for a run-time self-check and/or self-invalidation.

## 4. Application entry contract

The boot side starts the application at the address stored at header **+0x7C** [O][I]. The stock
entry routine shows what the application must do itself [O]:

1. Raise the interrupt mask to 15 (all maskable interrupts off).
2. Set **VBR = address of the vector table − 0x10**; the stock image uses VBR = `0x40070`, so
   vector *n* is at `VBR + 4n` and vectors 4 and up are the table at header +0x80. The vector
   table therefore lies inside the CRC range.
3. Initialise its RAM, then peripherals and modules in a fixed order (spec 05 §4).
4. Lower the interrupt mask, enter the main scheduling loops; never return.

The stock entry routine **does not set the stack pointer** [O] → [I] the boot side hands over with
a valid stack. A new application should set its own stack pointer immediately on entry anyway.

## 5. RAM shared with the boot software

`0xFFF84000–0xFFF8403F` is a handover area written by the bootloader and boot manager (session
information; a flag byte at `0xFFF8403E` is cleared when the boot manager restarts) and read by
the protection block [O]. The stock application never accesses it directly [O]. [I] A new
application should **not** use this area, so that requests passed between boot and application
across a reset survive. The full extent of boot-reserved RAM is **open**.

### 5a. Boot request record at `0xFFF80080` (application ↔ boot, across a reset)

A 0x48-byte record that lets the application hand a diagnostic request to the boot software
(and back) across a software reset [O]. The boot manager start-up (`0x22444`) runs
`0x2242C` over `0xFFF80000–0xFFF800FF` and `0xFFF84000–0xFFF840FF`; that routine **reads each word
and writes it back** (RAM error-check initialisation), it does **not** clear them [O] — so the
record survives the reset. Boot software runtime RAM starts at `0xFFF84100` [O].

| Offset | Size | Content [O] |
|---|---|---|
| +0x00 | 2 | `0xA55A` |
| +0x02 | 2 | `0x0048` (record length) |
| +0x04 | 1 | source: 3 written by the application (`0x73FC4`) and by the bootloader (`0x11E34`); the boot manager accepts 2 or 3 |
| +0x05 | 1 | target: **1** = bootloader, **2** = boot manager, **3** = application |
| +0x08–0x18 | 4 each | diagnostic timing values from the active session's entry; bench (stock, after a diagnostic connection): `+0x08` 0, `+0x0C` 40 000, `+0x10` 5 000 000, `+0x14` 5 000, `+0x18` 500 000 (spec 15 §4a); all zero before any diagnostic request |
| +0x24 | 1 | `'w'` (0x77) = request active |
| +0x25 | 1 | session-change flag (0x6C8C8: `0xFFF93B4E`) |
| +0x26 | 1 | `0xFFF93B4D` (0x6C8B8) |
| +0x2B | 1 | requested session index (`0xFFF9407C`), written when target = 1; also from 0x6C878 (`0xFFF93B4C`) |
| +0x2C | 1 | communication-control state (`0xFFF93B4A`) |
| +0x2E | 1 | `0xFFF924D4` |
| +0x30 | 1 | session index to restore on return to the application |
| +0x32, +0x33 | 1 | session status bytes |
| +0x35 | 1 | 1 = a complete positive response is stored below; 0 = only the request is stored |
| +0x36 | 1 | response length + 1 |
| +0x37 | 1 | response SID (`SID \| 0x40` when +0x35 = 1, else the request SID) |
| +0x38 | 1 | sub-function |
| +0x39… | ≤ 8 | response data (session parameter record: P2 in ms, P2* in 10 ms, big-endian) |
| +0x44 | 2 | `0x5AA5` |

Readers [O]:
- Boot manager `0x2264A`: if +0x00/+0x02/+0x44 are valid, +0x04 is 2 or 3 and +0x05 = 2, it
  invalidates the header; `0x2362C` tests +0x05 = 2 and +0x24 = `'w'`.
- Bootloader `0x11EA6`: +0x05 = 1 and +0x24 = `'w'` → stay in the bootloader and answer the
  stored request. Going back, `0x11E34` writes source 3 and target 1 or 2 (by +0x2B), `0x11EBE` clears.
- Application `0x7406A`: +0x05 = 3 and +0x24 = `'w'` → at start-up (`0x7FB94`, `0x80618`) send the
  pending response, then clear the record (`0x74084`: header 0, source/target 0xFF, +0x24 = 0, +0x44 = 0).

**Stock application path for DiagnosticSessionControl (0x10)** [O]:
1. Service table entry at `0x4553C`: SID 0x10, handler `0x80A10`, confirmation `0x80C5C`,
   session configuration `0x46660`.
2. Session list `0x46927` (7 entries, index → session): 0 = `0x01`, 1 = `0x60`, 2 = `0x61`,
   3 = `0x6B`, 4 = `0x02`, 5 = `0x03`, 6 = `0x66`. Handlers `0x4660C`; timing pairs `0x46628`
   (0x01: 40 ms / 2 s; 0x60, 0x61, 0x6B: 50 ms / 5 s; 0x02, 0x03, 0x66: 40 ms / 2 s; units µs).
3. Sessions 0x61, 0x6B and 0x02 (handlers `0x75EC6`, `0x75F38`, `0x75EFC`) check preconditions
   (a condition bitmask; for 0x02 the mask comes from a variant-indexed table) and NRC 0x22 if
   they fail, then run the state machine `0x76022` (state byte `0xFFF84A06`):
   - state 0 → start shutdown preparation (`0x728F6`); answer 0x78 (response pending) until done;
   - state 2 → prepare outputs (`0x14863A`, `0x73F76(0)`), return code 0x18 → `0x75E9C` stores the
     request (+0x35 = 0) and `0x734FE` sets `'w'` and calls the header writer `0x73FC4` through the
     diagnostic configuration (`0x45474` +0x1C);
   - return code 0x17 (other path) → `0x75E2A` stores a full positive response (+0x35 = 1).
4. `0x73FC4` sets target = 1 (bootloader) when the requested session index is 2 (`0x61`), or 3
   (`0x6B`) with a configuration flag set; otherwise target = 2 (boot manager), which is the case
   for the programming session `0x02`.
5. The controller is then reset. [I] The reset is the stock reset routine `0x6865C` (reason code
   in r0, logged in `0xFFF80D24…`, then a WDT quick reset by `0x966B8`: WRCR = `0x5A80`,
   WTCR = `0xA563`); the exact call site after the record write was not traced.

**Power-on check of the record** [O]: the boot data block descriptor (`0x6078`) offers the user
boot MAT a table of entry points into the boot manager: `0x24A0C`, `0x24A26`, `0x24A24` (empty),
`0x24B10`, `0x24B26`, `0x624C`, `0x625A`, `0x631A`, `0x62F6`. [I] `0x24A26` is the start-up
request check:
- record header `0xA55A`, length `0x48` and trailer `0x5AA5` valid, target **2** → (source 3:
  `0x25416` over the hand-over area) set `0xFFF8403E` = 1 and run the **boot manager** main loop
  (`0x24974`), which takes over the stored request through its own copy of the diagnostic session
  layer (`0x25680`, `0x2558A`; the same Bosch library as the application). `'w'` is not tested here;
- valid header, target **1** → bootloader entry (pointer `0x24B38`);
- a second record form at `0xFFF80000` (length `0x14`, trailer at `+0x12`) → a separate start
  path (`0x24B70`), meaning open;
- otherwise `0x24144(0x2F0)` and return → [I] the user boot MAT goes on to start the application.

`0x24A0C` copies a 0x34-byte block into the hand-over area, clears `0xFFF8403E` and enters the
boot manager: [I] the path used when the user boot MAT does not start the application.
No code was found that listens on CAN at power-up **before** the application starts, without a
record. [I] So with an application that does not hand over, the Bosch boot software is
reachable only if the user boot MAT rejects the application (markers / CRC / header).

[I] **Project application (M1):** to hand over to the Bosch boot side for reprogramming, write the
record exactly as `0x73FC4`/`0x75E9C` do for session `0x02` (target 2, request stored, +0x35 = 0)
and reset through the WDT. The project linker keeps `0xFFF80000–0xFFF840FF` unused, so the record
area is never touched by the application except for this write. To be confirmed on the bench
spare before a vehicle unit relies on it.

### 5b. ACDP-2 (module 19) read operations — bench observations (2026-10-17)

Captured passively on bus 1 (TCU pins 5/6; the module 19 harness uses only this pair) with a
listen-only adapter. A normal (acknowledging, error-active) node on the bus made the flash read
fail; sniffing must be listen-only.

- **Identification:** plain UDS on `0x7E1`/`0x7E9` at 500 kbit/s while the application runs:
  `3E 80` ×3, `3E 00`, `22 F1 90` (VIN). The harness powers the TCU only for an operation.
- **Flash, maps and EEPROM read:** the ACDP-2 powers the TCU and, within milliseconds, sends the
  fixed 4-byte frame **`7E1#D7 65 69 15`**. The TCU answers on `0x7E9` within 0.4 ms with a
  4-byte value that differs every time (`B1A461B7`, `8C390217`, `02AE1C9F` …), then the transfer
  runs on `0x7E1`/`0x7E9` (DLC 8, 500 kbit/s) with **every frame encrypted** (all 256 first-byte
  values; not one repeated frame, even over erased flash areas). The application never starts
  (no periodic messages). Volumes from the TCU: flash 302 914 frames (≈ 2.42 MB), maps 65 568
  frames (512 KB + 256 B), EEPROM 17 603 frames (≈ 141 KB).
- [I] So there is a **CAN entry at power-up that does not depend on the application**, served by
  boot code (boot blocks or the user boot MAT), using a challenge (4-byte random value) and an
  encrypted session. If writing uses the same entry, a unit whose application does not hand over
  (§5a) is still reachable by the ACDP-2 — to be confirmed with a write capture.
- **"Full backup data":** an 8 KB block followed by the flash and EEPROM transfers on the bus;
  the saved `_Full.bin` (2 818 064 bytes) is an encrypted container (no plain image inside). It does
  not contain a readable user boot MAT.
- **Flash write of an identical image** (three captures): wake-up frame and challenge, an 8 KB
  block from the TCU (0.4 s), ~50–130 s of file preparation on the ACDP-2 with no bus traffic, then
  the "writing" progress bar: 62 request/response pairs, one every ~230 ms, 496 bytes each way.
  No image data was transferred. [I] The ACDP-2 asks the boot code for a checksum per flash region
  (≈ 230 ms each to compute), compares it with the file (the 8 KB block is probably the reference
  table) and rewrites only regions that differ; with an identical image nothing is written and
  the tool still reports success. A real write capture needs an image that differs.
- Captures were made with a listen-only adapter accepting 11- and 29-bit identifiers; no
  extended frames and no bit-rate change were seen.
- Flash and maps read on 2026-10-17 are byte-identical to the September read-outs (same unit).

## 6. Links that bind the application to other data

- **EEPROM:** the application CRC (`0x29B578DF`) and the calibration CRC (`0x33569BA2`) are stored
  in each EEPROM section header and in EEPROM block `0x0112` [O]. [I] The software compares them at
  start-up to detect an application/calibration change; the reaction (EEPROM re-initialisation,
  learned-value reset, fault) is **open**.
- **Protection record:** the 32-byte value in the protection record at `0x3D12C` also appears
  inside the application at `0x412E0` [O]. The bootloader contains a verification state machine
  that hashes address ranges through services at `0x200` and compares the result with stored
  references [O]. [I] This is a cryptographic integrity check (`TPROT`). Whether a changed
  application passes it — or whether the ACDP-2 takes care of it — is **open** and must be
  answered before relying on a self-built image.

### 6.0a Block 0 = Bosch software-authentication module (2026-10-17) [O]

The lowest block `0x0000–0x3FFF` has a block header at `0x200` (ident `bhie0101`, service table
`0x25C…0x280`: `0x2540`, `0x1F18`, `0x2060`, `0x1F30`, `0x2144`, `0x20E4`, `0x23F0`, `0x243E`,
`0x24C8`, `0x2570`), referenced by the bootloader (`0x1E294`), the boot manager (`0x21DDC…`), the
protection block (`0x3EF14`) and the boot data block (`0x6208`). It contains the SHA-1 routine
(`0x345A`), a RAM-download address check (`0x1A80`: `0xFFF80100–0xFFF81FFF`,
`0xFFF88400–0xFFF8FFFF`), a CAN parameter block (`0x1B00`: IDs `0x7E1`/`0x7E1`/`0x7E9`, mask `0x7FF`,
timeouts 25 / 75 / 150 ms and 1 s) and **no CAN register access**. Its name, stored bit-inverted at
`0x288`, is **`Bosch.common.Auth_TSW.TCU2.V001`**, followed by key material.

Matching bit-inverted identifiers, each followed by key data (not reproduced here), at `0x41D4`
(boot data) and `0x2112C` (boot manager): `GS_003_0001.Auth_TSW`, `GS_003_0001.Auth_Calibration`,
`GS_003_0002.Auth_Calibration`; at `0x5498` / `0x21A31`: `TC_003_0001.TSW`, `TC_003_0001.Project`
(two versions), `GS_003_0001.Auth_Prog`.

[I] This is Bosch's secured flashing (the `TPROT` question of §6): the application ("TSW") and the
calibration are authenticated against keys in the boot area, and programming needs an
authorisation (`Auth_Prog`). The ACDP-2's encrypted power-up session (§5b) writes modified images,
so it bypasses or satisfies this path. Whether anything checks the application at every
start-up was **answered on the bench (item 12, 2026-10-17): no.** An image with 5 changed bytes in
the application padding (`0x172000 = 5A`, `0x172004–07` chosen so the CRC-32 stays `29B578DF`) was
written with the ACDP-2 (it rewrote only the changed region: ≈ 900 bytes on the bus), read back
byte-identical, and the application started normally (periodic messages and UDS replies). So the
Bosch authentication only guards the Bosch reprogramming path; at power-on the boot side accepts
any application whose header, CRC and markers are valid (§7).

The power-up handler that answers `7E1#D7 65 69 15` is **not** in the dumped flash (the constant
does not occur in any byte order; block 0 does not drive CAN). [I] It is in the user boot MAT.

## 6.1 Programming-tool support (as of 2026-10-14)

The ACDP-2 produces three files per read [O]:

| File | Content |
|---|---|
| `…_IFla.bin` | full 2.5 MB flash image (user MAT) |
| `…_Maps.bin` | same 2.5 MB address space, **calibration block only** (`0x180000–0x1FFFFF`), everything else `0xFF`; byte-identical to the flash image in that block, including markers, header and CRC |
| `…_IEep.bin` | 128 KB EEPROM (data flash) |

[I] If the ACDP-2 can write the maps file on its own, calibration-only changes need not rewrite
the application.

Per the user's research, the **ACDP-2 does not correct header CRCs and does not handle the
protection check**. Consequences:

- **CRCs:** the project build must write correct CRC-32 values. `fix_header_crc.py` (project
  root) checks and recomputes them by following the header chain; it reproduces the stock values
  exactly and corrects a modified copy [O].
- **Protection:** observed so far [O] — the boot software contains SHA-1 primitives (round
  constant at `0x2180`, initial value at `0x3D04`) reached through the boot service table at
  `0x200`; the protection record at `0x3D12C` holds a 16-byte reference value followed by two
  pointers into the calibration area (`0x1BD48C`, `0x1BD4F4`); the application holds that value
  plus 16 further bytes at `0x412E0`. Whether the check is keyed is **open**.
- **Where the protection check runs** [O]: the verification state machine (`0x1BD58`) is
  referenced only from the bootloader block. Its callers are the address-range check used before
  erase/download (`0x18540`) and a post-download check job (`0x190CC`), both run by the
  bootloader's background job dispatcher (`0x13780`) during a **reprogramming session**. No
  reference was found from the boot manager's start-up path (`0x22224`) or from the application.
  [I] The check therefore gates reprogramming through the Bosch bootloader, not normal start-up.
- The protection block's code (`0x3D8F6`, `0x3DA56`) passes pointers to seven calibration tables
  in `0x1BDA56–0x1BEAF2` (plain 16-bit data, not code) [O]. [I] Parts of the calibration may be
  covered by the protection data as well as by the calibration CRC.
  The power-on selection code in the user boot MAT has not been seen, so a start-up check there
  cannot be excluded.

**Removing TPROT** (project decision 2026-10-14, right-to-repair scope under the DMCA §1201
vehicle exemption):
- Application side: nothing to remove — the project application simply does not include the
  reference value or any self-check.
- Boot side: if the bench test (§8 item 2) shows a CRC-corrected modified application starts,
  no boot change is needed. Patching the protection check out of the bootloader is possible in
  principle, but is **not recommended** while the user boot MAT is not backed up: a failed write
  to the boot blocks can leave the unit recoverable only through chip boot mode, which erases the
  user boot MAT.
- Consequence to note: after the application is replaced, a later reprogramming attempt through
  the Bosch bootloader (dealer/OEM tools) may fail its own protection check.

## 7. What a project image must provide (summary)

1. Header at `0x40000` in the format of §2, chained to the calibration header at `0x180200`,
   with the end word `0x12345678` at the address in +0x0C (checked by the boot manager).
2. CRC-32 of the stated range at +0x44 (application and calibration), computed by the build.
3. Entry address at +0x7C; vector table from +0x80, inside the CRC range; VBR set by the entry code.
4. Marker areas in the state shown in §3 (or written by the programming tool).
4a. **The stock identification (bench 2026-10-17)**: tail record `0x17FB70–0x17FBFB` with the
   project code `0JIXC224` (followed by zeros), header +0x74 = `0x17FA70` and the table there
   (pointers into the calibration's identification). An image that was byte-correct in flash but
   carried `8HPTCU-OPEN M0` in the tail and +0x74 = 0 was **not started**: the boot manager stayed
   in control (its identifiers answered, blank VIN). The same firmware with these fields copied
   from the unit's read-out (`build_image.py --stock-ident`) started. [I] The boot side compares
   the project code `0JIX` (present in every Bosch block: `4HJD0JIX` at `0x7C74`, `0JIX22` in the
   boot manager at `0x21A1E`, `0JIX0JIX` in the protection block at `0x3FD70`); which of the three
   fields is checked was not isolated.
5. No use of the boot handover RAM (§5) other than writing the boot request record (§5a) when
   handing over to the boot side for reprogramming.
6. A defined behavior for the EEPROM CRC binding (§6).

## 8. Open items

1. **Read-out route for the user boot MAT** [M][D]: software can select it with `ROMMAT`
   (`0xFFFFA820`, write `0x3B01`; back with `0x3B00`) from RAM-resident code with interrupts
   masked and the ROM cache flushed (manual §25.3.5, §25.9.1). The project firmware does this on
   request (UDS routine `FE20`, `firmware/app/bench_dump.c`); not possible with the stock firmware,
   whose memory services refuse the code area.
   Boot selection logic in the user boot MAT (not dumped): exact conditions for starting the
   application (markers, CRC, protection check) and the register state at hand-over (stack, SR).
2. ~~Protection (`TPROT`) behavior with a modified application~~ — **answered 2026-10-17**: a
   modified application (content changed, CRC kept) starts normally (§6.0a). Still open: an image
   whose header CRC changes (the project image) — the boot check of the CRC is known (§2), the
   EEPROM CRC binding (§6) only matters to the stock application.
3. ~~Whether the ACDP-2 recalculates CRCs~~ — it does not (user research); whether it writes the
   markers is still open.
4. EEPROM reaction to a changed application/calibration CRC.
5. Extent of boot-reserved RAM (known: `0xFFF80000–0xFFF800FF` and `0xFFF84000–0xFFF840FF` are
   preserved across a reset; boot runtime RAM starts at `0xFFF84100`).
6. The user boot MAT's start decision. ACDP-2 module 19 connects over CAN through a custom plug
   (user, 2026-10-14), so a unit whose application does not hand over (§5a) can only be
   recovered if the user boot MAT rejects that application, or through the chip's own boot mode
   (which risks the user boot MAT, §6.1).
7. Call site of the reset after the stock application writes the request record (§5a step 5),
   and whether the boot manager needs anything beyond the record to start the bootloader.

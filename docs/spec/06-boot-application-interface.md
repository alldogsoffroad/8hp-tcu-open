# Spec 06 — Boot-to-application interface

Status: **draft.** The block layout, headers, CRCs, validity markers and the application entry
contract are established. The very first reset stage (selection between bootloader and
application) runs from the 32 KB user boot MAT, which is **not in the available dump**, so the
exact start conditions are partly inferred.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Context: the project writes the controller with an ACDP-2 (module 19) through the stock Bosch
software, so the Bosch boot blocks stay in place and the project replaces the **application**
and **calibration** blocks only.

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

1. Header at `0x40000` in the format of §2, chained to the calibration header at `0x180200`.
2. CRC-32 of the stated range at +0x44 (application and calibration), computed by the build.
3. Entry address at +0x7C; vector table from +0x80, inside the CRC range; VBR set by the entry code.
4. Marker areas in the state shown in §3 (or written by the programming tool).
5. No use of the boot handover RAM (§5).
6. A defined behavior for the EEPROM CRC binding (§6).

## 8. Open items

1. Boot selection logic in the user boot MAT (not dumped): exact conditions for starting the
   application (markers, CRC, protection check) and the register state at hand-over (stack, SR).
2. Protection (`TPROT`) behavior with a modified application. Bench test on a spare: change one
   unused byte in the application, fix the CRC with `fix_header_crc.py`, write with the ACDP-2,
   and observe whether the application starts (CAN traffic, diagnostics).
3. ~~Whether the ACDP-2 recalculates CRCs~~ — it does not (user research); whether it writes the
   markers is still open.
4. EEPROM reaction to a changed application/calibration CRC.
5. Extent of boot-reserved RAM.

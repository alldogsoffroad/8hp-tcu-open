#!/usr/bin/env python3
"""Wrap a raw SH7254R flash dump in a minimal big-endian ELF32 so REA/Ghidra can load it.

Layout (from the SH7254R hardware manual, 2.5 MB ROM variant; block layout from spec 06):
  ROM code   0x00000000-0x0017FFFF  boot blocks + application (PT_LOAD, R+X)
  ROM data   0x00180000-0x0027FFFF  calibration + unused top (PT_LOAD, R+W, not executable)
  EEPROM     0x80100000-0x8011FFFF  optional EEPROM/data-flash dump (PT_LOAD, R+W)
  RAM        0xFFF80000-0xFFF9FFFF  128 KB, zero-filled (PT_LOAD, R+W)
  Peripheral 0xFFFC0000-0xFFFFFFEF  on-chip I/O registers, zero-filled (PT_LOAD, R+W)

RAM and I/O carry zero file bytes because Ghidra 12.1 marks memsz-only segments read-only,
which would let the decompiler fold RAM reads into constants.

The calibration area is a separate non-executable segment so analysis does not create false
functions in data. It is marked writable on purpose: Ghidra treats non-writable initialized
memory as constant and would fold calibration values into the decompiled code, hiding which
calibration item each piece of code reads.

Usage: make_sh7254r_elf.py <raw.bin> <out.elf> [--eeprom eep.bin] [--machine N] [--entry 0xADDR]
"""
import argparse
import struct

ROM_BASE = 0x00000000
CODE_END = 0x00180000  # first address of the calibration block (spec 06)
EEPROM_BASE = 0x80100000
EEPROM_SIZE = 0x20000
SEGMENTS_ZERO = [
    (0xFFF80000, 0x20000, 6),  # on-chip RAM, RW
    (0xFFFC0000, 0x3FFF0, 6),  # peripheral I/O registers, RW (stops 16 bytes short: Ghidra overflows at 0xFFFFFFFF)
]
PAGE = 0x1000


def build(raw: bytes, eeprom: bytes | None, machine: int, entry: int, flags: int) -> bytes:
    ehsize, phentsize = 52, 32
    loads = [(ROM_BASE, raw[:CODE_END], 5)]  # R+X: boot blocks + application
    if len(raw) > CODE_END:
        loads.append((ROM_BASE + CODE_END, raw[CODE_END:], 6))  # R+W, not executable: calibration
    if eeprom is not None:
        loads.append((EEPROM_BASE, eeprom, 6))  # R+W
    loads += [(vaddr, b"\0" * size, seg_flags) for vaddr, size, seg_flags in SEGMENTS_ZERO]
    phnum = len(loads)
    header = struct.pack(
        ">4sBBBBB7sHHIIIIIHHHHHH",
        b"\x7fELF", 1, 2, 1, 0, 0, b"\0" * 7,  # ELF32, big-endian, version 1
        2, machine, 1, entry, ehsize, 0, flags,  # ET_EXEC, e_machine, version, entry, phoff, shoff, e_flags
        ehsize, phentsize, phnum, 0, 0, 0,
    )
    phdrs = b""
    body = b""
    offset = PAGE  # page-align file contents
    for vaddr, data, seg_flags in loads:
        phdrs += struct.pack(">IIIIIIII", 1, offset, vaddr, vaddr, len(data), len(data), seg_flags, 4)
        padded = data + b"\0" * (-len(data) % PAGE)
        body += padded
        offset += len(padded)
    out = header + phdrs
    return out + b"\0" * (PAGE - len(out)) + body


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("raw")
    ap.add_argument("out")
    ap.add_argument("--eeprom", help=f"EEPROM/data-flash dump mapped at {EEPROM_BASE:#x} ({EEPROM_SIZE // 1024} KB)")
    ap.add_argument("--machine", type=lambda s: int(s, 0), default=40, help="ELF e_machine. 40 (EM_ARM) because REA only admits x86/x64/MIPS/ARM/AArch64 ELF and its MIPS lane needs extra ABI records; REA_GHIDRA_LANGUAGE_ID forces SH-2A in Ghidra regardless. 42 = EM_SH")
    ap.add_argument("--flags", type=lambda s: int(s, 0), default=0, help="ELF e_flags")
    ap.add_argument("--entry", type=lambda s: int(s, 0), default=0)
    a = ap.parse_args()
    raw = open(a.raw, "rb").read()
    eeprom = None
    if a.eeprom:
        eeprom = open(a.eeprom, "rb").read()
        if len(eeprom) != EEPROM_SIZE:
            ap.error(f"EEPROM dump is {len(eeprom):#x} bytes; expected {EEPROM_SIZE:#x}")
    open(a.out, "wb").write(build(raw, eeprom, a.machine, a.entry, a.flags))
    extra = f", {len(eeprom):#x} EEPROM bytes at {EEPROM_BASE:#x}" if eeprom else ""
    print(f"wrote {a.out}: {len(raw):#x} ROM bytes at {ROM_BASE:#x}{extra}, e_machine={a.machine}, entry={a.entry:#x}")


if __name__ == "__main__":
    main()

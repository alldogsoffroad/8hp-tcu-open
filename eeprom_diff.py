#!/usr/bin/env python3
"""Reassemble and compare SH7254R EEPROM blocks (Bosch TCU 1034420267 layout).

Layout (from firmware analysis):
  4 x 32 KB sections; each starts with 0x4001, a 32-bit sequence counter at +8 and
  a byte-sum checksum at +0x54. Records start at section offset +0x80, each with an
  8-byte header: [flag bits | block hi][block lo][u16 data len][u16 total len]
  [u8 fragment][u8 0x01]. Blocks larger than 0x300 bytes are split into fragments.
  "MMSBLANK" 8-byte units are the reading tool's marker for erased flash.

Usage:
  eeprom_diff.py A.bin [B.bin]          compare newest section of A with newest of B
  eeprom_diff.py A.bin --sections X Y   compare sections X and Y of one dump
  eeprom_diff.py A.bin --list           list sections and blocks
"""
import argparse
import struct

SECTION = 0x8000
FILLER = (b"MMSBLANK", b"\xff" * 8)


def sections(dump: bytes):
    out = []
    for i in range(len(dump) // SECTION):
        base = i * SECTION
        if dump[base:base + 2] == b"\x40\x01":
            out.append((i, struct.unpack_from(">I", dump, base + 8)[0]))
    return out


def blocks(dump: bytes, index: int) -> dict[int, bytes]:
    base = index * SECTION
    h = lambda o: struct.unpack_from(">H", dump, o)[0]
    o, out = base + 0x80, {}
    while o < base + SECTION - 8:
        ln, tot = h(o + 2), h(o + 4)
        if 0 < ln < 0x4000 and ln + 8 <= tot <= ln + 16 and o + tot <= base + SECTION:
            hdr = h(o)
            block = ((hdr >> 8) & 0x0F) << 8 | (hdr & 0xFF)
            out[block] = out.get(block, b"") + dump[o + 8:o + 8 + ln]
            o += tot
        elif dump[o:o + 8] in FILLER:
            o += 8
        else:
            o += 2
    return out


def newest(dump: bytes) -> int:
    return max(sections(dump), key=lambda s: s[1])[0]


def diff(a: dict[int, bytes], b: dict[int, bytes]) -> None:
    for block in sorted(set(a) | set(b)):
        x, y = a.get(block), b.get(block)
        if x is None or y is None:
            print(f"block 0x{block:04X}: only in {'B' if x is None else 'A'}")
            continue
        changed = [i for i in range(min(len(x), len(y))) if x[i] != y[i]]
        if len(x) != len(y):
            print(f"block 0x{block:04X}: length 0x{len(x):X} -> 0x{len(y):X}")
        if not changed:
            continue
        print(f"block 0x{block:04X}: {len(changed)} byte(s) changed")
        run_start = prev = changed[0]
        for i in changed[1:] + [None]:
            if i is not None and i == prev + 1:
                prev = i
                continue
            print(f"   +0x{run_start:04X}..0x{prev:04X}  {x[run_start:prev + 1].hex(' ')}  ->  {y[run_start:prev + 1].hex(' ')}")
            if i is not None:
                run_start = prev = i


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b", nargs="?")
    ap.add_argument("--sections", nargs=2, type=int, metavar=("X", "Y"))
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()
    a = open(args.a, "rb").read()
    if args.list:
        for i, seq in sections(a):
            blk = blocks(a, i)
            print(f"section {i} seq 0x{seq:X}: " + ", ".join(f"0x{k:04X}({len(v):#x})" for k, v in sorted(blk.items())))
        return
    if args.sections:
        x, y = args.sections
        print(f"section {x} -> section {y}")
        diff(blocks(a, x), blocks(a, y))
    else:
        b = open(args.b, "rb").read() if args.b else a
        print(f"A newest section {newest(a)} -> B newest section {newest(b)}")
        diff(blocks(a, newest(a)), blocks(b, newest(b)))


if __name__ == "__main__":
    main()

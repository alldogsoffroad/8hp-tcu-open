#!/usr/bin/env python3
"""Check or recompute the CRC-32 in the block headers of a 2.5 MB SH7254R flash image.

Header format (spec 06): marker 0x876543xx at +0x00, CRC range start/end (inclusive) at
+0x38/+0x3C, standard CRC-32 (zlib/IEEE) of that range at +0x44. Headers are found by
following the chain from the application header (+0x08 = next header, 0 = end). Works on the
full flash image and on the ACDP-2 maps file (calibration only; starts at the calibration
header when the application area is blank).

Usage:
  fix_header_crc.py image.bin            report stored vs computed CRC for each header
  fix_header_crc.py image.bin -o out.bin write a copy with corrected CRCs

Note: this covers only the CRC fields. It does not touch the validity markers or the
protection record (spec 06 §3, §6).
"""
import argparse
import struct
import zlib

APP_HEADER = 0x40000
CAL_HEADER = 0x180200  # used when the image is an ACDP-2 maps file (application area blank)
MARKER_MASK = 0xFFFFFF00
MARKER = 0x87654300


def u32(buf: bytes, off: int) -> int:
    return struct.unpack_from(">I", buf, off)[0]


def header_chain(img: bytes, start: int = APP_HEADER):
    seen, off = set(), start
    while off and off not in seen and off + 0x48 <= len(img):
        if u32(img, off) & MARKER_MASK != MARKER:
            raise ValueError(f"no header marker at {off:#x}")
        seen.add(off)
        yield off
        off = u32(img, off + 0x08)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("-o", "--output", help="write corrected image here")
    a = ap.parse_args()
    img = bytearray(open(a.image, "rb").read())
    changed = False
    start = APP_HEADER if u32(img, APP_HEADER) & MARKER_MASK == MARKER else CAL_HEADER
    if start != APP_HEADER:
        print("no application header; treating the image as a calibration-only (maps) file")
    for hdr in header_chain(img, start):
        start, end, stored = u32(img, hdr + 0x38), u32(img, hdr + 0x3C), u32(img, hdr + 0x44)
        if not (hdr < start <= end < len(img)):
            raise ValueError(f"header {hdr:#x}: implausible CRC range {start:#x}-{end:#x}")
        crc = zlib.crc32(img[start:end + 1]) & 0xFFFFFFFF
        state = "ok" if crc == stored else "MISMATCH"
        print(f"header {hdr:#08x} type {img[hdr + 3]:02X} range {start:#08x}-{end:#08x} "
              f"stored {stored:08X} computed {crc:08X} {state}")
        if crc != stored:
            struct.pack_into(">I", img, hdr + 0x44, crc)
            changed = True
    if a.output:
        open(a.output, "wb").write(img)
        print(f"wrote {a.output}" + (" (CRCs corrected)" if changed else " (no changes needed)"))


if __name__ == "__main__":
    main()

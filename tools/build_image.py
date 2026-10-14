#!/usr/bin/env python3
"""Build a 2.5 MB flash image for the ACDP-2 from the project application block.

The boot blocks (0x000000-0x03FFFF) and the calibration block (0x180000-0x1FFFFF) are taken
from a reference read-out of the user's own controller (not part of the repository). The
application block (0x040000-0x17FFFF) is replaced by the linked project application.

Checks and fixes (spec 06):
  * header magic 0x876543C1 at 0x40000, chained to the calibration header at 0x180200
  * end pointer (+0x0C) inside the block, 4-byte aligned, word there == 0x12345678
    (checked by the Bosch boot manager, 0x24FD0)
  * entry point (+0x7C) inside the application code area
  * CRC-32 (zlib) of +0x38..+0x3C written to +0x44
  * validity markers: 0x17FE00 programmed (0x00), 0x17FF00 erased (0xFF)
  * calibration header CRC verified (not changed)

Usage:
  build_image.py --app build/sh2a/tcu_app.hex --base "<read-out>_IFla.bin" -o out_IFla.bin
"""
import argparse
import struct
import sys
import zlib

APP_START, APP_END = 0x040000, 0x180000          # application block (end exclusive)
MARKER1, MARKER2, MARKER_LEN = 0x17FE00, 0x17FF00, 0x100
CAL_HEADER = 0x180200
FLASH_SIZE = 0x280000
END_MAGIC = 0x12345678


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def fail(msg):
    sys.exit(f"build_image: ERROR: {msg}")


def check_header(img, hdr, block_start, block_end, name):
    if u32(img, hdr) & 0xFFFFFF00 != 0x87654300:
        fail(f"{name}: no header magic at {hdr:#x}")
    end = u32(img, hdr + 0x0C)
    if not (block_start <= end < block_end) or end & 3:
        fail(f"{name}: end pointer {end:#x} outside the block or not aligned")
    if u32(img, end) != END_MAGIC:
        fail(f"{name}: word at end pointer {end:#x} is {u32(img, end):#010x}, expected 0x12345678")
    start, stop = u32(img, hdr + 0x38), u32(img, hdr + 0x3C)
    if not (hdr < start <= stop < block_end):
        fail(f"{name}: implausible CRC range {start:#x}-{stop:#x}")
    return start, stop


def read_ihex(path):
    """Return the application block (from APP_START) as bytes, gaps filled with 0xFF."""
    data, base = {}, 0
    for line in open(path):
        line = line.strip()
        if not line.startswith(":"):
            continue
        rec = bytes.fromhex(line[1:])
        if sum(rec) & 0xFF:
            fail(f"checksum error in {path}: {line}")
        n, addr, typ, payload = rec[0], (rec[1] << 8) | rec[2], rec[3], rec[4:4 + rec[0]]
        if typ == 0x00:
            for i, v in enumerate(payload):
                data[base + addr + i] = v
        elif typ == 0x04:
            base = ((payload[0] << 8) | payload[1]) << 16
        elif typ == 0x02:
            base = ((payload[0] << 8) | payload[1]) << 4
        elif typ == 0x01:
            break
    if not data:
        fail("no data in the HEX file")
    lo, hi = min(data), max(data)
    if lo < APP_START or hi >= APP_END:
        fail(f"HEX data {lo:#x}-{hi:#x} outside the application block")
    out = bytearray(b"\xff" * (hi + 1 - APP_START))
    for k, v in data.items():
        out[k - APP_START] = v
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--app", required=True, help="linked application (Intel HEX, load addresses)")
    ap.add_argument("--base", required=True, help="reference full flash read-out (_IFla.bin)")
    ap.add_argument("-o", "--output", required=True)
    a = ap.parse_args()

    img = bytearray(open(a.base, "rb").read())
    if len(img) != FLASH_SIZE:
        fail(f"reference image is {len(img):#x} bytes, expected {FLASH_SIZE:#x}")
    app = read_ihex(a.app)
    if len(app) > MARKER1 - APP_START:
        fail(f"application is {len(app):#x} bytes and overlaps the marker area")

    # reference calibration must be intact
    cs, ce = check_header(img, CAL_HEADER, 0x180000, 0x200000, "calibration")
    if zlib.crc32(img[cs:ce + 1]) & 0xFFFFFFFF != u32(img, CAL_HEADER + 0x44):
        fail("calibration CRC in the reference image does not match")

    # replace the application block
    img[APP_START:APP_END] = b"\xff" * (APP_END - APP_START)
    img[APP_START:APP_START + len(app)] = app
    img[MARKER1:MARKER1 + MARKER_LEN] = b"\x00" * MARKER_LEN
    img[MARKER2:MARKER2 + MARKER_LEN] = b"\xff" * MARKER_LEN

    if u32(img, APP_START) != 0x876543C1:
        fail("application header magic missing (is --app the block starting at 0x40000?)")
    if u32(img, APP_START + 0x08) != CAL_HEADER:
        fail("application header does not chain to the calibration header")
    s, e = check_header(img, APP_START, APP_START, APP_END, "application")
    entry = u32(img, APP_START + 0x7C)
    if not (0x40870 <= entry <= e):
        fail(f"entry point {entry:#x} outside the application code")
    crc = zlib.crc32(img[s:e + 1]) & 0xFFFFFFFF
    struct.pack_into(">I", img, APP_START + 0x44, crc)

    open(a.output, "wb").write(img)
    print(f"application: {len(app):#x} bytes, entry {entry:#08x}, CRC {s:#x}-{e:#x} = {crc:08X}")
    print(f"markers: {MARKER1:#x} programmed, {MARKER2:#x} erased; calibration taken from reference")
    print(f"wrote {a.output}")


if __name__ == "__main__":
    main()

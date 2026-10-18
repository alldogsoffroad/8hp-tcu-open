#!/usr/bin/env python3
"""Build the data-flash application image (spec 18 section 4) from the RAM application.

Input: the linked RAM application (ELF, for the load and entry addresses) and its .image section
as a flat binary (objcopy -O binary --only-section=.image). Output: header (128 bytes) + payload,
to be written at 0x80100000 (data flash DB00 onwards) by `tcu_bench.py app-flash`.

Usage: build_app_image.py --elf tcu_app_ram.elf --bin tcu_app_ram.bin -o tcu_app_df.bin
"""
import argparse
import shutil
import struct
import subprocess
import sys
import time
import zlib

MAGIC = b"8HPAPP01"
HDR_LEN = 0x80
LOAD = 0xFFF84100
MAX_PAYLOAD = 0x10000 - HDR_LEN


def symbols(elf, nm):
    out = subprocess.run([nm, elf], check=True, capture_output=True, text=True).stdout
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            syms[parts[2]] = int(parts[0], 16)
    return syms


def build(payload, entry, version):
    if len(payload) % 128:
        payload += b"\x00" * (128 - len(payload) % 128)
    if not 0 < len(payload) <= MAX_PAYLOAD:
        sys.exit(f"build_app_image: payload {len(payload):#x} bytes does not fit (max {MAX_PAYLOAD:#x})")
    if not LOAD <= entry < LOAD + len(payload):
        sys.exit(f"build_app_image: entry {entry:#x} outside the payload")
    ver = version.encode()[:16].ljust(16, b"\x00")
    hdr = MAGIC + struct.pack(">IIII", len(payload), LOAD, entry, zlib.crc32(payload) & 0xFFFFFFFF) + ver
    return hdr.ljust(HDR_LEN, b"\x00") + payload


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--bin", required=True)
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--nm", default=shutil.which("sh-elf-nm") or "sh-elf-nm")
    ap.add_argument("--version", default="RAM " + time.strftime("%y%m%d-%H%M"))
    a = ap.parse_args()
    syms = symbols(a.elf, a.nm)
    if syms.get("__image_start") != LOAD:
        sys.exit(f"build_app_image: __image_start is {syms.get('__image_start')}, expected {LOAD:#x}")
    img = build(open(a.bin, "rb").read(), syms["_start"], a.version)
    open(a.output, "wb").write(img)
    print(f"data-flash image: {len(img) - HDR_LEN:#x} payload bytes, entry {syms['_start']:#x}, "
          f"CRC {struct.unpack_from('>I', img, 0x14)[0]:08X}, version '{a.version}' -> {a.output}")


if __name__ == "__main__":
    main()

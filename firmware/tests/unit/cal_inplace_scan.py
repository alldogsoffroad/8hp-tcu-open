#!/usr/bin/env python3
"""cal_inplace_scan.py — the shadows read the stock calibration IN PLACE (app/cal_inplace.h), so the
flashed application must never place a section at or above APP_LIMIT.

2026-11-20 correction: the application block is 0x040000-0x17FFFF (spec 06, tools/build_image.py
APP_START/APP_END, the linker's APPROM, the app CRC 0x40080-0x17FB6F); the stock calibration block
is 0x180000-0x1FFFFF. The earlier "block 0x80000 and up is calibration" was wrong: 0x80000-0x17FFFF
holds stock application code our firmware never runs. APP_LIMIT = 0xA0000 for now (blocks
0x80000/0x90000 are 64 KB, the same write path as 0x40000-0x70000); raise it deliberately.
Every in-place address must lie at or above APP_LIMIT (never inside our own application).

  cal_inplace_scan.py elf OBJDUMP ELF...   `objdump -h` of each ELF: no loaded (CONTENTS) section whose VMA or LMA
                                           range touches [APP_LIMIT, 0x280000); the application's own
                                           flash sections (0x40000 .. APP_LIMIT) are listed with the
                                           highest end address
  cal_inplace_scan.py text FILE...         the same check on saved `objdump -h` text (the negative fixture)
  cal_inplace_scan.py header FILE          every CAL_* address #define of app/cal_inplace.h that names a
                                           stock address is at or above APP_LIMIT and inside the image
Exit 1 ("cal in-place violation") on any hit.
"""
import re
import subprocess
import sys

APP_LIMIT = 0xA0000          # our application's flash limit (end exclusive)
CAL_BASE = APP_LIMIT          # in-place reads must lie at or above it
IMG_END = 0x280000
APP_BASE = 0x40000

# the stock's application tail / end markers (platform/sh7254r/header.c), byte-identical to the stock image
# at those addresses ("0JIHC224" and 0x12345678) and never installed (the install writes blocks 0x40000..0x70000
# only): the one pair of sections allowed at APP_LIMIT or above
ALLOW = {('.app_tail', 0x17FB70, 0x10), ('.app_end', 0x17FBFC, 0x4)}

SEC = re.compile(r'^\s*\d+\s+(\S+)\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s+[0-9a-fA-F]{8}\s+2\*\*\d+\s*$')


def sections(text):
    lines = text.splitlines()
    out = []
    for i, ln in enumerate(lines):
        m = SEC.match(ln)
        if not m:
            continue
        flags = lines[i + 1] if i + 1 < len(lines) else ''
        out.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16), 'CONTENTS' in flags))
    return out


def check(text, label):
    bad = 0
    app_end = 0
    secs = sections(text)
    if not secs:
        print('cal in-place violation: no sections parsed from %s' % label)
        return 1
    for name, size, vma, lma, loaded in secs:
        if not loaded or size == 0:       # NOLOAD / .bss: nothing is written
            continue
        if (name, lma, size) in ALLOW and vma == lma:
            continue
        for kind, a in (('VMA', vma), ('LMA', lma)):
            if a < IMG_END and a + size > CAL_BASE:
                print('cal in-place violation: %s section %s %s 0x%06X..0x%06X touches the stock calibration '
                      'region / beyond the app limit 0x%06X..0x%06X' % (label, name, kind, a, a + size, CAL_BASE, IMG_END))
                bad += 1
        if APP_BASE <= lma < CAL_BASE:
            app_end = max(app_end, lma + size)
    if not bad:
        print('cal in-place: %s: %d sections, none at 0x%X or above (flash app ends 0x%X)' % (label, len(secs), CAL_BASE, app_end))
    return bad


def main():
    mode = sys.argv[1]
    bad = 0
    if mode == 'elf':
        objdump = sys.argv[2]
        for elf in sys.argv[3:]:
            txt = subprocess.run([objdump, '-h', elf], check=True, capture_output=True, text=True).stdout
            bad += check(txt, elf)
    elif mode == 'text':
        for f in sys.argv[2:]:
            bad += check(open(f).read(), f)
    elif mode == 'header':
        txt = open(sys.argv[2]).read()
        n = 0
        for m in re.finditer(r'^#define\s+(CAL_(?:FR|SM|GP)_[A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+)u\b', txt, re.M):
            name, v = m.group(1), int(m.group(2), 16)
            if name.endswith('_LEN') or name.endswith('_STEP') or name.endswith('_ROW') or name.endswith('_NG') \
                    or name.endswith('_NE') or name.endswith('_NMAP'):
                continue
            n += 1
            if not (CAL_BASE <= v < IMG_END):
                print('cal in-place violation: %s = 0x%X is outside 0x%X..0x%X' % (name, v, CAL_BASE, IMG_END))
                bad += 1
        print('cal in-place: %d address defines checked' % n)
    else:
        print(__doc__)
        return 2
    if bad:
        print('cal in-place violation (%d)' % bad)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

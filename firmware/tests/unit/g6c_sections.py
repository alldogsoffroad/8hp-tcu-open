#!/usr/bin/env python3
"""g6c_sections.py - G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): NO TWO RAM
SECTIONS OF THE IMAGE OVERLAP. ld does not refuse overlapping NOLOAD
sections; linker.ld ASSERTs hold each pinned object between its neighbours,
and this scan checks every allocated section of the linked image pairwise
(VMA in the MCU RAM 0xFFF80000..0xFFFA0000, size > 0).

  g6c_sections.py OBJDUMP ELF [--fixture FILE]
  (--fixture: read 'name vma size' lines instead of the ELF — the negative)
Exit 1 ("RAM section overlap") on any overlap.
"""
import subprocess
import sys

LO, HI = 0xFFF80000, 0xFFFA0000


def sections(objdump, elf):
    out = subprocess.run([objdump, "-h", elf], capture_output=True, text=True, check=True).stdout.split("\n")
    secs = []
    for i, line in enumerate(out):
        f = line.split()
        if len(f) >= 7 and f[0].isdigit():
            flags = out[i + 1] if i + 1 < len(out) else ""
            if "ALLOC" not in flags:
                continue
            name, size, vma = f[1], int(f[2], 16), int(f[3], 16)
            if size > 0 and LO <= vma < HI:
                secs.append((name, vma, size))
    return secs


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "--fixture":
        secs = []
        for line in open(sys.argv[2]):
            f = line.split()
            if len(f) == 3 and not line.startswith("#"):
                secs.append((f[0], int(f[1], 16), int(f[2], 16)))
    else:
        secs = sections(sys.argv[1], sys.argv[2])
    secs.sort(key=lambda s: s[1])
    bad = []
    for i in range(len(secs)):
        for j in range(i + 1, len(secs)):
            a, b = secs[i], secs[j]
            if b[1] >= a[1] + a[2]:
                break
            bad.append("%s 0x%08X+0x%X overlaps %s 0x%08X+0x%X" % (a[0], a[1], a[2], b[0], b[1], b[2]))
    if bad:
        print("RAM section overlap:\n  " + "\n  ".join(bad))
        return 1
    print("RAM sections: %d allocated, no overlap" % len(secs))
    return 0


if __name__ == "__main__":
    sys.exit(main())

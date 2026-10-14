#!/usr/bin/env python3
"""List and dump calibration objects of a 1034420267 flash or maps image (spec 09 §1).

The calibration block holds an index of 12,198 object addresses at 0x180280. Each object is
a scalar, a byte array, a 1-D curve or a 2-D map (formats in spec 09 §1.2). This tool reads
them from the user's own dump, so no calibration values need to be copied into the project.

Usage:
  cal_catalog.py image.bin --csv catalog.csv     one row per object (index, address, type, axes)
  cal_catalog.py image.bin --show 11642          print one object by index (or 0x1E9910 by address)
  cal_catalog.py image.bin --shift-maps          list the 14-column shift maps (spec 09 §2)
"""
import argparse
import csv
import struct

INDEX_START, INDEX_END = 0x180280, 0x18C118
CAL_END = 0x1FFD60  # end of the calibration CRC range + 1
SHIFT_GROUP_FIRST = 11642  # first object of the shift-map group (RAM pointer 0xFFF84EB0)


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def s16(b, o):
    return struct.unpack_from(">h", b, o)[0]


def load_index(img):
    idx = [u32(img, o) for o in range(INDEX_START, INDEX_END, 4)]
    addrs = sorted(set(idx))
    nxt = {a: (addrs[i + 1] if i + 1 < len(addrs) else CAL_END) for i, a in enumerate(addrs)}
    return idx, nxt


def classify(img, a, size):
    """Return (type, x_axis, y_axis, data). Types: map, map8, curve, curve8, arr8, scalar, raw."""
    if size in (1, 2) or a % 4:
        if size == 1:
            return "scalar", None, None, [img[a]]
        if size == 2:
            return "scalar", None, None, [s16(img, a)]
        return "raw", None, None, list(img[a:a + size])
    if size >= 12:
        n1 = u32(img, a)
        if 1 < n1 <= 64:
            n2 = u32(img, a + 4)
            if 1 < n2 <= 64:
                x = [s16(img, a + 8 + 2 * k) for k in range(n1)]
                y = [s16(img, a + 8 + 2 * n1 + 2 * k) for k in range(n2)]
                if x == sorted(x) and y == sorted(y):
                    base = a + 8 + 2 * (n1 + n2)
                    need16 = 8 + 2 * (n1 + n2 + n1 * n2)
                    need8 = 8 + 2 * (n1 + n2) + n1 * n2
                    if need16 <= size < need16 + 4:
                        return "map", x, y, [s16(img, base + 2 * k) for k in range(n1 * n2)]
                    if need8 <= size < need8 + 4:
                        return "map8", x, y, list(img[base:base + n1 * n2])
            x = [s16(img, a + 4 + 2 * k) for k in range(n1)]
            if x == sorted(x):
                if 4 + 4 * n1 <= size < 4 + 4 * n1 + 4:
                    return "curve", x, None, [s16(img, a + 4 + 2 * n1 + 2 * k) for k in range(n1)]
                if 4 + 3 * n1 <= size < 4 + 3 * n1 + 4:
                    return "curve8", x, None, list(img[a + 4 + 2 * n1:a + 4 + 3 * n1])
            if 4 + n1 <= size < 4 + n1 + 4:
                return "arr8", None, None, list(img[a + 4:a + 4 + n1])
    return "raw", None, None, list(img[a:a + size])


def show(img, i, a, size):
    t, x, y, d = classify(img, a, size)
    print(f"index {i}  address {a:#08x}  size {size}  type {t}")
    if t in ("map", "map8"):
        print("x:", x)
        for r, yv in enumerate(y):
            print(f"{yv:>7}: " + " ".join(f"{v:6d}" for v in d[r * len(x):(r + 1) * len(x)]))
    elif t in ("curve", "curve8"):
        print("x:", x)
        print("y:", d)
    else:
        print(d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--csv")
    ap.add_argument("--show")
    ap.add_argument("--shift-maps", action="store_true")
    args = ap.parse_args()
    img = open(args.image, "rb").read()
    idx, nxt = load_index(img)

    if args.csv:
        with open(args.csv, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["index", "address", "size", "type", "x_axis", "y_axis"])
            for i, a in enumerate(idx):
                t, x, y, _ = classify(img, a, nxt[a] - a)
                w.writerow([i, f"0x{a:06X}", nxt[a] - a, t,
                            " ".join(map(str, x)) if x else "", " ".join(map(str, y)) if y else ""])
        print(f"wrote {len(idx)} objects to {args.csv}")

    if args.show:
        v = int(args.show, 0)
        if v < len(idx):
            show(img, v, idx[v], nxt[idx[v]] - idx[v])
        else:
            for i, a in enumerate(idx):
                if a == v:
                    show(img, i, a, nxt[a] - a)
                    break
            else:
                print(f"{v:#x} is not the start of a calibration object")

    if args.shift_maps:
        print("index  address   offset  pedal_points  WOT upshift 1-2..7-8 (output rpm)")
        for i, a in enumerate(idx):
            t, x, y, d = classify(img, a, nxt[a] - a)
            if t == "map" and x == list(range(1, 15)):
                wot = d[(len(y) - 1) * 14:(len(y) - 1) * 14 + 7]
                print(f"{i:5d}  {a:#08x}  {i - SHIFT_GROUP_FIRST:+5d}  {len(y):3d}  {wot}")


if __name__ == "__main__":
    main()

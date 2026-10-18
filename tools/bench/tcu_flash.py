"""Project CAN reflash (spec 16): image checks, block comparison and the power-loss-safe write order.

Independent of the CAN adapter: `Flasher` takes any object with
`request(payload: bytes, timeout: float) -> bytes | None` (tcu_bench.Uds, or the simulator bridge
in test_tcu_flash.py).

Write order (spec 16 section 5):
  1. compare every erase block in 0x040000-0x1FFFFF with the image (CRC-32), keep the differing ones
  2. invalidate: erase the block holding marker 1 of every region that differs (application
     0x160000 first, then calibration 0x180000)
  3. erase + program + verify the other differing blocks
  4. program the marker block except marker 1, verify; then marker 1 last (calibration first,
     application last), verify
A power loss before step 4 completes leaves marker 1 erased: the Bosch boot software does not start
the application and the ACDP-2 power-up path recovers the unit.
"""
import zlib

WIN_LO, WIN_HI = 0x040000, 0x200000
PAGE = 256
FLASH_LEN = 0x200000

REGIONS = (
    {"name": "application", "lo": 0x040000, "hi": 0x180000, "marker1": 0x17FE00, "marker2": 0x17FF00},
    {"name": "calibration", "lo": 0x180000, "hi": 0x200000, "marker1": 0x180000, "marker2": 0x180100},
)


class FlashError(Exception):
    pass


def block_size(a):
    if a < 0x010000:
        return 0x2000
    if a < 0x0A0000:
        return 0x10000
    return 0x20000


def blocks(lo, hi):
    a = lo
    while a < hi:
        yield a, block_size(a)
        a += block_size(a)


def block_of(a):
    for b, n in blocks(0, FLASH_LEN):
        if b <= a < b + n:
            return b
    raise ValueError(hex(a))


def crc(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def check_image(img):
    """List of reasons the image must not be written (empty = fine)."""
    bad = []
    if len(img) < FLASH_LEN:
        return [f"image is {len(img):#x} bytes, expected at least {FLASH_LEN:#x} (a full _IFla.bin)"]
    for r in REGIONS:
        if img[r["marker1"]:r["marker1"] + PAGE] != b"\x00" * PAGE:
            bad.append(f"{r['name']}: marker 1 at {r['marker1']:#x} is not programmed (256 x 00)")
        if img[r["marker2"]:r["marker2"] + PAGE] != b"\xFF" * PAGE:
            bad.append(f"{r['name']}: marker 2 at {r['marker2']:#x} is not erased (256 x FF)")
    if img[0x40000:0x40003] != b"\x87\x65\x43":
        bad.append("no block header (0x876543xx) at 0x40000")
    if img[0x180200:0x180203] != b"\x87\x65\x43":
        bad.append("no block header (0x876543xx) at 0x180200")
    return bad


class Flasher:
    def __init__(self, uds, log=print):
        self.uds = uds
        self.log = log

    def req(self, payload, timeout=3.0, ok=None):
        r = self.uds.request(bytes(payload), timeout=timeout)
        if r is None:
            raise FlashError(f"no response to {bytes(payload[:12]).hex(' ')}")
        if r[0] == 0x7F:
            raise FlashError(f"negative response {r.hex(' ')} to {bytes(payload[:12]).hex(' ')}"
                             + {0x31: " (outside the window / not aligned)", 0x70: " (target not blank)",
                                0x72: " (flash controller error: see `ident`)", 0x73: " (sequence counter)",
                                0x24: " (sequence error)"}.get(r[2] if len(r) > 2 else 0, ""))
        if ok is not None and r[0] != ok:
            raise FlashError(f"unexpected response {r.hex(' ')}")
        return r

    def ident(self):
        r = self.uds.request(b"\x22\xF1\x95", timeout=1.0)
        if not r or len(r) < 12 or r[:5] != b"\x62\xF1\x95FL":
            return None
        return {"version": r[5], "fpmon": r[6], "fstatr0": r[7], "fstatr1": r[8], "fastat": r[9],
                "fpestat": r[10], "last_error": r[11], "fwe": bool(r[6] & 0x80)}

    def crc(self, a, n):
        r = self.req(b"\x31\x01\xFF\x01" + a.to_bytes(4, "big") + n.to_bytes(4, "big"), timeout=10.0, ok=0x71)
        return int.from_bytes(r[4:8], "big")

    def erase(self, a, n):
        self.req(b"\x31\x01\xFF\x00" + a.to_bytes(4, "big") + n.to_bytes(4, "big"), timeout=10.0, ok=0x71)

    def program(self, a, data):
        assert a % PAGE == 0 and len(data) % PAGE == 0 and data
        self.req(b"\x34\x00\x44" + a.to_bytes(4, "big") + len(data).to_bytes(4, "big"), ok=0x74)
        for i in range(0, len(data), PAGE):
            ctr = (i // PAGE + 1) & 0xFF
            self.req(bytes([0x36, ctr]) + data[i:i + PAGE], ok=0x76)
        self.req(b"\x37", ok=0x77)

    def reset(self):
        self.uds.request(b"\x11\x01", timeout=1.0)


def program_block(fl, img, b, n, skip=None):
    """Program the non-blank pages of one erased block in runs (pages of all FF stay erased)."""
    pages = 0
    run = None
    for a in range(b, b + n + PAGE, PAGE):
        data = img[a:a + PAGE] if a < b + n and a != skip else None
        if data is not None and data != b"\xFF" * PAGE:
            run = a if run is None else run
            continue
        if run is not None:
            fl.program(run, img[run:a])
            pages += (a - run) // PAGE
            run = None
    return pages


def plan(fl, img):
    """{region name: [differing block addresses]} (marker block added where its region differs)."""
    out = {}
    for r in REGIONS:
        diff = [b for b, n in blocks(r["lo"], r["hi"]) if fl.crc(b, n) != crc(img[b:b + n])]
        if diff:
            inv = block_of(r["marker1"])
            if inv not in diff:
                diff.append(inv)
            out[r["name"]] = sorted(diff)
    return out


def write(fl, img, todo=None, progress=None):
    """Write the differing blocks in the order of spec 16 section 5. Returns pages programmed."""
    say = progress or (lambda s: None)
    todo = plan(fl, img) if todo is None else todo
    regs = [r for r in REGIONS if r["name"] in todo]
    pages = 0
    for r in sorted(regs, key=lambda r: r["name"] != "application"):      # application first
        inv = block_of(r["marker1"])
        say(f"invalidate {r['name']}: erase {inv:#08x}")
        fl.erase(inv, block_size(inv))
    for r in regs:
        inv = block_of(r["marker1"])
        for b in todo[r["name"]]:
            if b == inv:
                continue
            n = block_size(b)
            say(f"{r['name']}: block {b:#08x}")
            fl.erase(b, n)
            pages += program_block(fl, img, b, n)
            if fl.crc(b, n) != crc(img[b:b + n]):
                raise FlashError(f"verify failed for block {b:#x}")
        n = block_size(inv)
        say(f"{r['name']}: marker block {inv:#08x} without marker 1")
        pages += program_block(fl, img, inv, n, skip=r["marker1"])
        expect = bytearray(img[inv:inv + n])
        expect[r["marker1"] - inv:r["marker1"] - inv + PAGE] = b"\xFF" * PAGE
        if fl.crc(inv, n) != crc(bytes(expect)):
            raise FlashError(f"verify failed for marker block {inv:#x}")
    for r in sorted(regs, key=lambda r: r["name"] == "application"):      # application last
        for b, n in blocks(r["lo"], r["hi"]):
            want = bytearray(img[b:b + n])
            if b == block_of(r["marker1"]):
                want[r["marker1"] - b:r["marker1"] - b + PAGE] = b"\xFF" * PAGE
            if b in todo[r["name"]] and fl.crc(b, n) != crc(bytes(want)):
                raise FlashError(f"final check failed for block {b:#x}")
        say(f"commit {r['name']}: marker 1 at {r['marker1']:#08x}")
        fl.program(r["marker1"], img[r["marker1"]:r["marker1"] + PAGE])
        pages += 1
        inv = block_of(r["marker1"])
        if fl.crc(inv, block_size(inv)) != crc(img[inv:inv + block_size(inv)]):
            raise FlashError(f"verify failed after marker 1 of the {r['name']}")
    return pages

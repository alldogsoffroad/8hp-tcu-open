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

    def ident(self, kinds=("FL",)):
        """Flasher identification; kinds: "FL" (code-flash RAM flasher, spec 16) and/or "LD"
        (data-flash loader, spec 18). None if something else answered."""
        r = self.uds.request(b"\x22\xF1\x95", timeout=1.0)
        if not r or len(r) < 12 or r[:3] != b"\x62\xF1\x95" or r[3:5].decode("latin-1") not in kinds:
            return None
        return {"kind": r[3:5].decode(), "version": r[5], "fpmon": r[6], "fstatr0": r[7],
                "fstatr1": r[8], "fastat": r[9], "fpestat": r[10], "last_error": r[11],
                "fwe": bool(r[6] & 0x80)}

    def crc(self, a, n):
        r = self.req(b"\x31\x01\xFF\x01" + a.to_bytes(4, "big") + n.to_bytes(4, "big"), timeout=10.0, ok=0x71)
        return int.from_bytes(r[4:8], "big")

    def erase(self, a, n):
        self.req(b"\x31\x01\xFF\x00" + a.to_bytes(4, "big") + n.to_bytes(4, "big"), timeout=10.0, ok=0x71)

    def program(self, a, data):
        """Download data at a; the page size comes from the 0x74 reply (max block length - 2):
        256 for code flash, 128 for data flash."""
        r = self.req(b"\x34\x00\x44" + a.to_bytes(4, "big") + len(data).to_bytes(4, "big"), ok=0x74)
        page = (int.from_bytes(r[2:4], "big") - 2) if len(r) >= 4 and r[1] == 0x20 else PAGE
        assert a % page == 0 and len(data) % page == 0 and data
        for i in range(0, len(data), page):
            ctr = (i // page + 1) & 0xFF
            self.req(bytes([0x36, ctr]) + data[i:i + page], ok=0x76)
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


# ----------------------------------------------------------------------------- data-flash image (spec 18)

DF_BASE = 0x80100000
DF_BLOCK = 0x2000
DF_APP_LEN = 0x10000               # DB00-DB07; DB08-DB15 keep the stock EEPROM sections
DF_HDR = 0x80
DF_MAGIC = b"8HPAPP01"
DF_LOAD = 0xFFF84100
DF_RAM_LIMIT = 0xFFF9C000


def df_image_check(img):
    """Reasons a data-flash image (header + payload, from build_app_image.py) must not be written."""
    if len(img) < DF_HDR + 0x80 or img[:8] != DF_MAGIC:
        return ["not a data-flash application image (magic 8HPAPP01 missing)"]
    ln, load, entry, c = (int.from_bytes(img[o:o + 4], "big") for o in (8, 12, 16, 20))
    bad = []
    if ln != len(img) - DF_HDR or ln % 128 or ln > DF_APP_LEN - DF_HDR:
        bad.append(f"payload length {ln:#x} inconsistent or larger than {DF_APP_LEN - DF_HDR:#x}")
    if load != DF_LOAD or load + ln > DF_RAM_LIMIT:
        bad.append(f"load address {load:#x} is not {DF_LOAD:#x}")
    if not load <= entry < load + ln:
        bad.append(f"entry {entry:#x} outside the payload")
    if crc(img[DF_HDR:]) != c:
        bad.append("payload CRC does not match the header")
    return bad


def df_image_valid(mem):
    """What the loader decides for a data-flash content (bytes from DF_BASE): True = it runs it.
    Mirrors ld_image_parse() + the payload CRC (firmware/loader)."""
    if mem[:8] != DF_MAGIC:
        return False
    ln, load, entry, c = (int.from_bytes(mem[o:o + 4], "big") for o in (8, 12, 16, 20))
    if ln == 0 or ln > DF_APP_LEN - DF_HDR or ln % 128 or load != DF_LOAD or load + ln > DF_RAM_LIMIT:
        return False
    if not load <= entry < load + ln or entry & 1:
        return False
    return crc(mem[DF_HDR:DF_HDR + ln]) == c


def df_write(fl, img, progress=None):
    """Write an application image to data flash, power-loss safe (spec 18 section 4):
    erase DB00 first (the old header goes), the other blocks, payload, verify, header last."""
    say = progress or (lambda s: None)
    nblk = -(-len(img) // DF_BLOCK)
    for i in range(nblk):
        a = DF_BASE + i * DF_BLOCK
        say(f"erase DB{i:02d}")
        fl.erase(a, DF_BLOCK)
    say(f"program payload ({len(img) - DF_HDR} bytes)")
    fl.program(DF_BASE + DF_HDR, img[DF_HDR:])
    if fl.crc(DF_BASE + DF_HDR, len(img) - DF_HDR) != crc(img[DF_HDR:]):
        raise FlashError("payload verify failed (header not written: the loader stays in loader mode)")
    say("program header")
    fl.program(DF_BASE, img[:DF_HDR])
    if fl.crc(DF_BASE, len(img)) != crc(img):
        raise FlashError("verify after the header failed")
    return len(img)

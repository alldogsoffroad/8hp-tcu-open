#!/usr/bin/env python3
"""gen_fr_cal.py — THE FAULT-REACTION SHADOW: emit firmware/app/fr_cal.c, the
const copies of the stock calibration the read-only fault-reaction / gear-code
shadow (app/fr_shadow.c, app/fr_fm.c) uses. Every object carries its stock
flash address; the host test fr_cal_bytes (tests/unit/test_fr_shadow.c --cal)
re-reads the image and compares byte for byte / value for value.

Roots (~/tcu_sweep/f0_fault_chain_decode.md, can_inputs_decode.md,
gear_code_decode.md, fault_manager_decode.md; each indirection is the stock's
own, read in the bytes here and asserted):
  0x4F1D0      the 195 DSM group-config pointers -> 0x1923EC + 36 (g-1)
  0xFFF898AC   (dump) -> 0x4F1D0 + 804 -> 0x1955A8: the dependency table,
               3 u16 masters per group (0x9CDA8 via 0xAA786)
  0x642BC      the comm-monitor cfg: supply min / max, the U0001 / U11E4 /
               U0074 / group-137 thresholds, the U0001 / U0074 masks, the
               reload / threshold / mask tables (4-byte header + 53
               entries), the b2 enables
  0x4BCE0      0x1513F8's 53 status-getter slots -> movi20 stubs -> the RX
               status byte 0xFFF8400C + 0x10 k -> the RX ROM object
               (0x415C8 + 0x50 h, +0x10 = state = status - 8) -> timeout
               (+4), DLC (+0xC), the enable / tolerance getters (+0x34 /
               +0x38 / +0x3C / +0x40: movi20 0xFFF90AB8; mov.l @(pc) table;
               *(table + 0) = the cal byte) and the CRC / counter check
               pointers (+0x18 / +0x1C); the ID from the mailbox list
               0x44120 [8 + (state - 0xFFF84024) / 0x10] (bus 1) / 0x4538C
               (bus 2, handles 0x43..0x45)
  0x664D8      the counter-step table [0xFFF90AB8 = 0] -> 0x1918FE (= 3)
  0x64450      0x13A9D4 / 0x13A616's cal pointer table (obj+0xC)
  0x64CC8      0x14E0FC's cfg (+0x28 the timer reload)
  0x6344E      0xDBCA8's code table (54 B)
  0x632A6      the reaction-id identity table (the ids the shadow tests)
  dump         0xFFF85289 (3 B per comm entry: group, bus, slot — RAM,
               built at init by 0x15A458) and the 0x13A9D4 object's
               subscription words (0xFFF95858 +0 / +8) — checked against the
               dump file by the host test

usage: gen_fr_cal.py <stock image> <driving dump> <out .c>
"""
import struct
import sys

NG = 195
NE = 53


def main():
    img = open(sys.argv[1], 'rb').read()
    dmp = open(sys.argv[2], 'rb').read()
    u32 = lambda a: struct.unpack('>I', img[a:a + 4])[0]
    u16 = lambda a: struct.unpack('>H', img[a:a + 2])[0]
    d32 = lambda a: struct.unpack('>I', dmp[a - 0xFFF80000:a - 0xFFF7FFFC])[0]

    # ── the group configs ────────────────────────────────────────────────
    GRP = 0x1923EC
    for i in range(NG):
        assert u32(0x4F1D0 + 4 * i) == GRP + 36 * i, i
    assert d32(0xFFF898AC) == 0x4F1D0
    DEP = u32(0x4F1D0 + 804)
    assert DEP == 0x1955A8, hex(DEP)
    # only groups 97 / 149 / 150 carry masters (f0 §4.1)
    deps = {}
    for g in range(1, NG + 1):
        m = [u16(DEP + 6 * (g - 1) + 2 * k) for k in range(3)]
        if any(m):
            deps[g] = m
    assert set(deps) == {97, 149, 150}, deps

    # ── the comm-monitor cfg ─────────────────────────────────────────────
    CC = 0x642BC
    cp = [u32(CC + 4 * k) for k in range(12)]
    assert cp == [0x195EBE, 0x195EC0, 0x195EC2, 0x195EC4, 0x195D76, 0x195EC6, 0x195EC8, 0x195D77,
                  0x191B2C, 0x191BB0, 0x191C34, 0x195D78], [hex(x) for x in cp]
    for t in (0x191B2C, 0x191BB0, 0x191C34):
        assert img[t:t + 4] == b'\x00\x00\x00\x40', hex(t)
    reload = [u16(0x191B2C + 4 + 2 * i) for i in range(NE)]
    thr = [u16(0x191BB0 + 4 + 2 * i) for i in range(NE)]
    mask = [img[0x191C34 + 4 + i] for i in range(NE)]
    ent = [tuple(dmp[0x5289 + 3 * i:0x5289 + 3 * i + 3]) for i in range(NE)]

    # ── the RX objects behind the 53 entries ─────────────────────────────
    def movi20(a):
        w0, w1 = u16(a), u16(a + 2)
        assert (w0 & 0xF00F) == 0x0000, hex(a)          # 0000 nnnn iiii 0000: movi20
        imm = (((w0 >> 4) & 0xF) << 16) | w1
        if imm & 0x80000:
            imm -= 0x100000
        return imm & 0xFFFFFFFF

    def getter_cal(g):
        # movi20 #0xFFF90AB8,r1; mov.l @r1,r5; mov.l @(disp,pc),r2; add r5,r2;
        # mov.l @r2,r2; movu.b @(0,r2),r6; rtv/n r6
        assert movi20(g) == 0xFFF90AB8, hex(g)
        assert u16(g + 4) == 0x6512, hex(g)
        w = u16(g + 6)
        assert (w & 0xF000) == 0xD000, hex(g)
        pool = ((g + 6 + 4) & ~3) + 4 * (w & 0xFF)
        table = u32(pool)
        cal = u32(table)                 # index 0xFFF90AB8 = 0
        return cal, img[cal]

    objs = {}
    for h in range(0x46):
        o = 0x415C8 + 0x50 * h
        objs[u32(o + 0x10)] = (h, o)
    bus1_list = [u32(0x44120 + 4 * k) for k in range(80)]
    bus2_list = [u32(0x4538C + 4 * k) for k in range(3)]
    rx = []
    for n in range(NE):
        gaddr = u32(0x4BCE0 + 4 * n)
        st = movi20(gaddr)
        assert u16(gaddr + 4) == 0x000B and u16(gaddr + 6) == 0x6060, hex(gaddr)
        h, o = objs[st - 8]
        bus = 2 if h >= 0x43 else 1
        if bus == 1:
            ident = bus1_list[8 + ((st - 8) - 0xFFF84024) // 0x10]
        else:
            ident = bus2_list[h - 0x43]
        to = u16(o + 4)
        dlc = img[o + 0xC]
        en_a, en = getter_cal(u32(o + 0x34))
        crcf, ctrf, dlcf = u32(o + 0x18), u32(o + 0x1C), u32(o + 0x20)
        tol = []
        for k, f in ((0x38, crcf), (0x3C, ctrf), (0x40, dlcf)):
            if f:
                a, v = getter_cal(u32(o + k))
                tol.append((a, v))
            else:
                tol.append((0, 0))
        rx.append(dict(n=n, st=st, h=h, o=o, id=ident, bus=bus, to=to, dlc=dlc, en=en, en_a=en_a,
                       crc=1 if crcf else 0, ctr=1 if ctrf else 0, tol=tol))
    # cross-checks against the decode's corrected map (can_inputs §4a)
    want = {1: 0x108, 2: 0x2F8, 11: 0x140, 15: 0x112, 24: 0x14B, 30: 0x2EA, 52: 0x210, 22: 0x310}
    for n, i in want.items():
        assert rx[n]['id'] == i, (n, hex(rx[n]['id']))
    STEP_TAB = 0x664D8
    step_a = u32(STEP_TAB)
    assert step_a == 0x1918FE and img[step_a] == 3, hex(step_a)

    # ── 0x13A9D4 / 0x13A616 cal ──────────────────────────────────────────
    C13 = 0x64450
    p13 = [u32(C13 + 4 * k) for k in range(33)]
    v13w = [u16(p13[k]) for k in range(26)]          # +0 .. +0x64: u16
    v13b = [img[p13[k]] for k in range(26, 32)]      # +0x68 .. +0x7C: u8 flags
    v13x = u16(p13[32])                               # +0x80
    assert v13w[:2] == [20, 20] and v13w[2:8] == [600, 1670, 2440, 3080, 4040, 5250], v13w
    obj13 = 0xFFF95858
    sub0, sub1 = d32(obj13), d32(obj13 + 8)
    assert (sub0, sub1) == (0xFE002000, 3) and d32(obj13 + 0xC) == C13
    # ── 0x14E0FC ─────────────────────────────────────────────────────────
    assert d32(0xFFF95838) == 0x64CC8
    c14_a = u32(0x64CC8 + 0x28)
    c14 = u16(c14_a)
    # ── 0xDBCA8's table, the reaction ids ────────────────────────────────
    T6344E = img[0x6344E:0x6344E + 54]
    IDS = [0x0D, 0x16, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x2B, 0x32, 0x38, 0x39]
    for i in IDS:
        assert img[0x632A6 + i] == i, hex(i)

    o = []
    o.append('/* fr_cal.c — GENERATED by tools/gen_fr_cal.py from the stock image (and the driving')
    o.append(' * dump for the RAM-built tables); do not edit. The fault-reaction shadow\'s calibration')
    o.append(' * (app/fr_cal.h); byte-checked by tests/unit/test_fr_shadow.c --cal (ctest fr_cal_bytes). */')
    o.append('#define TCU_FR_SHADOW_TU 1')
    o.append('#include "app/fr_cal.h"')
    o.append('')
    o.append('/* the mutation negative (ctest fr_shadow_neg_CFG): group 136\'s fail time')
    o.append(' * 0x01C2 (450 ms) -> 0x01C4 — the U11E4 confirm must move one pass */')
    o.append('#if defined(FR_NEG_CFG)')
    o.append('#define FR_NEG_B(v) ((v) + 2u)')
    o.append('#else')
    o.append('#define FR_NEG_B(v) (v)')
    o.append('#endif')
    o.append('')
    o.append('/* 0x1923EC: the 195 DSM group configs, 36 B each (pointer cell 0x4F1D0) */')
    o.append('const uint8_t fr_cal_grp[FR_NG][36] = {')
    for i in range(NG):
        b = img[GRP + 36 * i:GRP + 36 * i + 36]
        vals = ['%du' % x for x in b]
        if i == 135:
            assert b[2:4] == b'\x01\xc2'
            vals[3] = 'FR_NEG_B(194u)'
        o.append('    { %s }, /* %d */' % (', '.join(vals), i + 1))
    o.append('};')
    o.append('')
    o.append('/* 0x1955A8: the dependency masters, 3 per group (0x9CDA8) */')
    o.append('const uint16_t fr_cal_dep[FR_NG][3] = {')
    for g in range(1, NG + 1):
        m = [u16(DEP + 6 * (g - 1) + 2 * k) for k in range(3)]
        o.append('    { %du, %du, %du },' % tuple(m))
    o.append('};')
    o.append('')
    o.append('/* 0x642BC: the comm-monitor cfg (values at 0x195EBE..C8, 0x195D76..78) */')
    o.append('const fr_cal_comm_t fr_cal_comm = {')
    o.append('    %du, %du, %du, %du, %du, %du, %du, %du, %du,' % (
        u16(0x195EBE), u16(0x195EC0), u16(0x195EC2), u16(0x195EC4), u16(0x195EC6), u16(0x195EC8),
        img[0x195D76], img[0x195D77], img[0x195D78]))
    o.append('};')
    o.append('/* 0x191B2C / 0x191BB0 / 0x191C34 (+4): the per-entry reload, threshold (u16, x 10 ms)')
    o.append(' * and mask (u8) */')
    o.append('const uint16_t fr_cal_reload[FR_NE] = { %s };' % ', '.join('%du' % v for v in reload))
    o.append('const uint16_t fr_cal_thr[FR_NE] = { %s };' % ', '.join('%du' % v for v in thr))
    o.append('const uint8_t  fr_cal_mask[FR_NE] = { %s };' % ', '.join('%du' % v for v in mask))
    o.append('/* RAM 0xFFF85289 (the driving dump; built by 0x15A458): group, bus, slot per entry */')
    o.append('const uint8_t  fr_cal_ent[FR_NE][3] = {')
    for i in range(0, NE, 8):
        o.append('    %s,' % ', '.join('{ %du, %du, %du }' % e for e in ent[i:i + 8]))
    o.append('};')
    slot = [0] * 13
    for (g, b, sl) in ent:
        if 1 <= sl <= 12 and slot[sl] == 0:
            slot[sl] = g
        assert slot[sl] == g
    o.append('/* the slot -> group map of 0xFFF85289 (0x13661C reports slot by slot) */')
    o.append('const uint8_t  fr_cal_slot_grp[13] = { %s };' % ', '.join('%du' % v for v in slot))
    o.append('')
    o.append('/* the RX objects behind the 53 comm entries (0x4BCE0 -> status 0xFFF8400C + 0x10 k ->')
    o.append(' * ROM object 0x415C8 + 0x50 h): id, bus, DLC, timeout (x 10 ms), the coding byte, the')
    o.append(' * CRC / counter checks present, the three tolerances; the status byte address */')
    o.append('const fr_cal_rx_t fr_cal_rx[FR_NE] = {')
    for r in rx:
        o.append('    { 0x%03Xu, %du, %du, %du, 0x%02Xu, %du, %du, %du, %du, %du, 0x%02Xu, 0x%08Xu, 0x%05Xu, 0x%06Xu }, /* %d */' % (
            r['id'], r['bus'], r['dlc'], r['to'], r['en'], r['crc'], r['ctr'],
            r['tol'][0][1], r['tol'][1][1], r['tol'][2][1], r['h'], r['st'], r['o'], r['en_a'], r['n']))
    o.append('};')
    o.append('const uint8_t fr_cal_ctr_step = %du;   /* 0x1918FE (via 0x664D8[0]) */' % img[step_a])
    o.append('')
    o.append('/* 0x64450 (0xFFF95858 +0xC): 0x13A9D4 / 0x13A616\'s cal, value by value */')
    o.append('const fr_cal_13a_t fr_cal_13a = {')
    o.append('    { %s },' % ', '.join('%du' % v for v in v13w))
    o.append('    { %s },' % ', '.join('%du' % v for v in v13b))
    o.append('    %du, 0x%08Xu, 0x%08Xu,' % (v13x, sub0, sub1))
    o.append('};')
    o.append('const uint32_t fr_cal_13a_ptr[33] = { %s };' % ', '.join('0x%06Xu' % v for v in p13))
    o.append('/* 0x195FCA (0x64CC8 +0x28): 0x14E0FC\'s timer reload */')
    o.append('const uint16_t fr_cal_14e_reload = %du;' % c14)
    o.append('/* 0x6344E: 0xDBCA8\'s code table */')
    o.append('const uint8_t fr_cal_6344e[54] = { %s };' % ', '.join('%du' % b for b in T6344E))
    o.append('')
    # the scalar byte-check list (address, width, value) for the derived values
    sc = []
    for r in rx:
        sc.append((r['o'] + 4, 2, r['to']))
        sc.append((r['o'] + 0xC, 1, r['dlc']))
        sc.append((r['en_a'], 1, r['en']))
        for (a, v) in r['tol']:
            if a:
                sc.append((a, 1, v))
    for k in range(26):
        sc.append((p13[k], 2, v13w[k]))
    for k in range(6):
        sc.append((p13[26 + k], 1, v13b[k]))
    sc.append((p13[32], 2, v13x))
    sc.append((c14_a, 2, c14))
    sc.append((step_a, 1, img[step_a]))
    for i in IDS:
        sc.append((0x632A6 + i, 1, i))
    o.append('/* the scalar byte checks (tests/unit/test_fr_shadow.c --cal) */')
    o.append('const fr_cal_chk_t fr_cal_chk[FR_CAL_NCHK] = {')
    for (a, w, v) in sc:
        o.append('    { 0x%06Xu, %du, %du },' % (a, w, v))
    o.append('};')
    o.append('')
    open(sys.argv[3], 'w').write('\n'.join(o) + '\n')
    print('fr_cal: %d groups, %d entries, %d scalar checks' % (NG, NE, len(sc)))
    print('FR_CAL_NCHK %d' % len(sc))


if __name__ == '__main__':
    main()

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
    o.append(' * (app/fr_cal.h); byte-checked by tests/unit/test_fr_shadow.c --cal (ctest fr_cal_bytes).')
    o.append(' * The group configs, the dependency masters and the reload / threshold / mask tables are NOT')
    o.append(' * copied: they are read in place (app/cal_inplace.h). */')
    o.append('#define TCU_FR_SHADOW_TU 1')
    o.append('#include "app/fr_cal.h"')
    o.append('')
    # the group configs (0x1923EC), the dependency masters (0x1955A8) and the comm entries'
    # reload / threshold / mask tables (0x191B30 / 0x191BB4 / 0x191C38) are read IN PLACE
    # (app/cal_inplace.h, 2026-11-20): this script only asserts their addresses.
    assert GRP == 0x1923EC and DEP == 0x1955A8
    assert 0x191B2C + 4 == 0x191B30 and 0x191BB0 + 4 == 0x191BB4 and 0x191C34 + 4 == 0x191C38
    o.append('/* 0x642BC: the comm-monitor cfg (values at 0x195EBE..C8, 0x195D76..78) */')
    o.append('const fr_cal_comm_t fr_cal_comm = {')
    o.append('    %du, %du, %du, %du, %du, %du, %du, %du, %du,' % (
        u16(0x195EBE), u16(0x195EC0), u16(0x195EC2), u16(0x195EC4), u16(0x195EC6), u16(0x195EC8),
        img[0x195D76], img[0x195D77], img[0x195D78]))
    o.append('};')
    # OPTION C (2026-11-23): the Nissan build (TCU_VEHICLE_NISSAN) binds the comm monitor to the
    # Nissan messages: its own hand-written tables in app/fr_cal_nissan.c replace these three; the
    # FCA preprocessed text (and so the FCA image) is unchanged
    o.append('#if !defined(TCU_VEHICLE_NISSAN)   /* the Nissan tables: app/fr_cal_nissan.c (option C) */')
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
    o.append('#endif /* !TCU_VEHICLE_NISSAN */')
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
    # ── group 27 / 28: the TOE32 (k = 3, PL7) off/on-state test (2026-11-20,
    # ~/tcu_sweep/group28_decode.md, group28_port.md): 0x849D4's gate on
    # 0x8B284(k), 0x8B5DE's gate, 0x8B284's thresholds, 0x7B358's pair table,
    # 0x82D4C's code table, 0x9140A's conversion — every pool / table pointer
    # asserted ([0xFFF90AB8 = 0]: the first slot of each cal-bank table)
    K = 3

    def bank(pool):                 # pool -> pointer table -> [dataset 0] -> the cal
        return u32(u32(pool))
    assert u32(0x84BAC) == 0x91350 and u32(0x84BBC) == 0x8B284 and u32(0x914E0) == 0x1918E6
    g_fs = 0x1918E6                 # 0x91350's mask
    g_en = bank(0x84BB0)            # 0x849D4's enable byte
    assert g_en == 0x1919B4, hex(g_en)
    g_src = bank(0x8B6D8) + K       # the 0x8B5DE source byte (FE = battery)
    assert bank(0x8B6D8) == 0x1919BE and bank(0x84BB4) == 0x1919BE
    g_2d = bank(0x8B6F8) + K        # 0x9482D's reload
    g_28 = bank(0x8B798) + K        # 0x94828's reload
    g_win = bank(0x8B79C)           # the pre-'L' window mask (0x95005)
    g_slo = bank(0x8B7A0) + 2 * K   # the supply window
    g_shi = bank(0x8B7A4) + 2 * K
    g_fe = bank(0x8B700)            # source FE's threshold (0xFFF94F86)
    g_lo = bank(0x8B4A8) + 2 * K    # 0x8B284's thresholds
    g_hi = bank(0x8B4AC) + 2 * K
    g_on = bank(0x8B4B0) + 2 * K
    g_dmin = u32(0x8B4A0) + 2 * K   # (direct pointers, no bank table)
    g_dmax = u32(0x8B4A4) + 2 * K
    g_cut = bank(0x8B4B4)           # the d >= 1000 cut-test mask
    g_cvt = bank(0x8B6D4)           # the 0x08 / 0x10 -> 0x20 mask
    g_and = bank(0x8B6E4) + 2 * K   # the mask AND
    assert (g_2d, g_28, g_win, g_slo, g_shi, g_fe, g_lo, g_hi, g_on, g_dmin, g_dmax, g_cut, g_cvt, g_and) == (
        0x191A14, 0x191A0F, 0x1919B5, 0x19185A, 0x191850, 0x191566, 0x191878, 0x191864, 0x19186E,
        0x1914BE, 0x1914B4, 0x191A09, 0x191A0A, 0x191846), 'g28 cal addresses'
    assert u32(0x8B6E8) == 0x7BABA and u32(0x8B6F4) == 0x87120 and u32(0x8B6E0) == 0x86414
    assert u32(0x7B4D8) == 0x45C84 and u32(0x7B4E8) == 0x45CD4
    PT = 0x45C84 + 16 * K           # 0x7B358's pair table entry for k = 3
    assert u32(PT) == 0xFFF940BF and u32(PT + 4) == 0xFFF940BE and u32(PT + 8) == 0xFFF938CC
    g27, g28 = img[PT + 12], img[PT + 13]
    assert (g27, g28) == (27, 28), (g27, g28)
    ctab = [(u16(0x45CD4 + 4 * i), u16(0x45CD4 + 4 * i + 2)) for i in range(7)]
    assert ctab[-1] == (0, 1) and (0x10, 0x84) in ctab and (0x04, 0x82) in ctab, ctab
    CT = 0x40A34 + 16 * K           # 0x9140A's conversion entry for k = 3
    assert (u32(CT), u32(CT + 4), u32(CT + 8), u32(CT + 12)) == (0xFFF90578, 0x3EC35000, 0xFFF908A2, 0xFFF908AC)
    seq = img[0x40ECC:0x40ECC + 80]
    assert seq[35] == seq[40] == 0x40, (seq[35], seq[40])    # both slots: mux input 4
    g28v = dict(fs=img[g_fs], en=img[g_en], src=img[g_src], r2d=img[g_2d], r28=img[g_28], win=img[g_win],
             slo=u16(g_slo), shi=u16(g_shi), fe=u16(g_fe), lo=u16(g_lo), hi=u16(g_hi), on=u16(g_on),
             dmin=u16(g_dmin), dmax=u16(g_dmax), cut=img[g_cut], cvt=img[g_cvt], andm=u16(g_and))
    assert g28v == dict(fs=0x1F, en=0x0B, src=0xFE, r2d=10, r28=4, win=0x0F, slo=6500, shi=23500, fe=6700,
                     lo=2400, hi=4000, on=3500, dmin=50, dmax=950, cut=0x17, cvt=0x00, andm=0xF7FF), g28v
    o.append('/* group 27 / 28 (P1D8D / P1DCA / P1D8E): the TOE32 (k = 3, pin PL7) off / on-state test:')
    o.append(' * 0x849D4 -> 0x8B284(3) / 0x8B5DE(3), the reporter 0x7B358(3) (pair table 0x45C84 + 48), the')
    o.append(' * code table 0x45CD4 (0x82D4C), the conversion 0x9140A (0x40A34 + 48: scale 0x3EC35000).')
    o.append(' * The mutation negative (ctest fr_shadow_neg_G28): hi 4000 -> 3400 (below the')
    o.append(' * bench\'s 3436 mV: the open band empties, group 28 never fails). */')
    o.append('#if defined(FR_NEG_G28)')
    o.append('#define FR_NEG_HI(v) (3400u)')
    o.append('#else')
    o.append('#define FR_NEG_HI(v) (v)')
    o.append('#endif')
    o.append('const fr_cal_g28_t fr_cal_g28 = {')
    o.append('    0x%02Xu, 0x%02Xu, 0x%02Xu, %du, %du, 0x%02Xu,  /* fs 0x1918E6, en 0x1919B4, src 0x%06X, '
             'r2d 0x%06X, r28 0x%06X, win 0x1919B5 */' % (g28v['fs'], g28v['en'], g28v['src'], g28v['r2d'], g28v['r28'],
                                                           g28v['win'], g_src, g_2d, g_28))
    o.append('    %du, %du, %du,  /* the supply window 0x%06X / 0x%06X, source FE\'s threshold 0x191566 */'
             % (g28v['slo'], g28v['shi'], g28v['fe'], g_slo, g_shi))
    o.append('    %du, FR_NEG_HI(%du), %du, %du, %du,  /* lo 0x%06X hi 0x%06X on 0x%06X dmin 0x%06X dmax 0x%06X */'
             % (g28v['lo'], g28v['hi'], g28v['on'], g28v['dmin'], g28v['dmax'], g_lo, g_hi, g_on, g_dmin, g_dmax))
    o.append('    0x%02Xu, 0x%02Xu, 0x%04Xu,  /* cut 0x191A09, cvt 0x191A0A, and 0x%06X */'
             % (g28v['cut'], g28v['cvt'], g28v['andm'], g_and))
    o.append('    %du, %du,  /* the groups (0x45C84 + 48 + 12 / 13) */' % (g27, g28))
    o.append('    { %s },  /* 0x45CD4: mask, code */' % ', '.join('{ 0x%04Xu, 0x%02Xu }' % e for e in ctab))
    o.append('};')
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
    for (a, w, val) in ((g_fs, 1, g28v['fs']), (g_en, 1, g28v['en']), (g_src, 1, g28v['src']), (g_2d, 1, g28v['r2d']),
                        (g_28, 1, g28v['r28']), (g_win, 1, g28v['win']), (g_slo, 2, g28v['slo']), (g_shi, 2, g28v['shi']),
                        (g_fe, 2, g28v['fe']), (g_lo, 2, g28v['lo']), (g_hi, 2, g28v['hi']), (g_on, 2, g28v['on']),
                        (g_dmin, 2, g28v['dmin']), (g_dmax, 2, g28v['dmax']), (g_cut, 1, g28v['cut']),
                        (g_cvt, 1, g28v['cvt']), (g_and, 2, g28v['andm']), (PT + 12, 1, g27), (PT + 13, 1, g28),
                        (0x40ECC + 35, 1, 0x40), (0x40ECC + 40, 1, 0x40)):
        sc.append((a, w, val))
    for i, (m, c) in enumerate(ctab):
        sc.append((0x45CD4 + 4 * i, 2, m))
        sc.append((0x45CD4 + 4 * i + 2, 2, c))
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

#!/usr/bin/env python3
"""gen_fb_cal.py — the PWM-FEEDBACK SHADOW: emit firmware/app/fb_cal.c, the
typed const copies of the stock calibration the read-only feedback-chain
shadow (app/fb_shadow.c) uses, each with its stock flash address so the host
test fb_cal_bytes can re-encode it big-endian and compare it with the image.

Roots (~/tcu_sweep/feedback_chain_decode.md, ~/tcu_sweep/feedback_shadow_port.md;
the indirections are the stock's own, each read in the bytes):
  0x40954  the two Timer F capture configs (0x97648 r5: +0 u32 no-capture
           limit, +4 u32 period limit, +8 u16 GRBF reload, +10 u8 polarity)
  0x40D96  the chip -> Timer F channel table (3 bytes per chip)
  0x190210 / 0x1902D0  the sampler interval tables (0x90844 state 1, row 0)
  0x191490 the expected period (0x8FC5E -> 0x9083C, 1000 us)
  0x66A14 -> 0x1916C4 (u16, the classifier margin)   0x89AE0..0x89B12
  0x66A0C -> 0x1919F3 (u8, the mixed-window hold)    0x89DF6..0x89E00
  0x66A10 -> 0x191294 (s16[10], the duty filter)     0x89E86..0x89E8C
  0x66974 [10] -> the diode-drop curves a             0x83944..0x83950
  0x6694C [10] -> the gain curves b                   0x83960..0x8396C
  0x669B8 [10] -> the driver-resistance curves        0x83AF2..0x83AFC
  0x669E0 [10] -> the self-heating curves             0x83B24..0x83B30
  0x669B4 -> 0x191140 (u16[10], R_fix)  0x669B0 -> 0x19112C (u16[10], R20)
  0x669AC -> 0x1916B8 (u16, I_nom)      0x669A4 -> 0x191118 (u16[10], split)
  0x6699C -> 0x1910F0 (u16[10], fast)   0x669A0 -> 0x191104 (u16[10], slow)
  0x669A8 -> 0x1916B6 (u16, ambient filter)
  0x65B31  the initialised-data image of 0xFFF84A21 (DTBL 0x415B0: ROM
           0x65110 -> RAM 0xFFF84000)

usage: gen_fb_cal.py <stock image> <out .c>
"""
import struct
import sys


def main():
    img = open(sys.argv[1], 'rb').read()
    u32 = lambda a: struct.unpack('>I', img[a:a + 4])[0]
    u16 = lambda a: struct.unpack('>H', img[a:a + 2])[0]
    s16 = lambda a: struct.unpack('>h', img[a:a + 2])[0]

    out = []
    checks = []          # (name, addr, kind, count)

    def emit(name, ctype, kind, addr, vals, comment):
        out.append('/* %s */' % comment)
        out.append('const %s %s[%d] = { %s };' % (ctype, name, len(vals),
                                                  ', '.join(fmt(kind, v) for v in vals)))
        checks.append((name, addr, kind, len(vals)))

    def fmt(kind, v):
        if kind == 'u32':
            return '0x%08Xu' % v
        if kind == 'u16':
            return '%du' % v
        if kind == 's16':
            return '%d' % v
        return '0x%02Xu' % v

    def curve(a):
        n = s16(a)
        return [s16(a + 2 * i) for i in range(1 + 2 * n)]

    # capture configs
    for s in range(2):
        a = 0x40954 + 12 * s
        emit('FB_CFG%d_LIM' % s, 'uint32_t', 'u32', a, [u32(a), u32(a + 4)],
             '0x%05X: sub %d no-capture limit, period limit (counts)' % (a, s))
        emit('FB_CFG%d_GRBF' % s, 'uint16_t', 'u16', a + 8, [u16(a + 8)],
             '0x%05X: sub %d GRBF reload' % (a + 8, s))
        emit('FB_CFG%d_POL' % s, 'uint8_t', 'u8', a + 10, [img[a + 10]],
             '0x%05X: sub %d polarity (1 rising)' % (a + 10, s))
    emit('FB_PINS', 'uint8_t', 'u8', 0x40D96, list(img[0x40D96:0x40D96 + 15]),
         '0x40D96: chip -> Timer F channel (sub 0, sub 1, group-B byte)')
    emit('FB_IV_190210', 'uint16_t', 'u16', 0x190210, [u16(0x190210 + 2 * i) for i in range(3)],
         '0x190210 row 0: (t1, t2, t3) for 0x9090B bit 5 set')
    emit('FB_IV_1902D0', 'uint16_t', 'u16', 0x1902D0, [u16(0x1902D0 + 2 * i) for i in range(3)],
         '0x1902D0 row 0: (t1, t2, t3) for 0x9090B bit 5 clear')
    emit('FB_PEXP', 'uint16_t', 'u16', 0x191490, [u16(0x191490)], '0x191490: expected period (us)')

    def ptr(p):
        q = u32(p)
        checks.append(('&' + hex(q), p, 'ptr', q))
        return q

    q = ptr(0x66A14)
    emit('FB_M', 'uint16_t', 'u16', q, [u16(q)], '*0x66A14 = 0x%06X: classifier margin cal' % q)
    q = ptr(0x66A0C)
    emit('FB_HOLD', 'uint8_t', 'u8', q, [img[q]], '*0x66A0C = 0x%06X: mixed-window hold' % q)
    q = ptr(0x66A10)
    emit('FB_COEF', 'int16_t', 's16', q, [s16(q + 2 * i) for i in range(10)],
         '*0x66A10 = 0x%06X: duty filter gain' % q)
    for name, p in (('FB_RFIX', 0x669B4), ('FB_R20', 0x669B0), ('FB_SPLIT', 0x669A4),
                    ('FB_KF', 0x6699C), ('FB_KS', 0x669A0)):
        q = ptr(p)
        emit(name, 'uint16_t', 'u16', q, [u16(q + 2 * i) for i in range(10)],
             '*0x%05X = 0x%06X' % (p, q))
    for name, p in (('FB_INOM', 0x669AC), ('FB_KA', 0x669A8)):
        q = ptr(p)
        emit(name, 'uint16_t', 'u16', q, [u16(q)], '*0x%05X = 0x%06X' % (p, q))

    # per-channel curve pointer tables
    for tname, p, n in (('A', 0x66974, 9), ('B', 0x6694C, 5), ('RDRV', 0x669B8, 9),
                        ('HEAT', 0x669E0, 7)):
        ptrs = [u32(p + 4 * i) for i in range(10)]
        emit('FB_PT_%s' % tname, 'uint32_t', 'u32', p, ptrs,
             '0x%05X: per-channel curve pointers' % p)
        for ch, q in enumerate(ptrs):
            c = curve(q)
            assert len(c) == n, (tname, ch, c)
            emit('FB_C_%s_%d' % (tname, ch), 'int16_t', 's16', q, c,
                 '0x%06X: curve %s ch %d (n, x.., y..)' % (q, tname, ch))
        out.append('const int16_t *const FB_C_%s[10] = { %s };' %
                   (tname, ', '.join('FB_C_%s_%d' % (tname, ch) for ch in range(10))))

    emit('FB_DTBL_84A21', 'uint8_t', 'u8', 0x65110 + 0xA21, [img[0x65110 + 0xA21]],
         '0x65B31: DTBL init byte of 0xFFF84A21')

    # the check table
    out.append('')
    out.append('const fb_cal_check_t FB_CAL_CHECKS[] = {')
    for (name, addr, kind, cnt) in checks:
        if kind == 'ptr':
            out.append('    { 0x%06Xu, FB_K_PTR, 0x%06Xu, 0 },' % (addr, cnt))
        else:
            k = {'u32': 'FB_K_U32', 'u16': 'FB_K_U16', 's16': 'FB_K_S16', 'u8': 'FB_K_U8'}[kind]
            out.append('    { 0x%06Xu, %s, %du, %s },' % (addr, k, cnt, name))
    out.append('};')
    out.append('const unsigned FB_CAL_NCHECKS = %du;' % len(checks))

    hdr = [
        '/* fb_cal.c — GENERATED by tools/gen_fb_cal.py from the pristine stock image',
        ' * (260924131233 1034420267_IFla.bin). Do not edit: regenerate.',
        ' * The calibration of the READ-ONLY PWM-feedback shadow (app/fb_shadow.c),',
        ' * typed, each with its stock flash address; the host test fb_cal_bytes',
        ' * re-encodes every object big-endian and compares it with the image. */',
        '#include "app/fb_cal.h"',
        '',
        '#if !defined(TCU_RAM_APP)',
    ]
    open(sys.argv[2], 'w').write('\n'.join(hdr + out + ['#endif', '']))
    print('fb_cal: %d objects' % len(checks))


if __name__ == '__main__':
    main()

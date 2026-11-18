/* pa7_shadow.c — the READ-ONLY shadow of the stock's PA7 / "TRS Park
 * signal" law (app/pa7_shadow.h; ~/tcu_sweep/pa7_decode.md, the port
 * ~/tcu_sweep/r04d_recorder_pa7_shadow.md). Every routine below is ported
 * from the stock bytes (addresses at each); every value lands in the
 * shadow's own struct (pa7s_t, section .pa7_shadow) — no stock cell, no
 * pin, no bus-B word. Its inputs arrive by value (app/pa7_hook.c).
 */
#define TCU_PA7_SHADOW_TU
#include "app/pa7_shadow.h"

#include <stdbool.h>

#if defined(TCU_RAM_APP)
/* the RAM application carries no shadow (its 64 KB limit: zero margin) */
void pa7_shadow_init(void) {}
void pa7_shadow_range(const pa7_rng_in_t *in) { (void)in; }
void pa7_shadow_pin(const pa7_pin_in_t *in) { (void)in; }
const volatile pa7s_t *pa7_shadow_state(void) { return 0; }
#else

#if defined(__sh__)
#define PA7_RAM __attribute__((section(".pa7_shadow")))
#else
#define PA7_RAM
#endif

static pa7s_t s_pa7 PA7_RAM;

#if defined(PA7_NEG_LAW)
/* negative control (host only): state 2 read as state 1 regardless of the
 * supervisor's bit 1, and state 0 as high — the law must FAIL the tests */
#define PA7_NEG 1
#endif

/* ════════════════════════════════════════════════════════════════════════
 * THE RANGE PART: 0xA6CF8 → 0xA6D12 (E567), 0xC6116 (D713), 0xC60CE (D712)
 * ════════════════════════════════════════════════════════════════════════ */

/* 0xA6D6E [O bytes 0xA6D6E–0xA6DF2]: the range code from D6D6 / the lever */
static uint8_t a6d6e(const pa7_rng_in_t *in)
{
    const uint8_t lev = in->d672;                       /* @(0,r15)  0xA7820 */
    const uint8_t src = in->d673;                       /* @(4,r15)  0xA7848 */
    const uint8_t d65e = in->d65e;                      /* @(8,r15) */
    bool pn;
    if (in->d6da == 2u) {                               /* 0xA7B8C */
        return 127u;
    }
    pn = (src == 2u) || (d65e != 0u) || (lev == 8u) || (lev == 6u);   /* 0xA6DAA.. */
    switch (in->d6d6) {                                 /* 0xA7B78 */
    case 8u: return pn ? 8u : 0u;
    case 6u: return pn ? 6u : 0u;
    case 7u: return 7u;
    case 5u: return 5u;
    default: return 127u;
    }
}

/* 0xA6D12 [O bytes 0xA6D12–0xA6D6C]: E567. The override (cal 0x1960B8 == 1
 * and 0xFFF83078 > 0 → 0) is off on this unit (PA7_CAL_E567_OVR, asserted);
 * 0xC627A / 0xC62F6 (its 0xFFF8E568 / 0xFFF83078 bookkeeping) not ported */
static uint8_t a6d12(uint8_t code)
{
    switch (code) {
    case 8u:   return 2u;
    case 6u:   return 1u;
    case 127u: return 3u;
    default:   return 0u;
    }
}

/* 0xC6116 [O bytes 0xC6116–0xC615C]: D713 */
static uint8_t c6116(const pa7_rng_in_t *in)
{
    if (in->d60a != 1u) {                               /* 0xA77AA */
        return 0u;
    }
    /* 0xA7B08: the byte 0xFFF81C6C > 0 → 1 — stood in by the dump's 00 */
    if (in->d5f9 == 1u) {                               /* 0xA7720 */
        return 1u;
    }
    if (in->d5f8 == 0u) {                               /* 0xA7716 */
        return 1u;
    }
    /* cmp/hs on the sign-extended words: (u32)D65A >= (u32)5000 → 0 */
    if ((uint32_t)(int32_t)in->d65a >= (uint32_t)(int32_t)(int16_t)PA7_CAL_D65A_MIN) {
        return 0u;
    }
    return 1u;
}

/* 0xC60CE [O bytes 0xC60CE–0xC6114]: D712 */
static uint8_t c60ce(uint8_t e567, uint8_t d713, const pa7_rng_in_t *in)
{
    if (d713 == 1u) {
        return 0u;
    }
    switch (e567) {
    case 2u:
        if (in->d5f8 == 1u) {
            return 2u;
        }
        return (in->d5f9 == 0u) ? 1u : 0u;              /* 0xC6100 */
    case 1u:
        return (in->d5f9 == 0u) ? 1u : 0u;
    case 3u:
        return 3u;
    default:
        return 0u;
    }
}

void pa7_shadow_range(const pa7_rng_in_t *in)
{
    s_pa7.rng_n++;
    s_pa7.rin = *in;
    s_pa7.a6d6e = a6d6e(in);
    s_pa7.e567 = a6d12(s_pa7.a6d6e);                            /* 0xA6D12 → E567 */
    s_pa7.d713 = c6116(in);                                 /* 0xC6116 → D713 */
    s_pa7.d712 = c60ce(s_pa7.e567, s_pa7.d713, in);                 /* 0xC60CE → D712 */
}

/* ════════════════════════════════════════════════════════════════════════
 * THE PIN-CUT PART: the countdown copy, 0x7ACF2, 0x86B0C → 0x8CCD4,
 * 0x8CE48, 0x8CB7C
 * ════════════════════════════════════════════════════════════════════════ */

static uint16_t cd16(uint16_t v, uint8_t n) { return (uint16_t)((v > n) ? (uint32_t)(v - n) : 0u); }
static uint8_t  cd8(uint8_t v, uint8_t n)   { return (uint8_t)((v > n) ? (uint32_t)(v - n) : 0u); }

/* 0x8CDDC [O bytes 0x8CDDC–0x8CE20]: the line check may run */
static bool c8cddc(const pa7_pin_in_t *in)
{
    /* 0x8CC76 / 0x8CC98: cal 0x191A18 != 0xFF → 0x94F86 / 0x94FEC */
    const int32_t sup = in->sup;
    if (in->sup_st != 8u) {
        return false;
    }
    if ((uint32_t)(int32_t)(int16_t)PA7_CAL_SUPMIN >= (uint32_t)sup) {   /* cmp/hs r5,r6 */
        return false;
    }
    return s_pa7.blank_b == 0u && s_pa7.blank_w == 0u;          /* 0x94834, 0x94784 */
}

/* 0x8CCD4 [O bytes 0x8CCD4–0x8CDDA]: the line check */
static void c8ccd4(const pa7_pin_in_t *in)
{
    uint16_t tested = 0u, fail = 0u;
    const bool valid = c8cddc(in);
    s_pa7.valid = valid ? 1u : 0u;
    if (valid) {
        const int32_t an = (int32_t)s_pa7.an7_f;           /* movu.w 0x93B2A */
        const bool exp_hi = (s_pa7.s9417c & 0x02u) != 0u;   /* tst #2 */
        bool chk_hi;                                    /* true: the > 9000 check (8CD10) */
        if (exp_hi) {
            chk_hi = (PA7_CAL_SENSE != 1u);
        } else {
            chk_hi = (PA7_CAL_SENSE == 1u);
        }
        if (chk_hi) {
            tested = 4u;                                /* delay slot: always */
            if (an > (int32_t)PA7_CAL_HIMV) {           /* cmp/gt (signed) */
                fail = 4u;
            }
        } else {
            tested = 8u;
            if (!(an >= (int32_t)PA7_CAL_LOMV)) {       /* cmp/ge */
                fail = 8u;
            }
        }
    }
    if (PA7_CAL_ENABLE != 0u) {                         /* 0x8CD36 */
        const uint8_t nib = in->nib;                    /* 0xFFF90958, live */
        const bool b3clr = (nib & 0x08u) == 0u;         /* bld #3; movrt r4 */
        uint8_t f = s_pa7.s9417c;
        if ((nib & 0x02u) == 0u) {
            f = (uint8_t)(f | 0x10u);                   /* 0x8CDB6 */
        } else {
            f = (uint8_t)(f & (uint8_t)~0x10u);         /* the bf.s delay slot */
            if (b3clr) {
                tested = (uint16_t)(tested | 0x200u);
                f = (uint8_t)(f | 0x20u);
            } else {
                tested = (uint16_t)(tested | 0x100u);
                f = (uint8_t)(f & (uint8_t)~0x20u);
            }
            if (s_pa7.pa7 == 1u) {                          /* 0x94120 */
                if (s_pa7.t_lo == 0u && !b3clr) {           /* 30 ms high and bit 3 set */
                    fail = (uint16_t)(fail | 0x100u);
                }
            } else if (s_pa7.pa7 == 0u) {
                if (s_pa7.t_hi == 0u && b3clr) {            /* 7 s low and bit 3 clear */
                    fail = (uint16_t)(fail | 0x200u);
                }
            }
        }
        s_pa7.s9417c = f;
    }
    tested = (uint16_t)(tested & PA7_CAL_MASK);         /* 0x19189C */
    s_pa7.fail = fail;                                      /* 0x93B2C */
    s_pa7.tested = tested;                                  /* 0x93B2E */
    s_pa7.report_n++;                                       /* 0x887C0(tested, fail): not ported */
    if ((fail & 0x0004u) != 0u) { s_pa7.f4_n++; }
    if ((fail & 0x0008u) != 0u) { s_pa7.f8_n++; }
    if ((fail & 0x0100u) != 0u) { s_pa7.f100_n++; }
    if ((fail & 0x0200u) != 0u) { s_pa7.f200_n++; }
}

/* 0x8CE48 [O bytes 0x8CE48–0x8CEC8]: the law (→ 0x90CF6, PADR bit 7) */
static void c8ce48(const pa7_pin_in_t *in)
{
    const bool b1clr = (in->nib & 0x02u) == 0u;         /* bld.b #1; movrt */
    uint8_t pa7 = 0u, c9 = 0u;
    if (PA7_CAL_ENABLE != 0u) {
        uint8_t st = s_pa7.s94122;
#if defined(PA7_NEG)
        if (st == 2u || st == 0u) {
            st = 1u;
        }
#endif
        if (st == 1u || (st == 2u && b1clr)) {          /* 0x8CE88: high */
            pa7 = 1u;
            c9 = 0u;
            s_pa7.t_hi = PA7_CAL_THI;                       /* 0x94766 */
        } else if (st == 0u || st == 2u) {              /* 0x8CE9C: low, the C9 request */
            pa7 = 0u;
            c9 = 1u;
            s_pa7.t_lo = PA7_CAL_TLO;                       /* 0x94768 */
        } else {                                        /* 0x8CE82 → 0x8CEA0 */
            pa7 = 0u;
            c9 = 0u;
            s_pa7.t_lo = PA7_CAL_TLO;
        }
    }
    s_pa7.c9b7 = c9;                                        /* 0x94121 + 0x9095A */
    s_pa7.pa7 = pa7;                                        /* 0x94120 + 0x90920 (0x90CF6) */
}

/* 0x8CB7C [O bytes 0x8CB7C–0x8CC74]: AN7, the nibble, the expected level */
static void c8cb7c(const pa7_pin_in_t *in)
{
    const int32_t an = in->an7;                         /* mov.w @(r0,r12) (sign-extended) */
    const uint32_t prev = (uint32_t)s_pa7.an7_f;           /* extu.w of 0x93B2A */
    const int32_t hi = (int32_t)(prev + PA7_CAL_STEP);
    const int32_t lo = (int32_t)prev - (int32_t)PA7_CAL_STEP;
    const int32_t a5 = (int32_t)(uint16_t)an;           /* extu.w r4 */
    uint8_t f = s_pa7.s9417c;
    uint8_t nibv = s_pa7.s9417b;                            /* the old latch (delay slot) */
    uint16_t keep = s_pa7.an7_f;
    s_pa7.an7_raw = (uint16_t)an;                           /* 0x93A94 */
    if (!(a5 > hi) && a5 >= lo) {
        nibv = in->nib;                                 /* 0x8CBE0: in band */
        keep = (uint16_t)an;
    } else if ((f & 0x08u) == 0u) {
        nibv = in->nib;                                 /* not initialised: accept */
        keep = (uint16_t)an;
    } else if ((f & 0x04u) != 0u) {
        nibv = in->nib;                                 /* the second out-of-band pass */
        keep = (uint16_t)an;
        f = (uint8_t)(f & (uint8_t)~0x04u);
    } else {
        f = (uint8_t)(f | 0x04u);                       /* hold one pass */
    }
    {
        bool hi_exp = false;
        if (nibv == 2u || nibv == 4u) {
            if (PA7_CAL_ENABLE == 0u) {
                hi_exp = true;
            } else if (PA7_CAL_ENABLE == 1u) {
                hi_exp = (s_pa7.c9b7 == 0u);                /* 0x94121 */
            }
        }
        {
            const uint8_t was = (uint8_t)(f & 0x02u);
            const uint8_t now = hi_exp ? 1u : 0u;
            if (was == 2u && now == 0u) {               /* going low: 750 ms blanking */
                s_pa7.blank_b = 0u;
                s_pa7.blank_w = PA7_CAL_BLANK_LO;
            } else if (was == 0u && now == 1u) {        /* going high: 50 ms */
                s_pa7.blank_w = 0u;
                s_pa7.blank_b = PA7_CAL_BLANK_HI;
            }
            f = (uint8_t)((f & (uint8_t)~0x03u) | (now != 0u ? 0x03u : 0u));
        }
    }
    s_pa7.an7_f = keep;                                     /* 0x93B2A */
    s_pa7.s9417b = nibv;                                    /* 0x9417B */
    s_pa7.s9417c = (uint8_t)(f | 0x08u);                    /* 0x9417C */
}

void pa7_shadow_pin(const pa7_pin_in_t *in)
{
    const uint8_t out0 = (uint8_t)(s_pa7.pa7 | (uint8_t)(s_pa7.c9b7 << 1));
    s_pa7.pin_n++;
    s_pa7.pin = *in;
    /* our copy of the 5 ms countdown 0x7C7E4 for the four cells (the list
     * head, before 0x7AB9C and 0x7CC32) */
    s_pa7.t_hi = cd16(s_pa7.t_hi, in->n5);
    s_pa7.t_lo = cd16(s_pa7.t_lo, in->n5);
    s_pa7.blank_w = cd16(s_pa7.blank_w, in->n5);
    s_pa7.blank_b = cd8(s_pa7.blank_b, in->n5);
    /* 0x91ACA (the reply proc, before the pin-cut entry): 1 → 0x0F */
    if (s_pa7.s9095b == 1u) {
        s_pa7.s9095b = 0x0Fu;
    }
    s_pa7.s94122 = s_pa7.d712;                                  /* 0x7AB9C → 0x7ACF2 */
    if (s_pa7.s9095b >= 15u) {                              /* 0x86B0C: cmp/hs #15 */
        if (s_pa7.s9411f != 0u) {
            c8ccd4(in);
        }
        c8ce48(in);
        c8cb7c(in);
        s_pa7.s9411f = 1u;
        s_pa7.law_n++;
        if (s_pa7.law_n == 1u) {
            s_pa7.first_law_us = in->t_us;
        }
        if (s_pa7.pa7 != 0u) {
            s_pa7.hi_n++;
        }
        if (s_pa7.c9b7 != 0u) {
            s_pa7.c9b7_n++;
        }
        {
            const uint8_t out = (uint8_t)(s_pa7.pa7 | (uint8_t)(s_pa7.c9b7 << 1));
            if (out != out0) {
                pa7_tr_t *t = &s_pa7.tr[s_pa7.tr_head];
                t->t_us = in->t_us;
                t->pass = (uint16_t)s_pa7.pin_n;
                t->d6d6 = s_pa7.rin.d6d6;
                t->d672 = s_pa7.rin.d672;
                t->d673 = s_pa7.rin.d673;
                t->e567 = s_pa7.e567;
                t->d712 = s_pa7.s94122;
                t->out = out;
                s_pa7.tr_head = (uint8_t)((s_pa7.tr_head + 1u) % PA7_TR_N);
                s_pa7.tr_n++;
            }
        }
    }
    /* 0x91DAA (the batch, after the pin-cut entry): 0 → 1 */
    if (s_pa7.s9095b < 1u) {
        s_pa7.s9095b = 1u;
    }
}

void pa7_shadow_init(void)
{
    uint8_t *p = (uint8_t *)(void *)&s_pa7;
    for (uint32_t k = 0u; k < sizeof s_pa7; k++) {
        p[k] = 0u;                                      /* 0x90D12: PA7 0; the cells cleared */
    }
}

const volatile pa7s_t *pa7_shadow_state(void) { return &s_pa7; }

#endif /* !TCU_RAM_APP */

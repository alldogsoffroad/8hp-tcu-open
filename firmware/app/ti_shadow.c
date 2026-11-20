/* ti_shadow.c - the READ-ONLY torque-intervention shadow (app/ti_shadow.h).
 * (2026-11-20, ~/tcu_sweep/lp_ti_shadow_port.md) Includes no live module, names no live
 * cell, calls no live function, has no volatile pointer; the only RAM it writes is its own
 * (.ti_shadow). It NEVER transmits: the would-be field values stay in the witness. */
#define TCU_TI_SHADOW_TU 1
#include "app/ti_shadow.h"
#include "app/lp_cal.h"

static ti_t s_ti __attribute__((section(".ti_shadow")));
#define T (s_ti)
#define W (T.w)
#define I (T.in)

#if !defined(__sh__)
static int16_t s_host_amount;
void ti_shadow_host_amount(int16_t nm) { s_host_amount = nm; }
#define AMOUNT_SI (s_host_amount)
#else
#define AMOUNT_SI ((int16_t)0)
#endif

uint16_t ti_field13(int32_t q)
{
    const int32_t f = q + 2000;
    return (uint16_t)((f < 0) ? 0 : ((f > 0x1FFD) ? 0x1FFD : f));
}

static int16_t sat_s16(int32_t v)
{
    return (v > 32767) ? (int16_t)32767 : ((v < -32768) ? (int16_t)-32768 : (int16_t)v);
}

/* 0xE5A20 (arm) + 0xE5E48 (start) */
static void arm(uint8_t bd16)
{
    T.armed = 1u;                                        /* TI+0x17 */
    T.cls = LP_TI_CLASS[bd16 & 7u];                      /* 0x634D6[BD16] */
    T.step = 0u;
    T.phase = 0u;
    T.tmr = 0u;
    T.out = 0;
    W.arm_n[bd16 & 3u]++;
    W.si_misc_n++;                                       /* the per-type calibration blocks (0xE584E / 0xE5814 / 0xE5892) */
}

/* 0xE630C: does the phase end? */
static uint8_t phase_exit(void)
{
    switch (T.phase) {
    case 8u:
        return 1u;                                       /* 0xE6AE8, then true (decoded) */
    case 0xCu:
        return 0u;                                       /* the terminal phase holds (decoded) */
    case 3u: case 4u: case 5u: case 6u: case 9u: case 0xAu: case 0xBu:
        if (T.tmr >= TI_SI_DWELL) {
            W.si_dwell_n++;
            return 1u;
        }
        return 0u;
    default:
        return 1u;                                       /* the switch's default (phases 0, 2, ...) */
    }
}

/* 0xE65F4: the amount of the phase, Nm */
static int16_t amount(void)
{
    switch (T.phase) {
    case 2u: case 8u: case 0xCu:
        return 0;                                        /* decoded zeros */
    default:
        W.si_amount_n++;
        return AMOUNT_SI;                                /* the per-type calibration chains: stood in */
    }
}

/* 0xE5EAA */
static void run(void)
{
    if (T.armed == 0u || I.bd1a == 0x0Au) {
        T.out = 0;
        return;
    }
    if (T.tmr < 0xFFFFu) {
        T.tmr++;
    }
    {
        uint32_t guard;
        for (guard = 0u; guard < 8u && phase_exit() != 0u; guard++) {
            const uint32_t row = ((uint32_t)T.step + 1u) * 3u + T.cls;
            uint8_t nxt;
            if (row >= 24u) {
                W.err_n++;                               /* 0xB75B4 (counted) */
                break;
            }
            nxt = LP_TI_PHASE[row];
#if defined(TI_NEG_TABLE)
            if (row == 8u) {
                nxt = 0xBu;                              /* the negative: the garage's second phase 0xA -> 0xB */
            }
#endif
            if (nxt == 0xFFu) {
                break;
            }
            T.step++;
            T.phase = nxt;
            T.tmr = 0u;
            W.phase_n++;
        }
    }
    T.out = amount();                                    /* 0xE6B6E / 0xE618C shaping: identity (stood in) */
}

void ti_shadow_init(void)
{
    uint8_t *p = (uint8_t *)&T;
    uint32_t i;
    for (i = 0u; i < (uint32_t)sizeof(T); i++) {
        p[i] = 0u;
    }
    T.fld_req = 0x1FFEu;
    T.fld_lim = ti_field13((int32_t)(TI_SI_LIM_CONV / 10) * 4);
}

void ti_shadow_pass(const ti_in_t *in)
{
    int32_t t3q;
    int32_t dq;
    int32_t req;
    T.in = *in;
    if (in->os_mode < 1u || in->os_mode > 3u) {
        W.skip_mode_n++;
        return;
    }
    W.pass_n++;
    /* the shift start / end: BD16 = 1 up / 2 down / 3 garage arms (0xED92E -> 0xE5A20, 0xEDC5A -> 0xE5E48);
     * its return (0xEBB6C / 0xEBC60 -> 0xE5FF6) ends */
    if (I.bd16 != T.bd16_last) {
        if (I.bd16 >= 1u && I.bd16 <= 3u) {
            arm(I.bd16);
        } else if (T.armed != 0u) {
            T.armed = 0u;                                /* 0xE5FF6: the ramp-out is stood in (immediate) */
            T.out = 0;
            T.phase = 0u;
            W.end_n++;
            W.si_misc_n++;
        }
        T.bd16_last = I.bd16;
    }
    run();                                               /* slot 36: 0xDE0D8 -> 0xE5EAA */
    /* 0xDDF84 / 0xDDF98: combine with E3DE */
    T.delta = T.out;
    if (T.out >= 0 && T.out < I.e3de) {
        T.delta = I.e3de;
    }
    W.si_misc_n++;                                       /* E3DE, the converter flags, E330: stood in */
    if (T.out == 0) {
        T.mode = (uint8_t)((I.e3de == 0) ? 0u : 1u);
    } else if (T.out < 0) {
        T.mode = 2u;
    } else {
        T.mode = 1u;
    }
    /* 0xD31D0: 0xD38B4 (the request), 0xD385C (pass-through), 0xD3208 (the type) */
    t3q = (int32_t)I.t3 * 4;
    dq = (int32_t)T.delta * 4;
    req = t3q;
    if (T.mode == 1u && T.delta > 0) {
        req = t3q + dq;
    }
    if (T.mode == 2u && dq < req - t3q) {
        req = t3q + dq;
    }
    if (t3q < req && TI_R23_HELD != 0u) {
        req = t3q;                                       /* an increase is cancelled while reaction 23 is active */
        W.r23_n++;
    }
    T.req_q = sat_s16(req);
    T.dreq_q = sat_s16(req - t3q);
    if (req > t3q) {
        T.type = (uint8_t)((T.mode == 1u && TI_R23_HELD == 0u) ? 1u : 0u);
    } else if (req < t3q) {
        T.type = 2u;
    } else {
        T.type = 0u;
    }
    if (T.type != 0u) {
        W.req_n++;
    }
    /* 0xA32E4 + the safety packer: the limit (the converter's) and the 13-bit fields */
    T.lim_nm = (int16_t)(TI_SI_LIM_CONV / 10);
    T.fld_req = (T.type == 0u) ? 0x1FFEu : ti_field13(T.req_q);
    T.fld_lim = ti_field13((int32_t)T.lim_nm * 4);
}

const volatile ti_t *ti_shadow_state(void) { return &s_ti; }

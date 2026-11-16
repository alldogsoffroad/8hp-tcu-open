/* drv_pksens.c — the park position sensor's switch evaluator, READ-ONLY
 * (drv_pksens.h). Ported from the bytes of 0x88EB0..0x88EE4,
 * 0x8DEF0..0x8DF2C, 0x8DF2E..0x8E27E, 0x8ED40..0x8ED5C, 0x88EF0..0x890DE,
 * 0x7B05C..0x7B078, 0x7B07A..0x7B12A, 0x7B12C..0x7B14E, 0x7B150..0x7B172,
 * 0x89320..0x8932C (sh-elf-objdump -b binary -m sh2a -EB of the pristine
 * image; the Ghidra text of 0x8DF2E is wrong in three places — it stores
 * only the "started" bit in 0x94169 (the bytes store the whole flag byte
 * at 0x8DFB0), it sets the low-reference flag to 2 instead of OR-ing bit 1
 * (bset #1 at 0x8DF7E), and it reads "1 << 0" as 0 for the digital-bit
 * index (shld at 0x8E14A)). Calibration through the 0x66xxx pointer tables
 * at index 0x90AB8 = 0. ~/tcu_sweep/park_sensor_port.md §2 has the
 * write-for-write table.
 *
 * STAND-INS / NOT PORTED:
 *   0x6F594 "fault memory ready" = 1 (as sup_ladder.c S1).
 *   0x9094B / 0x9094C (variant / AN-map offset): written only by 0x91ACA's
 *   tail (0x91C1A..0x91C5E: 1 / 6 when J5 0x907CF = 0xBC and the ID byte
 *   0x907D0 is 0 or 1 with cal 0x1918E1 = 0xFF; else 0 / 0), which is not
 *   ported. This bench's ID byte is 0x14 → 0 / 0 (the dump's values):
 *   constants here; the variant-1 / offset-6 tables are kept.
 *   0x7CA24(0x992, k) — the event hook at the head of 0x7B07A: no-op.
 *   0x72AE0(245000) at the head and tail of 0x88EF0 — the profiling hook
 *   (it compares r4 with the running-task word *0xFFF847E8 and sets bits in
 *   0x9406B/C; not a lock): dropped, as drv_temp.c / drv_groupa.c do. No
 *   lock is needed: every reader and writer here runs in the same task.
 *   0x6BDCA pair checks (fatal 0x221 → 0x6865C): counted in a witness, not
 *   fatal (our deviation).
 *   0x6F5C0 (the event call inside 0x7B12C): 0x7B12C(0x3E, 5) never calls
 *   it (r5 = 5 >= 3 and 0x3E & 0xC1 == 0) — ported as that test.
 */
#include "bsw/drv/drv_pksens.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/sup/sup_cells.h"
#include "platform.h"

#if defined(TCU_RAM_APP)
/* not in the RAM application (as drv_temp.c): no AN table, no mux 15 */
void drv_pksens_init(void) {}
void drv_pksens_ref_tick(void) {}
void drv_pksens_tick(void) {}
void drv_pksens_copy_tick(void) {}
uint32_t drv_pksens_f221_n(void) { return 0u; }
uint32_t drv_pksens_f221_site(void) { return 0u; }
#else

/* ── calibration [O bytes, pointer tables at index 0] ───────────────────── */
static const uint8_t CAL_EN[2]     = { 0x0Cu, 0x0Cu };       /* 0x1914AC[var] inputs enabled */
static const uint8_t CAL_AN[2]     = { 0x0Cu, 0x0Cu };       /* 0x1914AA[var] analog evaluation */
static const uint8_t CAL_ANIDX[12] = { 3u, 4u, 5u, 6u, 9u, 10u,    /* 0x1903D8[k + off] */
                                       3u, 4u, 5u, 6u, 9u, 10u };
static const uint8_t CAL_DIGB[12]  = { 0u, 1u, 2u, 3u, 0u, 0u,     /* 0x1903F4[k + off] */
                                       0u, 1u, 2u, 3u, 0u, 0u };
/* 0x66B28 → 0x1913A8: 5 u16 edges per input (10-byte stride), all six equal */
static const uint16_t CAL_EDGE[6][5] = {
    { 100u, 1900u, 5000u, 8000u, 9500u }, { 100u, 1900u, 5000u, 8000u, 9500u },
    { 100u, 1900u, 5000u, 8000u, 9500u }, { 100u, 1900u, 5000u, 8000u, 9500u },
    { 100u, 1900u, 5000u, 8000u, 9500u }, { 100u, 1900u, 5000u, 8000u, 9500u },
};
#define CAL_THR         5000u       /* 0x66B2C → 0x191720 state threshold */
#define CAL_REF_MIN     6000u       /* 0x66B34 → 0x191724 reference minimum (mV) */
#define CAL_START       2u          /* 0x66B24 → 0x191A00 start passes */
#define CAL_ST_RELOAD   100u        /* 0x66B30 → 0x191722 self-test reload */
#define CAL_MMTEST      0x00u       /* 0x66B70 → 0x191A03 mismatch-test enable */
#define CAL_MON         0x0Cu       /* 0x66B74 → 0x191A04 monitored inputs */
#define STANDIN_VAR     0u          /* 0x9094B (writer 0x91ACA tail not ported) */
#define STANDIN_OFF     0u          /* 0x9094C */
#define STANDIN_FAULTMEM_READY 1u   /* 0x6F594 */

/* ── witnesses (ours, .pk_wit at 0xFFF84D00: low RAM, not .bss) ─────────── */
#if defined(__sh__)
#define PK_WIT __attribute__((section(".pk_wit")))
#else
#define PK_WIT
#endif
static volatile uint32_t s_f221_n PK_WIT;
static volatile uint32_t s_f221_site PK_WIT;

uint32_t drv_pksens_f221_n(void)    { return s_f221_n; }
uint32_t drv_pksens_f221_site(void) { return s_f221_site; }

/* 0x6BDCA: a byte pair value / complement; torn → (stock) 0x6865C(0x221)
 * fatal; here counted. Returns the value. */
static uint8_t pair_check(volatile uint8_t *v, volatile uint8_t *c)
{
    const uint8_t x = *v;
    if (x != (uint8_t)~*c) {
        s_f221_n = s_f221_n + 1u;
        s_f221_site = (uint32_t)(uintptr_t)v;
    }
    return x;
}

/* 0x6BD68 */
static void pair8(volatile uint8_t *v, volatile uint8_t *c, uint8_t x)
{
    *v = x;
    *c = (uint8_t)~x;
}

/* 0x8ED40: x * mul / den, 0xFFFF on den == 0 or > 0xFFFF (mulr, divu) */
static uint16_t ratio_8ed40(uint16_t x, uint16_t mul, uint16_t den)
{
    if (den == 0u) {
        return 0xFFFFu;
    }
    {
        const uint32_t q = ((uint32_t)x * (uint32_t)mul) / (uint32_t)den;
        return q > 0xFFFFu ? 0xFFFFu : (uint16_t)q;
    }
}

/* ── 0x8DF2E: the evaluator ─────────────────────────────────────────────── */
static void eval_8df2e(void)
{
    const uint8_t var = STANDIN_VAR;
    const uint8_t off = STANDIN_OFF;
    const uint16_t ref = CELL_PKS_REF;          /* @(20,r15): 0x93942 at entry */
    uint8_t en_acc = 0u;                        /* @(4)  */
    uint8_t inval = 0u;                         /* @(8)  */
    uint8_t act = 0u;                           /* @(12) */
    uint8_t an_acc = 0u;                        /* @(0)  */
    uint8_t mism = 0u;                          /* @(16) */
    uint8_t dig = 0u;                           /* r8    */
    uint8_t fl;                                 /* r5    */
    uint8_t mode;

    CELL_PKS_DIGM_PREV = CELL_PKS_DIGM;         /* 0x8DF32..0x8DF3E */
    if (CELL_PKS_START != 0u) {                 /* 0x8DF52: tst; bf.s */
        CELL_PKS_START = (uint8_t)(CELL_PKS_START - 1u);
        fl = 0u;
    } else {
        fl = 1u;                                /* started */
    }
    if (ref < CAL_REF_MIN) {                    /* 0x8DF7A cmp/hi: 6000 > ref */
        fl = (uint8_t)(fl | 0x02u);
    }
    if (CELL_PKS_SELFTEST == 0u) {              /* 0x8DF80 */
        CELL_PKS_ST_TMR = CAL_ST_RELOAD;
    } else {
        fl = (uint8_t)(fl | 0x04u);             /* bset #2 (the delay slot) */
        if (CELL_PKS_ST_TMR == 0u) {
            fl = (uint8_t)(fl | 0x08u);
        }
    }
    CELL_PKS_FLAGS = fl;                        /* 0x8DFB0 (delay slot): the whole byte */
    if ((fl & 0x01u) == 0u) {
        mode = 0u;
    } else if ((fl & 0x0Eu) == 0u) {
        mode = 8u;
    } else if ((fl & 0x08u) != 0u) {
        mode = 2u;
    } else if ((fl & 0x04u) != 0u) {
        mode = 7u;
    } else {
        mode = 2u;
    }
    CELL_PKS_MODE = mode;                       /* 0x8DFD8 */
    if (mode != 8u && mode != 2u) {
        CELL_PKS_CHG = 0u;                      /* 0x8DFE6 */
        return;
    }

    for (uint32_t k = 0u; k < 6u; k++) {
        const uint8_t bit = (uint8_t)(1u << k); /* r9 */
        uint8_t en_k;                           /* r10 */
        uint8_t st, dk, comp;

        if ((CAL_EN[var] & bit) != 0u) {
            en_acc = (uint8_t)(en_acc | bit);
        }
        if ((CAL_AN[var] & bit) != 0u) {
            an_acc = (uint8_t)(an_acc | bit);
        }
        (void)pair_check(&CELL_PKS_PAIR[k], &CELL_PKS_PAIR_C[k]);   /* 0x8E040 */
        en_k = (uint8_t)(en_acc & bit);
        if ((an_acc & bit) != 0u) {
            const uint8_t an = CAL_ANIDX[k + off];
            const uint16_t ratio = ratio_8ed40(CELL_AN_MV(an), 10000u, ref);
            uint8_t j = 0u;

            CELL_PKS_RATIO[k] = ratio;          /* 0x8E08A */
            CELL_PKS_STATE[k] = (uint8_t)(ratio < CAL_THR ? 1u : 0u);   /* cmp/ge, movrt */
            CELL_PKS_BANDJ = 0u;
            CELL_PKS_K = (uint8_t)k;
            /* 0x8E0C0..0x8E0DE: j counts the edges <= ratio, capped at 5 */
            while (j < 5u && CAL_EDGE[k][j] <= ratio) {
                j++;
                CELL_PKS_BANDJ = j;
            }
            CELL_PKS_BAND[k] = j;               /* 0x8E0E6 */
            if (j != 1u && j != 3u) {
                inval = (uint8_t)(inval | bit);
            }
            /* 0x8E0F8..0x8E114: 0xFF - (float(thr) > float(ratio)) */
            CELL_PKS_STATE_C[k] = (uint8_t)(0xFFu - (ratio < CAL_THR ? 1u : 0u));
        } else {
            CELL_PKS_RATIO[k] = 0u;             /* 0x8E128 */
            CELL_PKS_BAND[k] = 2u;              /* 0x8E12E (delay slot) */
            if (en_k == 0u) {
                CELL_PKS_STATE[k] = 0u;         /* 0x8E194 */
                CELL_PKS_STATE_C[k] = 0xFFu;
            } else {                            /* 0x8E130: the digital input */
                const uint8_t d = (uint8_t)((CELL_SSLB0_NIB & (1u << CAL_DIGB[k + off])) != 0u ? 1u : 0u);
                CELL_PKS_STATE_C[k] = (uint8_t)(0xFFu - d);
                CELL_PKS_STATE[k] = d;
            }
        }
        /* 0x8E1A4.. */
        st = CELL_PKS_STATE[k];
        if (st != 0u) {
            act = (uint8_t)(act | bit);
        }
        if (en_k == 0u) {
            dk = 0u;
        } else {
            dk = (uint8_t)((CELL_SSLB0_NIB & (1u << CAL_DIGB[k + off])) != 0u ? 1u : 0u);
        }
        CELL_PKS_DIG[k] = dk;                   /* 0x8E1F0 */
        dk = CELL_PKS_DIG[k];
        if (dk != 0u) {
            dig = (uint8_t)(dig | bit);
        }
        comp = CELL_PKS_STATE_C[k];
        CELL_PKS_PAIR[k] = st;                  /* 0x8E222 */
        CELL_PKS_PAIR_C[k] = comp;              /* 0x8E226 (delay slot) */
        if (((en_acc & an_acc) & bit) != 0u) {
            if (st != dk || comp != (uint8_t)~dk) {
                mism = (uint8_t)(mism | bit);   /* 0x8E234 */
            }
        }
    }
    CELL_PKS_CHG = (uint8_t)(en_acc & (uint8_t)(CELL_PKS_DIGM_PREV ^ dig));   /* 0x8E248 */
    CELL_PKS_ACT = act;
    CELL_PKS_INVAL = inval;
    CELL_PKS_MISM = mism;
    CELL_PKS_DIGM = dig;
}

/* 0x7B05C: tested / error bits → the 4-bit fault code */
static uint8_t map_7b05c(uint32_t x)
{
    uint8_t r = (uint8_t)((x >> 3) & 1u);       /* bld #3, movt */
    if ((x & 0x04u) != 0u) r = (uint8_t)(r | 0x02u);
    if ((x & 0x10u) != 0u) r = (uint8_t)(r | 0x04u);
    if ((x & 0x20u) != 0u) r = (uint8_t)(r | 0x08u);
    if ((x & 0x02u) != 0u) r = (uint8_t)(r | 0x08u);
    return r;
}

/* 0x7B07A(tested, errors, k) */
static void report_7b07a(uint16_t tested, uint16_t err, uint8_t k)
{
    uint16_t w;
    uint8_t m, old;

    /* 0x7CA24(0x992, k): the event hook — no-op */
    if (k >= 4u) {                              /* 0x7B098 cmp/ge #4 */
        return;
    }
    w = (uint16_t)((CELL_PKS_MERR[k] & (uint16_t)~tested) | (err & tested));
    CELL_PKS_MERR[k] = w;                       /* 0x7B0BC (delay slot) */
    m = map_7b05c(tested);
    CELL_PKS_MTESTED[k] = m;                    /* 0x7B0DA */
    old = CELL_PKS_CP_FLT[k];
    if ((w & tested) == 0u) {                   /* 0x7B11A */
        CELL_PKS_CP_FLT[k] = (uint8_t)(old & (uint8_t)~m);
        CELL_PKS_CP_FLT_C[k] = (uint8_t)(CELL_PKS_CP_FLT_C[k] | m);
    } else {                                    /* 0x7B0E4 */
        const uint8_t me = (uint8_t)(map_7b05c(w) & m);
        CELL_PKS_CP_FLT[k] = (uint8_t)(me | (uint8_t)(old & (uint8_t)~m));
        CELL_PKS_CP_FLT_C[k] = (uint8_t)((uint8_t)(CELL_PKS_CP_FLT_C[k] | m) & (uint8_t)~me);
    }
}

/* 0x7B12C(tested, n): the event call 0x6F5C0(0x992, 16, x) */
static void event_7b12c(uint16_t tested, uint8_t n)
{
    if (n < 3u) {
        /* 0x6F5C0(0x992, 16, 1): the event hook — no-op */
    } else if ((tested & 0xFFC1u) != 0u) {      /* mov #-63 (0x...FFC1), tst */
        /* 0x6F5C0(0x992, 16, 0): the event hook — no-op */
    }
}

/* ── 0x88EF0: the monitor ───────────────────────────────────────────────── */
void drv_pksens_tick(void)
{
    const uint8_t var = STANDIN_VAR;

    /* 0x72AE0(245000): profiling hook — not ported */
    CELL_PKS_ACT_PREV = CELL_PKS_ACT;           /* 0x88F10 */
    for (uint32_t i = 0u; i < 6u; i++) {
        CELL_PKS_BAND_PREV[i] = CELL_PKS_BAND[i];
    }
    eval_8df2e();
    CELL_PKS_STABLE = (uint8_t)((CELL_PKS_MODE == 8u && CELL_PKS_CHG == 0u &&
                                 CELL_PKS_ACT_PREV == CELL_PKS_ACT) ? 1u : 0u);   /* 0x88F52 */

    for (uint32_t k = 0u; k < 6u; k++) {
        const uint8_t bit = (uint8_t)(1u << k); /* r7 / r11 */
        uint16_t tested = 0u;                   /* r13 */
        uint16_t err = 0u;                      /* r14 */

        if ((CAL_AN[var] & bit) != 0u && (CAL_MON & bit) != 0u) {
            const uint8_t band = CELL_PKS_BAND[k];
            uint8_t stb = 0u;                   /* r4 */
            uint8_t mm = 0u;                    /* r1 */

            if (CELL_PKS_STABLE != 0u && CELL_PKS_BAND_PREV[k] == band) {
                stb = 1u;
            }
            if ((CAL_EN[var] & bit) != 0u && (CELL_PKS_INVAL & bit) == 0u &&
                (CELL_PKS_MISM & bit) == 0u && (CAL_MMTEST & bit) != 0u) {
                mm = 1u;
            }
            if (mm == 1u) {                     /* 0x88FD6 */
                const uint8_t seen = (uint8_t)(CELL_PKS_CHGSEEN | (CELL_PKS_CHG & bit));
                CELL_PKS_CHGSEEN = seen;
                stb = (uint8_t)((stb != 0u && (seen & bit) != 0u) ? 1u : 0u);
            } else {                            /* 0x88FF8 */
                CELL_PKS_CHGSEEN = (uint8_t)(CELL_PKS_CHGSEEN & (uint8_t)~bit);
            }
            tested = (uint16_t)(stb == 1u ? 0x3Eu : 0u);
            switch (band) {                     /* 0x8900E.. */
            case 0u: err = 0x08u; break;
            case 2u: err = 0x20u; break;
            case 4u: err = 0x10u; break;
            case 5u: err = 0x04u; break;
            default:
                err = (uint16_t)((CELL_PKS_MISM & bit) != 0u ? 0x02u : 0u);
                break;
            }
        }
        if (STANDIN_FAULTMEM_READY != 0u) {     /* 0x6F594 */
            report_7b07a(tested, err, (uint8_t)k);
        }
        CELL_PKS_TESTED[k] = tested;            /* 0x89068 */
        CELL_PKS_ERR[k] = err;                  /* 0x8906C (delay slot) */
        if (tested != 0u) {
            CELL_PKS_LATCH[k] = (uint8_t)(err != 0u ? 1u : 0u);
        }
        (void)pair_check(&CELL_PKS_ST[k], &CELL_PKS_ST_C[k]);       /* 0x8908E */
        {
            const uint8_t m = CELL_PKS_MODE;
            const uint8_t l = CELL_PKS_LATCH[k];
            uint8_t v;
            if (m == 8u) {
                v = (uint8_t)(l != 0u ? 2u : 8u);
            } else {
                v = (uint8_t)(l != 0u ? 2u : m);
            }
            pair8(&CELL_PKS_ST[k], &CELL_PKS_ST_C[k], v);           /* 0x890C6 */
        }
    }
    /* 0x72AE0(245000): profiling hook — not ported */
}

/* ── 0x89320 ────────────────────────────────────────────────────────────── */
void drv_pksens_ref_tick(void)
{
    CELL_PKS_REF = CELL_PKS_MUX15;
}

/* ── 0x7B150 ────────────────────────────────────────────────────────────── */
void drv_pksens_copy_tick(void)
{
    for (uint32_t i = 0u; i < 4u; i++) {
        CELL_PKS_CP_ST[i] = CELL_PKS_PAIR[i];
        CELL_PKS_CP_ST_C[i] = CELL_PKS_PAIR_C[i];
    }
}

/* ── 0x88EB0 (→ 0x8DEF0 → 0x7B12C) ─────────────────────────────────────── */
void drv_pksens_init(void)
{
    /* our NOLOAD sections (the stock RAM starts at 0, except 0x847F4..: the
     * stock's initialised-data block, ROM 0x65110 → RAM 0xFFF84000 (DTBL
     * 0x415B0), ROM 0x65904..: 00 x 8, FF x 8) */
    for (uint32_t a = 0xFFF934C4u; a < 0xFFF934E8u; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    CELL_PKS_REF = 0u;
    for (uint32_t a = 0xFFF93AC4u; a < 0xFFF93B04u; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    for (uint32_t a = 0xFFF9415Fu; a < 0xFFF9416Eu; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    for (uint32_t a = 0xFFF94170u; a < 0xFFF94174u; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    for (uint32_t a = 0xFFF92FE4u; a < 0xFFF92FF0u; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    for (uint32_t i = 0u; i < 8u; i++) {
        CELL_PKS_CP_ST[i] = 0x00u;              /* 0x847F4..0x847FB */
        CELL_PKS_CP_FLT_C[i] = 0xFFu;           /* 0x847FC..0x84803 */
    }
    s_f221_n = 0u;
    s_f221_site = 0u;

    for (uint32_t k = 0u; k < 6u; k++) {        /* 0x88EB2..0x88ED4 */
        pair8(&CELL_PKS_ST[k], &CELL_PKS_ST_C[k], 0u);
    }
    CELL_PKS_START = CAL_START;                 /* 0x8DEF0 */
    CELL_PKS_ST_TMR = CAL_ST_RELOAD;
    for (uint32_t k = 0u; k < 6u; k++) {
        CELL_PKS_PAIR[k] = 0u;
        CELL_PKS_PAIR_C[k] = 0xFFu;
    }
    event_7b12c(0x3Eu, 5u);                     /* 0x88EDA: jmp 0x7B12C */
}
#endif /* !TCU_RAM_APP */

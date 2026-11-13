/* drv_reqlaw.c — the stock's solenoid request law (drv_reqlaw.h for the
 * safety frame). Ported from the bytes of 0x83DB0..0x8422C, 0x89540..
 * 0x89646, 0x90118..0x9019E, 0x901A0..0x903CA (sh-elf-objdump of the
 * pristine image; the decompiles of r08_app checked against them);
 * calibration through the 0x66xxx pointer tables at index 0x90AB8 = 0.
 * ~/tcu_sweep/request_law_port.md.
 *
 * NOT PORTED (diagnostics with no write to any cell this law outputs —
 * named stand-ins / deviations, request_law_port.md §3):
 *   0x8422E  rail filter → 0x93100 (no reader in this law)
 *   0x89F5C  per-channel diagnosis (from pass 2, 0x84A22)
 *   0x7BA76  fault class (0x93004 / 0x84890) — stand-in 1 (no fault)
 *   0x93A42  channel fault class — stand-in 0 (no fault)
 *   0x897AC, 0x8A788, 0x84644, 0x84392, 0x8493A — fault monitors
 *   0x6865C  fault reports 0x226 (complement checks) / 0x238 (counted only)
 *   0x837E0  adaptive correction — stand-in 0 (host: injectable)
 *   0x89648  dither demand → 0x90320 stays 0 (dump: 0)
 *   0x921E0  chip-ID harvest into the records → 0x9091B stays 0
 *   0x91E34 / 0x8FDB6 / the 0x90919 tail (reduced frequency, group B)
 */
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/sup/sup_cells.h"
#include <string.h>

#if defined(__sh__)
_Static_assert(DRV_RQ_REQUEST_SOURCE == 0,
               "drv_reqlaw: SH builds carry no solenoid request source");
#endif

/* ── calibration (flash 0x19xxxx, read through the 0x66xxx tables) ────── */
#define CAL_CHIP_EN     0x1Fu       /* 0x1918C1 (0x8FD94, pool 0x8FE8C) */
#define CAL_CH_MASK     0x00FFu     /* 0x19154C (pool 0x8402C → 0x667C0) */
#define CAL_UV_MV       7400u       /* 0x191554 (pool 0x84030 → 0x667DC) */
#define CAL_UV_THR      4000u       /* 0x191548 (pool 0x84034 → 0x667B8) */
#define CAL_FULL15      0x03FFu     /* 0x191550 (pool 0x84044 → 0x667CC) */
#define CAL_FULL14      0x03FFu     /* 0x19154E (pool 0x84048 → 0x667C4) */
#define CAL_UV_REL      500u        /* 0x191552 (pool 0x8404C → 0x667D8) */
#define CAL_UV_FLAGS    0x0001u     /* 0x190510[10], all 0x0001 (pool 0x84054 → 0x667C8) */
#define CAL_TYPE        1u          /* 0x1916BA[10], all 01 (pools 0x84050/0x84320/0x896F8 → 0x66A08) */
#define CAL_IMIN        500u        /* 0x19154A (pool 0x8431C → 0x667BC) */
#define CAL_MODE_B4     0x10u       /* 0x1918B9 & 0x10 (pool 0x903E0) */
#define CAL_ROUND_INIT  0x00u       /* 0x1918BB (0x90010 tail, pool 0x90174) */
#define CAL_HI_MASK     0x0080u     /* 0x191488 (pool 0x903DC) */
#define I_CLAMP         0x2FFAu     /* 0x83EDE / 0x895DA / 0x90132 */

/* 0x90010 → 0x8FC5E / 0x8FDB6: the config bytes at 0x9091B = 0 (dump
 * 0x90900 = 9F×5, 0x90905 = 21×5, 0x908FB = 55×5; the r02v readbacks
 * A29F / A221 / A255) — constants here, the computation is not ported */
#define CFG_8E          0x9Fu
#define CFG_90          0x21u
#define CFG_96          0x55u

/* ── our witnesses (.drv_wit at 0xFFF84C40: low RAM, not .bss) ─────────── */
#if defined(__sh__)
#define WIT __attribute__((section(".drv_wit")))
#else
#define WIT
#endif
static volatile uint32_t s_rq_ilock_n WIT;
static volatile uint32_t s_rq_cguard_n WIT;
static volatile uint32_t s_rq_f238_n WIT;

uint32_t drv_rq_ilock_n(void)  { return s_rq_ilock_n; }
uint32_t drv_rq_cguard_n(void) { return s_rq_cguard_n; }
uint32_t drv_rq_f238_n(void)   { return s_rq_f238_n; }

/* ── the request source ────────────────────────────────────────────────── */
static inline uint16_t rq_request(uint8_t ch)
{
#if DRV_RQ_REQUEST_SOURCE
    return CELL_RQ_REQ[ch];
#elif defined(__sh__) && defined(DRV_RQ_PSET)
    /* r03f: the stock P set (drv_reqlaw.h), compile-time constants */
    static const uint16_t PSET[10] = DRV_RQ_PSET_TABLE;
    return (ch < 10u) ? PSET[ch] : 0u;
#else
    (void)ch;
    return 0u;          /* SAFETY: the target has no request source */
#endif
}

#if !defined(__sh__)
static int16_t s_corr_inject[DRV_RQ_NCH];

void drv_rq_request_set(uint8_t ch, uint16_t req01ma)
{
    if (ch < DRV_RQ_NCH) {
        CELL_RQ_REQ[ch] = req01ma;
    }
}

void drv_rq_corr_inject(uint8_t ch, int16_t corr)
{
    if (ch < DRV_RQ_NCH) {
        s_corr_inject[ch] = corr;
    }
}
#endif

/* 0x837E0 stand-in: the stock's adaptive correction (0x93390 channel mask,
 * 0x940FC > 5, 0x9410A / 0x9410B, the 0x8856C learning table) is not
 * ported. Its disabled path — 0x9337C[ch] = 0, return 0 — is what the
 * target runs; the host may inject the value the stock would have
 * returned (the dump's 0x9337C = 0 4 4 2 6 2 2 0 0 0). Like the stock, a
 * zero current (param_2 == 0) gets no correction. */
static int16_t rq_correction(uint8_t ch, uint16_t i)
{
    int16_t c = 0;
#if !defined(__sh__)
    if (i != 0u) {
        c = s_corr_inject[ch];
    }
#else
    (void)i;
#endif
    CELL_RQ_CORR[ch] = c;
    return c;
}

/* our interlock (not stock; drv_jobs.c r02w) */
bool drv_rq_rail_quiet(void)
{
    return CELL_J2_STAGED == 0x9000u &&
           CELL_LAD_REQ[0] == 0u && CELL_LAD_REQ[1] == 0u;
}

/* 0x8FD94: chip ch >> 1 present in cal 0x1918C1 */
static bool rq_chip_enabled(uint8_t ch)
{
    const uint8_t chip = (uint8_t)(ch >> 1);
    return chip < 5u && ((CAL_CHIP_EN >> chip) & 1u) != 0u;
}

/* 0x7BA76 stand-in: 1 (0x93004[ch] == 0 → 1; the stock returns 2 only with
 * 0x93004[ch] != 0 and the request copy 0x84890[ch] == 0) */
static uint8_t rq_fault_class(uint8_t ch) { (void)ch; return 1u; }

/* 0x93A42[ch] stand-in: 0 (dump: 00 ×10) */
static uint8_t rq_chan_class(uint8_t ch) { (void)ch; return 0u; }

/* ── 0x89540: the letter law, one channel ─────────────────────────────── */
static void rq_letters(uint8_t ch)
{
    uint16_t loc = 0u;                          /* local_1c */
    uint8_t letter;

    if (CAL_TYPE == 1u) {                       /* 0x89560 */
        const uint8_t m = CELL_RQ_MODE[ch];
        if (m == 1u) {
            letter = 0u;                        /* 0x8957A: disabled */
        } else if (m == 2u) {
            letter = (uint8_t)'!';              /* 0x89596, I = 0 */
        } else if (m == 4u) {
            letter = (uint8_t)'C';              /* 0x895A8: full on */
        } else if (m == 3u) {
            loc = CELL_RQ_I2[ch];               /* 0x8958C */
            letter = (uint8_t)'!';
        } else {
            letter = (uint8_t)'2';              /* 0x895AE */
        }
    } else if (CAL_TYPE == 2u && CELL_RQ_I2[ch] != 0u) {
        letter = (uint8_t)'C';
    } else {
        letter = (uint8_t)'2';
    }
    /* 0x89648 (dither demand, not ported: 0x90320 stays 0) */
    {
        const int16_t corr = rq_correction(ch, loc);    /* 0x837E0 */
        int16_t cur;
        uint16_t out;
        CELL_RQ_CORR_LAST = corr;                       /* 0x895CA */
        cur = (int16_t)(uint16_t)((uint16_t)corr + loc);/* exts.w */
        if (cur < 1 || loc == 0u) {                     /* cmp/pl; tst r5 */
            out = 0u;
        } else {
            out = (uint16_t)cur;
            if (out > I_CLAMP) {
                out = I_CLAMP;
            }
        }
        CELL_RQ_LETTER[ch] = letter;                    /* 0x89600 */
        CELL_RQ_CUR[ch] = out;                          /* 0x8960E */
        CELL_RQ_I2[ch] = out;                           /* 0x89612 */
    }
}

/* ── 0x83DB0: the per-channel gates ───────────────────────────────────── */
void drv_rq_law(void)
{
    uint16_t full_mask = 0u;                    /* @(4,r15) → 0x9397C */
    uint16_t fault_mask = 0u;                   /* @(8,r15) → 0x9397A */

    /* 0x8422E (rail filter → 0x93100): not ported, no reader here */
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        const uint16_t bit = (uint16_t)(1u << ch);

        if (!rq_chip_enabled(ch) || (CAL_CH_MASK & bit) == 0u) {
            /* 0x84156: the channel is not driven */
            CELL_RQ_I2[ch] = 0u;
            CELL_RQ_I[ch] = 0u;
            CELL_RQ_MODE[ch] = 0u;
            CELL_RQ_CUR[ch] = 0u;
            CELL_RQ_LETTER[ch] = (uint8_t)'2';
            continue;                           /* → 0x84644/0x84392/0x8493A (not ported) */
        }
        {
            const uint16_t rail = CELL_OUT_MV[0];           /* 0x94ED0, read per channel */
            const uint8_t s = CELL_SEQ2_STATE[0];           /* 0x85DC4(0) */
            const bool t15 = (CELL_DIAG_94115 & 1u) != 0u;  /* 0x83E42 */
            const bool t14 = (CELL_LAD_94114 & 1u) != 0u;   /* 0x83E4A */
            const uint8_t fc = rq_fault_class(ch);          /* 0x7BA76 */
            const uint16_t req = rq_request(ch);
            const bool test_state = s == 0x11u || s == 0x16u || s == (uint8_t)'3';
            uint8_t mode = 1u;                              /* r12 */
            bool fault = false;
            uint16_t i = 0u;                                /* r11 */
            uint8_t cls;

            if (fc != 0u && fc != 1u) {                     /* 0x83E7E..0x83EA0 */
                mode = 0u;
                fault = true;
                fault_mask |= bit;
            }
            CELL_RQ_ACTIVE[ch] = (req != 0u && !fault) ? 1u : 0u;   /* 0x83EA6 */
            if (s == (uint8_t)'U' && !fault) {              /* 0x83ECE: THE 'U' GATE */
                i = req;
            }
            if (i > I_CLAMP) {                              /* 0x83EDE */
                i = I_CLAMP;
            }
            cls = rq_chan_class(ch);                        /* 0x93A42 */
            if (cls == 2u) {
                mode = 2u;
            } else if (cls == 1u || cls == 0u) {
                if (i != 0u) {
                    mode = 3u;                              /* current control */
                }
            } else {
                mode = 0u;
            }
            if (test_state) {                               /* 0x83F1C */
                const bool full =
                    (t15 && (CAL_FULL15 & bit) != 0u) ||
                    (t14 && (s == 0x11u || s == 0x16u) && (CAL_FULL14 & bit) != 0u);
                if (full) {
                    mode = 4u;                              /* 0x83F72: FULL ON */
                    full_mask |= bit;
                } else {
                    i = 0u;                                 /* 0x83F84 */
                    mode = 0u;
                }
            }
            /* the rail-undervoltage law (0x83F90..0x84082) */
            if ((uint32_t)rail >= (uint32_t)CAL_UV_MV + CAL_UV_REL) {
                CELL_RQ_UVFLAGS = 0u;                       /* release, every channel */
            }
            if (rail < CAL_UV_MV || (CELL_RQ_UVFLAGS & bit) != 0u) {
                if (i != 0u && CAL_TYPE == 1u) {
                    if (i > CAL_UV_THR) {                   /* 0x83FF2 → 0x84058 */
                        CELL_RQ_UVFLAGS = (uint16_t)(CELL_RQ_UVFLAGS & (uint16_t)~bit);
                        if ((CAL_UV_FLAGS & 2u) != 0u) {    /* 0x8406E (cal: never) */
                            i = I_CLAMP;
                            mode = 4u;
                        }
                    } else if ((CAL_UV_FLAGS & 1u) != 0u) { /* 0x84002 */
                        CELL_RQ_UVFLAGS = (uint16_t)(CELL_RQ_UVFLAGS | bit);
                        i = 0u;
                        mode = 0u;
                    }
                }
            }
            /* the minimum-current law (0x84084) */
            if (i < CAL_IMIN && i != 0u && CAL_TYPE == 1u) {
                i = 0u;
                mode = 0u;
            }
            /* OUR INTERLOCK (not stock): no full-on while the rail word or
             * a supervisor request is up — the stock gets this from its
             * same-tick ordering; our bus-A cyclic is not on that tick */
            if (mode == 4u && !drv_rq_rail_quiet()) {
                mode = 0u;
                i = 0u;
                full_mask = (uint16_t)(full_mask & (uint16_t)~bit);
                s_rq_ilock_n++;
            }
            CELL_RQ_I[ch] = i;                              /* 0x840C4 */
            CELL_RQ_MODE[ch] = mode;                        /* 0x840C8 */
            CELL_RQ_FULLMASK = full_mask;                   /* 0x840D2 */
            CELL_RQ_FAULTMASK = fault_mask;                 /* 0x840DE */
            /* 0x897AC (fault monitor): not ported */
            CELL_RQ_I2[ch] = i;                             /* 0x840E6 */
            rq_letters(ch);                                 /* 0x89540 */
            /* 0x8A788, then the 0x226 complement checks: not ported */
        }
        /* 0x84644 / 0x84392 / 0x8493A: not ported */
    }
    /* 0x841D6: 0x84A22 = 1 / 0x84A23 = 0xFE (the 0x89F5C enable): not ported */
}

/* ── 0x90118: one channel's setpoint ──────────────────────────────────── */
static uint16_t rq_setpoint(uint8_t ch, uint8_t letter)
{
    uint16_t cur;
    if (letter == (uint8_t)'!') {
        cur = CELL_RQ_CUR[ch];
        if (cur > I_CLAMP) {                        /* 0x90138 */
            cur = I_CLAMP;
        }
    } else if (letter == (uint8_t)'C') {
        cur = I_CLAMP;                              /* 0x90178: full on */
    } else {
        cur = 0u;
    }
    CELL_RQ_CUR_SENT[ch] = cur;                     /* 0x90190, before the offset */
    /* (I + 0x9090A) × 0x903CC (= 0x3E2AAAAB, 1/6 rounded up), ftrc */
    return (uint16_t)(int32_t)((float)((uint32_t)cur + CELL_RQ_ROUND) * 0x1.555556p-3f);
}

/* the 0x901A0 mode-byte code of one letter (0x902F0.. / 0x9031A..):
 * < 0x32: '!' → 2, other (0) → 0, dither allowed; >= 0x32: 1, 'C' → 3 */
static uint8_t rq_code(uint8_t letter, bool *dither_ok)
{
    if (letter < 0x32u) {
        *dither_ok = true;
        return letter == (uint8_t)'!' ? 2u : 0u;
    }
    *dither_ok = false;
    return letter == (uint8_t)'C' ? 3u : 1u;
}

/* the dither byte (0x901A0 tail of the record loop) */
static uint8_t rq_dither(uint8_t ch, uint16_t sp, bool ok)
{
    uint16_t d = CELL_RQ_DITHER[ch];
    if (CELL_RQ_REDFREQ == 2u) {
        d = (uint16_t)((uint32_t)(d + 1u) >> 1);
    }
    if (sp == 0u || d < 0x32u || !ok) {
        CELL_RQ_DITH_ST[ch] = 0u;
        return 0u;
    }
    if (CELL_RQ_DITH_ST[ch] != 0u) {
        CELL_RQ_DITH_ST[ch] = 2u;
    }
    /* × 0x903E4 (= 0x3CB02C0B, 1/46.5) */
    return (uint8_t)(int32_t)((float)d * 0x1.605816p-6f);
}

/* ── 0x901A0: the per-chip records ────────────────────────────────────── */
void drv_rq_compose(void)
{
    uint16_t hi = 0u;                           /* uStack_34 */
    const bool quiet = drv_rq_rail_quiet();
    const uint16_t *st = drv_job_tx(DRV_CYC);

    /* 0x901BC: the staged setpoints (0x92A60: E8 / F0 rows & 0x7FF) x 6
     * against 0x90244 — stock fault 0x238 beyond 15; counted here */
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        const uint16_t sp = st != 0
            ? (uint16_t)(st[((ch & 1u) != 0u ? 20u : 10u) + (ch >> 1)] & 0x7FFu) : 0u;
        const uint16_t x6 = (uint16_t)(sp * 6u);
        const uint16_t sent = CELL_RQ_CUR_SENT[ch];
        const uint16_t d = x6 >= sent ? (uint16_t)(x6 - sent) : (uint16_t)(sent - x6);
        if (d > 15u) {
            s_rq_f238_n++;
        }
    }
    /* 0x901F2: the chip-ID rule (0x9110C == 0xBD) — the records' ID bytes
     * are not harvested here (0x921E0 not ported), so it stays idle and
     * 0x9091B keeps 0; the reconfiguration calls are not ported */
    if (CELL_REC_U8(0, REC_ID_LO) == 0xBDu) {
        const uint8_t id = CELL_REC_U8(0, REC_ID_HI);
        const uint8_t r = id < 0x10u ? 1u : (id < 0x11u ? 2u : 0u);
        if (r != CELL_RQ_REDFREQ) {
            CELL_RQ_REDFREQ = r;                /* (0x91E34 / 0x8FDB6 on 1: not ported) */
        }
    }
    /* 0x90232: the letters as applied, and the high-current chips */
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        uint8_t l = CELL_RQ_LETTER[ch];
        if (((CAL_HI_MASK >> ch) & 1u) != 0u && CELL_RQ_CUR[ch] >= 10000u &&
            CELL_RQ_CUR_SENT[ch] < 10000u) {
            hi = (uint16_t)(hi | (1u << (ch >> 1)));
        }
        if (l != CELL_RQ_LET_APPLIED[ch] && !rq_chip_enabled(ch)) {
            l = (uint8_t)'2';                   /* 0x90296 */
        }
        CELL_RQ_LET_APPLIED[ch] = l;
    }
    /* 0x902A8: the rounding offset, 3 stays, < 3 → 4, > 3 → 1 */
    if (CELL_RQ_ROUND != 3u) {
        CELL_RQ_ROUND = CELL_RQ_ROUND < 3u ? 4u : 1u;
    }
    {
        uint8_t base = CAL_MODE_B4;
        if (CELL_RQ_REDFREQ == 1u) {
            base = (uint8_t)(base | 0x20u);     /* 0x902CE: bset #5 */
        }
        for (uint8_t c = 0u; c < DRV_RQ_NCHIP; c++) {
            const uint8_t c0 = (uint8_t)(2u * c), c1 = (uint8_t)(2u * c + 1u);
            uint8_t l0 = CELL_RQ_LET_APPLIED[c0], l1 = CELL_RQ_LET_APPLIED[c1];
            bool ok0, ok1;
            uint8_t m;
            uint16_t sp0, sp1;

            /* OUR COMPOSE-TIME GUARD (not stock): the law already refused
             * full-on without a quiet rail; nothing may stage 'C' here */
            if (!quiet && l0 == (uint8_t)'C') { l0 = (uint8_t)'2'; s_rq_cguard_n++; }
            if (!quiet && l1 == (uint8_t)'C') { l1 = (uint8_t)'2'; s_rq_cguard_n++; }

            CELL_REC_U8(c, REC_CFG90) = CELL_RQ_CFG90[c];
            CELL_REC_U8(c, REC_CFG96) = CELL_RQ_CFG96[c];
            CELL_REC_U8(c, REC_CFG8E) = CELL_RQ_CFG8E[c];
            if (((hi >> c) & 1u) != 0u) {
                CELL_REC_U8(c, REC_CFG90) = (uint8_t)((CELL_REC_U8(c, REC_CFG90) & 0xC0u) | 2u);
            }
            m = (uint8_t)(base | rq_code(l0, &ok0) | (uint8_t)(rq_code(l1, &ok1) << 2));
            CELL_REC_U8(c, REC_MODE) = m;
            sp0 = rq_setpoint(c0, l0);
            CELL_REC_U16(c, REC_SP0) = sp0;
            sp1 = rq_setpoint(c1, l1);
            CELL_REC_U16(c, REC_SP1) = sp1;
            CELL_REC_U8(c, REC_DITH0) = rq_dither(c0, sp0, ok0);
            CELL_REC_U8(c, REC_DITH1) = rq_dither(c1, sp1, ok1);
        }
    }
    /* 0x924B0 / 0x92344 (staging + queue): drv_jobs.c. The 0x90919
     * frequency-reconfiguration tail (group B): not ported. */
}

/* ── init (0x90010's part) ────────────────────────────────────────────── */
void drv_rq_init(void)
{
    /* our own NOLOAD ranges (linker.ld .drv_rq1 .. .drv_rq6) */
    memset((void *)(uintptr_t)0xFFF90230u, 0, 0x28u);   /* 0x90230 / 0x90244 */
    memset((void *)(uintptr_t)0xFFF90320u, 0, 0x14u);   /* 0x90320 dither */
    memset((void *)(uintptr_t)0xFFF91108u, 0, 0x8Cu);   /* the records */
    memset((void *)(uintptr_t)0xFFF9337Cu, 0, 0x14u);   /* 0x9337C corrections */
    memset((void *)(uintptr_t)0xFFF93978u, 0, 0x06u);   /* 0x93978/7A/7C */
    memset((void *)(uintptr_t)0xFFF93A12u, 0, 0x02u);   /* 0x93A12 */
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        CELL_RQ_LETTER[ch] = (uint8_t)'2';              /* 0x90010: '2' ×10 */
        CELL_RQ_LET_APPLIED[ch] = (uint8_t)'2';
        CELL_RQ_DITH_ST[ch] = 0u;
        CELL_RQ_MODE[ch] = 0u;
        CELL_RQ_ACTIVE[ch] = 0u;
        CELL_RQ_I[ch] = 0u;
        CELL_RQ_I2[ch] = 0u;
#if !defined(__sh__)
        CELL_RQ_REQ[ch] = 0u;
        s_corr_inject[ch] = 0;
#endif
    }
    for (uint8_t c = 0u; c < DRV_RQ_NCHIP; c++) {
        CELL_RQ_CFG8E[c] = CFG_8E;
        CELL_RQ_CFG90[c] = CFG_90;
        CELL_RQ_CFG96[c] = CFG_96;
    }
    CELL_RQ_ROUND = CAL_ROUND_INIT;                     /* 0x90108 */
    CELL_RQ_REDFREQ = 0u;
    s_rq_ilock_n = 0u;
    s_rq_cguard_n = 0u;
    s_rq_f238_n = 0u;
}

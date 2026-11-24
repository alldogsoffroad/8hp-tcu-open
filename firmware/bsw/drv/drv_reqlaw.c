/* drv_reqlaw.c — the stock's solenoid request law (drv_reqlaw.h for the
 * safety frame). Ported from the bytes of 0x83DB0..0x8422C, 0x89540..
 * 0x89646, 0x90118..0x9019E, 0x901A0..0x903CA (sh-elf-objdump of the
 * pristine image; the decompiles of r08_app checked against them);
 * calibration through the 0x66xxx pointer tables at index 0x90AB8 = 0.
 * ~/tcu_sweep/request_law_port.md.
 *
 * THE OUTPUT DIAGNOSIS (2026-11-17, drv_odiag.c; output_diag_port.md):
 * 0x8422E, 0x89F5C, 0x7BA76 (the fault class), 0x93A42 (the channel
 * class), 0x897AC, 0x8A788, 0x84392 run in the stock order inside the
 * DIAGNOSTIC pass of the law (drv_rq_law_diag: ONE pass per output tick,
 * the stock's 0x7CC1E slot — drv_jobs.c drv_out_law, 2026-11-18);
 * 0x84644 / 0x8493A and this function's own inline 0x226 checks are
 * WITNESS ONLY (counted, never a reset). The plain drv_rq_law (tests and
 * the bench's drv_cyclic_build only) reads the same classes, no monitor.
 *
 * NOT PORTED (named stand-ins / deviations, request_law_port.md §3):
 *   0x6865C  fault 0x238 (counted only)
 *   0x837E0  adaptive correction — stand-in 0 (host: injectable)
 *   0x89648  dither demand → 0x90320 stays 0 (dump: 0)
 *   0x921E0  chip-ID harvest into the records → 0x9091B stays 0
 *   0x91E34 / 0x8FDB6 / the 0x90919 tail (reduced frequency, group B)
 */
/* READ-ONLY park position sensor (2026-11-16): this translation unit is
 * an output path — the sensor headers refuse to compile here (and the
 * ctest pksens_readonly_scan fails if this file names a sensor cell) */
#define TCU_OUTPUT_PATH 1
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_odiag.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/drv/drv_gsbr.h"     /* the garage bridge seam (G1: GS_LIVE_DIR 0, absent) */
#include <string.h>

#if defined(__sh__)
_Static_assert(DRV_RQ_REQUEST_SOURCE == 0,
               "drv_reqlaw: SH builds carry no solenoid request source");
#endif
#if DRV_RQ_USE_PRODUCER && !GS_G6B
#include "bsw/drv/drv_rqprod.h"
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
/* RESERVED (2026-11-18): r03x's s_rq_tick_full (the 10 ms rebuild's
 * full-on rule — the rebuild is gone, output_cadence_port.md); kept so no
 * .drv_wit witness moves */
#if defined(__sh__)
static uint16_t s_rq_rsv_tick_full WIT __attribute__((used));
#endif

/* M5-0 guard (.m5_wit at 0xFFF84D40, with app/rq_prod.c's counters) */
#if defined(__sh__) && DRV_RQ_USE_PRODUCER
static volatile uint32_t s_m5_guard_n __attribute__((section(".m5_wit")));
#else
static volatile uint32_t s_m5_guard_n;
#endif
uint32_t drv_rq_m5_guard_n(void) { return s_m5_guard_n; }

uint32_t drv_rq_ilock_n(void)  { return s_rq_ilock_n; }
uint32_t drv_rq_cguard_n(void) { return s_rq_cguard_n; }
uint32_t drv_rq_f238_n(void)   { return s_rq_f238_n; }

/* ── the request source ────────────────────────────────────────────────── */
uint16_t drv_rq_window_step(drv_rq_win_t *w, uint32_t now_ms, bool at_u, uint16_t req)
{
    const bool on = (req != 0u) && at_u;

    if (w->state == 2u) {
        return 0u;                              /* latched off until power-off */
    }
    if (w->state == 1u) {
        /* the release was on since the previous pass: count that time */
        w->acc_ms += (uint32_t)(now_ms - w->t_last_ms);
        w->acc_inv = ~w->acc_ms;
        w->t_last_ms = now_ms;
        if (w->acc_ms >= DRV_RQ_REL_WINDOW_MS) {
            w->state = 2u;
            return 0u;
        }
        if (!on) {
            w->state = 0u;                      /* off: the window closes, time kept */
            return 0u;
        }
        return req;
    }
    if (on) {                                   /* the release turns on: open */
        w->state = 1u;
        w->t_last_ms = now_ms;
        return req;
    }
    return 0u;
}

void drv_rq_window_boot(drv_rq_win_t *w, bool cold)
{
    if (cold) {
        w->acc_ms = 0u;                         /* power-on: a fresh budget */
    } else if (w->magic != DRV_RQ_WIN_MAGIC || w->acc_inv != ~w->acc_ms) {
        w->acc_ms = DRV_RQ_REL_WINDOW_MS;       /* warm reset, broken guard: fail safe, latched */
    }
    w->magic = DRV_RQ_WIN_MAGIC;
    w->acc_inv = ~w->acc_ms;
    w->t_last_ms = 0u;                          /* the ms clock restarts at boot */
    w->state = (w->acc_ms >= DRV_RQ_REL_WINDOW_MS) ? 2u : 0u;
}

#if defined(__sh__) && defined(DRV_RQ_PSET)
#include "bsw/sched/sched.h"
#include "platform/platform.h"
/* .noinit: survives MCU resets, cleared at power-on by drv_rq_window_boot */
static drv_rq_win_t s_rq_rel_win __attribute__((section(".noinit")));
#endif

uint8_t drv_rq_rel_win_state(void)
{
#if defined(__sh__) && defined(DRV_RQ_PSET)
    return s_rq_rel_win.state;
#else
    return 0u;
#endif
}

#if DRV_RQ_USE_PRODUCER
#if defined(__sh__)
#define rq_prod_on() true
_Static_assert(DRV_RQ_P0 == 500u && DRV_RQ_P1 == 8500u && DRV_RQ_P2 == 8500u &&
               DRV_RQ_P3 == 8500u && DRV_RQ_P4 == 8500u && DRV_RQ_P5 == 500u &&
               DRV_RQ_P6 == 500u,
               "drv_reqlaw: DRV_RQ_R03F_TABLE is DRV_RQ_P0..P6");
#else
static bool s_rq_prod_host = true;
void drv_rq_prod_host_select(bool on) { s_rq_prod_host = on; }
#define rq_prod_on() s_rq_prod_host
#endif

/* M5-0 / M5-1: the producer's channel 0..6 current through OUR GUARD (not
 * stock). Passes: (a) the two mode-1 currents 500 / 8500 with a good
 * complement; (b) M5-1, channels 0..4 only: a value in the stock fill
 * range [DRV_RQ_FILL_MIN, DRV_RQ_FILL_MAX] while that element's FSM is in a
 * fill state (1 quick fill, 2 ramp) — the mode-0 law 0x107C3E ends in the
 * clamp 0x107D6A → 0xB7786 to [i_min 500, i_max 8500]. Anything else (a
 * complement mismatch, RQ_PROD_INVALID, a fill value outside a fill state,
 * any non-mode-1 value on WK / Sys) is replaced by the r03f value of that
 * channel and counted. Before the producer's first pass (the boot pass at
 * boot state 1) the r03f values are used, uncounted (ours: r03n's wire). */
bool drv_rq_fill_ok(uint8_t ch, uint8_t st, uint16_t v)
{
    return ch < 5u && (st == 1u || st == 2u) &&
           v >= DRV_RQ_FILL_MIN && v <= DRV_RQ_FILL_MAX;
}

#if GS_G6B
/* r04r (2026-11-19, ~/tcu_sweep/r04r_port.md): THE LIVE HOLDER IS RETIRED.
 * The stock engine (the candidate, app/gs_cand.c: engine + holder + the
 * actuator laws in the stock 10 ms slot order) is the SOLE producer of
 * channels 0..6; app/rq_prod.c (the old holder-only 0xDC4C4 with the M5-0
 * compile-time inputs) is no longer in the image (a host-only reference).
 * Every pass the seam (drv_gsbr_take, app/gs_bridge.c) hands this law the
 * monitor's decision — the candidate, or the latched / stall-fence P content
 * — for ch 0..6, from the candidate's boot pass on:
 *   - ch 0..4 (A D B E C): the widened guard, as r04o..r04q: any current in
 *     the stock clamp [i_min 500, i_max 8500] (0xB7786) passes, anything
 *     else → the P content, counted;
 *   - ch 5 / 6 (WK / Sys): the engine's 0xDC4C4 sends Sys (source 0x15) and
 *     WK (source 0x16) through 0x10809C — 500 each with the reaction-50 /
 *     WK-off stand-ins (gs_engine.c GS_LINEP / GS_SYSMODE, D-G6-10). The
 *     guard passes ONLY the P content's compile-time 500 (DRV_RQ_PCONT_WK /
 *     _SYS): any other engine value → 500, counted. The wire's WK / Sys
 *     cannot leave 500 on this build.
 * Before the boot pass (the seam empty) the r03f table, uncounted (D-G6-15;
 * r04q: until rq_prod_started(), the old holder's boot pass, which ran in
 * the same row-1 call just before the candidate's — the seam's first value
 * comes from gs_bridge_boot_pass in that call, no F stage in between). */
#define RQ_ENG_NCH  7u          /* ch 0..6: A D B E C WK Sys */
#if GS_LIVE_DIR != 0            /* always here: G6b needs the seam (drv_gsbr.h) */
static uint16_t rq_prod_guarded(uint8_t ch)
{
    static const uint16_t R03F[RQ_ENG_NCH] = DRV_RQ_R03F_TABLE;
    static const uint16_t PCONT[RQ_ENG_NCH] = DRV_RQ_PCONT_TABLE;
    uint16_t v;
    if (!drv_gsbr_take(ch, &v)) {
        return R03F[ch];                /* before the engine's boot pass: r03n's wire (uncounted) */
    }
    if (ch < 5u) {
        if (v >= DRV_RQ_FILL_MIN && v <= DRV_RQ_FILL_MAX) {
            return v;
        }
    } else if (v == PCONT[ch]) {
        return v;
    }
#if DRV_RQ_SYS_LIVE
    /* NISSAN_LP_LIVE (drv_gsbr.h, app/nissan_lp.h): Sys (ch 6) may carry the healthy
     * line-pressure law's current, inside the stock's Sys range; WK (ch 5) stays 500-only */
    if (ch == 6u && v >= DRV_RQ_SYS_LIVE_MIN && v <= DRV_RQ_SYS_LIVE_MAX) {
        return v;
    }
#endif
    s_m5_guard_n++;                     /* never in a sound build */
    return PCONT[ch];
}
#endif /* GS_LIVE_DIR != 0 */
_Static_assert(DRV_RQ_PCONT_WK == 500u && DRV_RQ_PCONT_SYS == 500u,
               "drv_reqlaw: WK / Sys stay the compile-time 500 (D-G6-10)");
#if DRV_RQ_SYS_LIVE
_Static_assert(DRV_RQ_SYS_LIVE_MIN == DRV_RQ_PCONT_SYS && DRV_RQ_SYS_LIVE_MAX == 8500u,
               "drv_reqlaw: the Nissan live Sys range is the stock's [500, 8500], its floor the P content");
#endif
#else
/* the holder-fed form (host M5 reference tests only since r04r; the image's
 * r03n..r04n form): app/rq_prod.c's channel 0..6 current through the guard;
 * the r03f table before its boot pass and as the fallback */
static uint16_t rq_prod_guarded(uint8_t ch)
{
    static const uint16_t R03F[RQ_PROD_NCH] = DRV_RQ_R03F_TABLE;
    uint16_t v;
    if (!rq_prod_started()) {
        return R03F[ch];                /* before the boot pass: r03n's wire (uncounted, not a fallback) */
    }
    v = rq_prod_ch(ch);
    if (v == 500u || v == 8500u) {
        return v;
    }
    if (v != RQ_PROD_INVALID && drv_rq_fill_ok(ch, rq_prod_fill_state(ch), v)) {
        return v;
    }
    s_m5_guard_n++;
    return R03F[ch];
}
#endif /* GS_G6B (r04r) */
#endif

static inline uint16_t rq_request(uint8_t ch)
{
#if DRV_RQ_USE_PRODUCER && GS_G6B
    if (ch < RQ_ENG_NCH && rq_prod_on()) {
        return rq_prod_guarded(ch);
    }
#elif DRV_RQ_USE_PRODUCER
    if (ch < RQ_PROD_NCH && rq_prod_on()) {
        return rq_prod_guarded(ch);
    }
#endif
#if DRV_RQ_REQUEST_SOURCE
    return CELL_RQ_REQ[ch];
#elif defined(__sh__) && defined(DRV_RQ_PSET)
    /* r03f: the stock P set (drv_reqlaw.h), compile-time constants (ch 7:
     * the bound of the park-release demand) */
    static const uint16_t PSET[10] = DRV_RQ_PSET_TABLE;
    if (ch == 7u) {
        /* the park release: the ported range/park logic's demand (stock
         * slot getter 0xA469A ← 0xE8888(id 2), app/park.c), passed only
         * when it equals the compile-time bound DRV_RQ_P7 (0 or 10000),
         * then our D-M4-7 on-time limit */
        const uint16_t v = (PSET[7] != 0u) ? park_slot7_release() : 0u;
        const uint16_t r = (PSET[7] != 0u && v == PSET[7]) ? PSET[7] : 0u;
        return drv_rq_window_step(&s_rq_rel_win, sched_now_ms(),
                                  CELL_SEQ2_STATE[0] == (uint8_t)'U', r);
    }
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

/* OUR RAIL-OPEN GUARD (not stock; 2026-11-17, fullon_interlock_decode.md).
 * The stock commands full-on (mode 4 / letter 'C'; group A duty 1000) with
 * no check of its own: its sequence alone keeps the supervisor rail open.
 * On this calibration that is ONE tick per ladder run [O bytes]: phase
 * 0x10's entry (0x94114 = the alive group, 0x8BD6A; cleared on the phase's
 * pass, 0x8BDB2) — phase 4's 0x94114 is group 0 = 0 (0x1915A4[0] = 2) and
 * the '3' diagnosis' 0x94115 is never set (0x1915B8 bit 0 = 0, 0x1919C9 =
 * 0). In that tick: phase 8's pass wrote both request cells 0 in the same
 * 0x8B7A8 call, seq2 0x11 copied the request into the enable (0), so this
 * tick's J2/F word is 0x9000; the previous tick carried phase 8's one
 * 0x9020 frame at walk level >= 5 with 0x14 bit 1 = 0, which the chip
 * refused (phase 8 passes on class 0x22 in the very call that enters 0x10).
 *
 * The guard reads THIS tick's state (the law runs after the ladder and seq2
 * and before the J2 stage, stock list order) — r02w..r03w read the staged
 * J2 word, which is the PREVIOUS tick's there, and refused the stock's
 * test on every boot (8 channels = s_rq_ilock_n 8). Full-on passes only if
 * ALL hold:
 *   1. both request cells 0xFFF939B6 are 0 (0x85E64 ran before us);
 *   2. seq2's enable 0x93A90 is 0 on both slots and no letter 'c' is up
 *      (0x8C7A8 ran before us): this tick's 0x907D6 bits 5..7 are 0, the
 *      J2/F word composed after us is 0x9000;
 *   3. the output node 0x94ED0[0] is not above the off band: <= 4000 mV
 *      (cal 0x1912C8, the class-0x22 top; ON reads ~supply);
 *   4. the word last staged is 0x9000, or the chip does not certify
 *      (0x90959 = 0x14 bit 1 = 0): a rail command heard by a non-certifying
 *      chip does not close the rail [O the ladder's own expectation in
 *      phases 2..8; I the chip side].
 * Each term is true in the stock's test tick (host: test_fullon). */
#define RQ_RAIL_OFF_MV  4000u       /* 0x1912C8 (sup_ladder CAL_C2) */

bool drv_rq_rail_quiet(void)
{
    return CELL_LAD_REQ[0] == 0u && CELL_LAD_REQ[1] == 0u &&
           CELL_EN_MIRROR[0] == 0u && CELL_EN_MIRROR[1] == 0u &&
           CELL_LETTERS[0] != (uint8_t)'c' && CELL_LETTERS[1] != (uint8_t)'c' &&
           CELL_LETTERS[2] != (uint8_t)'c' &&
           CELL_OUT_MV[0] <= RQ_RAIL_OFF_MV &&
           (CELL_J2_STAGED == 0x9000u || CELL_SSLB0_BIT2 == 0u);
}

/* 0x8FD94: chip ch >> 1 present in cal 0x1918C1 */
static bool rq_chip_enabled(uint8_t ch)
{
    const uint8_t chip = (uint8_t)(ch >> 1);
    return chip < 5u && ((CAL_CHIP_EN >> chip) & 1u) != 0u;
}

/* 0x7BA76: 2 only with a fault word 0x93004[ch] != 0 AND the request copy
 * 0x84890[ch] == 0 (an unrequested channel), else 1 (drv_odiag.c) */
static uint8_t rq_fault_class(uint8_t ch) { return drv_od_7ba76(ch); }

/* 0x93A42[ch]: the channel class of the off-state test machine 0x8E8E4
 * (drv_odiag.c; 0 at rest — dump 00 ×10) */
static uint8_t rq_chan_class(uint8_t ch) { return drv_od_chan_class(ch); }

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
/* diag: the stock's full pass with its monitors (drv_odiag.c), once per
 * output tick (drv_out_law); !diag: the same law without the monitors
 * (tests / drv_cyclic_build), reading the classes the last diagnostic
 * pass left */
#if defined(TCU_RAM_APP)
#define RQ_DIAG(d) false        /* the RAM application carries no diagnosis (64 KB) */
#else
#define RQ_DIAG(d) (d)
#endif

static void rq_law_run(bool diag_req)
{
    const bool diag = RQ_DIAG(diag_req);
    (void)diag_req;
    drv_out_note_law();                         /* the witness: law passes */
    /* M5-1: the producer no longer runs here — it runs at the stock cadence
     * (boot state 1, then the 10 ms task in modes 1..3: drv_rqprod.h); the
     * law reads its last 0xFFF84B78 through the guard */
    uint16_t full_mask = 0u;                    /* @(4,r15) → 0x9397C */
    uint16_t fault_mask = 0u;                   /* @(8,r15) → 0x9397A */
    uint16_t req_all[DRV_RQ_NCH];
    uint8_t loops = 0u;

    /* 0x7B6D8 (before the law): the request of every channel this pass —
     * the stock's 0x84890 slot array / 0x94D60 complement (0x94EE8 itself
     * is never written on SH: the safety frame) */
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        req_all[ch] = rq_request(ch);
    }
    if (diag) {
        drv_od_requests(req_all);
        drv_od_8422e();                         /* 0x8422E */
    }
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        const uint16_t bit = (uint16_t)(1u << ch);
        bool ilock = false;

        loops++;
        if (!rq_chip_enabled(ch) || (CAL_CH_MASK & bit) == 0u) {
            /* 0x84156: the channel is not driven */
            CELL_RQ_I2[ch] = 0u;
            CELL_RQ_I[ch] = 0u;
            CELL_RQ_MODE[ch] = 0u;
            CELL_RQ_CUR[ch] = 0u;
            CELL_RQ_LETTER[ch] = (uint8_t)'2';
        } else {
            const uint16_t rail = CELL_OUT_MV[0];           /* 0x94ED0, read per channel */
            const uint8_t s = CELL_SEQ2_STATE[0];           /* 0x85DC4(0) */
            const uint8_t s_b = CELL_SEQ2_STATE[0];         /* 0x85DC4(0), the second read */
            const bool t15 = (CELL_DIAG_94115 & 1u) != 0u;  /* 0x83E42 */
            const bool t14 = (CELL_LAD_94114 & 1u) != 0u;   /* 0x83E4A */
            uint8_t fc;
            const uint16_t req = req_all[ch];
            const bool test_state = s == 0x11u || s == 0x16u || s == (uint8_t)'3';
            uint8_t mode = 1u;                              /* r12 */
            uint8_t inv_m = 0xFEu;                          /* bVar18: ~mode */
            uint16_t inv_i = 0xFFFFu;                       /* puVar13: ~I */
            bool fault = false;
            uint16_t i = 0u;                                /* r11 */
            uint8_t cls;

            if (diag && drv_od_enabled()) {
                drv_od_89f5c(ch);                           /* 0x89F5C (from pass 2) */
            }
            fc = rq_fault_class(ch);                        /* 0x7BA76 */
            if (fc != 0u && fc != 1u) {                     /* 0x83E7E..0x83EA0 */
                mode = 0u;
                inv_m = 0xFFu;
                fault = true;
                fault_mask |= bit;
            }
            CELL_RQ_ACTIVE[ch] = (req != 0u && !fault) ? 1u : 0u;   /* 0x83EA6 */
            if (s == (uint8_t)'U' && !fault) {              /* 0x83ECE: THE 'U' GATE */
                i = req;
                inv_i = (uint16_t)~req;
            }
            if (i > I_CLAMP) {                              /* 0x83EDE */
                i = I_CLAMP;
                inv_i = (uint16_t)~I_CLAMP;
            }
            cls = rq_chan_class(ch);                        /* 0x93A42 */
            if (cls == 2u) {
                mode = 2u;
                inv_m = 0xFDu;
            } else if (cls == 1u || cls == 0u) {
                if (i != 0u) {
                    mode = 3u;                              /* current control */
                    inv_m = 0xFCu;
                }
            } else {
                mode = 0u;
                inv_m = 0xFFu;
            }
            if (test_state) {                               /* 0x83F1C */
                const bool full =
                    (t15 && (CAL_FULL15 & bit) != 0u) ||
                    (t14 && (s == 0x11u || s == 0x16u) && (CAL_FULL14 & bit) != 0u);
                if (full) {
                    mode = 4u;                              /* 0x83F72: FULL ON */
                    inv_m = 0xFBu;
                    full_mask |= bit;
                } else {
                    i = 0u;                                 /* 0x83F84 */
                    inv_i = 0xFFFFu;
                    mode = 0u;
                    inv_m = 0xFFu;
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
                            inv_i = (uint16_t)~I_CLAMP;
                            mode = 4u;
                            inv_m = 0xFBu;
                        }
                    } else if ((CAL_UV_FLAGS & 1u) != 0u) { /* 0x84002 */
                        CELL_RQ_UVFLAGS = (uint16_t)(CELL_RQ_UVFLAGS | bit);
                        i = 0u;
                        inv_i = 0xFFFFu;
                        mode = 0u;
                        inv_m = 0xFFu;
                    }
                }
            }
            /* the minimum-current law (0x84084) */
            if (i < CAL_IMIN && i != 0u && CAL_TYPE == 1u) {
                i = 0u;
                inv_i = 0xFFFFu;
                mode = 0u;
                inv_m = 0xFFu;
            }
            /* OUR RAIL-OPEN GUARD (not stock; drv_rq_rail_quiet): never
             * fires in the stock's sequence — only a full-on with the rail
             * (possibly) closed is refused and counted. (2026-11-18: the
             * 10 ms rebuild and its "repeat only" rule are gone — one law
             * pass per output tick, in the stock's slot.) */
            if (mode == 4u && !drv_rq_rail_quiet()) {
                mode = 0u;
                inv_m = 0xFFu;
                i = 0u;
                inv_i = 0xFFFFu;
                full_mask = (uint16_t)(full_mask & (uint16_t)~bit);
                s_rq_ilock_n++;
                ilock = true;
            }
            CELL_RQ_I[ch] = i;                              /* 0x840C4 */
            CELL_RQ_MODE[ch] = mode;                        /* 0x840C8 */
            CELL_RQ_FULLMASK = full_mask;                   /* 0x840D2 */
            CELL_RQ_FAULTMASK = fault_mask;                 /* 0x840DE */
            if (diag) {
                drv_od_897ac(ch);                           /* 0x897AC */
            }
            CELL_RQ_I2[ch] = i;                             /* 0x840E6 */
            rq_letters(ch);                                 /* 0x89540 */
            if (diag) {
                drv_od_8a788(ch);                           /* 0x8A788 */
                /* the inline 0x226 checks (WITNESS ONLY) */
                if (i != (uint16_t)~inv_i) {
                    drv_od_fatal_226(0u, ch);
                }
                if (mode != (uint8_t)~inv_m) {
                    drv_od_fatal_226(1u, ch);
                }
                if (s != s_b) {
                    drv_od_fatal_226(2u, ch);
                }
            }
        }
        if (diag) {
            drv_od_ch_tail(ch, ilock);                      /* 0x84644 / 0x84392 / 0x8493A */
        }
    }
    if (diag) {
        drv_od_pass_end(loops);                     /* 0x841D6: 0x84A22 = 1 / 0x84A23 = 0xFE */
    }
}

void drv_rq_law(void)         { rq_law_run(false); }
void drv_rq_law_diag(void)    { rq_law_run(true); }

/* ── 0x90118: one channel's setpoint ──────────────────────────────────── */
static uint16_t rq_cur_of(uint8_t ch, uint8_t letter)   /* 0x90118's current */
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
    return cur;
}

/* RESERVED (2026-11-18): r03x's s_rq_cur_staged / s_rq_hi_pend, the
 * compose-cells patch (0x901A0 now runs every F pass, so 0x90244 is the
 * stock's "last pass composed" by construction); kept so no .drv_wit
 * witness moves */
#if defined(__sh__) && !defined(TCU_RAM_APP)
static uint16_t s_rq_rsv_cur_staged[DRV_RQ_NCH] WIT __attribute__((used));
static uint16_t s_rq_rsv_hi_pend WIT __attribute__((used));
#endif

static uint16_t rq_setpoint(uint8_t ch, uint8_t letter)
{
    const uint16_t cur = rq_cur_of(ch, letter);
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

    drv_out_note_compose();                     /* the witness: compose passes */
    /* 0x901BC: the staged setpoints (0x92A60: E8 / F0 rows & 0x7FF) x 6
     * against 0x90244 — stock fault 0x238 beyond 15; counted here. Both
     * are the last pass's (the compose runs every F pass, the staging when
     * the cyclic was free) */
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        const uint16_t sp = st != 0
            ? (uint16_t)(st[((ch & 1u) != 0u ? 20u : 10u) + (ch >> 1)] & 0x7FFu) : 0u;
        const uint16_t x6 = (uint16_t)(sp * 6u);
        const uint16_t sent = CELL_RQ_CUR_SENT[ch]; /* 0x90244: the last pass's */
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
#if defined(__sh__) && defined(DRV_RQ_PSET)
    drv_rq_window_boot(&s_rq_rel_win, plat_boot_index() <= 1u);
#endif
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
#if defined(__sh__) && !defined(TCU_RAM_APP)
    memset(s_rq_rsv_cur_staged, 0, sizeof s_rq_rsv_cur_staged);  /* NOLOAD */
    s_rq_rsv_hi_pend = 0u;
#endif
#if defined(__sh__)
    s_rq_rsv_tick_full = 0u;
#endif
    s_rq_f238_n = 0u;
    s_m5_guard_n = 0u;
    drv_od_init();                      /* the output diagnosis' cells */
#if DRV_RQ_USE_PRODUCER && !GS_G6B
    rq_prod_init();                     /* M5-0: 0xD9CF8 + the objects (r04r: not on the image) */
#endif
}

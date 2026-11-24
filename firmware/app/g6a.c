/* g6a.c — G6a, THE DUAL-RUN COMPARATOR (ours, not stock; 2026-11-19,
 * ~/tcu_sweep/g6a_port.md). The frame and the classes: app/g6a.h.
 * G6c (~/tcu_sweep/g6c_port.md): the comparison with the shadow is retired
 * with the shadow; what stays is the wire self-check (compare() below) and
 * the CPU witnesses.
 *
 * READ-ONLY: this TU writes only its own object (.g6a, s_g6a) and, through
 * the candidate's API, the candidate's own RAM (.gsc_req / .gsc_obj). It
 * READS the live path through getters and const volatile literals only:
 *   DRV_RQ_R03F_TABLE, DRV_RQ_PCONT_TABLE, DRV_RQ_FILL_MIN / MAX
 *                                                       (the guard, replicated)
 *   drv_out_wit()->law_n                                (law passes)
 *   gs_bridge_state()->applied / ->src / ->app[]        (the seam's value —
 *       NOT drv_gsbr_take, which counts its reads)
 *   gs_cand_* (the engine; G6c: the shadow, instance 1, is deleted)
 *   0xFFF94ED4 (the law's gated current), 0xFFF94FD0 (seq2), plat_frt_us.
 * r04r (~/tcu_sweep/r04r_port.md): the live holder (app/rq_prod.c) is
 * retired — its getters and its request objects 0xFFF8B7C8.. are no longer
 * read; the replica below is drv_reqlaw.c's r04r rq_prod_guarded().
 * ctest g6a_scan_cmp holds this TU to that list (no other call, no store
 * through a literal address, no CELL_ / PCELL_ macro).
 */
#include "app/g6a.h"
#include "app/gs_cand.h"
#include "app/gs_bridge.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_gsbr.h"      /* G6b: GS_G6B, the P content (DRV_RQ_PCONT_TABLE) */
#include "platform/platform.h"

#if defined(__sh__)
#  define G6A_SEC __attribute__((section(".g6a")))
#else
#  define G6A_SEC
#endif

static volatile g6a_t s_g6a G6A_SEC;
#define C   (*(g6a_t *)&s_g6a)

#define LRD8(a)     (*(const volatile uint8_t *)(uintptr_t)(a))
#define LRD16(a)    (*(const volatile uint16_t *)(uintptr_t)(a))
#define G6A_NCH     7u          /* ch 0..6: A D B E C WK Sys */
#if DRV_RQ_SYS_LIVE
#  define G6A_LAW_NCH 6u        /* NISSAN_LP_LIVE: Sys (ch 6) not replicable here (wire_cur) */
#else
#  define G6A_LAW_NCH 7u
#endif
#define LAW_I(ch)       LRD16(0xFFF94ED4u + 2u * (uint32_t)(ch))   /* 0x94ED4[ch] gated current */
#define SEQ2_0          LRD8(0xFFF94FD0u)                         /* seq2 state, channel 0 */

void g6a_init(void)
{
    volatile uint8_t *b = (volatile uint8_t *)&s_g6a;
    for (unsigned i = 0u; i < sizeof s_g6a; i++) {
        b[i] = 0u;
    }
    gs_cand_init();
    C.first_unexp = 0xFFFFFFFFu;
    C.last_unexp = 0xFFFFFFFFu;
    C.boot_close = 0xFFFFu;
    C.boot_open = 1u;
    C.magic = G6A_MAGIC;
}

/* the wire source this pass: what rq_prod_guarded() (drv_reqlaw.c, the r04r
 * form) returns to the F stage after this 10 ms task, replicated without its
 * counters: before the candidate's boot pass (the seam empty) the r03f
 * table; then the seam's value — A..E the monitor's applied value through
 * the widened guard, WK / Sys the candidate's current (or the P content's
 * while latched / fenced) through the 500-only guard */
static uint16_t wire_cur(uint8_t ch, bool applied, const volatile gsb_t *b, bool *guard)
{
    static const uint16_t R03F[G6A_NCH] = DRV_RQ_R03F_TABLE;
#if !DRV_RQ_USE_PRODUCER || !GS_G6B
    /* a host build whose law does not take the engine (tcu_sim): no
     * meaningful wire value — the comparison is the SH configuration's */
    (void)applied;
    (void)b;
    (void)guard;
    return R03F[ch];
#else
    static const uint16_t PCONT[G6A_NCH] = DRV_RQ_PCONT_TABLE;
    uint16_t v;
    if (!applied) {
        return R03F[ch];
    }
    if (ch < 5u) {
        v = b->app[ch];
        if (v >= DRV_RQ_FILL_MIN && v <= DRV_RQ_FILL_MAX) {
            return v;
        }
    } else {
#if DRV_RQ_SYS_LIVE
        if (ch == 6u) {
            /* NISSAN_LP_LIVE (app/nissan_lp.h): the seam's Sys is the line-pressure shadow's
             * value, computed AFTER this comparison in the 10 ms task (lp_hook_10ms) - not
             * replicable here: the candidate's own Sys is recorded and channel 6 is left out
             * of the law cross-check (G6A_LAW_NCH); the Nissan chain test checks it instead */
            return gs_cand_cur(ch);
        }
#endif
        v = (b->src == GSM_S_CAND) ? gs_cand_cur(ch) : PCONT[ch];
        if (v == PCONT[ch]) {
            return v;
        }
    }
    *guard = true;
    return PCONT[ch];
#endif
}

static void put_rec(volatile g6a_rec_t *r, uint8_t cls, uint8_t dcur, uint16_t dms, uint8_t src,
                    const uint16_t cand[7], const uint16_t wire[7])
{
    r->pass = (uint16_t)(C.passes > 0xFFFFu ? 0xFFFFu : C.passes);
    r->cls = cls;
    r->dcur = dcur;
    r->dms = dms;
    r->src = src;
    r->e28e_pf = (uint8_t)((gs_cand_wit()->e28e & 0x0Fu) | (uint8_t)(gs_cand_pf() << 4));
    r->cph[0] = gs_cand_phase(0u);
    r->cph[1] = gs_cand_phase(2u);
    r->sph[0] = 0u;                                      /* G6c: no shadow */
    r->sph[1] = 0u;
    for (unsigned c = 0u; c < 5u; c++) {
        r->cand[c] = cand[c];
        r->wire[c] = wire[c];
    }
}

/* G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): THE WIRE SELF-CHECKS. The
 * shadow (instance 1, the comparator's reference) is deleted (D-G6-7), so
 * the candidate-vs-shadow classes are retired. What stays, per pass:
 *   - the wire value the F stage takes (rq_prod_guarded() replicated without
 *     its counters: the seam's value — the candidate, or the latched P
 *     content —, else the E28E fence's live producer through the guard);
 *   - the law cross-check (the law's gated 0x94ED4 = the previous pass's
 *     replicated wire value — proves the replica IS the seam + guard);
 *   - the guard replica (a fallback would be counted here);
 *   - candidate vs wire: AGREE (equal), BOOT (inside the boot window: the
 *     E28E boot fence), FENCED (the monitor chose the live producer or the
 *     latched P content — the difference is the monitor's decision), UNEXP
 *     (the seam carried the candidate and the wire differs: only a guard
 *     fallback can do that). r04o's FSMTAIL (candidate != shadow in
 *     non-filling states only) counts as AGREE now. */
static void compare(void)
{
    const volatile gsb_t *b = gs_bridge_state();
    const bool applied = b->applied != 0u;
    const uint8_t src = b->src;
    const bool started = applied;                        /* r04r: the seam's first value = the start */
    uint16_t cand[7], wire[7];
    uint8_t dcur = 0u, filling = 0u;
    uint16_t dms = 0u;
    bool guard = false;
    uint8_t cls, srcf;

    {
        const uint32_t ln = drv_out_wit()->law_n;
        if (C.prev_ok != 0u && ln != C.prev_law && SEQ2_0 == (uint8_t)'U') {
            C.law_n++;
            for (uint8_t ch = 0u; ch < G6A_LAW_NCH; ch++) {
                if (LAW_I(ch) != C.prev_app[ch]) {
                    C.law_x++;
                    break;
                }
            }
        }
        C.prev_law = ln;
    }
    for (uint8_t ch = 0u; ch < G6A_NCH; ch++) {
        cand[ch] = gs_cand_cur(ch);
        wire[ch] = wire_cur(ch, applied, b, &guard);
        C.prev_app[ch] = wire[ch];
        if (cand[ch] != wire[ch]) {
            dcur = (uint8_t)(dcur | (1u << ch));
        }
        if (ch < 5u) {
            const uint8_t cm = gs_cand_reqmode(ch), cs = gs_cand_fillst(ch);
            if (src != GSM_S_CAND && cm != 1u) {
                /* the P content (latched / the stall fence) and, before the
                 * boot pass (src LIVE: the seam empty), the r03f table are
                 * mode-1 content. r04r: the LIVE case read the retired
                 * holder's request objects; compare() never runs before the
                 * boot pass (g6a_boot_compare follows it, the 10 ms compare
                 * is modes 1..3), so the case is unreachable either way */
                dms = (uint16_t)(dms | (1u << ch));
            }
            if (cs == 1u || cs == 2u) {
                filling = 1u;
            }
        }
    }
    C.prev_ok = 1u;
    if (guard) {
        C.guard_x++;
    }
    if (applied) {
        C.applied_n++;
    }
    srcf = (uint8_t)(((src == GSM_S_CAND) ? G6A_S_APPLIED : 0u) | ((b->owns != 0u) ? G6A_S_OWNED : 0u) |
                     (C.boot_open ? G6A_S_BOOTWIN : 0u) | (gs_cand_releasing() ? G6A_S_CREL : 0u) |
                     (started ? G6A_S_STARTED : 0u));
    if (dcur == 0u && dms == 0u && C.boot_open != 0u && gs_cand_wit()->e28e == 0u && filling == 0u &&
        src == GSM_S_CAND) {
        C.boot_open = 0u;                                /* the boot window ends before this pass */
        C.boot_close = (uint16_t)(C.passes > 0xFFFEu ? 0xFFFEu : C.passes);
    }
    if (C.boot_open != 0u && C.passes >= G6A_BOOT_MAX) {
        C.boot_open = 0u;                                /* the window's cap */
        C.boot_close = (uint16_t)C.passes;
    }
    if (C.boot_open != 0u) {
        cls = (uint8_t)((dcur == 0u && dms == 0u) ? G6A_C_AGREE : G6A_C_BOOT);
        if (cls == G6A_C_BOOT) {
            C.boot_dis++;
        }
    } else if (dcur == 0u && dms == 0u) {
        cls = G6A_C_AGREE;
    } else if (src != GSM_S_CAND) {
        cls = G6A_C_FENCED;                              /* the monitor fenced / latched */
    } else {
        cls = G6A_C_UNEXP;                               /* the seam carried the candidate, the wire differs */
    }
    C.cls_n[cls]++;
    if (cls != G6A_C_AGREE) {
        if (dms != 0u) {
            C.ms_dis_n++;
        }
        for (uint8_t ch = 0u; ch < 7u; ch++) {
            if ((dcur & (1u << ch)) != 0u) {
                C.dis_ch[ch]++;
                if (cls == G6A_C_UNEXP) {
                    C.unexp_ch[ch]++;
                }
            }
        }
        if (C.rec_n < G6A_REC_N) {
            put_rec(&s_g6a.rec[C.rec_n], cls, dcur, dms, srcf, cand, wire);
            C.rec_n++;
        }
        if (cls == G6A_C_UNEXP) {
            if (C.first_unexp == 0xFFFFFFFFu) {
                C.first_unexp = C.passes;
            }
            C.last_unexp = C.passes;
            if (C.urec_n < G6A_UREC_N) {
                put_rec(&s_g6a.urec[C.urec_n], cls, dcur, dms, srcf, cand, wire);
                C.urec_n++;
            }
        }
    }
    C.passes++;
}

static uint16_t us16(uint32_t d)
{
    return (uint16_t)(d > 0xFFFFu ? 0xFFFFu : d);
}

void g6a_boot(const gs_in_t *gi)
{
    const uint32_t t0 = plat_frt_us();
    gs_cand_boot_pass(gi);
    C.boot_us = us16(plat_frt_us() - t0);
    C.booted = 1u;
}

/* r04q: the boot pass's comparison, AFTER the monitor's boot evaluation
 * (app/main.c g6a_boot_pass: g6a_boot → gs_bridge_boot_pass → this), so it
 * sees what the seam hands the F stages from the boot pass on */
void g6a_boot_compare(void)
{
    compare();
}

/* G6b: a slot-group mark of the candidate's tick (app/gs_engine.c GSE_STAGE
 * via app/gs_cand_bind.h) — the group's duration since the previous mark */
void g6a_stage(unsigned k)
{
    const uint32_t now = plat_frt_us();
    if (k < G6A_NST) {
        const uint16_t d = us16(now - C.st_t0);
        C.st_last[k] = d;
        if (d > C.st_max[k]) {
            C.st_max[k] = d;
        }
    }
    C.st_t0 = now;
}

void g6a_cand_tick(const gs_in_t *gi)
{
    uint32_t t0;
    if (gi->os_mode < 1u || gi->os_mode > 3u) {
        return;                                          /* the stock's lists: modes 1..3 */
    }
    for (unsigned k = 0u; k < G6A_NST; k++) {
        C.st_last[k] = 0u;
    }
    t0 = plat_frt_us();
    C.st_t0 = t0;
    gs_cand_tick_10ms(gi);
    C.cand_us_last = us16(plat_frt_us() - t0);
    if (C.cand_us_last > C.cand_us_max) {
        C.cand_us_max = C.cand_us_last;
        C.cand_max_pass = C.passes;
        for (unsigned k = 0u; k < G6A_NST; k++) {
            C.st_atmax[k] = C.st_last[k];
        }
    }
}

void g6a_compare_10ms(uint8_t os_mode)
{
    uint32_t t1;
    if (os_mode < 1u || os_mode > 3u) {
        return;
    }
    t1 = plat_frt_us();
    compare();
    C.cmp_us_last = us16(plat_frt_us() - t1);
    if (C.cmp_us_last > C.cmp_us_max) {
        C.cmp_us_max = C.cmp_us_last;
    }
}

void g6a_time(unsigned what, uint32_t us)
{
    const uint16_t d = us16(us);
    if (what == G6A_T_MON) {
        C.mon_us_last = d;
        if (d > C.mon_us_max) {
            C.mon_us_max = d;
        }
    }
}

void g6a_task_time(uint8_t os_mode, uint32_t us)
{
    if (os_mode < 1u || os_mode > 3u) {
        return;
    }
    C.task_n++;
    C.task_us_last = us16(us);
    if (C.task_us_last > C.task_us_max) {
        C.task_us_max = C.task_us_last;
    }
    if (us > 1000u) {
        C.task_over++;
    }
}

const volatile g6a_t *g6a_state(void)
{
    return &s_g6a;
}

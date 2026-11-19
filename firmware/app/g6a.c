/* g6a.c — G6a, THE DUAL-RUN COMPARATOR (ours, not stock; 2026-11-19,
 * ~/tcu_sweep/g6a_port.md). The frame and the classes: app/g6a.h.
 *
 * READ-ONLY: this TU writes only its own object (.g6a, s_g6a) and, through
 * the candidate's API, the candidate's own RAM (.gsc_req / .gsc_obj). It
 * READS the live path through getters and const volatile literals only:
 *   rq_prod_started / rq_prod_ch / rq_prod_fill_state   (app/rq_prod.c)
 *   drv_rq_fill_ok, DRV_RQ_R03F_TABLE                   (the guard, replicated)
 *   drv_out_wit()->law_n                                (law passes)
 *   gs_bridge_state()->applied / ->app[]                (the seam's value —
 *       NOT drv_gsbr_take, which counts its reads)
 *   gs_shadow_* (instance 1), gs_cand_* (instance 2)
 *   0xFFF8B7C8.. (+7 mode, +0x34 state of the live request objects),
 *   0xFFF94ED4 (the law's gated current), 0xFFF94FD0 (seq2), plat_frt_us.
 * ctest g6a_scan_cmp holds this TU to that list (no other call, no store
 * through a literal address, no CELL_ / PCELL_ macro).
 */
#include "app/g6a.h"
#include "app/gs_cand.h"
#include "app/gs_bridge.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_jobs.h"
#include "platform/platform.h"

#if defined(__sh__)
#  define G6A_SEC __attribute__((section(".g6a")))
#else
#  define G6A_SEC
#endif

static volatile g6a_t s_g6a G6A_SEC;
#define C   (*(g6a_t *)&s_g6a)

/* the live request objects behind ch 0..4 (A D B E C; rq_fill.h RQ_OBJ_*) */
static const uint32_t LIVE_OBJ[5] = { 0xFFF8B7C8u, 0xFFF8B8E8u, 0xFFF8B828u, 0xFFF8B948u, 0xFFF8B888u };
#define LRD8(a)     (*(const volatile uint8_t *)(uintptr_t)(a))
#define LRD16(a)    (*(const volatile uint16_t *)(uintptr_t)(a))
#define LIVE_MODE(ch)   LRD8(LIVE_OBJ[ch] + 0x07u)
#define LIVE_ST(ch)     LRD8(LIVE_OBJ[ch] + 0x34u)
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

/* the wire source this pass: what rq_prod_guarded() (drv_reqlaw.c) returns
 * to the F stage after this 10 ms task, replicated without its counters */
static uint16_t wire_cur(uint8_t ch, bool applied, const volatile gsb_t *b, bool *guard)
{
    static const uint16_t R03F[RQ_PROD_NCH] = DRV_RQ_R03F_TABLE;
    uint16_t v;
#if !DRV_RQ_USE_PRODUCER
    /* a host build whose law does not take the producer (tcu_sim): no
     * meaningful wire value — the comparison is the SH configuration's */
    (void)applied;
    (void)b;
    (void)guard;
    (void)v;
    return R03F[ch];
#else
    if (!rq_prod_started()) {
        return R03F[ch];
    }
    if (ch < 5u && applied) {
        v = b->app[ch];
        if (v >= DRV_RQ_FILL_MIN && v <= DRV_RQ_FILL_MAX) {
            return v;
        }
    }
    v = rq_prod_ch(ch);
    if (v == 500u || v == 8500u) {
        return v;
    }
    if (v != RQ_PROD_INVALID && drv_rq_fill_ok(ch, rq_prod_fill_state(ch), v)) {
        return v;
    }
    *guard = true;
    return R03F[ch];
#endif
}

/* not filling: off, applied, release wait, release ramp (a filling element's
 * current is its fill value — a state difference there is never a tail) */
static bool idle_or_rel(uint8_t st)
{
    return st == 0u || st == 3u || st == 4u || st == 5u;
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
    r->sph[0] = gs_shadow_phase(0u);
    r->sph[1] = gs_shadow_phase(2u);
    for (unsigned c = 0u; c < 5u; c++) {
        r->cand[c] = cand[c];
        r->wire[c] = wire[c];
    }
}

static void compare(void)
{
    const volatile gsb_t *b = gs_bridge_state();
    const bool applied = b->applied != 0u;
    const bool started = rq_prod_started();
    uint16_t cand[7], wire[7], shad[7];
    uint8_t dcur = 0u, cvs = 0u, filling = 0u;
    uint16_t dms = 0u;
    bool guard = false, tail_ok = true, ceqs, owned;
    uint8_t cls, src;

    /* the law cross-check: the law pass(es) since the previous comparator
     * pass took the previous prediction (seq2 'U' passes it ungated) */
    {
        const uint32_t ln = drv_out_wit()->law_n;
        if (C.prev_ok != 0u && ln != C.prev_law && SEQ2_0 == (uint8_t)'U') {
            C.law_n++;
            for (uint8_t ch = 0u; ch < 7u; ch++) {
                if (LAW_I(ch) != C.prev_app[ch]) {
                    C.law_x++;
                    break;
                }
            }
        }
        C.prev_law = ln;
    }

    for (uint8_t ch = 0u; ch < 7u; ch++) {
        cand[ch] = gs_cand_cur(ch);
        shad[ch] = gs_shadow_cur(ch);
        wire[ch] = wire_cur(ch, applied, b, &guard);
        C.prev_app[ch] = wire[ch];
        if (cand[ch] != wire[ch]) {
            dcur = (uint8_t)(dcur | (1u << ch));
        }
        if (cand[ch] != shad[ch]) {
            cvs = 1u;
        }
        if (ch < 5u) {
            const uint8_t cm = gs_cand_reqmode(ch), cs = gs_cand_fillst(ch);
            const uint8_t wm = applied ? gs_shadow_reqmode(ch) : LIVE_MODE(ch);
            const uint8_t ws = applied ? gs_shadow_fillst(ch) : LIVE_ST(ch);
            if (cm != wm) {
                dms = (uint16_t)(dms | (1u << ch));
            }
            if (cs != ws) {
                dms = (uint16_t)(dms | (1u << (8u + ch)));
                if (!idle_or_rel(cs) || !idle_or_rel(ws)) {
                    tail_ok = false;
                }
            }
            if (cm != gs_shadow_reqmode(ch) || cs != gs_shadow_fillst(ch)) {
                cvs = 1u;
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
    ceqs = cvs == 0u;
    owned = gs_shadow_pf() != 0u || gs_shadow_releasing() != 0u || gs_shadow_pending() != 0u ||
            (gs_shadow_wit()->now_flags & 0x08u) != 0u || gs_shadow_wit()->e28e != 0u;
    src = (uint8_t)((applied ? G6A_S_APPLIED : 0u) | (gs_shadow_releasing() ? G6A_S_SHREL : 0u) |
                    (owned ? G6A_S_OWNED : 0u) | (ceqs ? G6A_S_CEQS : 0u) |
                    (C.boot_open ? G6A_S_BOOTWIN : 0u) | (gs_cand_releasing() ? G6A_S_CREL : 0u) |
                    (started ? G6A_S_STARTED : 0u));

    if (dcur == 0u && dms == 0u) {
        cls = G6A_C_AGREE;
        if (C.boot_open != 0u && gs_cand_wit()->e28e == 0u && filling == 0u) {
            C.boot_open = 0u;                            /* the boot window ends before this pass */
            C.boot_close = (uint16_t)(C.passes > 0xFFFEu ? 0xFFFEu : C.passes);
        }
    } else if (C.boot_open != 0u && C.passes < G6A_BOOT_MAX) {
        cls = G6A_C_BOOT;
        C.boot_dis++;
    } else {
        if (C.boot_open != 0u) {
            C.boot_open = 0u;                            /* the window's cap */
            C.boot_close = (uint16_t)C.passes;
        }
        if (!applied && ceqs && owned) {
            cls = G6A_C_FENCED;
        } else if (!applied && ceqs && dcur == 0u && (dms & 0x00FFu) == 0u && tail_ok) {
            cls = G6A_C_FSMTAIL;
        } else {
            cls = G6A_C_UNEXP;
        }
    }
    if (applied && !ceqs) {
        C.cvs_n++;                                       /* the wire IS the shadow this pass */
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
            put_rec(&s_g6a.rec[C.rec_n], cls, dcur, dms, src, cand, wire);
            C.rec_n++;
        }
        if (cls == G6A_C_UNEXP) {
            if (C.first_unexp == 0xFFFFFFFFu) {
                C.first_unexp = C.passes;
            }
            C.last_unexp = C.passes;
            if (C.urec_n < G6A_UREC_N) {
                put_rec(&s_g6a.urec[C.urec_n], cls, dcur, dms, src, cand, wire);
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
    compare();
}

void g6a_tick_10ms(const gs_in_t *gi)
{
    uint32_t t0, t1, t2;
    if (gi->os_mode < 1u || gi->os_mode > 3u) {
        return;                                          /* the stock's lists: modes 1..3 */
    }
    t0 = plat_frt_us();
    gs_cand_tick_10ms(gi);
    t1 = plat_frt_us();
    compare();
    t2 = plat_frt_us();
    C.cand_us_last = us16(t1 - t0);
    if (C.cand_us_last > C.cand_us_max) {
        C.cand_us_max = C.cand_us_last;
    }
    if (us16(t2 - t1) > C.cmp_us_max) {
        C.cmp_us_max = us16(t2 - t1);
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

/* test_gs_bridge_logic.c — THE GARAGE BRIDGE's logic (app/gs_bridge.c;
 * ~/tcu_sweep/garage_g1_port.md, plan ~/tcu_sweep/garage_live_plan.md §2,
 * §5 items 3 / 4). The light harness of test_gs_shadow.c: main.c's 10 ms
 * path (CAN sims → vehicle → kl15 → the range chain → park_tick → the
 * garage shadow → the bridge) and the request law + compose
 * (drv_cyclic_build) every 10 ms; the staged cyclic words are the wire.
 *
 * Builds (CMakeLists "garage bridge"):
 *   test_gs_bridge_logic   GS_LIVE_DIR 0 (G1): the OS-mode-2 witness, the
 *       .noinit windows (cold / warm / broken), the guard negatives (9000 on
 *       a moving channel, E moving in D, 7000 in a mode-1 pass, a
 *       disagreement at arm entry) — each must trip ITS bit in the record,
 *       latch where the plan says, and the live staged words NEVER move.
 *   test_gs_bridge_live    GS_LIVE_DIR 3 (host only): the same negatives and
 *       every abort of plan §5 item 3 with the bridge APPLIED — lever P at
 *       passes 5 / 30 / 60 / 90, the engine frames stopping mid-fill, a
 *       valid 0x108 at 0 rpm, a park_mon latch, the release window latched,
 *       a diagnosis fault on B, a warm reset mid-engagement, the 30 s
 *       budget, the 16-commit cap — each: the P content on the wire within
 *       2 passes, the latch where §2 says, the windows carried over a reset.
 *   test_gs_bridge_mut     GSB_HOST_MUT_TABLE (E pinned at 500 in D): the
 *       engagement's first armed pass must trip G_FIXED.
 *   test_gs_bridge_engt    GS_LIVE_DIR 3, GSB_ENG_PASSES 50: an engagement
 *       longer than the limit is aborted and latched.
 */
#define _POSIX_C_SOURCE 200809L
#include "app/gs_shadow.h"
#include "app/gs_fill.h"
#include "app/gs_bridge.h"
#include "app/park.h"
#include "app/park_law.h"
#include "app/rq_prod.h"
#include "app/rng_chain.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_gsbr.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/sup/sup_cells.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "safety/kl15.h"
#include "safety/park_mon.h"
#include "tests/unit/stub_plat.h"
#if defined(GS_DUAL)
/* G6-0 (~/tcu_sweep/g6_0_port.md): instance 2 of the engine template (the
 * candidate) driven beside the shadow through every abort case */
#include "tests/unit/gs_dual.h"
#  define DUAL_INIT()     gs_dual_init()
#  define DUAL_TICK(g)    gs_dual_tick(g)
#else
#  define DUAL_INIT()     ((void)0)
#  define DUAL_TICK(g)    ((void)0)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sensors_in_t s_z;
const sensors_in_t *sensors_in(void) { return &s_z; }

static int s_fail;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); s_fail++; } } while (0)

/* ── the harness ───────────────────────────────────────────────────────── */
static uint8_t s_ctr[0x800];
static void send(can_bus_t bus, uint16_t id, uint8_t dlc, bool prot, const uint8_t *data)
{
    can_frame_t f = { .id = id, .dlc = dlc };
    memcpy(f.data, data, dlc);
    if (prot) {
        s_ctr[id] = (uint8_t)((s_ctr[id] + 1u) & 0x0Fu);
        f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(s_ctr[id] << 4));
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
    }
    vehicle_rx(bus, &f);
}

static struct {
    uint8_t  lever;         /* 1 P 2 R 3 N 4 D */
    bool     brake;
    bool     eng_send;
    uint16_t eng_rpm;
    uint8_t  mon;           /* injected park_mon fault byte */
    uint8_t  relwin;        /* injected release-window state */
    uint16_t tur;           /* the shadow's 0x89CBA + the bridge's turbine */
} C;
static unsigned T;          /* ticks (10 ms) since boot */

static void tick(void)
{
    uint8_t d[8];
    {
        const uint8_t l[5] = { C.lever, 0, 0, 0, 0 };
        send(CAN_BUS1, 0x2EAu, 5u, true, l);
        send(CAN_BUS2, 0x210u, 5u, true, l);
    }
    if (C.brake) {
        memset(d, 0, sizeof d);
        d[0] = 0x04u;
        send(CAN_BUS1, 0x140u, 8u, true, d);
    }
    if (C.eng_send) {
        memset(d, 0, sizeof d);
        d[0] = (uint8_t)(C.eng_rpm >> 8);
        d[1] = (uint8_t)C.eng_rpm;
        d[2] = 0x1Fu;
        d[3] = 0xFFu;
        send(CAN_BUS1, 0x108u, 8u, true, d);
    }
    rq_prod_task_10ms(1u);
    vehicle_update_10ms();
    *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL = 13206u;
    kl15_tick_10ms();
    rng_chain_tick_10ms(1u, 1u, 1u);
    park_set_os_mode(1u);
    park_tick_10ms();
    {
        const vehicle_in_t *v = vehicle_in();
        const gs_in_t gi = {
            .os_mode = 1u,
            .rngd = *(const volatile uint8_t *)0xFFF89CDCu,
            .park_model = park_law_state(),
            .park_req = park_law_request(),
            .temp = *(const volatile uint8_t *)0xFFF89C13u,
            .cd2 = *(const volatile uint8_t *)0xFFF89CD2u,
            .eng_speed = (uint16_t)v->engine_speed,
            .eng_valid = (v->q_engine_speed == Q_OK) ? 1u : 0u,
            .out_rpm = 0u,
            .tur_rpm = C.tur,
        };
        const gsb_in_t bi = {
            .mon_faults = (uint8_t)(park_mon_faults() | C.mon),
            .relwin_st = C.relwin,
            .eng_valid = gi.eng_valid,
            .tur_ok = 1u,
            .eng_speed = gi.eng_speed,
            .tur_rpm = C.tur,
            .cyc = drv_job_tx(DRV_CYC),
        };
        gs_shadow_tick_10ms(&gi);
        DUAL_TICK(&gi);
        gs_bridge_tick_10ms(&bi);
    }
    drv_rq_request_set(7u, park_slot7_release());
    drv_cyclic_build(0, 8u);
    T++;
}

static void boot(bool cold)
{
    memset(s_ctr, 0, sizeof s_ctr);
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    memset(&s_z, 0, sizeof s_z);
    s_z.q_output_speed = Q_OK;
    vehicle_init();
    park_init();
    rng_chain_init();
    kl15_init();
    *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL = 13206u;
    drv_jobs_init();
    *(volatile uint8_t *)0xFFF89C13u = 0x4Fu;
    *(volatile uint8_t *)0xFFF89CD2u = 0x4Fu;
    drv_rq_prod_host_select(true);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, 0u);
    }
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LETTERS[0] = (uint8_t)'c';
    CELL_OS_MODE = 1u;
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9020u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 12940u;
    CELL_SUPPLY_RAW = 13366u;
    rq_prod_boot_pass();
    gs_shadow_init();
    DUAL_INIT();
    gs_bridge_init(cold);
    gs_bridge_host_inject(0xFFu, 0u, 0u, 0u);
    memset(&C, 0, sizeof C);
    C.lever = 1u;
    C.brake = true;
    C.eng_send = true;
    C.eng_rpm = 700u;
    T = 0u;
}

static unsigned sp6(uint16_t w) { return 6u * (unsigned)(w & 0x7FFu); }
static unsigned absd(unsigned a, unsigned b) { return a > b ? a - b : b - a; }

/* the live staged A..E words carry the P content (A D B E C = 8500 8500 500 8500 8500) */
static uint8_t s_skip;      /* channels a case exempts (a live-law reaction of its own) */
static bool staged_P(void)
{
    static const uint16_t P[5] = { 8500u, 8500u, 500u, 8500u, 8500u };
    const uint16_t *cyc = drv_job_tx(DRV_CYC);
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        if (((s_skip >> ch) & 1u) == 0u && absd(sp6(cyc[(((ch & 1u) != 0u) ? 20u : 10u) + (ch >> 1)]), P[ch]) > 6u) {
            return false;
        }
    }
    return true;
}

/* warm up in P (0.6 s), lever `lev` and run to the shadow's nonzero commit;
 * returns the commit tick (the bridge's slot t0 = its bridge pass) */
static unsigned engage(uint8_t lev)
{
    const uint32_t c0 = gs_shadow_wit()->commits;
    for (unsigned k = 0u; k < 60u; k++) {
        tick();
    }
    C.lever = lev;
    for (unsigned k = 0u; k < 200u; k++) {
        tick();
        if (gs_shadow_wit()->commits != c0 && gs_shadow_wit()->commit_new != 0u) {
            return T - 1u;
        }
    }
    CHECK(0, "engage: no commit");
    return T;
}

static void run_to(unsigned t)
{
    while (T < t) {
        tick();
    }
}

static const volatile gsb_slot_t *slot(uint8_t k) { return &gs_bridge_state()->slot[k]; }

/* the P content within 2 passes of tick t0 (inclusive); returns the lag */
static unsigned p_within(unsigned t0, const char *name)
{
    unsigned lag = 0xFFu;
    while (T <= t0 + 2u) {
        tick();
        if (T - 1u >= t0 && staged_P()) {
            lag = T - 1u - t0;
            break;
        }
    }
    CHECK(lag <= 2u, "%s: the P content not on the wire within 2 passes (lag %u)", name, lag);
    return lag;
}

/* ── G1 (and both): the OS-mode-2 witness, unit ─────────────────────────── */
static void test_m2(void)
{
    gsb_in_t in = { .mon_faults = 0u, .relwin_st = 0u, .eng_valid = 0u, .tur_ok = 1u, .eng_speed = 0u,
                    .tur_rpm = 0u, .cyc = 0 };
    const volatile gsb_m2_t *m;
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    gs_shadow_init();
    DUAL_INIT();
    gs_bridge_init(true);
    m = &gs_bridge_state()->m2;
    CELL_OS_MODE = 1u;
    *(volatile uint8_t *)0xFFF8E9FFu = 2u;
    CELL_SUPPLY_RAW = 13366u;
    for (int k = 0; k < 5; k++) { gs_bridge_tick_10ms(&in); }
    CHECK(m->req == 1u && m->n2 == 0u, "m2: no engine → 1 (req %u)", m->req);
    in.eng_valid = 1u;
    in.eng_speed = 149u;
    gs_bridge_tick_10ms(&in);
    CHECK(m->req == 1u, "m2: 149 rpm → 1");
    in.eng_speed = 150u;
    gs_bridge_tick_10ms(&in);
    CHECK(m->req == 2u && m->first2 != 0u, "m2: 150 rpm valid → 2 (req %u)", m->req);
    in.eng_valid = 0u;
    for (int k = 0; k < 5; k++) { gs_bridge_tick_10ms(&in); }
    CHECK(m->req == 2u, "m2: mode 2 latches when the engine frames stop (req %u)", m->req);
    *(volatile uint8_t *)0xFFF8E9FFu = 4u;
    gs_bridge_tick_10ms(&in);
    CHECK(m->req == 1u, "m2: terminal off (E9FF 4) → 1 (req %u)", m->req);
    *(volatile uint8_t *)0xFFF8E9FFu = 2u;
    in.tur_rpm = 100u;
    in.tur_ok = 0u;
    gs_bridge_tick_10ms(&in);
    CHECK(m->req == 1u, "m2: turbine 100 with a faulted sensor → 1");
    in.tur_ok = 1u;
    CELL_SUPPLY_RAW = 17000u;
    gs_bridge_tick_10ms(&in);
    CHECK(m->req == 1u, "m2: supply 17000 (not < 17000) → 1");
    CELL_SUPPLY_RAW = 6000u;
    gs_bridge_tick_10ms(&in);
    CHECK(m->req == 1u, "m2: supply 6000 (not > 6000) → 1");
    CELL_SUPPLY_RAW = 6001u;
    gs_bridge_tick_10ms(&in);
    CHECK(m->req == 2u, "m2: turbine 100 ok, supply 6001 → 2 (req %u)", m->req);
    CELL_OS_MODE = 0u;
    {
        const uint32_t n = m->n;
        gs_bridge_tick_10ms(&in);
        CHECK(m->n == n, "m2: not run in OS mode 0");
    }
    printf("m2 witness: ok paths (no engine 1, 150 rpm 2, latch, E9FF 4 → 1, turbine, supply limits, mode 0)\n");
}

/* ── the windows (.noinit) ─────────────────────────────────────────────── */
static void test_windows(void)
{
    const volatile gsb_win_t *w = gs_bridge_win();
    gs_bridge_init(true);
    CHECK(w->acc_ms == 0u && w->commits == 0u && w->latch == 0u && w->magic == GSB_WIN_MAGIC,
          "cold: a fresh budget");
    /* carry: forge a used budget through a run? — directly: an engagement */
    boot(true);
    (void)engage(4u);
    run_to(T + 50u);
    {
        const uint32_t acc = w->acc_ms;
        const uint16_t cm = w->commits;
        CHECK(acc >= 500u && cm == 1u, "an engagement counts (acc %u commits %u)", (unsigned)acc, cm);
        gs_bridge_init(false);
        CHECK(w->acc_ms == acc && w->commits == cm && w->latch == 0u && w->warm == 1u,
              "warm: carried (acc %u commits %u latch %02X)", (unsigned)w->acc_ms, w->commits, w->latch);
        *(volatile uint32_t *)&w->acc_inv ^= 1u;        /* tear it */
        gs_bridge_init(false);
        CHECK(w->broken == 1u && w->acc_ms >= GSB_WIN_MS && (w->latch & GSB_L_WIN) != 0u,
              "warm + broken guard: spent and latched (acc %u latch %02X)", (unsigned)w->acc_ms, w->latch);
        gs_bridge_init(true);
        CHECK(w->acc_ms == 0u && w->latch == 0u && w->broken == 0u, "cold after broken: fresh");
    }
    printf("windows: cold fresh, warm carried, warm broken → spent, cold → fresh\n");
}

/* ── the guard negatives: each trips its bit in the record ─────────────── */
typedef struct { const char *name; uint8_t lev; uint8_t ch; uint16_t v; uint16_t at; uint16_t want; } neg_t;

static void test_guard_negative(const neg_t *n)
{
    unsigned c, at, hit = 0xFFFFu;
    boot(true);
    c = engage(n->lev);
    at = c + n->at;
    gs_bridge_host_inject(n->ch, n->v, at, at + 1u);
    run_to(at + 6u);
    {
        const uint8_t k = (n->lev == 2u) ? 1u : 0u;
        for (uint8_t i = 0u; i < slot(k)->n; i++) {
            const volatile gsb_rec_t *e = gs_bridge_rec(k, i);
            if ((e->why & n->want) != 0u) {
                hit = i;
                break;
            }
        }
        CHECK(hit == n->at, "%s: the guard bit %04X in the record at pass %u (found %u)", n->name, n->want,
              n->at, hit);
        CHECK(hit == 0xFFFFu || (gs_bridge_rec(k, (uint8_t)hit)->why & GSB_G_LATCH) != 0u,
              "%s: the trip carries G_LATCH", n->name);
        CHECK((gs_bridge_win()->latch & GSB_L_GUARD) != 0u && gs_bridge_state()->armed == 0u,
              "%s: latched off (latch %02X armed %u)", n->name, gs_bridge_win()->latch, gs_bridge_state()->armed);
        CHECK(gs_bridge_state()->guard_ch == n->ch && gs_bridge_state()->guard_v == n->v,
              "%s: the trip names ch %u value %u (got %u / %u)", n->name, n->ch, n->v,
              gs_bridge_state()->guard_ch, gs_bridge_state()->guard_v);
        CHECK(staged_P(), "%s: the wire carries the P content after the trip", n->name);
        /* a later engagement in the same power cycle never arms */
        C.lever = 1u;
        run_to(T + 80u);
        C.lever = n->lev;
        run_to(T + 200u);
        CHECK(gs_bridge_state()->armed == 0u && staged_P(), "%s: stays latched until power-off", n->name);
    }
    printf("guard negative %-28s: bit %04X at pass %u, latched, P content\n", n->name, n->want, hit);
}

static void test_agree_negative(void)
{
    unsigned c;
    boot(true);
    for (unsigned k = 0u; k < 60u; k++) {
        tick();
    }
    /* the shadow's C reads 8000 on the passes just before the lever's commit */
    gs_bridge_host_inject(4u, 8000u, T + 5u, T + 20u);
    C.lever = 4u;
    c = T;
    {
        const uint32_t c0 = gs_shadow_wit()->commits;
        while (T < c + 200u) {
            tick();
            if (gs_shadow_wit()->commits != c0) {
                break;
            }
        }
    }
    run_to(T + 5u);
    CHECK(slot(0)->n > 0u && (gs_bridge_rec(0, 0)->why & GSB_R_AGREE) != 0u &&
          (gs_bridge_rec(0, 0)->st & 1u) == 0u,
          "agree negative: no arm at the commit (why %04X)", gs_bridge_rec(0, 0)->why);
    CHECK(staged_P(), "agree negative: the wire carries the P content");
    CHECK((gs_bridge_win()->latch & GSB_L_GUARD) == 0u, "agree negative: not a guard latch");
    printf("agree negative: R_AGREE at the commit pass, not armed, no latch\n");
}

#if GS_LIVE_DIR != 0
/* ── G4b: THE BRIDGED RELEASE (2026-11-19, ~/tcu_sweep/g4b_release_bridge_port.md)
 * In this harness drv_cyclic_build runs after the bridge in the same tick,
 * so the staged words carry THIS pass's applied current (lag 0; the chain
 * test checks the real F-stage lag of 1). */
static uint16_t stw(uint8_t ch)
{
    const uint16_t *cyc = drv_job_tx(DRV_CYC);
    return cyc[(((ch & 1u) != 0u) ? 20u : 10u) + (ch >> 1)];
}

typedef struct {
    unsigned len;           /* release passes the shadow ran (BD16 = 4) */
    unsigned applied;       /* of them, applied by the bridge */
    unsigned follow_bad;    /* staged B / third != the shadow's current (applied passes) */
    unsigned fix_bad;       /* a pinned channel off 8500 (applied passes) */
    unsigned mono_bad;      /* B rose or the third fell between applied passes */
    unsigned p_at;          /* release pass at which the staged words first read the P content */
    unsigned shadow_end;    /* release pass at which the shadow stopped releasing */
    uint16_t b_first, b_last, x_first, x_last;
} relobs_t;

/* tick from the lever-P pass (the caller has set the lever) until the
 * shadow's release has ended and the staged words read the P content (or
 * `max` passes); pf 1 D (third C) / 2 R (third D). act(k) is called before
 * release pass k (NULL: none). */
static relobs_t follow_release(uint8_t pf, unsigned max, void (*act)(unsigned))
{
    relobs_t o;
    const uint8_t x = (pf == 2u) ? 1u : 4u;
    const uint8_t fixm = (pf == 2u) ? 0x19u : 0x0Bu;    /* R: A C E; D: A D E */
    uint16_t pb = 0xFFFFu, px = 0u;
    memset(&o, 0, sizeof o);
    o.p_at = 0xFFFFu;
    o.shadow_end = 0xFFFFu;
    for (unsigned k = 0u; k < max; k++) {
        if (act != 0) {
            act(k);
        }
        tick();
        if (k == 0u && C.lever == 1u) {
            /* the park outputs follow the range chain: off in the lever-P pass */
            CHECK(park_slot7_release() == 0u && park_slot11_hold() == 0u,
                  "release: the park outputs drop in the lever-P pass (release %u hold %u)",
                  park_slot7_release(), park_slot11_hold());
        }
        if (gs_shadow_releasing() != 0u) {
            o.len++;
        } else if (o.shadow_end == 0xFFFFu) {
            o.shadow_end = k;
        }
        if (gs_bridge_state()->applied != 0u && gs_shadow_releasing() != 0u) {
            const uint16_t b = (uint16_t)sp6(stw(2u)), xv = (uint16_t)sp6(stw(x));
            o.applied++;
            if (absd(b, gs_shadow_cur(2u)) > 6u || absd(xv, gs_shadow_cur(x)) > 6u) {
                o.follow_bad++;
            }
            for (uint8_t ch = 0u; ch < 5u; ch++) {
                if (((fixm >> ch) & 1u) != 0u && absd(sp6(stw(ch)), 8500u) > 6u) {
                    o.fix_bad++;
                }
            }
            if (pb != 0xFFFFu && (b > pb + 6u || xv + 6u < px)) {
                o.mono_bad++;
            }
            if (o.applied == 1u) {
                o.b_first = b;
                o.x_first = xv;
            }
            o.b_last = b;
            o.x_last = xv;
            pb = b;
            px = xv;
        }
        if (o.p_at == 0xFFFFu && staged_P()) {
            o.p_at = k;
        }
        if (o.shadow_end != 0xFFFFu && o.p_at != 0xFFFFu && k > o.shadow_end + 2u) {
            break;
        }
    }
    return o;
}

static const volatile gsbr_slot_t *rslot(uint8_t k) { return &gs_bridge_rel()->slot[k]; }

/* (1) a bridged release after lever P at engagement pass `at` */
static void rel_bridged(uint8_t lev, unsigned at)
{
    char nm[64];
    const uint8_t pf = (lev == 2u) ? 2u : 1u, k = (uint8_t)(pf - 1u);
    unsigned c;
    uint32_t acc0;
    relobs_t o;
    snprintf(nm, sizeof nm, "release %s, lever P at pass %u", pf == 1u ? "D" : "R", at);
    boot(true);
    c = engage(lev);
    run_to(c + at);
    CHECK(gs_bridge_state()->applied != 0u, "%s: applied before lever P", nm);
    acc0 = gs_bridge_win()->acc_ms;
    C.lever = 1u;
    o = follow_release(pf, 200u, 0);
    if (gs_shadow_rel()->slot[k].len == 0u) {
        /* lever P during B's fill: the stock's release completes in its
         * commit pass (cause 5) — nothing to bridge, the P content at once */
        CHECK(o.p_at == 0u && o.applied == 0u, "%s: release length 0 → the P content in the lever-P pass "
              "(p_at %u applied %u)", nm, o.p_at, o.applied);
        printf("release %-30s: shadow length 0 (cause %u) → P content in the lever-P pass\n", nm,
               gs_shadow_rel()->slot[k].cause);
        return;
    }
    CHECK(o.len == gs_shadow_rel()->slot[k].len, "%s: shadow release length %u (slot %u)", nm, o.len,
          gs_shadow_rel()->slot[k].len);
    CHECK(o.applied == o.len, "%s: every release pass bridged (%u of %u)", nm, o.applied, o.len);
    CHECK(o.follow_bad == 0u && o.fix_bad == 0u && o.mono_bad == 0u,
          "%s: staged follows the shadow (bad %u), pins hold (bad %u), envelope monotone (bad %u)", nm,
          o.follow_bad, o.fix_bad, o.mono_bad);
    CHECK(o.p_at == o.shadow_end, "%s: the P content exactly at the shadow's completion pass "
          "(P at release pass %u, shadow end %u)", nm, o.p_at, o.shadow_end);
    CHECK(gs_bridge_win()->latch == 0u && gs_bridge_rel()->trip_n == 0u, "%s: no latch / trip", nm);
    CHECK(rslot(k)->pf == pf && rslot(k)->len == o.len && rslot(k)->end_code == GSBR_E_DONE &&
          rslot(k)->end_t == o.len, "%s: recorder slot pf %u len %u end %u at %u", nm, rslot(k)->pf,
          rslot(k)->len, rslot(k)->end_code, rslot(k)->end_t);
    CHECK(gs_bridge_win()->acc_ms >= acc0 + 10u * o.len, "%s: release passes count against the budget "
          "(acc %u → %u)", nm, (unsigned)acc0, (unsigned)gs_bridge_win()->acc_ms);
    CHECK(gs_bridge_win()->commits == 1u, "%s: the release commit is not counted (commits %u)", nm,
          gs_bridge_win()->commits);
    printf("release %-30s: %u passes bridged, B %u → %u, %s %u → %u, P content at release pass %u "
           "(shadow end %u, cause %u), budget +%u ms\n", nm, o.applied, o.b_first, o.b_last,
           pf == 1u ? "C" : "D", o.x_first, o.x_last, o.p_at, o.shadow_end, gs_shadow_rel()->slot[k].cause,
           (unsigned)(gs_bridge_win()->acc_ms - acc0));
}

/* (2) aborts during a release: P content at once, never re-armed */
static unsigned s_rel_abort_at;
static void (*s_rel_abort)(void);
static void rel_act(unsigned k)
{
    if (k == s_rel_abort_at && s_rel_abort != 0) {
        s_rel_abort();
    }
}
static void rel_abort(const char *nm, void (*act)(void), int want_latch, uint16_t want_why)
{
    unsigned c, lag = 0xFFFFu;
    relobs_t o;
    boot(true);
    s_skip = 0u;
    c = engage(4u);
    run_to(c + 150u);
    C.lever = 1u;
    s_rel_abort_at = 30u;
    s_rel_abort = act;
    /* release passes 0..29 bridged, then the abort before pass 30 */
    o = follow_release(1u, 30u, 0);
    CHECK(o.applied == 30u, "%s: 30 release passes bridged before the abort (%u)", nm, o.applied);
    act();
    tick();
    if (gs_bridge_state()->applied == 0u && staged_P()) {
        lag = 0u;
    }
    CHECK(lag == 0u, "%s: the P content in the abort pass (applied %u)", nm, gs_bridge_state()->applied);
    CHECK(rslot(0)->end_code != GSBR_E_DONE && rslot(0)->end_t == 30u,
          "%s: the release left at pass 30 (end %02X at %u)", nm, rslot(0)->end_code, rslot(0)->end_t);
    CHECK((rslot(0)->end_why & want_why) != 0u, "%s: end reason %04X (want %04X)", nm, rslot(0)->end_why,
          want_why);
    CHECK(want_latch < 0 || gs_bridge_win()->latch == (uint8_t)want_latch, "%s: latch %02X", nm,
          gs_bridge_win()->latch);
    /* never re-armed for the rest of the release, even when the cause clears */
    C.mon = 0u;
    C.relwin = 0u;
    *(volatile uint16_t *)(uintptr_t)(0xFFF93004u + 2u * 2u) = 0u;
    {
        unsigned re = 0u, notp = 0u;
        for (unsigned k = 31u; k < 140u; k++) {
            tick();
            re += gs_bridge_state()->applied;
            if (!staged_P()) {
                notp++;
            }
        }
        CHECK(re == 0u && notp == 0u, "%s: not re-armed (applied %u), P content held (%u passes not P)", nm,
              re, notp);
    }
    printf("release abort %-30s: P content in the abort pass, end %02X why %04X, latch %02X, not re-armed\n",
           nm, rslot(0)->end_code, rslot(0)->end_why, gs_bridge_win()->latch);
    s_skip = 0u;
}
static void ract_mon(void) { C.mon = 0x04u; }
static void ract_relwin(void) { C.relwin = 2u; }
static void ract_diag_b(void) { *(volatile uint16_t *)(uintptr_t)(0xFFF93004u + 2u * 2u) = 0x0010u; s_skip = 0x04u; }

static void rel_warm_reset(void)
{
    unsigned c, notp = 0u;
    relobs_t o;
    boot(true);
    c = engage(4u);
    run_to(c + 150u);
    C.lever = 1u;
    o = follow_release(1u, 30u, 0);
    CHECK(o.applied == 30u, "release warm reset: bridged before (%u)", o.applied);
    gs_shadow_init();
    DUAL_INIT();
    gs_bridge_init(false);
    tick();
    CHECK(gs_bridge_state()->applied == 0u && staged_P(), "release warm reset: the P content at once");
    for (unsigned k = 0u; k < 150u; k++) {
        tick();
        if (!staged_P() || gs_bridge_state()->applied != 0u) {
            notp++;
        }
    }
    CHECK(notp == 0u, "release warm reset: P content held (%u)", notp);
    printf("release abort warm reset              : P content at once, nothing re-armed\n");
}

/* (3) the envelope negatives: each trips, latches, the P content at once */
typedef struct { const char *name; uint8_t lev; uint8_t ch; uint16_t v; uint8_t code; uint16_t gbit; } rneg_t;
static void rel_envelope_negative(const rneg_t *n)
{
    unsigned c, at;
    const uint8_t k = (n->lev == 2u) ? 1u : 0u;
    boot(true);
    c = engage(n->lev);
    run_to(c + 150u);
    C.lever = 1u;
    at = T + 20u;                       /* release pass 20 */
    gs_bridge_host_inject(n->ch, n->v, at, at + 1u);
    run_to(at + 1u);
    CHECK(gs_bridge_rel()->trip_n == 1u && gs_bridge_rel()->trip_code == n->code &&
          gs_bridge_rel()->trip_ch == n->ch && gs_bridge_rel()->trip_v == n->v,
          "%s: trip %u code %u ch %u v %u (want code %u ch %u v %u)", n->name, gs_bridge_rel()->trip_n,
          gs_bridge_rel()->trip_code, gs_bridge_rel()->trip_ch, gs_bridge_rel()->trip_v, n->code, n->ch, n->v);
    CHECK((gs_bridge_win()->latch & GSB_L_GUARD) != 0u && gs_bridge_state()->applied == 0u && staged_P(),
          "%s: latched, the P content in the trip pass", n->name);
    CHECK(rslot(k)->end_code == (uint8_t)(0x10u + n->code) && rslot(k)->end_t == 20u &&
          (rslot(k)->end_why & n->gbit) != 0u, "%s: slot end %02X at %u why %04X", n->name, rslot(k)->end_code,
          rslot(k)->end_t, rslot(k)->end_why);
    /* latched until power-off: the next engagement never arms */
    run_to(T + 150u);
    C.lever = n->lev;
    run_to(T + 200u);
    CHECK(gs_bridge_state()->armed == 0u && staged_P(), "%s: stays latched", n->name);
    printf("release envelope negative %-26s: code %u at release pass 20, latched, P content\n", n->name,
           n->code);
}

/* (4) a lever change back to D / R during the release: the stock ends its
 * release (cause 4, the new request) and re-commits once its gates allow
 * (the park model back at "released", 0xDC37E); the bridge's own arm rules
 * apply — the P content in between, a re-arm only after GSB_AGREE_N
 * agreeing passes at the new commit */
static void rel_lever_back(uint8_t back)
{
    unsigned c, k_end = 0xFFFFu, k_commit = 0xFFFFu, k_arm = 0xFFFFu, notp_gap = 0u, trips;
    uint32_t c0;
    relobs_t o;
    const uint8_t k2 = (back == 2u) ? 1u : 0u;
    boot(true);
    c = engage(4u);
    run_to(c + 150u);
    C.lever = 1u;
    o = follow_release(1u, 40u, 0);
    CHECK(o.applied == 40u, "lever back: 40 release passes bridged (%u)", o.applied);
    C.lever = back;
    trips = gs_bridge_rel()->trip_n;
    c0 = gs_shadow_wit()->commits;
    for (unsigned k = 0u; k < 200u; k++) {
        tick();
        if (k_end == 0xFFFFu && gs_shadow_releasing() == 0u) {
            k_end = k;
        }
        if (k_commit == 0xFFFFu && gs_shadow_wit()->commits != c0 && gs_shadow_wit()->commit_new != 0u) {
            k_commit = k;
        }
        if (k_arm == 0xFFFFu && gs_bridge_state()->applied != 0u && gs_shadow_releasing() == 0u) {
            k_arm = k;
        }
        if (k_end != 0xFFFFu && k_arm == 0xFFFFu && !staged_P()) {
            notp_gap++;
        }
    }
    CHECK(k_end == 0u, "lever back: the shadow ends its release in the lever pass (k %u)", k_end);
    CHECK(rslot(0)->end_code == GSBR_E_DONE && rslot(0)->end_t == 40u, "lever back: release ended by the "
          "shadow at pass 40 (end %02X at %u)", rslot(0)->end_code, rslot(0)->end_t);
    CHECK(k_commit != 0xFFFFu && k_commit >= GSB_AGREE_N && k_arm == k_commit && notp_gap == 0u,
          "lever back to %s: the P content until the re-commit (%u not P), re-armed at the commit "
          "(commit %u arm %u)", back == 4u ? "D" : "R", notp_gap, k_commit, k_arm);
    CHECK(gs_bridge_rel()->trip_n == trips && gs_bridge_win()->latch == 0u && slot(k2)->arm_on == 0u,
          "lever back: no trip, the re-engagement armed at its commit (arm_on %u)", slot(k2)->arm_on);
    printf("release, lever back to %s at release pass 40: shadow release ends in that pass (cause %u), "
           "the P content for %u passes, re-commit at +%u, bridged from its commit pass; no trip\n",
           back == 4u ? "D" : "R", gs_shadow_rel()->slot[0].cause, k_commit, k_commit);
}

/* (5) no fresh arm into a release: the engagement was disarmed (park_mon,
 * cleared again), the release must not be bridged */
static void rel_no_fresh_arm(void)
{
    unsigned c;
    relobs_t o;
    boot(true);
    c = engage(4u);
    run_to(c + 30u);
    C.mon = 0x04u;
    run_to(c + 40u);
    C.mon = 0u;
    run_to(c + 150u);
    CHECK(gs_bridge_state()->applied == 0u, "fresh arm: the engagement stays disarmed after park_mon");
    C.lever = 1u;
    o = follow_release(1u, 200u, 0);
    CHECK(o.len >= 100u && o.applied == 0u && gs_bridge_rel()->refused >= 1u,
          "fresh arm: the release is not bridged (len %u applied %u refused %u)", o.len, o.applied,
          gs_bridge_rel()->refused);
    printf("release after a disarmed engagement: %u shadow release passes, %u bridged, refused %u\n", o.len,
           o.applied, gs_bridge_rel()->refused);
}

/* (6) the release length limit (host override GSB_REL_PASSES) */
static void rel_length_limit(void)
{
    unsigned c;
    boot(true);
    c = engage(4u);
    run_to(c + 150u);
    C.lever = 1u;
    run_to(T + GSB_REL_PASSES + 2u);
    CHECK(gs_bridge_rel()->trip_code == GSBR_T_LEN && (gs_bridge_win()->latch & GSB_L_GUARD) != 0u &&
          rslot(0)->end_t == GSB_REL_PASSES && staged_P(),
          "release length limit %u: tripped (code %u end_t %u latch %02X)", (unsigned)GSB_REL_PASSES,
          gs_bridge_rel()->trip_code, rslot(0)->end_t, gs_bridge_win()->latch);
    printf("release length limit %u passes: tripped at release pass %u, latched, P content\n",
           (unsigned)GSB_REL_PASSES, rslot(0)->end_t);
}

/* (7) the engine stops (valid 0x108 at 0 rpm) while engaged in D: the stock
 * commits power flow 0 and releases; the bridge carries that release */
static void rel_engine_stop(void)
{
    unsigned c;
    relobs_t o;
    boot(true);
    c = engage(4u);
    run_to(c + 150u);
    C.eng_rpm = 0u;
    o = follow_release(1u, 200u, 0);
    printf("release on engine stop (0x108 valid at 0 rpm, lever D): shadow %u release passes (cause %u), "
           "bridged %u, P content at release pass %u, shadow end %u, follow bad %u\n", o.len,
           gs_shadow_rel()->slot[0].cause, o.applied, o.p_at, o.shadow_end, o.follow_bad);
    CHECK(o.applied == o.len && o.follow_bad == 0u && o.fix_bad == 0u && o.mono_bad == 0u &&
          o.p_at == o.shadow_end && gs_bridge_win()->latch == 0u,
          "engine stop: the stock's release bridged, P content at its completion");
    /* the lever is still in D, the engine is stopped: nothing re-arms */
    run_to(T + 100u);
    CHECK(gs_bridge_state()->applied == 0u && staged_P(), "engine stop: P content held after the release");
}

/* ── the aborts (applied) ──────────────────────────────────────────────── */
static void abort_lever(unsigned at)
{
    char nm[48];
    unsigned c, lag;
    snprintf(nm, sizeof nm, "lever P at pass %u", at);
    boot(true);
    c = engage(4u);
    run_to(c + at);
    CHECK(gs_bridge_state()->applied != 0u, "%s: applied before the abort", nm);
    C.lever = 1u;
    lag = p_within(T, nm);
    CHECK(gs_bridge_win()->latch == 0u, "%s: no latch (%02X)", nm, gs_bridge_win()->latch);
    CHECK(slot(0)->arm_off != GSB_NONE && (slot(0)->off_why & GSB_R_RANGE) != 0u, "%s: disarmed by the range", nm);
    printf("abort %-24s: P content in %u pass(es), disarm why %04X, no latch\n", nm, lag, slot(0)->off_why);
}

static void abort_case(const char *nm, unsigned at, void (*act)(void), int want_latch, uint16_t want_why)
{
    unsigned c, t0, lag = 0xFFu;
    boot(true);
    s_skip = 0u;
    c = engage(4u);
    run_to(c + at);
    CHECK(gs_bridge_state()->applied != 0u, "%s: applied before the abort", nm);
    act();
    t0 = T;
    /* the disarm, then the P content within 2 passes */
    while (T < t0 + 60u && gs_bridge_state()->armed != 0u) {
        tick();
    }
    CHECK(gs_bridge_state()->armed == 0u, "%s: never disarmed", nm);
    {
        const unsigned td = T - 1u;
        unsigned k = 0u;
        while (!staged_P() && k < 3u) {
            tick();
            k++;
        }
        lag = k;
        CHECK(staged_P() && lag <= 2u, "%s: the P content within 2 passes of the disarm (lag %u)", nm, lag);
        CHECK((gs_bridge_state()->why & want_why) != 0u || (slot(0)->off_why & want_why) != 0u,
              "%s: reason %04X (now %04X, off %04X)", nm, want_why, gs_bridge_state()->why, slot(0)->off_why);
        CHECK(want_latch < 0 || gs_bridge_win()->latch == (uint8_t)want_latch,
              "%s: latch %02X (want %d)", nm, gs_bridge_win()->latch, want_latch);
        printf("abort %-34s: disarm %u passes after, P content +%u, why %04X, latch %02X\n", nm, td - t0 + 1u,
               lag, slot(0)->off_why, gs_bridge_win()->latch);
    }
    s_skip = 0u;
}

/* the engine frames stop mid-fill: the shadow (0xD4DC2, ported) HOLDS engine
 * state 3 on an invalid 0x108 (e[0] = 380 < n || q != 0) — the engagement
 * runs on to its normal completion and the bridge stays armed; the abort
 * is lever P (or the windows). CORRECTION to plan §2.5. */
static void engine_stop_continues(void)
{
    unsigned c, lag;
    boot(true);
    c = engage(4u);
    run_to(c + 5u);
    C.eng_send = false;
    run_to(c + 120u);
    CHECK(gs_bridge_state()->armed != 0u && slot(0)->done_at != GSB_NONE && slot(0)->done_cause == 1u &&
          gs_shadow_wit()->e6a8 == 3u,
          "engine frames stop: the engagement completes armed (armed %u done %u cause %u e6a8 %u)",
          gs_bridge_state()->armed, slot(0)->done_at, slot(0)->done_cause, gs_shadow_wit()->e6a8);
    C.lever = 1u;
    {
        /* G4b: lever P now starts the bridged release (the P content at its completion) */
        const relobs_t o = follow_release(1u, 200u, 0);
        lag = o.p_at;
        CHECK(o.applied == o.len && o.len > 0u && o.p_at == o.shadow_end,
              "engine frames stop, then lever P: the release bridged (len %u applied %u P at %u end %u)",
              o.len, o.applied, o.p_at, o.shadow_end);
    }
    printf("engine frames stop mid-fill: engine state held 3 (0xD4DC2), completion at pass %u armed; "
           "lever P → the bridged release, P content at release pass %u\n", slot(0)->done_at, lag);
}
static void act_mon(void) { C.mon = 0x04u; }
static void act_relwin(void) { C.relwin = 2u; }
/* the fault word on B: the LIVE law reacts on B itself (0x7BA76), so B is
 * exempt from the P-content check; A D E C must read the P content */
static void act_diag_b(void) { *(volatile uint16_t *)(uintptr_t)(0xFFF93004u + 2u * 2u) = 0x0010u; s_skip = 0x04u; }

static void abort_warm_reset(void)
{
    unsigned c;
    uint32_t acc;
    boot(true);
    c = engage(4u);
    run_to(c + 30u);
    acc = gs_bridge_win()->acc_ms;
    /* a warm reset mid-engagement: the bridge and the shadow restart (the
     * live modules keep running here — the wire's view of the reset is the
     * bridge's disarm) */
    gs_shadow_init();
    DUAL_INIT();
    gs_bridge_init(false);
    CHECK(gs_bridge_win()->acc_ms == acc && gs_bridge_win()->commits == 1u && gs_bridge_win()->warm == 1u,
          "warm reset: the windows carried (acc %u/%u commits %u)", (unsigned)gs_bridge_win()->acc_ms,
          (unsigned)acc, gs_bridge_win()->commits);
    (void)p_within(T, "warm reset");
    /* the lever still says D: the shadow re-engages, the bridge re-arms only
     * after 10 agreeing passes */
    run_to(T + 200u);
    CHECK(gs_bridge_win()->commits == 2u && gs_bridge_state()->arm_ev >= 1u && slot(0)->arm_on == 0u,
          "warm reset: re-engaged and re-armed (commits %u arm_ev %u arm_on %u)", gs_bridge_win()->commits,
          gs_bridge_state()->arm_ev, slot(0)->arm_on);
    CHECK(gs_bridge_win()->acc_ms > acc, "warm reset: the budget keeps counting");
    printf("abort warm reset mid-engagement: windows carried (acc %u ms, commits 2), re-armed after agreement\n",
           (unsigned)gs_bridge_win()->acc_ms);
}

static void abort_budget(void)
{
    unsigned c;
    boot(true);
    c = engage(4u);
    run_to(c + 2990u);
    CHECK(gs_bridge_state()->armed != 0u, "budget: still armed at 29.9 s");
    run_to(c + 3010u);
    CHECK(gs_bridge_state()->armed == 0u && (gs_bridge_win()->latch & GSB_L_WIN) != 0u &&
          gs_bridge_win()->acc_ms == GSB_WIN_MS && staged_P(),
          "budget: latched off at 30 s (acc %u latch %02X armed %u)", (unsigned)gs_bridge_win()->acc_ms,
          gs_bridge_win()->latch, gs_bridge_state()->armed);
    printf("abort 30 s budget: latched at %u ms, P content with the lever still in D\n",
           (unsigned)gs_bridge_win()->acc_ms);
}

static void abort_commits(void)
{
    boot(true);
    for (unsigned n = 1u; n <= 17u; n++) {
        const unsigned c = engage(4u);
        run_to(c + 40u);
        if (n <= 16u) {
            CHECK(gs_bridge_state()->armed != 0u, "commits: engagement %u armed", n);
        } else {
            CHECK(gs_bridge_state()->armed == 0u && (gs_bridge_win()->latch & GSB_L_COMMITS) != 0u &&
                  staged_P(), "commits: the 17th is refused (latch %02X)", gs_bridge_win()->latch);
        }
        C.lever = 1u;
    }
    printf("abort commit cap: 16 armed, the 17th latched (commits %u, acc %u ms)\n", gs_bridge_win()->commits,
           (unsigned)gs_bridge_win()->acc_ms);
}

static void abort_engt(void)
{
    unsigned c;
    boot(true);
    c = engage(4u);
    run_to(c + GSB_ENG_PASSES + 5u);
    CHECK(gs_bridge_state()->armed == 0u && (gs_bridge_win()->latch & GSB_L_ENGT) != 0u && staged_P(),
          "engagement limit %u passes: aborted and latched (latch %02X)", (unsigned)GSB_ENG_PASSES,
          gs_bridge_win()->latch);
    printf("abort engagement limit (%u passes): latched, P content\n", (unsigned)GSB_ENG_PASSES);
}

static void timeout_inside_limit(void)
{
    unsigned c;
    boot(true);
    C.tur = 700u;                       /* never synchronises: the stock's 250-pass timeout */
    c = engage(4u);
    run_to(c + 345u);
    CHECK(slot(0)->done_at != GSB_NONE && slot(0)->done_cause == 2u && gs_bridge_win()->latch == 0u,
          "the stock timeout completes inside 3.5 s (done_at %u cause %u latch %02X)", slot(0)->done_at,
          slot(0)->done_cause, gs_bridge_win()->latch);
    printf("timeout path: completion cause 2 at pass %u (< %u), no latch\n", slot(0)->done_at,
           (unsigned)GSB_ENG_PASSES);
}
#endif

int main(void)
{
#if defined(GSB_HOST_MUT_TABLE)
    {
        /* the table mutation: E pinned at 500 in D — the first armed-eligible
         * pass (the commit) must trip G_FIXED on E */
        const neg_t n = { "table mutation (E 500 in D)", 4u, 0xFFu, 0u, 0u, GSB_G_FIXED };
        unsigned c;
        boot(true);
        c = engage(n.lev);
        run_to(c + 5u);
        CHECK((gs_bridge_rec(0, 0)->why & GSB_G_FIXED) != 0u && gs_bridge_state()->guard_ch == 3u &&
              gs_bridge_state()->guard_v == 8500u && (gs_bridge_win()->latch & GSB_L_GUARD) != 0u && staged_P(),
              "mutation: G_FIXED on E at the commit (why %04X ch %u v %u)", gs_bridge_rec(0, 0)->why,
              gs_bridge_state()->guard_ch, gs_bridge_state()->guard_v);
        printf("table mutation: G_FIXED on E (ch 3, 8500) at the commit pass, latched\n");
    }
#elif defined(TEST_ENGT)
    abort_engt();
#elif defined(TEST_REL_LEN)
    rel_length_limit();
#elif defined(TEST_REL_ONLY)
    /* the release negatives' builds: GSB_HOST_NO_REL_BRIDGE (the r04k
     * behaviour) and GSB_HOST_NEG_REL_FRESH must FAIL here */
    rel_bridged(4u, 150u);
    rel_bridged(2u, 150u);
    rel_no_fresh_arm();
#else
    test_m2();
    test_windows();
    {
        static const neg_t N[] = {
            { "C 9000 (moving, D)",          4u, 4u, 9000u, 20u, GSB_G_RANGE },
            { "E 500 (fixed, D)",            4u, 3u, 500u,  20u, GSB_G_FIXED },
            { "B 7000 in a mode-1 pass (D)", 4u, 2u, 7000u, 5u,  GSB_G_RANGE },
            { "D 400 (moving, R)",           2u, 1u, 400u,  20u, GSB_G_RANGE },
            { "C 6000 (fixed, R)",           2u, 4u, 6000u, 20u, GSB_G_FIXED },
        };
        for (unsigned k = 0u; k < sizeof N / sizeof N[0]; k++) {
            test_guard_negative(&N[k]);
        }
    }
    test_agree_negative();
#if GS_LIVE_DIR != 0
    /* lever P during the fill: the stock's release ends in its commit pass,
     * the P content at once (G2's abort). Later (G4b): the bridged release */
    abort_lever(5u);
    rel_bridged(4u, 5u);
    rel_bridged(4u, 20u);
    rel_bridged(4u, 30u);
    rel_bridged(4u, 60u);
    rel_bridged(4u, 90u);
    rel_bridged(4u, 150u);
    rel_bridged(2u, 150u);
    rel_bridged(2u, 40u);
    engine_stop_continues();
    rel_engine_stop();
    rel_abort("park_mon latch", ract_mon, 0, GSB_R_MON);
    rel_abort("release window latched", ract_relwin, 0, GSB_R_RELWIN);
    rel_abort("diagnosis fault word on B", ract_diag_b, 0, GSB_R_DIAG);
    rel_warm_reset();
    {
        static const rneg_t RN[] = {
            { "B above its start (D)",     4u, 2u, 4200u, GSBR_T_BUP, GSB_G_RANGE },
            { "C below its start (D)",     4u, 4u, 6000u, GSBR_T_XDN, GSB_G_RANGE },
            { "E off its pin (D)",         4u, 3u, 8000u, GSBR_T_FIXED, GSB_G_FIXED },
            { "C (pinned) moves (R)",      2u, 4u, 8400u, GSBR_T_FIXED, GSB_G_FIXED },
            { "D below its start (R)",     2u, 1u, 6000u, GSBR_T_XDN, GSB_G_RANGE },
            { "B 9000 (R)",                2u, 2u, 9000u, GSBR_T_RANGE, GSB_G_RANGE },
        };
        for (unsigned k = 0u; k < sizeof RN / sizeof RN[0]; k++) {
            rel_envelope_negative(&RN[k]);
        }
    }
    rel_lever_back(4u);
    rel_lever_back(2u);
    rel_no_fresh_arm();
    abort_case("park_mon latch", 30u, act_mon, 0u, GSB_R_MON);
    abort_case("release window latched mid-D", 30u, act_relwin, 0u, GSB_R_RELWIN);
    abort_case("diagnosis fault word on B", 30u, act_diag_b, 0u, GSB_R_DIAG);
    abort_warm_reset();
    timeout_inside_limit();
    abort_budget();
    abort_commits();
#endif
#endif
#if defined(GS_DUAL)
    gs_dual_report("test_gs_bridge_logic");
    CHECK(gs_dual_passes() > 1000u, "gs_dual: only %u passes compared", gs_dual_passes());
    CHECK(gs_dual_bad() == 0u, "gs_dual: %u passes where the candidate differs from the shadow",
          gs_dual_bad());
#endif
    if (s_fail == 0) {
        printf("test_gs_bridge_logic: ok\n");
    }
    return s_fail != 0;
}

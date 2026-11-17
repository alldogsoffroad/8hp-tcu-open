/* Unit test: the READ-ONLY range shadow (app/rng_shadow.c; 2026-11-17,
 * ~/tcu_sweep/range_shadow_port.md).
 *
 * 1. Read-only: the lever schedule P → R → N → D → P → none, run through the
 *    live path (vehicle → park_tick: app/range.c, app/park_law.c, the park
 *    demands, park_mon) with and without the shadow — every live output, the
 *    park status, and the whole stock cell window 0xFFF80000..0xFFF96000
 *    (hashed per tick) bit-identical.
 * 2. The shadow on the dump stand-ins, from the bytes:
 *      E531 follows the lever; D5FB 1 → 0 on the first valid P frame;
 *      E53C / 0x8941A are one-pass pulses on a lever change; the 0x89434
 *      filter (sub-object +0, table 0xFFF8E64C) maps a R/N/D request at
 *      D6D6 = 8 to 0, so 0x894B6 / D6CE never carry 7/6/5; 893E8 = 0, so
 *      0xC7A56 never copies D6CE into E545; E545 = D6DE = D6D6 = 8, D6D9 = 0
 *      on every pass; 304C+0 = 8; no unported path reached but the counted
 *      ones (0xC5F14 every pass, the coding-mask enables +0x70).
 *    And started in R before any P: D5FB stays 1, E53C stays 0, D6D6 8.
 * 3. A positive control (host hooks, not the bench): with the brake-like
 *    input D607 (D64B = 1, D631 = 0) disabling the filter and a speed word
 *    D63E = 500 taking 893E8 to 1, lever R reaches D6DE = 7 and then
 *    0xC767C answers 8 / 0x33 (D666 = 1, the dump's); with D666 = D664 = 0
 *    too, D6D6 = 7 and holds — the chain does carry 7 when its interlocks
 *    open.
 * 4-8 (2026-11-17, the live inputs, ~/tcu_sweep/range_shadow_live_inputs.md):
 *    at rest (no frame, P0850 failed) the reaction bytes become the dump's
 *    once U1267 fails at pass 50; no pass in OS modes 0 / 4; decode §9 —
 *    lever from power-on + 0x140 brake ON + 0x108 700 rpm: 893E8 1, D607 1,
 *    D664 = D666 = 0, P 8, R 7, N 6, D 5, P 8 (reason 0); no brake: 8
 *    throughout; lever late: U1267 → R 8/0x33, N and D 8/0x3C. The read-only
 *    comparison (1) also runs with the brake / engine sims and a late lever.
 */
#include "app/rng_shadow.h"
#include "app/park.h"
#include "app/park_cells.h"
#include "app/range.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_cells.h"
#include "io/vehicle/vehicle.h"
#include "io/sensors/sensors.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

static sensors_in_t s_z;
const sensors_in_t *sensors_in(void) { return &s_z; }

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

/* 0x2EA lever codes: 1 P, 2 R, 3 N, 4 D (0 = no frame) */
static void lever_frame(uint8_t code)
{
    uint8_t l[5] = { code, 0, 0, 0, 0 };

    if (code != 0u) {
        send(CAN_BUS1, 0x2EAu, 5u, true, l);
        send(CAN_BUS2, 0x210u, 5u, false, l);
    }
}

#define SEG    30u
#define N_SEG  6u
#define N_T    (SEG * N_SEG)
static const uint8_t k_sched[N_SEG] = { 1u, 2u, 3u, 4u, 1u, 0u };   /* P R N D P none */
static const uint8_t k_code[5] = { 0u, 8u, 7u, 6u, 5u };

typedef struct {
    uint16_t rel, hold;
    uint8_t  tgt, tgt_c, arb[3], ae28, ae29, obj[4], out[5], out_c[5], rng, rngd;
    uint8_t  target, reason, state, request, lock, mon, lever;
    uint64_t window;                    /* FNV-1a over 0xFFF80000..0xFFF96000 */
} live_t;

static uint64_t window_hash(void)
{
    const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)0xFFF80000u;
    uint64_t h = 0xCBF29CE484222325ull;

    for (uint32_t i = 0u; i < 0x16000u; i++) {
        h = (h ^ p[i]) * 0x100000001B3ull;
    }
    return h;
}

static void live_snap(live_t *p)
{
    const park_status_t *st = park_status();

    memset(p, 0, sizeof *p);
    p->rel = park_slot7_release();
    p->hold = park_slot11_hold();
    p->tgt = PCELL_TGT;
    p->tgt_c = PCELL_TGT_C;
    for (int i = 0; i < 3; i++) p->arb[i] = PCELL_ARB[i];
    p->ae28 = PCELL_AE28;
    p->ae29 = PCELL_AE29;
    for (int i = 0; i < 4; i++) p->obj[i] = PCELL_OBJ[i];
    for (int i = 0; i < 5; i++) { p->out[i] = PCELL_OUT[i]; p->out_c[i] = PCELL_OUT_C[i]; }
    p->rng = PCELL_RNG;
    p->rngd = PCELL_RNGD;
    p->target = (uint8_t)range_target();
    p->reason = range_reason();
    p->state = st->state;
    p->request = st->request;
    p->lock = st->lock;
    p->mon = st->mon_faults;
    p->lever = st->lever;
    p->window = window_hash();
}

typedef struct {
    uint8_t d672, d673, d5fb, e531, e53c, a, b6, ce, e545, de, df, d6, d9, s8;
    uint32_t c304;
    uint8_t d607, d631, d64b, d63c, d660, d661, d664, d665, d666, g109;
    uint16_t d63a, d60c;
} shad_t;

static void shad_snap(shad_t *s)
{
    const rng_shadow_cells_t *c = rng_shadow_cells();

    s->d672 = c->d61a[0xD672u - 0xD61Au];
    s->d673 = c->d61a[0xD673u - 0xD61Au];
    s->d5fb = c->d5e6[0xD5FBu - 0xD5E6u];
    s->e531 = c->e530[0xE531u - 0xE530u];
    s->e53c = c->e530[0xE53Cu - 0xE530u];
    s->a = c->x89418[0x8941Au - 0x89418u];
    s->b6 = c->x894b6;
    s->ce = c->d6ca[0xD6CEu - 0xD6CAu];
    s->e545 = c->e530[0xE545u - 0xE530u];
    s->de = c->d6de[0];
    s->df = c->d6de[1];
    s->d6 = c->d6d4[0xD6D6u - 0xD6D4u];
    s->d9 = c->d6d4[0xD6D9u - 0xD6D4u];
    s->s8 = c->x893e0[0x893E8u - 0x893E0u];
    s->c304 = c->o304c[0];
    s->d607 = c->d5e6[0xD607u - 0xD5E6u];
    s->d631 = c->d61a[0xD631u - 0xD61Au];
    s->d64b = c->d61a[0xD64Bu - 0xD61Au];
    s->d63c = c->d61a[0xD63Cu - 0xD61Au];
    s->d63a = (uint16_t)(c->d61a[0xD63Au - 0xD61Au] << 8 | c->d61a[0xD63Bu - 0xD61Au]);
    s->d660 = c->d61a[0xD660u - 0xD61Au];
    s->d661 = c->d61a[0xD661u - 0xD61Au];
    s->d664 = c->d61a[0xD664u - 0xD61Au];
    s->d665 = c->d61a[0xD665u - 0xD61Au];
    s->d666 = c->d61a[0xD666u - 0xD61Au];
    s->g109 = c->g109;
    s->d60c = c->d60c_0;
}

/* the simulated vehicle frames besides the lever (the bench tool's
 * shadow-check sims): 0x140 brake switch, 0x108 engine speed, both E2E */
typedef struct {
    bool     brake;         /* send 0x140 with byte 0 bits 3..2 = 01 (pressed) */
    bool     brake_off;     /* send 0x140 with 00 (released, valid) */
    uint16_t engine;        /* send 0x108 with bytes 0-1 = engine (0 = no frame) */
    uint32_t lever_from;    /* first tick that carries lever frames */
    uint8_t  os_mode;       /* passed to the shadow (the stock runs it in 1..3) */
    uint8_t  g98;           /* the park_pair group-98 verdict passed in */
} sim_t;
static const sim_t k_sim_none = { false, false, 0u, 0u, 1u, 0u };
static sim_t s_sim;

static void sim_frames(void)
{
    uint8_t d[8];

    if (s_sim.brake || s_sim.brake_off) {
        memset(d, 0, sizeof d);
        d[0] = s_sim.brake ? 0x04u : 0x00u;
        send(CAN_BUS1, 0x140u, 8u, true, d);
    }
    if (s_sim.engine != 0u) {
        memset(d, 0, sizeof d);
        d[0] = (uint8_t)(s_sim.engine >> 8);
        d[1] = (uint8_t)s_sim.engine;
        d[2] = 0x1Fu;                       /* torque T1 invalid (0x1FFF) */
        d[3] = 0xFFu;
        send(CAN_BUS1, 0x108u, 8u, true, d);
    }
}

/* the live path, the shadow optional, over a schedule */
static void run(const uint8_t *sched, uint32_t nseg, bool shadow, live_t *lv, shad_t *sh)
{
    memset(&s_z, 0, sizeof s_z);
    memset(s_ctr, 0, sizeof s_ctr);
    /* every run from a zeroed stock cell window, so a stray write of one
     * run cannot hide in the next run's reference */
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    CELL_TMP_BYTE = 67u;
    vehicle_init();
    park_init();
    if (shadow) {
        rng_shadow_init();
    }
    for (uint32_t t = 0u; t < nseg * SEG; t++) {
        if (t >= s_sim.lever_from) {
            lever_frame(sched[t / SEG]);
        }
        sim_frames();
        s_z.output_speed = 0;
        s_z.q_output_speed = Q_OK;
        vehicle_update_10ms();
        if (shadow) {
            rng_shadow_tick_10ms(s_sim.os_mode, s_sim.g98);
        }
        park_tick_10ms();
        if (lv != NULL) {
            live_snap(&lv[t]);
        }
        if (sh != NULL) {
            shad_snap(&sh[t]);
        }
    }
}

/* ── 1. read-only ───────────────────────────────────────────────────────── */
static void readonly_once(const sim_t *sim, const char *name)
{
    static live_t ref[N_T], with[N_T];
    unsigned rel = 0u;

    s_sim = *sim;
    run(k_sched, N_SEG, false, ref, NULL);
    run(k_sched, N_SEG, true, with, NULL);
    for (uint32_t t = 0u; t < N_T; t++) {
        rel += ref[t].rel != 0u;
        if (memcmp(&ref[t], &with[t], sizeof ref[t]) != 0) {
            printf("FAIL read-only (%s): live path differs with the shadow at tick %u\n",
                   name, (unsigned)t);
            fails++;
            break;
        }
    }
    CHECK(rel > 0u);            /* the live r03k path does release on R/N/D */
    s_sim = k_sim_none;
}

static void test_readonly(void)
{
    sim_t sim = k_sim_none;

    readonly_once(&sim, "lever only");
    sim.brake = true;           /* + 0x140 brake ON, 0x108 700 rpm, the P0850 verdict */
    sim.engine = 700u;
    sim.g98 = 1u;
    readonly_once(&sim, "lever + brake + engine");
    sim.lever_from = 60u;       /* + lever late: U1267 fails in the shadow */
    readonly_once(&sim, "late lever + brake + engine");
}

/* ── 2. the shadow on the dump stand-ins ────────────────────────────────── */
static void test_shadow_dump(void)
{
    static shad_t sh[N_T];
    const rng_shadow_wit_t *w;
    uint8_t prev_lev = 0u;
    unsigned pulses = 0u, b6_8 = 0u;

    run(k_sched, N_SEG, true, NULL, sh);
    w = rng_shadow_wit();
    /* the D5FB latch: 1 until the first valid P frame, 0 from then on */
    CHECKV(sh[0].d5fb, 0);              /* tick 0 already carries the P frame */
    for (uint32_t t = 0u; t < N_T; t++) {
        uint8_t lev = k_code[k_sched[t / SEG]];

        /* the last segment sends no frame: the lever stays valid until the
         * 50 ms receive timeout, then D673 = 2 and D672 = 0 */
        if (k_sched[t / SEG] == 0u && sh[t].d673 != 2u) {
            lev = prev_lev;
        }

        /* E531 follows the lever (D672) */
        CHECKV(sh[t].e531, lev);
        CHECKV(sh[t].d672, lev);
        CHECKV(sh[t].d673, lev != 0u ? 0 : 2);
        /* E53C / 0x8941A: a pulse on the pass the lever changes */
        if (lev != prev_lev && lev != 0u) {
            CHECKV(sh[t].e53c, lev);
            CHECKV(sh[t].a, lev);
            pulses++;
            /* the filter: at D6D6 = 8, P passes, R/N/D become 0 */
            CHECKV(sh[t].b6, lev == 8u ? 8 : 0);
            b6_8 += sh[t].b6 == 8u;
        } else {
            CHECKV(sh[t].e53c, 0);
            CHECKV(sh[t].a, 0);
            CHECKV(sh[t].b6, 0);
        }
        CHECKV(sh[t].ce, sh[t].b6);
        /* 893E8 = 0; E545 = D6DE = D6D6 = 8, reasons 0 */
        CHECKV(sh[t].s8, 0);
        CHECKV(sh[t].e545, 8);
        CHECKV(sh[t].de, 8);
        CHECKV(sh[t].df, 0);
        CHECKV(sh[t].d6, 8);
        CHECKV(sh[t].d9, 0);
        CHECKV(sh[t].c304, 8);
        if (lev == 0u) {
            CHECKV(sh[t].d5fb, 1);      /* no source again: D673 = 2 sets it */
        } else {
            CHECKV(sh[t].d5fb, 0);
        }
        /* no brake / engine / 0x14B frame: the dump's gathered bytes */
        CHECKV(sh[t].d631, 2);
        CHECKV(sh[t].d64b, 0);
        CHECKV(sh[t].d63c, 2);
        CHECKV(sh[t].d63a, 0);
        CHECKV(sh[t].d607, 0);
        /* U1267: tested from pass 50 (D60C = 51 > 50), failed once D673 = 2
         * (the lever times out at tick 154, an even pass = a 20 ms verdict);
         * the reaction bytes follow on the next gather; D665 never */
        CHECKV(sh[t].g109, t >= 154u ? 1 : 0);
        CHECKV(sh[t].d664, t >= 155u ? 1 : 0);
        CHECKV(sh[t].d666, t >= 155u ? 1 : 0);
        CHECKV(sh[t].d661, t >= 155u ? 1 : 0);
        CHECKV(sh[t].d660, t >= 155u ? 1 : 0);     /* g98 = 0 in this run */
        CHECKV(sh[t].d665, 0);
        CHECKV(sh[t].d60c, t + 1u);
        prev_lev = lev;
    }
    CHECKV(pulses, 5);                  /* P R N D P */
    CHECKV(b6_8, 2);
    CHECKV(w->passes, N_T);
    CHECKV(w->c5f14_n, N_T);            /* cal 0x19606C = 0xFA > 0: every pass */
    CHECKV(w->mask_n[5], N_T);          /* +0x70: the mask decides every pass */
    CHECKV(w->mask_n[0] + w->mask_n[1] + w->mask_n[2] + w->mask_n[3] + w->mask_n[6], 0);
    CHECKV(w->guard_c290e_n, 0);
    CHECKV(w->guard_c792e_n, 0);
    CHECKV(w->report_c78be_n, N_T);     /* 0xC78BE's reporter is called every pass */
    CHECKV(w->ovr_n, 0);
    CHECKV(w->s8942c_n, 0);
    /* the init pass: the 0xA548C arbiter at 304C+0 = 0 → 8 / 0x17 */
    {
        const rng_shadow_cells_t *c = rng_shadow_cells();

        CHECKV(c->e530[0xE549u - 0xE530u], 0);
        CHECKV(c->e530[0xE54Au - 0xE530u], 8);
        CHECKV(c->e530[0xE54Bu - 0xE530u], 0x17);
        CHECKV(c->d6d4[0xD6D7u - 0xD6D4u], 0xF7);
        CHECKV(c->o304c[1], 0xFFFFFFF7u);
        CHECKV(c->e64b, 8);
        CHECKV(c->d5e6[0xD5F2u - 0xD5E6u], 1);   /* D641 = 2 → D5F2 1 (dump 01) */
        CHECKV(c->d5e6[0xD5F3u - 0xD5E6u], 1);   /* dump 01 */
        CHECKV(c->d5e6[0xD607u - 0xD5E6u], 0);   /* dump 00 */
        CHECKV(c->x893e0[0x893EAu - 0x893E0u], 1);   /* dump 0x893EA = 01 */
        CHECKV(c->flt_en[4], 1);                 /* dump 0x89438 = 01 */
    }
    /* the recorder holds the change passes (P R N D P none, plus the
     * no-source D5FB flip) */
    CHECK(w->rec_n >= 6u);
}

/* started in R before any P */
static void test_start_r(void)
{
    static const uint8_t sch[2] = { 2u, 1u };    /* R, then P */
    static shad_t sh[2u * SEG];

    run(sch, 2u, true, NULL, sh);
    for (uint32_t t = 0u; t < SEG; t++) {
        CHECKV(sh[t].d5fb, 1);          /* D672 7 != D6D6 8: the latch holds */
        CHECKV(sh[t].e53c, 0);          /* 0xC7DE2: D5FB = 1 blocks R */
        CHECKV(sh[t].d6, 8);
    }
    CHECKV(sh[SEG].d5fb, 0);            /* the P frame releases it */
    CHECKV(sh[SEG].e53c, 8);
    CHECKV(sh[SEG].d6, 8);
}

/* ── 3. positive control (host hooks) ───────────────────────────────────── */
static void test_positive_control(void)
{
    static const uint8_t sch[3] = { 1u, 2u, 2u };   /* P, R, R */
    static shad_t sh[3u * SEG];

    rng_shadow_test_clear();
    rng_shadow_test_override((uint8_t)(0xD64Bu - 0xD61Au), 1u);   /* D607 = 1: filter +0 off */
    rng_shadow_test_override((uint8_t)(0xD631u - 0xD61Au), 0u);
    rng_shadow_test_override((uint8_t)(0xD63Eu - 0xD61Au), 0x01u);   /* D63E = 0x01F4 = 500 */
    rng_shadow_test_override((uint8_t)(0xD63Fu - 0xD61Au), 0xF4u);
    /* the dump's reaction bits (U1267 failed): the shadow now computes them
     * (0 here — the lever is valid from tick 0), so force the dump's */
    rng_shadow_test_override((uint8_t)(0xD666u - 0xD61Au), 1u);
    rng_shadow_test_override((uint8_t)(0xD664u - 0xD61Au), 1u);
    run(sch, 3u, true, NULL, sh);
    CHECKV(sh[SEG - 1u].s8, 1);         /* 893E8 → 1 (speed word > 430) */
    /* the R pass: 0x894B6 = 7, D6CE 7, E545 7 (0xC7A56), D6DE 7, then
     * 0xC767C: D6D8 8, 893E8 != 0, D6DE 7, D666 = 1 → 8 / 0x33 */
    CHECKV(sh[SEG].e53c, 7);
    CHECKV(sh[SEG].b6, 7);
    CHECKV(sh[SEG].ce, 7);
    CHECKV(sh[SEG].e545, 7);
    CHECKV(sh[SEG].de, 7);
    CHECKV(sh[SEG].d6, 8);
    CHECKV(sh[SEG].d9, 0x33);

    rng_shadow_test_override((uint8_t)(0xD666u - 0xD61Au), 0u);
    rng_shadow_test_override((uint8_t)(0xD664u - 0xD61Au), 0u);
    run(sch, 3u, true, NULL, sh);
    CHECKV(sh[SEG].de, 7);
    CHECKV(sh[SEG].d6, 7);              /* the stock's cyclic D6D6 = 7 */
    CHECKV(sh[SEG].d9, 0);
    CHECKV(sh[SEG].c304, 7);
    for (uint32_t t = SEG + 1u; t < 3u * SEG; t++) {
        CHECKV(sh[t].e545, 7);          /* held through 0xC7BAA (D6CE 0 → E545 = D6D6) */
        CHECKV(sh[t].d6, 7);
    }
    rng_shadow_test_clear();
}

/* ── 4. at rest on the bench: no CAN frame at all, the P0850 verdict
 *    failed (park_pair on this bench) — the dump's reaction bytes once
 *    U1267 has failed, D6D6 8 throughout (the r03q trace) ─────────────── */
static void test_rest(void)
{
    static const uint8_t sch[4] = { 0u, 0u, 0u, 0u };
    static shad_t sh[4u * SEG];
    const rng_shadow_cells_t *c = rng_shadow_cells();

    s_sim = k_sim_none;
    s_sim.g98 = 1u;
    run(sch, 4u, true, NULL, sh);
    for (uint32_t t = 0u; t < 4u * SEG; t++) {
        CHECKV(sh[t].d6, 8);
        CHECKV(sh[t].d9, 0);
        CHECKV(sh[t].de, 8);
        CHECKV(sh[t].e545, 8);
        CHECKV(sh[t].s8, 0);
        CHECKV(sh[t].d660, 1);                  /* g98 latched before pass 0's gather */
        CHECKV(sh[t].g109, t >= 50u ? 1 : 0);   /* the first verdict with D60C > 50 */
        CHECKV(sh[t].d664, t >= 51u ? 1 : 0);
        CHECKV(sh[t].d666, t >= 51u ? 1 : 0);
        CHECKV(sh[t].d661, t >= 51u ? 1 : 0);
    }
    /* from pass 51 the gathered struct carries the dump's reaction bytes */
    CHECKV(c->d61a[0xD660u - 0xD61Au], 1);
    CHECKV(c->d61a[0xD661u - 0xD61Au], 1);
    CHECKV(c->d61a[0xD664u - 0xD61Au], 1);
    CHECKV(c->d61a[0xD665u - 0xD61Au], 0);
    CHECKV(c->d61a[0xD666u - 0xD61Au], 1);
    CHECKV(c->u_fail_n, 4u * SEG / 2u - 25u);  /* verdicts 50, 52, .., 118 */
    CHECKV(c->u_pass_n, 0);
    CHECKV(c->d60c_4, 4u * SEG);               /* D5F2 = 1 every pass */
    CHECKV(c->p85160, 1);
    s_sim = k_sim_none;
}

/* ── 5. the mode gate: no pass in OS mode 0 ─────────────────────────────── */
static void test_mode_gate(void)
{
    static const uint8_t sch[1] = { 1u };
    const rng_shadow_cells_t *c = rng_shadow_cells();
    const rng_shadow_wit_t *w = rng_shadow_wit();

    s_sim = k_sim_none;
    s_sim.os_mode = 0u;
    run(sch, 1u, true, NULL, NULL);
    CHECKV(w->passes, 0);
    CHECKV(c->skip_mode_n, SEG);
    CHECKV(c->d60c_0, 0);
    CHECKV(c->d5e6[0xD5FBu - 0xD5E6u], 1);     /* the one-shot's 1, untouched */
    CHECKV(c->d6d4[0xD6D6u - 0xD6D4u], 8);
    s_sim.os_mode = 4u;
    run(sch, 1u, true, NULL, NULL);
    CHECKV(w->passes, 0);
    s_sim = k_sim_none;
}

/* ── 6. decode §9: lever from power-on + brake ON + engine 700 rpm ──────── */
static void test_live_release(void)
{
    static const uint8_t sch[6] = { 1u, 2u, 3u, 4u, 1u, 2u };   /* P R N D P R */
    static shad_t sh[6u * SEG];
    const rng_shadow_cells_t *c = rng_shadow_cells();

    s_sim = k_sim_none;
    s_sim.brake = true;
    s_sim.engine = 700u;
    s_sim.g98 = 1u;
    run(sch, 6u, true, NULL, sh);
    for (uint32_t t = 0u; t < 6u * SEG; t++) {
        const uint8_t seg = sch[t / SEG];
        const uint8_t want = seg == 1u ? 8u : seg == 2u ? 7u : seg == 3u ? 6u : 5u;

        CHECKV(sh[t].d631, 0);
        CHECKV(sh[t].d64b, 1);
        CHECKV(sh[t].d63c, 0);
        CHECKV(sh[t].d63a, 700);
        CHECKV(sh[t].d607, 1);              /* 0xA90A0: brake on and valid */
        CHECKV(sh[t].s8, 1);                /* 0xC3900: 0 → 1 on the first pass (700 > 430) */
        CHECKV(sh[t].g109, 0);              /* U1267 never failed: the lever is always valid */
        CHECKV(sh[t].d664, 0);
        CHECKV(sh[t].d666, 0);
        CHECKV(sh[t].d661, 0);
        CHECKV(sh[t].d660, 1);              /* P0850 only (g98) */
        CHECKV(sh[t].d5fb, 0);
        CHECKV(sh[t].d6, want);
        CHECKV(sh[t].d9, 0);
        CHECKV(sh[t].c304, want);
        if (sh[t].d6 != want) {
            printf("  t=%u lever %u: D6D6 %u reason 0x%02X D6DE %u/%02X E545 %u D6CE %u 894B6 %u\n",
                   (unsigned)t, want, sh[t].d6, sh[t].d9, sh[t].de, sh[t].df, sh[t].e545,
                   sh[t].ce, sh[t].b6);
            break;
        }
    }
    CHECK(c->u_pass_n > 0u);                /* tested from pass 50, not failed */
    CHECKV(c->u_fail_n, 0);
    s_sim = k_sim_none;
}

/* ── 7. the same without the brake: the filter keeps P ─────────────────── */
static void test_live_nobrake(void)
{
    static const uint8_t sch[5] = { 1u, 2u, 3u, 4u, 1u };
    static shad_t sh[5u * SEG];

    s_sim = k_sim_none;
    s_sim.engine = 700u;
    s_sim.brake_off = true;                 /* 0x140 valid, pedal released */
    s_sim.g98 = 1u;
    run(sch, 5u, true, NULL, sh);
    for (uint32_t t = 0u; t < 5u * SEG; t++) {
        CHECKV(sh[t].d631, 0);
        CHECKV(sh[t].d64b, 0);
        CHECKV(sh[t].d607, 0);
        CHECKV(sh[t].s8, 1);
        CHECKV(sh[t].d6, 8);
        CHECKV(sh[t].d9, 0);
        CHECKV(sh[t].de, 8);
    }
    /* and with no 0x140 at all (brake invalid, 0x14B absent): the same */
    s_sim.brake_off = false;
    run(sch, 5u, true, NULL, sh);
    for (uint32_t t = 0u; t < 5u * SEG; t++) {
        CHECKV(sh[t].d631, 2);
        CHECKV(sh[t].d607, 0);
        CHECKV(sh[t].d6, 8);
    }
    s_sim = k_sim_none;
}

/* ── 8. lever frames late (> 0.5 s of chain passes): U1267 fails at the
 *    pass-50 verdict; R / N / D then stay in P for the power cycle ───── */
static void test_live_late_lever(void)
{
    static const uint8_t sch[7] = { 1u, 1u, 1u, 2u, 3u, 4u, 1u };   /* (none none) P R N D P */
    static shad_t sh[7u * SEG];
    uint8_t rsn[3] = { 0u, 0u, 0u };

    s_sim = k_sim_none;
    s_sim.brake = true;
    s_sim.engine = 700u;
    s_sim.g98 = 1u;
    s_sim.lever_from = 60u;                 /* first lever frame on chain pass 60 */
    run(sch, 7u, true, NULL, sh);
    CHECKV(sh[49].g109, 0);
    CHECKV(sh[50].g109, 1);                 /* D60C 51 > 50 and D673 = 2 at the pass-50 verdict */
    CHECKV(sh[50].d664, 0);
    CHECKV(sh[51].d664, 1);                 /* read by the next gather */
    CHECKV(sh[51].d666, 1);
    CHECKV(sh[51].d661, 1);
    for (uint32_t t = 60u; t < 7u * SEG; t++) {
        CHECKV(sh[t].d6, 8);                /* P kept */
        CHECKV(sh[t].g109, 1);              /* latched (healing not decoded) */
        CHECKV(sh[t].s8, 1);
        CHECKV(sh[t].d607, 1);
        if (t % SEG == SEG - 1u && t / SEG >= 3u) {
            /* the candidate's answer is a one-pass event: by the end of
             * the step D6DE / E545 are back at 8 and the reason at 0 */
            CHECKV(sh[t].de, 8);
            CHECKV(sh[t].e545, 8);
            CHECKV(sh[t].d9, 0);
        }
        if (t % SEG == 0u && t / SEG >= 3u && t / SEG <= 5u) {
            rsn[t / SEG - 3u] = sh[t].d9;
            printf("  late lever: lever %u -> D6DE %u/%02X, D6D6 %u reason 0x%02X\n",
                   (unsigned)sch[t / SEG], sh[t].de, sh[t].df, sh[t].d6, sh[t].d9);
        }
    }
    /* from the bytes (0xC7390's candidate order C77FE, C7444, C747C, C767C,
     * C7756, ..): R → 0xC767C (D6DE 7, D666 = 1) 8 / 0x33; N and D →
     * 0xC7756 (D664 = 1) 8 / 0x3C. (The decode §9 had R → 0x3C.) */
    CHECKV(rsn[0], 0x33);
    CHECKV(rsn[1], 0x3C);
    CHECKV(rsn[2], 0x3C);
    s_sim = k_sim_none;
}

int main(void)
{
    s_sim = k_sim_none;
    test_readonly();
    test_shadow_dump();
    test_start_r();
    test_positive_control();
    test_rest();
    test_mode_gate();
    test_live_release();
    test_live_nobrake();
    test_live_late_lever();
    if (fails != 0) {
        printf("%d failure(s)\n", fails);
        return 1;
    }
    printf("rng_shadow: all checks passed\n");
    return 0;
}

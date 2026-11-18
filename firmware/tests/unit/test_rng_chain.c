/* Unit test: THE RANGE CHAIN, LIVE (app/rng_chain.c; 2026-11-17,
 * ~/tcu_sweep/range_chain_live_port.md — the r03q..r03s shadow made the live
 * owner of D6D6). Every scenario runs the firmware's 10 ms order: vehicle →
 * rng_chain_tick_10ms → park_tick_10ms (the park law and park_mon) → the two
 * park demands. The chain's cells are read at their STOCK addresses.
 *
 * 1. No sims (the bench at rest, lever P R N D P none): the dump-stand-in
 *    trace of r03q (E531 follows the lever; D5FB 1 → 0 on the first P; the
 *    one-pass pulses; the 0x89434 filter maps R/N/D at D6D6 = 8 to 0;
 *    893E8 = 0; D6D6 = 8 throughout) AND THE PARK OUTPUTS OFF THROUGHOUT —
 *    the stock holds P on this bench; started in R before any P: the same.
 * 2. A positive control (host hooks inside the chain): the chain reaches
 *    D6D6 = 7 on gathered bytes the raw signals do not support (no 0x140, no
 *    0x108) — park_mon, which reads the raw signals, latches and both
 *    demands stay 0: the monitor is independent of the chain's inputs.
 * 3. At rest with the P0850 verdict: U1267 / P0850 confirm as on r03s; the
 *    outputs stay off; the OS-mode gate (no pass in modes 0 / 4: D6D6 keeps
 *    the init arbiter's 8).
 * 4. Lever + 0x140 brake ON + 0x108 700 rpm from power-on (r03s (b)): P 8,
 *    R 7, N 6, D 5, P 8, reasons 0 — RELEASE + HOLD ON in R/N/D, both off in
 *    P (model 2 → 1, 3 → 4), park_mon silent; identical with and without
 *    the P0850 verdict; without the brake: 8 and off throughout.
 * 5. Lever late by 0.1 s / 1.1 s, and lever-frame gaps (the U1267 debounce
 *    of r03s): the outputs follow D6D6; a 2 s gap with R held → U1267 →
 *    0xC76D8 6 / 0x6F (still released: N), healed 500 ms after the frames
 *    return; P at the end engages.
 * 6. park_mon mutations in the loop: the chain's D6D6 overwritten with a
 *    wrong value after its pass — a release with no brake / no engine / no
 *    lever, an out-of-range value, a torn complement — latched at once,
 *    both demands 0 and held there while the chain later releases legally.
 */
#include "app/rng_chain.h"
#include "app/park.h"
#include "app/park_cells.h"
#include "app/range.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_cells.h"
#include "io/vehicle/vehicle.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)


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
        send(CAN_BUS2, 0x210u, 5u, true, l);
    }
}

#define SEG    30u
#define N_SEG  6u
#define N_T    (SEG * N_SEG)
static const uint8_t k_sched[N_SEG] = { 1u, 2u, 3u, 4u, 1u, 0u };   /* P R N D P none */
static const uint8_t k_code[5] = { 0u, 8u, 7u, 6u, 5u };

/* the chain's cells at their stock addresses (host: the mapped window) */
#define C8(a)   (*(volatile uint8_t *)(uintptr_t)(0xFFF80000u | (uint32_t)(a)))
#define U16(a)  (*(volatile uint16_t *)(uintptr_t)(0xFFF80000u | (uint32_t)(a)))
#define U32(a)  (*(volatile uint32_t *)(uintptr_t)(0xFFF80000u | (uint32_t)(a)))

typedef struct {
    uint8_t d672, d673, d5fb, e531, e53c, a, b6, ce, e545, de, df, d6, d9, s8;
    uint32_t c304;
    uint8_t d607, d631, d64b, d63c, d660, d661, d664, d665, d666, g109, g98;
    uint16_t d63a, d60c, t109;
    /* the live outputs */
    uint16_t rel, hold;
    uint8_t  d6c, ae29, lock, state, mon;
} shad_t;

static void shad_snap(shad_t *s)
{
    const rng_chain_model_t *m = rng_chain_model();

    s->d672 = C8(0xD672u);
    s->d673 = C8(0xD673u);
    s->d5fb = C8(0xD5FBu);
    s->e531 = C8(0xE531u);
    s->e53c = C8(0xE53Cu);
    s->a = C8(0x8941Au);
    s->b6 = C8(0x894B6u);
    s->ce = C8(0xD6CEu);
    s->e545 = C8(0xE545u);
    s->de = C8(0xD6DEu);
    s->df = C8(0xD6DFu);
    s->d6 = C8(0xD6D6u);
    s->d6c = C8(0xD6D7u);
    s->d9 = C8(0xD6D9u);
    s->s8 = C8(0x893E8u);
    s->c304 = U32(0x304Cu);
    s->d607 = C8(0xD607u);
    s->d631 = C8(0xD631u);
    s->d64b = C8(0xD64Bu);
    s->d63c = C8(0xD63Cu);
    s->d63a = (uint16_t)(C8(0xD63Au) << 8 | C8(0xD63Bu));
    s->d660 = C8(0xD660u);
    s->d661 = C8(0xD661u);
    s->d664 = C8(0xD664u);
    s->d665 = C8(0xD665u);
    s->d666 = C8(0xD666u);
    s->g109 = m->g109;
    s->g98 = m->g98;
    s->d60c = U16(0xD60Cu);
    s->t109 = m->fm109.d714;
    s->rel = park_slot7_release();
    s->hold = park_slot11_hold();
    s->ae29 = PCELL_AE29;
    s->lock = park_law_lock();
    s->state = park_law_state();
    s->mon = park_mon_faults();
}

/* outputs: release + hold both on (1), both off (0), anything else (2) */
static int outs(const shad_t *s)
{
    if (s->rel == DRV_PARK_ON_REQ && s->hold == DRV_PARK_ON_REQ) {
        return 1;
    }
    return (s->rel == 0u && s->hold == 0u) ? 0 : 2;
}

/* the simulated vehicle frames besides the lever (the bench tool's
 * shadow-check sims): 0x140 brake switch, 0x108 engine speed, both E2E */
typedef struct {
    bool     brake;         /* send 0x140 with byte 0 bits 3..2 = 01 (pressed) */
    bool     brake_off;     /* send 0x140 with 00 (released, valid) */
    uint16_t engine;        /* send 0x108 with bytes 0-1 = engine (0 = no frame) */
    uint32_t lever_from;    /* first tick that carries lever frames */
    uint8_t  os_mode;       /* passed to the shadow (the stock runs it in 1..3) */
    uint8_t  g98;           /* the park_pair group-98 verdict passed in (failed; tested = 1) */
    uint32_t gap_from;      /* lever frames withheld for gap_len ticks from here */
    uint32_t gap_len;
} sim_t;
static const sim_t k_sim_none = { false, false, 0u, 0u, 1u, 0u, 0u, 0u };
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

/* a mutation of the chain's output (park_mon tests): called after the
 * chain's pass and before park_tick, may overwrite D6D6 / D6D7 */
static void (*s_mut)(uint32_t t);

/* the firmware's 10 ms order over a schedule */
static void run(const uint8_t *sched, uint32_t nseg, shad_t *sh)
{
    memset(s_ctr, 0, sizeof s_ctr);
    /* every run from a zeroed stock cell window */
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    CELL_TMP_BYTE = 67u;
    vehicle_init();
    park_init();
    rng_chain_init();
    for (uint32_t t = 0u; t < nseg * SEG; t++) {
        if (t >= s_sim.lever_from &&
            !(t >= s_sim.gap_from && t < s_sim.gap_from + s_sim.gap_len)) {
            lever_frame(sched[t / SEG]);
        }
        sim_frames();
        vehicle_update_10ms();
        rng_chain_tick_10ms(s_sim.os_mode, s_sim.g98, 1u);
        if (s_mut != NULL) {
            s_mut(t);
        }
        park_tick_10ms();
        if (sh != NULL) {
            shad_snap(&sh[t]);
        }
    }
}

/* ── 2. the shadow on the dump stand-ins ────────────────────────────────── */
static void test_dump(void)
{
    static shad_t sh[N_T];
    const rng_chain_wit_t *w;
    uint8_t prev_lev = 0u;
    unsigned pulses = 0u, b6_8 = 0u;

    run(k_sched, N_SEG, sh);
    w = rng_chain_wit();
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
        CHECKV(sh[t].d6c, 0xF7);
        CHECKV(sh[t].d9, 0);
        CHECKV(sh[t].c304, 8);
        /* LIVE: the stock holds P on this bench — both park outputs off */
        CHECKV(outs(&sh[t]), 0);
        CHECKV(sh[t].ae29, 1);
        CHECKV(sh[t].mon, 0);
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
         * (the lever times out at tick 154, an even pass = a 20 ms verdict):
         * the fault manager's up/down timer starts at 1 ms and adds 20 ms
         * per failed verdict — 26 verdicts (500 ms) are needed, the run
         * ends after 13, so no reaction byte is ever set */
        CHECKV(sh[t].t109, t < 154u ? 0 : 1u + 20u * ((t - 154u) / 2u));
        CHECKV(sh[t].g109, 0);
        CHECKV(sh[t].d664, 0);
        CHECKV(sh[t].d666, 0);
        CHECKV(sh[t].d661, 0);
        CHECKV(sh[t].d660, 0);                     /* g98 = 0 in this run */
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
    CHECKV(w->flt_ptr_bad_n, 0);
    /* the 0x89434 sub-objects' table pointers and the 0x15BB28 tables at the
     * stock addresses, as the dump holds them */
    CHECK(U32(0x89434u) == 0xFFF8E64Cu && C8(0x89438u) == 1u && U32(0x89498u) == 0xFFF8E67Fu &&
          U32(0x894A4u) == 0xFFF8E65Du && U32(0x89468u) == 0xFFF8E64Cu);
    CHECK(C8(0xE64Cu) == 0xFFu && C8(0xE64Du) == 0x00u && C8(0xE65Cu) == 0x01u &&
          C8(0xE66Du) == 0x03u && C8(0xE67Eu) == 0x02u && C8(0xE68Fu) == 0x0Du &&
          C8(0xE68Cu) == 0x06u);
    /* the init pass: the 0xA548C arbiter at 304C+0 = 0 → 8 / 0x17 */
    {
        CHECKV(C8(0xE549u), 0);
        CHECKV(C8(0xE54Au), 8);
        CHECKV(C8(0xE54Bu), 0x17);
        CHECKV(C8(0xD6D7u), 0xF7);
        CHECKV(U32(0x3050u), 0xFFFFFFF7u);
        CHECKV(C8(0xE64Bu), 8);
        CHECKV(C8(0xD5F2u), 1);   /* D641 = 2 → D5F2 1 (dump 01) */
        CHECKV(C8(0xD5F3u), 1);   /* dump 01 */
        CHECKV(C8(0xD607u), 0);   /* dump 00 */
        CHECKV(C8(0x893EAu), 1);   /* dump 0x893EA = 01 */
        CHECKV(C8(0x89438u), 1);                 /* dump 0x89438 = 01 */
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

    run(sch, 2u, sh);
    for (uint32_t t = 0u; t < SEG; t++) {
        CHECKV(sh[t].d5fb, 1);          /* D672 7 != D6D6 8: the latch holds */
        CHECKV(sh[t].e53c, 0);          /* 0xC7DE2: D5FB = 1 blocks R */
        CHECKV(sh[t].d6, 8);
        CHECKV(outs(&sh[t]), 0);
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

    rng_chain_test_clear();
    rng_chain_test_override((uint8_t)(0xD64Bu - 0xD61Au), 1u);   /* D607 = 1: filter +0 off */
    rng_chain_test_override((uint8_t)(0xD631u - 0xD61Au), 0u);
    rng_chain_test_override((uint8_t)(0xD63Eu - 0xD61Au), 0x01u);   /* D63E = 0x01F4 = 500 */
    rng_chain_test_override((uint8_t)(0xD63Fu - 0xD61Au), 0xF4u);
    /* the dump's reaction bits (U1267 failed): the shadow now computes them
     * (0 here — the lever is valid from tick 0), so force the dump's */
    rng_chain_test_override((uint8_t)(0xD666u - 0xD61Au), 1u);
    rng_chain_test_override((uint8_t)(0xD664u - 0xD61Au), 1u);
    run(sch, 3u, sh);
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

    rng_chain_test_override((uint8_t)(0xD666u - 0xD61Au), 0u);
    rng_chain_test_override((uint8_t)(0xD664u - 0xD61Au), 0u);
    run(sch, 3u, sh);
    CHECKV(sh[SEG].de, 7);
    CHECKV(sh[SEG].d6, 7);              /* the stock's cyclic D6D6 = 7 */
    CHECKV(sh[SEG].d9, 0);
    CHECKV(sh[SEG].c304, 7);
    for (uint32_t t = SEG + 1u; t < 3u * SEG; t++) {
        CHECKV(sh[t].e545, 7);          /* held through 0xC7BAA (D6CE 0 → E545 = D6D6) */
        CHECKV(sh[t].d6, 7);
    }
    /* LIVE: the chain's 7 rests on gathered bytes the raw signals do not
     * carry (no 0x140, no 0x108): park_mon, reading the raw signals,
     * latches brake + engine at the transition — both demands stay 0 */
    CHECKV(sh[SEG].mon, MON_F_BRAKE | MON_F_ENGINE);
    for (uint32_t t = 0u; t < 3u * SEG; t++) {
        CHECKV(outs(&sh[t]), 0);
    }
    rng_chain_test_clear();
}

/* ── 4. at rest on the bench: no CAN frame at all, the P0850 verdict
 *    failed (park_pair on this bench) — the dump's reaction bytes once
 *    U1267 has failed, D6D6 8 throughout (the r03q trace) ─────────────── */
static void test_rest(void)
{
    static const uint8_t sch[10] = { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    static shad_t sh[10u * SEG];
    const rng_chain_model_t *c = rng_chain_model();

    s_sim = k_sim_none;
    s_sim.g98 = 1u;
    run(sch, 10u, sh);
    for (uint32_t t = 0u; t < 10u * SEG; t++) {
        CHECKV(sh[t].d6, 8);
        CHECKV(sh[t].d9, 0);
        CHECKV(sh[t].de, 8);
        CHECKV(sh[t].e545, 8);
        CHECKV(sh[t].s8, 0);
        CHECKV(outs(&sh[t]), 0);
        CHECKV(sh[t].mon, 0);
        /* U1267: the first failed verdict at pass 50 starts the timer at
         * 1 ms; +20 ms per verdict; 1 + 25 * 20 >= 500 at pass 100 */
        CHECKV(sh[t].g109, t >= 100u ? 1 : 0);
        CHECKV(sh[t].d664, t >= 101u ? 1 : 0);
        CHECKV(sh[t].d666, t >= 101u ? 1 : 0);
        CHECKV(sh[t].d661, t >= 101u ? 1 : 0);
        /* P0850 (group 98, failed every 10 ms from tick 0): 1 + 10 k >=
         * 2500 at k = 250 (the dump: confirmed at 2690 ms, DD2C 2500) */
        CHECKV(sh[t].g98, t >= 250u ? 1 : 0);
        CHECKV(sh[t].d660, t >= 101u ? 1 : 0);
    }
    CHECKV(c->fm109.confirm_n, 1);
    CHECKV(c->fm109.heal_n, 0);
    CHECKV(c->fm109.dd2c, 500);                /* the dump's 0xFFF8DD2C[108] */
    CHECKV(c->fm109.st & 0x200Fu, 0x200Bu);    /* tested, failed, confirmed (dump 0x36BB) */
    CHECKV(c->fm98.dd2c, 2500);                /* the dump's 0xFFF8DD2C[97] */
    CHECKV(c->fm98.confirm_n, 1);
    /* from pass 51 the gathered struct carries the dump's reaction bytes */
    CHECKV(C8(0xD660u), 1);
    CHECKV(C8(0xD661u), 1);
    CHECKV(C8(0xD664u), 1);
    CHECKV(C8(0xD665u), 0);
    CHECKV(C8(0xD666u), 1);
    CHECKV(c->u_fail_n, (10u * SEG - 50u) / 2u);  /* verdicts 50, 52, .., 298 */
    CHECKV(c->u_pass_n, 0);
    CHECKV(U16(0xD610u), 10u * SEG);              /* D5F2 = 1 every pass */
    CHECKV(C8(0x85160u), 1);
    s_sim = k_sim_none;
}

/* ── 5. the mode gate: no pass in OS mode 0 ─────────────────────────────── */
static void test_mode_gate(void)
{
    static const uint8_t sch[1] = { 1u };
    const rng_chain_model_t *c = rng_chain_model();
    const rng_chain_wit_t *w = rng_chain_wit();

    s_sim = k_sim_none;
    s_sim.os_mode = 0u;
    run(sch, 1u, NULL);
    CHECKV(w->passes, 0);
    CHECKV(c->skip_mode_n, SEG);
    CHECKV(U16(0xD60Cu), 0);
    CHECKV(C8(0xD5FBu), 1);     /* the one-shot's 1, untouched */
    CHECKV(C8(0xD6D6u), 8);
    CHECKV(C8(0xD6D7u), 0xF7);
    /* the init arbiter, run once: E549..E54B = 00 08 17, D6D6 its target */
    CHECK(C8(0xE549u) == 0u && C8(0xE54Au) == 8u && C8(0xE54Bu) == 0x17u);
    CHECK(park_slot7_release() == 0u && park_slot11_hold() == 0u && park_mon_faults() == 0u);
    s_sim.os_mode = 4u;
    run(sch, 1u, NULL);
    CHECKV(w->passes, 0);
    s_sim = k_sim_none;
}

/* ── 6. decode §9: lever from power-on + brake ON + engine 700 rpm ──────── */
/* 2026-11-17 (park_chain_loose_ends_decode Q1): the filter sub-object +0x34
 * (0xC823E: 894B8 == 0, engine 893EE > 0x1920BE = 4000 rpm, 893EA != 2), now
 * enabled by the stock's cal mask 0x1920B4 instead of the dump's enable byte,
 * refuses P -> R/N/D at standstill with the engine above 4000 rpm: brake on,
 * engine 4500, lever P then R -> P held, outputs off; at 700 rpm R is taken */
static void test_high_rpm_lockout(void)
{
    static const uint8_t sch[3] = { 1u, 2u, 3u };    /* P R N */
    static shad_t sh[3u * SEG];

    s_sim = k_sim_none;
    s_sim.brake = true;
    s_sim.engine = 4500u;
    s_sim.g98 = 1u;
    run(sch, 3u, sh);
    for (uint32_t t = 0u; t < 3u * SEG; t++) {
        CHECKV(sh[t].d607, 1);
        CHECKV(sh[t].s8, 1);
        CHECKV(sh[t].d6, 8);                /* the lock-out holds P */
        CHECKV(outs(&sh[t]), 0);
        CHECKV(sh[t].mon, 0);
    }
}

static void test_live_release(void)
{
    static const uint8_t sch[6] = { 1u, 2u, 3u, 4u, 1u, 2u };   /* P R N D P R */
    static shad_t sh[6u * SEG];
    const rng_chain_model_t *c = rng_chain_model();

    s_sim = k_sim_none;
    s_sim.brake = true;
    s_sim.engine = 700u;
    s_sim.g98 = 1u;
    run(sch, 6u, sh);
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
        CHECKV(sh[t].d660, 0);              /* P0850 needs 2.5 s of failure (run: 1.8 s) */
        CHECKV(sh[t].d5fb, 0);
        CHECKV(sh[t].d6, want);
        CHECKV(sh[t].d9, 0);
        CHECKV(sh[t].c304, want);
        /* LIVE: release + hold ON out of P, both off in P — the same pass */
        CHECKV(outs(&sh[t]), want == 8u ? 0 : 1);
        CHECKV(sh[t].ae29, want == 8u ? 1 : 0);
        CHECKV(sh[t].mon, 0);
        if (sh[t].d6 != want) {
            printf("  t=%u lever %u: D6D6 %u reason 0x%02X D6DE %u/%02X E545 %u D6CE %u 894B6 %u\n",
                   (unsigned)t, want, sh[t].d6, sh[t].d9, sh[t].de, sh[t].df, sh[t].e545,
                   sh[t].ce, sh[t].b6);
            break;
        }
    }
    CHECK(c->u_pass_n > 0u);                /* tested from pass 50, not failed */
    CHECKV(c->u_fail_n, 0);
    /* the park model: releasing → released 150 ms after the R pass, engaging
     * → engaged 200 ms (27 °C) after the P pass */
    CHECKV(sh[SEG].state, 2);
    CHECKV(sh[SEG + 15u].state, 1);
    CHECKV(sh[4u * SEG].state, 3);
    CHECKV(sh[4u * SEG + 20u].state, 4);
    /* the P0850 verdict (group 98 → D660 → 0xC77A0) does not move the
     * outputs here: the same run with no group-98 report */
    {
        static shad_t sh0[6u * SEG];
        unsigned diff = 0u;

        s_sim.g98 = 0u;
        run(sch, 6u, sh0);
        for (uint32_t t = 0u; t < 6u * SEG; t++) {
            diff += sh0[t].rel != sh[t].rel || sh0[t].hold != sh[t].hold || sh0[t].d6 != sh[t].d6;
        }
        CHECKV(diff, 0);
    }
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
    run(sch, 5u, sh);
    for (uint32_t t = 0u; t < 5u * SEG; t++) {
        CHECKV(sh[t].d631, 0);
        CHECKV(sh[t].d64b, 0);
        CHECKV(sh[t].d607, 0);
        CHECKV(sh[t].s8, 1);
        CHECKV(sh[t].d6, 8);
        CHECKV(sh[t].d9, 0);
        CHECKV(sh[t].de, 8);
        CHECKV(outs(&sh[t]), 0);
        CHECKV(sh[t].mon, 0);
    }
    /* and with no 0x140 at all (brake invalid, 0x14B absent): the same */
    s_sim.brake_off = false;
    run(sch, 5u, sh);
    for (uint32_t t = 0u; t < 5u * SEG; t++) {
        CHECKV(sh[t].d631, 2);
        CHECKV(sh[t].d607, 0);
        CHECKV(sh[t].d6, 8);
        CHECKV(outs(&sh[t]), 0);
    }
    s_sim = k_sim_none;
}

/* ── 8. lever frames late. 8a: by 0.1 s (first frame on chain pass 60):
 *    five failed verdicts (50..58) take the up/down timer to 81 ms, the
 *    passes from 60 take it back to 0 at 68 — never confirmed, so the
 *    release goes through. 8b: by 1.1 s (pass 110): U1267 is confirmed at
 *    the pass-100 verdict (26 failed verdicts), heals 500 ms after the
 *    first passed one (verdict 160): R at 150 meets D666 = 1 → 8 / 0x33,
 *    N at 180 → 6, D at 210 → 5, P → 8 ─────────────────────────────────── */
static void test_live_late_lever(void)
{
    static const uint8_t sch[7] = { 1u, 1u, 1u, 2u, 3u, 4u, 1u };   /* (none none) P R N D P */
    static shad_t sh[9u * SEG];
    static const uint8_t sch_b[9] = { 1u, 1u, 1u, 1u, 1u, 2u, 3u, 4u, 1u };
    const rng_chain_model_t *c = rng_chain_model();

    s_sim = k_sim_none;
    s_sim.brake = true;
    s_sim.engine = 700u;
    s_sim.g98 = 1u;
    s_sim.lever_from = 60u;                 /* first lever frame on chain pass 60 */
    run(sch, 7u, sh);
    CHECKV(sh[49].t109, 0);
    CHECKV(sh[50].t109, 1);                 /* D60C 51 > 50, D673 = 2: the timer starts */
    CHECKV(sh[58].t109, 81);
    CHECKV(sh[60].t109, 61);                /* the first passed verdict: − 20 ms */
    CHECKV(sh[66].t109, 1);
    CHECKV(sh[68].t109, 0);
    CHECKV(c->fm109.confirm_n, 0);
    CHECKV(c->fm109.heal_n, 1);             /* the timer reaching 0 (nothing to heal) */
    for (uint32_t t = 0u; t < 7u * SEG; t++) {
        CHECKV(sh[t].g109, 0);
        CHECKV(sh[t].d664, 0);
        CHECKV(sh[t].d666, 0);
        if (t >= 3u * SEG) {
            const uint8_t seg = sch[t / SEG];
            CHECKV(sh[t].d6, seg == 1u ? 8 : seg == 2u ? 7 : seg == 3u ? 6 : 5);
        }
    }

    s_sim.lever_from = 110u;
    run(sch_b, 9u, sh);
    CHECKV(sh[99].g109, 0);
    CHECKV(sh[100].g109, 1);                /* 1 + 25 x 20 >= 500 at the pass-100 verdict */
    CHECKV(sh[100].d664, 0);
    CHECKV(sh[101].d664, 1);
    CHECKV(sh[101].d666, 1);
    CHECKV(sh[101].d661, 1);
    CHECKV(sh[110].t109, 500);              /* the first passed verdict starts the heal timer */
    CHECKV(sh[158].t109, 20);
    CHECKV(sh[159].g109, 1);
    CHECKV(sh[160].g109, 0);                /* healed: 0xAAC0E drops group 109 (cfg+0x1C bit 8) */
    CHECKV(sh[160].d664, 1);
    CHECKV(sh[161].d664, 0);
    CHECKV(sh[161].d666, 0);
    CHECKV(sh[161].d661, 0);
    /* R arrives at 150 with D666 = 1: 0xC767C → 8 / 0x33 (one pass) */
    CHECKV(sh[150].de, 7);
    CHECKV(sh[150].d6, 8);
    CHECKV(sh[150].d9, 0x33);
    for (uint32_t t = 150u; t < 180u; t++) {
        CHECKV(sh[t].d6, 8);                /* no new lever change: R stays refused */
    }
    for (uint32_t t = 180u; t < 9u * SEG; t++) {
        const uint8_t seg = sch_b[t / SEG];
        CHECKV(sh[t].d6, seg == 1u ? 8 : seg == 3u ? 6 : 5);   /* healed: N 6, D 5, P 8 */
    }
    for (uint32_t t = 0u; t < 9u * SEG; t++) {
        CHECKV(outs(&sh[t]), sh[t].d6 == 8u ? 0 : 1);
        CHECKV(sh[t].mon, 0);
    }
    CHECKV(c->fm109.confirm_n, 1);
    CHECKV(c->fm109.heal_n, 1);
    CHECKV(c->fm109.rec & 0x0018u, 0);      /* record inactive, out of the masks */
    s_sim = k_sim_none;
}

/* ── 9. lever-frame gaps mid-test (brake + engine sims, lever from power-on:
 *    P, then R held). A gap of n ticks withholds the frames; the receive
 *    layer's 50 ms timeout then sets D673 = 2. ─────────────────────────── */
#define GAP_SEG 16u
static const uint8_t k_gap_sch[GAP_SEG] = { 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u,
                                            2u, 2u, 2u, 1u };
static shad_t s_gsh[GAP_SEG * SEG];

static void gap_run(uint32_t from, uint32_t len)
{
    s_sim = k_sim_none;
    s_sim.brake = true;
    s_sim.engine = 700u;
    s_sim.gap_from = from;
    s_sim.gap_len = len;
    run(k_gap_sch, GAP_SEG, s_gsh);
}

/* print-only: the 2 s gap with N or D held (the prediction table) */
static void gap_print_held(uint8_t code)
{
    static uint8_t sch[GAP_SEG];
    const rng_chain_model_t *c = rng_chain_model();

    for (uint32_t i = 0u; i < GAP_SEG; i++) {
        sch[i] = (uint8_t)(i < 4u ? 1u : (i == GAP_SEG - 1u ? 1u : code));
    }
    s_sim = k_sim_none;
    s_sim.brake = true;
    s_sim.engine = 700u;
    s_sim.gap_from = 140u;
    s_sim.gap_len = 200u;
    run(sch, GAP_SEG, s_gsh);
    printf("  gap 2 s, lever code %u held: confirm %u heal %u;", code,
           (unsigned)c->fm109.confirm_n, (unsigned)c->fm109.heal_n);
    for (uint32_t t = 121u; t < GAP_SEG * SEG; t++) {
        if (s_gsh[t].d6 != s_gsh[t - 1u].d6 || s_gsh[t].d9 != 0u) {
            printf(" t%u:%u/%02X", (unsigned)t, s_gsh[t].d6, s_gsh[t].d9);
        }
    }
    printf("\n");
    s_sim = k_sim_none;
}

static void test_gaps(void)
{
    const rng_chain_model_t *c = rng_chain_model();
    uint32_t fails_seen[8] = { 0 };

    /* short gaps (1..25 failed verdicts): never confirmed, the timer back
     * at 0 and the reaction bytes 0 throughout */
    for (uint32_t len = 1u; len <= 54u; len++) {
        gap_run(150u, len);
        CHECKV(c->fm109.confirm_n, 0);
        CHECKV(c->fm109.d714, 0);
        if (c->u_fail_n < 8u) {
            fails_seen[c->u_fail_n] = len;
        }
        for (uint32_t t = 0u; t < GAP_SEG * SEG; t++) {
            CHECKV(s_gsh[t].d664, 0);
            CHECKV(s_gsh[t].d666, 0);
        }
    }
    for (uint32_t k = 1u; k <= 7u; k++) {
        CHECK(fails_seen[k] != 0u);         /* every count 1..7 was exercised */
    }
    /* the ~70 ms gap (r03r +16.26 s): R released before it (7), and after */
    gap_run(150u, 7u);
    printf("  gap 70 ms: failed verdicts %u, timer max %u, D6D6 before %u, during/after:",
           (unsigned)c->u_fail_n, (unsigned)c->fm109.dd2c, s_gsh[149].d6);
    for (uint32_t t = 150u; t < 175u; t++) {
        /* '*' D673 = 2, 'L' D5FB = 1 */
        printf(" %u%s", s_gsh[t].d6, s_gsh[t].d673 == 2u ? "*" : (s_gsh[t].d5fb != 0u ? "L" : ""));
    }
    printf(" .. end %u (D5FB %u, reason 0x%02X)\n", s_gsh[GAP_SEG * SEG - 1u].d6,
           s_gsh[GAP_SEG * SEG - 1u].d5fb, s_gsh[GAP_SEG * SEG - 1u].d9);
    CHECKV(s_gsh[149].d6, 7);
    CHECKV(c->fm109.confirm_n, 0);
    /* 2 s without frames: confirmed after 26 failed verdicts, healed 500 ms
     * after the frames return */
    gap_run(140u, 200u);
    {
        uint32_t t_conf = 0u, t_heal = 0u;

        for (uint32_t t = 1u; t < GAP_SEG * SEG; t++) {
            if (s_gsh[t].g109 != 0u && s_gsh[t - 1u].g109 == 0u) {
                t_conf = t;
            }
            if (s_gsh[t].g109 == 0u && s_gsh[t - 1u].g109 != 0u) {
                t_heal = t;
            }
        }
        printf("  gap 2 s (ticks 140..339): first D673=2 tick %u, confirmed at %u, healed at %u;"
               " D6D6 at 339 %u, end %u (D5FB %u)\n",
               (unsigned)({ uint32_t f = 0u; for (uint32_t t = 140u; t < 340u; t++) { if (s_gsh[t].d673 == 2u) { f = t; break; } } f; }),
               (unsigned)t_conf, (unsigned)t_heal, s_gsh[339].d6,
               s_gsh[GAP_SEG * SEG - 1u].d6, s_gsh[GAP_SEG * SEG - 1u].d5fb);
        CHECK(t_conf != 0u && t_heal > t_conf);
        /* LIVE: the outputs follow the range through the gap (R 7 → U1267
         * 0xC76D8 6 / 0x6F: still out of P, released) and the final P;
         * park_mon has nothing to object to (no transition out of 8
         * without lever / brake / engine) */
        for (uint32_t t = 0u; t < GAP_SEG * SEG; t++) {
            CHECKV(outs(&s_gsh[t]), s_gsh[t].d6 == 8u ? 0 : 1);
            CHECKV(s_gsh[t].mon, 0);
        }
        CHECKV(s_gsh[GAP_SEG * SEG - 1u].d6, 8);
        CHECKV(c->fm109.confirm_n, 1);
        CHECKV(c->fm109.heal_n, 1);
        /* heal 26 verdicts (500 ms) after the first passed verdict */
        CHECK(t_heal >= 340u + 50u && t_heal <= 340u + 54u);
        for (uint32_t t = 120u; t < GAP_SEG * SEG; t++) {
            if (s_gsh[t].d6 != s_gsh[t - 1u].d6 || s_gsh[t].d5fb != s_gsh[t - 1u].d5fb ||
                s_gsh[t].d9 != 0u || s_gsh[t].g109 != s_gsh[t - 1u].g109) {
                printf("    t=%u lever-seg %u D673 %u D5FB %u g109 %u D6DE %u/%02X D6D6 %u reason 0x%02X\n",
                       (unsigned)t, k_gap_sch[t / SEG], s_gsh[t].d673, s_gsh[t].d5fb, s_gsh[t].g109,
                       s_gsh[t].de, s_gsh[t].df, s_gsh[t].d6, s_gsh[t].d9);
            }
        }
    }
    s_sim = k_sim_none;
}

/* ── 10. park_mon mutations in the loop: the chain's D6D6 overwritten
 *     after its pass (one pass), before park_tick ───────────────────────── */
static uint32_t s_mut_t;
static uint8_t s_mut_v, s_mut_c;

static void mut_once(uint32_t t)
{
    if (t == s_mut_t) {
        *(volatile uint8_t *)(uintptr_t)0xFFF8D6D6u = s_mut_v;
        *(volatile uint8_t *)(uintptr_t)0xFFF8D6D7u = s_mut_c;
    }
}

static void mut_case(const char *name, bool sims, uint32_t at, uint8_t v, uint8_t vc, uint8_t want)
{
    static const uint8_t sch[6] = { 1u, 2u, 1u, 2u, 3u, 1u };   /* P R P R N P */
    static shad_t sh[6u * SEG];
    unsigned legal_after = 0u;

    s_sim = k_sim_none;
    s_sim.brake = sims;
    s_sim.engine = sims ? 700u : 0u;
    s_sim.g98 = 1u;
    s_mut_t = at;
    s_mut_v = v;
    s_mut_c = vc;
    s_mut = mut_once;
    run(sch, 6u, sh);
    s_mut = NULL;
    for (uint32_t t = 0u; t < at; t++) {
        CHECKV(sh[t].mon, 0);
    }
    if (sh[at].mon != want) {
        printf("FAIL mutation %s: park_mon 0x%02X at the mutated pass, expected 0x%02X\n",
               name, sh[at].mon, want);
        fails++;
    }
    for (uint32_t t = at; t < 6u * SEG; t++) {
        if (want != 0u) {
            CHECKV(outs(&sh[t]), 0);        /* latched: both demands 0 for good */
        } else {
            CHECKV(sh[t].mon, 0);
            CHECKV(outs(&sh[t]), (t == at ? s_mut_v : sh[t].d6) == 8u ? 0 : 1);
        }
        legal_after += sh[t].d6 != 8u;      /* ... while the chain releases legally */
    }
    if (sims) {
        CHECK(legal_after > 0u);
    }
    s_sim = k_sim_none;
}

static void test_mon_mutations(void)
{
    /* the bench at rest (no sims): the chain says 8 in R; forced to 7 */
    mut_case("release without brake / engine", false, SEG + 10u, 7u, 0xF8u,
             MON_F_BRAKE | MON_F_ENGINE);
    /* sims on, lever P: forced to D */
    mut_case("D while the lever says P", true, 10u, 5u, 0xFAu, MON_F_LEVER);
    /* sims on, lever R, the chain at 7: forced to D — a move between R / N
     * / D is NOT checked (ours: only the exit from P changes the park
     * outputs; documented in park_mon.h) */
    mut_case("D while the lever says R (out of P already)", true, 3u * SEG + 10u, 5u, 0xFAu, 0u);
    /* sims on: out-of-range and torn values */
    mut_case("range 3", true, SEG + 10u, 3u, 0xFCu, MON_F_RANGE);
    mut_case("torn complement in P", true, 10u, 8u, 0x00u, MON_F_RANGE);
    mut_case("torn complement in R", true, SEG + 10u, 7u, 0x00u, MON_F_RANGE);
}

/* ── 11. the bench tool's park-check sequence (tools/bench/tcu_bench.py
 *     cmd_park_check, --settle 2.0), print-only: the predicted range and
 *     outputs per step, without and with the brake + engine sims. The TCU
 *     has been up 3 s without lever frames before the tool starts (U1267
 *     confirmed), as on the bench. ──────────────────────────────────────── */
static void park_check_print(bool sims, bool before_power)
{
    static const struct { const char *d; uint8_t code; bool a, c, bad; } st[13] = {
        { "no shifter frames", 1u, false, false, false }, { "P", 1u, true, true, false },
        { "R", 2u, true, true, false }, { "N", 3u, true, true, false }, { "D", 4u, true, true, false },
        { "code 0 after D", 0u, true, true, false }, { "P", 1u, true, true, false },
        { "0x2EA bad CRC, 0x210 R", 2u, true, true, true }, { "0x2EA missing, 0x210 N", 3u, false, true, false },
        { "0x2EA D, 0x210 missing", 4u, true, false, false }, { "invalid code 7", 7u, true, true, false },
        { "both missing again", 4u, false, false, false }, { "P", 1u, true, true, false },
    };
    uint8_t d[8];

    memset(s_ctr, 0, sizeof s_ctr);
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    CELL_TMP_BYTE = 67u;
    vehicle_init();
    park_init();
    rng_chain_init();
    printf("  park-check %s%s:", sims ? "WITH brake + engine sims" : "no sims",
           before_power ? ", frames from power-on" : ", TCU up 3 s before");
    for (int k = before_power ? 0 : -1; k < 13; k++) {
        const uint32_t n = k < 0 ? 300u : 200u;
        shad_t snap;

        for (uint32_t t = 0u; t < n; t++) {
            if (k >= 0 && (st[k].a || st[k].c)) {
                uint8_t l[5] = { st[k].code, 0, 0, 0, 0 };
                if (st[k].a) {
                    s_ctr[0x2EA] = (uint8_t)((s_ctr[0x2EA] + 1u) & 0x0Fu);
                    l[3] = (uint8_t)(s_ctr[0x2EA] << 4);
                    l[4] = (uint8_t)(crc8_j1850(l, 4u) ^ (st[k].bad ? 0x55u : 0u));
                    {
                        can_frame_t f = { .id = 0x2EAu, .dlc = 5u };
                        memcpy(f.data, l, 5u);
                        vehicle_rx(CAN_BUS1, &f);
                    }
                }
                if (st[k].c) {
                    const uint8_t m[5] = { st[k].code, 0, 0, 0, 0 };
                    send(CAN_BUS2, 0x210u, 5u, true, m);
                }
            }
            if (sims && (k >= 0 || before_power)) {
                memset(d, 0, sizeof d);
                d[0] = 0x04u;
                send(CAN_BUS1, 0x140u, 8u, true, d);
                memset(d, 0, sizeof d);
                d[0] = 0x02u;
                d[1] = 0xBCu;
                d[2] = 0x1Fu;
                d[3] = 0xFFu;
                send(CAN_BUS1, 0x108u, 8u, true, d);
            }
            vehicle_update_10ms();
            rng_chain_tick_10ms(1u, 1u, 1u);
            park_tick_10ms();
        }
        if (k < 0) {
            continue;
        }
        shad_snap(&snap);
        printf("\n    %-24s D6D6 %u/%02X out %d mon %02X lever %u", st[k].d, snap.d6, snap.d9,
               outs(&snap), snap.mon, park_status()->lever);
    }
    printf("\n");
}

int main(void)
{
    s_sim = k_sim_none;
    test_dump();
    test_start_r();
    test_positive_control();
    test_rest();
    test_mode_gate();
    test_live_release();
    test_high_rpm_lockout();
    test_live_nobrake();
    test_live_late_lever();
    test_gaps();
    test_mon_mutations();
    park_check_print(false, false);
    park_check_print(true, false);
    park_check_print(true, true);
    gap_print_held(3u);
    gap_print_held(4u);
    if (fails != 0) {
        printf("%d failure(s)\n", fails);
        return 1;
    }
    printf("rng_chain: all checks passed\n");
    return 0;
}

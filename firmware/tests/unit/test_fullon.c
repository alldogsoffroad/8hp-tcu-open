/* test_fullon.c — THE STOCK'S RAIL-OFF FULL-ON TESTS against our rail-open
 * guard and the 0x84644 witness (2026-11-17, ~/tcu_sweep/fullon_interlock_decode.md).
 *
 * The real supervisor (test_ladder.c's chip and switched-output plant, L3b:
 * every hold released — the r03w target configuration) runs together with
 * the real bus-A chain (drv_jobs / drv_reqlaw / drv_odiag) in main.c's
 * schedule: the 2.5 ms monitor (0 / 3 ms), the 5 ms batch at ms 2 (ladder →
 * seq2 → drv_letter_refresh → J2 stage), the 10 ms task (the periodic
 * cyclic) at ms 1 of every second frame — both parities, because which
 * frames the 10 ms task falls between is fixed per boot by the ladder start —
 * and the 1 ms diag poll. Bus A is NOT drained at once: one frame per 12 µs
 * (the r02y-measured CPU-fed frame time), so a refresh 1 ms after the
 * periodic finds the config job still in flight, as on the bench.
 *
 * What the stock does on this calibration [O, the bytes]: ONE full-on
 * command per ladder run — phase 0x10's entry tick (0x94114 = the alive
 * group, 0x8BD6A; cleared on the phase's pass, 0x8BDB2). Phase 4 writes
 * 0x94114 = group 0 (0x8BBE0), which is 0 here (0x1915A4[0] = 2 puts the one
 * enabled channel in group 2); the '3' diagnosis' 0x94115 is
 * calibration-dead (0x1915B8 bit 0 = 0, 0x1919C9 = 0). In that tick the
 * request cells are 0 and seq2's enable is 0, so that tick's J2 word is
 * 0x9000; the previous tick carried phase 8's refused 0x9020; the rail is
 * open.
 *
 * ASSERTED, both parities:
 *   - the stock full-on tick is COMMANDED by the law (no refusal):
 *     s_rq_ilock_n stays 0, CELL_RQ_FULLMASK = 0x00FF on exactly that tick;
 *   - every full-on cyclic that reaches the bus-A wire goes out while the
 *     plant's rail is OFF (and the chip has not certified a rail command);
 *   - 0x84644 (the witness of fatal 0x226) never counts; 0x8493A neither;
 *   - after 'U' the request steps (a park-release on/off, fills) never count.
 * Mutations: the rail node high, a letter, a request, or a rail word with
 * the chip certifying → the guard refuses, nothing full-on on the wire.
 */
#define main test_ladder_main
#include "test_ladder.c"
#undef main

#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_odiag.h"

/* ── the bench's four healthy driver chips (test_odiag.c's model) ──────── */
static uint8_t  f_reg8[5][256];
static uint16_t f_e8[5], f_f0[5];

static uint16_t drv_chip(uint16_t tx, uint8_t c)
{
    const uint8_t top5 = (uint8_t)(tx >> 11);
    const uint8_t cmd = (uint8_t)(tx >> 8);
    const uint8_t d = (uint8_t)tx;
    if (c >= 4u) {
        return 0x0000u;                         /* the empty fifth select */
    }
    switch (top5) {
    case 0x1Du: f_e8[c] = (uint16_t)(tx & 0x7FFu); return 0xA000u;
    case 0x1Eu: f_f0[c] = (uint16_t)(tx & 0x7FFu); return 0xA000u;
    case 0x18u: return (uint16_t)(0xA000u | f_e8[c]);
    case 0x1Bu: return (uint16_t)(0xA000u | f_f0[c]);
    default: break;
    }
    switch (cmd) {
    case 0x88u: case 0x8Eu: case 0x90u: case 0x96u: case 0xA0u: case 0xA6u:
        f_reg8[c][cmd] = d;
        return 0xA200u;
    case 0x28u: return (uint16_t)(0xA200u | 0x80u | f_reg8[c][0x88]);
    case 0x30u: return (uint16_t)(0xA200u | f_reg8[c][0x90]);
    case 0x2Eu: return (uint16_t)(0xA200u | f_reg8[c][0x8E]);
    case 0x36u: return (uint16_t)(0xA200u | f_reg8[c][0x96]);
    case 0x60u: return (uint16_t)(0xA200u | f_reg8[c][0xA0]);
    case 0x66u: return (uint16_t)(0xA200u | f_reg8[c][0xA6]);
    case 0x18u: case 0x1Eu: return 0xA200u;     /* diag bytes 00 (healthy) */
    case 0x06u: return 0xA212u;
    case 0x00u: return 0xA2BDu;
    default: return 0xA200u;
    }
}

/* ── bus B drained at once (test_ladder.c's drain, bus 1 only) ─────────── */
static void drain_b(void)
{
    unsigned guard = 512u;
    while (stub_rspi_busy(1u) && guard-- != 0u) {
        unsigned before = stub_log_n;
        stub_rspi_isr(1u);
        if (stub_log_n != before && stub_log[before].bus == 1u &&
            stub_log[before].job == JOB_DUTY &&
            (stub_log[before].word & 0xFF00u) == 0x8800u &&
            (before == 0u || stub_log[before - 1u].job != JOB_DUTY ||
             (stub_log[before - 1u].word & 0xFF00u) != 0x8800u)) {
            if (!chip.ignore_tokens && chip.level != 0u) {
                chip.level--;
            }
            chip.sel = (uint8_t)((chip.sel + 1u) & 0x0Fu);
        }
    }
}

/* ── bus A in time: one frame per 12 µs ────────────────────────────────── */
static bool s_a_was_busy;
static uint32_t s_a_due;
static void bus_a_step(void)
{
    const bool busy = stub_rspi_busy(0u);
    if (busy && !s_a_was_busy) {
        s_a_due = stub.frt_us + 12u;
    }
    s_a_was_busy = busy;
    if (busy && (int32_t)(stub.frt_us - s_a_due) >= 0) {
        stub_rspi_isr(0u);
        s_a_due += 12u;
        s_a_was_busy = stub_rspi_busy(0u);
        if (s_a_was_busy && !busy) {
            s_a_due = stub.frt_us + 12u;
        }
    }
}

/* ── the run ────────────────────────────────────────────────────────────── */
typedef struct {
    unsigned parity;            /* frames (p & 1) == parity get the 10 ms task */
    unsigned frames;
    void (*hook)(unsigned p);   /* before the frame (mutations) */
    void (*steps)(unsigned p);  /* after 'U': request steps */
    /* results */
    unsigned fullon_law_ticks;  /* refresh passes with CELL_RQ_FULLMASK != 0 */
    unsigned fullon_frames[8];  /* … which frames */
    unsigned fullon_wire;       /* full-on cyclics launched on bus A */
    unsigned fullon_wire_rail;  /* … while the plant's rail was ON */
    unsigned fullon_wire_cert;  /* … after a heard 0x90|bit5 with the chip certifying */
    unsigned phase4_frame, phase10_frame;   /* the entry ticks (0x94114 set after the batch) */
    unsigned u_frame;
    uint32_t ilock, f84644, f8493a, f226inline;
    uint32_t ilock_at104;       /* refusals counted before frame 105 */
    unsigned hi_cfg;            /* config words 0x9002 (the 0x90232 high-current pass) */
} fo_run_t;

static bool s_rail_on;          /* the plant's output this frame (its 5 ms model) */

static void fo_reset(void)
{
    reset_all();
    plant.enabled = true;
    plant.lag = 0u;
    plant.never_on = false;
    plant.ignore_level = false;
    memset(f_reg8, 0, sizeof f_reg8);
    stub.drv_reply_fn = drv_chip;
    drv_od_host_seed_select(false);
    drv_jobs_init();
    /* the r03w requests: the gear-1 P set (ch 0..6), ch 7 (park release) 0 = Auto-P */
    {
        static const uint16_t P[10] = { 8500u, 8500u, 500u, 8500u, 8500u, 500u, 500u, 0u, 0u, 0u };
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            drv_rq_request_set(ch, P[ch]);
        }
    }
    s_a_was_busy = false;
    stub_log_n = 0u;
}

/* a bus-A cyclic word block starting at index k in the log is full-on when
 * its mode row carries 'C' (0x881F on a chip slot) */
static void scan_bus_a(fo_run_t *r, unsigned from)
{
    for (unsigned k = from; k < stub_log_n; k++) {
        if (stub_log[k].bus == 0u && stub_log[k].job == DRV_CFG && stub_log[k].word == 0x9002u) {
            r->hi_cfg++;
        }
        if (stub_log[k].bus == 0u && stub_log[k].job == DRV_CYC &&
            (stub_log[k].word & 0xFF0Fu) == 0x880Fu && (stub_log[k].word & 0x00F0u) == 0x0010u &&
            (k == 0u || stub_log[k - 1u].job != DRV_CYC || stub_log[k - 1u].bus != 0u ||
             stub_log[k - 1u].word != stub_log[k].word)) {
            /* first 'C' mode word of a cyclic (0x881F: both channels 'C') */
            r->fullon_wire++;
            if (s_rail_on) {
                r->fullon_wire_rail++;
            }
            if ((chip.w90 & 0x20u) != 0u && chip.level <= 4u) {
                r->fullon_wire_cert++;
            }
        }
    }
}

static void fo_run(fo_run_t *r)
{
    const uint32_t ilock0 = drv_rq_ilock_n();
    const drv_od_fatal_t *f = drv_od_fatal();
    const uint32_t s84644 = f->site_n[6], s0 = f->site_n[0] + f->site_n[1] + f->site_n[2];
    uint32_t s8493a = 0u;
    for (unsigned i = 8u; i < 12u; i++) {
        s8493a += f->site_n[i];
    }

    for (unsigned p = 1u; p <= r->frames; p++) {
        if (p == 100u) {
            stub.adc_ready = true;
        }
        if (p == 105u) {
            r->ilock_at104 = drv_rq_ilock_n() - ilock0;
        }
        if (r->hook != NULL) {
            r->hook(p);
        }
        inputs();
        s_rail_on = CELL_MUX_MV[0] > 4000u;
        for (unsigned step = 0u; step < 500u; step++) {
            const unsigned us = step * 10u;
            const unsigned log0 = stub_log_n;
            if (us % 1000u == 0u) {
                drv_od_diag_poll(stub.frt_us);  /* task_1ms */
            }
            if (us == 0u || us == 3000u) {
                sup_monitor_poll();
                drain_b();
            }
            if (us == 1000u && (p & 1u) == r->parity) {
                drv_cyclic_periodic();          /* task_10ms → sol_tick_10ms */
            }
            if (us == 1500u && r->steps != NULL && r->u_frame != 0u) {
                /* a request step AFTER the periodic, so only the refresh can
                 * take it (on the target the producer and the park logic
                 * run in task_10ms just before the periodic — this is the
                 * harder case: the refresh 0.5 ms later finds bus A busy) */
                r->steps(p - r->u_frame);
            }
            if (us == 2000u) {
                sup_batch_5ms();                /* … drv_letter_refresh after seq2 */
                drain_b();
                if (CELL_RQ_FULLMASK != 0u) {
                    if (r->fullon_law_ticks < 8u) {
                        r->fullon_frames[r->fullon_law_ticks] = p;
                    }
                    r->fullon_law_ticks++;
                }
                if (CELL_LAD_PHASE == 0x0004u && r->phase4_frame == 0u) {
                    r->phase4_frame = p;
                }
                if (CELL_LAD_94114 != 0u && CELL_LAD_PHASE == 0x0010u && r->phase10_frame == 0u) {
                    r->phase10_frame = p;
                }
                if (CELL_SEQ2_STATE[0] == (uint8_t)'U' && r->u_frame == 0u) {
                    r->u_frame = p;
                }
            }
            if (plat_walktmr_running() &&
                (int32_t)(stub.frt_us - stub.walk_fire_us) >= 0) {
                plat_walktmr_stop();
                sup_walk_isr();
                drain_b();
            }
            sup_mode_background();
            bus_a_step();
            scan_bus_a(r, log0);
            stub.frt_us += 10u;
        }
        stub_log_n = 0u;
    }
    r->ilock = drv_rq_ilock_n() - ilock0;
    r->f84644 = f->site_n[6] - s84644;
    r->f226inline = f->site_n[0] + f->site_n[1] + f->site_n[2] - s0;
    {
        uint32_t n = 0u;
        for (unsigned i = 8u; i < 12u; i++) {
            n += f->site_n[i];
        }
        r->f8493a = n - s8493a;
    }
}

static void report(const char *name, const fo_run_t *r)
{
    printf("%s parity %u: phase4 F%u phase10 F%u 'U' F%u | law full-on ticks %u (F%u F%u F%u) | "
           "wire full-on cyclics %u (rail on %u, certified %u) | ilock %u 84644 %u 8493A %u inline %u\n",
           name, r->parity, r->phase4_frame, r->phase10_frame, r->u_frame, r->fullon_law_ticks,
           r->fullon_frames[0], r->fullon_frames[1], r->fullon_frames[2], r->fullon_wire,
           r->fullon_wire_rail, r->fullon_wire_cert, r->ilock, r->f84644, r->f8493a, r->f226inline);
}

/* ── 1. the stock sequence, both parities ──────────────────────────────── */
static void park_steps(unsigned k)
{
    /* after 'U': the park release on (10000) for 300 ms, off, again — the
     * request steps a lever R/N/D → P does on the bench; and a 500 → 8500
     * step on B (ch 2) as a fill end */
    const uint16_t rel = ((k >= 40u && k < 100u) || (k >= 160u && k < 220u)) ? 10000u : 0u;
    drv_rq_request_set(7u, rel);
    drv_rq_request_set(2u, (k >= 120u && k < 180u) ? 8500u : 500u);
}

static void test_stock_sequence(void)
{
    for (unsigned par = 0u; par < 2u; par++) {
        fo_run_t r;
        memset(&r, 0, sizeof r);
        r.parity = par;
        r.frames = 600u;
        r.steps = park_steps;
        fo_reset();
        fo_run(&r);
        report("stock", &r);
        CHECK(r.phase4_frame != 0u && r.phase10_frame == r.phase4_frame + 2u);
        CHECK(r.u_frame != 0u);
        /* the stock's one full-on tick (phase 0x10's entry) commanded, no other:
         * phase 4's 0x94114 is group 0 = 0 on this calibration */
        CHECK(r.fullon_law_ticks == 1u);
        CHECK(r.fullon_frames[0] == r.phase10_frame);
        CHECK(r.ilock == 0u);
        CHECK(drv_rq_cguard_n() == 0u);
        /* nothing full-on on the wire with the rail on or a certified rail command */
        CHECK(r.fullon_wire_rail == 0u && r.fullon_wire_cert == 0u);
        /* the witness counts nothing — the full-on steps, 'U', the release steps */
        CHECK(r.f84644 == 0u && r.f8493a == 0u && r.f226inline == 0u);
        CHECK(CELL_SEQ2_STATE[0] == (uint8_t)'U' && CELL_J2_STAGED == 0x9020u);
        /* the compose-cells step keeps the 0x901BC head check and the
         * 0x90232 high-current pass: no 0x238 count, and each of the two
         * park-release turn-ons puts chip 3's 0x90 byte 0x02 on the wire */
        CHECK(drv_rq_f238_n() == 0u);
        printf("  high-current config words 0x9002: %u\n", r.hi_cfg);
        CHECK(r.hi_cfg == 2u);
    }
}

/* ── 2. mutations: the rail-open guard refuses ─────────────────────────── */
/* In the stock sequence the ladder's own checks stand in front of the test
 * tick: a rail that is ON fails phase 4 / phase 8 (class 0x22) before
 * phase 0x10 is ever entered — so the node term cannot be reached by a
 * whole-ladder run (test_guard_unit covers it). The case the sequence does
 * NOT catch: a chip that CERTIFIES while it hears phase 8's 0x9020 (walk
 * level <= 4) with an output that lags one tick — phase 8 still passes on
 * the old node reading, phase 0x10 is entered, and the rail closes on the
 * next tick. Term 4 (last word 0x9020 AND 0x90959 = 1) refuses it. */
static unsigned s_mut;
static void mut_hook(unsigned p)
{
    if (s_mut == 1u && (p == 101u || p == 102u)) {
        chip.level = 3u;                        /* certifying through phase 8 */
    }
}
static void test_guard_unit(void)
{
    /* the law alone, phase-0x10 state: seq2 0x11, 0x94114 = 1, open rail */
    fo_reset();
    CELL_SEQ2_STATE[0] = 0x11u;
    CELL_LAD_94114 = 1u;
    CELL_J2_STAGED = 0x9020u;           /* the PREVIOUS tick's word (phase 8) … */
    CELL_SSLB0_BIT2 = 0u;               /* … at a non-certifying chip */
    CELL_OUT_MV[0] = 2939u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_EN_MIRROR[0] = 0u;
    CELL_LETTERS[0] = 0u;
    {
        const uint32_t n0 = drv_rq_ilock_n();
        drv_rq_law();
        CHECK(CELL_RQ_FULLMASK == 0x00FFu && drv_rq_ilock_n() == n0);
    }
    /* each single closed-rail condition refuses, on all 8 driven channels */
    for (unsigned m = 0u; m < 5u; m++) {
        const uint32_t n0 = drv_rq_ilock_n();
        switch (m) {
        case 0u: CELL_OUT_MV[0] = 4001u; break;          /* node above the off band */
        case 1u: CELL_LAD_REQ[0] = 1u; break;            /* a request this tick */
        case 2u: CELL_EN_MIRROR[0] = 1u; break;          /* seq2's enable this tick */
        case 3u: CELL_LETTERS[0] = (uint8_t)'c'; break;  /* a letter (bit 5 composed) */
        default: CELL_SSLB0_BIT2 = 1u; break;            /* rail word sent + chip certifying */
        }
        drv_rq_law();
        CHECK(CELL_RQ_FULLMASK == 0u && drv_rq_ilock_n() == n0 + 8u);
        for (uint8_t ch = 0u; ch < 8u; ch++) {
            CHECK(CELL_RQ_LETTER[ch] != (uint8_t)'C');
        }
        CELL_OUT_MV[0] = 2939u; CELL_LAD_REQ[0] = 0u; CELL_EN_MIRROR[0] = 0u;
        CELL_LETTERS[0] = 0u; CELL_SSLB0_BIT2 = 0u;
    }
    /* the previous word 0x9000 needs no gate reading */
    CELL_J2_STAGED = 0x9000u;
    CELL_SSLB0_BIT2 = 1u;
    drv_rq_law();
    CHECK(CELL_RQ_FULLMASK == 0x00FFu);
    /* the compose-time guard: a 'C' left by a law pass is not staged once the
     * node reads the rail on (law not re-run) */
    CELL_OUT_MV[0] = 13300u;
    {
        const uint32_t g0 = drv_rq_cguard_n();
        drv_rq_compose();
        CHECK(drv_rq_cguard_n() == g0 + 8u);
        for (unsigned c = 0u; c < 4u; c++) {
            CHECK((CELL_REC_U8(c, REC_MODE) & 0x0Fu) == 0x05u);  /* '2' '2', not 'C' */
        }
    }
    CELL_LAD_94114 = 0u;
}

static void test_mutation_runs(void)
{
    for (unsigned par = 0u; par < 2u; par++) {
        fo_run_t r;
        memset(&r, 0, sizeof r);
        r.parity = par;
        r.frames = 140u;
        r.hook = mut_hook;
        s_mut = 1u;
        fo_reset();
        plant.lag = 1u;
        fo_run(&r);
        report("certifying-chip", &r);
        CHECK(r.phase10_frame == 102u);         /* the sequence did not catch it */
        /* the guard did: refused at F102 (term 4) and at F103 — the stock
         * itself would command full-on at F103 with the rail found ON
         * (0x94114 survives the 0xF3 failure until the retry's phase 1) —
         * then the retry's phase 0x10 (F106, chip no longer certifying,
         * rail open) is the stock's test again and is commanded */
        CHECK(r.ilock >= 16u && r.ilock == r.ilock_at104);   /* F102..F104 only (F104: the periodic before the retry) */
        CHECK(r.fullon_law_ticks == 1u && r.fullon_frames[0] == 106u);
        CHECK(r.fullon_wire_rail == 0u && r.fullon_wire_cert == 0u);
        CHECK(r.f84644 == 0u && r.f8493a == 0u);
        s_mut = 0u;
        plant.lag = 0u;
    }
}

int main(void)
{
    test_guard_unit();
    test_stock_sequence();
    test_mutation_runs();
    if (fails == 0) {
        printf("test_fullon: ok\n");
    }
    return fails != 0;
}

/* test_ladder.c — L1: the stock self-test ladder (sup_ladder.c), HELD.
 *
 * Drives the firmware's 5 ms batch (which now carries the countdown, the
 * publishers, the ladder, seq2 and the pin cut in the stock list order) and
 * the 2.5 ms monitor on main.c's phases, on a 10 µs step, against a chip
 * model, with the r02q bench inputs:
 *   output node (mux 8)  2942 mV      supply (mux 11)  13 366 mV
 *   mux 13 13 366 / min 13 347       mux 10 24 916 mV
 * The ladder's start gate (plat_adcmux_ready) opens at 500 ms, as on the
 * bench (the deferred Timer G start + the ready margin).
 *
 * Chip model [I, from transcript.c and gate_bit1_decode.md]: the walk level
 * rises by one on a repeat 84 write (cap 7) and falls by one on a W1 token
 * (floor 0, the selector rotates); the 0x14 reply is C5 | bit 1 | bit 4 with
 *   bit 1 = (level <= 4) AND the TIOC clock present,
 *   bit 4 = the TIOC clock absent (the pins on GPIO: plat_tioc_cut).
 * Variants break one law each to drive the ladder's fault paths.
 *
 * ASSERTED IN EVERY SCENARIO, EVERY 5 ms FRAME: no request cell nonzero, no
 * letter, no 0x90 enables bit, J2 TX staged 0x9000 and every J2/F word on
 * the wire 0x9000; the walk FSM never leaves 8 (the 0x87766 hold).
 */
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_ladder.h"
#include "stub_plat.h"

extern struct { uint8_t bus; uint8_t job; uint16_t word; uint32_t t_us; } stub_log[32768];
extern unsigned stub_log_n;

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* ── the chip model ─────────────────────────────────────────────────────── */
static struct {
    uint8_t level, sel, echo;
    bool ignore_cut;            /* bit 1 does not drop with the clock cut */
    bool ignore_tokens;         /* tokens do not lower the level */
} chip;

static uint8_t reg14(void)
{
    const bool clock = !stub.tioc_cut || chip.ignore_cut;
    uint8_t r = 0xC5u;

    if (chip.level <= 4u && clock) {
        r |= 0x02u;
    }
    if (!clock) {
        r |= 0x10u;
    }
    return r;
}

static uint16_t chip_reply(uint16_t tx)
{
    switch (tx & 0xFF00u) {
    case 0x8400u:
        if ((uint8_t)tx == chip.echo && chip.level < 7u) {
            chip.level++;
        }
        chip.echo = (uint8_t)tx;
        return 0xAA00u;
    case 0x8800u: return 0xAA00u;
    case 0x0A00u: return (uint16_t)(0xAA00u | chip.echo);
    case 0x0C00u: return (uint16_t)(0xAA80u | (uint16_t)(chip.level << 4) | chip.sel);
    case 0x1200u: return 0xAA00u;
    case 0x1400u: return (uint16_t)(0xAA00u | reg14());
    case 0x2200u: return (uint16_t)(0xAA00u | (CELL_REQUEST & 0x01u));
    case 0x0600u: return 0xAA14u;
    default:      return 0xAA00u;
    }
}

static void drain(void)
{
    for (uint8_t b = 0u; b < 2u; b++) {
        unsigned guard = 512u;
        while (stub_rspi_busy(b) && guard-- != 0u) {
            unsigned before = stub_log_n;
            stub_rspi_isr(b);
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
}

/* ── the bench inputs (what plat_adcmux's finisher publishes) ──────────── */
static uint16_t in_out_mv = 2942u, in_supply = 13366u;

static void inputs(void)
{
    CELL_MUX_MV[0] = in_out_mv;
    CELL_MUX_MV[1] = 24916u;
    CELL_MUX11_MV = in_supply;
    CELL_MUX13_MV = in_supply;          /* a real supply move moves mux 13 too —
                                         * 0x8D8DE substitutes mux 13 + 200 mV
                                         * when mux 11 alone disagrees */
    CELL_MUX13_MIN = (uint16_t)(in_supply - 19u);
    CELL_MUX13_ENV = 40u;
}

/* ── the run ────────────────────────────────────────────────────────────── */
typedef struct {
    unsigned frames;            /* 5 ms frames run */
    unsigned start_frame;       /* first frame with module 11 != 0 */
    unsigned hold_frame;        /* first frame parked at phase 0x40 */
    unsigned cut_frame;         /* frame of the cut */
    unsigned drop_frame;        /* first frame bit 1 read 0 after the cut */
    bool saw_d5;                /* 0x14 read with bit 4 set (clock absent) */
    unsigned bad_wire;          /* J2/F words other than 0x9000 */
    unsigned bad_wire_frames;   /* frames carrying one */
    unsigned first_bad_frame;
    unsigned bad_cells;         /* frames with a request / letter / enable bit */
    unsigned fsm_left_8;        /* frames with the outer FSM != 8 */
    unsigned retries_seen;      /* retry restarts (phase back to 1, sub 0) */
    bool saw_200;               /* the supply window parked the ladder */
} run_t;

static void reset_all(void)
{
    sup_cells_init();
    plat_walktmr_stop();
    stub.tioc_cut = false;
    stub.tioc_cuts = 0u;
    stub.adc_ready = false;
    stub_log_n = 0u;
    chip.level = 7u;
    chip.sel = 0u;
    chip.echo = 0u;
    inputs();
    sup_jobs_init();
    sup_walk_init();
    sup_batch_init();
    stub.sup_reply_fn = chip_reply;
    sup_job_engine_pump();
    drain();
}

static void run(run_t *r, unsigned frames, void (*hook)(unsigned frame))
{
    memset(r, 0, sizeof *r);
    for (unsigned p = 1u; p <= frames; p++) {
        const uint16_t ph0 = CELL_LAD_PHASE;
        const uint8_t sub0 = CELL_LAD_SUB;

        if (p == 100u) {
            stub.adc_ready = true;      /* 500 ms: the deferred chain is valid */
        }
        if (hook != NULL) {
            hook(p);
        }
        inputs();
        for (unsigned step = 0u; step < 500u; step++) {
            const unsigned us = step * 10u;
            if (us == 0u || us == 3000u) {
                sup_monitor_poll();
                drain();
            }
            if (us == 2000u) {
                const unsigned cuts = stub.tioc_cuts;
                sup_batch_5ms();
                drain();
                if (stub.tioc_cuts != cuts && r->cut_frame == 0u) {
                    r->cut_frame = p;
                }
            }
            if (plat_walktmr_running() &&
                (int32_t)(stub.frt_us - stub.walk_fire_us) >= 0) {
                plat_walktmr_stop();
                sup_walk_isr();
                drain();
            }
            stub.frt_us += 10u;
        }
        /* the per-frame invariants */
        {
            const unsigned before = r->bad_wire;
            for (unsigned j = 0u; j < stub_log_n; j++) {
                if (stub_log[j].bus == 1u &&
                    (stub_log[j].job == JOB_EN || stub_log[j].job == JOB_FLT) &&
                    (stub_log[j].word & 0xFF00u) == 0x9000u && stub_log[j].word != 0x9000u) {
                    r->bad_wire++;
                }
            }
            if (r->bad_wire != before) {
                r->bad_wire_frames++;
                if (r->first_bad_frame == 0u) {
                    r->first_bad_frame = p;
                }
            }
        }
        stub_log_n = 0u;
        if (CELL_LAD_REQ[0] != 0u || CELL_LAD_REQ[1] != 0u || CELL_LETTERS[0] != 0u ||
            CELL_LETTERS[1] != 0u || CELL_LETTERS[2] != 0u ||
            (CELL_EN_90 & 0xE0u) != 0u || CELL_J2_STAGED != 0x9000u ||
            CELL_EN_MIRROR[0] != 0u) {
            r->bad_cells++;
        }
        if (CELL_OUTER_FSM != SUP_FSM_SUPERVISE && p > 2u) {
            r->fsm_left_8++;
        }
        if (CELL_MOD11 != 0u && r->start_frame == 0u) {
            r->start_frame = p;
        }
        if (CELL_LAD_PHASE == 0x40u && r->hold_frame == 0u) {
            r->hold_frame = p;
        }
        if (CELL_LAD_PHASE == 0x200u) {
            r->saw_200 = true;
        }
        if ((CELL_REG_14 & 0x10u) != 0u) {
            r->saw_d5 = true;
        }
        if (r->cut_frame != 0u && r->drop_frame == 0u && CELL_SSLB0_BIT2 == 0u) {
            r->drop_frame = p;
        }
        if (CELL_LAD_PHASE == 1u && CELL_LAD_SUB == 0x31u && !(ph0 == 1u && sub0 == 0x31u) &&
            r->start_frame != 0u && p > r->start_frame + 1u) {
            r->retries_seen++;
        }
        r->frames = p;
    }
}

static void common_invariants(const run_t *r, const char *name)
{
    if (r->bad_wire != 0u || r->bad_cells != 0u || r->fsm_left_8 != 0u) {
        printf("FAIL %s: wire %u, cells %u, fsm-left-8 %u\n", name, r->bad_wire,
               r->bad_cells, r->fsm_left_8);
        fails++;
    }
}

/* ── scenarios ─────────────────────────────────────────────────────────── */

static unsigned s_dip_from, s_dip_to;
static void dip_hook(unsigned p)
{
    in_supply = (p >= s_dip_from && p < s_dip_to) ? 5000u : 13366u;
}

static void scen_nominal(void)
{
    run_t r;

    reset_all();
    run(&r, 300u, NULL);                         /* 1.5 s */
    common_invariants(&r, "nominal");
    printf("NOMINAL: start F%u, cut F%u, bit 1 drop F%u, hold F%u (%u ms after start)\n",
           r.start_frame, r.cut_frame, r.drop_frame, r.hold_frame,
           (r.hold_frame - r.start_frame) * 5u);
    printf("NOMINAL at the hold: phase %04X sub %02X pass %04X fault %04X ok %04X "
           "class %02X/%02X status %02X seq2 %02X req %02X%02X retries %u target %u "
           "level %u 0x14 %02X out %u mV supply %u mV st %u mod11 %02X mod7 %02X "
           "cut %02X/%02X cnt %u/%u/%u\n",
           CELL_LAD_PHASE, CELL_LAD_SUB, CELL_LAD_PASS, CELL_LAD_FAULT, CELL_LAD_OKFLAGS,
           CELL_OUT_CLASS[0], CELL_OUT_CLASS[1], CELL_SEQ2_STATUS[0], CELL_SEQ2_STATE[0],
           CELL_LAD_REQ[0], CELL_LAD_REQ[1], CELL_LAD_RETRIES, CELL_TARGET,
           CELL_LVL_REPORTED, CELL_REG_14, CELL_OUT_MV[0], CELL_SUPPLY, CELL_SUPPLY_ST,
           CELL_MOD11, CELL_MOD7, CELL_CUT_MODE, CELL_CUT_REQ, CELL_CNT_BIT1,
           CELL_CNT_LADDER, CELL_CNT_OUTTOL);
    /* the recipe's predicted state at the hold (ladder_blockers_decode §5.5) */
    CHECK(r.start_frame == 100u);
    CHECK(r.hold_frame != 0u);
    CHECK(CELL_LAD_PHASE == 0x0040u && CELL_LAD_PHASE_C == 0xFFBFu);
    CHECK(CELL_LAD_SUB == 0x55u && CELL_LAD_SUB_C == 0xAAu);
    CHECK(CELL_LAD_PASS == 0x003Fu);
    CHECK(CELL_LAD_FAULT == 0u && CELL_LAD_OKFLAGS == 0u);
    CHECK(CELL_LAD_RETRIES == 3u);
    CHECK(CELL_GRP(2u) == 1u && CELL_GRP(3u) == 1u);
    CHECK(CELL_LAD_MISMATCH[0] == 0u && CELL_LAD_MISMATCH[1] == 0x33u);
    CHECK(CELL_OUT_CLASS[0] == 0x22u && CELL_OUT_CLASS[1] == 0x33u);
    CHECK(CELL_OUT_MV[0] == 2942u && CELL_SUPPLY == 13366u && CELL_SUPPLY_ST == 8u);
    CHECK(CELL_SEQ2_STATE[0] == 0x11u && CELL_SEQ2_STATE[1] == 0x22u);
    CHECK(CELL_SEQ2_DIAG[0] == 1u && CELL_SEQ2_STATUS[0] == 1u);
    CHECK(CELL_CUT_MODE == 0x11u && CELL_CUT_REQ == 0u);
    CHECK(stub.tioc_cuts == 1u && !stub.tioc_cut && CELL_TIOC_ON == 1u);
    CHECK(r.saw_d5);                             /* the chip saw the clock cut */
    CHECK(r.drop_frame != 0u && r.drop_frame - r.cut_frame <= 3u);   /* within 15 ms */
    CHECK(CELL_TARGET == 3u && CELL_LVL_REPORTED <= 4u);
    CHECK(CELL_REG_14 == 0xC7u && CELL_SSLB0_BIT2 == 1u);   /* back to C7 */
    CHECK(CELL_MOD11 == 1u && CELL_MOD7 == 0u);
    CHECK(sup_ladder_fsm_req_held() == 0u);
    CHECK(CELL_CLEAR_LATCH == 0u);
    /* parked is frozen whole: a supply dip after the hold does not move it */
    {
        run_t r2;
        s_dip_from = 1u;
        s_dip_to = 6u;
        run(&r2, 20u, dip_hook);
        common_invariants(&r2, "parked-dip");
        CHECK(!r2.saw_200 && CELL_LAD_PHASE == 0x40u && CELL_LAD_SUB == 0x55u);
        CHECK(stub.tioc_cuts == 1u);
    }
    /* the hold persists: nothing in the ladder runs again */
    {
        const uint8_t cnt = CELL_CNT_LADDER;
        CHECK(cnt == 0u);                        /* 200 ms long expired … */
        CHECK(CELL_LAD_SUB == 0x55u);            /* … and no 0xF6 timeout */
    }
}

/* phase 4 sees the output at supply (class 0x66): F2, three retries, then
 * the exhaustion path — groups 0x22, strip latch, module 11 = 0x0F, and the
 * held 0x87766(0x23) */
static void scen_output_on(void)
{
    run_t r;

    reset_all();
    in_out_mv = 13300u;
    run(&r, 300u, NULL);
    in_out_mv = 2942u;
    common_invariants(&r, "output-on");
    printf("OUTPUT-ON: seq %02X sub %02X fault %04X ok %04X failcls %02X retries %u "
           "seq2 %02X mod11 %02X mod7 %02X latch %u held %02X\n",
           CELL_SEQ_STATE, CELL_LAD_SUB, CELL_LAD_FAULT, CELL_LAD_OKFLAGS,
           CELL_LAD_FAILCLS[0], CELL_LAD_RETRIES, CELL_SEQ2_STATE[0], CELL_MOD11,
           CELL_MOD7, CELL_CLEAR_LATCH, sup_ladder_fsm_req_held());
    CHECK(CELL_SEQ_STATE == 0x1Fu);
    CHECK(CELL_LAD_SUB == 0xF2u && CELL_LAD_RETRIES == 0u);
    CHECK(CELL_LAD_FAILCLS[0] == 0x66u);
    CHECK(CELL_SEQ2_STATE[0] == 0x22u && CELL_CUT_MODE == 0x22u);
    CHECK(CELL_MOD11 == 0x0Fu && CELL_MOD7 == 0x0Fu);
    CHECK(CELL_CLEAR_LATCH == 1u);
    CHECK(sup_ladder_fsm_req_held() == 0x23u);
    CHECK(stub.tioc_cuts == 0u);
}

/* the chip keeps bit 1 through the cut: phase 0x20 '3' → 0xF4 after the
 * 15 ms tolerance, retries (each a full re-run with a cut), exhaustion */
static void scen_no_drop(void)
{
    run_t r;

    reset_all();
    chip.ignore_cut = true;
    run(&r, 400u, NULL);
    chip.ignore_cut = false;
    common_invariants(&r, "no-drop");
    printf("NO-DROP: seq %02X sub %02X fault %04X retries %u cuts %u held %02X\n",
           CELL_SEQ_STATE, CELL_LAD_SUB, CELL_LAD_FAULT, CELL_LAD_RETRIES,
           stub.tioc_cuts, sup_ladder_fsm_req_held());
    CHECK(CELL_SEQ_STATE == 0x1Fu && CELL_LAD_SUB == 0xF4u);
    CHECK(stub.tioc_cuts == 4u && !stub.tioc_cut);   /* 1 + 3 retries, all restored */
    CHECK(sup_ladder_fsm_req_held() == 0x23u);
    CHECK(CELL_CLEAR_LATCH == 1u);
}

/* the level never comes down to 3: phase 0x20's budget runs out → 0xF1;
 * exhaustion requests 0x12 (held) */
static void scen_budget(void)
{
    run_t r;

    reset_all();
    chip.ignore_tokens = true;
    run(&r, 400u, NULL);
    chip.ignore_tokens = false;
    common_invariants(&r, "budget");
    printf("BUDGET: seq %02X sub %02X fault %04X retries %u held %02X\n",
           CELL_SEQ_STATE, CELL_LAD_SUB, CELL_LAD_FAULT, CELL_LAD_RETRIES,
           sup_ladder_fsm_req_held());
    CHECK(CELL_SEQ_STATE == 0x1Fu && CELL_LAD_SUB == 0xF1u);
    CHECK(sup_ladder_fsm_req_held() == 0x12u);
    CHECK(stub.tioc_cuts == 0u);
}

/* the supply leaves the window mid-ladder and comes back: phase 0x200
 * saves and resumes; then a supply that stays out: sub 0x40 abort, groups
 * 0x16, module 11 = 0x0F (no retry); then mux 11 ALONE drops (a bad
 * reading): 0x8D8DE substitutes mux 13 + 200 mV after its 5-count
 * countdown and the ladder completes */

static void scen_supply(void)
{
    run_t r;

    reset_all();
    s_dip_from = 102u;
    s_dip_to = 106u;
    run(&r, 300u, dip_hook);
    common_invariants(&r, "supply-dip");
    printf("SUPPLY-DIP: phase %04X sub %02X pass %04X seq %02X\n", CELL_LAD_PHASE,
           CELL_LAD_SUB, CELL_LAD_PASS, CELL_SEQ_STATE);
    CHECK(r.saw_200);                            /* parked at 0x200 … */
    CHECK(CELL_LAD_PHASE == 0x40u && CELL_LAD_SUB == 0x55u && CELL_LAD_PASS == 0x3Fu);  /* … resumed */

    reset_all();
    s_dip_from = 101u;
    s_dip_to = 100000u;
    run(&r, 200u, dip_hook);
    common_invariants(&r, "supply-out");
    printf("SUPPLY-OUT: phase %04X sub %02X seq2 %02X mod11 %02X mod7 %02X suplow %02X\n",
           CELL_LAD_PHASE, CELL_LAD_SUB, CELL_SEQ2_STATE[0], CELL_MOD11, CELL_MOD7,
           CELL_SEQ2_SUPLOW);
    CHECK(CELL_LAD_PHASE == 0x200u && CELL_LAD_SUB == 0x40u);
    CHECK(CELL_SEQ2_STATE[0] == 0x16u && CELL_MOD11 == 0x0Fu && CELL_MOD7 == 0x0Fu);
    CHECK(CELL_SEQ2_SUPLOW == 0x01u);
    in_supply = 13366u;

    reset_all();
    run(&r, 101u, NULL);
    for (unsigned i = 0u; i < 60u; i++) {        /* mux 11 alone at 5 V */
        CELL_MUX11_MV = 5000u;
        sup_batch_5ms();
        drain();
        if (i == 59u) {
            printf("MUX11-ALONE: supply %u st %u flags %04X phase %04X sub %02X\n",
                   CELL_SUPPLY, CELL_SUPPLY_ST, CELL_SUP_FLAGS, CELL_LAD_PHASE, CELL_LAD_SUB);
        }
    }
    CHECK(CELL_SUPPLY == 13566u && CELL_SUPPLY_ST == 6u);
    CHECK((CELL_SUP_FLAGS & 0x03u) == 0x03u);
}

/* no inputs: the ladder never starts (module 11 stays 0), the old r02q
 * conversation runs unchanged at target 5 */
static void scen_not_ready(void)
{
    run_t r;

    reset_all();
    run(&r, 99u, NULL);
    common_invariants(&r, "not-ready");
    CHECK(CELL_MOD11 == 0u && CELL_LAD_PHASE == 1u && CELL_LAD_SUB == 0u);
    CHECK(CELL_TARGET == 5u && stub.tioc_cuts == 0u);
    CHECK(CELL_SEQ2_STATE[0] == 0x11u && CELL_CUT_MODE == 0x11u);
}

/* the countdown service: every nonzero word / byte of its range, once */
static void test_countdown(void)
{
    reset_all();
    CELL_L1_U16(0xFFF94734u) = 2u;
    CELL_L1_U16(0xFFF94784u) = 1u;
    CELL_L1_U8(0xFFF94788u) = 3u;
    CELL_L1_U8(0xFFF94834u) = 1u;
    CELL_L1_U8(0xFFF94835u) = 9u;               /* outside: untouched */
    sup_ladder_tick_head();
    CHECK(CELL_L1_U16(0xFFF94734u) == 1u && CELL_L1_U16(0xFFF94784u) == 0u);
    CHECK(CELL_L1_U8(0xFFF94788u) == 2u && CELL_L1_U8(0xFFF94834u) == 0u);
    CHECK(CELL_L1_U8(0xFFF94835u) == 9u);
    sup_ladder_tick_head();
    CHECK(CELL_L1_U16(0xFFF94784u) == 0u);      /* floors at 0 */
    CELL_L1_U8(0xFFF94835u) = 0u;
}

#if !SUP_L1_HOLD_PHASE8_REQUEST
/* THE NEGATIVE TEST (built as test_ladder_l2 with the phase-8 hold
 * released — the recipe's "L2"): the invariant checks must see the stock's
 * one-frame 0x9020 shut-off test — the request lands at phase 4's pass tick
 * (phase 8 'U' runs in the same 0x8B7A8 call), seq2 / the composer / the J2
 * stage follow in the same tick, and phase 8's pass withdraws it the next
 * tick. Proves the J2 check is live and the stock's same-tick order holds. */
int main(void)
{
    run_t r;

    reset_all();
    run(&r, 300u, NULL);
    printf("L2 (phase-8 hold released): 0x9020 in %u frame(s), first F%u; hold F%u\n",
           r.bad_wire_frames, r.first_bad_frame, r.hold_frame);
    CHECK(r.bad_wire_frames == 1u && r.first_bad_frame == 101u);
    CHECK(CELL_LAD_PHASE == 0x40u && CELL_LAD_PASS == 0x3Fu);
    CHECK(CELL_LAD_REQ[0] == 0u && CELL_LETTERS[0] == 0u && CELL_J2_STAGED == 0x9000u);
    if (fails == 0) {
        printf("test_ladder_l2: ok (the invariant check sees the L2 frame)\n");
    }
    return fails != 0;
}
#else
int main(void)
{
    test_countdown();
    scen_not_ready();
    scen_nominal();
    scen_output_on();
    scen_no_drop();
    scen_budget();
    scen_supply();
    if (fails == 0) {
        printf("test_ladder: ok\n");
    }
    return fails != 0;
}
#endif

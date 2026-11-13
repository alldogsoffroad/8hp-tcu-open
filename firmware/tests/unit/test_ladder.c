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
    uint8_t w90;                /* L3: the low byte of the last 0x90 word heard */
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
    case 0x9000u: chip.w90 = (uint8_t)tx; return 0xAA00u;
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

/* L3: the switched output (mux 8) as a plant [I]: ON at ~supply while the
 * chip has heard a letter-bearing 0x90 word (bit 5) AND certifies (walk
 * level <= 4 with the TIOC clock present — the 0x14 bit-1 condition); one
 * 5 ms tick of decay (8000 mV, class 0x55) when it turns off; otherwise the
 * off-with-bias node (in_out_mv, 2942 mV). The chip's state is the one left
 * by the previous frame's batch, so the output follows the wire by one tick
 * (+ plant.lag more). */
static struct {
    bool enabled;
    bool never_on;              /* negative: the rail never comes up */
    bool ignore_level;          /* negative: the rail does not drop at level >= 5 */
    uint16_t ignore_from_phase; /* … only once the ladder phase reads >= this */
    unsigned lag;               /* extra ticks (0..6) */
    bool hist[8];
    bool was_on;
    unsigned on_frames;
} plant;

static uint16_t plant_mv(void)
{
    const bool clock = !stub.tioc_cut;
    bool on = (chip.w90 & 0x20u) != 0u && clock && !plant.never_on &&
              (chip.level <= 4u ||
               (plant.ignore_level && CELL_LAD_PHASE >= plant.ignore_from_phase));
    bool out;
    uint16_t mv;

    for (unsigned i = 7u; i > 0u; i--) {
        plant.hist[i] = plant.hist[i - 1u];
    }
    plant.hist[0] = on;
    out = plant.hist[plant.lag];
    if (out) {
        mv = 13300u;
        plant.on_frames++;
    } else if (plant.was_on) {
        mv = 8000u;
    } else {
        mv = in_out_mv;
    }
    plant.was_on = out;
    return mv;
}

static void inputs(void)
{
    CELL_MUX_MV[0] = plant.enabled ? plant_mv() : in_out_mv;
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

/* L3: a per-frame record (end of frame, after the batch) */
typedef struct {
    uint16_t phase;
    uint8_t sub, req0, seq, seq2, diag, cls, lvl, tgt, st939b8, fsm, en;
    bool w9020;                 /* a J2/F word 0x9020 went out this frame */
    uint16_t j2, mv;
} frame_t;
static frame_t s_fr[1200];

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
    chip.w90 = 0u;
    memset(plant.hist, 0, sizeof plant.hist);
    plant.was_on = false;
    plant.on_frames = 0u;
    memset(s_fr, 0, sizeof s_fr);
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
            bool w9020 = false;
            for (unsigned j = 0u; j < stub_log_n; j++) {
                if (stub_log[j].bus == 1u &&
                    (stub_log[j].job == JOB_EN || stub_log[j].job == JOB_FLT) &&
                    (stub_log[j].word & 0xFF00u) == 0x9000u && stub_log[j].word != 0x9000u) {
                    r->bad_wire++;
                    if (stub_log[j].word == 0x9020u) {
                        w9020 = true;
                    }
                }
            }
            if (p < sizeof s_fr / sizeof s_fr[0]) {
                frame_t *f = &s_fr[p];
                f->phase = CELL_LAD_PHASE;
                f->sub = CELL_LAD_SUB;
                f->req0 = CELL_LAD_REQ[0];
                f->seq = CELL_SEQ_STATE;
                f->seq2 = CELL_SEQ2_STATE[0];
                f->diag = CELL_SEQ2_DIAG[0];
                f->cls = CELL_OUT_CLASS[0];
                f->lvl = CELL_LVL_REPORTED;
                f->tgt = CELL_TARGET;
                f->st939b8 = CELL_SEQ2_STATUS[0];
                f->fsm = CELL_OUTER_FSM;
                f->en = CELL_EN_MIRROR[0];
                f->w9020 = w9020;
                f->j2 = CELL_J2_STAGED;
                f->mv = CELL_OUT_MV[0];
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

#if !SUP_L1_HOLD_PHASE40
/* ── L3: H2 released (phases 0x40 → 0x80 → 0x100 → 0xAA). Built as
 * test_ladder_l3a (H3 held — the target default) and test_ladder_l3b (H3
 * released: FSM 0x23 → 'E', sequencer 'L', seq2 '3' → 'U', letter 'c').
 * The wire check is per frame: which frames carried a J2/F word 0x9020,
 * and in which ladder phase (end of frame) each window sat. ─────────── */

typedef struct { unsigned from, to; } win_t;

static unsigned windows(unsigned frames, win_t *w, unsigned max)
{
    unsigned n = 0u;

    for (unsigned p = 1u; p <= frames; p++) {
        if (s_fr[p].w9020 && (p == 1u || !s_fr[p - 1u].w9020)) {
            if (n < max) {
                w[n].from = p;
            }
            n++;
        }
        if (s_fr[p].w9020 && (p == frames || !s_fr[p + 1u].w9020) && n != 0u && n <= max) {
            w[n - 1u].to = p;
        }
    }
    return n;
}

static void timeline(unsigned from, unsigned to)
{
    printf("  frame  t(ms) phase sub req wire seq seq2 diag cls   mV  lvl tgt fsm 939B8 en\n");
    for (unsigned p = from; p <= to; p++) {
        const frame_t *f = &s_fr[p];
        printf("  F%-4u %5d  %04X  %02X  %u  %s  %02X  %02X   %02X  %02X %5u  %u   %u  %02X   %u   %u\n",
               p, ((int)p - 100) * 5, f->phase, f->sub, f->req0, f->w9020 ? "9020" : "9000",
               f->seq, f->seq2, f->diag, f->cls, f->mv, f->lvl, f->tgt, f->fsm, f->st939b8,
               f->en);
    }
}

static unsigned first_frame(unsigned frames, bool (*pred)(const frame_t *))
{
    for (unsigned p = 1u; p <= frames; p++) {
        if (pred(&s_fr[p])) {
            return p;
        }
    }
    return 0u;
}

static bool is_aa(const frame_t *f) { return f->sub == 0xAAu; }
static bool is_u(const frame_t *f) { return f->seq2 == (uint8_t)'U'; }

/* the nominal run: returns the frame of 0xAA; checks the L2 frame and the
 * phase-0x40..0x80 window; prints the timeline */
static unsigned l3_nominal(unsigned lag, bool print)
{
    run_t r;
    win_t w[8];
    unsigned n, aa;

    reset_all();
    plant.enabled = true;
    plant.never_on = false;
    plant.ignore_level = false;
    plant.lag = lag;
    run(&r, 400u, NULL);                         /* 2 s */
    n = windows(r.frames, w, 8u);
    aa = first_frame(r.frames, is_aa);
    printf("L3 nominal (lag %u): start F%u, 0xAA F%u (%u ms after start), %u window(s):",
           lag, r.start_frame, aa, (aa - r.start_frame) * 5u, n);
    for (unsigned i = 0u; i < n && i < 8u; i++) {
        printf(" [F%u..F%u %u frame(s)]", w[i].from, w[i].to, w[i].to - w[i].from + 1u);
    }
    printf("\n");
    if (print) {
        timeline(99u, aa + 3u);
    }
    CHECK(r.start_frame == 100u);
    CHECK(aa != 0u && (aa - r.start_frame) * 5u < 200u);    /* inside the ladder's 200 ms */
    CHECK(n >= 2u);
    /* W0: the L2 shut-off frame, phase 8 */
    CHECK(w[0].from == 101u && w[0].to == 101u && s_fr[101].phase == 0x0008u);
    /* W1: phases 0x40 / 0x80 only, ends before 0xAA; the first frame is
     * phase 0x40's 'U' entry (sub 0x34), the frame after it has phase 0x80
     * passed (request 0 written before seq2 in the same tick) */
    for (unsigned p = w[1].from; p <= w[1].to; p++) {
        CHECK(s_fr[p].phase == 0x0040u || s_fr[p].phase == 0x0080u);
        CHECK(s_fr[p].req0 == 1u);
    }
    CHECK(s_fr[w[1].from].phase == 0x0040u && s_fr[w[1].from].sub == 0x34u);
    CHECK(s_fr[w[1].from - 1u].phase == 0x0020u);
    CHECK(s_fr[w[1].to + 1u].phase == 0x0100u && s_fr[w[1].to + 1u].req0 == 0u);
    CHECK(w[1].to < aa);
    return aa;
}

#if SUP_L1_HOLD_FAIL_FSM
/* L3a: the ladder completes (0xAA), the sequencer waits in '.', the FSM
 * stays 8 (0x87766(0x23) recorded), J2 9000 for good */
static void l3a_nominal(void)
{
    win_t w[8];
    unsigned n, aa;

    aa = l3_nominal(0u, true);
    n = windows(400u, w, 8u);
    CHECK(n == 2u);
    for (unsigned p = w[1].to + 1u; p <= 400u; p++) {
        CHECK(!s_fr[p].w9020 && s_fr[p].j2 == 0x9000u && s_fr[p].req0 == 0u);
        CHECK(s_fr[p].fsm == SUP_FSM_SUPERVISE);
    }
    printf("L3a at the end: phase %04X sub %02X pass %04X ok %04X fault %04X retries %u seq %02X "
           "fsm %02X held %02X target %u level %u class %02X seq2 %02X req %02X%02X letters %02X "
           "J2 %04X mod11 %02X mod7 %02X cut %02X 0x14 %02X gate %u\n",
           CELL_LAD_PHASE, CELL_LAD_SUB, CELL_LAD_PASS, CELL_LAD_OKFLAGS, CELL_LAD_FAULT,
           CELL_LAD_RETRIES, CELL_SEQ_STATE, CELL_OUTER_FSM, sup_ladder_fsm_req_held(),
           CELL_TARGET, CELL_LVL_REPORTED, CELL_OUT_CLASS[0], CELL_SEQ2_STATE[0],
           CELL_LAD_REQ[0], CELL_LAD_REQ[1], CELL_LETTERS[0], CELL_J2_STAGED, CELL_MOD11,
           CELL_MOD7, CELL_CUT_MODE, CELL_REG_14, CELL_SSLB0_BIT2);
    CHECK(CELL_LAD_PHASE == 0x0100u && CELL_LAD_SUB == 0xAAu);
    CHECK(CELL_LAD_PASS == 0x00FFu && CELL_LAD_OKFLAGS == 0x00FFu && CELL_LAD_FAULT == 0u);
    CHECK(CELL_LAD_RETRIES == 3u);
    CHECK(CELL_SEQ_STATE == (uint8_t)'.');
    CHECK(CELL_OUTER_FSM == SUP_FSM_SUPERVISE && sup_ladder_fsm_req_held() == 0x23u);
    CHECK(CELL_LAD_REQ[0] == 0u && CELL_LAD_REQ[1] == 0u);
    CHECK(CELL_LETTERS[0] == 0u && CELL_EN_MIRROR[0] == 0u && CELL_J2_STAGED == 0x9000u);
    CHECK(CELL_SEQ2_STATE[0] == 0x11u && CELL_OUT_CLASS[0] == 0x22u);
    CHECK(CELL_MOD11 == 0x0Fu && CELL_MOD7 == 0u && CELL_CUT_MODE == 0x33u);
    CHECK(CELL_TARGET == 0u && CELL_LVL_REPORTED <= 1u);
    CHECK(CELL_CLEAR_LATCH == 0u);
    (void)aa;
    /* one more tick of plant lag: the window grows by one frame, the rest
     * is unchanged */
    {
        win_t w0[8];
        unsigned n0;
        (void)l3_nominal(0u, false);
        n0 = windows(400u, w0, 8u);
        (void)l3_nominal(1u, false);
        n = windows(400u, w, 8u);
        CHECK(n0 == 2u && n == 2u);
        CHECK(w[1].to - w[1].from == w0[1].to - w0[1].from + 1u);
        CHECK(CELL_LAD_SUB == 0xAAu && CELL_SEQ_STATE == (uint8_t)'.');
    }
}
#elif SUP_L3_HOLD_SEQ_L
/* L3a-E (H3 released, H4 held): after 0xAA the FSM runs the stock's 0x23 →
 * 'E' walk (level 0, era 0x1838, the 0x8CEDC rule), the sequencer waits in
 * '.', no '3' / 'U': J2 0x9000 for good */
static void l3ae_nominal(void)
{
    win_t w[8];
    unsigned n, aa, e_frame = 0u, ctx[4] = { 0u, 0u, 0u, 0u };

    aa = l3_nominal(0u, false);
    n = windows(400u, w, 8u);
    for (unsigned p = aa; p <= 400u; p++) {
        if (s_fr[p].fsm == SUP_FSM_PROCEED) { e_frame = p; break; }
    }
    timeline(108u, e_frame + 4u);
    printf("L3a-E: 0xAA F%u, FSM 'E' F%u; end: seq %02X fsm %02X ret %02X target %u level %u "
           "period %04X J2 %04X letters %02X seq2 %02X 0x95011 %02X 0x93B97 %02X\n",
           aa, e_frame, CELL_SEQ_STATE, CELL_OUTER_FSM, CELL_FSM_RETURN, CELL_TARGET,
           CELL_LVL_REPORTED, CELL_PERIOD, CELL_J2_STAGED, CELL_LETTERS[0], CELL_SEQ2_STATE[0],
           CELL_E_STATE, CELL_E_LAST);
    CHECK(n == 2u);
    for (unsigned p = w[1].to + 1u; p <= 400u; p++) {
        CHECK(!s_fr[p].w9020 && s_fr[p].j2 == 0x9000u && s_fr[p].seq2 == 0x11u);
    }
    CHECK(e_frame != 0u && e_frame - aa <= 6u);
    CHECK(CELL_SEQ_STATE == (uint8_t)'.' && CELL_OUTER_FSM == SUP_FSM_PROCEED);
    CHECK(CELL_FSM_RETURN == SUP_FSM_DUTY && CELL_PERIOD == 0x1838u && CELL_TARGET == 0u);
    CHECK(CELL_LETTERS[0] == 0u && CELL_J2_STAGED == 0x9000u && CELL_LAD_REQ[0] == 0u);
    CHECK(CELL_REG_14 == 0xC7u && CELL_SSLB0_BIT2 == 1u);   /* '.' would advance … H4 */
    (void)ctx;
}
#else
/* L3b: through 'E' / 'L' / '3' to 'U' — the stock driving dump's state */
static void l3b_nominal(void)
{
    win_t w[8];
    unsigned n, aa, u, diag_frames = 0u, e_frame;

    aa = l3_nominal(0u, false);
    n = windows(400u, w, 8u);
    u = first_frame(400u, is_u);
    for (unsigned p = 1u; p <= 400u; p++) {
        if (s_fr[p].seq2 == 0x33u) {
            diag_frames++;
        }
    }
    e_frame = 0u;
    for (unsigned p = aa; p <= 400u; p++) {
        if (s_fr[p].fsm == SUP_FSM_PROCEED) { e_frame = p; break; }
    }
    printf("L3b: 0xAA F%u, FSM 'E' F%u, 'U' F%u (%u ms after the ladder start), '3' %u frame(s)\n",
           aa, e_frame, u, (u - 100u) * 5u, diag_frames);
    timeline(99u, u + 8u);
    CHECK(n == 3u);
    CHECK(u != 0u && w[2].from == u && w[2].to == 400u);   /* permanent from 'U' */
    CHECK(diag_frames == 4u);                    /* 10 ms settle + pass + 8 + 9 */
    for (unsigned p = w[1].to + 1u; p < u; p++) {
        CHECK(!s_fr[p].w9020 && s_fr[p].j2 == 0x9000u);
    }
    printf("L3b at the end: seq %c sub %02X phase %04X pass %04X ok %04X letters %02X %02X %02X "
           "907D6 %02X J2 %04X fsm %02X ret %02X 93B99 %u target %u level %u class %02X "
           "seq2 %02X/%02X 939B8 %u diag %02X req %02X%02X en %u/%u mod7 %02X mod11 %02X "
           "94118 %02X period %04X 939C2 %u test %04X seen %04X 9495C %04X 94958 %04X "
           "939C4 %02X hold %u cut %02X 0x14 %02X gate %u\n",
           CELL_SEQ_STATE, CELL_LAD_SUB, CELL_LAD_PHASE, CELL_LAD_PASS, CELL_LAD_OKFLAGS,
           CELL_LETTERS[0], CELL_LETTERS[1], CELL_LETTERS[2], CELL_EN_90, CELL_J2_STAGED,
           CELL_OUTER_FSM, CELL_FSM_RETURN, CELL_FSM_93B99, CELL_TARGET, CELL_LVL_REPORTED,
           CELL_OUT_CLASS[0], CELL_SEQ2_STATE[0], CELL_SEQ2_STATE[1], CELL_SEQ2_STATUS[0],
           CELL_SEQ2_DIAG[0], CELL_LAD_REQ[0], CELL_LAD_REQ[1], CELL_EN_MIRROR[0],
           CELL_EN_MIRROR[1], CELL_MOD7, CELL_MOD11, CELL_SEQ2_RESOLVED, CELL_PERIOD,
           CELL_DIAG_SUB[0], CELL_DIAG_TEST(0u), CELL_DIAG_SEEN(0u), CELL_DIAG_9495C[0],
           CELL_DIAG_94958[0], CELL_DIAG_CLS[0], CELL_CNT_HOLDOFF[0], CELL_CUT_MODE,
           CELL_REG_14, CELL_SSLB0_BIT2);
    /* the stock driving dump (letters_law_decode.md §4) */
    CHECK(CELL_SEQ_STATE == (uint8_t)'L');
    CHECK(CELL_LAD_SUB == 0xAAu && CELL_LAD_PHASE == 0x0100u && CELL_LAD_PASS == 0x00FFu);
    CHECK(CELL_LAD_OKFLAGS == 0x02FFu && CELL_LAD_FAULT == 0u && CELL_LAD_RETRIES == 3u);
    CHECK(CELL_LETTERS[0] == (uint8_t)'c' && CELL_LETTERS[1] == 0u && CELL_LETTERS[2] == 0u);
    CHECK(CELL_EN_90 == 0x20u && CELL_J2_STAGED == 0x9020u);
    CHECK(CELL_OUTER_FSM == SUP_FSM_PROCEED && CELL_OUTER_FSM_C == 0xBAu);
    CHECK(CELL_FSM_RETURN == SUP_FSM_DUTY && CELL_FSM_93B99 == 1u);
    CHECK(CELL_TARGET == 0u && CELL_PERIOD == 0x1838u);
    CHECK(CELL_OUT_CLASS[0] == 0x66u && CELL_OUT_CLASS[1] == 0x33u);
    CHECK(CELL_SEQ2_STATE[0] == (uint8_t)'U' && CELL_SEQ2_STATE[1] == 0x22u);
    CHECK(CELL_SEQ2_STATUS[0] == 4u && CELL_SEQ2_DIAG[0] == 0xFFu);
    CHECK(CELL_LAD_REQ[0] == 0u && CELL_LAD_REQ[1] == 0u);
    CHECK(CELL_EN_MIRROR[0] == 1u && CELL_EN_MIRROR[1] == 0u);
    CHECK(CELL_MOD7 == 0x0Fu && CELL_MOD11 == 0x0Fu && CELL_SEQ2_RESOLVED == 0x01u);
    CHECK(CELL_DIAG_SUB[0] == 3u && CELL_DIAG_TEST(0u) == 0x006Cu && CELL_DIAG_SEEN(0u) == 0u);
    CHECK(CELL_DIAG_94958[0] == 0u && CELL_CNT_HOLDOFF[0] == 0u);
    CHECK(CELL_CUT_MODE == 0x33u && CELL_REG_14 == 0xC7u && CELL_SSLB0_BIT2 == 1u);
    CHECK(CELL_CLEAR_LATCH == 0u && sup_ladder_fsm_req_held() == 0x23u);
}

/* L3b: the engaged output drops out of band → 0x85ED4's 0x86628 check
 * (after the 30 ms 'U' settle) loads the 72-tick hold-off 0x947B2: '4'
 * (letter off, 0x9000) the same tick; when it expires '4' → 'U' again (the
 * class is 0x22, not 0x11/0x44), 0x9020 for the 6-tick settle, and the
 * check fails again — the stock's retry cycle while the rail stays away */
static void l3b_engaged_fault(void)
{
    run_t r;
    unsigned drop = 0u, back = 0u, drop2 = 0u;

    (void)l3_nominal(0u, false);
    plant.never_on = true;                      /* the rail falls away */
    run(&r, 120u, NULL);
    plant.never_on = false;
    for (unsigned p = 1u; p <= r.frames; p++) {
        if (drop == 0u && s_fr[p].seq2 == (uint8_t)'4') { drop = p; }
        else if (drop != 0u && back == 0u && s_fr[p].seq2 == (uint8_t)'U') { back = p; }
        else if (back != 0u && drop2 == 0u && s_fr[p].seq2 == (uint8_t)'4') { drop2 = p; }
    }
    printf("L3b engaged fault: '4' at +F%u (9000), back to 'U' at +F%u (9020), '4' again "
           "at +F%u\n", drop, back, drop2);
    CHECK(drop != 0u && drop <= 3u);            /* the out-of-band mV seen next tick */
    CHECK(s_fr[drop].j2 == 0x9000u && !s_fr[drop].w9020 && s_fr[drop].en == 0u);
    CHECK(back == drop + 0x48u);                /* 72-tick (360 ms) hold-off */
    CHECK(s_fr[back].w9020);
    CHECK(drop2 == back + 6u);                  /* the 6-tick settle expires, the check runs */
    CHECK(CELL_DIAG_93120[0] == 0x0040u);       /* 0x865E0 latched the 'U' fault */
}
#endif

/* negative: the rail never comes on → phase 0x40 F2 after its 30 ms
 * tolerance, three retries (each a full re-run: one phase-8 frame and one
 * 0x40 window of at most 7 frames), exhaustion; the wire back at 0x9000 */
static void l3_never_on(void)
{
    run_t r;
    win_t w[16];
    unsigned n;

    reset_all();
    plant.enabled = true;
    plant.never_on = true;
    plant.ignore_level = false;
    plant.lag = 0u;
    run(&r, 600u, NULL);
    plant.never_on = false;
    n = windows(r.frames, w, 16u);
    printf("L3 never-on: seq %02X sub %02X retries %u failcls %02X held %02X fsm %02X "
           "latch %u seq2 %02X, %u window(s):", CELL_SEQ_STATE, CELL_LAD_SUB,
           CELL_LAD_RETRIES, CELL_LAD_FAILCLS[0], sup_ladder_fsm_req_held(), CELL_OUTER_FSM,
           CELL_CLEAR_LATCH, CELL_SEQ2_STATE[0], n);
    for (unsigned i = 0u; i < n && i < 16u; i++) {
        printf(" [F%u..F%u]", w[i].from, w[i].to);
    }
    printf("\n");
    CHECK(CELL_SEQ_STATE == 0x1Fu && CELL_LAD_SUB == 0xF2u && CELL_LAD_RETRIES == 0u);
    CHECK(CELL_LAD_FAILCLS[0] == 0x22u && sup_ladder_fsm_req_held() == 0x23u);
    CHECK(CELL_CLEAR_LATCH == 1u && CELL_SEQ2_STATE[0] == 0x22u);
    CHECK(n == 8u);                             /* 4 attempts × (phase 8 + phase 0x40) */
    for (unsigned i = 0u; i < n && i < 16u; i++) {
        CHECK(w[i].to - w[i].from + 1u <= 7u);
        for (unsigned p = w[i].from; p <= w[i].to; p++) {
            CHECK(s_fr[p].phase == 0x0008u || s_fr[p].phase == 0x0040u ||
                  (s_fr[p].phase == 0x0001u && s_fr[p].sub == 0x00u));   /* the retry tick */
        }
    }
    for (unsigned p = w[n - 1u].to + 1u; p <= r.frames; p++) {
        CHECK(!s_fr[p].w9020 && s_fr[p].j2 == 0x9000u);
    }
    CHECK(CELL_LETTERS[0] == 0u && CELL_EN_90 == 0u && CELL_J2_STAGED == 0x9000u);
    /* the last attempt's phase-0x40 request is never withdrawn (no phase-1
     * entry follows exhaustion, and 0x85E64 lands only in seq2 0x11/0x16):
     * the cell reads 1 but seq2 0x22 drives the enable 0 — stock behaviour */
    CHECK(CELL_LAD_REQ[0] == 1u && CELL_EN_MIRROR[0] == 0u);
#if SUP_L1_HOLD_FAIL_FSM
    CHECK(CELL_OUTER_FSM == SUP_FSM_SUPERVISE);
#else
    CHECK(CELL_OUTER_FSM == SUP_FSM_PROCEED);   /* 0x87766(0x23) made → 'E' */
    CHECK(CELL_SEQ_STATE == 0x1Fu);             /* … but no 'L' from 0x1F */
#endif
}

/* negative: the chip does not shut the rail off at level >= 5.
 * from_phase 1: phase 8's own shut-off frame turns it on → phase 8 F2 (no
 * wait: 0x8C2A4(0)), retries, exhaustion — phase 8 catches it before the
 * gate opens. from_phase 0x40: phase 0x80 waits its 30 ms with the request
 * still 1, then F2 → retry (phase 1 writes the request 0 the next tick). */
static void l3_no_shutoff(uint16_t from_phase)
{
    run_t r;
    win_t w[16];
    unsigned n;

    reset_all();
    plant.enabled = true;
    plant.never_on = false;
    plant.ignore_level = true;
    plant.ignore_from_phase = from_phase;
    plant.lag = 0u;
    run(&r, 600u, NULL);
    plant.ignore_level = false;
    n = windows(r.frames, w, 16u);
    printf("L3 no-shutoff from phase %04X: seq %02X sub %02X retries %u failcls %02X held %02X "
           "fsm %02X latch %u, %u window(s):", from_phase, CELL_SEQ_STATE, CELL_LAD_SUB,
           CELL_LAD_RETRIES, CELL_LAD_FAILCLS[0], sup_ladder_fsm_req_held(), CELL_OUTER_FSM,
           CELL_CLEAR_LATCH, n);
    for (unsigned i = 0u; i < n && i < 16u; i++) {
        printf(" [F%u..F%u]", w[i].from, w[i].to);
    }
    printf("\n");
    CHECK(CELL_SEQ_STATE == 0x1Fu && CELL_LAD_RETRIES == 0u && CELL_CLEAR_LATCH == 1u);
    CHECK(CELL_LAD_SUB == 0xF2u && sup_ladder_fsm_req_held() == 0x23u);
    CHECK(n >= 2u);
    /* bound: 0x40 'U' + its pass tick + the 0x80 level climb (here 1-2
     * ticks) + the 30 ms tolerance + the retry tick (the request is
     * withdrawn only by the next phase-1 entry) */
    for (unsigned i = 0u; i < n && i < 16u; i++) {
        CHECK(w[i].to - w[i].from + 1u <= 11u);
    }
    for (unsigned p = w[n - 1u].to + 1u; p <= r.frames; p++) {
        CHECK(!s_fr[p].w9020 && s_fr[p].j2 == 0x9000u);
    }
    CHECK(CELL_LETTERS[0] == 0u && CELL_J2_STAGED == 0x9000u);
    if (from_phase == 0x40u) {
        unsigned in80 = 0u;
        for (unsigned p = 1u; p <= r.frames; p++) {
            if (s_fr[p].phase == 0x0080u && s_fr[p].w9020) {
                in80++;
            }
        }
        CHECK(in80 >= 6u);                       /* the 30 ms wait rode 0x9020 */
        timeline(99u, 118u);
    }
}

int main(void)
{
    test_countdown();
#if SUP_L1_HOLD_FAIL_FSM
    l3a_nominal();
#elif SUP_L3_HOLD_SEQ_L
    l3ae_nominal();
#else
    l3b_nominal();
    l3b_engaged_fault();
#endif
    l3_never_on();
    l3_no_shutoff(0x0001u);
    l3_no_shutoff(0x0040u);
    if (fails == 0) {
        printf("test_ladder_l3%s: ok\n",
               SUP_L1_HOLD_FAIL_FSM ? "a" : SUP_L3_HOLD_SEQ_L ? "a_e" : "b");
    }
    return fails != 0;
}
#elif !SUP_L1_HOLD_PHASE8_REQUEST
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

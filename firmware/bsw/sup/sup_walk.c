/* sup_walk.c — the supervisor commissioning walk (design §7.7, spec 22 §6).
 *
 * The stock's machine, ported from the top-down decode (spec 22 §6 and
 * artifact2 §6; stock addresses in every comment). Structure:
 *
 *   monitor pass (5 ms, 0x873F0):  token stage (0x7EE8A) → THE OPENING
 *   LAW: everything below is gated on the chip's 0x14 bit 1 (the gate
 *   cell, harvested from the cyclic J6 reply) — pre-gate the wire is the
 *   stock's pre-flip batch only; the one-shot enable, walk task (0x6C63E)
 *   and outer FSM 0xFFF95010 arm at the first gate-open pass (parked
 *   while the walk state is non-zero).
 *
 *   walk task: state 7 samples (FRT deadline gate, stale reply → stall++,
 *   stop the timer → 0x19), state 0x19 = era stepper + reply processor,
 *   state 0 = era stepper; tail: staging ping-pong completes → 7, else 0.
 *   The output gate runs EVERY pass.
 *
 *   timer ISR (1 ms, plat_walktmr_isr → here): WDT already fed; dispatch by
 *   the job-mode cell: 0 → W1 duty token, 7 → W2 level. One-in-flight,
 *   silent skip when busy — dispatch starts on the FIRST tick after the
 *   programmer arms the timer (E60ab: no 84-class silence gaps).
 *
 * Documented interpretations ([I], kept adjacent to the law they serve):
 *   1. The reply processor harvests in walk state 0 as well as 7/0x19 — the
 *      decode shows 0x6C1EA called from 7 and 0x19 [O]; state 0 must also
 *      see fresh replies or the FSM's level gates (state 8 target, 0x45
 *      proceed) read a frozen nibble.
 *   2. The expected-vs-reported check resyncs silently (expected :=
 *      reported); the ATTENTION restart fires on the 0x0A echo mismatch —
 *      the bump-and-resend loop "syncs on the echo, not the nibble"
 *      (spec 03 level-exchange note). Firing it on the nibble too would
 *      fight its own resync every era.
 *   3. The expected/echo checks are level-era checks (job mode 7): during
 *      the DUTY drag the level is falling by design; only the commit phase
 *      mask (duty-context-set) is checked in mode 0.
 *   4. Guard bumps: level programmer bumps expected +1 (+2 for the 0x1F
 *      class), cap 7 — a downward command first pulls the expectation down.
 *      Duty twin: drag-down expected-- when level > 6, else a climb bump.
 *      (Design A2/V7: the guard-window compare scaling is replaced by fixed
 *      re-arm + this bookkeeping, "unchanged in effect".)
 */
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_token.h"
#include "bsw/sup/sup_ctrl.h"
#include "platform.h"

/* ── stock constants ────────────────────────────────────────────────────── */

#define WALK_VAL_INIT        0x3Fu    /* 0x6C12C boot init (never on the wire) */
#define WALK_ERA_06          0x4B0u   /* the 0x06 era period (6 x 200)         */
#define WALK_ERA_1F          0x1838u  /* the 0x1F era period (0x1F x 200)      */
#define WALK_PERIOD_MAX      0x3139u  /* 0x6C48C range law: value < 0x3139     */
#define WALK_LEVEL_MAX       7u
#define WALK_ERA_TAIL_COUNTS 2400u    /* the 0x960 tick tail of every era (raw
                                       * TCNTA counts — stock 0x6C542 compares
                                       * elapsed counts vs period + 2400, no
                                       * scaling; 2026-11-10 decode) */

/* budgets (spec 22 budget_table; lengths [I], structure [O]) */
#define WALK_S8_BUDGET_PASSES   4000u  /* state 8 supervise (~20 s at 5 ms)  */
#define WALK_S23_TIMEOUT_PASSES 4000u  /* 0x8D1BA duty-era timeout (~20 s)   */

#define WALK_ECHO_RETRIES    3u        /* 0x6C742 confirm retry bound        */

/* The state-7 sample window is the stock's GUARD CELL 0xFFF93844, which
 * 0x6C594 recomputes from the era period on every era change (the second
 * call site 0x6C7AC, right after the value*200 store). The stock's exact
 * integer law, mirrored instruction-for-instruction from 0x6C5DA-0x6C610
 * (period = era value x 200, e.g. 0x3138 → window 11722 counts):
 *   window = (((period - 16) * 31160 >> 15) - 136) * 32472 >> 15
 * (the subtract 136 is extu.b of the 165640 literal; the >> 15 are shld
 * -15). The old flat 4000 [I] was 3x short of the stock's steady-era
 * window and made the state-7 stale verdicts fire ~3 eras early. */
static uint16_t walk_reply_window(void)
{
    const uint32_t period = CELL_PERIOD;
    uint32_t w = (period - 16u) * 31160u;

    w >>= 15;
    w -= 136u;
    w *= 32472u;
    w >>= 15;
    return (uint16_t)w;
}
/* F1 (protocol_audit §1): the staging confirm loop has no timeout of its
 * own — the stock gets its bound from the WDT, our port feeds the WDT from
 * the tick task, so a stall parked in COLD_STAGED/WARM_PENDING spun
 * forever. N passes without a fresh reply (100 x 5 ms = 0.5 s) re-stage
 * the era and re-arm the timer. */
#define WALK_CONFIRM_TIMEOUT_PASSES 100u

/* duty contexts (see sup_token.c) */
#define WALK_CTX_STEADY      0x1Eu
#define WALK_CTX_INV         0x19u

/* walk flag bits (0xFFF93BA7) */
#define WALK_FL_EXPECT_LOW   0x08u     /* reported below expected  [I] */
#define WALK_FL_EXPECT_HIGH  0x10u     /* reported above expected  [I] */
#define WALK_FL_ECHO_LOW     0x20u     /* echo below staged        [I] */
#define WALK_FL_ECHO_HIGH    0x40u     /* echo above staged        [I] */
#define WALK_FL_GIVEUP       0x04u     /* ping-pong give-up                */

/* ── pass-scoped budgets (ordinary .bss — not bench-read cells) ────────── */

static uint16_t s_s8_budget;         /* the 0xFFF9417D countdown */
static uint16_t s_s23_timeout;       /* the 0x8D1BA timeout */
static uint16_t s_confirm_stale;     /* F1: confirm-loop staleness counter */

/* The stock's hardware-condition gate (0x68BE0: 0xFFFF1001 bit 6 — "no
 * fault condition present"). The platform layer owns register reads; until
 * a strong plat_hwcond() lands there, this weak default returns "condition
 * clear" (the bench-proven path) and the platform overrides it later. */
bool plat_hwcond(void);
__attribute__((weak)) bool plat_hwcond(void)
{
    return false;
}

/* ── forward ────────────────────────────────────────────────────────────── */

static void program_level(void);
static void duty_programmer(void);
static void era_stepper(void);
static void reply_processor(void);
static bool ping_pong_confirm(void);
static bool reply_fresh_now(void);

/* ── timer programmers (0x6C0BA level / 0x6BF9C duty twin) ─────────────── */

static void program_timer(void)
{
    /* the deadline cell is ELAPSED-relative: the stock's programmer
     * (0x6C0BA tail, 0x6C11C-0x6C124) stores window + elapsed_at_program —
     * and the state-7 gate compares (TCNTA - base) against it. The old
     * absolute `now + window` form could never elapse against an elapsed
     * gate (the state-7 park that the FRT=0 read masked). */
    CELL_ERA_BASE = plat_frt_us();                /* 0xFFF9240C */
    CELL_ERA_DEADLINE = walk_reply_window();      /* 0xFFF92410, guard cell 0xFFF93844 */
    /* dispatch starts on the first compare tick (E60ab: no silence gap) */
    plat_walktmr_start();
}

static void program_level(void)
{
    uint8_t lvl = CELL_VALUE;                     /* the commanded era value */

    CELL_JOB_MODE = 7u;                           /* 0xFFF93B9B */
    if (lvl < 8u) {
        if (lvl < CELL_LVL_EXPECTED) {
            /* a downward command pulls the expectation down first (the
             * 0x06 era from init-7) */
            CELL_LVL_EXPECTED = lvl;
        } else if (CELL_LVL_EXPECTED < WALK_LEVEL_MAX) {
            CELL_LVL_EXPECTED++;                  /* guard bump +1 */
        }
    } else if (CELL_LVL_EXPECTED < WALK_LEVEL_MAX) {
        /* the 0x1F class: guard bump +2 (era ramp, cap 7) */
        CELL_LVL_EXPECTED = (uint8_t)(CELL_LVL_EXPECTED + 2u);
        if (CELL_LVL_EXPECTED > WALK_LEVEL_MAX) {
            CELL_LVL_EXPECTED = WALK_LEVEL_MAX;
        }
    }
    CELL_COMMIT_MASK = 0u;                        /* settling phase */
    program_timer();
}

static void duty_programmer(void)
{
    uint8_t ctx = CELL_DUTY_CTX;

    CELL_JOB_MODE = 0u;                           /* the duty twin */
    /* phase mask: 1 while the context is cycling (0/7), 0 for the plain
     * and complement contexts 0x1E/0x19 */
    CELL_COMMIT_MASK = (ctx == WALK_CTX_STEADY || ctx == WALK_CTX_INV)
                       ? 0u : 1u;
    if (CELL_LVL_REPORTED > 6u) {
        if (CELL_LVL_EXPECTED != 0u) {
            CELL_LVL_EXPECTED--;                  /* drag-down */
        }
    } else if (CELL_LVL_EXPECTED < WALK_LEVEL_MAX) {
        CELL_LVL_EXPECTED++;                      /* context-aware climb [I] */
    }
    program_timer();
}

/* arm the DUTY era (the FSM's mode_req_set(0) law, 0x8D020 → 0x6C802):
 * steady context first, token staged, duty twin programmed. 0x6C802 is
 * self-guarded on the walk state — busy (nonzero state) means a sample is
 * in flight and the arm is skipped. The stock calls this arm EVERY steady
 * monitor pass (0x8799C → 0x8D020/0x8CEDC → 0x6C802), so the guard is what
 * makes the every-pass call safe: each idle pass re-stages the token and
 * re-runs the duty programmer, whose tail (0x6C0BA/0x6BF9C law) re-writes
 * OCR/TIER/TSR/TSTR — the walk tick is repaired 200×/s at steady. */
static void duty_arm(void)
{
    if (CELL_WALK_STATE != SUP_WALK_IDLE) {
        return;                               /* 0x6C802: busy — skip the arm */
    }
    CELL_DUTY_CTX = WALK_CTX_STEADY;
    CELL_DUTY_CTX_COMPL = (uint8_t)~WALK_CTX_STEADY;
    sup_token_stage(CELL_SELECTOR);
    duty_programmer();
}

/* the mode chain's level request (0x8D136 law). Target 0 arms the drag
 * (the DUTY era); targets >= 5 ride the level eras — the supervise state
 * watches the harvested level against them. */
static void mode_req_set(uint8_t target)
{
    if (target == 0u) {
        duty_arm();
    }
}

/* ── init (stock 0x6C12C park_pwm_init) ────────────────────────────────── */

void sup_walk_init(void)
{
    /* the commanded value pair */
    CELL_VALUE = WALK_VAL_INIT;
    CELL_VALUE_COMPL = 0xC0u;
    /* the period pair */
    CELL_PERIOD = 0x3138u;
    CELL_PERIOD_BASE = 0x3138u;
    /* expected AND reported level = 7 */
    CELL_LVL_EXPECTED = WALK_LEVEL_MAX;
    CELL_LVL_REPORTED = WALK_LEVEL_MAX;
    /* era counter pair = 1 / ~1 */
    CELL_ERA_COUNT = 1u;
    CELL_ERA_COMPL = 0xFEu;
    /* time base = now; guard windows derived (elapsed-relative, stock law) */
    CELL_ERA_BASE = plat_frt_us();
    CELL_ERA_DEADLINE = walk_reply_window();
    /* machine cells */
    CELL_BOOT_FLAG = 1u;                          /* 0xFFF9500E: walk armed  */
    CELL_WALK_STATE = 0u;                         /* 0xFFF93BA3 (RAM-cleared)*/
    CELL_OUTER_FSM = 0u;                          /* 0xFFF95010 */
    CELL_FSM_RETURN = 0u;                         /* 0xFFF93B93 */
    CELL_WALK_SUBST = 0u;                         /* 0xFFF93B43 */
    CELL_PENDING = 0u;                            /* 0xFFF94FE9 */
    CELL_JOB_MODE = 0u;
    CELL_COMMIT_MASK = 0u;
    CELL_SELECTOR = 0u;
    CELL_ATTENTION = 0u;
    CELL_WALK_FLAGS = 0u;
    CELL_STALL_CNT = 0u;
    CELL_RETRIES = 0u;
    CELL_COMMIT_RETRIES = 0u;
    CELL_TWO_PASS = 0u;
    CELL_MAX_LATCH = WALK_LEVEL_MAX;
    CELL_LAST_DUTY_CNT = 0u;
    CELL_LAST_LEVEL_CNT = 0u;
    CELL_STAGED_84 = 0u;
    /* duty context pair starts at the steady context [I] */
    CELL_DUTY_CTX = WALK_CTX_STEADY;
    CELL_DUTY_CTX_COMPL = (uint8_t)~WALK_CTX_STEADY;
    /* the one-shot enable latch */
    CELL_EN_ONESHOT = 0u;

    s_s8_budget = 0u;
    s_s23_timeout = 0u;
    s_confirm_stale = 0u;

    /* NO 84 word is sent here (D4/NR-2); the walk timer is not started —
     * the first stage arms it from the FSM's first pass. */
}

/* ── the one-shot enable (0x873F0 first-pass law) ──────────────────────── */

static void one_shot_enable(void)
{
    /* fires once per boot, on the first pass with a harvested level < 7
     * and the hardware condition clear — i.e. once the 0x06 era is live,
     * so the ask rides a running conversation (NR-1: never ask into a
     * silent bus; the bench-proven +105 ms shape falls out of this). */
    if (CELL_EN_ONESHOT == 0u &&
        CELL_LVL_REPORTED < WALK_LEVEL_MAX &&
        !plat_hwcond()) {
        sup_enable_request(true);
        CELL_EN_ONESHOT = 1u;
    }
}

/* ── era stepper (stock 0x6C542) ────────────────────────────────────────── */

/* era stepper (stock 0x6C542, decoded 2026-11-10): elapsed = TCNTA(base)
 * compared RAW against *(u16)0xFFF93848 + 2400 counts — no unit scaling —
 * and the base advances by the threshold itself (drift-free). The 32-bit
 * FRT deltas make the unsigned compare wrap-safe. */
static void era_stepper(void)
{
    uint32_t elapsed = plat_frt_us() - CELL_ERA_BASE;
    uint32_t threshold = (uint32_t)CELL_PERIOD + WALK_ERA_TAIL_COUNTS;

    if (elapsed < threshold) {
        return;
    }
    /* advance the era base */
    CELL_ERA_BASE = plat_frt_us();
    CELL_ERA_DEADLINE = walk_reply_window();   /* elapsed-relative (stock law) */
    /* the era counter pair steps */
    CELL_ERA_COUNT++;
    CELL_ERA_COMPL = (uint8_t)~CELL_ERA_COUNT;
    if (CELL_JOB_MODE == 7u) {
        /* level era: bump the expected level toward 7 */
        if (CELL_LVL_EXPECTED < WALK_LEVEL_MAX) {
            CELL_LVL_EXPECTED++;
        }
    } else {
        /* duty era: step the context cycle, re-program the twin (its phase
         * mask and bumps follow the new context) */
        (void)sup_token_context();
        duty_programmer();
    }
}

/* ── reply freshness (the mode-dependent counter watch) ────────────────── */

static bool reply_fresh_now(void)
{
    if (CELL_JOB_MODE == 7u) {
        return CELL_LEVEL_COUNT != CELL_LAST_LEVEL_CNT;
    }
    return CELL_DUTY_COUNT != CELL_LAST_DUTY_CNT;
}

/* ── reply processor (stock 0x6C1EA park_monitor_feedback) ─────────────── */

static void reply_processor(void)
{
    uint8_t mode = CELL_JOB_MODE;
    uint8_t lvl;

    /* freshness: the duty counter in job-mode 0, the level counter in
     * mode 7, each vs its last-seen cell */
    if (mode == 7u) {
        if (CELL_LEVEL_COUNT == CELL_LAST_LEVEL_CNT) {
            return;                               /* nothing fresh */
        }
        CELL_LAST_LEVEL_CNT = CELL_LEVEL_COUNT;
    } else {
        if (CELL_DUTY_COUNT == CELL_LAST_DUTY_CNT) {
            return;
        }
        CELL_LAST_DUTY_CNT = CELL_DUTY_COUNT;
    }

    /* harvest: selector = fb & 0xF, level = (fb >> 4) & 7 — the only
     * level-nibble harvester in the image */
    CELL_SELECTOR = (uint8_t)(CELL_FB_0C & 0x0Fu);
    lvl = (uint8_t)((CELL_FB_0C >> 4) & 7u);
    if (lvl > CELL_MAX_LATCH) {
        CELL_MAX_LATCH = lvl;
    }
    CELL_LVL_REPORTED = lvl;

    /* check 1: expected vs reported (level eras only — interpretation 3).
     * Mismatch → flag + silent resync (expected := reported). */
    if (mode == 7u && CELL_LVL_EXPECTED != CELL_LVL_REPORTED) {
        CELL_WALK_FLAGS |= (CELL_LVL_REPORTED < CELL_LVL_EXPECTED)
                           ? WALK_FL_EXPECT_LOW : WALK_FL_EXPECT_HIGH;
        CELL_LVL_EXPECTED = CELL_LVL_REPORTED;
    }

    /* check 2: the commit phase mask — (commit & 5) must equal the mask
     * the programmer set (0 settling / 1 duty-cycle). Mismatch counts. */
    if ((uint8_t)(CELL_COMMIT_12 & 5u) != CELL_COMMIT_MASK) {
        CELL_COMMIT_RETRIES++;
    }

    /* check 3: the 0x0A echo vs the staged value (level eras only).
     * Mismatch → flag + attention 7 → sub-state 0x2A → bump-and-resend;
     * the loop re-syncs on the echo (spec 03 level-exchange note). */
    if (mode == 7u && CELL_STAGED_84 != CELL_ECHO_0A) {
        CELL_WALK_FLAGS |= (CELL_ECHO_0A < CELL_STAGED_84)
                           ? WALK_FL_ECHO_LOW : WALK_FL_ECHO_HIGH;
        CELL_ATTENTION = 7u;
        CELL_WALK_SUBST = 0x2Au;
        sup_walk_req_restart();
    }
    /* SUBST 0x2A is the stock's attention marker; the restart has already
     * re-staged (SUBST now warm-pending or cold), so nothing to clear. */
}

/* ── staging ping-pong confirm (stock 0x6C742) ─────────────────────────── */

static bool ping_pong_confirm(void)
{
    switch (CELL_WALK_SUBST) {
    case SUP_SUB_COLD_STAGED: {
        /* the era value is on the wire; confirm on a fresh reply's echo */
        if (!reply_fresh_now()) {
            return false;
        }
        if (CELL_ECHO_0A == CELL_STAGED_84) {
            /* commit: the period is the era value x 200 */
            CELL_PERIOD = (uint16_t)((uint16_t)CELL_VALUE * 200u);
            CELL_PERIOD_BASE = CELL_PERIOD;
            CELL_RETRIES = 0u;
            CELL_WALK_SUBST = SUP_SUB_NONE;
            return true;                          /* era confirmed live */
        }
        /* fresh reply, echo mismatch: retry with a re-program */
        CELL_RETRIES++;
        if (CELL_RETRIES >= WALK_ECHO_RETRIES) {
            CELL_WALK_FLAGS |= WALK_FL_GIVEUP;    /* give up this value */
            CELL_RETRIES = 0u;
            CELL_WALK_SUBST = SUP_SUB_NONE;
            return false;
        }
        if (CELL_JOB_MODE == 7u) {
            program_level();                      /* re-program per retry */
        } else {
            duty_programmer();
        }
        return false;
    }
    case SUP_SUB_WARM_PENDING:
        /* promote the parked slot (0x6BD48-class diverse write) and put it
         * on the wire; the pass after confirms echo == staged */
        CELL_VALUE = CELL_PENDING;
        CELL_VALUE_COMPL = (uint8_t)~CELL_PENDING;
        CELL_PENDING = 0u;
        program_level();
        CELL_WALK_SUBST = SUP_SUB_COLD_STAGED;
        return false;
    default:
        return false;
    }
}

/* ── bump and resend (stock 0x6C720 park_pwm_req_restart) ──────────────── */

void sup_walk_req_restart(void)
{
    /* re-stage the current era value; the guard bump rides the programmer
     * and the exchange re-syncs on the 0x0A echo. Before the first stage
     * the era value is the 0x3F init — re-stage the 0x06 era instead. */
    uint16_t value = (CELL_VALUE == WALK_VAL_INIT)
                     ? WALK_ERA_06
                     : (uint16_t)((uint16_t)CELL_VALUE * 200u);
    (void)sup_walk_stage(value);
}

/* ── the staging ping-pong value request (stock 0x6C48C) ───────────────── */

uint8_t sup_walk_stage(uint16_t value)
{
    uint8_t level;

    if (value >= WALK_PERIOD_MAX) {
        return 2u;                                /* stock range law */
    }
    level = (uint8_t)(value / 200u);
    if (level == 0u) {
        return 2u;                                /* never stage an 84|00 */
    }

    if (CELL_WALK_STATE == 0u && CELL_WALK_SUBST == SUP_SUB_NONE) {
        /* cold: stage the diverse pair, program the timer, walk state 7 */
        CELL_VALUE = level;
        CELL_VALUE_COMPL = (uint8_t)~level;
        program_level();
        CELL_WALK_SUBST = SUP_SUB_COLD_STAGED;
        CELL_WALK_STATE = SUP_WALK_SAMPLE;
    } else {
        /* warm: park in the pending slot; the next pass promotes it, the
         * pass after confirms the echo */
        CELL_PENDING = level;
        CELL_WALK_SUBST = SUP_SUB_WARM_PENDING;
    }
    return 0u;
}

/* ── the output gate (0x6C63E tail — every pass) ───────────────────────── */

static void output_gate(void)
{
    /* certified byte: level >= 6 AND the granted echo bit */
    if (CELL_LVL_REPORTED >= 6u && CELL_GRANT != 0u) {
        CELL_CERTIFIED = 1u;
    }
    /* certification event: level 7 on two consecutive passes */
    if (CELL_LVL_REPORTED == WALK_LEVEL_MAX) {
        CELL_TWO_PASS++;
        if (CELL_TWO_PASS >= 2u) {
            CELL_CERT_EVENTS |= 0x2000u;
        }
    } else {
        CELL_TWO_PASS = 0u;
    }
}

/* ── the walk task (stock 0x6C63E) ──────────────────────────────────────── */

static void walk_task(void)
{
    switch (CELL_WALK_STATE) {
    case SUP_WALK_SAMPLE:
        /* state 7: wait for the sample point, then process (stock 0x6C64E
         * compares the elapsed delta against the deadline CELL 0xFFF92410) */
        if ((uint32_t)(plat_frt_us() - CELL_ERA_BASE) >= CELL_ERA_DEADLINE) {
            if (!reply_fresh_now()) {
                /* stale reply: stall++, stop the timer, → 0x19 — and ride
                 * the stock's own restart law so the ladder never sits
                 * silent (E60ab: no 84-class silence gaps). F1: re-arm the
                 * timer HERE — the restart's warm stage only promotes (and
                 * re-programs) on the NEXT pass tail, and a stopped timer
                 * can never produce the confirming reply. */
                CELL_STALL_CNT++;
                plat_walktmr_stop();
                CELL_WALK_STATE = SUP_WALK_ERA;
                sup_walk_req_restart();
                plat_walktmr_start();
                break;
            }
            reply_processor();
        }
        break;
    case SUP_WALK_ERA:
        /* state 0x19: era stepper + reply processor */
        era_stepper();
        reply_processor();
        break;
    default:
        /* state 0: era stepper; the reply processor also runs here so the
         * FSM's level gates stay live (interpretation 1) */
        era_stepper();
        reply_processor();
        break;
    }

    /* tail: the staging ping-pong completes → 7, else 0 */
    if (ping_pong_confirm()) {
        CELL_WALK_STATE = SUP_WALK_SAMPLE;
    } else {
        CELL_WALK_STATE = SUP_WALK_IDLE;
    }

    /* F1 (protocol_audit §1): bound the staging confirm loop. COLD_STAGED
     * only exits on a fresh reply, and only a timer dispatch can produce
     * one — the r01j wedge parked the machine here with the timer stopped,
     * spinning forever. After WALK_CONFIRM_TIMEOUT_PASSES without a fresh
     * reply, re-stage the era (sup_walk_req_restart — the same 0x6C720 law
     * the echo-mismatch path uses) and re-arm the timer immediately. */
    if (CELL_WALK_SUBST == SUP_SUB_COLD_STAGED ||
        CELL_WALK_SUBST == SUP_SUB_WARM_PENDING) {
        if (reply_fresh_now()) {
            s_confirm_stale = 0u;
        } else if (++s_confirm_stale >= WALK_CONFIRM_TIMEOUT_PASSES) {
            s_confirm_stale = 0u;
            sup_walk_req_restart();
            plat_walktmr_start();
        }
    } else {
        s_confirm_stale = 0u;
    }

    output_gate();
}

/* ── the outer FSM (0xFFF95010 — the era sequence) ──────────────────────── */

static void outer_fsm(void)
{
    switch (CELL_OUTER_FSM) {
    case SUP_FSM_BOOT:
        /* boot flag → mode_req_set(5); stage the 0x06 era (the first 84|06
         * on the wire); → 8 */
        if (CELL_BOOT_FLAG != 0u) {
            CELL_BOOT_FLAG = 0u;
            mode_req_set(5u);
        }
        if (CELL_PERIOD != WALK_ERA_06) {
            (void)sup_walk_stage(WALK_ERA_06);
        }
        CELL_FSM_RETURN = SUP_FSM_BOOT;
        CELL_OUTER_FSM = SUP_FSM_SUPERVISE;
        s_s8_budget = WALK_S8_BUDGET_PASSES;
        break;

    case SUP_FSM_SUPERVISE:
        /* C2, THE ERA-DWELL LAW (the first-500ms diff, D3): the stock sits
         * in state 8 / era 1 (84|06 ONLY) for the full era dwell — 0x4B0 +
         * 0x960 = 3600 ms — observed advancing 8→0x12/0x23 at ~+3.85 s
         * absolute (spec 21 §1 [O-bench]); the duty drag starts ~+3.9 s,
         * LONG after the letters flip (+150-250 ms). The old `lvl>=5`
         * early advance was a mis-port: with it the FIRST 84|06 reply
         * (level 6) advanced the FSM within 2-3 passes and the chip heard
         * 88-duty tokens in its flip window instead of 84|06 alone — the
         * one remaining first-500ms divergence after C1's refutation.
         * The dwell is elapsed-relative against CELL_ERA_BASE (stamped by
         * program_timer at the era staging; plat_frt_us counts µs). */
        if ((uint32_t)(plat_frt_us() - CELL_ERA_BASE) >= 3600000u) {
            CELL_FSM_RETURN = SUP_FSM_SUPERVISE;
            CELL_OUTER_FSM = SUP_FSM_REARM;
            CELL_BOOT_FLAG = 1u;
            break;
        }
        if (s_s8_budget != 0u) {
            s_s8_budget--;
            break;
        }
        sup_walk_req_restart();
        s_s8_budget = WALK_S8_BUDGET_PASSES;
        break;

    case SUP_FSM_REARM:
        /* boot flag → mode_req_set(5) again; → 0x23 */
        if (CELL_BOOT_FLAG != 0u) {
            CELL_BOOT_FLAG = 0u;
            mode_req_set(5u);
        }
        CELL_FSM_RETURN = SUP_FSM_REARM;
        CELL_OUTER_FSM = SUP_FSM_DUTY;
        CELL_BOOT_FLAG = 1u;
        break;

    case SUP_FSM_DUTY:
        /* boot flag → mode_req_set(0): the drag. The DUTY era (token,
         * context cycling) is already armed by mode_req_set(0). Timeout →
         * 0x34; reported level == 0 → stage the 0x1F era, → 0x45 */
        if (CELL_BOOT_FLAG != 0u) {
            CELL_BOOT_FLAG = 0u;
            mode_req_set(0u);
            s_s23_timeout = WALK_S23_TIMEOUT_PASSES;
        }
        if (CELL_LVL_REPORTED == 0u && CELL_PERIOD != WALK_ERA_1F) {
            (void)sup_walk_stage(WALK_ERA_1F);
            CELL_FSM_RETURN = SUP_FSM_DUTY;
            CELL_OUTER_FSM = SUP_FSM_PROCEED;
            break;
        }
        if (s_s23_timeout != 0u) {
            s_s23_timeout--;
            break;
        }
        CELL_FSM_RETURN = SUP_FSM_DUTY;
        CELL_OUTER_FSM = SUP_FSM_ADVANCE;
        break;

    case SUP_FSM_PROCEED:
        /* proceed gate: level >= 5 (a warm chip passes instantly) → 0x34 */
        if (CELL_LVL_REPORTED >= 5u) {
            CELL_FSM_RETURN = SUP_FSM_PROCEED;
            CELL_OUTER_FSM = SUP_FSM_ADVANCE;
        }
        break;

    case SUP_FSM_ADVANCE:
        /* fault_state_advance: prev 0x23 && level 0 → re-request the 0x1F
         * era; else steady supervision — the stock re-arms the duty twin
         * EVERY steady pass (0x8799C steady handler: RETURN==0x23 &&
         * reported!=0 → 0x8D020, else 0x8CEDC; both → 0x6C802 → the duty
         * programmer 0x6BF9C → the timer programmer tail). The old port
         * guarded the arm on mode!=0, which at steady (mode 0) re-armed
         * nothing — when the walk tick died mid-boot (r01o) nothing ever
         * revived it: W1 never reached the wire, the chip's feedback nibble
         * stayed 7 and the one-shot never fired. duty_arm() carries the
         * 0x6C802 state guard, so the every-pass call is safe. */
        if (CELL_FSM_RETURN == SUP_FSM_DUTY &&
            CELL_LVL_REPORTED == 0u &&
            CELL_PERIOD != WALK_ERA_1F) {
            (void)sup_walk_stage(WALK_ERA_1F);
            CELL_OUTER_FSM = SUP_FSM_PROCEED;
        } else {
            duty_arm();                           /* steady token rounds — every pass */
            CELL_BOOT_FLAG = 0u;
        }
        break;

    default:
        /* event bit path: return to the previous state */
        CELL_OUTER_FSM = CELL_FSM_RETURN;
        break;
    }
}

/* ── the 5 ms monitor task (stock 0x873F0) ─────────────────────────────── */

/* the opening lead-in: ~20 monitor passes ≈ 100 ms of batch-only wire,
 * the stock's scheduler-imposed window (0x67B16 gated on boot-state 3) */
#define OPENING_LEAD_IN_PASSES 20u
static uint8_t s_opening_passes;

void sup_monitor_poll(void)
{
    /* the monitor integrity sweep computes and stages the token each pass
     * (stock 0x7EE8A; the challenge selector rotates under chip control).
     * RAM-only staging: no wire word. */
    sup_token_stage(CELL_SELECTOR);

    /* THE OPENING LAW, SECOND REVISION (the first-500ms diff, r01x): the
     * r01w pre-gate return was WRONG — the stock's monitor 0x873F0 runs
     * walk task + one-shot + outer FSM UNCONDITIONALLY (decompile lines
     * 43-47: the one-shot is `latch==0 && reported<7`, no bit-1 gate in
     * the function). The stock's real opening: the 5 ms batch does not
     * run for ~95 ms (scheduler 0x67B16 hard-gated on boot-state
     * 0xFFF93B32==3 — the chip hears J5 ID then ~95 ms of lawful
     * silence), THEN the first effective monitor pass arms walk+ask at
     * ~+100 ms; the first 84|06 reply drags reported to 6 and the ask
     * fires ~5-10 ms later; the flip follows ~+150-250 ms. r01v (arm
     * immediately) and r01w (never arm) were both refuted on the bench —
     * the LEAD-IN-THEN-ARM sequence was the one untested cell. The lead-in
     * below reproduces the stock's ~20-pass batch-only window; after it,
     * the stock's body runs unconditionally every pass. The letters
     * ladder stays honestly bit-1-gated (0x8BB34) and the tamper
     * interlock is untouched. */
    if (s_opening_passes < OPENING_LEAD_IN_PASSES) {
        s_opening_passes++;
        return;
    }

    one_shot_enable();

    /* the walk task runs every pass; the outer FSM is parked while the
     * walk state is non-zero */
    walk_task();
    if (CELL_WALK_STATE == SUP_WALK_IDLE) {
        outer_fsm();
    }
}

/* ── the W2 level builder (stock 0x91190) ───────────────────────────────── */

void sup_level_job(void)
{
    uint16_t *st;

    if (!sup_job_check_free(JOB_LEVEL)) {
        return;                                   /* one-in-flight: silent */
    }
    if (CELL_VALUE == WALK_VAL_INIT) {
        return;                                   /* 0x3F never goes on the wire */
    }
    /* the stagers own the words: stage all four frames */
    st = sup_job_table[JOB_LEVEL].tx_stage;
    st[0] = (uint16_t)(0x8400u | (uint16_t)CELL_VALUE);
    st[1] = 0x0A00u;
    st[2] = 0x1200u;
    st[3] = 0x0C00u;
    CELL_STAGED_84 = CELL_VALUE;                  /* the echo-compare value */
    (void)sup_job_queue(JOB_LEVEL);
}

/* ── the W1 duty builder (stock 0x91086) ────────────────────────────────── */

void sup_duty_job(void)
{
    uint32_t tok;
    uint16_t *st;

    if (!sup_job_check_free(JOB_DUTY)) {
        return;                                   /* one-in-flight: silent */
    }
    tok = CELL_TOKEN_STAGED;
    if (CELL_DUTY_CTX == WALK_CTX_INV) {
        tok = ~tok;                               /* context 0x19 sends ~token */
    }
    /* the four token bytes OR'd onto the four 0x88 template words */
    st = sup_job_table[JOB_DUTY].tx_stage;
    st[0] = (uint16_t)(0x8800u | ((tok >> 24) & 0xFFu));
    st[1] = (uint16_t)(0x8800u | ((tok >> 16) & 0xFFu));
    st[2] = (uint16_t)(0x8800u | ((tok >> 8) & 0xFFu));
    st[3] = (uint16_t)(0x8800u | (tok & 0xFFu));
    st[4] = 0x1200u;
    st[5] = 0x0C00u;
    st[6] = 0x0A00u;
    CELL_TOKEN_LAST = tok;                        /* 0xFFF90548 (builder) */
    (void)sup_job_queue(JOB_DUTY);
}

/* ── the walk timer ISR service (stock 0x6C1BA park_pwm_service) ───────── */

void sup_walk_isr(void)
{
    /* The WDT feed is already done by plat_walktmr_isr (the stock feeds
     * before dispatching — the walk's liveness IS the WDT's liveness).
     * Dispatch by the job-mode cell; a busy job is skipped silently
     * (stock 0x93B8E check-free inside the builders). Then deliver: the
     * walk queue has priority, so a queued walk job goes out on this very
     * tick when the bus is idle (E60ab: dispatch from the first tick — a
     * busy bus leaves it queued and the next handler-done/batch pump
     * delivers it). */
    uint8_t mode = CELL_JOB_MODE;

    if (mode == 0u) {
        sup_duty_job();                           /* the DUTY era: W1 */
    } else if (mode == 7u) {
        sup_level_job();                          /* the LEVEL eras: W2 */
    }
    sup_job_engine_pump();
}

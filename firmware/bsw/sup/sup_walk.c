/* sup_walk.c — the supervisor walk: the stock's walk core, ported function
 * for function (r02f, 2026-11-11 decode; ~/tcu_sweep/supervisor_reset_decode.md).
 *
 * WHY THIS REWRITE. Every build that put the walk/ask on bus B was reset by
 * something outside the MCU (s_wovf_count flat in every log); r01w — no walk,
 * no ask — was the only clean build. The port had re-shaped the stock's walk
 * core: a free-running ~5 ms W2 tick (Design A2/V7 "fixed re-arm, unchanged
 * in effect" [I]), state 8 parked on 84|06 alone for "3600 ms" (C2), the
 * reply processor run in walk state 0, exchange timing re-based on
 * programming time. The decompiles say otherwise [O]:
 *   - TCNTA counts µs (1 MHz, measured on r02e), so 0x4B0 + 0x960 is 3600
 *     µs: the 06 era's CYCLE, not a 3.6 s dwell. C2 was a units error.
 *   - every walk job is a timed ONE-SHOT placed relative to the last
 *     exchange (the W1/W2 completion stamp 0x90544 = TCNTA - 0x29) against
 *     guard windows 0x6C594 derives from the era period;
 *   - from its first idle pass in state 8 the stock answers the chip with
 *     W1 tokens (0x8D020 → 0x6C802(0x7EE8A(), 0x1E)) while the reported
 *     level is above the target 5, and resends 84|v (0x6C720) at level ≤ 5;
 *   - the walk task runs the reply processor in states 7 and 0x19 only.
 * The chip-side law (a windowed challenge/response the supervisor enforces
 * by resetting the MCU) is [I]; the port divergences are [O].
 *
 * Stock map (decompile addresses):
 *   0x6C12C init · 0x6C594 guards · 0x6C542 era stepper · 0x6C1EA reply
 *   processor · 0x6C742 staging confirm · 0x6C48C stage · 0x6C0BA level
 *   programmer · 0x6BF9C duty twin · 0x6C720 resend · 0x6C802 token arm ·
 *   0x6C63E walk task · 0x8D136/0x8D108 level target · 0x8D020 state-8 law ·
 *   0x6C1BA timer service · 0x873F0 monitor body.
 *
 * Ported deviations (each [I] or platform-forced, kept adjacent to its law):
 *   D1 THE SHOT IS CLOSED-LOOP ON TCNTA. The stock loads DCNTD00 = delay<<8
 *      and trusts the counter. In our clock setup one DCNTD00 tick is ~28 µs
 *      (r01t: 4 ticks ≈ 112 µs per launch; r01u: 176 ticks ≈ 4.9 ms), not the
 *      µs the stock's arithmetic implies — the counter's clock select is not
 *      decoded. So the programmers compute the stock's exact fire time in
 *      TCNTA µs, the platform arms a deliberately SHORT shot (32 µs/tick),
 *      the service re-arms for the remainder while more than 40 µs remain,
 *      then busy-waits the last stretch on TCNTA (inside the walk ISR).
 *      Never early; late by the ISR latency only.
 *   D2 Fault-manager reporting (0x6865C fault 0x221/0x226 on diverse-pair
 *      mismatches, the 0x94F84/0x94DFC lamp bits, 0x9384C trace mask) is not
 *      ported; pair mismatches are counted in s_pair_faults.
 *   D3 The FSM's exits from 8 are 0x87766 calls made ONLY by the mode
 *      sequencer 0x85160 (sup_ladder.c): 0x23 after the ladder's 0xAA, and
 *      0x12/0x23 on retry exhaustion (ladder_blockers_decode.md §3.2).
 *      L3 (2026-11-13) PORTS them: sup_monitor_request = 0x87766, states
 *      0x12 / 0x23 / 'E' (0x45) / 0x34 and the "other" recovery of the
 *      0x873F0 body (bytes 0x874F2..0x876F0), 0x8799C and the 'E' rule
 *      0x8CEDC (bytes 0x8CEDC..0x8CFEE) — see ~/tcu_sweep/l3_port.md. The
 *      sequencer still HOLDS the calls under SUP_L1_HOLD_FAIL_FSM (H3, the
 *      L3a target default), so on L3a the FSM stays 8; L3b releases them.
 *      Not ported (D2 class): the 0x9384C trace bits, the 0x94F84/0x94DFC
 *      lamp bits the 0x34 path and the 'other' recovery set, the per-pass
 *      0x94F84 clear at 0x873F0 entry, and the fault 0x226 report.
 *   D4 The token is sup_token_compute(selector) (the bench-proven E-era fold
 *      of the 0x7EE8A sweep's table); the full integrity sweep is not ported.
 */
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_token.h"
#include "bsw/sup/sup_ctrl.h"
#include "platform.h"

/* ── stock constants ────────────────────────────────────────────────────── */

#define WALK_VAL_INIT     0x3Fu      /* 0x6C12C: value pair 3F/C0 (never on the wire) */
#define WALK_PERIOD_INIT  0x3138u    /* 0x6C12C: period = 0x3F x 200 */
#define WALK_ERA_06       0x4B0u     /* the 06 era period (0x873F0 state 0) */
#define WALK_ERA_TAIL     0x960u     /* 0x6C542: every cycle = period + 2400 µs */
#define WALK_PERIOD_MAX   0x3139u    /* 0x6C48C range law */
#define WALK_LEVEL_MAX    7u
#define WALK_BURST_GUARD  (0x29u + 0xBBu)  /* 0x6C12C: max(0x40CA2,0x40CA4)+0xBB = 228 */
#define WALK_STAMP_BACK   0x29u      /* 0x910EE/0x911D8: stamp = TCNTA - 0x29 */
#define WALK_MIN_DELAY    5u         /* 0x6C0BA/0x6BF9C: delay floor */
#define WALK_S8_BASE      3u         /* 0x8D136: *(u8*)0x191A1B via 0x66C94[0x90AB8=0] */

#define WALK_SHOT_SPIN_US 40u        /* D1: busy-wait the final stretch on TCNTA */
#define CTX_STEADY        0x1Eu      /* the 0x8D020 token context */
#define CTX_INV           0x19u
#define CTX_ATTN          0x07u

#define STATE_SAMPLE      SUP_WALK_SAMPLE  /* 7 */
#define STATE_ERA         SUP_WALK_ERA     /* 0x19 */
#define STATE_IDLE        SUP_WALK_IDLE    /* 0 */

static uint16_t s_pair_faults;       /* D2: diverse-pair mismatches seen */
static uint32_t s_shot_at;           /* D1: the TCNTA µs time the armed job fires */
static bool     s_shot_armed;

/* ── small stock helpers ────────────────────────────────────────────────── */

/* 0x6BDCA: read a diverse pair (value, complement); mismatch → fault 0x221 */
static uint8_t pair_get(volatile uint8_t *v, volatile uint8_t *c)
{
    if (*v != (uint8_t)~*c) {
        s_pair_faults++;
    }
    return *v;
}

/* 0x6BD48: write a diverse pair under the lock (raise to 0xE) */
static void pair_set(volatile uint8_t *v, volatile uint8_t *c, uint8_t val)
{
    uint32_t prev = plat_sr_i();

    if (prev < 0xEu) {
        plat_sr_set(0xEu);
    }
    *v = val;
    *c = (uint8_t)~val;
    plat_sr_set(prev);
}

static uint32_t elapsed_now(void)
{
    return plat_frt_us() - CELL_ERA_BASE;
}

/* ── guards (stock 0x6C594, exact integer law) ──────────────────────────── */

/* For period 0x4B0: 2346 / 3207 / 3775 / 945. For 0x1838: 7346 / 7918 /
 * 9087 / 5657 — the stock driving dump's 0x9383E..44 = 1CB2 1EEE 237F 1619
 * exactly. (The old walk_reply_window() subtracted 136 for the 0xAB; the
 * dump says 171.) */
static void guards_compute(void)
{
    const uint32_t p = CELL_PERIOD;

    CELL_GUARD_OPEN = (uint16_t)(p + 0x47Au);
    CELL_GUARD_CLOSE = (uint16_t)(((((p * 0x79B8u + 0x46D8180u) >> 15) - 0xABu)
                                   * 0x7ED8u) >> 15);
    CELL_GUARD_END = (uint16_t)(((((p * 0x8688u + 0x4ED3B00u) >> 15) - 0x36u)
                                 * 0x8168u + 0x28708u) >> 15);
    CELL_GUARD_SAMPLE = (uint16_t)((((((p - 0x10u) * 0x79B8u) >> 15) - 0xABu)
                                    * 0x7ED8u) >> 15);
}

/* ── the shared programmer tail (0x6C0BA / 0x6BF9C, identical) ──────────── */

/* fire = the elapsed-from-base point the job must go out at; base0 = the
 * base as read at the programmer's entry. Stock: delay = (fire & 0xFFFF) - 1
 * - (TCNTA - base0), floor 5, DCNTD00 = delay << 8, DSTRD0 |= 1. */
/* Review fixes (r02f review, 2026-11-11):
 *  - the stock arms inside 0x91004 with SR.I = 0xF (jsr 0x96650) and reads
 *    TCNTA after masking — so does this tail now (no interleave with the
 *    walk ISR's own re-arm, no stale shot firing under a new job mode);
 *  - SHORT DELAYS DO NOT USE THE COUNTER: a 5 µs delay would round up to
 *    one ~28 µs tick, so a resend programmed just before the keep-out
 *    (e in CLOSE-28..CLOSE) would launch INSIDE it (the stock's 5 µs floor
 *    leaves ~5 µs of exposure). At ≤ 40 µs the service runs here directly:
 *    it spins the remainder on TCNTA and sends. */
static void arm_shot(uint32_t fire, uint32_t base0)
{
    uint32_t prev = plat_sr_i();
    uint32_t now;
    int32_t delay;

    plat_sr_set(0xFu);
    now = plat_frt_us();
    delay = (int32_t)(fire & 0xFFFFu) - 1 - (int32_t)(now - base0);
    if (delay < (int32_t)WALK_MIN_DELAY) {
        delay = (int32_t)WALK_MIN_DELAY;
    }
    CELL_ERA_DEADLINE = CELL_BURST_GUARD + fire;          /* 0x92410 */
    s_shot_at = now + (uint32_t)delay;
    s_shot_armed = true;
    if (delay <= (int32_t)WALK_SHOT_SPIN_US) {
        plat_walktmr_stop();                              /* no stale shot pending */
        sup_walk_isr();                                   /* spin to s_shot_at, send */
    } else {
        plat_walktmr_arm_us((uint32_t)delay);             /* D1 */
    }
    plat_sr_set(prev);
}

/* ── level programmer (stock 0x6C0BA) ───────────────────────────────────── */

static void level_prog(void)
{
    uint32_t base0 = CELL_ERA_BASE;
    uint32_t e;

    CELL_JOB_MODE = 7u;
    e = plat_frt_us() - base0;
    /* never place an 84 write in the keep-out (0x93840, 0x93842) */
    if (CELL_GUARD_CLOSE < e && e < CELL_GUARD_END) {
        CELL_LVL_EXPECTED++;
        CELL_WALK_FLAGS |= 0x80u;
        e = CELL_GUARD_END;
    }
    CELL_LVL_EXPECTED++;
    if (CELL_LVL_EXPECTED > WALK_LEVEL_MAX) {
        CELL_LVL_EXPECTED = WALK_LEVEL_MAX;
    }
    CELL_COMMIT_MASK = 0u;                                /* 0x93BA5 */
    arm_shot(e, base0);
}

/* ── duty twin (stock 0x6BF9C) ──────────────────────────────────────────── */

static void duty_prog(void)
{
    uint32_t base0;
    uint32_t e;
    uint32_t fire;
    uint8_t ctx;

    CELL_JOB_MODE = 0u;
    e = plat_frt_us() - CELL_ERA_BASE;
    ctx = pair_get(&CELL_DUTY_CTX, &CELL_DUTY_CTX_COMPL);
    base0 = CELL_ERA_BASE;
    if (ctx != CTX_STEADY && ctx != CTX_INV) {
        if (ctx == CTX_ATTN) {
            fire = CELL_GUARD_END;
            if (CELL_GUARD_END < e) {
                fire = e;
            }
            CELL_LVL_EXPECTED = (uint8_t)(CELL_LVL_EXPECTED + 2u);
        } else {
            fire = CELL_GUARD_SAMPLE;
            if (fire < e) {
                CELL_LVL_EXPECTED = (uint8_t)(CELL_LVL_EXPECTED + 2u);
                CELL_WALK_FLAGS |= 0x02u;
                fire = CELL_GUARD_END;
            } else {
                CELL_LVL_EXPECTED++;
            }
        }
        if (CELL_LVL_EXPECTED > WALK_LEVEL_MAX) {
            CELL_LVL_EXPECTED = WALK_LEVEL_MAX;
        }
        CELL_COMMIT_MASK = 1u;
        arm_shot(fire, base0);
        return;
    }
    /* contexts 0x1E / 0x19: the token window [0x9383E, 0x93840] */
    fire = CELL_GUARD_OPEN;
    if (CELL_GUARD_OPEN < e && (fire = CELL_GUARD_CLOSE, CELL_GUARD_CLOSE < e)) {
        /* late: the next cycle's window open */
        fire = (uint32_t)CELL_PERIOD + WALK_ERA_TAIL + CELL_GUARD_OPEN;
        CELL_WALK_FLAGS |= 0x01u;
        if (CELL_LVL_REPORTED > 6u) {
            CELL_LVL_EXPECTED--;
        }
    } else {
        CELL_LVL_EXPECTED--;
    }
    if (ctx == CTX_STEADY) {
        if (CELL_LVL_EXPECTED > WALK_LEVEL_MAX) {
            CELL_LVL_EXPECTED = 0u;                       /* the u8 underflow → 0 */
        }
        CELL_COMMIT_MASK = 0u;
    } else {
        CELL_LVL_EXPECTED = (uint8_t)(CELL_LVL_EXPECTED + 2u);
        if (CELL_LVL_EXPECTED > WALK_LEVEL_MAX) {
            CELL_LVL_EXPECTED = WALK_LEVEL_MAX;
        }
        CELL_COMMIT_MASK = 4u;
    }
    arm_shot(fire, base0);
}

/* ── init (stock 0x6C12C, plus the machine cells the stock RAM-clears) ──── */

void sup_walk_init(void)
{
    CELL_PERIOD = WALK_PERIOD_INIT;
    CELL_VALUE = WALK_VAL_INIT;
    CELL_VALUE_COMPL = 0xC0u;
    CELL_PENDING = 0u;
    CELL_PENDING_COMPL = 0xFFu;
    CELL_LVL_EXPECTED = WALK_LEVEL_MAX;
    CELL_LVL_REPORTED = WALK_LEVEL_MAX;
    CELL_ERA_BASE = plat_frt_us();
    CELL_ERA_COUNT = 1u;
    CELL_ERA_COMPL = 0xFEu;
    CELL_LAST_DUTY_CNT = (uint8_t)~CELL_DUTY_RXCNT;
    CELL_LAST_LEVEL_CNT = (uint8_t)~CELL_LEVEL_COUNT;
    guards_compute();
    CELL_BURST_GUARD = WALK_BURST_GUARD;

    /* RAM-cleared in the stock; explicit here (several sit outside the
     * ranges sup_cells_init clears on older layouts) */
    CELL_BOOT_FLAG = 1u;                  /* 0x9500E (0x87732 sets it) */
    CELL_BOOT_FLAG_C = 0xFEu;
    CELL_WALK_STATE = STATE_IDLE;
    CELL_OUTER_FSM = SUP_FSM_BOOT;
    CELL_OUTER_FSM_C = 0xFFu;
    CELL_FSM_RETURN = 0u;
    CELL_FSM_93B99 = 0u;
    CELL_E_STATE = CTX_STEADY;            /* 0x8CFF0 (from 0x87732): 0x95011 = 0x1E, */
    CELL_E_STATE_C = (uint8_t)~CTX_STEADY;/* 0x94E89 = 0xE1, 0x93B97 = 0x1E          */
    CELL_E_LAST = CTX_STEADY;
    CELL_STAGE_SUB = 0u;
    CELL_WALK_SUBST = 0u;
    CELL_JOB_MODE = 0u;
    CELL_COMMIT_MASK = 0u;
    CELL_SELECTOR = 0u;
    CELL_ATTENTION = 0u;
    CELL_WALK_FLAGS = 0u;
    CELL_STALL_CNT = 0u;
    CELL_RXERR_CNT = 0u;
    CELL_RETRIES = 0u;
    CELL_COMMIT_RETRIES = 0u;
    CELL_WALK_ERRCNT = 0u;
    CELL_TWO_PASS = 0u;
    CELL_STAGED_84 = 0u;
    CELL_DUTY_CTX = CTX_STEADY;
    CELL_DUTY_CTX_COMPL = (uint8_t)~CTX_STEADY;
    CELL_TARGET = 0u;
    CELL_TARGET_COMPL = 0xFFu;
    CELL_S8_BUDGET = 0u;
    CELL_S8_KIND = 0u;
    CELL_S8_FLAG = 0u;
    CELL_EN_ONESHOT = 0u;
    s_pair_faults = 0u;
    s_shot_armed = false;
    /* no 84 word here and no timer start: the FSM's first pass stages */
}

/* ── era stepper (stock 0x6C542) ────────────────────────────────────────── */

static void era_stepper(void)
{
    uint32_t cycle = (uint32_t)CELL_PERIOD + WALK_ERA_TAIL;

    if (cycle <= elapsed_now()) {
        CELL_ERA_BASE += cycle;                           /* drift-free */
        if (CELL_LVL_EXPECTED < WALK_LEVEL_MAX) {
            CELL_LVL_REPORTED = (uint8_t)(CELL_LVL_EXPECTED + 1u);
            CELL_LVL_EXPECTED = CELL_LVL_REPORTED;
        }
        CELL_ERA_COUNT++;
        CELL_ERA_COMPL--;
    }
}

/* ── reply processor (stock 0x6C1EA) ────────────────────────────────────── */

/* returns 1 when walk state 7 saw no fresh reply (→ the task moves to 0x19) */
static uint8_t reply_processor(void)
{
    const uint8_t era = CELL_ERA_COUNT;                   /* bVar2 */
    const uint8_t era_c = CELL_ERA_COMPL;                 /* bVar1 */
    uint8_t attn = 0u;                                    /* uVar5 */
    uint8_t attn6 = 0u;                                   /* uVar6 */
    uint8_t stale_in_7 = 0u;
    bool stale;

    if (CELL_JOB_MODE == 0u) {
        stale = (CELL_DUTY_RXCNT == CELL_LAST_DUTY_CNT);
        if (!stale) {
            CELL_LAST_DUTY_CNT = CELL_DUTY_RXCNT;
        }
    } else {
        stale = (CELL_LEVEL_COUNT == CELL_LAST_LEVEL_CNT);
        if (!stale) {
            CELL_LAST_LEVEL_CNT = CELL_LEVEL_COUNT;
        }
    }

    if (stale) {
        if (CELL_WALK_STATE == STATE_SAMPLE) {
            CELL_STALL_CNT++;
            stale_in_7 = 1u;
        } else {
            plat_walktmr_stop();                          /* 0x91058 */
            s_shot_armed = false;
            attn = 7u;
        }
    } else if (CELL_RX_ERR != 0u) {
        CELL_RXERR_CNT++;
        CELL_RX_ERR = 0u;
        CELL_REPLY_FRESH = 0u;
        attn = 7u;
    } else {
        CELL_ERA_COUNT = 0u;
        CELL_ERA_COMPL = 0xFFu;
        CELL_SELECTOR = (uint8_t)(CELL_FB_0C & 0x0Fu);
        CELL_LVL_REPORTED = (uint8_t)((CELL_FB_0C >> 4) & 7u);
        if (CELL_LVL_EXPECTED == CELL_LVL_REPORTED) {
            if ((uint8_t)(CELL_COMMIT_12 & 5u) == CELL_COMMIT_MASK) {
                if (CELL_COMMIT_RETRIES != 0u) {
                    CELL_COMMIT_RETRIES--;
                }
            } else {
                CELL_WALK_FLAGS |= 0x10u;
                if (era == 0u) {
                    if (CELL_WALK_ERRCNT != 0xFFFFu) {
                        CELL_WALK_ERRCNT++;
                    }
                    CELL_COMMIT_RETRIES++;
                } else {
                    attn6 = 7u;
                }
            }
        } else {
            if ((CELL_COMMIT_12 & 4u) != 0u) {
                CELL_WALK_FLAGS |= 0x08u;
            }
            if (era == 0u) {
                if (CELL_WALK_ERRCNT != 0xFFFFu) {
                    CELL_WALK_ERRCNT++;
                }
                CELL_COMMIT_RETRIES++;
            }
            attn6 = 7u;
            CELL_LVL_EXPECTED = CELL_LVL_REPORTED;
        }
        /* the echo check: the staged value vs the 0x0A echo */
        attn = attn6;
        if (pair_get(&CELL_VALUE, &CELL_VALUE_COMPL) != CELL_ECHO_0A) {
            CELL_WALK_FLAGS |= (CELL_STAGE_SUB == STATE_ERA) ? 0x20u : 0x40u;
            attn = 7u;
        }
        if (CELL_RXERR_CNT != 0u) {
            CELL_RXERR_CNT--;
        }
        if ((uint8_t)(era ^ era_c) != 0xFFu) {
            s_pair_faults++;                              /* D2: fault 0x226 */
        }
    }
    if (CELL_STALL_CNT != 0u && !stale) {                 /* every non-stale path */
        CELL_STALL_CNT--;
    }
    CELL_ERA_BASE = CELL_REPLY_STAMP;                     /* re-base on the exchange */
    CELL_ATTENTION = attn;
    return stale_in_7;
}

/* ── staging confirm (stock 0x6C742) ────────────────────────────────────── */

static bool stage_confirm(void)
{
    uint8_t v;
    uint8_t vc;

    if (CELL_STAGE_SUB == STATE_ERA) {
        v = CELL_VALUE;
        vc = CELL_VALUE_COMPL;
        if (v != CELL_ECHO_0A) {
            if (CELL_RETRIES != 0u) {
                CELL_RETRIES--;
                level_prog();
                goto check;
            }
            CELL_WALK_FLAGS |= 0x04u;
        }
        v = CELL_VALUE;
        if ((uint16_t)((uint16_t)v * 200u) != CELL_PERIOD) {
            CELL_PERIOD = (uint16_t)((uint16_t)v * 200u);
            guards_compute();
        }
        CELL_STAGE_SUB = 0u;
    } else {
        v = CELL_PENDING;
        vc = CELL_PENDING_COMPL;
        CELL_STAGE_SUB = STATE_ERA;
        pair_set(&CELL_VALUE, &CELL_VALUE_COMPL, CELL_PENDING);
    }
check:
    if ((uint8_t)(v ^ vc) != 0xFFu) {
        s_pair_faults++;                                  /* D2: fault 0x226 */
    }
    CELL_WALK_SUBST = CELL_STAGE_SUB;                     /* bench mirror */
    return CELL_STAGE_SUB == STATE_ERA;
}

/* ── stage a period (stock 0x6C48C) ─────────────────────────────────────── */

uint8_t sup_walk_stage(uint16_t period, uint8_t tries)
{
    uint8_t v;

    if (CELL_STAGE_SUB != 0u) {
        return 1u;
    }
    if (period >= WALK_PERIOD_MAX) {
        return 2u;
    }
    CELL_RETRIES = (uint8_t)(tries - 1u);
    v = (uint8_t)(((uint32_t)period * 0x148u) >> 16);
    if (CELL_WALK_STATE == STATE_IDLE) {
        CELL_STAGE_SUB = STATE_ERA;
        pair_set(&CELL_VALUE, &CELL_VALUE_COMPL, v);
        CELL_WALK_STATE = STATE_SAMPLE;
        level_prog();
    } else {
        CELL_PENDING_COMPL = (uint8_t)~v;
        CELL_STAGE_SUB = STATE_SAMPLE;
        CELL_PENDING = v;
    }
    CELL_WALK_SUBST = CELL_STAGE_SUB;
    return 0u;
}

/* ── resend / token arm (stock 0x6C720 / 0x6C802) ───────────────────────── */

/* returns true when busy (walk state nonzero): nothing sent */
bool sup_walk_resend(void)
{
    if (CELL_WALK_STATE != STATE_IDLE) {
        return true;
    }
    CELL_WALK_STATE = STATE_SAMPLE;
    CELL_ATTENTION = 0x19u;
    level_prog();
    return false;
}

static bool token_arm(uint32_t token, uint8_t ctx)
{
    if (CELL_WALK_STATE != STATE_IDLE) {
        return true;
    }
    CELL_WALK_STATE = STATE_SAMPLE;
    CELL_DUTY_CTX_COMPL = (uint8_t)~ctx;
    CELL_TOKEN_STAGED = token;                            /* 0x92414 */
    CELL_DUTY_CTX = ctx;
    duty_prog();
    return false;
}

/* ── walk task (stock 0x6C63E) ──────────────────────────────────────────── */

static void walk_task(void)
{
    bool tail = false;

    if (CELL_WALK_STATE == STATE_SAMPLE) {
        if (elapsed_now() >= CELL_ERA_DEADLINE) {
            if (reply_processor() != 0u) {
                CELL_WALK_STATE = STATE_ERA;
            } else {
                tail = true;
            }
        }
    } else if (CELL_WALK_STATE == STATE_ERA) {
        era_stepper();
        (void)reply_processor();
        tail = true;
    } else {
        era_stepper();
        tail = true;
    }
    if (tail) {
        if (CELL_STAGE_SUB == 0u || !stage_confirm()) {
            CELL_WALK_STATE = STATE_IDLE;
        } else {
            CELL_WALK_STATE = STATE_SAMPLE;
        }
    }

    /* the output gate (0x6C63E tail) */
    if (CELL_LVL_REPORTED < 6u || CELL_GRANT == 0u) {
        CELL_CERTIFIED = 0u;
    } else {
        CELL_CERTIFIED = 1u;
        if (CELL_LVL_REPORTED < WALK_LEVEL_MAX) {
            CELL_TWO_PASS = 0u;
        } else {
            CELL_TWO_PASS++;
            if (CELL_TWO_PASS == 2u) {
                CELL_CERT_EVENTS |= 0x2000u;
            }
        }
    }
}

/* ── r02u THE DESCENT RECORDER (diagnostic; no wire effect) ─────────────
 * Armed by the first 0x8D136(0) (phase 0x80's pass: target 0). From then
 * on, every monitor pass that reaches the FSM switch AND either arms
 * (token / resend, armed or busy) or sees the reported level, the expected
 * level, the attention or the FSM change, writes one entry, until DSC_N:
 * µs since entry, FSM, budget 0x9417D, reported / expected level,
 * attention, the arm's kind (0x8D020's 0x9417E or the 'E' context), result
 * (bits 7:6: 1 armed / 2 busy / 0 none) | polled-fallback walk deliveries
 * since the previous entry (bits 5:0, saturating), W2 error count. Fits
 * the stack guard (12 bytes x 24). Read once after boot. */
#define DSC_N 24u
typedef struct {
    uint32_t t_us;
    uint8_t fsm, budget, rep, exp;
    uint8_t attn, kind, res_poll, w2err;
} dsc_rec_t;                             /* 12 bytes */
dsc_rec_t s_dsc[DSC_N];
volatile uint8_t s_dsc_n;
static uint8_t s_dsc_on;
static uint8_t s_dsc_kind, s_dsc_res;
static uint8_t s_dsc_prev[4];            /* fsm, rep, exp, attn of the last entry */
static uint32_t s_dsc_poll0;
extern volatile uint32_t s_walk_poll_count __attribute__((weak));

static uint32_t dsc_poll(void) { return (&s_walk_poll_count != 0) ? s_walk_poll_count : 0u; }

static void dsc_arm(void)
{
    if (s_dsc_on == 0u) {
        s_dsc_on = 1u;
        s_dsc_n = 0u;
        s_dsc_poll0 = dsc_poll();
        s_dsc_prev[0] = 0xFFu;           /* force the first entry */
    }
}

static void dsc_note(uint8_t kind, bool busy)
{
    s_dsc_kind = kind;
    s_dsc_res = busy ? 2u : 1u;
}

static void dsc_record(void)
{
    if (s_dsc_on == 0u || s_dsc_n >= DSC_N) {
        return;
    }
    const uint8_t f = CELL_OUTER_FSM, r = CELL_LVL_REPORTED,
                  x = CELL_LVL_EXPECTED, a = CELL_ATTENTION;
    if (s_dsc_res == 0u && f == s_dsc_prev[0] && r == s_dsc_prev[1] &&
        x == s_dsc_prev[2] && a == s_dsc_prev[3]) {
        return;
    }
    dsc_rec_t *e = &s_dsc[s_dsc_n];
    const uint32_t p = dsc_poll();
    const uint32_t dp = p - s_dsc_poll0;
    e->t_us = plat_frt_us() - plat_boot_tcnta();
    e->fsm = f;
    e->budget = CELL_S8_BUDGET;
    e->rep = r;
    e->exp = x;
    e->attn = a;
    e->kind = s_dsc_kind;
    e->res_poll = (uint8_t)((uint8_t)(s_dsc_res << 6) | (dp > 63u ? 63u : dp));
    e->w2err = sup_job_err_get(JOB_LEVEL);
    s_dsc_poll0 = p;
    s_dsc_prev[0] = f; s_dsc_prev[1] = r; s_dsc_prev[2] = x; s_dsc_prev[3] = a;
    s_dsc_n = (uint8_t)(s_dsc_n + 1u);
}

/* ── the level target (stock 0x8D136 / 0x8D108) ─────────────────────────── */

void sup_walk_target_set(uint8_t t)                      /* 0x8D136 */
{
    uint8_t d = CELL_LVL_REPORTED;

    if (t == 0u) {
        dsc_arm();                                       /* r02u */
        CELL_TARGET = 0u;
        CELL_TARGET_COMPL = 0xFFu;
    } else if (t == 5u) {
        CELL_TARGET = 5u;
        CELL_TARGET_COMPL = 0xFAu;
        if (CELL_LVL_REPORTED < 5u) {
            d = (uint8_t)(5u - CELL_LVL_REPORTED);
        }
    } else {
        CELL_TARGET = 3u;
        CELL_TARGET_COMPL = 0xFCu;
        if (CELL_LVL_REPORTED > 4u) {
            d = (uint8_t)(CELL_LVL_REPORTED - 4u);
        }
    }
    CELL_S8_BUDGET = 0u;
    if (d != 0u) {
        CELL_S8_BUDGET = (uint8_t)(WALK_S8_BASE + d + 2u);
    }
    CELL_S8_FLAG = 0u;
}

/* 0x8D1BA: the budget, with its side effect — at budget 1 it raises the
 * 0x93B96 flag the 0x8D020 law consumes (budget < 2 && flag → budget 0) */
uint8_t sup_walk_budget(void)
{
    if (CELL_S8_BUDGET == 1u) {
        CELL_S8_FLAG = 1u;
    }
    return CELL_S8_BUDGET;
}

bool sup_walk_target_reached(uint8_t t)                 /* 0x8D108 */
{
    if (CELL_LVL_REPORTED != t) {
        if (t == 5u) {
            if (CELL_LVL_REPORTED < 5u) {
                return false;
            }
        } else {
            if (t != 3u) {
                return false;
            }
            if (CELL_LVL_REPORTED > 4u) {
                return false;
            }
        }
    }
    return true;
}

/* ── the state-8 law (stock 0x8D020) ────────────────────────────────────── */

static void state8_law(void)
{
    const uint8_t t = CELL_TARGET;
    const uint8_t tc = CELL_TARGET_COMPL;
    bool busy;

    if (CELL_S8_BUDGET < 2u) {
        if (CELL_S8_FLAG != 0u) {
            CELL_S8_FLAG = 0u;
            CELL_S8_BUDGET = 0u;
        }
    } else if (!sup_walk_target_reached(t)) {
        CELL_S8_BUDGET--;
    } else {
        CELL_S8_BUDGET = 0u;
    }

    if (CELL_ATTENTION == 7u) {
        CELL_S8_KIND = 0u;
        busy = sup_walk_resend();
    } else if (t == 0u || t < CELL_LVL_REPORTED) {
        CELL_S8_KIND = 7u;
        busy = token_arm(sup_token_compute(CELL_SELECTOR), CTX_STEADY);
    } else {
        CELL_S8_KIND = 0x19u;
        busy = sup_walk_resend();
    }
    dsc_note(CELL_S8_KIND, busy);                         /* r02u */
    if (busy && CELL_S8_BUDGET > 1u) {
        CELL_S8_BUDGET++;
    }
    if ((uint8_t)(t ^ tc) != 0xFFu) {
        s_pair_faults++;                                  /* D2: fault 0x226 */
    }
}

/* ── the one-shot enable (0x873F0:43-47) ────────────────────────────────── */

/* The stock's hardware-condition gate (0x68BE0: 0xFFFF1001 bit 6). Weak
 * default "condition clear" until the platform supplies it. */
bool plat_hwcond(void);
__attribute__((weak)) bool plat_hwcond(void)
{
    return false;
}

static void one_shot_enable(void)
{
    if (CELL_EN_ONESHOT == 0u && CELL_LVL_REPORTED < WALK_LEVEL_MAX &&
        !plat_hwcond()) {
        sup_enable_request(true);                         /* 0x6C612(1) */
        CELL_EN_ONESHOT = 1u;
    }
}

/* ── the FSM write helpers (the stock's 0x6BD68 pair stores) ───────────── */

static void fsm_set(uint8_t st)
{
    CELL_OUTER_FSM = st;
    CELL_OUTER_FSM_C = (uint8_t)~st;
}

static void boot_flag_clear(void)
{
    CELL_BOOT_FLAG = 0u;
    CELL_BOOT_FLAG_C = 0xFFu;
}

/* ── the FSM request (stock 0x87766; bytes 0x87766..0x877BE) ───────────── */

/* The ONLY caller is the mode sequencer 0x85160 (sup_ladder.c): 0x23 after
 * the ladder's 0xAA; 0x12 (sub 0xF1) or 0x23 on retry exhaustion. Under the
 * 0x9A428 lock: s = 0x6BDCA(0x95010); if t != s and s != 0 the state moves
 * to t (complement ~t), the previous state goes to 0x93B93, and for t in
 * {0x23, 0x12, 0} the entry flag 0x9500E is raised (0x94E86 = 0xFE). */
void sup_monitor_request(uint8_t t)
{
    uint32_t prev = plat_sr_i();
    uint8_t s;

    if (prev < 1u) {
        plat_sr_set(1u);                                  /* 0x9A428 (cal 0x41174 = 0x10) */
    }
    s = pair_get(&CELL_OUTER_FSM, &CELL_OUTER_FSM_C);
    if (t != s && s != 0u) {
        CELL_OUTER_FSM_C = (uint8_t)~t;
        CELL_FSM_RETURN = s;
        CELL_OUTER_FSM = t;
        if (t == SUP_FSM_DUTY || t == SUP_FSM_REARM || t == SUP_FSM_BOOT) {
            CELL_BOOT_FLAG = 1u;
            CELL_BOOT_FLAG_C = 0xFEu;
        }
    }
    plat_sr_set(prev);                                    /* 0x9A3FC */
}

/* ── the 'E' walk rule (stock 0x8CEDC; bytes 0x8CEDC..0x8CFEE) ──────────── */

/* One armed shot per idle pass. While the reported level is above 0 the
 * rule is the steady token (context 0x1E); at level 0 it walks the
 * contexts 0 → 7 → 0x19 → 0 …, returning to 0x1E after every successful
 * arm (0x93B97 remembers the last context). Context 0x19 arms the INVERTED
 * token: the caller inverts (0x8CF88 `not r14,r14`) and the W1 builder
 * sends 0x92414 verbatim. Attention 7 enters at 0x2A: the 84 resend
 * 0x6C720. A busy arm keeps the state for the next pass. */
static void e_rule(void)
{
    uint32_t tok = sup_token_compute(CELL_SELECTOR);     /* 0x7EE8A (D4) */
    uint8_t s;
    uint8_t sc;
    bool again;

    if (CELL_ATTENTION == 7u) {
        s = 0x2Au;
        sc = 0xD5u;
    } else {
        s = pair_get(&CELL_E_STATE, &CELL_E_STATE_C);
        sc = CELL_E_STATE_C;
    }
    do {
        bool busy;

        again = false;
        switch (s) {
        case CTX_STEADY:
            if (CELL_LVL_REPORTED != 0u) {
                (void)token_arm(tok, CTX_STEADY);        /* state stays 0x1E */
            } else {
                const uint8_t last = CELL_E_LAST;
                s = (last == 0u) ? CTX_ATTN : (last == CTX_ATTN) ? CTX_INV : 0u;
                sc = (uint8_t)~s;
                CELL_E_LAST = CTX_STEADY;
                again = true;
            }
            continue;
        case 0x00u:
            busy = token_arm(tok, 0x00u);
            break;
        case CTX_ATTN:
            busy = token_arm(tok, CTX_ATTN);
            break;
        case CTX_INV:
            tok = ~tok;                                   /* 0x8CF88: the caller inverts */
            busy = token_arm(tok, CTX_INV);
            break;
        case 0x2Au:
            busy = sup_walk_resend();                     /* 0x6C720 */
            break;
        default:
            /* 0x94F84 |= 0x40 / 0x94DFC &= ~0x40: lamp bits (D2) */
            s = 0x2Au;
            sc = 0xD5u;
            again = true;
            continue;
        }
        dsc_note(s, busy);                                /* r02u */
        if (!busy) {
            CELL_E_LAST = s;
            s = CTX_STEADY;
            sc = (uint8_t)~CTX_STEADY;
        }
    } while (again);
    CELL_E_STATE = s;
    CELL_E_STATE_C = sc;
    if ((uint8_t)(s ^ sc) != 0xFFu) {
        s_pair_faults++;                                  /* D2: fault 0x226 */
    }
}

/* ── the 0x34 state (stock 0x8799C; bytes 0x8799C..0x87A1C) ─────────────── */

static void state34(void)
{
    if (CELL_FSM_RETURN == SUP_FSM_DUTY) {
        if (CELL_LVL_REPORTED == 0u) {
            CELL_FSM_RETURN = pair_get(&CELL_OUTER_FSM, &CELL_OUTER_FSM_C);  /* = 0x34 */
            fsm_set(SUP_FSM_PROCEED);                     /* 'E' (0xBA) */
            CELL_FSM_93B99 = 1u;
            (void)sup_walk_stage(0x1838u, 2u);            /* 0x6C48C(0x1838, 0xE7C7, 2) */
        } else {
            state8_law();                                 /* 0x8D020 */
        }
        return;
    }
    if (CELL_LVL_REPORTED == 0u) {
        CELL_FSM_RETURN = pair_get(&CELL_OUTER_FSM, &CELL_OUTER_FSM_C);
        fsm_set(SUP_FSM_PROCEED);
    }
    e_rule();                                             /* 0x8CEDC */
}

/* the 0x34 path (0x875EE): lamp bits 0x94F84 |= 0x800 / 0x94DFC &= ~0x800
 * (D2, not ported), state 0x34 (0xCB), 0x93B93 = the state left, 0x8799C */
static void go_34(uint8_t from)
{
    fsm_set(SUP_FSM_ADVANCE);
    CELL_FSM_RETURN = from;
    state34();
}

/* ── the monitor body (stock 0x873F0; 2.5 ms, every mode, no lead-in) ───── */

void sup_monitor_poll(void)
{
    uint8_t st;

    plat_alive(0x02u);                                    /* S11 breadcrumb */
    /* (no per-pass token stage: the stock writes 0x92414 only in 0x6C802,
     *  at arm time — r02f review) */
    walk_task();                                          /* 0x876C4 → 0x6C63E */
    one_shot_enable();                                    /* 0x876C8 → 0x6C612 */
    if (CELL_WALK_STATE != STATE_IDLE) {
        return;
    }
    st = pair_get(&CELL_OUTER_FSM, &CELL_OUTER_FSM_C);   /* 0x874FE: 0x6BDCA */
    s_dsc_kind = 0u;                                      /* r02u */
    s_dsc_res = 0u;
    switch (st) {
    case SUP_FSM_PROCEED:                                 /* 'E' 0x87502 */
        if (CELL_LVL_REPORTED >= 5u) {
            go_34(st);
        } else {
            e_rule();                                     /* 0x8CEDC, not 0x8D020 */
        }
        break;
    case SUP_FSM_SUPERVISE:                               /* 8: 0x87526 → 0x87688 */
        /* The level target comes only from the stock's own writers: FSM
         * states 0 / 0x12 / 0x23 and the ladder (sup_ladder.c) phases 1
         * (5), 0x10 (3) and 0x80 (5, then 0). */
        state8_law();                                     /* 0x8D020 */
        break;
    case SUP_FSM_REARM:                                   /* 0x12: 0x87542 */
        if (pair_get(&CELL_BOOT_FLAG, &CELL_BOOT_FLAG_C) != 0u) {
            boot_flag_clear();
            sup_walk_target_set(5u);                      /* 0x8D136(5) */
        }
        state8_law();                                     /* no exit of its own */
        break;
    case SUP_FSM_DUTY:                                    /* 0x23: 0x87570 */
        if (pair_get(&CELL_BOOT_FLAG, &CELL_BOOT_FLAG_C) != 0u) {
            boot_flag_clear();
            sup_walk_target_set(0u);                      /* 0x8D136(0) */
        }
        /* (0x9384C |= 0x800: trace, D2) */
        if (sup_walk_budget() == 1u) {                    /* 0x8D1BA */
            go_34(st);
            break;
        }
        if (sup_walk_target_reached(0u) &&                /* 0x8D108(0): level == 0 */
            sup_walk_stage(0x1838u, 2u) == 0u) {          /* 0x6C48C(0x1838, 0xE7C7, 2) */
            fsm_set(SUP_FSM_PROCEED);                     /* 'E' (0xBA) */
            CELL_FSM_93B99 = 1u;
            CELL_FSM_RETURN = st;                         /* 0x23 */
            break;                                        /* no 0x8D020 this pass */
        }
        state8_law();                                     /* drives the level to 0 */
        break;
    case SUP_FSM_ADVANCE:                                 /* 0x34: 0x87536 → 0x87616 */
        state34();
        break;
    case SUP_FSM_BOOT:                                    /* 0: 0x8761E */
        if (pair_get(&CELL_BOOT_FLAG, &CELL_BOOT_FLAG_C) != 0u) {
            boot_flag_clear();
            sup_walk_target_set(5u);                      /* 0x8D136(5) */
            if (CELL_PERIOD != WALK_ERA_06) {
                (void)sup_walk_stage(WALK_ERA_06, 2u);    /* 0x6C48C(0x4B0, 0xFB4F, 2) */
            }
        }
        CELL_FSM_RETURN = SUP_FSM_BOOT;
        fsm_set(SUP_FSM_SUPERVISE);
        CELL_FSM_93B99 = 0u;
        if (CELL_WALK_STATE != STATE_IDLE) {
            return;
        }
        state8_law();
        break;
    default:                                              /* 0x87690 */
        /* lamp bit 0x40 (D2); back to the state left, or 0 */
        fsm_set(CELL_FSM_RETURN != 0u ? CELL_FSM_RETURN : SUP_FSM_BOOT);
        break;
    }
    dsc_record();                                         /* r02u */
}

/* ── the W2 level builder (stock 0x91190) ───────────────────────────────── */

void sup_level_job(void)
{
    uint16_t *st;
    uint8_t v;

    if (!sup_job_check_free(JOB_LEVEL)) {
        return;
    }
    v = pair_get(&CELL_VALUE, &CELL_VALUE_COMPL);
    if (v == WALK_VAL_INIT) {
        return;                                           /* 0x3F never on the wire */
    }
    st = sup_job_table[JOB_LEVEL].tx_stage;
    st[0] = (uint16_t)(0x8400u | (uint16_t)v);
    st[1] = 0x0A00u;
    st[2] = 0x1200u;
    st[3] = 0x0C00u;
    CELL_STAGED_84 = v;
    (void)sup_job_queue(JOB_LEVEL);
}

/* ── the W1 duty builder (stock 0x91086) ────────────────────────────────── */

void sup_duty_job(void)
{
    uint32_t tok;
    uint16_t *st;

    if (!sup_job_check_free(JOB_DUTY)) {
        return;
    }
    /* VERBATIM (walkdata review #3): the stock's 0x8CEDC inverts the token
     * BEFORE arming (ctx 0x19 → 0x6C802 stages ~token in 0x92414); 0x6C1BA
     * and 0x91086 send 0x92414 as-is. The old builder-side inversion would
     * double-invert the moment 0x8CEDC is ported — a port must pass ~token
     * to token_arm. (Unreachable in r02f: only CTX_STEADY is armed.) */
    tok = CELL_TOKEN_STAGED;
    st = sup_job_table[JOB_DUTY].tx_stage;
    st[0] = (uint16_t)(0x8800u | ((tok >> 24) & 0xFFu));
    st[1] = (uint16_t)(0x8800u | ((tok >> 16) & 0xFFu));
    st[2] = (uint16_t)(0x8800u | ((tok >> 8) & 0xFFu));
    st[3] = (uint16_t)(0x8800u | (tok & 0xFFu));
    st[4] = 0x1200u;
    st[5] = 0x0C00u;
    st[6] = 0x0A00u;
    CELL_TOKEN_LAST = tok;
    (void)sup_job_queue(JOB_DUTY);
}

/* ── the walk timer service (stock 0x6C1BA, with D1's closed loop) ──────── */

void sup_walk_isr(void)
{
    plat_alive(0x06u);                                    /* S11 breadcrumb */
    /* the WDT is fed by the platform ISR first (0x6C244 → 0x920C6) */
    if (!s_shot_armed) {
        return;
    }
    {
        int32_t rem = (int32_t)(s_shot_at - plat_frt_us());

        if (rem > (int32_t)WALK_SHOT_SPIN_US) {
            plat_walktmr_arm_us((uint32_t)rem);           /* D1: not yet — re-arm */
            return;
        }
        if (rem > 0) {
            /* D1: the last < 40 µs by TCNTA, not by a ~28 µs tick — the
             * token's usual slot is the window CLOSE (0x93840), where a
             * late tick would land outside the window */
            plat_walktmr_wait_until(s_shot_at);
        }
    }
    s_shot_armed = false;
    if (CELL_JOB_MODE == 0u) {
        sup_duty_job();                                   /* 0x91086(0x92414) */
    } else {
        sup_level_job();                                  /* 0x91190(value) */
    }
    sup_job_engine_pump();
}

uint32_t sup_walk_shot_at(void)
{
    return s_shot_armed ? s_shot_at : 0u;
}

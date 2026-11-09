#include "bsw/sup/sup_engage.h"

#include <string.h>

/* E60p (spec 22): the commissioning walk, verbatim from the top-down decode.
 *
 * Stock machinery mirrored here (addresses in comments are the stock's):
 *   - init          fault_manager_init 0x87732 -> park_pwm_init 0x6C12C
 *   - outer FSM     0xFFF95010: 0 -> 8 -> 0x12 -> 0x23 -> 0x45 -> 0x34 -> steady
 *   - era staging   park_pwm_learn_value 0x6C48C + confirm 0x6C742 (ping-pong)
 *   - reply check   park_monitor_feedback 0x6C1EA (echo / commit mask / expected)
 *   - timer cadence ISR 0x9106C -> park_pwm_service 0x6C1BA, one-in-flight
 *                     (check-free 0x93B8E), era dwell = value*200 + 2400 ms
 *   - duty token    ~(table[sel] ^ 0xAAAA ^ 0xB32F8A6A), contexts 0->7->0x19
 *                     while level==0 (0x19 sends ~token), 0x1E steady (0x6BF9C)
 *
 * Scrambler table read from the stock ELF (flash 0x4526C). */

static const uint32_t S_TOKEN_TABLE[16] = {
    0xB3DF2F3Fu, 0xFC906070u, 0xA5C93929u, 0xEA867666u,
    0x3955A5B5u, 0x761AEAFAu, 0x2F43B3A3u, 0x600CFCECu,
    0x9EF20212u, 0xD1BD4D5Du, 0x88E41404u, 0xC7AB5B4Bu,
    0x14788898u, 0x5B37C7D7u, 0x026E9E8Eu, 0x4D21D1C1u,
};
#define S_TOKEN_CONST 0xB32F8A6Au

static uint32_t token(uint8_t sel)
{
    return ~(S_TOKEN_TABLE[sel & 0xFu] ^ 0xAAAAu ^ S_TOKEN_CONST);
}

/* Era arithmetic (0x6C48C): dwell_ms = value*200 + 2400 (the 0x960 tick tail) */
static uint32_t era_dwell_ms(uint8_t value)
{
    return (uint32_t)value * 200u + 2400u;
}

/* Stock constants */
#define VAL_INIT 0x3Fu      /* 0x6C12C boot init */
#define VAL_06   0x06u      /* outer-FSM state 0 stages this (0x4B0 period) */
#define VAL_1F   0x1Fu      /* state 0x23 stages this (0x1838 period) */
#define LVL_TARGET_S0  5u   /* mode_req_set(5) at states 0 and 0x12 */
#define LVL_PROCEED    5u   /* state 0x45 proceed gate */
#define LVL_MAX        7u
#define S8_BUDGET_MS   20000u   /* mode_transition_supervise budget [I: length] */
#define S23_TIMEOUT_MS 20000u   /* 0x8D1BA duty-era timeout [I: length] */
#define CONFIRM_RETRY  3u       /* 0x6C742 retry bound */
#define TX_TIMEOUT_MS  10u      /* in-flight safety clear (one pass + slack) */
#define ZERO_ROUNDS    5u       /* S23: duty rounds at level 0 before staging 0x1F [I] */
#define ASK_PASS       21u      /* the ask's proven timing (E60i); the stock arms on
                                 * the monitor's first pass — one variable at a time */      /* E60r: the request cell arms at pass 21 (+105 ms) —
                                 * the E60i-proven ask timing, the ONLY one that ever
                                 * granted; pass-1 asks are the E60m/E16 reset class
                                 * in our context (spec 22 READY law, open question 3) */

static sup_eng_status_t s;
static uint32_t s_now;          /* last tick's now_ms */
static uint32_t s_era_start;    /* ms stamp of the current era */
static uint32_t s_tx_ms;        /* ms stamp of the in-flight job */
static bool s_force;            /* FE13 sub-11 lab bypass */
static uint8_t s_prev_state;    /* 0x34's "previous was 0x23" check */
static bool s_34_retried;       /* the one re-request of the 0x1F era */
static uint8_t s_zero_rounds;    /* S23: duty rounds with the level settled at 0 */
static uint16_t s_pass;         /* E60r: 5 ms passes since boot */

void sup_engage_boot(void)
{
    memset(&s, 0, sizeof s);
    s.state = SUP_ENG_S0;
    s.value = VAL_06;    /* E60aa: the wire starts at 06 (the decompile's
                          * cold-path fact); 0x3F is the cell init, never staged.
                          * S0's staging of 0x06 is then a no-op (v == s.value) */
    s.lvl_exp = LVL_MAX;      /* 0xFFF93BA2 = 7 at init */
    s.lvl = LVL_MAX;          /* 0xFFF93BA1 = 7 at init (boot-warm cell) */
    s.mode = 7u;              /* job mode: level (the init value rides W2) */
    s.ctx = 0u;
    s.enable_req = false;     /* E60r: arms at ASK_PASS — the E60i-proven timing.
                               * NR-1 holds (the ask and the walk are one system);
                               * only the TIMING retreats to our proven-safe cell */
    s_prev_state = SUP_ENG_IDLE;
    s_pass = 0u;
}

bool sup_engage_active(void)
{
    return s.state != SUP_ENG_IDLE;
}

const sup_eng_status_t *sup_engage_status(void)
{
    return &s;
}

void sup_engage_echo_bit0(bool echo_set)
{
    s.echo_grant = echo_set;
}

bool sup_engage_enable_bit(void)
{
    /* Stock law (0x91C86): bit 0 = the request cell, FORCED on every pass once
     * armed (unless the disable flag is set). The bench reset class was bit 0
     * without the rolling conversation (E60m proof), never bit 0 with it. */
    s.enable_sent = (s.enable_req && !s.enable_kill) || s_force;
    return s.enable_sent;
}

void sup_engage_kill(bool on)
{
    s.enable_kill = on;
}

/* Legacy adapters (lab/build plumbing) */
void sup_engage_arm(bool on)
{
    if (on && s.state == SUP_ENG_IDLE) {
        sup_engage_boot();
    } else if (!on) {
        s.state = SUP_ENG_IDLE;
        s.enable_req = false;
    }
}

void sup_engage_request_enable(bool on)
{
    s.enable_req = on;
}

void sup_engage_force_enable(bool on)
{
    s_force = on;
}

void sup_engage_enable_from_start(bool on)
{
    (void)on;                 /* the request cell is on from boot (E60p) */
}

/* Stage a new era value (0x6C48C's staging half): fills the pending slot; the
 * next tick promotes it into the W2 job (the ping-pong), the reply's 0x0A echo
 * confirms, and only then does the era clock restart. */
static void stage_value(uint8_t v)
{
    if (v == s.value) {
        return;
    }
    s.pending = v;
    s.staged = false;
    s.retries = 0u;
    s.mode = 7u;              /* level job while a value is in play */
}

static void era_started(void)
{
    s_era_start = s_now;
    s.era_ms = 0u;
}

/* The outer FSM (0xFFF95010) — stepped every tick; the one-shot states run
 * their action and advance immediately. */
static void fsm_step(void)
{
    switch (s.state) {
    case SUP_ENG_S0:
        /* mode_req_set(5) + the 0x06 era (0x6C48C(0x4B0,...)) */
        s.lvl_exp = LVL_TARGET_S0;
        stage_value(VAL_06);
        era_started();
        s.state = SUP_ENG_S8;
        break;
    case SUP_ENG_S8:
        /* mode_transition_supervise: target reached -> advance; budget spent ->
         * walk restart (0x6C720) and keep supervising */
        if (s.lvl >= LVL_TARGET_S0) {
            s.state = SUP_ENG_S12;
        } else if ((s_now - s_era_start) >= S8_BUDGET_MS) {
            stage_value(VAL_06);
            era_started();
        }
        break;
    case SUP_ENG_S12:
        s.lvl_exp = LVL_TARGET_S0;   /* mode_req_set(5) again */
        s.state = SUP_ENG_S23;
        if (s.pending == 0u) {       /* the duty era takes the wire on entry */
            s.mode = 0u;
            era_started();
        }
        break;
    case SUP_ENG_S23:
        /* DUTY era armed on entry: mode_req_set(0) drags the level to 0 */
        if (s.mode != 0u && s.pending == 0u) {
            s.lvl_exp = 0u;
            s.mode = 0u;             /* duty job takes the wire */
            era_started();
        }
        if (s.lvl == 0u && s.mode == 0u) {
            if (s.pending == 0u && ++s_zero_rounds >= ZERO_ROUNDS) {
                /* 0x8D108(0) && 0x6C48C(0x1838): the 0x1F era, staged once the
                 * level has SETTLED at 0 (the drag-down passes through) */
                stage_value(VAL_1F);
                s_prev_state = SUP_ENG_S23;
                s.state = SUP_ENG_S45;
            }
        } else {
            s_zero_rounds = 0u;
        }
        if (s.pending == 0u && s.mode == 0u &&
            (s_now - s_era_start) >= S23_TIMEOUT_MS) {
            /* 0x8D1BA timeout -> 0x34 directly */
            s_prev_state = SUP_ENG_S23;
            s.state = SUP_ENG_S34;
        }
        break;
    case SUP_ENG_S45:
        /* proceed gate: level >= 5 (warm chip passes instantly) */
        if (s.lvl >= LVL_PROCEED) {
            s.state = SUP_ENG_S34;
        }
        break;
    case SUP_ENG_S34:
        /* 0x8799C fault_state_advance: re-request the 0x1F era once if we came
         * from 0x23 with the level still 0; else steady supervision */
        if (s_prev_state == SUP_ENG_S23 && s.lvl == 0u && !s_34_retried) {
            s_34_retried = true;
            stage_value(VAL_1F);
            s_prev_state = SUP_ENG_IDLE;
            s.state = SUP_ENG_S45;
        } else {
            s.state = SUP_ENG_STEADY;
            s.mode = 0u;            /* steady = the duty token continues (spec 22
                                     * section 5: hundreds of consecutive rounds) */
        }
        break;
    default:
        break;
    }
}

/* Duty context cycling (0x6BF9C + 0x8CEDC): while the level is 0 the contexts
 * walk 0 -> 7 -> 0x19 (0x19 sends the COMPLEMENT token); once the level is
 * nonzero the steady context is 0x1E (plain token). */
static void duty_ctx_step(void)
{
    if (s.lvl == 0u) {
        if (s.ctx == 0u) {
            s.ctx = 7u;
        } else if (s.ctx == 7u) {
            s.ctx = 0x19u;
        } else if (s.ctx != 0x19u) {
            s.ctx = 0u;             /* returning to 0: restart the cycle */
        }
    } else {
        s.ctx = 0x1Eu;
    }
}

bool sup_engage_tick(uint32_t now_ms, uint16_t *words, uint8_t *n)
{
    uint32_t v;

    if (s.state == SUP_ENG_IDLE) {
        return false;
    }
    s_now = now_ms;
    s.era_ms = (uint16_t)(s_now - s_era_start);
    s_pass++;                          /* the ask's timing cell */
    if (s_pass == ASK_PASS) {
        s.enable_req = true;           /* the E60i-proven +105 ms ask */
    }
    /* E60ab: the hold DELETED — the stall decode proved the chip cannot
     * tolerate 80 ms of 84-class silence after the handshake arms its
     * commit/echo state machine (the RSPI-B wedge at pass 16). The stock
     * never holds its walk either — the "84|06 at the ask window" was a
     * timer-arithmetic coincidence, not a delay. Our 84|06 from pass 1
     * is the correct shape (continuous walk from the first service). */
    if (s.pending != 0u) {
        /* the staging ping-pong (0x6C742/0x6BD48): the pending slot promotes
         * ONE PASS after staging — pass 1 stages, pass 2 sends. The value
         * cell's prior contents (the 0x3F init) ride the first W2 job, the
         * stock's literal first register words. */
        s.value = s.pending;
        s.pending = 0u;
        era_started();
    }
    /* E60t (amended, stock-faithful): the walk runs from the FIRST pass —
     * the value cell rides its init value 0x3F in the W2 job (the stock's
     * literal first register words; accepted by this chip by construction —
     * the stock walks per-boot on this bench). S0's staging ping-pong
     * promotes 0x06 a few passes later, as the stock's does. This is the
     * conversation the pass-21 ask rides on (E60m's law) with no invented
     * probe and no early 84|06 (E60n's law). */

    /* in-flight discipline: skip while a job awaits its reply (0x93B8E);
     * a reply that never comes clears itself after the safety timeout */
    if (s.in_flight) {
        if ((s_now - s_tx_ms) <= TX_TIMEOUT_MS) {
            return false;
        }
        s.in_flight = false;
    }

    fsm_step();
    if (s.state == SUP_ENG_IDLE) {
        return false;
    }

    /* The era stepper (0x6C542): advance the era base when the dwell expires
     * and bump the expected level toward 7 (the duty twin drags it DOWN again
     * while the level reads > 6 — 0x6BF9C). */
    if (s.pending == 0u && s.era_ms >= era_dwell_ms(s.value)) {
        if (s.state == SUP_ENG_S8 || s.state == SUP_ENG_STEADY) {
            if (s.lvl_exp < LVL_MAX) {
                s.lvl_exp++;
            }
            era_started();
        }
        if (s.mode == 0u && s.lvl_exp > 6u) {
            s.lvl_exp--;
        }
    }

    /* Compose the job (park_pwm_service 0x6C1BA by job-mode cell 0xFFF93B9B) */
    if (s.mode == 7u) {
        /* W2 level job {84|value, 0A00, 1200, 0C00} — 0x91190; the value cell
         * rides as-is (the tick head does the ping-pong promote) */
        words[0] = (uint16_t)(0x8400u | (s.value & 0x3Fu));
        words[1] = 0x0A00u;
        words[2] = 0x1200u;
        words[3] = 0x0C00u;
        *n = 4u;
    } else {
        /* W1 duty job {88|t0..t3, 1200, 0C00, 0A00} — 0x91086 */
        duty_ctx_step();
        v = token(s.sel);
        if (s.ctx == 0x19u) {
            v = ~v;                    /* the complement-token dialect */
        }
        words[0] = (uint16_t)(0x8800u | ((v >> 24) & 0xFFu));
        words[1] = (uint16_t)(0x8800u | ((v >> 16) & 0xFFu));
        words[2] = (uint16_t)(0x8800u | ((v >> 8) & 0xFFu));
        words[3] = (uint16_t)(0x8800u | (v & 0xFFu));
        words[4] = 0x1200u;
        words[5] = 0x0C00u;
        words[6] = 0x0A00u;
        *n = 7u;
    }
    s.in_flight = true;
    s_tx_ms = s_now;
    s.jobs++;
    for (uint8_t i = 0u; i < *n; i++) {
        s.last_words[i] = words[i];
    }
    s.last_n = *n;
    return true;
}

static void harvest_fb(uint8_t fb)
{
    if ((fb & 0xFu) != s.sel) {
        /* the supervisor advanced its challenge — the token table follows */
    }
    s.sel = fb & 0xFu;
    s.lvl = (uint8_t)((fb >> 4) & 7u);
}

void sup_engage_tx_failed(void)
{
    s.in_flight = false;
}

void sup_engage_replies(const uint16_t *rx, uint8_t n)
{
    s.in_flight = false;

    if (n == 3u) {                       /* probe: echo [0], fb [1] */
        if ((rx[1] & 0xF900u) != 0xA800u) {
            s.bad++;
            return;
        }
        harvest_fb((uint8_t)(rx[1] & 0xFFu));
        s.lvl_echo = (uint8_t)(rx[0] & 0x3Fu);
    } else if (n == 4u) {                /* W2: echo [1], commit [2], fb [3] */
        if ((rx[3] & 0xF900u) != 0xA800u) {
            s.bad++;
            return;
        }
        s.status = (uint8_t)(rx[2] & 0xFFu);
        harvest_fb((uint8_t)(rx[3] & 0xFFu));
        s.lvl_echo = (uint8_t)(rx[1] & 0x3Fu);

        /* The three checks of 0x6C1EA, every reply:
         * (1) 0x0A echo == staged value (mismatch -> bump-and-resend)
         * (2) commit (0x12 & 5) vs the phase mask (0 settling / 1|4 re-target)
         * (3) expected vs reported level (mismatch -> expected := reported) */
        const bool retargeting = s.lvl_echo != s.value;
        const uint8_t commit = (uint8_t)(rx[2] & 0x05u);
        const bool commit_ok = retargeting ? (commit == 1u || commit == 4u)
                                           : (commit == 0u);
        if (!commit_ok) {
            s.commit_bad++;
        }
        if (s.lvl_exp != s.lvl) {
            s.lvl_exp = s.lvl;           /* resync the expectation */
        }
        if (s.lvl_echo == s.value) {
            s.echo_ok++;
            s.retries = 0u;
        } else {
            s.echo_bad++;
            /* 0x6C742/0x6C720: resend (bump) while the echo disagrees; after
             * the retry bound, give up on this value (flag) and let the era
             * stepper proceed — the stock's 0x93BA7 |= 4 class */
            if (++s.retries > CONFIRM_RETRY) {
                s.retries = 0u;
                s.lvl_exp = s.lvl;
            }
        }
    } else if (n == 7u) {                /* W1: commit [4], fb [5], echo [6] */
        if ((rx[5] & 0xF900u) != 0xA800u) {
            s.bad++;
            return;
        }
        s.status = (uint8_t)(rx[4] & 0xFFu);
        harvest_fb((uint8_t)(rx[5] & 0xFFu));
        s.lvl_echo = (uint8_t)(rx[6] & 0x3Fu);
        s.tokens++;
        /* drag-down (0x6BF9C): the expected level follows the chip down while
         * the duty era runs */
        if (s.lvl_exp > s.lvl) {
            s.lvl_exp = s.lvl;
        }
    }
}

/* ---- driver register sync: RETIRED emitters, kept for the lab readout ---- */

static sup_drv_status_t s_drv;

static bool     s_wj_on;
static uint8_t  s_wj_v8e;
static uint16_t s_wj_div;
static bool     s_sp_on;
static bool     s_cc_on;
static uint16_t s_cc_sp;
static bool     s_cb_on;

void sup_drv_write_arm(bool on, uint8_t v8e)
{
    if (on != s_wj_on || v8e != s_wj_v8e) {
        s_wj_div = 0u;
    }
    s_wj_on = on;
    s_wj_v8e = v8e;
}

void sup_drv_cyclic_arm(bool on, uint16_t sp)
{
    s_cc_on = on;
    s_cc_sp = sp & 0x7FFu;
}

void sup_drv_cyclic_boot(bool on)
{
    s_cb_on = on;
}

void sup_drv_setpoint_arm(bool on)
{
    if (on != s_sp_on) {
        s_wj_div = 0u;
    }
    s_sp_on = on;
}

void sup_drv_sync(bool on)
{
    (void)on;
}

void sup_drv_write_once(void)
{
}

void sup_drv_setpoint_once(void)
{
}

const sup_drv_status_t *sup_drv_status(void)
{
    return &s_drv;
}

bool sup_drv_poll(uint16_t *words, uint8_t *n)
{
    /* E52's captured 45-frame cyclic (the boot companion) — OFF in E60p: the
     * walk owns the wire cadence now, and the stock's own discipline keeps
     * bus-B traffic to its ten descriptors. Retained for lab arming only. */
    if (s_cb_on) {
        static const uint16_t STOCK_CYCLIC[45] = {
            0x881Au, 0x881Au, 0x881Au, 0x8812u, 0x8815u,
            0xA000u, 0xA000u, 0xA000u, 0xA000u, 0xA000u,
            0xE853u, 0xED89u, 0xED89u, 0xE853u, 0xE800u,
            0xA600u, 0xA600u, 0xA600u, 0xA600u, 0xA600u,
            0xF589u, 0xF589u, 0xF053u, 0xF000u, 0xF000u,
            0x6000u, 0x6000u, 0x6000u, 0x6000u, 0x6000u,
            0xC000u, 0xC000u, 0xC000u, 0xC000u, 0xC000u,
            0x6600u, 0x6600u, 0x6600u, 0x6600u, 0x6600u,
            0xD800u, 0xD800u, 0xD800u, 0xD800u, 0xD800u,
        };
        static uint8_t s_cb_div;
        if (++s_cb_div < 2u) {
            return false;
        }
        s_cb_div = 0u;
        for (uint8_t i = 0u; i < 45u; i++) {
            words[i] = STOCK_CYCLIC[i];
        }
        *n = 45u;
        s_drv.drv_jobs++;
        return true;
    }
    static uint16_t s_cc_div;
    if (s_cc_on) {
        if (++s_cc_div >= 400u) {
            static const uint16_t STOCK_CYCLIC[45] = {
                0x881Au, 0x881Au, 0x881Au, 0x8812u, 0x8815u,
                0xA000u, 0xA000u, 0xA000u, 0xA000u, 0xA000u,
                0xE853u, 0xED89u, 0xED89u, 0xE853u, 0xE800u,
                0xA600u, 0xA600u, 0xA600u, 0xA600u, 0xA600u,
                0xF589u, 0xF589u, 0xF053u, 0xF000u, 0xF000u,
                0x6000u, 0x6000u, 0x6000u, 0x6000u, 0x6000u,
                0xC000u, 0xC000u, 0xC000u, 0xC000u, 0xC000u,
                0x6600u, 0x6600u, 0x6600u, 0x6600u, 0x6600u,
                0xD800u, 0xD800u, 0xD800u, 0xD800u, 0xD800u,
            };
            s_cc_div = 0u;
            for (uint8_t i = 0u; i < 45u; i++) {
                words[i] = STOCK_CYCLIC[i];
            }
            *n = 45u;
            s_drv.drv_jobs++;
            return true;
        }
        return false;
    }
    if ((!s_wj_on && !s_sp_on) || s.state == SUP_ENG_IDLE) {
        return false;
    }
    if (++s_wj_div < 400u) {
        return false;
    }
    s_wj_div = 0u;
    if (s_wj_on) {
        {
            static const uint8_t MODE[4] = { 0x1Au, 0x1Au, 0x12u, 0x15u };
            for (uint8_t r = 0u; r < 10u; r++) {
                for (uint8_t c = 0u; c < 4u; c++) {
                    uint16_t w;
                    switch (r) {
                    case 0u: case 8u: w = (uint16_t)(0x8E00u | s_wj_v8e); break;
                    case 1u: case 9u: w = (uint16_t)(0x9000u | 0x21u);   break;
                    case 2u: w = (uint16_t)(0x9600u | 0x55u);   break;
                    case 3u: w = (uint16_t)(0x8800u | MODE[c]); break;
                    case 4u: w = 0x2800u; break;
                    case 5u: w = 0x3000u; break;
                    case 6u: w = 0x2E00u; break;
                    default: w = 0x3600u; break;
                    }
                    words[4u * r + c] = w;
                }
            }
        }
        *n = DRV_WRITE_WORDS;
        s_drv.drv_jobs++;
        s_drv.drv_writes++;
        return true;
    }
    {
        static const uint8_t MODE5[5] = { 0x1Au, 0x1Au, 0x1Au, 0x12u, 0x15u };
        for (uint8_t c = 0u; c < 5u; c++) {
            words[c] = (uint16_t)(0x8800u | MODE5[c]);
        }
        for (uint8_t c = 0u; c < 5u; c++) {
            words[5u + c] = 0xA000u;
        }
    }
    *n = 10u;
    s_drv.drv_jobs++;
    s_drv.drv_sp++;
    return true;
}

void sup_drv_replies(const uint16_t *rx, uint8_t n)
{
    for (uint8_t i = 0u; i < n && i < SUP_DRV_READ_WORDS; i++) {
        if ((rx[i] & 0xFE00u) == 0xA800u) {
            s_drv.drv_a8++;
        } else if ((rx[i] & 0xFE00u) == 0xAA00u) {
            s_drv.drv_aa++;
        } else if ((rx[i] & 0xFF00u) == 0xA200u) {
            s_drv.drv_a2++;
        } else {
            s_drv.drv_bad++;
        }
    }
}

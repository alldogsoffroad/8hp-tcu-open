#include "bsw/sup/sup_engage.h"

#include <string.h>

/* Scrambler table read from the stock ELF (flash 0x4526C, 16 entries) and the
 * constant (flash 0x7F424 ^ 0xAAAA == flash 0x8D20C) — the duty job payload is
 * ~(table[sel] ^ 0xAAAA ^ const). Stock walker 0x7EE8A, reply parsers 0x910EE /
 * 0x911D8, harvest 0x6C1EA. */
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

/* One register job every N passes. E18 (spec 03 section 6.2o): the stock paces its
 * register jobs with calibration dwells (level*200 + 2400 timer ticks, engine
 * 0x6BF9C) - millisecond-class, and its commissioning exchange completes in the
 * first passes. Our bench-era 300 ms pacing put HOLD at ~0.7-1 s, exactly where
 * the rail drops (E17: rail up through +320 ms of our stream, down by +1 s).
 * 2 passes = 10 ms per rung: probe/token/level/confirm complete inside the
 * proven rail-up window. */
#define JOB_PERIOD 2u
#define SYNC_JOBS  2u
#define LEVEL_MAX  7u      /* the stock's target cell caps at 7 (0x6C542) */
#define LEVEL_RUN  31u     /* the engaged commanded level: 0x1F, live-observed
                            * (2026-10-30 shadow 0x9054E/F = 1f 1f; the 841F
                            * one-shot latched AA1F on the bench) */
#define ECHO_RETRY 3u     /* 0x6C742: resend the level job while the 0x0A echo
                           * disagrees with the command */

enum { ST_IDLE = 0u, ST_SYNC = 1u, ST_RAMP = 2u, ST_HOLD = 3u };

static sup_eng_status_t s;
static uint16_t s_div;
static bool s_next_level;      /* ramp alternates duty / level jobs */

void sup_engage_arm(bool on)
{
    if (on) {
        memset(&s, 0, sizeof s);
        s.state = ST_SYNC;
        s.lvl_cmd = 1u;
        s_div = JOB_PERIOD - 1u;         /* first job on the next pass */
        s_next_level = false;
    } else {
        s.state = ST_IDLE;
        s.enable_req = false;
        s.enable_sent = false;
        s.enable_from_start = false;
    }
}

void sup_engage_request_enable(bool on)
{
    s.enable_req = on;
}

static bool s_enable_forced;

void sup_engage_force_enable(bool on)
{
    s_enable_forced = on;
}

void sup_engage_enable_from_start(bool on)
{
    s.enable_from_start = on;
}

bool sup_engage_active(void)
{
    return s.state != ST_IDLE;
}

void sup_engage_echo_bit0(bool echo_set)
{
    s.echo_grant = echo_set;
}

bool sup_engage_enable_bit(void)
{
    /* Stock law (0x91C86 dispatch + word builder): the word's bit 0 goes out
     * only once the supervisor's 0x22 echo shows bit 0 (the acceptance grant).
     * The request itself is a shadow (0xFFF90953) latched early; asserting it
     * unilaterally resets the TCU in every timing tried (bench 2026-10-30
     * through 2026-11-01: late, from-start cold, warm, both dialects). */
    s.enable_sent = (s.enable_req && s.echo_grant) || s_enable_forced;
    return s.enable_sent;
}

const sup_eng_status_t *sup_engage_status(void)
{
    return &s;
}

bool sup_engage_poll(uint16_t *words, uint8_t *n)
{
    uint32_t v;

    if (s.state == ST_IDLE) {
        return false;
    }
    if (s_div < JOB_PERIOD) {
        s_div++;
        return false;
    }
    s_div = 0u;
    if (s.state == ST_SYNC && s.jobs == 0u) {
        /* probe before the first token: pure reads (the ID job's shape) — the
         * challenge must be harvested before it can be answered */
        words[0] = 0x0A00u;              /* commanded-level echo read */
        words[1] = 0x0C00u;              /* challenge feedback read */
        words[2] = 0x0C01u;              /* trailing read, stock duty template */
        *n = 3u;
    } else if (s.state == ST_RAMP && s_next_level) {
        words[0] = (uint16_t)(0x8400u | (s.lvl_cmd & 0x3Fu));
        words[1] = 0x0A00u;              /* level echo read */
        words[2] = 0x1200u;              /* commit */
        words[3] = 0x0C00u;              /* challenge feedback read */
        *n = 4u;
        s_next_level = false;
    } else {
        v = token(s.sel);
        words[0] = (uint16_t)(0x8800u | ((v >> 24) & 0xFFu));
        words[1] = (uint16_t)(0x8800u | ((v >> 16) & 0xFFu));
        words[2] = (uint16_t)(0x8800u | ((v >> 8) & 0xFFu));
        words[3] = (uint16_t)(0x8800u | (v & 0xFFu));
        words[4] = 0x1200u;              /* commit */
        words[5] = 0x0C00u;              /* challenge feedback read */
        words[6] = 0x0A00u;              /* level echo read */
        words[7] = 0x0C01u;              /* stock template's trailing read */
        *n = 8u;
        if (s.state == ST_RAMP) {
            s_next_level = true;
        }
    }
    s.jobs++;
    return true;
}

static void harvest_fb(uint16_t fb_word)
{
    uint8_t fb = (uint8_t)(fb_word & 0xFFu);

    if ((fb & 0xFu) != s.sel) {
        s.rot++;                         /* the supervisor advanced its challenge */
    }
    s.sel = fb & 0xFu;
    s.lvl = (uint8_t)((fb >> 4) & 7u);
}

void sup_engage_replies(const uint16_t *rx, uint8_t n)
{
    if (n == 3u) {                       /* probe: echo [0], fb [1] */
        if ((rx[1] & 0xF900u) != 0xA800u) {
            s.bad++;
            return;
        }
        harvest_fb(rx[1]);
        s.lvl_echo = (uint8_t)(rx[0] & 0x3Fu);
        if (s.lvl_echo == LEVEL_RUN) {
            /* E14 (spec 03 section 6.2j): the supervisor powers up at 0x1F —
             * already engaged-ready (boot-era probe reads 1F BEFORE any level
             * command, 2026-11-05). Do NOT disturb it with the 1..7 ramp: the
             * stock's own law arms its enable in one monitor pass with no ramp
             * (0x873F0, state <= 6 ~always true). Command 0x841F — confirm
             * what is already there — and the HOLD transition follows on the
             * first matching echo. */
            s.lvl_cmd = LEVEL_RUN;
        }
        s.state = ST_RAMP;               /* the exchange IS the sequence: the
                                          * fb nibble wanders (reads 7 cold) —
                                          * the stock always runs the loop */
    } else if (n == 8u) {                /* duty job: fb [5], status [4], echo [6] */
        if ((rx[5] & 0xF900u) != 0xA800u) {
            s.bad++;
            return;
        }
        s.status = (uint8_t)(rx[4] & 0xFFu);
        s.tokens++;
        harvest_fb(rx[5]);
        if (s.state == ST_SYNC && s.tokens >= SYNC_JOBS) {
            s.state = ST_RAMP;
        }
    } else if (n == 4u) {                /* level job: echo [1], status [2], fb [3] */
        if ((rx[3] & 0xF900u) != 0xA800u) {
            s.bad++;
            return;
        }
        s.status = (uint8_t)(rx[2] & 0xFFu);
        harvest_fb(rx[3]);
        s.lvl_echo = (uint8_t)(rx[1] & 0x3Fu);
        if (s.lvl_echo == s.lvl_cmd) {
            s.echo_ok++;
            s.retries = 0u;
            if (s.state == ST_RAMP) {
                if (s.lvl_cmd < LEVEL_MAX) {
                    s.lvl_cmd++;
                } else if (s.lvl_cmd == LEVEL_MAX) {
                    s.lvl_cmd = LEVEL_RUN;   /* 7 confirmed -> the engaged 0x1F */
                } else {
                    s.state = ST_HOLD;       /* 0x1F confirmed: steady duty arms */
                }
            }
        } else {
            s.echo_bad++;
            /* 0x6C742: the confirm poll resends while the echo disagrees;
             * after persistent mismatch step anyway (observed, logged) */
            if (s.state == ST_RAMP && ++s.retries > ECHO_RETRY) {
                s.retries = 0u;
                if (s.lvl_cmd < LEVEL_MAX) {
                    s.lvl_cmd++;
                } else if (s.lvl_cmd == LEVEL_MAX) {
                    s.lvl_cmd = LEVEL_RUN;
                } else {
                    s.state = ST_HOLD;
                }
            }
        }
    }
}

/* ---- driver register sync: RETIRED 2026-11-04 (spec 03 section 6.2e) -----------------
 *
 * The ID (0x0A), write (0x28) and setpoint (0x2D) jobs emulated here were sent on the
 * supervisor bus (RSPI-B five-select). The driving-state dump of 2026-11-04 proves they are
 * bus-0 jobs: their live TX staging (0xFFF88194 = 8E9F/9021/9655 rows, RX = A200 x4 + 0000
 * — the four real driver chips plus the dead fifth select) matches the bus-0 signature, and
 * the bus-B five-select never draws A2-class replies (E4/E5: 0x0000 from the fourth chip).
 * The real implementations live in io/actuators/sol_drv.c (job_id / job_config / job_cyclic
 * on RSPI-A) with the live write values; the level law is sol_engage_config(). The status
 * block and counters stay for the lab readout; the emitters are gone. */

static sup_drv_status_t s_drv;

/* E41 (spec 03 section 6.2ad): the bus-B commissioning write job, revived.
 * The stock's eight-register write (job 0x28 class: the 8E9F/9021 rows plus
 * mode/config registers) rides the FOUR-select rotation (SSLB3/2/0/5 - the
 * supervisor stays in the read rotation; register data shaped like control
 * words resets the TCU from our context, 0x8E55 bench 2026-11-01). af01
 * (SSLB0) is the target: its 0x24 switch echo (AAFF ours / AA20 stock
 * driving) never sees the engaged values from our RSPI-A commissioning.
 * Grant-gated (the engaged 0x8E row must never precede acceptance) and
 * slow - every 400 passes (~2 s), the m4r cadence that never stressed the
 * 5 ms stream (E40b's 100 ms RSPI-A burst withdrew the grant). The 0x88
 * mode row carries the stock's setpoint-row values in rotation order minus
 * the supervisor: {1A, 1A, 12, 15} - af01 = 0x12. Rows 8/9 repeat the 8E/90
 * rows, the two the driving dump shows the stock writing. */
static bool     s_wj_on;
static uint8_t  s_wj_v8e;
static uint16_t s_wj_div;
static bool     s_sp_on;       /* E42: the bus-B setpoint job (magic data[7]) */
static bool     s_cc_on;       /* E45: the bus-B cyclic companion (magic data[7]=2) */
static uint16_t s_cc_sp;       /* E45: its setpoint value (staged by the caller) */

void sup_drv_write_arm(bool on, uint8_t v8e)
{
    if (on != s_wj_on || v8e != s_wj_v8e) {
        s_wj_div = 0u;        /* reset ONLY on a state change — task_10ms re-asserts the
                               * arm every tick and a per-call reset starves the 400-pass
                               * cadence counter (the E40 lesson, reborn; caught live:
                               * div pinned at ~2 with on=1, drv_writes frozen at 0) */
    }
    s_wj_on = on;
    s_wj_v8e = v8e;
}

void sup_drv_cyclic_arm(bool on, uint16_t sp)
{
    s_cc_on = on;
    s_cc_sp = sp & 0x7FFu;
}

void sup_drv_setpoint_arm(bool on)
{
    if (on != s_sp_on) {
        s_wj_div = 0u;        /* transition-only reset, same discipline */
    }
    s_sp_on = on;
}

void sup_drv_sync(bool on)
{
    (void)on;                 /* retired: the bus-0 commissioning runs in sol_drv */
}

void sup_drv_setpoint_once(void)
{
    /* retired: setpoints ride the sol_drv cyclic job */
}

const sup_drv_status_t *sup_drv_status(void)
{
    return &s_drv;
}

bool sup_drv_poll(uint16_t *words, uint8_t *n)
{
    /* E45 (spec 03 section 6.2af): the bus-B CYCLIC COMPANION — the stock's 45-frame
     * driver cyclic (template 0x40D0C) rides THE SUPERVISOR'S OWN BUS on the five-select
     * rotation af31/af21/af11/af01/af51 = SSLB3/2/1/0/5 — FRAME INDEX 2 GOES TO THE
     * SUPERVISOR (0x88|mode, A0, E8|setpoint, A6, F0|setpoint, 60, C0, 66, D8), every
     * 10 ms from app start. OUR driver cyclic rides RSPI-A where the supervisor sees
     * nothing — it has never witnessed a setpoint. The E42 "five-select wall" was a
     * dialect error, not a wall: the lone burst's 0x881A told the supervisor its own
     * "channels" were in current control; the stock's frame-2 mode is its chip-record
     * mode (flag-only, 0x10-class) and its rhythm is continuous from app start.
     * Setpoints: the E43 window-load staging (250 mA class per channel, the park-hold
     * fail-safe magnitude) mirrored by the caller; frame 2 (supervisor) carries zero
     * setpoints and the benign 0x10 mode. Grant-independent like the stock's. */
    static uint16_t s_cc_div;
    if (s_cc_on) {
        /* E47: 2 s cadence — the proven-survivable rate for a standalone job. */
        if (++s_cc_div >= 400u) {
            /* E50 (spec 03 section 6.2ai): THE VERBATIM REPLAY. The stock's live
             * 45-frame cyclic TX buffer (0xFFF88518), captured 2026-11-08 while
             * engaged at 1.786 A (park target 8, self-engaged P-hold):
             *   row0 881A 881A 881A 8812 8815   (modes — frame 2 = the supervisor,
             *                                     BOTH channels current control)
             *   row2 E853 ED89 ED89 E853 E800   (ch0 setpoints: frame 2 = 0x589
             *                                     = 855 mA on the supervisor's C)
             *   row4 F589 F589 F053 F000 F000   (ch1: frame 2 = 0x53 = 50 mA TCC)
             * If THIS resets, the exact values are eliminated and the transport
             * (wire timing) is definitively the wall; if it survives, the specific
             * P-hold distribution was the missing piece. */
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
            *n = 45u;                    /* nine rows x five selects -> xfer5 */
            s_drv.drv_jobs++;
            return true;
        }
        return false;
    }
    if ((!s_wj_on && !s_sp_on) || s.state == ST_IDLE || !s.echo_grant) {
        return false;
    }
    if (++s_wj_div < 400u) {              /* ~2 s at the 5 ms pass */
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
    /* E42c: the dump-exact engaged setpoint job (driving dump 0x2D): TEN words
     * on the five-select rotation — the 88 mode row {1A,1A,1A,12,15} then
     * A000 x5 (dither zero). E42's 45-word cyclic-companion burst reset the
     * TCU (stall > 20 ms when stacked on the same tick, the E28 law —
     * BSS-wipe forensics: armed statics zeroed, ladder re-climbed, drv_sp
     * cleared). The 10-word shape is ID-poll length: stream-safe by
     * construction. The cycling keep-warm VALUES (E8/F0 rows) ride the
     * RSPI-A cyclic as since E37; this job feeds af01 the mode/dither rows
     * it never saw. */
    {
        static const uint8_t MODE5[5] = { 0x1Au, 0x1Au, 0x1Au, 0x12u, 0x15u };
        for (uint8_t c = 0u; c < 5u; c++) {
            words[c] = (uint16_t)(0x8800u | MODE5[c]);
        }
        for (uint8_t c = 0u; c < 5u; c++) {
            words[5u + c] = 0xA000u;
        }
    }
    *n = 10u;                              /* two rows x five selects -> xfer5 */
    s_drv.drv_jobs++;
    s_drv.drv_sp++;
    return true;
}

void sup_drv_replies(const uint16_t *rx, uint8_t n)
{
    /* strict classes — kept for the historical lab data; no emitter feeds it now */
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


/* ---- the setpoint job (stock 0x2D, sender 0x924B0): template flash 0x40D0C
 * = [88, A0, E8, A6, F0, 60, C0, 66, D8]; the stock ORs the flags byte into
 * 0x88, bytes into A0/A6, and the 11-BIT CURRENT SETPOINTS into E8/F0 (two
 * per chip, sender positions 10-14 / 20-24 = chan+0xC / chan+0x14). The
 * engaged stock's keep-warm machine commands these continuously from boot
 * (0x90586 capture: 11 xx 00 01 11 xx, xx cycling 41/34/3A/27/0E/1F/3C/26/44);
 * our commissioning sends a static keep-warm-class 0x1141 on both channels. */


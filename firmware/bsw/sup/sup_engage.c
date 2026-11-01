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

/* One register job every 60 passes = 300 ms. The stock paces its jobs with
 * calibration dwells (level*200 + 2400 timer ticks, engine 0x6BF9C); the bench
 * exchanges ran clean at 0.3-1.1 s cadences (2026-10-31 v2/v3 traces). */
#define JOB_PERIOD 60u
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
    s.enable_sent = s.enable_req && s.echo_grant;
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

/* ---- driver register sync (the stock's five-select read job, read-only) ---- */

/* Read registers from the stock template at flash 0x40CE8: 18 1E 28 88 A0 E8
 * A6 F0 60 C0 66 D8, register-major so the five-select rotation addresses each
 * chip in turn (word i -> select i%5: SSLB3/2/1/0/5). */
static const uint16_t S_DRV_READ_REGS[12] = {
    0x1800u, 0x1E00u, 0x2800u, 0x8800u, 0xA000u, 0xE800u,
    0xA600u, 0xF000u, 0x6000u, 0xC000u, 0x6600u, 0xD800u,
};

/* One 60-word pass every 400 passes = 2 s (the stock syncs on channel-state
 * change from its current loop; a slow read cadence is the safe subset). */
#define DRV_PERIOD 400u

static sup_drv_status_t s_drv;
static uint16_t s_drv_div;
static bool s_drv_on;

/* The write job (stock sender 0x92344): template flash 0x40AD8 verbatim,
 * register-major across the five-select rotation; the stock ORs per-channel
 * data into 8E/90/96/88 and leaves 28/30/2E/36 bare as readbacks. Our first
 * pass is the zero-value commissioning: configs cleared, setpoints zero. */
#define DRV_WRITE_WORDS 40u
static const uint16_t S_DRV_WRITE_TMPL[8] = {
    0x8E00u, 0x9000u, 0x9600u, 0x8800u, 0x2800u, 0x3000u, 0x2E00u, 0x3600u,
};
static bool s_drv_write_pending;

void sup_drv_sync(bool on)
{
    s_drv_on = on;
    s_drv_div = DRV_PERIOD - 1u;        /* first pass on the next tick */
    s_drv_write_pending = on;           /* one commissioning write at arm */
}

const sup_drv_status_t *sup_drv_status(void)
{
    return &s_drv;
}

bool sup_drv_poll(uint16_t *words, uint8_t *n)
{
    if (!s_drv_on) {
        return false;
    }
    if (s_drv_write_pending) {
        /* zero-value commissioning pass: all configs cleared, setpoints zero,
         * readbacks bare — the safe subset of the stock's write+verify job */
        s_drv_write_pending = false;
        for (uint8_t r = 0u; r < 8u; r++) {
            for (uint8_t c = 0u; c < 5u; c++) {
                words[5u * r + c] = S_DRV_WRITE_TMPL[r];
            }
        }
        *n = DRV_WRITE_WORDS;
        s_drv.drv_jobs++;
        s_drv.drv_writes++;
        return true;
    }
    if (s_drv_div < DRV_PERIOD) {
        s_drv_div++;
        return false;
    }
    s_drv_div = 0u;
    for (uint8_t r = 0u; r < 12u; r++) {
        for (uint8_t c = 0u; c < 5u; c++) {
            words[5u * r + c] = S_DRV_READ_REGS[r];
        }
    }
    *n = SUP_DRV_READ_WORDS;
    s_drv.drv_jobs++;
    return true;
}

void sup_drv_replies(const uint16_t *rx, uint8_t n)
{
    /* strict classes — the stock's 0xF900 mask accepts 0xAAxx into the 0xA8
     * class too (fine for its parity checks, useless for per-chip counts) */
    for (uint8_t i = 0u; i < n && i < SUP_DRV_READ_WORDS; i++) {
        if ((rx[i] & 0xFE00u) == 0xA800u) {
            s_drv.drv_a8++;
        } else if ((rx[i] & 0xFE00u) == 0xAA00u) {
            s_drv.drv_aa++;
        } else {
            s_drv.drv_bad++;
        }
    }
}


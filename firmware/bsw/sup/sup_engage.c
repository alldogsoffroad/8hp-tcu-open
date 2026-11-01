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
        s.lvl_cmd = s.lvl >= 1u ? s.lvl : 1u;
        if (s.lvl >= 6u) {
            s.state = ST_HOLD;           /* already engaged-class: answer tokens */
        }
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
        if ((rx[1] & 0xFFu) == s.lvl_cmd) {
            s.echo_ok++;
            if (s.state == ST_RAMP) {
                if (s.lvl_cmd < 7u) {
                    s.lvl_cmd++;
                } else if (s.lvl >= 6u) {
                    s.state = ST_HOLD;   /* steady state: token duty arms, 0x6C63E 0x1E */
                }
            }
        } else {
            s.echo_bad++;
        }
    }
}

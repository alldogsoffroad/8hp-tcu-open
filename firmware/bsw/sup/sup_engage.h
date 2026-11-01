/* Supervisor commissioning (engage) sequence — spec 03 "challenge-response decoded"
 * (2026-10-31). The stock's register jobs ride the 5 ms cyclic stream: the duty job
 * answers the supervisor's rolling challenge — selector = low nibble of the 0x0C read
 * reply — with ~(table[sel] ^ 0xAAAA ^ const), and the level job steps the commanded
 * level up the ramp. This module schedules those jobs FROM the 5 ms tick (single bus
 * owner: no host one-shot bursts — they stop the cyclic stream and with it the
 * acceptance echo) and harvests the replies. */
#ifndef TCU_SUP_ENGAGE_H
#define TCU_SUP_ENGAGE_H

#include <stdint.h>
#include <stdbool.h>

#define SUP_ENG_JOB_MAX 8u

typedef struct {
    uint8_t  state;        /* 0 idle, 1 sync, 2 ramp, 3 hold */
    uint8_t  sel;          /* current challenge selector (0x0C reply low nibble) */
    uint8_t  lvl;          /* achieved level (0x0C reply high nibble, & 7) */
    uint8_t  lvl_cmd;      /* commanded level (the ramp) */
    uint8_t  rot;          /* challenge rotations observed */
    uint8_t  jobs;         /* register jobs sent */
    uint8_t  bad;          /* register-job replies outside the 0xA8 class */
    uint8_t  echo_ok;      /* level echo (0x0A reply) matched the command */
    uint8_t  echo_bad;     /* level echo mismatches */
    uint8_t  lvl_echo;     /* last 0x0A echo value (the supervisor's commanded) */
    uint8_t  retries;      /* consecutive echo mismatches at this level */
    uint8_t  status;       /* last 0x1200 commit status byte */
    uint16_t tokens;       /* duty jobs answered */
    bool     enable_req;   /* host-gated enable request (the shadow) */
    bool     enable_from_start;  /* stock order: bit 0 in the word from frame one
                                  * (monitor 0x873F0 latches the enable on its FIRST
                                  * pass, level still 0 — not after the ramp) */
    bool     echo_grant;   /* supervisor's 0x22 echo bit 0 observed (acceptance) */
    bool     enable_sent;  /* enable bit included in the 8E word */
} sup_eng_status_t;

void sup_engage_arm(bool on);
void sup_engage_request_enable(bool on);
/* Stock-order variant: the enable rides the 8E word from the first frame of the
 * sequence (bench 2026-11-01: enabling only AFTER level 7 resets the TCU). */
void sup_engage_enable_from_start(bool on);
/* The supervisor grants acceptance (0x22 echo bit 0) spontaneously; the stock
 * app answers by putting bit 0 in the word — NEVER the other way round. Every
 * unilateral enable attempt has reset the TCU (bench 2026-10-30..11-01). Feed
 * the observed echo bit here every 5 ms pass; the word's bit 0 follows it. */
void sup_engage_echo_bit0(bool echo_set);
bool sup_engage_active(void);
/* The 0x8E word's enable bit is legal once the ramp reached level 6 (stock gate
 * 0x6C63E: level >= 6 AND the supervisor's echo acceptance). */
bool sup_engage_enable_bit(void);
/* Called from the 5 ms pattern tick: returns true and fills words and *n when a
 * register job is due this pass; the caller transmits it and hands the replies
 * to sup_engage_replies(). */
bool sup_engage_poll(uint16_t *words, uint8_t *n);
void sup_engage_replies(const uint16_t *rx, uint8_t n);
const sup_eng_status_t *sup_engage_status(void);

#endif

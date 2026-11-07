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
    bool     enable_from_start;  /* stock order: the enable shadow latched at arm
                                  * (monitor 0x873F0 one-shot), never asserted
                                  * before the grant */
    bool     echo_grant;   /* supervisor's 0x22 echo bit 0 observed (acceptance) */
    bool     enable_sent;  /* enable bit included in the 8E word */
} sup_eng_status_t;

/* Driver register sync (the stock's five-select jobs, spec 03 job registry
 * 2026-11-01): twelve read registers x the SSLB3/2/1/0/5 rotation = 60 words
 * per pass on RSPI-B, the supervisor's select IN the rotation — the traffic it
 * has never witnessed from us (our solenoid path is RSPI-A). */
#define SUP_DRV_READ_WORDS 60u
#define DRV_WRITE_WORDS 40u   /* 8 regs x all five selects: the stock's commissioning
                                * write includes the supervisor (bench 2026-11-04: its
                                * 0x84/0x88/0x90 read empty because we skipped select 4) */
#define DRV_SP_WORDS 36u      /* 9 regs x 4 chips, the setpoint job */
#define DRV_ID_WORDS 10u      /* 0x06 + 0x00 reads x 5 selects */

typedef struct {
    uint8_t  drv_jobs;     /* driver jobs sent (read + write) */
    uint8_t  drv_writes;   /* commissioning write jobs sent */
    uint8_t  drv_sp;       /* setpoint jobs sent (keep-warm class) */
    uint8_t  drv_id;       /* ID-poll jobs sent (the 0x0A pass) */
    uint16_t drv_a2;       /* chip ID-class (0xA2xx) replies seen */
    uint8_t  drv_bad;      /* words whose reply fits no known class */
    uint16_t drv_a8;       /* supervisor-class (0xA8xx) replies seen */
    uint16_t drv_aa;       /* other-chip-class (0xAAxx) replies seen */
} sup_drv_status_t;

void sup_engage_arm(bool on);
void sup_engage_request_enable(bool on);
void sup_engage_force_enable(bool on);     /* BENCH ONLY (FE13 sub 11, 2026-11-04): put the
                                            * enable bit into the cyclic control word for a
                                            * bounded window regardless of the echo grant -
                                            * the deliberate A-law bypass experiment; the
                                            * stock itself sends 0x8E81 while engaged */
void sup_engage_enable_from_start(bool on);
/* The supervisor grants acceptance (0x22 echo bit 0) spontaneously; the stock
 * app answers by putting bit 0 in the word — NEVER the other way round. Every
 * unilateral enable attempt has reset the TCU (bench 2026-10-30..11-01). Feed
 * the observed echo bit here every 5 ms pass; the word's bit 0 follows it. */
void sup_engage_echo_bit0(bool echo_set);
bool sup_engage_active(void);
bool sup_engage_enable_bit(void);
/* Called from the 5 ms pattern tick: returns true and fills words and *n when a
 * register job is due this pass; the caller transmits it and hands the replies
 * to sup_engage_replies(). */
bool sup_engage_poll(uint16_t *words, uint8_t *n);
void sup_engage_replies(const uint16_t *rx, uint8_t n);
const sup_eng_status_t *sup_engage_status(void);

/* Read-only driver sync interleaved into the same 5 ms tick (independent of the
 * engage state machine); sup_drv_poll() returns true when the 60-word read job
 * is due this pass — the caller transmits it via the five-select transport and
 * hands the replies to sup_drv_replies(). */
void sup_drv_sync(bool on);
void sup_drv_write_once(void);   /* queue one zero-value write pass */
void sup_drv_setpoint_once(void);/* queue one keep-warm setpoint pass */
bool sup_drv_poll(uint16_t *words, uint8_t *n);
void sup_drv_replies(const uint16_t *rx, uint8_t n);
const sup_drv_status_t *sup_drv_status(void);
/* E41 (spec 03 section 6.2ad): arm the bus-B commissioning write job (the
 * stock's job 0x28 shape on the four-select rotation, supervisor excluded).
 * v8e = the live 0x8E row value (0x9F = the stock constant). Grant/ladder
 * gating and the ~2 s cadence live inside sup_drv_poll; the arm alone is
 * inert until the mirror carries bit 0. */
void sup_drv_write_arm(bool on, uint8_t v8e);
/* E42 (spec 03 section 6.2ad): arm the bus-B nine-row five-select setpoint
 * job (template 0x40D0C — the stock's continuous keep-warm carrier, the job
 * our stream never sent; supervisor's select included as the stock runs it).
 * Same grant gating and ~2 s cadence inside sup_drv_poll; supersedes the
 * write job when both are armed (write takes priority). */
void sup_drv_setpoint_arm(bool on);

#endif

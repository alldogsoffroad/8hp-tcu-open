/* Supervisor commissioning walk — E60p (spec 22, the unified protocol).
 *
 * REWRITE of the E60n engine: the walk is the STOCK's machine, verbatim from
 * the top-down decode — driven from a 1 ms tick (the stock's timer ISR 0x9106C
 * class), era dwells = value*200 + 2400 ms, the outer FSM 0 -> 8 -> 0x12 ->
 * 0x23 (duty era: token with context cycling 0 -> 7 -> 0x19 while level==0,
 * context 0x19 sends ~token) -> 0x1F era when the reported level reaches 0 ->
 * 0x45 proceed gate (level >= 5) -> 0x34 -> steady. Jobs are one-in-flight;
 * every level reply is checked three ways (0x0A echo == staged value, 0x12
 * commit & 5 vs the phase mask, expected vs reported level) with the stock's
 * bump-and-resend resync (0x6C742 / 0x6C720).
 *
 * The 10 ms era-floor engine of E60n (spec 03 6.2be) is RETIRED: its cadence
 * broke never-rule NR-2 (level words outside the stock's timer frame). */
#ifndef TCU_SUP_ENGAGE_H
#define TCU_SUP_ENGAGE_H

#include <stdint.h>
#include <stdbool.h>

#define SUP_ENG_JOB_MAX 8u

/* Walk states (the outer FSM 0xFFF95010, spec 22 section 6.3) */
enum {
    SUP_ENG_IDLE = 0u,     /* not armed */
    SUP_ENG_S0 = 1u,       /* boot flag -> stage the 0x06 era */
    SUP_ENG_S8 = 2u,       /* mode_transition_supervise: budget to target >= 5 */
    SUP_ENG_S12 = 3u,      /* re-arm target 5 */
    SUP_ENG_S23 = 4u,      /* DUTY era: token + drag-down; level==0 -> 0x1F era */
    SUP_ENG_S45 = 5u,      /* proceed gate: level >= 5 */
    SUP_ENG_S34 = 6u,      /* advance (re-request 0x1F once) or steady */
    SUP_ENG_STEADY = 7u,   /* certified steady state (duty token continues) */
};

typedef struct {
    uint8_t  state;        /* SUP_ENG_* above */
    uint8_t  sel;          /* current challenge selector (0x0C reply low nibble) */
    uint8_t  lvl;          /* reported/achieved level (0x0C reply high nibble, & 7) */
    uint8_t  lvl_exp;      /* expected level (the stock's 0xFFF93BA2 - bumps toward 7) */
    uint8_t  value;        /* staged 84-value (0x3F init / 0x06 / 0x1F) */
    uint8_t  pending;      /* the ping-pong's pending slot value (0 = none) */
    uint8_t  ctx;          /* duty context (0 -> 7 -> 0x19 while level 0; 0x1E steady) */
    uint8_t  mode;         /* job mode cell (0 = duty W1, 7 = level W2) */
    uint8_t  lvl_echo;     /* last 0x0A echo value */
    uint8_t  status;       /* last 0x12 commit status byte */
    uint8_t  retries;      /* ping-pong confirm retries at this value */
    uint8_t  jobs;         /* walk jobs sent */
    uint16_t tokens;       /* duty jobs answered */
    uint16_t echo_ok;      /* level echo confirmed */
    uint16_t echo_bad;
    uint16_t commit_bad;   /* 0x12 mask mismatches */
    uint8_t  bad;          /* replies outside the 0xA8 class */
    uint16_t era_ms;       /* ms elapsed in the current era (live view) */
    bool     staged;       /* the pending value has been promoted (sent) */
    bool     enable_req;   /* the request cell 0xFFF90953: forced on once armed */
    bool     enable_kill;  /* lab kill-switch (magic/FE13 override) */
    bool     echo_grant;   /* supervisor's 0x22 echo bit 0 observed (acceptance) */
    bool     enable_sent;  /* enable bit included in the 8E word */
    bool     in_flight;    /* a walk job awaits its reply */
    uint16_t last_words[8]; /* the most recent walk job composed */
    uint8_t  last_n;       /* its frame count */
} sup_eng_status_t;

/* Driver register sync (retired emitters kept for the lab readout) */
#define SUP_DRV_READ_WORDS 60u
#define DRV_WRITE_WORDS 40u
#define DRV_SP_WORDS 36u
#define DRV_ID_WORDS 10u

typedef struct {
    uint8_t  drv_jobs;
    uint8_t  drv_writes;
    uint8_t  drv_sp;
    uint8_t  drv_id;
    uint16_t drv_a2;
    uint8_t  drv_bad;
    uint16_t drv_a8;
    uint16_t drv_aa;
} sup_drv_status_t;

/* E60p API --------------------------------------------------------------- */

/* Boot the walk (the stock's fault_manager_init -> park_pwm_init 0x6C12C):
 * value 0x3F, period 0x3138, expected/reported level 7, outer FSM S0. Also
 * arms the enable request cell (the stock's first-monitor-pass one-shot —
 * NR-1: the ask and the walk are one system). */
void sup_engage_boot(void);

/* One 1 ms tick of the walk service (the stock's timer-ISR 0x9106C class).
 * Returns true and fills words[0..*n) when a walk job is due NOW (one in
 * flight; the caller transmits it and hands the replies to
 * sup_engage_replies() — or calls sup_engage_tx_failed() when the bus was
 * busy, which clears the in-flight flag without counting a reply). */
bool sup_engage_tick(uint32_t now_ms, uint16_t *words, uint8_t *n);
void sup_engage_replies(const uint16_t *rx, uint8_t n);
void sup_engage_tx_failed(void);

/* The enable request cell (0xFFF90953): on from boot unless killed. The
 * mirror's bit 0 follows the CELL (stock law 0x91C86 — forced on every pass
 * once armed), no longer echo-gated: the reset class was bit 0 WITHOUT the
 * conversation (NR-1), never bit 0 with it. */
bool sup_engage_enable_bit(void);
void sup_engage_kill(bool on);        /* lab: suppress the request cell */
void sup_engage_echo_bit0(bool echo_set);
const sup_eng_status_t *sup_engage_status(void);

/* Legacy entry points kept for the lab/build plumbing (adapters) */
void sup_engage_arm(bool on);
void sup_engage_request_enable(bool on);
void sup_engage_force_enable(bool on);
void sup_engage_enable_from_start(bool on);
bool sup_engage_active(void);

/* Driver sync (unchanged, retired emitters) */
void sup_drv_sync(bool on);
void sup_drv_write_once(void);
void sup_drv_setpoint_once(void);
bool sup_drv_poll(uint16_t *words, uint8_t *n);
void sup_drv_replies(const uint16_t *rx, uint8_t n);
const sup_drv_status_t *sup_drv_status(void);
void sup_drv_write_arm(bool on, uint8_t v8e);
void sup_drv_setpoint_arm(bool on);
void sup_drv_cyclic_arm(bool on, uint16_t sp);
void sup_drv_cyclic_boot(bool on);

#endif

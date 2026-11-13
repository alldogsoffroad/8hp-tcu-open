/* sup_walk.h — the supervisor commissioning walk (design §7.7, spec 22 §6).
 *
 * Faithful port of the stock's machinery (addresses in comments):
 *   init              0x6C12C park_pwm_init     (from fault_manager_init)
 *   monitor pass      0x873F0 fault_manager_poll (the 5 ms task)
 *   walk task         0x6C63E park_pwm_task      (states 0/7/0x19)
 *   outer FSM         0xFFF95010                 (0/8/0x12/0x23/0x45/0x34)
 *   era stepper       0x6C542                    (FRT vs period + 0x960)
 *   reply processor   0x6C1EA park_monitor_feedback (the ONLY level-nibble
 *                                                 harvester in the image)
 *   staging ping-pong 0x6C742 + 0x6C48C park_pwm_learn_value
 *   timer programmers 0x6C0BA (level) + 0x6BF9C (duty twin)
 *   timer ISR service 0x6C1BA park_pwm_service  (job dispatch, WDT fed by
 *                                                plat_walktmr_isr first)
 *   restart/resync    0x6C720 park_pwm_req_restart (bump and resend, syncs
 *                                                  on the 0x0A echo)
 *
 * Task/ISR split (design §7.3): all state transitions run in the monitor
 * task; the timer ISR only reads the job-mode cell and dispatches the
 * staged job — exactly the stock's split.
 */
#ifndef TCU_SUP_WALK_H
#define TCU_SUP_WALK_H

#include <stdint.h>
#include <stdbool.h>

/* walk task states (cell 0xFFF93BA3) */
enum {
    SUP_WALK_IDLE   = 0x00,   /* era stepper; waiting for the ping-pong   */
    SUP_WALK_SAMPLE = 0x07,   /* sampling: deadline gate + reply processor */
    SUP_WALK_ERA    = 0x19,   /* era walk: stepper + reply processor       */
};

/* staging ping-pong sub-states (cell 0xFFF93B43) */
enum {
    SUP_SUB_NONE         = 0x00,
    SUP_SUB_WARM_PENDING = 0x07,   /* parked slot, next pass promotes      */
    SUP_SUB_COLD_STAGED  = 0x19,   /* value on the wire, confirm on echo   */
    SUP_SUB_ATTENTION    = 0x2A,   /* echo mismatch → bump-and-resend      */
};

/* outer FSM states (cell 0xFFF95010, complement 0xFFF94E88; 0x873F0) */
enum {
    SUP_FSM_BOOT     = 0x00,   /* stage the 0x06 era, target 5, → 8          */
    SUP_FSM_SUPERVISE= 0x08,   /* the 0x8D020 law (target from the ladder)  */
    SUP_FSM_REARM    = 0x12,   /* entry: target 5; 0x8D020 (budget failure) */
    SUP_FSM_DUTY     = 0x23,   /* entry: target 0; level 0 + era 0x1838 → 'E' */
    SUP_FSM_PROCEED  = 0x45,   /* 'E': the 0x8CEDC rule; level >= 5 → 0x34  */
    SUP_FSM_ADVANCE  = 0x34,   /* 0x8799C recovery back to 'E' at level 0   */
};

/* the FSM request (stock 0x87766): called only by the mode sequencer
 * 0x85160 (sup_ladder.c) — 0x23 after the ladder's 0xAA, 0x12/0x23 on
 * retry exhaustion. No-op while the FSM is 0 or already t. */
void sup_monitor_request(uint8_t t);

/* init (stock 0x6C12C): value pair 0x3F/0xC0, period 0x3138, expected =
 * reported = 7, era counter 1, time base = now, boot flag = 1, walk
 * state = 0. NO 84 word is sent here and the walk timer is NOT started —
 * the first 84|06 goes on the wire when the FSM's first pass stages the
 * 0x06 era (design D4/NR-2). */
void sup_walk_init(void);

/* the 5 ms monitor task (stock 0x873F0): the one-shot enable, the token
 * staging, the walk task, and the outer FSM (parked while the walk state
 * is non-zero). */
void sup_monitor_poll(void);

/* the walk timer ISR entry (called from plat_walktmr_isr, which has
 * already fed the WDT — stock 0x6C1BA law): builds and queues the current
 * walk job by the job-mode cell, then delivers it (walk queue priority).
 * One-in-flight: a busy job is skipped silently (stock 0x93B8E). Runs in
 * ISR context. */
void sup_walk_isr(void);

/* staging ping-pong value request (stock 0x6C48C park_pwm_learn_value):
 * value is the era period (level x 200; 0x4B0 = the 0x06 era, 0x1838 = the
 * 0x1F era). Returns 0 accepted, 2 invalid (stock range law < 0x3139). */
uint8_t sup_walk_stage(uint16_t period, uint8_t tries);   /* stock 0x6C48C */

/* bump and resend (stock 0x6C720 park_pwm_req_restart): re-stage the
 * current era value; the exchange re-syncs on the 0x0A echo. */
bool sup_walk_resend(void);                  /* stock 0x6C720: true = busy */
uint32_t sup_walk_shot_at(void);            /* TCNTA µs of the armed shot, 0 = none */

/* the level target (stock 0x8D136): 0, 5 or 3 (any other value = 3, as
 * the stock's else branch); sets the 0x9417D budget 3 + |delta| + 2 and
 * clears the 0x93B96 flag. Called by FSM state 0 and the ladder. */
void sup_walk_target_set(uint8_t t);
/* target reached (stock 0x8D108): 5 → reported >= 5, 3 → reported <= 4,
 * otherwise reported == t */
bool sup_walk_target_reached(uint8_t t);
/* the budget 0x9417D (stock 0x8D1BA; at 1 it sets the 0x93B96 flag) */
uint8_t sup_walk_budget(void);

/* the stock's hardware-condition gate 0x68BE0 (weak default in sup_walk.c:
 * condition clear); read by the one-shot enable and the sequencer 'L'/'=' */
bool plat_hwcond(void);

/* the W2 level builder (stock 0x91190): stages 0x8400|value into the level
 * job's first word and queues JOB_LEVEL. One-in-flight check-free. */
void sup_level_job(void);

/* the W1 duty builder (stock 0x91086): builds the 7-word duty staging from
 * the token + context cells and queues JOB_DUTY. One-in-flight check-free. */
void sup_duty_job(void);

#endif /* TCU_SUP_WALK_H */

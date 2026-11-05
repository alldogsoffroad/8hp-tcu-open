/* External supervisor service (spec 03 section 4.4).
 *
 * The supervisor on RSPI-B / SSLB1 expects its register 0x8E to be written regularly with bit 2
 * toggling (stock: every 5 ms); bit 0 of the same word enables the solenoid outputs. This module
 * sends the word every 5 ms, toggles bit 2 and keeps the output enable off unless requested.
 */
#ifndef TCU_SUP_H
#define TCU_SUP_H

#include "platform/platform.h"

#define SUP_CMD_CONTROL  0x8E00u
#define SUP_BIT_ENABLE   0x01u
#define SUP_BIT_TOGGLE   0x04u
#define SUP_BIT_FL_TOGGLE 0x10u     /* user boot MAT programming loop (state 0x19): bit 4 toggles, */
#define SUP_BIT_FL_MODE   0x80u     /* bit 7 held, bit 2 and the output enable off [I] FWE        */
#define SUP_REPLY_MASK   0xF900u
#define SUP_REPLY_OK     0xA800u

typedef struct {
    uint16_t last_tx;
    uint16_t last_rx;
    uint32_t sent;
    uint32_t replies_ok;
    uint32_t replies_bad;
    uint32_t busy;          /* transmitter not free when due */
    bool     enable;        /* output enable currently sent */
    /* Stock-pattern mode (spec 03 section 4.4, FE13): the three app cyclic jobs */
    bool     app_pattern;
    uint16_t rx_words[8];   /* last replies: job 8E/C9/22/09, then 90/24, then 28/2E */
    uint32_t echo_ok;       /* 0x22 control echo matched the sent word (mask 0xDF) */
    uint32_t echo_bad;
    uint32_t a8_bad;        /* SSLB1 (supervisor) frames not in the 0xA8 reply class */
    uint32_t aa_bad;        /* SSLB0 frames not in the 0xAA reply class */
    uint16_t sslb2_rx;       /* last SSLB2 reply (stock job 0x40C74 sends 0x00FF) */
    uint16_t rx_id[4];       /* last ID-job replies (stock 0x40B80: 06/03/00/00) */
    bool     sslb2_on;       /* pattern includes the cyclic SSLB2 exchange */
} sup_status_t;

void sup_init(void);
void sup_tick_5ms(void);
/* Request the solenoid output enable (register 0x8E bit 0). Several owners may need the
 * enable at the same time (a bench test, the park module): each sets and clears only its
 * own source bit, and the enable is sent while any source requests it. Takes effect with
 * the next 5 ms frame, so an enable can never outlive its requester's tick. */
#define SUP_EN_BENCH 0x01u
#define SUP_EN_PARK  0x02u
void sup_enable_set(uint8_t sources);
void sup_enable_clear(uint8_t sources);
const sup_status_t *sup_status(void);
/* Flash programming mode as the user boot MAT sends it (spec 16 section 10): word
 * 0x8E80 | (bit 4 toggling), output enable forced off. Bench test 2026-10-17. */
void sup_set_flash_mode(bool on);
/* Bench experiment (routine FE32): send word ^ (toggle on alternate frames) every 5 ms instead of
 * the normal word; in register 0x8E words the output enable bit 0 is always cleared. */
void sup_set_pattern(bool pattern_on, uint16_t word, uint16_t toggle);
void sup_set_pattern_raw(bool pattern_on, uint16_t word, uint16_t toggle); /* lab: no bit-0 clear */
void sup_suspend(bool on);            /* lab: suspend the 5ms tick (raw exchange) */
/* Stock-application pattern (FE13): instead of the single 0x8E word, the 5 ms tick sends the
 * app's three cyclic jobs verbatim (spec 03 section 4.4): [0x8E00|bits, 0xC900, 0x2200,
 * 0x0900], [0x9000|bits90, 0x2400], [0x2800, 0x2E00], with frames alternating SSLB1 / SSLB0.
 * Bit 2 of the 0x8E word follows the complement of the 0x22 echo's bit 2 as in the stock.
 * bits may carry bit 0 (output enable) — a deliberate, dangerous bench act: on the bench the
 * enable bit sent without this full context reset the TCU (2026-10-30). */
void sup_app_pattern(bool on, uint8_t bits8e, uint8_t bits90);
/* Full-fidelity variant: additionally sends the stock's cyclic SSLB2 exchange
 * ([0x00FF], SPI mode 0) each 5 ms pass, as job 0x40C74 does. */
void sup_app_pattern2(bool on, uint8_t bits8e, uint8_t bits90, bool with_sslb2);
/* Engaged-app word shape (FE13 type 8): the life-sign bit is caller-selected — the
 * engaged application alternates BIT 4 with bit 7 held (word 0x8E91/0x8E81, stock RAM
 * 0xFFF907D4 capture 2026-10-31), not the boot software's bit 2. */
void sup_app_pattern3(bool on, uint8_t bits8e, uint8_t toggle_bit, uint8_t bits90);
/* E11 boot bit-0 window (spec 03 section 6.2h): send bit 0 unconditionally for the first
 * `cycles` exchanges after boot — the stock's own enable fires in its first monitor pass
 * (0x873F0) and the echo bit 0 is latched at the first readable instant (2026-10-31
 * shadow-poll), so bit 0 rides the word from cycle one in the stock; every forced bit-0
 * word of ours was minutes late. If the echo mirrors it once, the A-law sustains it. The
 * caller bounds the total boot attempts (a rejecting supervisor = short reset storm, then
 * a normal A-law boot).
 * E13 (spec 03 section 6.2i): the boot-era capture showed the stock FULLY ENGAGED at
 * boot+15s (word 0x8E91 accepted, echo AAB1 = mirror + supervisor bit 5, rail live) —
 * while our E12 arm fired at +25ms (rejected: before the supervisor's own power-up) and
 * every earlier window was minutes late (rejected: after its acceptance closed). The
 * window is IN THE MIDDLE. `delay_5ms` holds the window that many exchanges before
 * sending bit 0. */
void sup_bit0_boot_arm(uint16_t cycles, uint16_t delay_5ms);

#endif

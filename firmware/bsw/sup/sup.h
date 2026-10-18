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
} sup_status_t;

void sup_init(void);
void sup_tick_5ms(void);
/* Request the solenoid output enable (bit 0). Takes effect with the next frame. */
void sup_set_output_enable(bool on);
const sup_status_t *sup_status(void);
/* Flash programming mode as the user boot MAT sends it (spec 16 section 10): word
 * 0x8E80 | (bit 4 toggling), output enable forced off. Bench test 2026-10-17. */
void sup_set_flash_mode(bool on);
/* Bench experiment (routine FE32): send word ^ (toggle on alternate frames) every 5 ms instead of
 * the normal word; the output enable bit 0 is always cleared. pattern_on = false: normal word. */
void sup_set_pattern(bool pattern_on, uint16_t word, uint16_t toggle);

#endif

/* Measurement protocol, read-only subset compatible with the stock protocol (spec 10).
 *
 * Requests on 0x361, responses on 0x398; periodic byte channels on 0x368/0x378/0x388/0x398.
 * Accepted on either bus; the first bus a request arrives on is latched for all responses.
 * Write commands and commands whose stock meaning is unverified are answered 0x03 (rejected).
 *
 * Measurement indices are project-defined (app/meas_table.c); they do not match the stock
 * index list of spec 10 section 5.
 */
#ifndef TCU_MEAS_H
#define TCU_MEAS_H

#include "platform/platform.h"

#define MEAS_REQ_ID   0x361u
#define MEAS_RSP_ID   0x398u
#define MEAS_CHANNELS 32u

typedef struct {
    const void *ptr;        /* variable in application RAM */
    uint8_t     size;       /* 1 or 2 bytes (big-endian in the periodic channels) */
} meas_index_t;

typedef struct {
    const meas_index_t *index;  /* index i is index[i - MEAS_FIRST_INDEX] */
    uint16_t            n_index;
    const char         *ident;  /* up to 24 characters, returned by 0x01 / 0x21 */
} meas_cfg_t;

#define MEAS_FIRST_INDEX 2u     /* stock numbering starts at 2 */

void meas_init(const meas_cfg_t *cfg);
/* Feed a received frame; returns true if it was a measurement request. */
bool meas_rx(can_bus_t bus, const can_frame_t *f);
/* Send pending responses and periodic data; call every 10 ms. */
void meas_tick_10ms(void);

#endif

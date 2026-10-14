/* ISO 15765-2 transport (normal addressing, 11-bit identifiers, classic CAN).
 *
 * One connection per instance: a physical request identifier, an optional functional request
 * identifier (single frames only) and a response identifier, all on one bus.
 */
#ifndef TCU_ISOTP_H
#define TCU_ISOTP_H

#include "platform/platform.h"

#define ISOTP_BUF 256u
#define ISOTP_PAD 0xAAu

typedef enum { ISOTP_PHYS = 0, ISOTP_FUNC } isotp_addr_t;

typedef struct {
    can_bus_t bus;
    uint16_t  rx_id;        /* physical request          */
    uint16_t  func_id;      /* functional request, 0 = none */
    uint16_t  tx_id;        /* response                  */
} isotp_cfg_t;

typedef struct {
    const isotp_cfg_t *cfg;
    /* receive */
    uint8_t  rx[ISOTP_BUF];
    uint16_t rx_len, rx_pos;
    uint8_t  rx_sn;
    uint16_t rx_timer;
    bool     rx_busy;
    bool     rx_done;
    isotp_addr_t rx_addr;
    /* transmit */
    uint8_t  tx[ISOTP_BUF];
    uint16_t tx_len, tx_pos;
    uint8_t  tx_sn;
    uint8_t  tx_bs, tx_bs_left, tx_stmin;
    uint16_t tx_timer;
    uint8_t  tx_state;
} isotp_t;

void isotp_init(isotp_t *c, const isotp_cfg_t *cfg);

/* Feed a received frame; returns true if it belonged to this connection. */
bool isotp_rx_frame(isotp_t *c, can_bus_t bus, const can_frame_t *f);

/* Advance timers and send pending frames; call every 1 ms. */
void isotp_tick_1ms(isotp_t *c);

/* A complete request is available: returns its length (0 if none) and the address type. */
uint16_t isotp_request(isotp_t *c, const uint8_t **data, isotp_addr_t *addr);
void isotp_request_done(isotp_t *c);

/* Start sending a response. Returns false if a transmission is still running. */
bool isotp_send(isotp_t *c, const uint8_t *data, uint16_t len);
bool isotp_tx_idle(const isotp_t *c);

#endif

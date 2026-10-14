/* CAN receive layer (spec 07 sections 1-3).
 *
 * The vehicle profile supplies a table of receive messages. For each message the layer keeps
 * the last accepted data, checks DLC, CRC-8 and rolling counter where configured, and ages
 * the message against its timeout. Signals are decoded by the profile from the accepted data
 * only; rejected frames never overwrite it.
 */
#ifndef TCU_COM_H
#define TCU_COM_H

#include "platform/platform.h"

/* Integrity protection forms (spec 07 section 2). Checksum in byte DLC-1, counter in the
 * upper nibble of byte DLC-2. */
typedef enum {
    COM_E2E_NONE = 0,
    COM_E2E_V1,         /* CRC over bytes 0..DLC-2; counter counts up by 1..max_step   */
    COM_E2E_V2          /* CRC over inverted bytes, compared inverted; counter counts down */
} com_e2e_t;

typedef struct {
    uint16_t  id;
    can_bus_t bus;
    uint8_t   dlc;          /* minimum accepted DLC */
    uint16_t  timeout_ms;
    com_e2e_t e2e;
    uint8_t   max_step;     /* counter: largest accepted increment, 1..15 */
} com_rx_cfg_t;

typedef enum {
    COM_RX_NEVER = 0,       /* nothing accepted since start              */
    COM_RX_OK,              /* accepted data younger than the timeout    */
    COM_RX_TIMEOUT          /* last accepted data older than the timeout */
} com_rx_status_t;

typedef struct {
    uint8_t  data[8];
    uint16_t age_ms;
    uint8_t  status;        /* com_rx_status_t */
    uint8_t  counter;       /* last accepted counter value */
    uint16_t err_dlc;
    uint16_t err_crc;
    uint16_t err_counter;
} com_rx_state_t;

/* cfg and st must hold n entries and stay valid for the lifetime of the layer. */
void com_init(const com_rx_cfg_t *cfg, com_rx_state_t *st, uint8_t n);

/* Ask the platform to receive every configured identifier (target: one mailbox each). */
bool com_register_hw(void);

/* Feed one received frame; frames for unknown identifiers are ignored.
 * Returns the message index, or -1 if the frame was not accepted. */
int com_rx_indication(can_bus_t bus, const can_frame_t *f);

/* Advance message ages; call periodically with the elapsed time. */
void com_tick(uint16_t elapsed_ms);

/* Copy the last accepted data of message idx into out (8 bytes) and return its status. */
com_rx_status_t com_rx_read(uint8_t idx, uint8_t out[8]);

const com_rx_state_t *com_rx_state(uint8_t idx);

/* Exposed for tests and for transmit-side generation. */
bool com_e2e_check(com_e2e_t e2e, const uint8_t *data, uint8_t dlc);
uint8_t com_e2e_counter(const uint8_t *data, uint8_t dlc);

#endif

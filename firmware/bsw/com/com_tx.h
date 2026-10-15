/* CAN periodic transmit layer (spec 08 sections 1-3).
 *
 * The vehicle profile supplies a table of messages with a pack callback. When a message is due
 * the layer clears the frame, lets the profile fill its fields, then adds the rolling counter
 * (upper nibble of byte DLC-2, +1 per transmission) and the CRC-8 (byte DLC-1) where configured.
 * Every message starts disabled; nothing is sent until the application enables it.
 */
#ifndef TCU_COM_TX_H
#define TCU_COM_TX_H

#include "platform/platform.h"

typedef void (*com_tx_pack_t)(uint8_t idx, uint8_t data[8]);

typedef struct {
    uint16_t  id;
    can_bus_t bus;
    uint8_t   dlc;
    uint16_t  period_ms;
    bool      crc;
    bool      counter;
} com_tx_cfg_t;

typedef struct {
    uint16_t timer_ms;
    uint8_t  counter;
    bool     enabled;
    uint32_t sent;
    uint16_t busy;          /* transmit buffer was full when due */
} com_tx_state_t;

void com_tx_init(const com_tx_cfg_t *cfg, com_tx_state_t *st, uint8_t n, com_tx_pack_t pack);
void com_tx_enable(uint8_t idx, bool on);
void com_tx_tick(uint16_t elapsed_ms);
const com_tx_state_t *com_tx_state(uint8_t idx);

/* Build frame idx now (pack + counter + CRC) without sending; for tests and diagnostics. */
void com_tx_build(uint8_t idx, can_frame_t *f);

#endif

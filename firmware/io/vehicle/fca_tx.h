/* FCA transmit profile: the 8 periodic messages of the stock TCU on bus 1 (spec 08).
 *
 * Field layouts are generated from the stock firmware (fca_tx_fields.h); their meanings are
 * still open, so the profile only offers raw field values. All messages start disabled and
 * all fields at 0. Do not enable them on a vehicle bus until the field meanings and the values
 * a Ram expects are confirmed from a capture (spec 08 section 5).
 */
#ifndef TCU_FCA_TX_H
#define TCU_FCA_TX_H

#include "io/vehicle/fca_tx_fields.h"
#include "platform/platform.h"

#define FCA_TX_ENUM_M(name, id, dlc, period, crc, ctr) FCA_##name,
#define FCA_TX_ENUM_F(name, msg, byte, msb, len) FCA_##name,
typedef enum { FCA_TX_MESSAGES(FCA_TX_ENUM_M) FCA_TX_MSG_COUNT } fca_tx_msg_t;
typedef enum { FCA_TX_FIELDS(FCA_TX_ENUM_F) FCA_TX_FIELD_COUNT } fca_tx_field_t;
#undef FCA_TX_ENUM_M
#undef FCA_TX_ENUM_F

void fca_tx_init(void);
/* Set a raw field value (low `len` bits are used). */
void fca_tx_set(fca_tx_field_t f, uint16_t raw);
uint16_t fca_tx_get(fca_tx_field_t f);
void fca_tx_enable(fca_tx_msg_t m, bool on);
/* Send due messages; call every 10 ms. */
void fca_tx_tick_10ms(void);

#endif

#include "io/vehicle/fca_tx.h"
#include "bsw/com/com_tx.h"
#include "bsw/com/sigpack.h"

#define CFG_M(name, id, dlc, period, crc, ctr) { id, CAN_BUS1, dlc, period, crc, ctr },
#define CFG_F(name, msg, byte, msb, len) { FCA_##msg, { byte, msb, len } },

static const com_tx_cfg_t s_msg[FCA_TX_MSG_COUNT] = { FCA_TX_MESSAGES(CFG_M) };

static const struct {
    fca_tx_msg_t msg;
    sig_t        sig;
} s_field[FCA_TX_FIELD_COUNT] = { FCA_TX_FIELDS(CFG_F) };

static com_tx_state_t s_st[FCA_TX_MSG_COUNT];
static uint16_t s_val[FCA_TX_FIELD_COUNT];

static void pack(uint8_t idx, uint8_t data[8])
{
    for (uint8_t f = 0u; f < FCA_TX_FIELD_COUNT; f++) {
        if (s_field[f].msg == idx) {
            sig_put(data, s_field[f].sig, s_val[f]);
        }
    }
}

void fca_tx_init(void)
{
    for (uint8_t f = 0u; f < FCA_TX_FIELD_COUNT; f++) {
        s_val[f] = 0u;
    }
    com_tx_init(s_msg, s_st, FCA_TX_MSG_COUNT, pack);
}

void fca_tx_set(fca_tx_field_t f, uint16_t raw)
{
    if (f < FCA_TX_FIELD_COUNT) {
        s_val[f] = (uint16_t)(raw & ((1u << s_field[f].sig.len) - 1u));
    }
}

uint16_t fca_tx_get(fca_tx_field_t f)
{
    return f < FCA_TX_FIELD_COUNT ? s_val[f] : 0u;
}

void fca_tx_enable(fca_tx_msg_t m, bool on)
{
    com_tx_enable((uint8_t)m, on);
}

void fca_tx_tick_1ms(void)
{
    com_tx_tick(1u);
}

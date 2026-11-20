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
/* Bit 15 of a 13-bit field's s_val marks "written since fca_tx_init()" (no
 * extra RAM: the .bss layout is pinned). Masked off before packing. */
#define FCA_SET_FLAG 0x8000u

static uint16_t field_mask(uint8_t f)
{
    return (s_field[f].sig.len >= 16u) ? 0xFFFFu : (uint16_t)((1u << s_field[f].sig.len) - 1u);
}

static void pack(uint8_t idx, uint8_t data[8])
{
    for (uint8_t f = 0u; f < FCA_TX_FIELD_COUNT; f++) {
        if (s_field[f].msg == idx) {
            sig_put(data, s_field[f].sig, (uint16_t)(s_val[f] & field_mask(f)));
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
        s_val[f] = (uint16_t)(raw & field_mask(f));
        if (s_field[f].sig.len == 13u) {
            s_val[f] = (uint16_t)(s_val[f] | FCA_SET_FLAG);
        }
    }
}

uint16_t fca_tx_get(fca_tx_field_t f)
{
    return f < FCA_TX_FIELD_COUNT ? (uint16_t)(s_val[f] & field_mask((uint8_t)f)) : 0u;
}

/* The torque-request messages (0x037, 0x2F9: 13-bit Nm*4+2000 fields) may only
 * go out once every 13-bit field has been written: their power-on value 0 is a
 * -500 Nm request. Ours, not stock (the stock always fills them first). */
static bool torque_fields_ready(fca_tx_msg_t m)
{
    if (m != FCA_M_037 && m != FCA_M_2F9) {
        return true;
    }
    for (uint8_t f = 0u; f < FCA_TX_FIELD_COUNT; f++) {
        if (s_field[f].msg == m && s_field[f].sig.len == 13u && (s_val[f] & FCA_SET_FLAG) == 0u) {
            return false;
        }
    }
    return true;
}

bool fca_tx_enable(fca_tx_msg_t m, bool on)
{
    if (on && !torque_fields_ready(m)) {
        com_tx_enable((uint8_t)m, false);
        return false;
    }
    com_tx_enable((uint8_t)m, on);
    return true;
}

void fca_tx_tick_1ms(void)
{
    com_tx_tick(1u);
}

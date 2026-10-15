#include "bsw/com/com_tx.h"
#include "bsw/com/crc8.h"

#include <string.h>

static const com_tx_cfg_t *s_cfg;
static com_tx_state_t *s_st;
static uint8_t s_n;
static com_tx_pack_t s_pack;

void com_tx_init(const com_tx_cfg_t *cfg, com_tx_state_t *st, uint8_t n, com_tx_pack_t pack)
{
    s_cfg = cfg;
    s_st = st;
    s_n = n;
    s_pack = pack;
    memset(st, 0, sizeof *st * n);
}

void com_tx_enable(uint8_t idx, bool on)
{
    if (idx < s_n) {
        s_st[idx].enabled = on;
        s_st[idx].timer_ms = 0u;            /* first frame on the next tick */
    }
}

void com_tx_build(uint8_t idx, can_frame_t *f)
{
    const com_tx_cfg_t *c = &s_cfg[idx];
    const uint8_t dlc = c->dlc;

    f->id = c->id;
    f->dlc = dlc;
    memset(f->data, 0, sizeof f->data);
    if (s_pack != NULL) {
        s_pack(idx, f->data);
    }
    if (c->counter && dlc >= 2u) {
        f->data[dlc - 2u] = (uint8_t)((f->data[dlc - 2u] & 0x0Fu) | (uint8_t)(s_st[idx].counter << 4));
    }
    if (c->crc && dlc >= 2u) {
        f->data[dlc - 1u] = crc8_j1850(f->data, (uint8_t)(dlc - 1u));
    }
}

void com_tx_tick(uint16_t elapsed_ms)
{
    for (uint8_t i = 0u; i < s_n; i++) {
        com_tx_state_t *s = &s_st[i];
        can_frame_t f;

        if (!s->enabled) {
            continue;
        }
        if (s->timer_ms > elapsed_ms) {
            s->timer_ms = (uint16_t)(s->timer_ms - elapsed_ms);
            continue;
        }
        com_tx_build(i, &f);
        if (plat_can_send(s_cfg[i].bus, &f)) {
            s->counter = (uint8_t)((s->counter + 1u) & 0x0Fu);
            s->sent++;
            s->timer_ms = s_cfg[i].period_ms;
        } else {
            s->busy++;
            s->timer_ms = 0u;               /* retry on the next tick */
        }
    }
}

const com_tx_state_t *com_tx_state(uint8_t idx)
{
    return idx < s_n ? &s_st[idx] : NULL;
}

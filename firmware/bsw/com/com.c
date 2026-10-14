/* CAN receive layer. Runs entirely in task context (frames are polled from the controller in
 * the 1 ms task), so no data is shared with interrupts. */
#include "bsw/com/com.h"
#include "bsw/com/crc8.h"

#include <string.h>

static const com_rx_cfg_t *s_cfg;
static com_rx_state_t *s_st;
static uint8_t s_n;

void com_init(const com_rx_cfg_t *cfg, com_rx_state_t *st, uint8_t n)
{
    s_cfg = cfg;
    s_st = st;
    s_n = n;
    memset(st, 0, sizeof *st * n);
}

bool com_register_hw(void)
{
    bool ok = true;

    for (uint8_t i = 0u; i < s_n; i++) {
        ok = plat_can_rx_register(s_cfg[i].bus, s_cfg[i].id) && ok;
    }
    return ok;
}

uint8_t com_e2e_counter(const uint8_t *data, uint8_t dlc)
{
    return (uint8_t)(data[dlc - 2u] >> 4);
}

bool com_e2e_check(com_e2e_t e2e, const uint8_t *data, uint8_t dlc)
{
    uint8_t buf[8];
    uint8_t len;

    if (e2e == COM_E2E_NONE) {
        return true;
    }
    if (dlc < 2u || dlc > 8u) {
        return false;
    }
    len = (uint8_t)(dlc - 1u);
    if (e2e == COM_E2E_V1) {
        return crc8_j1850(data, len) == data[len];
    }
    for (uint8_t i = 0u; i < len; i++) {
        buf[i] = (uint8_t)~data[i];
    }
    return (uint8_t)~crc8_j1850(buf, len) == data[len];
}

static bool counter_ok(const com_rx_cfg_t *c, const com_rx_state_t *s, uint8_t ctr)
{
    uint8_t step;

    if (s->status != COM_RX_OK) {
        return true;                        /* (re)synchronise on the first frame */
    }
    step = (uint8_t)((c->e2e == COM_E2E_V2 ? s->counter - ctr : ctr - s->counter) & 0x0Fu);
    return step >= 1u && step <= c->max_step;
}

int com_rx_indication(can_bus_t bus, const can_frame_t *f)
{
    for (uint8_t i = 0u; i < s_n; i++) {
        const com_rx_cfg_t *c = &s_cfg[i];
        com_rx_state_t *s = &s_st[i];

        if (c->id != f->id || c->bus != bus) {
            continue;
        }
        if (f->dlc < c->dlc || f->dlc > 8u) {
            s->err_dlc++;
            return -1;
        }
        if (c->e2e != COM_E2E_NONE) {
            uint8_t ctr = com_e2e_counter(f->data, f->dlc);
            if (!com_e2e_check(c->e2e, f->data, f->dlc)) {
                s->err_crc++;
                return -1;
            }
            if (!counter_ok(c, s, ctr)) {
                s->err_counter++;
                s->counter = ctr;           /* follow the sender; next frame can pass */
                return -1;
            }
            s->counter = ctr;
        }
        memset(s->data, 0, sizeof s->data);
        memcpy(s->data, f->data, f->dlc);
        s->age_ms = 0u;
        s->status = COM_RX_OK;
        return (int)i;
    }
    return -1;
}

void com_tick(uint16_t elapsed_ms)
{
    for (uint8_t i = 0u; i < s_n; i++) {
        com_rx_state_t *s = &s_st[i];

        if (s->status == COM_RX_NEVER) {
            continue;
        }
        s->age_ms = (uint16_t)(s->age_ms > 0xFFFFu - elapsed_ms ? 0xFFFFu : s->age_ms + elapsed_ms);
        if (s->age_ms > s_cfg[i].timeout_ms) {
            s->status = COM_RX_TIMEOUT;
        }
    }
}

com_rx_status_t com_rx_read(uint8_t idx, uint8_t out[8])
{
    if (idx >= s_n) {
        return COM_RX_NEVER;
    }
    memcpy(out, s_st[idx].data, 8u);
    return (com_rx_status_t)s_st[idx].status;
}

const com_rx_state_t *com_rx_state(uint8_t idx)
{
    return idx < s_n ? &s_st[idx] : NULL;
}

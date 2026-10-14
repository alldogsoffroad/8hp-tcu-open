#include "bsw/diag/isotp.h"

#include <string.h>

#define N_CR_MS   1000u     /* wait for next consecutive frame */
#define N_BS_MS   1000u     /* wait for flow control           */
#define FC_BS     0u        /* our flow control: no block limit */
#define FC_STMIN  0u        /* ... and no separation time       */

enum { TX_IDLE = 0, TX_WAIT_FC, TX_CF };

static bool send_frame(const isotp_t *c, const uint8_t *d, uint8_t n)
{
    can_frame_t f = { .id = c->cfg->tx_id, .dlc = 8u };

    memset(f.data, ISOTP_PAD, 8u);
    memcpy(f.data, d, n);
    return plat_can_send(c->cfg->bus, &f);
}

static void send_fc(const isotp_t *c, uint8_t status)
{
    const uint8_t fc[3] = { (uint8_t)(0x30u | status), FC_BS, FC_STMIN };
    (void)send_frame(c, fc, 3u);
}

void isotp_init(isotp_t *c, const isotp_cfg_t *cfg)
{
    memset(c, 0, sizeof *c);
    c->cfg = cfg;
}

static void rx_first(isotp_t *c, const can_frame_t *f)
{
    uint16_t len = (uint16_t)((f->data[0] & 0x0Fu) << 8 | f->data[1]);

    if (len < 8u || f->dlc < 8u) {
        return;
    }
    if (len > ISOTP_BUF) {
        send_fc(c, 2u);                     /* overflow */
        return;
    }
    memcpy(c->rx, &f->data[2], 6u);
    c->rx_len = len;
    c->rx_pos = 6u;
    c->rx_sn = 1u;
    c->rx_busy = true;
    c->rx_timer = N_CR_MS;
    c->rx_addr = ISOTP_PHYS;
    send_fc(c, 0u);
}

static void rx_consecutive(isotp_t *c, const can_frame_t *f)
{
    uint16_t n;

    if (!c->rx_busy) {
        return;
    }
    if ((f->data[0] & 0x0Fu) != c->rx_sn) {
        c->rx_busy = false;                 /* wrong sequence: abort reception */
        return;
    }
    c->rx_sn = (uint8_t)((c->rx_sn + 1u) & 0x0Fu);
    n = (uint16_t)(c->rx_len - c->rx_pos);
    if (n > 7u) {
        n = 7u;
    }
    if ((uint16_t)(f->dlc - 1u) < n) {
        c->rx_busy = false;
        return;
    }
    memcpy(&c->rx[c->rx_pos], &f->data[1], n);
    c->rx_pos = (uint16_t)(c->rx_pos + n);
    c->rx_timer = N_CR_MS;
    if (c->rx_pos >= c->rx_len) {
        c->rx_busy = false;
        c->rx_done = true;
    }
}

static void rx_flow_control(isotp_t *c, const can_frame_t *f)
{
    if (c->tx_state != TX_WAIT_FC || f->dlc < 3u) {
        return;
    }
    switch (f->data[0] & 0x0Fu) {
    case 0u:                                /* continue to send */
        c->tx_bs = f->data[1];
        c->tx_bs_left = c->tx_bs;
        c->tx_stmin = f->data[2] <= 0x7Fu ? f->data[2] : 1u;   /* 100-900 us -> 1 ms */
        c->tx_state = TX_CF;
        c->tx_timer = 0u;
        break;
    case 1u:                                /* wait */
        c->tx_timer = N_BS_MS;
        break;
    default:                                /* overflow or invalid: abort */
        c->tx_state = TX_IDLE;
        break;
    }
}

bool isotp_rx_frame(isotp_t *c, can_bus_t bus, const can_frame_t *f)
{
    const bool phys = f->id == c->cfg->rx_id;
    const bool func = c->cfg->func_id != 0u && f->id == c->cfg->func_id;
    uint8_t pci;

    if (bus != c->cfg->bus || (!phys && !func) || f->dlc == 0u) {
        return false;
    }
    pci = (uint8_t)(f->data[0] >> 4);
    if (pci == 3u) {
        if (phys) {
            rx_flow_control(c, f);
        }
        return true;
    }
    if (c->rx_done) {
        return true;                        /* previous request not yet processed: ignore */
    }
    if (pci == 0u) {
        uint8_t len = (uint8_t)(f->data[0] & 0x0Fu);
        if (len == 0u || len > 7u || len > f->dlc - 1u) {
            return true;
        }
        memcpy(c->rx, &f->data[1], len);
        c->rx_len = len;
        c->rx_busy = false;
        c->rx_done = true;
        c->rx_addr = phys ? ISOTP_PHYS : ISOTP_FUNC;
    } else if (phys && pci == 1u) {
        rx_first(c, f);
    } else if (phys && pci == 2u) {
        rx_consecutive(c, f);
    }
    return true;
}

static void tx_next_cf(isotp_t *c)
{
    uint8_t d[8];
    uint16_t n = (uint16_t)(c->tx_len - c->tx_pos);

    if (n > 7u) {
        n = 7u;
    }
    d[0] = (uint8_t)(0x20u | c->tx_sn);
    memcpy(&d[1], &c->tx[c->tx_pos], n);
    if (!send_frame(c, d, (uint8_t)(n + 1u))) {
        return;                             /* controller busy: retry next tick */
    }
    c->tx_sn = (uint8_t)((c->tx_sn + 1u) & 0x0Fu);
    c->tx_pos = (uint16_t)(c->tx_pos + n);
    if (c->tx_pos >= c->tx_len) {
        c->tx_state = TX_IDLE;
    } else if (c->tx_bs != 0u && --c->tx_bs_left == 0u) {
        c->tx_state = TX_WAIT_FC;
        c->tx_timer = N_BS_MS;
    } else {
        c->tx_timer = c->tx_stmin;
    }
}

void isotp_tick_1ms(isotp_t *c)
{
    if (c->rx_busy && c->rx_timer != 0u && --c->rx_timer == 0u) {
        c->rx_busy = false;                 /* N_Cr timeout */
    }
    if (c->tx_state == TX_WAIT_FC) {
        if (c->tx_timer != 0u && --c->tx_timer == 0u) {
            c->tx_state = TX_IDLE;          /* N_Bs timeout */
        }
    } else if (c->tx_state == TX_CF) {
        if (c->tx_timer != 0u) {
            c->tx_timer--;
        }
        if (c->tx_timer == 0u) {
            tx_next_cf(c);
        }
    }
}

uint16_t isotp_request(isotp_t *c, const uint8_t **data, isotp_addr_t *addr)
{
    if (!c->rx_done) {
        return 0u;
    }
    *data = c->rx;
    *addr = c->rx_addr;
    return c->rx_len;
}

void isotp_request_done(isotp_t *c)
{
    c->rx_done = false;
}

bool isotp_send(isotp_t *c, const uint8_t *data, uint16_t len)
{
    uint8_t d[8];

    if (c->tx_state != TX_IDLE || len == 0u || len > ISOTP_BUF) {
        return false;
    }
    if (len <= 7u) {
        d[0] = (uint8_t)len;
        memcpy(&d[1], data, len);
        return send_frame(c, d, (uint8_t)(len + 1u));
    }
    memcpy(c->tx, data, len);
    c->tx_len = len;
    d[0] = (uint8_t)(0x10u | (len >> 8));
    d[1] = (uint8_t)len;
    memcpy(&d[2], data, 6u);
    if (!send_frame(c, d, 8u)) {
        return false;
    }
    c->tx_pos = 6u;
    c->tx_sn = 1u;
    c->tx_state = TX_WAIT_FC;
    c->tx_timer = N_BS_MS;
    return true;
}

bool isotp_tx_idle(const isotp_t *c)
{
    return c->tx_state == TX_IDLE;
}

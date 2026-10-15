/* Measurement protocol (spec 10), read-only subset. Command layouts follow the stock
 * handlers: dispatcher 0x1543F8, word read 0x154088, block read 0x154166, reject 0x1543D0,
 * periodic list 0x15456C / 0x154718 / 0x154800. */
#include "bsw/meas/meas.h"

#include <string.h>

#define RSP_POS     0x81u
#define RSP_REJECT  0x03u
#define DENIED_WORD 0xFFu       /* 0x20: both bytes of a denied word   */
#define DENIED_BYTE 0x0Eu       /* 0x28/0x2A: each denied byte          */
#define A16_BASE    0xFFF80000u
#define A24_BASE    0xFF000000u

static const uint16_t s_periodic_id[4] = { 0x368u, 0x378u, 0x388u, 0x398u };

static const meas_cfg_t *s_cfg;
static bool s_latched;
static can_bus_t s_bus;

static uint8_t s_rsp[8];
static bool s_rsp_pending;
static uint8_t s_ident_part;

static uint16_t s_list[MEAS_CHANNELS];     /* requested indices, 0 = unused */
static uint16_t s_list_frames;             /* bit n = frame n received */
static const uint8_t *s_chan[MEAS_CHANNELS];
static bool s_periodic;
static const uint8_t s_zero;

void meas_init(const meas_cfg_t *cfg)
{
    s_cfg = cfg;
    s_latched = false;
    s_rsp_pending = false;
    s_ident_part = 0u;
    s_periodic = false;
    s_list_frames = 0u;
    memset(s_list, 0, sizeof s_list);
    for (uint8_t b = 0u; b < (uint8_t)CAN_BUS_COUNT; b++) {
        (void)plat_can_rx_register((can_bus_t)b, MEAS_REQ_ID);
    }
}

static const meas_index_t *index_get(uint16_t i)
{
    if (s_cfg == NULL || i < MEAS_FIRST_INDEX || i - MEAS_FIRST_INDEX >= s_cfg->n_index) {
        return NULL;
    }
    return &s_cfg->index[i - MEAS_FIRST_INDEX];
}

static uint32_t index_addr(const meas_index_t *m)
{
    return (uint32_t)(uintptr_t)m->ptr;
}

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(p[0] << 8 | p[1]);
}

/* 0x20: up to three words at A16 addresses. */
static void cmd_read_words(const uint8_t *req)
{
    for (uint8_t i = 0u; i < 3u; i++) {
        uint8_t w[2];
        const uint32_t a = A16_BASE + be16(&req[2u + 2u * i]);

        if (plat_mem_read(a, w, 2u)) {
            s_rsp[2u + 2u * i] = w[0];
            s_rsp[3u + 2u * i] = w[1];
        } else {
            s_rsp[2u + 2u * i] = DENIED_WORD;
            s_rsp[3u + 2u * i] = DENIED_WORD;
        }
    }
}

/* 0x28: 6 bytes at a 16-bit absolute address; 0x2A: at a 32-bit address. */
static void cmd_read_block(const uint8_t *req)
{
    uint32_t a = be16(&req[2]);

    if (req[0] == 0x2Au) {
        a = (uint32_t)be16(&req[2]) << 16 | be16(&req[4]);
    }
    for (uint8_t i = 0u; i < 6u; i++) {
        if (!plat_mem_read(a + i, &s_rsp[2u + i], 1u)) {
            s_rsp[2u + i] = DENIED_BYTE;
        }
    }
}

/* 0x30: two A24 addresses, three bytes each. */
static void cmd_read_a24(const uint8_t *req)
{
    for (uint8_t k = 0u; k < 2u; k++) {
        const uint8_t *p = &req[2u + 3u * k];
        const uint32_t a = A24_BASE | (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
        for (uint8_t i = 0u; i < 3u; i++) {
            if (!plat_mem_read(a + i, &s_rsp[2u + 3u * k + i], 1u)) {
                s_rsp[2u + 3u * k + i] = DENIED_BYTE;
            }
        }
    }
}

/* 0x01 / 0x21: identification text in four 6-byte parts. */
static void cmd_ident(uint8_t cmd)
{
    const char *t = s_cfg != NULL && s_cfg->ident != NULL ? s_cfg->ident : "";
    const size_t len = strlen(t);

    s_ident_part = cmd == 0x01u ? 0u : (uint8_t)((s_ident_part + 1u) & 3u);
    s_rsp[0] = 0x21u;
    s_rsp[1] = (uint8_t)(s_ident_part + 1u);
    for (uint8_t i = 0u; i < 6u; i++) {
        const size_t k = (size_t)s_ident_part * 6u + i;
        s_rsp[2u + i] = k < len ? (uint8_t)t[k] : 0x00u;
    }
}

/* 0x11, 0x22, 0x32: addresses of measurement indices. */
static void cmd_index_addr(const uint8_t *req)
{
    const meas_index_t *m = index_get(be16(&req[2]));
    const uint32_t a = m != NULL ? index_addr(m) : 0xFFFFFFFFu;

    if (req[0] == 0x11u) {
        s_rsp[2] = (uint8_t)(a >> 24);
        s_rsp[3] = (uint8_t)(a >> 16);
        s_rsp[4] = (uint8_t)(a >> 8);
        s_rsp[5] = (uint8_t)a;
    } else if (req[0] == 0x22u) {
        const bool a16 = m != NULL && (a & 0xFFFF0000u) == A16_BASE;
        s_rsp[2] = a16 ? (uint8_t)(a >> 8) : 0xFFu;
        s_rsp[3] = a16 ? (uint8_t)a : 0xFFu;
    } else {
        for (uint8_t k = 0u; k < 2u; k++) {
            const meas_index_t *mk = index_get(be16(&req[2u + 2u * k]));
            const uint32_t ak = mk != NULL ? index_addr(mk) : 0xFFFFFFFFu;
            s_rsp[2u + 3u * k] = (uint8_t)(ak >> 16);
            s_rsp[3u + 3u * k] = (uint8_t)(ak >> 8);
            s_rsp[4u + 3u * k] = (uint8_t)ak;
        }
    }
}

/* Resolve the requested indices into byte channels (stock 0x154718). Two-byte variables take
 * two consecutive channels, high byte first. Unknown indices read as 0. */
static void list_build(void)
{
    uint8_t ch = 0u;

    for (uint8_t i = 0u; i < MEAS_CHANNELS; i++) {
        s_chan[i] = &s_zero;
    }
    for (uint8_t i = 0u; i < MEAS_CHANNELS && ch < MEAS_CHANNELS; i++) {
        const meas_index_t *m = s_list[i] != 0u ? index_get(s_list[i]) : NULL;
        const uint8_t *p = m != NULL ? (const uint8_t *)m->ptr : &s_zero;

        if (s_list[i] == 0u) {
            continue;
        }
        if (m != NULL && m->size == 2u) {
            uint16_t probe = 1u;
            const bool little = *(const uint8_t *)&probe == 1u;   /* host builds */
            s_chan[ch++] = little ? p + 1 : p;
            if (ch < MEAS_CHANNELS) {
                s_chan[ch++] = little ? p : p + 1;
            }
        } else {
            s_chan[ch++] = p;
        }
    }
}

/* 0xE0: b1 low nibble = frame number 1..11, three indices per frame; b1 = 0x81..0x8E marks
 * the last frame, after which periodic sending starts. */
static void cmd_list(const uint8_t *req)
{
    const uint8_t n = (uint8_t)(req[1] & 0x0Fu);

    if (n == 0u) {
        return;
    }
    if (s_list_frames == 0u) {
        memset(s_list, 0, sizeof s_list);
        s_periodic = false;
    }
    for (uint8_t k = 0u; k < 3u; k++) {
        const uint16_t pos = (uint16_t)(3u * (n - 1u) + k);
        if (pos < MEAS_CHANNELS) {
            s_list[pos] = be16(&req[2u + 2u * k]);
        }
    }
    s_list_frames = (uint16_t)(s_list_frames | 1u << n);
    if (req[1] > 0x80u && req[1] < 0x8Fu) {
        list_build();
        s_list_frames = 0u;
        s_periodic = true;
    }
}

static void send_rsp(void)
{
    can_frame_t f = { .id = MEAS_RSP_ID, .dlc = 8u };

    memcpy(f.data, s_rsp, 8u);
    if (plat_can_send(s_bus, &f)) {
        s_rsp_pending = false;
    }
}

bool meas_rx(can_bus_t bus, const can_frame_t *f)
{
    const uint8_t *req = f->data;

    if (f->id != MEAS_REQ_ID || f->dlc < 8u) {
        return false;
    }
    if (!s_latched) {
        s_latched = true;
        s_bus = bus;
    } else if (bus != s_bus) {
        return true;                        /* only the latched bus is served */
    }

    memset(s_rsp, 0, sizeof s_rsp);
    s_rsp[0] = req[0];
    s_rsp[1] = RSP_POS;
    switch (req[0]) {
    case 0x01u:
    case 0x21u: cmd_ident(req[0]); break;
    case 0x02u:                             /* reset protocol state; stops periodic data */
        s_rsp[0] = 0x01u;
        s_rsp[1] = 0x00u;
        s_periodic = false;
        s_list_frames = 0u;
        break;
    case 0x11u:
    case 0x22u:
    case 0x32u: cmd_index_addr(req); break;
    case 0x20u: cmd_read_words(req); break;
    case 0x28u:
    case 0x2Au: cmd_read_block(req); break;
    case 0x30u: cmd_read_a24(req); break;
    case 0xE0u: cmd_list(req); return true;  /* no response frame */
    default:                                /* writes, 0x08, 0x0E, 0x12, 0x23, 0x29, 0x33, ... */
        memset(s_rsp, 0, sizeof s_rsp);
        s_rsp[0] = RSP_REJECT;
        break;
    }
    s_rsp_pending = true;
    send_rsp();
    return true;
}

void meas_tick_10ms(void)
{
    if (s_rsp_pending) {
        send_rsp();
    }
    if (!s_periodic) {
        return;
    }
    for (uint8_t b = 0u; b < 4u; b++) {
        can_frame_t f = { .id = s_periodic_id[b], .dlc = 8u };
        for (uint8_t i = 0u; i < 8u; i++) {
            f.data[i] = *s_chan[8u * b + i];
        }
        (void)plat_can_send(s_bus, &f);
    }
}

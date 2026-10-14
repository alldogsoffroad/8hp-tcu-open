#include "bsw/diag/uds.h"
#include "bsw/diag/boot_record.h"
#include "bsw/diag/isotp.h"

#include <string.h>

#define SW_IDENT    "8HPTCU-OPEN M1"
#define S3_MS       5000u       /* non-default session falls back after this without traffic */
#define P2_MS       50u         /* reported in the session response */
#define P2STAR_MS   5000u
#define LEAVE_DELAY_MS 20u      /* let the last response leave the controller */

enum { SES_DEFAULT = 0x01u, SES_PROG = 0x02u, SES_EXT = 0x03u };

enum {
    NRC_SNS = 0x11u, NRC_SFNS = 0x12u, NRC_IMLOIF = 0x13u, NRC_CNC = 0x22u, NRC_ROOR = 0x31u,
    NRC_RCRRP = 0x78u, NRC_SFNSIAS = 0x7Eu, NRC_SNSIAS = 0x7Fu
};

enum { LEAVE_NONE = 0, LEAVE_RESET, LEAVE_BOOT };

static const isotp_cfg_t s_tp_cfg[CAN_BUS_COUNT] = {
    { CAN_BUS1, 0x7E1u, 0x7DFu, 0x7E9u },
    { CAN_BUS2, 0x7E1u, 0x000u, 0x7E9u },
};
static isotp_t s_tp[CAN_BUS_COUNT];

static const uds_app_t *s_app;
static uint8_t s_session = SES_DEFAULT;
static uint16_t s_s3;
static uint8_t s_leave;
static uint16_t s_leave_timer;
static uint8_t s_leave_sid, s_leave_sub;

static uint8_t s_rsp[ISOTP_BUF];

void uds_init(const uds_app_t *app)
{
    s_app = app;
    s_session = SES_DEFAULT;
    s_leave = LEAVE_NONE;
    for (uint8_t b = 0u; b < (uint8_t)CAN_BUS_COUNT; b++) {
        isotp_init(&s_tp[b], &s_tp_cfg[b]);
    }
}

uint8_t uds_session(void)
{
    return s_session;
}

bool uds_rx(can_bus_t bus, const can_frame_t *f)
{
    return bus < CAN_BUS_COUNT && isotp_rx_frame(&s_tp[bus], bus, f);
}

static bool leave_allowed(void)
{
    return s_app == NULL || s_app->leave_allowed == NULL || s_app->leave_allowed();
}

/* Each handler fills s_rsp (positive response, without the SID echo) and returns its length,
 * or returns a negative NRC. */
static int svc_session(const uint8_t *req, uint16_t len)
{
    const uint8_t sub = (uint8_t)(req[1] & 0x7Fu);

    if (len != 2u) {
        return -NRC_IMLOIF;
    }
    if (sub != SES_DEFAULT && sub != SES_EXT && sub != SES_PROG) {
        return -NRC_SFNS;
    }
    if (sub == SES_PROG) {
        if (!leave_allowed()) {
            return -NRC_CNC;
        }
        s_leave = LEAVE_BOOT;           /* response comes from the boot software */
        s_leave_sid = req[0];
        s_leave_sub = req[1];
        s_leave_timer = LEAVE_DELAY_MS;
        return -NRC_RCRRP;
    }
    s_session = sub;
    s_rsp[0] = sub;
    s_rsp[1] = (uint8_t)(P2_MS >> 8);
    s_rsp[2] = (uint8_t)P2_MS;
    s_rsp[3] = (uint8_t)((P2STAR_MS / 10u) >> 8);
    s_rsp[4] = (uint8_t)(P2STAR_MS / 10u);
    return 5;
}

static int svc_reset(const uint8_t *req, uint16_t len)
{
    if (len != 2u) {
        return -NRC_IMLOIF;
    }
    if ((req[1] & 0x7Fu) != 0x01u) {
        return -NRC_SFNS;
    }
    if (!leave_allowed()) {
        return -NRC_CNC;
    }
    s_leave = LEAVE_RESET;
    s_leave_timer = LEAVE_DELAY_MS;
    s_rsp[0] = 0x01u;
    return 1;
}

static int svc_tester_present(const uint8_t *req, uint16_t len)
{
    if (len != 2u) {
        return -NRC_IMLOIF;
    }
    if ((req[1] & 0x7Fu) != 0x00u) {
        return -NRC_SFNS;
    }
    s_rsp[0] = 0x00u;
    return 1;
}

static int read_did(uint16_t did, uint8_t *out, uint16_t max)
{
    if (did == 0xF195u) {                       /* software version */
        const uint16_t n = (uint16_t)(sizeof SW_IDENT - 1u);
        if (n > max) {
            return -1;
        }
        memcpy(out, SW_IDENT, n);
        return n;
    }
    if (s_app != NULL && s_app->read_did != NULL) {
        return s_app->read_did(did, out, max);
    }
    return -1;
}

static int svc_read_did(const uint8_t *req, uint16_t len)
{
    uint16_t pos = 0u;

    if (len < 3u || (len & 1u) == 0u) {
        return -NRC_IMLOIF;
    }
    for (uint16_t i = 1u; i + 1u < len; i += 2u) {
        const uint16_t did = (uint16_t)(req[i] << 8 | req[i + 1u]);
        int n;

        if (pos + 2u > sizeof s_rsp - 1u) {
            return -NRC_IMLOIF;
        }
        s_rsp[pos] = req[i];
        s_rsp[pos + 1u] = req[i + 1u];
        n = read_did(did, &s_rsp[pos + 2u], (uint16_t)(sizeof s_rsp - 1u - pos - 2u));
        if (n < 0) {
            return -NRC_ROOR;
        }
        pos = (uint16_t)(pos + 2u + (uint16_t)n);
    }
    return (int)pos;
}

static void process(uint8_t bus, const uint8_t *req, uint16_t len, isotp_addr_t addr)
{
    const uint8_t sid = req[0];
    const bool has_sub = sid == 0x10u || sid == 0x11u || sid == 0x3Eu;
    uint8_t out[ISOTP_BUF];
    int r;

    s_s3 = S3_MS;
    switch (sid) {
    case 0x10u: r = svc_session(req, len); break;
    case 0x11u: r = svc_reset(req, len); break;
    case 0x22u: r = svc_read_did(req, len); break;
    case 0x3Eu: r = svc_tester_present(req, len); break;
    default:    r = -NRC_SNS; break;
    }

    if (r < 0) {
        const uint8_t nrc = (uint8_t)-r;
        if (addr == ISOTP_FUNC &&
            (nrc == NRC_SNS || nrc == NRC_SFNS || nrc == NRC_ROOR ||
             nrc == NRC_SFNSIAS || nrc == NRC_SNSIAS)) {
            return;                             /* no negative response to functional requests */
        }
        out[0] = 0x7Fu;
        out[1] = sid;
        out[2] = nrc;
        (void)isotp_send(&s_tp[bus], out, 3u);
        return;
    }
    if (has_sub && len >= 2u && (req[1] & 0x80u) != 0u) {
        return;                                 /* suppress positive response */
    }
    out[0] = (uint8_t)(sid | 0x40u);
    memcpy(&out[1], s_rsp, (size_t)r);
    (void)isotp_send(&s_tp[bus], out, (uint16_t)(r + 1));
}

static void leave_tick(void)
{
    uint8_t rec[BOOT_REC_LEN];

    if (s_leave == LEAVE_NONE) {
        return;
    }
    if (s_leave_timer != 0u) {
        s_leave_timer--;
        return;
    }
    for (uint8_t b = 0u; b < (uint8_t)CAN_BUS_COUNT; b++) {
        if (!isotp_tx_idle(&s_tp[b])) {
            return;                             /* response still being sent */
        }
    }
    if (s_leave == LEAVE_BOOT) {
        boot_record_build_prog(rec, s_leave_sid, s_leave_sub);
        s_leave = LEAVE_NONE;
        plat_boot_handover(rec, BOOT_REC_LEN);  /* does not return on the target */
    } else {
        s_leave = LEAVE_NONE;
        plat_reset();
    }
}

void uds_tick_1ms(void)
{
    for (uint8_t b = 0u; b < (uint8_t)CAN_BUS_COUNT; b++) {
        const uint8_t *req;
        isotp_addr_t addr;
        uint16_t len;

        isotp_tick_1ms(&s_tp[b]);
        len = isotp_request(&s_tp[b], &req, &addr);
        if (len != 0u && s_leave == LEAVE_NONE && isotp_tx_idle(&s_tp[b])) {
            process(b, req, len, addr);
            isotp_request_done(&s_tp[b]);
        }
    }
    if (s_session != SES_DEFAULT && s_s3 != 0u && --s_s3 == 0u) {
        s_session = SES_DEFAULT;
    }
    leave_tick();
}

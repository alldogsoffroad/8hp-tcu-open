/* Flasher protocol core (spec 16 section 4). Hardware independent; runs from RAM on the target.
 * No libc, no division: the flasher is linked on its own without libgcc, so any call into the
 * application or the C library fails at link time instead of at run time. */
#include "flasher/fl_core.h"
#include "flasher/fl_fcu.h"
#include "flasher/fl_hal.h"
#include "flasher/fl_regs.h"

#define NRC_SNS    0x11u
#define NRC_SFNS   0x12u
#define NRC_IMLOIF 0x13u
#define NRC_CNC    0x22u
#define NRC_RSE    0x24u
#define NRC_ROOR   0x31u
#define NRC_UDNA   0x70u    /* target page not blank */
#define NRC_GPF    0x72u    /* FCU error or read-back mismatch */
#define NRC_WBSC   0x73u
#define NRC_RCRRP  0x78u

#define BUF_LEN    (2u + FL_PAGE_MAX + 8u)
#define FC_WAIT_MS 1000u
#define PENDING_MS 1000u

/* Optional extra ident byte (reply byte 12): the loader sets this to the FPMON value latched at
 * boot hand-over, before any port setup (spec 18 / FWE investigation). Default: not reported, so
 * the reply stays 12 bytes for the spec-16 RAM flasher and the host test. */
uint8_t fl_ident_aux;
bool fl_ident_aux_set;

/* ---- ISO-TP receive state ---- */
static uint8_t s_rx[BUF_LEN];
static uint16_t s_rx_len, s_rx_got;
static uint8_t s_rx_sn;
static bool s_rx_active;

/* flow control received for our own multi-frame responses */
static bool s_fc_seen;
static uint8_t s_fc[3];

/* ---- download state ---- */
static bool s_dl;
static uint32_t s_dl_next, s_dl_end;
static uint8_t s_dl_ctr;            /* counter expected next */
static bool s_dl_any;               /* at least one page of this download written */

/* ---- response pending during long operations ---- */
static uint8_t s_busy_sid;
static uint32_t s_busy_t;

static uint8_t s_page[FL_PAGE_MAX];
static const fl_target_t *s_t = &fl_target_rom;

static void copy(uint8_t *d, const uint8_t *s, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        d[i] = s[i];
    }
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

uint32_t fl_block_size(uint32_t a)
{
    if (a < 0x00010000u) {
        return 0x2000u;
    }
    if (a < 0x000A0000u) {
        return 0x10000u;
    }
    return 0x20000u;
}

static uint32_t df_block_size(uint32_t a)
{
    (void)a;
    return FL_DF_BLOCK;
}

const fl_target_t fl_target_rom = {
    { 'F', 'L' }, FL_WIN_LO, FL_WIN_HI, 0u, FL_ROM_END, FL_PAGE, true,
    fl_block_size, fl_fcu_erase, fl_fcu_program,
};

const fl_target_t fl_target_df = {
    { 'L', 'D' }, FL_DF_WIN_LO, FL_DF_WIN_HI, FL_DF_BASE, FL_DF_BASE + FL_DF_SIZE, FL_DF_PAGE, false,
    df_block_size, fl_fcu_df_erase, fl_fcu_df_program,
};

static void send_frame(const uint8_t *d, uint8_t n)
{
    uint8_t f[8];
    const uint32_t t0 = fl_hal_ms();

    for (uint8_t i = 0u; i < 8u; i++) {
        f[i] = i < n ? d[i] : 0xAAu;
    }
    while (!fl_hal_can_send(f)) {
        fl_hal_poll();
        if (fl_hal_ms() - t0 > 100u) {
            return;                     /* bus off or nobody acknowledging: drop */
        }
    }
}

/* Wait for a flow-control frame from the tester; frames other than FC are dropped. */
static bool wait_fc(void)
{
    const uint32_t t0 = fl_hal_ms();
    uint8_t d[8], dlc;

    s_fc_seen = false;
    while (fl_hal_ms() - t0 < FC_WAIT_MS) {
        fl_hal_poll();
        if (fl_hal_can_recv(d, &dlc) && dlc >= 3u && (d[0] & 0xF0u) == 0x30u) {
            if ((d[0] & 0x0Fu) == 0u) {
                copy(s_fc, d, 3u);
                return true;
            }
            if ((d[0] & 0x0Fu) == 2u) {
                return false;           /* overflow / abort */
            }
        }
    }
    return false;
}

static void delay_ms(uint32_t ms)
{
    const uint32_t t0 = fl_hal_ms();

    while (fl_hal_ms() - t0 < ms) {
        fl_hal_poll();
    }
}

static void send(const uint8_t *r, uint16_t n)
{
    uint8_t f[8];
    uint16_t off;
    uint8_t sn = 1u, bs_left;

    if (n <= 7u) {
        f[0] = (uint8_t)n;
        copy(&f[1], r, n);
        send_frame(f, (uint8_t)(n + 1u));
        return;
    }
    f[0] = (uint8_t)(0x10u | (n >> 8));
    f[1] = (uint8_t)n;
    copy(&f[2], r, 6u);
    send_frame(f, 8u);
    off = 6u;
    if (!wait_fc()) {
        return;
    }
    bs_left = s_fc[1];
    while (off < n) {
        uint16_t k = (uint16_t)(n - off);
        if (k > 7u) {
            k = 7u;
        }
        delay_ms(s_fc[2] <= 0x7Fu ? s_fc[2] : 1u);
        f[0] = (uint8_t)(0x20u | (sn & 0x0Fu));
        copy(&f[1], &r[off], k);
        send_frame(f, (uint8_t)(k + 1u));
        off = (uint16_t)(off + k);
        sn++;
        if (s_fc[1] != 0u && off < n && --bs_left == 0u) {
            if (!wait_fc()) {
                return;
            }
            bs_left = s_fc[1];
        }
    }
}

static void neg(uint8_t sid, uint8_t nrc)
{
    const uint8_t r[3] = { 0x7Fu, sid, nrc };
    send(r, 3u);
}

static void busy_hook(void)
{
    if (s_busy_sid != 0u && fl_hal_ms() - s_busy_t >= PENDING_MS) {
        s_busy_t = fl_hal_ms();
        neg(s_busy_sid, NRC_RCRRP);
    }
}

static void busy_start(uint8_t sid)
{
    s_busy_sid = sid;
    s_busy_t = fl_hal_ms();
    neg(sid, NRC_RCRRP);
}

/* ---- CRC-32 (zlib), nibble table ---- */
static uint32_t crc32_rom(uint32_t a, uint32_t n)
{
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
        0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    uint32_t c = 0xFFFFFFFFu;

    while (n != 0u) {
        const uint32_t k = n > FL_PAGE_MAX ? FL_PAGE_MAX : n;
        fl_fcu_read(a, s_page, k);
        for (uint32_t i = 0u; i < k; i++) {
            c ^= s_page[i];
            c = (c >> 4) ^ t[c & 0x0Fu];
            c = (c >> 4) ^ t[c & 0x0Fu];
        }
        a += k;
        n -= k;
        busy_hook();
        fl_hal_poll();
    }
    return ~c;
}

static bool in_window(uint32_t a, uint32_t n)
{
    return n != 0u && a >= s_t->win_lo && a < s_t->win_hi && n <= s_t->win_hi - a;
}

static bool blank(uint32_t a, uint32_t n)
{
    while (n != 0u) {
        const uint32_t k = n > FL_PAGE_MAX ? FL_PAGE_MAX : n;
        fl_fcu_read(a, s_page, k);
        for (uint32_t i = 0u; i < k; i++) {
            if (s_page[i] != 0xFFu) {
                return false;
            }
        }
        a += k;
        n -= k;
        fl_hal_poll();
    }
    return true;
}

/* ---- services ---- */
static void svc_read_did(const uint8_t *q, uint16_t n)
{
    uint8_t r[13] = { 0x62u, 0xF1u, 0x95u, 0u, 0u, FL_VERSION };

    if (n != 3u) {
        neg(0x22u, NRC_IMLOIF);
        return;
    }
    if (q[1] != 0xF1u || q[2] != 0x95u) {
        neg(0x22u, NRC_ROOR);
        return;
    }
    r[3] = (uint8_t)s_t->ident[0];
    r[4] = (uint8_t)s_t->ident[1];
    fl_fcu_diag(&r[6]);                     /* r[6] = live FPMON, r[7..11] = FCU status */
    if (fl_ident_aux_set) {
        r[12] = fl_ident_aux;               /* FPMON latched at boot hand-over (loader only) */
        send(r, 13u);
    } else {
        send(r, 12u);
    }
}

static void routine_crc(uint32_t a, uint32_t len)
{
    uint8_t r[8] = { 0x71u, 0x01u, 0xFFu, 0x01u };

    if (len == 0u || a < s_t->rd_lo || a >= s_t->rd_hi || len > s_t->rd_hi - a) {
        neg(0x31u, NRC_ROOR);
        return;
    }
    if (len > 0x10000u) {
        busy_start(0x31u);
    }
    put32(&r[4], crc32_rom(a, len));
    s_busy_sid = 0u;
    send(r, sizeof r);
}

static void routine_erase(uint32_t a, uint32_t len)
{
    static const uint8_t ok[4] = { 0x71u, 0x01u, 0xFFu, 0x00u };
    uint32_t b;

    if (!in_window(a, len)) {
        neg(0x31u, NRC_ROOR);
        return;
    }
    /* whole blocks only: a must start a block and a + len must end one */
    for (b = s_t->win_lo; b < a; b += s_t->block_size(b)) {
    }
    if (b != a) {
        neg(0x31u, NRC_ROOR);
        return;
    }
    for (b = a; b < a + len; b += s_t->block_size(b)) {
    }
    if (b != a + len) {
        neg(0x31u, NRC_ROOR);
        return;
    }
    s_dl = false;
    busy_start(0x31u);
    for (b = a; b < a + len; b += s_t->block_size(b)) {
        /* data flash: erased content is undefined, so only the FCU status is checked */
        if (s_t->erase(b) != FL_OK || (s_t->erased_ff && !blank(b, s_t->block_size(b)))) {
            s_busy_sid = 0u;
            neg(0x31u, NRC_GPF);
            return;
        }
    }
    s_busy_sid = 0u;
    send(ok, sizeof ok);
}

static void svc_routine(const uint8_t *q, uint16_t n)
{
    uint16_t rid;

    if (n < 4u) {
        neg(0x31u, NRC_IMLOIF);
        return;
    }
    rid = (uint16_t)(q[2] << 8 | q[3]);
    if (q[1] != 0x01u) {
        neg(0x31u, NRC_SFNS);
        return;
    }
    if (rid != 0xFF00u && rid != 0xFF01u) {
        neg(0x31u, NRC_ROOR);
        return;
    }
    if (n != 12u) {
        neg(0x31u, NRC_IMLOIF);
        return;
    }
    if (rid == 0xFF01u) {
        routine_crc(get32(&q[4]), get32(&q[8]));
    } else {
        routine_erase(get32(&q[4]), get32(&q[8]));
    }
}

static void svc_download(const uint8_t *q, uint16_t n)
{
    const uint16_t blk = (uint16_t)(2u + s_t->page);
    const uint8_t ok[4] = { 0x74u, 0x20u, (uint8_t)(blk >> 8), (uint8_t)blk };
    uint32_t a, len;

    if (n != 11u || q[1] != 0x00u || q[2] != 0x44u) {
        neg(0x34u, NRC_IMLOIF);
        return;
    }
    a = get32(&q[3]);
    len = get32(&q[7]);
    if (!in_window(a, len) || (a & (s_t->page - 1u)) != 0u || (len & (s_t->page - 1u)) != 0u) {
        neg(0x34u, NRC_ROOR);
        return;
    }
    s_dl = true;
    s_dl_next = a;
    s_dl_end = a + len;
    s_dl_ctr = 1u;
    s_dl_any = false;
    send(ok, sizeof ok);
}

/* One page, then read back. Code flash: blank check first, and a page of all 0xFF is only
 * checked. Data flash: erased content is undefined, so every page is programmed (the FCU refuses
 * a page that was not erased). */
static uint8_t write_page(uint32_t a, const uint8_t *p)
{
    const uint16_t pg = s_t->page;

    if (s_t->erased_ff) {
        bool all_ff = true;
        for (uint32_t i = 0u; i < pg; i++) {
            all_ff = all_ff && p[i] == 0xFFu;
        }
        if (!blank(a, pg)) {
            return all_ff ? NRC_GPF : NRC_UDNA;
        }
        if (all_ff) {
            return 0u;
        }
    }
    if (s_t->program(a, p) != FL_OK) {
        return NRC_GPF;
    }
    fl_fcu_read(a, s_page, pg);
    for (uint32_t i = 0u; i < pg; i++) {
        if (s_page[i] != p[i]) {
            return NRC_GPF;
        }
    }
    return 0u;
}

static void svc_transfer(const uint8_t *q, uint16_t n)
{
    uint8_t r[2] = { 0x76u, 0u };
    uint8_t nrc;

    if (!s_dl) {
        neg(0x36u, NRC_RSE);
        return;
    }
    if (n < 2u) {
        neg(0x36u, NRC_IMLOIF);
        return;
    }
    r[1] = q[1];
    if (q[1] == (uint8_t)(s_dl_ctr - 1u) && s_dl_any && n == 2u + s_t->page) {
        send(r, 2u);                    /* repeated request: already programmed */
        return;
    }
    if (q[1] != s_dl_ctr) {
        neg(0x36u, NRC_WBSC);
        return;
    }
    if (n != 2u + s_t->page || s_dl_next >= s_dl_end) {
        neg(0x36u, NRC_IMLOIF);
        return;
    }
    if (!in_window(s_dl_next, s_t->page)) {    /* cannot happen after 0x34; checked anyway */
        neg(0x36u, NRC_ROOR);
        return;
    }
    nrc = write_page(s_dl_next, &q[2]);
    if (nrc != 0u) {
        neg(0x36u, nrc);
        return;
    }
    s_dl_next += s_t->page;
    s_dl_ctr++;
    s_dl_any = true;
    send(r, 2u);
}

static void svc_exit(uint16_t n)
{
    static const uint8_t ok[1] = { 0x77u };

    if (n != 1u) {
        neg(0x37u, NRC_IMLOIF);
        return;
    }
    if (!s_dl) {
        neg(0x37u, NRC_RSE);
        return;
    }
    s_dl = false;
    send(ok, 1u);
}

static void svc_reset(const uint8_t *q, uint16_t n)
{
    static const uint8_t ok[2] = { 0x51u, 0x01u };
    const uint32_t t0 = fl_hal_ms();

    if (n != 2u || q[1] != 0x01u) {
        neg(0x11u, n != 2u ? NRC_IMLOIF : NRC_SFNS);
        return;
    }
    send(ok, 2u);
    while (!fl_hal_can_idle() && fl_hal_ms() - t0 < 50u) {
        fl_hal_poll();
    }
    fl_hal_reset();
}

static void dispatch(const uint8_t *q, uint16_t n)
{
    switch (q[0]) {
    case 0x3Eu:
        if (n == 2u && (q[1] & 0x7Fu) == 0u) {
            if (q[1] == 0x00u) {
                static const uint8_t ok[2] = { 0x7Eu, 0x00u };
                send(ok, 2u);
            }
        } else {
            neg(0x3Eu, n != 2u ? NRC_IMLOIF : NRC_SFNS);
        }
        break;
    case 0x22u: svc_read_did(q, n); break;
    case 0x31u: svc_routine(q, n); break;
    case 0x34u: svc_download(q, n); break;
    case 0x36u: svc_transfer(q, n); break;
    case 0x37u: svc_exit(n); break;
    case 0x11u: svc_reset(q, n); break;
    default:
        neg(q[0], NRC_SNS);
        break;
    }
}

void fl_core_init(const fl_target_t *t)
{
    s_t = t;
    fl_fcu_init();
    s_rx_active = false;
    s_dl = false;
    s_busy_sid = 0u;
    fl_fcu_busy_hook = busy_hook;
}

void fl_core_rx(const uint8_t d[8], uint8_t dlc)
{
    const uint8_t pci = (uint8_t)(d[0] >> 4);

    if (dlc == 0u) {
        return;
    }
    if (pci == 0u) {
        const uint8_t n = (uint8_t)(d[0] & 0x0Fu);
        if (n != 0u && n < dlc) {
            s_rx_active = false;
            copy(s_rx, &d[1], n);
            dispatch(s_rx, n);
        }
    } else if (pci == 1u && dlc == 8u) {
        static const uint8_t ovfl[3] = { 0x32u, 0u, 0u };
        /* STmin 1 ms: one receive mailbox, polled; back-to-back frames would overwrite it */
        static const uint8_t cts[3] = { 0x30u, 0u, 0x01u };
        s_rx_len = (uint16_t)((d[0] & 0x0Fu) << 8 | d[1]);
        if (s_rx_len < 8u || s_rx_len > BUF_LEN) {
            s_rx_active = false;
            send_frame(ovfl, 3u);
            return;
        }
        copy(s_rx, &d[2], 6u);
        s_rx_got = 6u;
        s_rx_sn = 1u;
        s_rx_active = true;
        send_frame(cts, 3u);
    } else if (pci == 2u && s_rx_active) {
        uint16_t k = (uint16_t)(s_rx_len - s_rx_got);
        if ((d[0] & 0x0Fu) != (s_rx_sn & 0x0Fu)) {
            s_rx_active = false;        /* lost frame: drop the request, tester times out */
            return;
        }
        if (k > 7u) {
            k = 7u;
        }
        if (k > (uint16_t)(dlc - 1u)) {
            s_rx_active = false;
            return;
        }
        copy(&s_rx[s_rx_got], &d[1], k);
        s_rx_got = (uint16_t)(s_rx_got + k);
        s_rx_sn++;
        if (s_rx_got >= s_rx_len) {
            s_rx_active = false;
            dispatch(s_rx, s_rx_len);
        }
    }
    /* flow control frames outside wait_fc() are ignored */
}

void fl_main_loop(void)
{
    uint8_t d[8], dlc;

    for (;;) {
        fl_hal_poll();
        if (fl_hal_can_recv(d, &dlc)) {
            fl_core_rx(d, dlc);
        }
#ifdef FL_HOST
        if (fl_sim_stop()) {
            return;
        }
#endif
    }
}

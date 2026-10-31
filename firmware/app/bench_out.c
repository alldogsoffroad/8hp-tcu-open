#include "app/bench_out.h"
#include "bsw/diag/uds.h"
#include "bsw/sup/sup.h"
#include "io/actuators/sol_drv.h"
#include "io/vehicle/vehicle.h"
#include "platform/platform.h"          /* plat_sup_xfer (FE13); plat_toe_out_* (FE11) */

#define NRC_IMLOIF 0x13
#define NRC_CNC    0x22
#define NRC_ROOR   0x31
#define NRC_SFNS   0x12
#define SES_EXT    0x03u
#define ENGINE_RUNNING_RPM 50

typedef enum { BK_SOL = 0, BK_PARK_OUT = 1 } bench_kind_t;

static bool s_active;
static bench_kind_t s_kind;
static uint8_t s_ch;                    /* FE10 channel or FE11 output selector */
static uint16_t s_ma;
static uint16_t s_duty;
static uint16_t s_left_10ms;
static uint8_t s_reason;

static bool engine_running(void)
{
    const vehicle_in_t *v = vehicle_in();
    return v->q_engine_speed == Q_OK && v->engine_speed >= ENGINE_RUNNING_RPM;
}

static bool drivers_healthy(void)
{
    const sol_status_t *s = sol_status();
    return s->ready && s->faults == 0u;
}

static bool supervisor_healthy(void)
{
    const sup_status_t *s = sup_status();
    return (s->last_rx & SUP_REPLY_MASK) == SUP_REPLY_OK;
}

/* Channels 0-6 are the bench channels: forced off whenever no test drives them.
 * Channel 7 and TOE31 belong to the park module (M4b builds); they are cleared here
 * only when a park commissioning test ends. */
static void bench_channels_off(void)
{
    for (uint8_t ch = 0u; ch <= BENCH_MAX_CHANNEL; ch++) {
        sol_set(ch, SOL_MODE_OFF, 0u);
    }
}

static void stop(uint8_t reason)
{
    if (s_active) {
        s_reason = reason;
#if defined(TCU_M4B_OUTPUTS)
        if (s_kind == BK_PARK_OUT) {
            sol_set(SOL_CH_PARK_RELEASE, SOL_MODE_OFF, 0u);
            plat_toe_out_set(0u);
        }
#endif
    }
    s_active = false;
    s_left_10ms = 0u;
    bench_channels_off();
    sup_enable_clear(SUP_EN_BENCH);
}

void bench_init(void)
{
    s_active = false;
    s_kind = BK_SOL;
    s_reason = BENCH_END_NONE;
    s_left_10ms = 0u;
    bench_channels_off();
    sup_enable_clear(SUP_EN_BENCH);
}

bool bench_active(void)
{
    return s_active;
}

void bench_tick_10ms(void)
{
    if (!s_active) {
        bench_channels_off();
        sup_enable_clear(SUP_EN_BENCH);
        return;
    }
    if (uds_session() != SES_EXT) {
        stop(BENCH_END_SESSION);
    } else if (engine_running()) {
        stop(BENCH_END_ENGINE);
    } else if (!drivers_healthy()) {
        stop(BENCH_END_DRIVER);
    } else if (!supervisor_healthy()) {
        stop(BENCH_END_SUPERVISOR);
    } else if (s_left_10ms == 0u || --s_left_10ms == 0u) {
        stop(BENCH_END_TIMEOUT);
    } else if (s_kind == BK_SOL) {
        bench_channels_off();
        sol_set(s_ch, SOL_MODE_CURRENT, (ma10_t)(s_ma * 10u));
#if defined(TCU_M4B_OUTPUTS)
    } else {                             /* BK_PARK_OUT */
        bench_channels_off();
        if (s_ch == 0u) {
            sol_set(SOL_CH_PARK_RELEASE, SOL_MODE_FULL_ON, 0u);
            plat_toe_out_set(0u);
        } else {
            plat_toe_out_set(s_duty);
        }
#endif
        /* No supervisor output-enable request: on this unit the 0x8E enable bit in the
         * keep-alive word RESETS the TCU (bench 2026-10-30 — every output test died
         * within 0.6 s at any current; [8E,01] = control word with the bit set was a
         * known lab reset). The actuator rail is live without it. */
    }
}

static int sol_start(const uint8_t *in, uint16_t n)
{
    uint16_t ma;

    if (n != 3u) {
        return -NRC_IMLOIF;
    }
    ma = (uint16_t)(in[1] * 10u);
    if (in[0] > BENCH_MAX_CHANNEL || ma == 0u || ma > BENCH_MAX_MA ||
        in[2] == 0u || in[2] > BENCH_MAX_S) {
        return -NRC_ROOR;
    }
    if (s_active || engine_running() || !drivers_healthy() || !supervisor_healthy()) {
        return -NRC_CNC;
    }
    s_kind = BK_SOL;
    s_ch = in[0];
    s_ma = ma;
    s_left_10ms = (uint16_t)(in[2] * 100u);
    s_reason = BENCH_END_NONE;
    s_active = true;
    return 0;
}

#if defined(TCU_M4B_OUTPUTS)
static int park_out_start(const uint8_t *in, uint16_t n)
{
    if (n != 2u && n != 3u) {
        return -NRC_IMLOIF;
    }
    s_duty = 1000u;
    if (n == 3u) {
        if (in[2] == 0u || in[2] > 100u) {
            return -NRC_ROOR;
        }
        s_duty = (uint16_t)(in[2] * 10u);           /* percent -> permille */
    }
    if (in[0] > 1u || in[1] == 0u || in[1] > BENCH_MAX_S) {
        return -NRC_ROOR;
    }
    if (s_active || engine_running() || !drivers_healthy() || !supervisor_healthy()) {
        return -NRC_CNC;
    }
    s_kind = BK_PARK_OUT;
    s_ch = in[0];
    s_left_10ms = (uint16_t)(in[1] * 100u);
    s_reason = BENCH_END_NONE;
    s_active = true;
    return 0;
}
#endif

/* FE12: RSPI-A diagnostic (no outputs, no interlocks). Dumps the driver-chip bus registers
 * and tries one 16-bit transfer with four protocol variants, reporting the poll loop counts
 * and final status per variant so the working CPU discipline can be identified:
 *   regs   SPCRA SSLPA SPPCRA SPSRA SPSSRA SPBRA SPCKDA SSLNDA SPNDA SPSCRA
 *          SPCMD0H/L .. SPCMD4L  ADMATCR56H/L ADMATCR57H/L                 (24 bytes)
 *   v1 baseline (disable, SPSR=0, enable, poll SPTEF, SPDR, poll SPRF)     (6 bytes)
 *   v2 v1 + the manual's start trigger: read SPSR, SPDR, read SPSR, write 0 (6 bytes)
 *   v3 full per-frame setup exactly as the working RSPI-B exchange, single
 *       command (SPSCR=0, SPCMD0=0xAF11, SPND=1), then SPDR, poll SPRF      (6 bytes)
 *   v4 v3 + the v2 trigger                                                 (6 bytes)
 * Each: result(0 ok,1 no-SPTEF,2 no-SPRF) tef_loops BE16 rxf_loops BE16 spsr rx BE16.
 */
#if defined(TCU_M4B_OUTPUTS)
#include "platform/sh7254r/regs.h"      /* RSPI-A registers for the FE12 diagnostic */

#define RSPIA_WAIT 20000u

typedef struct {
    uint16_t tef;
    uint16_t rxf;
    uint8_t  spsr;
} rspi_diag_t;

static rspi_diag_t s_diag;

static uint8_t rspi_variant(uint8_t v, uint16_t *rx)
{
    uint32_t guard;
    uint16_t tef = 0u, rxf = 0u;

    if (v >= 3u) {
        SPCRA = 0x00u;
        RSPIA_DMA_REQ0 = 0u;
        RSPIA_DMA_REQ1 = 0u;
        SPSRA = 0u;
        SPPCRA = 0u;
        SPBRA = 5u;
        SPCKDA = 0u;
        SSLNDA = 0u;
        SPNDA = 1u;
        SPSCRA = 0u;
        SPCMDA(0) = 0xAF11u;
        if (v == 5u) {
            SPCMDA(0) = 0xAF31u;            /* SSLA3 instead of SSLA1  */
        } else if (v == 6u) {
            SPSCRA = 4u;                    /* the driver's sequence   */
            SPCMDA(0) = 0xAF31u;
            SPCMDA(1) = 0xAF21u;
            SPCMDA(2) = 0xAF11u;
            SPCMDA(3) = 0xAF01u;
            SPCMDA(4) = 0xAF51u;
        }
        SPCRA = 0x48u;
    } else {
        SPCRA = 0x08u;
        SPSRA = 0u;
        SPCRA = 0x48u;
    }
    for (guard = RSPIA_WAIT; guard != 0u && (SPSRA & SPSR_SPTEF) == 0u; guard--) {
    }
    tef = (uint16_t)(RSPIA_WAIT - guard);
    if (guard != 0u) {
        (void)SPSRA;                        /* read SPTEF = 1 (write prerequisite) */
        SPDRA = 0x8800u;
        if (v == 2u || v == 4u) {
            (void)SPSRA;                    /* read 1 ...                          */
            SPSRA = 0u;                     /* ... then write 0: the start trigger  */
        }
        for (guard = RSPIA_WAIT; guard != 0u && (SPSRA & SPSR_SPRF) == 0u; guard--) {
        }
        rxf = (uint16_t)(RSPIA_WAIT - guard);
        if (guard != 0u) {
            *rx = SPDRA;
        }
    }
    s_diag.tef = tef;
    s_diag.rxf = rxf;
    s_diag.spsr = SPSRA;
    if (tef >= RSPIA_WAIT) {
        return 1u;
    }
    return rxf >= RSPIA_WAIT ? 2u : 0u;
}

static int rspi_diag(uint8_t type, const uint8_t *in, uint16_t n, uint8_t *out, uint16_t max)
{
    uint16_t rx[6] = { 0u, 0u, 0u, 0u, 0u, 0u };
    uint8_t r[6];

    (void)in;
    (void)n;
    if (type != 1u || max < 72u) {
        return -NRC_IMLOIF;
    }
    out[0] = SPCRA;
    out[1] = REG8(0xFFFFB001u);          /* SSLPA  */
    out[2] = SPPCRA;
    out[3] = SPSRA;
    out[4] = REG8(0xFFFFB009u);          /* SPSSRA */
    out[5] = SPBRA;
    out[6] = SPCKDA;
    out[7] = SSLNDA;
    out[8] = REG8(0xFFFFB00Eu);          /* SPNDA  */
    out[9] = SPSCRA;
    for (uint8_t i = 0u; i < 5u; i++) {
        out[10u + 2u * i] = (uint8_t)(SPCMDA(i) >> 8);
        out[11u + 2u * i] = (uint8_t)SPCMDA(i);
    }
    out[20] = (uint8_t)(RSPIA_DMA_REQ0 >> 8);
    out[21] = (uint8_t)RSPIA_DMA_REQ0;
    out[22] = (uint8_t)(RSPIA_DMA_REQ1 >> 8);
    out[23] = (uint8_t)RSPIA_DMA_REQ1;

    for (uint8_t v = 0u; v < 6u; v++) {
        r[v] = rspi_variant((uint8_t)(v + 1u), &rx[v]);
        out[24u + v * 8u] = r[v];
        out[25u + v * 8u] = (uint8_t)(s_diag.tef >> 8);
        out[26u + v * 8u] = (uint8_t)s_diag.tef;
        out[27u + v * 8u] = (uint8_t)(s_diag.rxf >> 8);
        out[28u + v * 8u] = (uint8_t)s_diag.rxf;
        out[29u + v * 8u] = s_diag.spsr;
        out[30u + v * 8u] = (uint8_t)(rx[v] >> 8);
        out[31u + v * 8u] = (uint8_t)rx[v];
    }
    /* leave the registers as the driver layer expects them */
    SPCRA = 0x00u;
    SPSRA = 0u;
    SPSCRA = 4u;
    SPCMDA(0) = 0xAF31u;
    SPCMDA(1) = 0xAF21u;
    SPCMDA(2) = 0xAF11u;
    SPCMDA(3) = 0xAF01u;
    SPCMDA(4) = 0xAF51u;
    SPNDA = 0u;
    SPCRA = 0x48u;
    return 72;
}


/* FE12 type 3: driver-chip command map on chip 0 (SSLA3). For every config-style write
 * command: write a probe value, then read it back with every read command, so the real
 * write/read pairing and any unlock requirement become visible. Response words (BE16):
 *   phase 1 baseline reads:  06 00 18 1E 28 2E 30 36 60 66 C0 D8        (12 words)
 *   phase 2 write+readback:  for each W in {8E,88,90,96,A0,A6,E8,F0}:     (8 x 9 words)
 *       W->0x5A, then reads 06 18 1E 28 2E 30 36 60 66  (W echo may be any of them)
 *   phase 3 candidate special writes 01 11 21 50 51 70 71 80 -> then reads 28 30 18
 *       (8 x 3 words)                                                    (24 words)
 * Total 108 words = 216 bytes.
 */
#define NMAP_W 8u
#define NMAP_R 12u
static const uint16_t map_w[NMAP_W] = { 0x8Eu, 0x88u, 0x90u, 0x96u, 0xA0u, 0xA6u, 0xE8u, 0xF0u };
static const uint16_t map_r1[NMAP_R] = { 0x0600u, 0x0000u, 0x1800u, 0x1E00u, 0x2800u, 0x2E00u,
                                         0x3000u, 0x3600u, 0x6000u, 0x6600u, 0xC000u, 0xD800u };

static void map_xfer(const uint16_t *tx, uint8_t n, uint16_t *out_words)
{
    (void)plat_drv_xfer(tx, out_words, n);
}

static int rspi_map(uint8_t type, uint8_t *out, uint16_t max)
{
    uint16_t rx[12];
    uint16_t tx[10];
    uint8_t k = 0;

    (void)type;
    if (max < 216u) {
        return -NRC_IMLOIF;
    }
    /* phase 1: baseline reads (12 commands, one frame each to chip 0) */
    for (uint8_t i = 0u; i < NMAP_R; i++) {
        tx[0] = map_r1[i];
        map_xfer(tx, 1u, rx);
        out[2u * i] = (uint8_t)(rx[0] >> 8);
        out[2u * i + 1u] = (uint8_t)rx[0];
    }
    k = 24u;
    /* phase 2: write probe, read back with the 9 read commands */
    for (uint8_t w = 0u; w < NMAP_W; w++) {
        tx[0] = (uint16_t)(map_w[w] << 8) | 0x5Au;
        map_xfer(tx, 1u, rx);                     /* the write */
        static const uint16_t r2[9] = { 0x0600u, 0x1800u, 0x1E00u, 0x2800u, 0x2E00u,
                                        0x3000u, 0x3600u, 0x6000u, 0x6600u };
        for (uint8_t i = 0u; i < 9u; i++) {
            tx[0] = r2[i];
            map_xfer(tx, 1u, rx);
            out[k] = (uint8_t)(rx[0] >> 8);
            out[k + 1u] = (uint8_t)rx[0];
            k += 2u;
        }
    }
    /* phase 3: candidate unlock/watchdog words, then status reads */
    static const uint16_t w3[8] = { 0x0100u, 0x1100u, 0x2100u, 0x5000u,
                                    0x5100u, 0x7000u, 0x7100u, 0x8000u };
    static const uint16_t r3[3] = { 0x2800u, 0x3000u, 0x1800u };
    for (uint8_t w = 0u; w < 8u; w++) {
        tx[0] = w3[w];
        map_xfer(tx, 1u, rx);
        for (uint8_t i = 0u; i < 3u; i++) {
            tx[0] = r3[i];
            map_xfer(tx, 1u, rx);
            out[k] = (uint8_t)(rx[0] >> 8);
            out[k + 1u] = (uint8_t)rx[0];
            k += 2u;
        }
    }
    return (int)k;
}


/* FE12 type 4: RSPI-A sequence lab. Request: <n words, big-endian, n = 1..100>; runs the
 * words as one exchange (full setup per exchange as the driver layer does) and returns the
 * n replies, big-endian. Lets the bench iterate chip-command hypotheses live. */
static int rspi_lab(const uint8_t *in, uint16_t n, uint8_t *out, uint16_t max)
{
    uint16_t tx[100];
    uint16_t rx[100];

    if (n < 1u || n > 100u || n % 2u != 0u || max < n) {
        return -NRC_IMLOIF;
    }
    for (uint16_t i = 0u; i < n / 2u; i++) {
        tx[i] = (uint16_t)(((uint16_t)in[2u * i] << 8) | in[2u * i + 1u]);
    }
    if (!plat_drv_xfer(tx, rx, (uint8_t)(n / 2u))) {
        return -NRC_CNC;
    }
    for (uint16_t i = 0u; i < n / 2u; i++) {
        out[2u * i] = (uint8_t)(rx[i] >> 8);
        out[2u * i + 1u] = (uint8_t)rx[i];
    }
    return (int)n;
}

/* FE12 type 2: raw reply dump of the driver layer's ID job (0x0600, 0x0000 to every chip)
 * and configuration job, so the real reply formats can be compared with the [I] checks in
 * sol_drv.c (spec 12 section 2b). Response: 50 words, big-endian: 10 ID + 40 config. */
static int rspi_dump(uint8_t type, uint8_t *out, uint16_t max)
{
    static const uint16_t id_tx[10] = {
        0x0600u, 0x0600u, 0x0600u, 0x0600u, 0x0600u,
        0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u,
    };
    const uint16_t cfg_tx[40] = {
        0x8E00u, 0x8E00u, 0x8E00u, 0x8E00u, 0x8E00u,     /* CFG_8E = 0            */
        0x9021u, 0x9021u, 0x9021u, 0x9021u, 0x9021u,     /* CFG_90 = 0x21         */
        0x9655u, 0x9655u, 0x9655u, 0x9655u, 0x9655u,     /* CFG_96 = 0x55         */
        0x8810u, 0x8810u, 0x8810u, 0x8810u, 0x8810u,     /* mode bytes, bit 4 set */
        0x2800u, 0x2800u, 0x2800u, 0x2800u, 0x2800u,
        0x3000u, 0x3000u, 0x3000u, 0x3000u, 0x3000u,
        0x2E00u, 0x2E00u, 0x2E00u, 0x2E00u, 0x2E00u,
        0x3600u, 0x3600u, 0x3600u, 0x3600u, 0x3600u,
    };
    uint16_t rx[40];
    bool ok;

    (void)type;
    if (max < 100u) {
        return -NRC_IMLOIF;
    }
    ok = plat_drv_xfer(id_tx, rx, 10u);
    for (uint8_t i = 0u; i < 10u; i++) {
        out[2u * i] = (uint8_t)(rx[i] >> 8);
        out[2u * i + 1u] = (uint8_t)rx[i];
    }
    ok = ok && plat_drv_xfer(cfg_tx, rx, 40u);
    for (uint8_t i = 0u; i < 40u; i++) {
        out[20u + 2u * i] = (uint8_t)(rx[i] >> 8);
        out[21u + 2u * i] = (uint8_t)rx[i];
    }
    return ok ? 100 : -1;
}
#endif

/* ---- FE13: RSPI-B supervisor lab (spec 03 section 4.4) -------------------------------
 * One-shot raw exchanges with the stock two-command sequence (SSLB1 / SSLB0 alternating)
 * and a sustained replay of the stock application's cyclic supervisor jobs, so the bench
 * can iterate supervisor hypotheses live. Words with 0x8E bit 0 (output enable) or bit 7
 * are the danger words: they reset or re-flash the TCU when sent without the full context. */
#define SUP_LAB_MAX 100u

static uint16_t s_sup_rx[SUP_LAB_MAX];
static uint8_t  s_sup_n;
static uint8_t  s_sup_mode;            /* 0 idle, 1 one-shot done, 2 stock pattern on */

static int sup_lab_start(const uint8_t *in, uint16_t n)
{
    if (n < 1u) {
        return -NRC_IMLOIF;
    }
    if (in[0] == 1u) {                                     /* one-shot raw exchange */
        const uint16_t bytes = (uint16_t)(n - 1u);
        if (bytes < 2u || bytes > 2u * SUP_LAB_MAX || (bytes & 1u) != 0u) {
            return -NRC_IMLOIF;
        }
        {
            uint16_t tx[SUP_LAB_MAX];
            const uint8_t cnt = (uint8_t)(bytes / 2u);
            for (uint8_t i = 0u; i < cnt; i++) {
                tx[i] = (uint16_t)(((uint16_t)in[1u + 2u * i] << 8) | in[2u + 2u * i]);
            }
            if (!plat_sup_xfer(tx, s_sup_rx, cnt)) {
                return -NRC_CNC;
            }
            s_sup_n = cnt;
            s_sup_mode = 1u;
        }
        return 0;
    }
    if (in[0] == 4u) {                                     /* SSLB1-only burst (0x1D478 shape) */
        const uint16_t bytes = (uint16_t)(n - 1u);
        if (bytes < 2u || bytes > 2u * SUP_LAB_MAX || (bytes & 1u) != 0u) {
            return -NRC_IMLOIF;
        }
        {
            uint16_t tx[SUP_LAB_MAX];
            const uint8_t cnt = (uint8_t)(bytes / 2u);
            for (uint8_t i = 0u; i < cnt; i++) {
                tx[i] = (uint16_t)(((uint16_t)in[1u + 2u * i] << 8) | in[2u + 2u * i]);
            }
            if (!plat_sup_burst(tx, s_sup_rx, cnt)) {
                return -NRC_CNC;
            }
            s_sup_n = cnt;
            s_sup_mode = 1u;
        }
        return 0;
    }
    if (in[0] == 7u) {                                     /* pattern incl. SSLB2 */
        if (n != 3u) {
            return -NRC_IMLOIF;
        }
        sup_app_pattern2(true, in[1], in[2], true);
        s_sup_mode = 2u;
        return 0;
    }
    if (in[0] == 5u) {                                     /* watchdog mode switch */
        if (n != 2u) {
            return -NRC_IMLOIF;
        }
        plat_wdt_mode(in[1] != 0u);
        s_sup_mode = 0u;
        return 0;
    }
    if (in[0] == 6u) {                                     /* SSLB2-only burst (mode 0) */
        const uint16_t bytes = (uint16_t)(n - 1u);
        if (bytes < 2u || bytes > 2u * SUP_LAB_MAX || (bytes & 1u) != 0u) {
            return -NRC_IMLOIF;
        }
        {
            uint16_t tx[SUP_LAB_MAX];
            const uint8_t cnt = (uint8_t)(bytes / 2u);
            for (uint8_t i = 0u; i < cnt; i++) {
                tx[i] = (uint16_t)(((uint16_t)in[1u + 2u * i] << 8) | in[2u + 2u * i]);
            }
            if (!plat_sup_burst2(tx, s_sup_rx, cnt)) {
                return -NRC_CNC;
            }
            s_sup_n = cnt;
            s_sup_mode = 1u;
        }
        return 0;
    }
    if (in[0] == 2u) {                                     /* stock cyclic pattern on */
        if (n != 3u) {
            return -NRC_IMLOIF;
        }
        sup_app_pattern(true, in[1], in[2]);
        s_sup_mode = 2u;
        return 0;
    }
    if (in[0] == 3u) {                                     /* pattern off */
        sup_app_pattern(false, 0u, 0u);
        s_sup_mode = 0u;
        return 0;
    }
    return -NRC_SFNS;
}

static int sup_lab_results(uint8_t *out, uint16_t max)
{
    const sup_status_t *st = sup_status();

    if (s_sup_mode == 1u) {                                /* last one-shot replies */
        const uint16_t need = (uint16_t)(2u + 2u * s_sup_n);
        if (max < need) {
            return -NRC_IMLOIF;
        }
        out[0] = 1u;
        out[1] = s_sup_n;
        for (uint8_t i = 0u; i < s_sup_n; i++) {
            out[2u + 2u * i] = (uint8_t)(s_sup_rx[i] >> 8);
            out[3u + 2u * i] = (uint8_t)s_sup_rx[i];
        }
        return (int)need;
    }
    if (s_sup_mode == 2u) {                                /* pattern status + replies */
        if (max < 25u) {
            return -NRC_IMLOIF;
        }
        out[0] = 2u;
        for (uint8_t i = 0u; i < 8u; i++) {
            out[1u + 2u * i] = (uint8_t)(st->rx_words[i] >> 8);
            out[2u + 2u * i] = (uint8_t)st->rx_words[i];
        }
        out[17] = (uint8_t)(st->echo_ok >> 8);  out[18] = (uint8_t)st->echo_ok;
        out[19] = (uint8_t)(st->echo_bad >> 8); out[20] = (uint8_t)st->echo_bad;
        out[21] = (uint8_t)(st->a8_bad >> 8);   out[22] = (uint8_t)st->a8_bad;
        out[23] = (uint8_t)(st->aa_bad >> 8);   out[24] = (uint8_t)st->aa_bad;
        out[25] = (uint8_t)(st->sslb2_rx >> 8); out[26] = (uint8_t)st->sslb2_rx;
        return 27;
    }
    if (max < 1u) {
        return -NRC_IMLOIF;
    }
    out[0] = 0u;
    return 1;
}

int bench_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                  uint16_t max)
{
    const bool sol_rid = rid == BENCH_RID;
#if defined(TCU_M4B_OUTPUTS)
    const bool park_rid = rid == BENCH_PARK_RID;
#else
    const bool park_rid = false;
#endif

    if (rid == BENCH_SUP_RID) {
        if (type == 1u) {
            return sup_lab_start(in, n);
        }
        if (type == 2u) {
            sup_app_pattern(false, 0u, 0u);
            s_sup_mode = 0u;
            return 0;
        }
        return sup_lab_results(out, max);
    }
#if defined(TCU_M4B_OUTPUTS)
    if (rid == BENCH_RSPI_DIAG_RID) {
        if (type == 4u) {
            return rspi_lab(in, n, out, max);
        }
        return type == 1u ? rspi_diag(type, in, n, out, max)
             : type == 2u ? rspi_dump(type, out, max)
             : type == 3u ? rspi_map(type, out, max)
             : -NRC_SFNS;
    }
#endif
    if (!sol_rid && !park_rid) {
        return -NRC_ROOR;
    }
    if (type == 1u) {                               /* start */
        if (sol_rid) {
            return sol_start(in, n);
        }
#if defined(TCU_M4B_OUTPUTS)
        return park_out_start(in, n);
#else
        (void)in;
        (void)n;
        return -NRC_SFNS;                           /* park outputs not built in */
#endif
    }
    if (type == 2u) {                               /* stop */
        stop(BENCH_END_STOPPED);
        return 0;
    }
    if (max < 7u) {                                 /* results */
        return -NRC_IMLOIF;
    }
    out[0] = s_active ? 1u : 0u;
    if (park_rid) {
        out[1] = s_kind == BK_PARK_OUT ? s_ch : 0xFFu;
        out[2] = (uint8_t)(s_duty >> 8);
        out[3] = (uint8_t)s_duty;
    } else {
        out[1] = s_ch;
        out[2] = (uint8_t)(s_ma >> 8);
        out[3] = (uint8_t)s_ma;
    }
    out[4] = (uint8_t)(s_left_10ms / 10u > 255u ? 255u : s_left_10ms / 10u);
    out[5] = s_reason;
    out[6] = sol_status()->faults;
    return 7;
}

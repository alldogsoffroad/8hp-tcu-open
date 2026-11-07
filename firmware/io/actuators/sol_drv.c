/* Solenoid driver chip jobs. Frame layouts and reply checks follow the stock builders and
 * completion handlers (spec 12 section 2b): ID 0x921A0/0x921E0, configuration 0x92344/0x923BC,
 * status 0x9225C/0x92298, cyclic 0x924B0/0x92530. Frame n goes to chip n mod 5. */
#include "io/actuators/sol_drv.h"

#include <string.h>

#define HDR_BYTE_MASK   0xFF00u     /* ID / status / configuration replies */
#define HDR_BYTE        0xA200u
#define HDR_CYC_MASK    0xF800u     /* cyclic replies: 5-bit header + 11 data bits */
#define HDR_CYC         0xA000u

/* Stock driver configuration bytes (calibration 0x1918B4..0x1918BC, spec 12 section 2b). The
 * 0x8E dither-divider byte keeps its initial value in the stock software [I]; dither is not used. */
#define CFG_8E          0x9Fu    /* E44 (spec 03 6.2ae): the stock's engaged write job
                                  * carries 0x8E = 0x9F from the first capture — the
                                  * 1 kHz PWM divider + the chips' output-enable class.
                                  * The old 0x00 ("the divider keeps its initial value
                                  * [I]; dither not used") was an inference; the ~50 mA
                                  * delivery cap at any commanded load (E43) says the
                                  * outputs were never enabled. */
#define CFG_90          0x21u
#define CFG_96          0x55u
#define MODE_FLAG_BIT4  0x10u       /* calibration 0x1918B9 */

static sol_status_t s_st;
static uint8_t s_req_mode[SOL_CHANNELS];
static ma10_t s_req_cur[SOL_CHANNELS];
static uint8_t s_div;
static uint8_t s_eng_level;      /* supervisor ladder level mirrored into the 0x8E row */
static bool    s_cfg_pending;    /* sol_engage_config() staged a re-send               */
static bool    s_stream_on;      /* E29 (spec 03 section 6.2s): bus-0 traffic gate. Every
                                  * boot we have ever run interleaved the 45-word cyclic
                                  * (10 ms slot) and the ID polls with the 5 ms supervisor
                                  * pass - the RSPI-A burst can push one pass ~20 ms late,
                                  * and E28 proved the supervisor RESETS the MCU for a
                                  * stalled pass once granted. Default OFF: the pure
                                  * supervisor stream from boot (the rail bit has never
                                  * been observed on any stream that carried bus-0 noise).
                                  * FE13 sub 15 toggles it live for the A/B. */

static uint16_t s_tx[45];
static uint16_t s_rx[45];

uint16_t sol_setpoint(ma10_t current)
{
    /* setpoint = current / 6 (0.6 mA steps [I]), 11 bits; stock 0x90118 */
    const uint32_t c = current > SOL_MAX_MA10 ? SOL_MAX_MA10 : current;
    return (uint16_t)(c / 6u);
}

static uint8_t mode_byte(uint8_t chip)
{
    return (uint8_t)(MODE_FLAG_BIT4 | (s_st.mode[2u * chip] & 3u) |
                     (uint8_t)((s_st.mode[2u * chip + 1u] & 3u) << 2));
}

static bool xfer(uint8_t n)
{
#if defined(TCU_AUTOENGAGE)
    /* E58 (spec 03 6.2ap): the stock feed with the FULL ISR protocol —
     * SPSR=0x20 + the clock-gate ENABLE at 0xFFFE6030=0xF3. */
    if (!plat_drv_xfer_stock(s_tx, s_rx, n)) {
        s_st.faults |= SOL_F_BUS;
        return false;
    }
    return true;
#else
    if (!plat_drv_xfer(s_tx, s_rx, n)) {
        s_st.faults |= SOL_F_BUS;
        return false;
    }
    return true;
#endif
}

/* fill frames [5k .. 5k+4] with command cmd for every chip */
static void put_cmd(uint8_t k, uint16_t cmd)
{
    for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
        s_tx[5u * k + c] = cmd;
    }
}

#define SOL_CHIPS_REAL 4u              /* bench 2026-10-30: only chips 0-3 populated; the
                                        * fifth select (SSLA5) reads 0x0000 (no device) */

/* Header check over the four real chips: frame i belongs to chip i % 5. */
static bool real_chip_headers(uint8_t frames)
{
    for (uint8_t i = 0u; i < frames; i++) {
        if (i % 5u < SOL_CHIPS_REAL && (s_rx[i] & HDR_BYTE_MASK) != HDR_BYTE) {
            return false;
        }
    }
    return true;
}

static void job_id_once(void)
{
    put_cmd(0u, 0x0600u);
    put_cmd(1u, 0x0000u);
    (void)xfer(10u);
}

/* The first exchange after power-up can misalign (bench 2026-10-30: the boot-time ID job
 * fails the header check while the identical frames pass seconds later) — retry a few
 * times before latching the fault. */
static void job_id(void)
{
    for (uint8_t attempt = 0u; attempt < 5u; attempt++) {
        job_id_once();
        if (real_chip_headers(10u)) {
            for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
                s_st.chip[c].id[0] = (uint8_t)s_rx[c];
                s_st.chip[c].id[1] = (uint8_t)s_rx[5u + c];
            }
            return;
        }
        for (volatile uint32_t d = 0u; d < 20000u; d++) {
            if ((d & 0x3FFu) == 0u) {
                plat_wdt_service();
            }
        }
    }
    s_st.faults |= SOL_F_ID;
}

static void job_config(uint8_t v8e)
{
    put_cmd(0u, 0x8E00u | v8e);
    put_cmd(1u, 0x9000u | CFG_90);
    put_cmd(2u, 0x9600u | CFG_96);
    for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
        s_tx[15u + c] = (uint16_t)(0x8800u | mode_byte(c));
    }
    put_cmd(4u, 0x2800u);
    put_cmd(5u, 0x3000u);
    put_cmd(6u, 0x2E00u);
    put_cmd(7u, 0x3600u);
    if (!xfer(40u)) {
        return;
    }
    if (!real_chip_headers(40u)) {
        s_st.faults |= SOL_F_CONFIG;
        return;
    }
    /* read-backs as stock 0x923BC checks them: frame 20+c (low 6 bits) = mode byte of
     * frame 15+c, frame 25+c = 0x90 data, frame 30+c = 0x8E data, frame 35+c = 0x96 data.
     * Bench 2026-10-30: the four chips accept the writes (0xA2 headers) but these reads
     * return 0x00 in the data byte, so the reply format is open; count mismatches in
     * status instead of latching a fault until it is decoded (spec 12 section 2b). */
    for (uint8_t c = 0u; c < SOL_CHIPS_REAL; c++) {
        if ((s_rx[20u + c] & 0x3Fu) != (s_tx[15u + c] & 0x3Fu) ||
            (uint8_t)s_rx[25u + c] != (uint8_t)s_tx[5u + c] ||
            (uint8_t)s_rx[30u + c] != (uint8_t)s_tx[c] ||
            (uint8_t)s_rx[35u + c] != (uint8_t)s_tx[10u + c]) {
            s_st.cfg_mismatch++;
        }
    }
}

static void job_status(void)
{
    put_cmd(0u, 0x1800u);
    put_cmd(1u, 0x1E00u);
    put_cmd(2u, 0x2800u);
    if (!xfer(15u)) {
        return;
    }
    if (!real_chip_headers(15u)) {
        s_st.faults |= SOL_F_HEADER;
        return;
    }
    for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
        s_st.chip[c].status_ch[0] = (uint8_t)s_rx[c];
        s_st.chip[c].status_ch[1] = (uint8_t)s_rx[5u + c];
        s_st.chip[c].status_chip = (uint8_t)s_rx[10u + c];
    }
}

static void job_cyclic(void)
{
#if defined(TCU_AUTOENGAGE)
    /* E53 (spec 03 section 6.2aj): THE STOCK'S OWN WORDS on the driver cyclic.
     * The 2026-11-08 capture (0xFFF88518, engaged at 1.786 A) — the driver
     * cyclic rides THIS bus (RSPI-A): our bus-B probe found no A2-class chips
     * there, the stock's ID job reports four — the 0x40CE4 table values are
     * peripheral-relative and the job belongs to the A-peripheral's five
     * selects. Our invented values (mode staging, 0x86-class 0x8E rows,
     * keep-warm staging) never matched; the ~50 mA delivery cap at any
     * command (E43/E44) was our config, not the rail. VERBATIM rows: the
     * stock's modes {1A,1A,1A,12,15} and setpoints {53,589,589,53,0}/
     * {589,589,53,0,0} per chip. */
    {
        static const uint16_t STOCK_CYC[45] = {
            0x881Au, 0x881Au, 0x881Au, 0x8812u, 0x8815u,
            0xA000u, 0xA000u, 0xA000u, 0xA000u, 0xA000u,
            0xE853u, 0xED89u, 0xED89u, 0xE853u, 0xE800u,
            0xA600u, 0xA600u, 0xA600u, 0xA600u, 0xA600u,
            0xF589u, 0xF589u, 0xF053u, 0xF000u, 0xF000u,
            0x6000u, 0x6000u, 0x6000u, 0x6000u, 0x6000u,
            0xC000u, 0xC000u, 0xC000u, 0xC000u, 0xC000u,
            0x6600u, 0x6600u, 0x6600u, 0x6600u, 0x6600u,
            0xD800u, 0xD800u, 0xD800u, 0xD800u, 0xD800u,
        };
        for (uint8_t i = 0u; i < 45u; i++) {
            s_tx[i] = STOCK_CYC[i];
        }
    }
#else
    for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
        const uint8_t a = (uint8_t)(2u * c), b = (uint8_t)(a + 1u);
        const uint16_t spa = s_st.mode[a] == SOL_MODE_CURRENT ? sol_setpoint(s_st.current[a]) : 0u;
        const uint16_t spb = s_st.mode[b] == SOL_MODE_CURRENT ? sol_setpoint(s_st.current[b]) : 0u;

        s_tx[c] = (uint16_t)(0x8800u | mode_byte(c));
        s_tx[5u + c] = 0xA000u;                     /* ch 0 dither amplitude 0 */
        s_tx[10u + c] = (uint16_t)(0xE800u | spa);
        s_tx[15u + c] = 0xA600u;                    /* ch 1 dither amplitude 0 */
        s_tx[20u + c] = (uint16_t)(0xF000u | spb);
    }
    put_cmd(5u, 0x6000u);
    put_cmd(6u, 0xC000u);
    put_cmd(7u, 0x6600u);
    put_cmd(8u, 0xD800u);
#endif
    if (!xfer(45u)) {
        return;
    }
    /* stock 0x92530: every reply of a chip carries the cyclic header; the read frames return
     * the written dither (25+c vs 5+c, 35+c vs 15+c, low byte) and setpoints (30+c vs 10+c,
     * 40+c vs 20+c, 11 bits) */
    for (uint8_t c = 0u; c < SOL_CHIPS_REAL; c++) {
        for (uint8_t k = 0u; k < 9u; k++) {
            if ((s_rx[5u * k + c] & HDR_CYC_MASK) != HDR_CYC) {
                s_st.faults |= SOL_F_HEADER;
            }
        }
        if ((uint8_t)s_rx[25u + c] != (uint8_t)s_tx[5u + c] ||
            (s_rx[30u + c] & 0x7FFu) != (s_tx[10u + c] & 0x7FFu) ||
            (uint8_t)s_rx[35u + c] != (uint8_t)s_tx[15u + c] ||
            (s_rx[40u + c] & 0x7FFu) != (s_tx[20u + c] & 0x7FFu)) {
            s_st.cfg_mismatch++;       /* read-back reply format open (see job_config) */
        }
    }
}

bool sol_id_poll(void)
{
    if (!s_stream_on) {              /* E29: gated with the rest of bus-0 */
        return false;
    }
    /* E19 (spec 03 section 6.2o): the stock's 5 ms task queues the bus-0 ID job
     * EVERY pass (engine kick 0x921A0 -> descriptor 0x40A98) - we ran it once at
     * init. The RSPI-A lines run to the mechatronic connector: bus-0 activity is
     * electrically observable even though the supervisor never sees the frames.
     * No retries, no fault latch - a lost poll is just counted. */
    put_cmd(0u, 0x0600u);
    put_cmd(1u, 0x0000u);
    if (!xfer(10u)) {
        return false;
    }
    const bool ok = real_chip_headers(10u);
    if (ok) {
        s_st.id_poll_ok++;
    } else {
        s_st.id_poll_bad++;
    }
    return ok;
}

void sol_init(void)
{
    memset(&s_st, 0, sizeof s_st);
    for (uint8_t i = 0u; i < SOL_CHANNELS; i++) {
        s_st.mode[i] = SOL_MODE_OFF;
        s_req_mode[i] = SOL_MODE_OFF;
        s_req_cur[i] = 0u;
    }
    s_div = 0u;
    s_eng_level = 0u;
    s_cfg_pending = false;
    if (!s_stream_on) {              /* E29: pure-stream boot - commission the drivers
                                      * lazily when the gate opens (sol_stream_gate) */
        return;
    }
    job_id();
    if (s_st.faults == 0u) {
        job_config(CFG_8E);
    }
    s_st.ready = s_st.faults == 0u;
    if (!s_st.ready) {
        s_st.faults |= SOL_F_NOT_READY;
    }
}

void sol_stream_gate(bool on)
{
    if (on && !s_stream_on) {
        s_stream_on = true;
        job_id();                    /* first commissioning now that bus-0 is live */
        if (s_st.faults == 0u) {
            job_config(CFG_8E);
        }
        s_st.ready = s_st.faults == 0u;
        if (!s_st.ready) {
            s_st.faults |= SOL_F_NOT_READY;
        }
    } else {
        s_stream_on = on;
    }
}

void sol_engage_config(uint8_t level)
{
    s_eng_level = level;
    s_cfg_pending = true;
}

void sol_set(uint8_t ch, sol_mode_t mode, ma10_t current)
{
    if (ch < SOL_CHANNELS) {
        s_req_mode[ch] = (uint8_t)mode;     /* staged verbatim: OFF, CURRENT or FULL_ON */
        s_req_cur[ch] = current > SOL_MAX_MA10 ? SOL_MAX_MA10 : current;
    }
}

void sol_all_off(void)
{
    for (uint8_t i = 0u; i < SOL_CHANNELS; i++) {
        s_req_mode[i] = SOL_MODE_OFF;
        s_req_cur[i] = 0u;
    }
}

void sol_tick_10ms(void)
{
    /* any fault: everything off (the request is ignored until sol_init() clears the fault) */
    for (uint8_t i = 0u; i < SOL_CHANNELS; i++) {
        const bool allowed = s_st.faults == 0u && s_st.ready;
        s_st.mode[i] = allowed ? s_req_mode[i] : (uint8_t)SOL_MODE_OFF;
        s_st.current[i] = allowed && s_req_mode[i] == SOL_MODE_CURRENT ? s_req_cur[i] : 0u;
    }
    if (s_cfg_pending) {         /* engaged re-configuration (spec 03 section 6.2e): the
                                  * stock's write job carries 0x8E = 0x9F from the first
                                  * capture, NO level ramp (the 0x1F is the 1 kHz PWM
                                  * divider, spec 12 section 2b). E44 (spec 03 section
                                  * 6.2ae): the drivers' 0x8E register is their OWN
                                  * output-enable/PWM config — 0x86 (our old level law)
                                  * may leave the outputs un-enabled; the ~50 mA delivery
                                  * cap at any commanded load (E43) points here. 0x9F
                                  * constant, the stock's exact value, at the existing
                                  * per-level-change cadence (no new bursts — the E40b
                                  * lesson). */
        s_cfg_pending = false;
        job_config(0x9Fu);
    }
    if (!s_stream_on) {          /* E29: pure supervisor stream - no bus-0 traffic */
        return;
    }
    job_cyclic();
    if (++s_div >= 10u) {
        s_div = 0u;
        job_status();
    }
    s_st.cycles++;
}

/* The 40-word duty job (spec 03 section 6.2e): template flash 0x40ECC, driving dump
 * 2026-11-04. Eight five-select rotations; slot 2 (SSLA1) receives 0xD0 | duty[r] in
 * rotation r (the stock's live duties 90 10 20 40 40 60 00 00), the other selects carry
 * fixed maintenance words (SSLA2: 0x80 <- 0xB0 constant, SSLA0: 0x30/0xF0 alternating
 * reads, SSLA5: 0xE0 <- 0xD0 constant, SSLA3: the varying slot-0 row). */
static const uint16_t S_DUTY_TMPL[40] = {
    0xC070u, 0x80B0u, 0xD090u, 0x30A0u, 0xE0D0u,
    0x7050u, 0x80B0u, 0xD010u, 0xF0A0u, 0xE0D0u,
    0x2070u, 0x80B0u, 0xD020u, 0x30A0u, 0xE0D0u,
    0x2050u, 0x80B0u, 0xD040u, 0xF0A0u, 0xE0D0u,
    0x4070u, 0x80B0u, 0xD040u, 0x30A0u, 0xE0D0u,
    0x6050u, 0x80B0u, 0xD060u, 0xF0A0u, 0xE0D0u,
    0x6070u, 0x80B0u, 0xD000u, 0x30A0u, 0xE0D0u,
    0x0050u, 0x80B0u, 0xD000u, 0xF0A0u, 0xE0D0u,
};

bool sol_duty_job(const uint16_t duty[8])
{
    if (s_st.faults != 0u || !s_st.ready) {
        return false;
    }
    for (uint8_t i = 0u; i < 40u; i++) {
        s_tx[i] = S_DUTY_TMPL[i];
    }
    for (uint8_t r = 0u; r < 8u; r++) {
        s_tx[5u * r + 2u] = (uint16_t)(0xD000u | (duty[r] & 0xFFu));
    }
    if (!xfer(40u)) {
        return false;
    }
    for (uint8_t r = 0u; r < 8u; r++) {
        s_st.duty_rx[r] = s_rx[5u * r + 2u];
    }
    s_st.duty_runs++;
    return true;
}

const sol_status_t *sol_status(void)
{
    return &s_st;
}

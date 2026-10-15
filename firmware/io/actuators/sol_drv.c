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
#define CFG_8E          0x00u
#define CFG_90          0x21u
#define CFG_96          0x55u
#define MODE_FLAG_BIT4  0x10u       /* calibration 0x1918B9 */

static sol_status_t s_st;
static uint8_t s_req_mode[SOL_CHANNELS];
static ma10_t s_req_cur[SOL_CHANNELS];
static uint8_t s_div;

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
    if (!plat_drv_xfer(s_tx, s_rx, n)) {
        s_st.faults |= SOL_F_BUS;
        return false;
    }
    return true;
}

/* fill frames [5k .. 5k+4] with command cmd for every chip */
static void put_cmd(uint8_t k, uint16_t cmd)
{
    for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
        s_tx[5u * k + c] = cmd;
    }
}

static bool all_byte_headers(uint8_t frames)
{
    for (uint8_t i = 0u; i < frames; i++) {
        if ((s_rx[i] & HDR_BYTE_MASK) != HDR_BYTE) {
            return false;
        }
    }
    return true;
}

static void job_id(void)
{
    put_cmd(0u, 0x0600u);
    put_cmd(1u, 0x0000u);
    if (!xfer(10u)) {
        return;
    }
    if (!all_byte_headers(10u)) {
        s_st.faults |= SOL_F_ID;
        return;
    }
    for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
        s_st.chip[c].id[0] = (uint8_t)s_rx[c];
        s_st.chip[c].id[1] = (uint8_t)s_rx[5u + c];
    }
}

static void job_config(void)
{
    put_cmd(0u, 0x8E00u | CFG_8E);
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
    if (!all_byte_headers(40u)) {
        s_st.faults |= SOL_F_CONFIG;
        return;
    }
    /* read-backs as checked by stock 0x923BC: frame 20+c (low 6 bits) = mode byte of frame 15+c,
     * frame 25+c = 0x90 data, frame 30+c = 0x8E data, frame 35+c = 0x96 data */
    for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
        if ((s_rx[20u + c] & 0x3Fu) != (s_tx[15u + c] & 0x3Fu) ||
            (uint8_t)s_rx[25u + c] != (uint8_t)s_tx[5u + c] ||
            (uint8_t)s_rx[30u + c] != (uint8_t)s_tx[c] ||
            (uint8_t)s_rx[35u + c] != (uint8_t)s_tx[10u + c]) {
            s_st.faults |= SOL_F_CONFIG;
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
    if (!all_byte_headers(15u)) {
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
    if (!xfer(45u)) {
        return;
    }
    /* stock 0x92530: every reply of a chip carries the cyclic header; the read frames return
     * the written dither (25+c vs 5+c, 35+c vs 15+c, low byte) and setpoints (30+c vs 10+c,
     * 40+c vs 20+c, 11 bits) */
    for (uint8_t c = 0u; c < SOL_CHIPS; c++) {
        for (uint8_t k = 0u; k < 9u; k++) {
            if ((s_rx[5u * k + c] & HDR_CYC_MASK) != HDR_CYC) {
                s_st.faults |= SOL_F_HEADER;
            }
        }
        if ((uint8_t)s_rx[25u + c] != (uint8_t)s_tx[5u + c] ||
            (s_rx[30u + c] & 0x7FFu) != (s_tx[10u + c] & 0x7FFu) ||
            (uint8_t)s_rx[35u + c] != (uint8_t)s_tx[15u + c] ||
            (s_rx[40u + c] & 0x7FFu) != (s_tx[20u + c] & 0x7FFu)) {
            s_st.faults |= SOL_F_READBACK;
        }
    }
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
    job_id();
    if (s_st.faults == 0u) {
        job_config();
    }
    s_st.ready = s_st.faults == 0u;
    if (!s_st.ready) {
        s_st.faults |= SOL_F_NOT_READY;
    }
}

void sol_set(uint8_t ch, sol_mode_t mode, ma10_t current)
{
    if (ch < SOL_CHANNELS) {
        s_req_mode[ch] = (uint8_t)(mode == SOL_MODE_CURRENT ? SOL_MODE_CURRENT : SOL_MODE_OFF);
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
    job_cyclic();
    if (++s_div >= 10u) {
        s_div = 0u;
        job_status();
    }
    s_st.cycles++;
}

const sol_status_t *sol_status(void)
{
    return &s_st;
}

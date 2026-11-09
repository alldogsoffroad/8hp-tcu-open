/* sup_ctrl.c — the ctrl+C9 composers (design §7.6; stock 0x91C86/0x93126).
 *
 * The 0x8E data-byte law, bit by bit (artifact2 §3, live-stock verified):
 *   bit 0  enable request = 0xFFF90953, forced on every pass while
 *          0xFFF9094A != 1 (the builder law's tail)
 *   bit 1  the 0x90-composer's cell 0xFFF90964 & 2
 *   bit 2  normal dialect: complement of the echo's bit 2 (toggles every
 *          pass because the echo mirrors the word); engaged dialect: 0
 *   bit 3  always 0
 *   bit 4  engaged dialect life sign: toggles every pass when
 *          (mode & 0x11) == 0x11; cleared in the normal dialect
 *   bit 5  always 0 — the echo's bit 5 is the chip's OWN rail telemetry,
 *          never asserted by us (NR-8)
 *   bit 6  power-down: only when the mode byte == 6 (latches 0xFFF800EC)
 *   bit 7  engaged-dialect marker: (mode & 0x11) == 0x11 && !(mode & 0x20)
 *
 * Consequences law: the grant cell 0xFFF90954 = echo bit 0; the bit-0 edge
 * routes to the WDT (interval mode) ONLY when the echo confirms — no echo
 * bit 0 means no WDT switch (the A-law, design §5.6).
 */
#include "bsw/sup/sup_ctrl.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "platform.h"

/* engaged-dialect bit-4 toggle state (alternates every composed pass) */
static bool s_engaged_toggle;

/* WDT A-law: one-way switch to interval mode, echo-gated */
static bool s_wdt_interval;

/* 4-sample telemetry boxcar */
static uint16_t s_avg_win[4];
static uint32_t s_avg_sum;
static uint8_t  s_avg_i;

void sup_ctrl_compose(void)
{
    uint8_t mode = CELL_MODE_BYTE;                   /* 0xFFF90951 */
    bool engaged = ((mode & 0x11u) == 0x11u);
    uint8_t w;

    /* bit 0: the request cell, forced on every pass while outputs are not
     * disabled (stock 0x91C86 tail — NR-1: the ask rides the conversation) */
    if (CELL_OUTPUTS_DIS != 1u) {
        CELL_REQUEST = 1u;
    }
    w = (uint8_t)(CELL_REQUEST & 0x01u);

    /* bit 1: the 0x90-composer's cell */
    if ((CELL_CTRL_BIT1_SRC & 0x02u) != 0u) {
        w |= 0x02u;
    }

    if (engaged) {
        /* engaged dialect: bit 2 held 0, bit 4 toggles, bit 7 by mode */
        s_engaged_toggle = !s_engaged_toggle;
        if (s_engaged_toggle) {
            w |= 0x10u;
        }
        if ((mode & 0x20u) == 0u) {
            w |= 0x80u;
        }
    } else {
        /* normal dialect: life sign = complement of the echo's bit 2 */
        if ((CELL_ECHO_22 & 0x04u) == 0u) {
            w |= 0x04u;
        }
    }

    /* bits 3 and 5 are never set (composed paths mask them: 0x67/0x63/0x7B) */

    /* bit 6: power-down only in mode 6; latch the stock's cell */
    if (mode == 6u) {
        w |= 0x40u;
        CELL_PD_LATCH = 1u;
    }

    CELL_CTRL_STAGED = w;
    sup_job_table[JOB_CTRL].tx_stage[0] = (uint16_t)(0x8E00u | w);
    /* the J1 staging mirror the bench reads at 0xFFF88424 */
    CELL_J1_STAGED = (uint16_t)(0x8E00u | w);
}

void sup_c9_compose(void)
{
    uint8_t c9 = (uint8_t)(CELL_C9_BASE | (CELL_C9_HIGH & 0xF0u));

    /* bit 4 set while the shutdown-selftest flag is clear */
    if (CELL_SHUTDOWN_ST == 0u) {
        c9 |= 0x10u;
    }
    /* bit 7 from the variant cell */
    if ((CELL_C9_VARIANT & 0x80u) != 0u) {
        c9 |= 0x80u;
    }
    CELL_C9_STAGED = c9;
    sup_job_table[JOB_CTRL].tx_stage[1] = (uint16_t)(0xC900u | c9);
    /* the J1 staging mirror the bench reads at 0xFFF88426 */
    CELL_J1_C9_STAGED = (uint16_t)(0xC900u | c9);
}

void sup_enable_request(bool on)
{
    if (on) {
        CELL_REQUEST = 1u;
        CELL_OUTPUTS_DIS = 0u;
    } else {
        /* boot-handover disable shape (0x6C612 param 0) */
        CELL_OUTPUTS_DIS = 1u;
        CELL_REQUEST = 0u;
    }
}

void sup_reply_condense(void)
{
    /* grant: the 0x22 echo's bit 0 (stock 0x91ACA) */
    CELL_GRANT = ((CELL_ECHO_22 & 0x01u) != 0u) ? 1u : 0u;
    /* handover readiness: the 0x14 reply's bit 0 */
    CELL_HANDOVER_RDY = (uint8_t)(CELL_REG_14 & 0x01u);

    /* THE WDT A-LAW: switch to interval mode only when the sent word carried
     * bit 0 AND the echo confirms it. No echo bit 0 → no switch, ever. */
    if (!s_wdt_interval &&
        (CELL_CTRL_STAGED & 0x01u) != 0u &&
        (CELL_ECHO_22 & 0x01u) != 0u) {
        plat_wdt_mode(true);
        s_wdt_interval = true;
    }

    /* SSLB2 copy + sentinel re-arm (stock 0x91ACA law) */
    CELL_SSLB2_COPY = CELL_SSLB2_RAW;
    CELL_SSLB2_RAW = 0x8001u;

    /* 4-sample boxcar average of the 10-bit telemetry into 0xFFF908CA */
    s_avg_sum -= s_avg_win[s_avg_i];
    s_avg_win[s_avg_i] = CELL_TELEMETRY;
    s_avg_sum += s_avg_win[s_avg_i];
    s_avg_i = (uint8_t)((s_avg_i + 1u) & 3u);
    CELL_TELEM_AVG = (uint16_t)(s_avg_sum >> 2);
}

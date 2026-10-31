#include "bsw/sup/sup.h"

#include <string.h>

static sup_status_t s_st;
static bool s_toggle;
static uint8_t s_enable_req;             /* SUP_EN_* source bits currently requesting */
static bool s_flash_mode;
static bool s_pattern;
static bool s_pat_raw;                /* lab: raw pattern — bit 0 NOT cleared for 0x8E words */
static bool s_suspended;              /* lab: raw exchange owns the RSPI bus */
static uint16_t s_pat_word, s_pat_toggle;
static uint8_t s_app8e, s_app90;       /* stock-pattern data bytes (FE13) */
static bool s_app_sslb2;               /* pattern includes the SSLB2 exchange */

void sup_init(void)
{
    memset(&s_st, 0, sizeof s_st);
    s_toggle = false;
    s_enable_req = 0u;
    s_flash_mode = false;
    s_pattern = false;
    s_pat_raw = false;
    s_suspended = false;
    s_app8e = 0u;
    s_app90 = 0u;
    plat_sup_init();
}

void sup_suspend(bool on)
{
    s_suspended = on;
}

void sup_enable_set(uint8_t sources)
{
    s_enable_req |= (uint8_t)(sources & (SUP_EN_BENCH | SUP_EN_PARK));
}

void sup_enable_clear(uint8_t sources)
{
    s_enable_req = (uint8_t)(s_enable_req & ~sources);
}

void sup_set_flash_mode(bool on)
{
    s_flash_mode = on;
}

void sup_set_pattern(bool pattern_on, uint16_t word, uint16_t toggle)
{
    /* bit 0 is the output enable only in register 0x8E words: cleared there */
    const bool reg8e = (word & 0xFF00u) == SUP_CMD_CONTROL;
    s_pat_word = reg8e ? (uint16_t)(word & ~SUP_BIT_ENABLE) : word;
    s_pat_toggle = reg8e ? (uint16_t)(toggle & ~SUP_BIT_ENABLE) : toggle;
    s_pat_raw = false;
    s_pattern = pattern_on;
}

void sup_set_pattern_raw(bool pattern_on, uint16_t word, uint16_t toggle)
{
    /* lab: NO bit-0 clearing — lets [8E,01] through the tick path */
    s_pat_word = word;
    s_pat_toggle = toggle;
    s_pat_raw = true;
    s_pattern = pattern_on;
}

void sup_app_pattern(bool on, uint8_t bits8e, uint8_t bits90)
{
    sup_app_pattern2(on, bits8e, bits90, false);
}

void sup_app_pattern2(bool on, uint8_t bits8e, uint8_t bits90, bool with_sslb2)
{
    s_app8e = bits8e;
    s_app90 = bits90;
    s_app_sslb2 = with_sslb2;
    s_st.sslb2_on = with_sslb2;
    s_st.app_pattern = on;
    if (on) {
        s_pattern = false;             /* the stock pattern replaces the FE32 word pattern */
        s_flash_mode = false;
    }
}

/* One 5 ms pass of the stock application's supervisor exchange: the three cyclic jobs of
 * spec 03 section 4.4, frames alternating SSLB1 (even) / SSLB0 (odd) through the
 * two-command SPCMD sequence. */
static void app_pattern_tick(void)
{
    /* Bit 2 must ALTERNATE every frame: it is the supervisor watchdog's life sign. The
     * stock derives it as the complement of the 0x22 echo's bit 2 (0x91C86 with
     * 0xFFF907D5), which equals self-toggling while the echo works - but on the bench
     * the echo reads 0x00 and echo-following sent a CONSTANT word, which the supervisor
     * answered with a reset (2026-10-30: 0x8E44 held for ~600 frames killed the TCU).
     * The pattern therefore self-toggles unconditionally; the echo is only counted. */
    s_toggle = !s_toggle;
    const uint8_t bits = (uint8_t)((s_app8e & ~SUP_BIT_TOGGLE) |
                                   (s_toggle ? SUP_BIT_TOGGLE : 0u));
    const uint16_t job1[4] = {
        (uint16_t)(SUP_CMD_CONTROL | bits), 0xC900u, 0x2200u, 0x0900u,
    };
    const uint16_t job2[2] = { (uint16_t)(0x9000u | s_app90), 0x2400u };
    const uint16_t job3[2] = { 0x2800u, 0x2E00u };
    const uint16_t job4[1] = { 0x00FFu };           /* SSLB2, stock job 0x40C74 */
    const uint16_t job5[4] = { 0x0600u, 0x0300u, 0x0000u, 0x0000u };  /* stock 0x40B80 */
    const uint16_t job6[5] = { 0x1800u, 0x0C00u, 0x1E00u, 0x0600u, 0x1400u }; /* 0x40BA0 */
    uint16_t rx1[4], rx2[2], rx3[2], rx4[1], rx6[5];

    s_st.sent++;
    if (!plat_sup_xfer(job1, rx1, 4u) || !plat_sup_xfer(job2, rx2, 2u) ||
        !plat_sup_xfer(job3, rx3, 2u)) {
        s_st.busy++;
        return;
    }
    if (s_app_sslb2 && !plat_sup_burst2(job4, rx4, 1u)) {
        s_st.busy++;
        return;
    }
    s_st.sslb2_rx = rx4[0];
    if (s_app_sslb2) {                    /* complete cycle: the two mixed jobs too */
        if (!plat_sup_xfer(job5, s_st.rx_id, 4u) || !plat_sup_xfer(job6, rx6, 5u)) {
            s_st.busy++;
            return;
        }
    }
    s_st.last_tx = job1[0];
    s_st.last_rx = rx1[2];             /* the 0x22 control echo drives the next toggle */
    s_st.enable = (job1[0] & SUP_BIT_ENABLE) != 0u;
    for (uint8_t i = 0u; i < 4u; i++) {
        s_st.rx_words[i] = rx1[i];
    }
    s_st.rx_words[4] = rx2[0];
    s_st.rx_words[5] = rx2[1];
    s_st.rx_words[6] = rx3[0];
    s_st.rx_words[7] = rx3[1];
    /* Reply classes: SSLB1 (supervisor) frames answer 0xA8xx, SSLB0 frames 0xAAxx
     * (stock completion handler 0x92D94). SSLB1 frames are the even indices. */
    for (uint8_t i = 0u; i < 4u; i += 2u) {
        if ((rx1[i] & SUP_REPLY_MASK) != SUP_REPLY_OK) { s_st.a8_bad++; }
        if ((rx1[i + 1u] & 0xFE00u) != 0xAA00u)         { s_st.aa_bad++; }
    }
    if ((rx2[0] & SUP_REPLY_MASK) != SUP_REPLY_OK) { s_st.a8_bad++; }
    if ((rx2[1] & 0xFE00u) != 0xAA00u)             { s_st.aa_bad++; }
    if ((rx3[0] & SUP_REPLY_MASK) != SUP_REPLY_OK) { s_st.a8_bad++; }
    if ((rx3[1] & 0xFE00u) != 0xAA00u)             { s_st.aa_bad++; }
    /* The supervisor echoes the control word back through the 0x22 read; the stock checks
     * it against the sent word with the toggle bit (and bit 5) masked (0x92D94). */
    if (((rx1[2] ^ job1[0]) & 0xDFu) == 0u) {
        s_st.echo_ok++;
    } else {
        s_st.echo_bad++;
    }
}

void sup_tick_5ms(void)
{
    uint16_t rx = 0u;
    uint16_t tx;
    uint8_t r;

    if (s_suspended) {
        return;                        /* lab: the raw exchange owns the bus */
    }
    if (s_st.app_pattern) {
        app_pattern_tick();
        return;
    }
    s_toggle = !s_toggle;
    if (s_pattern) {
        tx = (uint16_t)(s_pat_word ^ (s_toggle ? s_pat_toggle : 0u));
        if (!s_pat_raw && (tx & 0xFF00u) == SUP_CMD_CONTROL) {
            tx = (uint16_t)(tx & ~SUP_BIT_ENABLE);
        }
    } else if (s_flash_mode) {
        tx = (uint16_t)(SUP_CMD_CONTROL | SUP_BIT_FL_MODE | (s_toggle ? SUP_BIT_FL_TOGGLE : 0u));
    } else {
        tx = (uint16_t)(SUP_CMD_CONTROL | (s_toggle ? SUP_BIT_TOGGLE : 0u) |
                        (s_enable_req != 0u ? SUP_BIT_ENABLE : 0u));
    }
    r = plat_sup_exchange(tx, &rx);
    if ((r & 2u) != 0u) {
        s_st.last_rx = rx;
        if ((rx & SUP_REPLY_MASK) == SUP_REPLY_OK) {
            s_st.replies_ok++;
        } else {
            s_st.replies_bad++;
        }
    }
    if ((r & 1u) != 0u) {
        s_st.last_tx = tx;
        s_st.sent++;
        s_st.enable = (tx & SUP_BIT_ENABLE) != 0u;
    } else {
        s_st.busy++;
        s_toggle = !s_toggle;           /* not sent: keep the toggle phase for the next try */
    }
}

const sup_status_t *sup_status(void)
{
    return &s_st;
}

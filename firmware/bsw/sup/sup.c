#include "bsw/sup/sup.h"
#include "bsw/sup/sup_engage.h"

#include <string.h>

static sup_status_t s_st;
static bool s_toggle;

/* E17 (spec 03 section 6.2o): the first-exchange evidence ring. The 0x22 echo of the
 * first 64 passes, recorded from pass one: bit 5 = the supervisor's rail state, bit 0
 * = the grant. One power cycle decides whether the rail is up when our stream starts
 * (and drops after our words) or already down at pass one (the drop is pre-app, in
 * the boot manager's reign). Read post-boot via the measurement RAM read. */
uint16_t g_sup_rx_hist[512];
uint32_t g_sup_rx_hist_n;
uint8_t  g_sup_state_hist[512];  /* E18: the ladder state per pass, beside the echo */
static uint8_t s_enable_req;             /* SUP_EN_* source bits currently requesting */
static bool s_flash_mode;
static bool s_pattern;
static bool s_pat_raw;                /* lab: raw pattern — bit 0 NOT cleared for 0x8E words */
static bool s_suspended;              /* lab: raw exchange owns the RSPI bus */
static uint16_t s_pat_word, s_pat_toggle;
static uint8_t s_app8e, s_app90;       /* stock-pattern data bytes (FE13) */
static uint8_t s_app_toggle = SUP_BIT_TOGGLE;   /* life-sign bit the pattern alternates */
static bool s_app_sslb2;               /* pattern includes the SSLB2 exchange */
static bool s_wdt_interval;            /* E21: the grant edge switched the MCU watchdog
                                        * (stock 0x920A4: enabled -> WTCR interval mode,
                                        * IPR05 low nibble 0xE — the WDTOVF pin change is
                                        * the observable; the supervisor watches it) */
static bool s_wdt_edge_early;          /* E22: the stock's first-pass enable edge is
                                        * COMPOSITE — 0x6C612(1) sets the shadow AND
                                        * fires 0x920A4 in the same first monitor pass,
                                        * INSIDE the ~320 ms rail window. E21 fired the
                                        * WDT switch at the grant (+640 ms, post-window)
                                        * — no rail. E22 fires it at the first 5 ms pass
                                        * (app start), mirroring the stock's edge timing;
                                        * the ask rides the host-armed window as before. */

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
    s_wdt_interval = false;
    s_wdt_edge_early = false;
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
    s_app_toggle = SUP_BIT_TOGGLE;
    s_app_sslb2 = with_sslb2;
    s_st.sslb2_on = with_sslb2;
    s_st.app_pattern = on;
    if (on) {
        s_pattern = false;             /* the stock pattern replaces the FE32 word pattern */
        s_flash_mode = false;
    }
}

void sup_app_pattern3(bool on, uint8_t bits8e, uint8_t toggle_bit, uint8_t bits90)
{
    /* Engaged-app shape (2026-10-31 stock RAM capture): the word is 0x8E91/0x8E81 —
     * bit 7 held, BIT 4 alternating (not the boot software's bit 2), enable on.
     * The toggle bit comes from the caller; bits outside the mask pass verbatim. */
    s_app_toggle = toggle_bit ? toggle_bit : SUP_BIT_TOGGLE;
    sup_app_pattern2(on, bits8e, bits90, true);
}

/* E11 (spec 03 section 6.2h): the BOOT bit-0 window. The stock's own enable fires within
 * its first monitor pass (monitor 0x873F0: state 0xFFF93BA1 <= 6, ~always true in normal
 * running) and the Oct-31 shadow-poll saw the echo's bit 0 latched at the first readable
 * instant after power-up — the stock sends bit 0 from its first cycles and it is ACCEPTED.
 * Every bit-0 word we ever forced was minutes late (all windows ran at ladder-complete).
 * This window sends bit 0 unconditionally for the first exchanges after boot; if the
 * echo mirrors it once, the A-law sustains it naturally. Bounded by the caller (the .noinit
 * trip counter in main caps the total attempts, so a rejecting supervisor produces a short
 * reset storm and then a normal A-law boot). */
static uint16_t s_boot_bit0_left;
static uint16_t s_boot_bit0_delay;

void sup_bit0_boot_arm(uint16_t cycles, uint16_t delay_5ms)
{
    s_boot_bit0_left = cycles;
    s_boot_bit0_delay = delay_5ms;
}

/* E23 (spec 03 section 6.2o): arm the boot-dialect opening phase - the first
 * `passes` 5 ms words are the state-6 constant 0x8E40 (the boot manager's own
 * dialect, matching the stock app's state-6 composer branch), before the
 * engaged pattern takes over. 0 = disabled. */
static uint16_t s_phase6_len;

void sup_phase6_arm(uint16_t passes)
{
    s_phase6_len = passes;
}

/* One 5 ms pass of the stock application's supervisor exchange: the three cyclic jobs of
 * spec 03 section 4.4, frames alternating SSLB1 (even) / SSLB0 (odd) through the
 * two-command SPCMD sequence. */
static void app_pattern_tick(void)
{
    static uint16_t s_phase6_n;        /* E23 (spec 03 section 6.2o): the stock app's
                                        * first phase is STATE 6 — the boot manager's
                                        * dialect: constant bit-6 word 0x8E40 (the
                                        * app composer's own state-6 branch = bit 6,
                                        * and the boot 0x30E40's state 6 = bits 0x40).
                                        * We entered straight into the ENGAGED dialect
                                        * (0x8E80/0x8E90) = a dialect break at the
                                        * handover; the rail window closes 320 ms later
                                        * in every build. Hold the boot dialect for the
                                        * first 100 passes, then go engaged. */
    if (!s_wdt_edge_early) {           /* E22: the stock's first-pass enable edge —
                                        * the WDT switch fires at app start, inside
                                        * the rail window (0x6C612+0x920A4 composite) */
        s_wdt_edge_early = true;
        s_wdt_interval = true;
        plat_wdt_mode(true);
    }
    /* Bit 2 must ALTERNATE every frame: it is the supervisor watchdog's life sign. The
     * stock derives it as the complement of the 0x22 echo's bit 2 (0x91C86 with
     * 0xFFF907D5), which equals self-toggling while the echo works - but on the bench
     * the echo reads 0x00 and echo-following sent a CONSTANT word, which the supervisor
     * answered with a reset (2026-10-30: 0x8E44 held for ~600 frames killed the TCU).
     * The pattern therefore self-toggles unconditionally; the echo is only counted. */
    s_toggle = !s_toggle;
    /* The previous pass's 0x22 echo decides this pass's enable bit (stock law:
     * the supervisor grants acceptance first; the app follows). */
    sup_engage_echo_bit0((s_st.rx_words[2] & SUP_BIT_ENABLE) != 0u);
    uint8_t bits = (uint8_t)((s_app8e & (uint8_t)~s_app_toggle) |
                             (s_toggle ? s_app_toggle : 0u));
    if (sup_engage_active() && sup_engage_enable_bit()) {
        bits |= SUP_BIT_ENABLE;
        if (!s_wdt_interval) {         /* E21 (spec 03 section 6.2o): the stock's
                                        * enable-edge switch (0x920A4) — the MCU's own
                                        * watchdog moves to interval mode exactly when
                                        * the enable goes out. The supervisor watches
                                        * the WDTOVF pin; the rail's next bit may hang
                                        * on it. */
            s_wdt_interval = true;
            plat_wdt_mode(true);
        }
    } else if (s_boot_bit0_delay != 0u) {
        s_boot_bit0_delay--;         /* E13: hold until the supervisor's own power-up is
                                      * done (stock engages at ~+15s, spec 03 6.2i) */
    } else if (s_boot_bit0_left != 0u) {
        s_boot_bit0_left--;
        bits |= SUP_BIT_ENABLE;      /* the window: bit 0 at the stock's engagement age */
    }
    uint16_t job1[4] = {
        (uint16_t)(SUP_CMD_CONTROL | bits), 0xC900u, 0x2200u, 0x0900u,
    };
    if (s_phase6_n < s_phase6_len) {   /* E23: boot-dialect phase — constant 0x8E40,
                                        * no toggle (state 6 = bit 6 only, both in the
                                        * boot's law and the app composer's). Length
                                        * set by the bench build via sup_phase6_arm();
                                        * 0 = disabled (library/tests). */
        s_phase6_n++;
        job1[0] = (uint16_t)(SUP_CMD_CONTROL | 0x40u);
    }
    const uint16_t job2[2] = { (uint16_t)(0x9000u | s_app90), 0x2400u };
    const uint16_t job3[2] = { 0x2800u, 0x2E00u };
    const uint16_t job4[1] = { 0x00FFu };           /* SSLB2, stock job 0x40C74 */
    const uint16_t job5[4] = { 0x0600u, 0x0300u, 0x0000u, 0x0000u };  /* stock 0x40B80 */
    const uint16_t job6[5] = { 0x1800u, 0x0C00u, 0x1E00u, 0x0600u, 0x1400u }; /* 0x40BA0 */
    uint16_t rx1[4], rx2[2], rx3[2], rx4[1], rx6[5];
    uint16_t dj[SUP_DRV_READ_WORDS], drx[SUP_DRV_READ_WORDS];

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
        /* E15 (spec 03 section 6.2m): the stock's seventh cyclic job 0x40BC0 - one
         * word 0x0000 to SSLB0 alone (select cfg 0x40B76 = SPCMD 0xAF01), the
         * status-nibble poll. Live staging 0xFFF885E0 = 0x0000; the completion
         * handler 0x92CC8 stores (rx & 0x0F) ^ 0x0F -> 0xFFF90958 and rx ->
         * 0xFFF90979. The stock's own live reply reads 0x0000 - not AA-class - and
         * it tolerates that (error bit only), so count classes, never fault. */
        {
            const uint16_t job7[1] = { 0x0000u };
            uint16_t rx7[1];

            if (!plat_sup_burst0(job7, rx7, 1u)) {
                s_st.busy++;
                return;
            }
            s_st.nib_rx = rx7[0];
            if ((rx7[0] & 0xFE00u) == 0xAA00u) {
                s_st.nib_aa++;
            } else {
                s_st.nib_other++;
            }
        }
    }
    if (sup_engage_active()) {            /* register job riding the cyclic stream:
                                           * single bus owner, the stream never stops */
        uint16_t ej[SUP_ENG_JOB_MAX], erx[SUP_ENG_JOB_MAX];
        uint8_t en = 0u;
        if (sup_engage_poll(ej, &en)) {
            if (plat_sup_burst(ej, erx, en)) {
                sup_engage_replies(erx, en);
            } else {
                s_st.busy++;
            }
        }
    }
    {   /* five-select driver read: the stock's commissioning traffic, witnessed
         * by the supervisor (its select is in the rotation) */
        uint8_t dn = 0u;
        if (sup_drv_poll(dj, &dn)) {
            const bool wr = dn == DRV_WRITE_WORDS || dn == DRV_SP_WORDS;
            if (!(wr ? plat_sup_xfer4(dj, drx, dn) : plat_sup_xfer5(dj, drx, dn))) {
                s_st.busy++;
            } else {
                sup_drv_replies(drx, dn);
            }
        }
    }
    s_st.last_tx = job1[0];
    s_st.last_rx = rx1[2];             /* the 0x22 control echo drives the next toggle */
    if (g_sup_rx_hist_n < 512u) {      /* E17/E18: the first-exchange evidence ring */
        g_sup_rx_hist[g_sup_rx_hist_n] = rx1[2];
        g_sup_state_hist[g_sup_rx_hist_n] = sup_engage_status()->state;
        g_sup_rx_hist_n++;
    }
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

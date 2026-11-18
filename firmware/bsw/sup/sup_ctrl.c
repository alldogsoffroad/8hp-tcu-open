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
/* the C9 word (bit 7 = the 0x9095A request) is composed here: an OUTPUT
 * path — it may not name the read-only PA7 shadow (app/pa7_shadow.h) */
#define TCU_OUTPUT_PATH 1
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
    /* S7: the TX words are written by the J1 arm (sup_batch.c, stock
     * 0x92D38: check-free first) — never here, where J1 may be in flight
     * (on the target 0xFFF88424 IS the live TX buffer) */
}

void sup_c9_compose(void)
{
    uint8_t c9 = (uint8_t)(CELL_C9_BASE | (CELL_C9_HIGH & 0xF0u));

    /* bit 4 set while the shutdown-selftest flag is clear */
    if (CELL_SHUTDOWN_ST == 0u) {
        c9 |= 0x10u;
    }
    /* bit 7 = (0x9095A != 0) — stock 0x91CA6..0x91CB6: `tst; nott;
     * bst.b #7` (r04g, ~/tcu_sweep/c9_bit7_decode.md). 0x8CE48 writes 0 or
     * 1 there (the PA7 Park-signal veto in R/D); the old `& 0x80` test could
     * never see a 1. */
    if (CELL_C9_VARIANT != 0u) {
        c9 |= 0x80u;
    }
    CELL_C9_STAGED = c9;
    /* the TX word is staged by the J1 arm (S7, see sup_ctrl_compose).
     * NO separate mirror store (2026-11-11 decode): on the target the J1
     * stage IS linker-pinned at 0xFFF88424, so frame 1 lives at 0xFFF88426
     * and CELL_J1_C9_STAGED (u8 at 0xFFF88426) is its big-endian HIGH byte
     * — the 0xC9 command byte. The old `CELL_J1_C9_STAGED = 0xC900|c9`
     * truncated to the data byte and overwrote the command byte: the wire
     * carried c9c9 (0x0000 since 8e9e431, 0x1010 before) where the stock
     * sends C900 (driving dump 0xFFF88426 = C9 00; our disassembly
     * 0x43ad6 mov.w then 0x43adc mov.b). Invisible on the host, where the
     * stage array is not linked at the pinned address. */
}

void sup_ctrl_bench_dialect(bool on)
{
    if (on) {
        CELL_MODE_BYTE |= 0x11u;   /* the engaged-dialect gate */
    } else {
        CELL_MODE_BYTE &= (uint8_t)~0x11u;
    }
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
    /* THE 0x9095B GATE (stock 0x91ACA, bytes 0x91ACE..0x91BD8) [O]: GBR =
     * 0xFFF9094B, r5 = @(16,gbr) = 0x9095B read once at entry; `tst; bt.s
     * 0x91B94` skips the whole condense block below while it is 0. The
     * batch's 0x91DAA sets it 0 → 1 at its tail (0x91DB8..0x91DC6, after
     * queueing J6/J1 — batch_8ee82 in sup_batch.c), so the stock's FIRST reply
     * proc (5 ms list entry 5, before the batch at 26) condenses nothing;
     * the second one sees r5 == 1 and RESEEDS the 4-slot average from the
     * sample of the J3 reply its own first pass queued (0x91B1A `cmp/eq
     * #1` → 0x91B5E), then sets 0x9095B = 0x0F (0x91BD4..D8). r02g..r04i
     * condensed from the first pass, before any J3 reply: the window was
     * seeded with the cleared cell's 0, so the second pass averaged (N + 0
     * + 0 + 0) / 4 ≈ 104 — the die at −24 °C, status 6, that r04h's
     * temperature shadow selected for ≈ 50 ms. */
    const uint8_t gate = CELL_PROC_GATE;                /* r5 */

    if (gate != 0u) {
        /* grant: the 0x22 echo's bit 0 (0x91AFC bld.b #0 0x907D5 → 0x91B06 0x90954) */
        CELL_GRANT = ((CELL_ECHO_22 & 0x01u) != 0u) ? 1u : 0u;
        /* handover readiness: the 0x14 reply's bit 0 (0x91AF4 bld.b #0 0x907D1 → 0x91B00 0x84A46) */
        CELL_HANDOVER_RDY = (uint8_t)(CELL_REG_14 & 0x01u);

        /* the telemetry average into 0xFFF908CA (0x91B04..0x91B70): an
         * invalid sample (0x907CC = 0xFFFF) propagates; gate == 1 or an
         * invalid average reseeds all 4 slots with the sample; otherwise
         * a 4-slot ring (0x90620, index 0x9095C), rounded (sum + 2) >> 2. */
        {
            /* ours: a witness that the reseed ran (kept as the old seeding
             * law's flag so no RAM symbol moves) */
            static volatile bool s_avg_seeded __attribute__((unused));
            const uint16_t smp = CELL_TELEMETRY;

            if (smp == 0xFFFFu) {
                CELL_TELEM_AVG = 0xFFFFu;
            }
            if (gate == 1u || CELL_TELEM_AVG == 0xFFFFu) {
                for (uint8_t i = 0u; i < 4u; i++) {
                    s_avg_win[i] = smp;
                }
                s_avg_sum = 4u * (uint32_t)smp;
                s_avg_seeded = true;
                CELL_TELEM_AVG = smp;
            } else {
                /* the ring index's start does not change the average (every
                 * slot holds the reseed value) */
                s_avg_sum -= s_avg_win[s_avg_i];
                s_avg_win[s_avg_i] = smp;
                s_avg_sum += smp;
                s_avg_i = (uint8_t)((s_avg_i + 1u) & 3u);
                CELL_TELEM_AVG = (uint16_t)((s_avg_sum + 2u) >> 2);
            }
        }

        /* the over-temperature comparator bits from the J6 0x1E harvest
         * (0x91B76..0x91B88: bld #5 / movt / shal, bld #4 / movt / add →
         * the WHOLE byte 0x9095E = (b5 << 1) | b4; 0x9095D = 0x907CE not
         * ported) */
        CELL_DIR_BITS = (uint8_t)((CELL_STAT_1E >> 4) & 3u);

        /* SSLB2 copy + sentinel re-arm (0x91B84..0x91B92) */
        CELL_SSLB2_COPY = CELL_SSLB2_RAW;
        CELL_SSLB2_RAW = 0x8001u;
    }
    if (gate == 1u) {
        /* 0x91B94..0x91BD8: the first condensing pass ends the gate's
         * reseed state (its 0x9095F/0x90960/0x90961/0x800ED copies are
         * not ported) */
        CELL_PROC_GATE = 0x0Fu;
    }

    /* THE WDT A-LAW (ours, not part of 0x91ACA; ungated as before): switch
     * to interval mode only when the sent word carried bit 0 AND the echo
     * confirms it. No echo bit 0 → no switch, ever. */
    if (!s_wdt_interval &&
        (CELL_CTRL_STAGED & 0x01u) != 0u &&
        (CELL_ECHO_22 & 0x01u) != 0u) {
        plat_wdt_mode(true);
        s_wdt_interval = true;
    }
}

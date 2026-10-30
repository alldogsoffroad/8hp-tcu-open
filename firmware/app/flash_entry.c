#include "app/flash_entry.h"
#include "platform/platform.h"
#include "platform/sh7254r/regs.h"     /* SPSRB/SPDRB, SPSR flags, WDT_WTCNT */
#include "bsw/sup/sup.h"

#define NRC_SFNS   0x12
#define NRC_IMLOIF 0x13
#define NRC_CNC    0x22
#define NRC_ROOR   0x31

/* WDT-servicing delay: safe for use in blocking routines */
static void lab_delay(uint32_t loops)
{
    while (loops-- > 0u) {
        if ((loops & 0x3FFu) == 0u) {
            WDT_WTCNT = 0x5ADBu;       /* ~30 ms to overflow */
        }
    }
}

/* T constants at flash 0x4526C (16 x uint32, from the static agent's RE) */
#define T_CONSTS ((const volatile uint32_t *)0x0004526Cu)
/* MISR registers */
#define MISRCR   (*(volatile uint8_t  *)0xFFFC1C00u)
#define MISR     (*(volatile uint32_t *)0xFFFC1C04u)
#define MISRCDR  (*(volatile uint32_t *)0xFFF7FFFCu)

/* stock fold primitive (misr_feeding_findings.md): save, seed, feed, read, restore.
 * Manual Sec.24 ordering: read MISR after seeding before MISRCDR writes; space the
 * post-MISRCDR read (stale hazard at Iclk x4). */
static uint32_t misr_fold(uint32_t seed, const uint32_t *words, uint32_t n)
{
    const uint32_t save = MISR;

    MISR = seed;
    (void)MISR;                         /* required read-back after the seed write */
    for (uint32_t i = 0u; i < n; i++) {
        MISRCDR = words[i];
        for (volatile uint32_t d = 0u; d < 8u; d++) { }
    }
    {
        uint32_t v = MISR;

        (void)v;                        /* first read may be stale */
        v = MISR;
        MISR = save;                    /* restore the persistent signature */
        return v;
    }
}

static uint8_t s_countdown;     /* ms until the flasher starts; 0 = not requested */

#if !defined(TCU_RAM_APP)
static void enable_lines(bool on)
{
    (void)plat_flash_enable(on);
    sup_set_flash_mode(on);
}
#endif

static uint8_t fwe_byte(void)
{
    return plat_flash_fwe() ? 0x80u : 0x00u;
}

static int enable_test(uint8_t type, uint16_t n, uint8_t *out, uint16_t max, bool allowed)
{
    if (n != 0u) {
        return -NRC_IMLOIF;
    }
    if (max < 3u) {
        return -NRC_IMLOIF;
    }
    if (type == 1u) {
        if (!allowed) {
            return -NRC_CNC;
        }
        out[0] = fwe_byte();
        out[2] = plat_flash_enable(true) ? 1u : 0u;
        sup_set_flash_mode(true);       /* takes effect with the next 5 ms supervisor frame:
                                           read FD03 afterwards */
        out[1] = fwe_byte();
        return 3;
    }
    if (type == 2u) {
        sup_set_flash_mode(false);
        out[1] = plat_flash_enable(false) ? 1u : 0u;
        out[0] = fwe_byte();
        return 2;
    }
    return -NRC_SFNS;
}

static int sup_pattern(uint8_t type, const uint8_t *in, uint16_t n, uint8_t *out, uint16_t max,
                       bool allowed)
{
    const sup_status_t *st = sup_status();

    if (type == 1u) {
        if (n != 3u) {
            return -NRC_IMLOIF;
        }
        if (!allowed) {
            return -NRC_CNC;
        }
        sup_set_pattern(true, (uint16_t)(in[0] << 8 | in[1]), in[2]);
        return 0;
    }
    if (type == 4u) {
        /* lab: raw pattern — NO bit-0 clearing (for [8E,01] init) */
        if (n != 3u) {
            return -NRC_IMLOIF;
        }
        if (!allowed) {
            return -NRC_CNC;
        }
        sup_set_pattern_raw(true, (uint16_t)(in[0] << 8 | in[1]), in[2]);
        return 0;
    }
    if (n != 0u) {
        return -NRC_IMLOIF;
    }
    if (type == 2u) {
        sup_set_pattern(false, 0u, 0u);
        return 0;
    }
    if (max < 5u) {
        return -NRC_IMLOIF;
    }
    out[0] = (uint8_t)(st->last_tx >> 8);
    out[1] = (uint8_t)st->last_tx;
    out[2] = (uint8_t)(st->last_rx >> 8);
    out[3] = (uint8_t)st->last_rx;
    out[4] = fwe_byte();
    if (type == 3u && max >= 21u) {
        /* lab counters: tx/rx/fwe + sent, replies_ok, replies_bad, busy (u32 BE) */
        out[5] = (uint8_t)(st->sent >> 24);
        out[6] = (uint8_t)(st->sent >> 16);
        out[7] = (uint8_t)(st->sent >> 8);
        out[8] = (uint8_t)st->sent;
        out[9] = (uint8_t)(st->replies_ok >> 24);
        out[10] = (uint8_t)(st->replies_ok >> 16);
        out[11] = (uint8_t)(st->replies_ok >> 8);
        out[12] = (uint8_t)st->replies_ok;
        out[13] = (uint8_t)(st->replies_bad >> 24);
        out[14] = (uint8_t)(st->replies_bad >> 16);
        out[15] = (uint8_t)(st->replies_bad >> 8);
        out[16] = (uint8_t)st->replies_bad;
        out[17] = (uint8_t)(st->busy >> 24);
        out[18] = (uint8_t)(st->busy >> 16);
        out[19] = (uint8_t)(st->busy >> 8);
        out[20] = (uint8_t)st->busy;
        return 21;
    }
    return 5;
}

/* Raw supervisor exchange: send one word, read the reply, report FPMON.
 * Suspends the 5ms tick so the RSPI bus is exclusively ours. */
static int sup_raw_exchange(uint8_t type, const uint8_t *in, uint16_t n,
                            uint8_t *out, uint16_t max, bool allowed)
{
    uint16_t rx = 0u;
    uint16_t tx;

    if (type != 1u) {
        return -NRC_SFNS;
    }
    if (n != 2u || max < 6u) {
        return -NRC_IMLOIF;
    }
    if (!allowed) {
        return -NRC_CNC;
    }
    tx = (uint16_t)(in[0] << 8 | in[1]);
    sup_suspend(true);                  /* stop the 5ms tick: we own the RSPI */
    for (volatile uint32_t d = 0u; d < 2000u; d++) { }  /* ~100us settle */
    /* clear any stale SPRF from the tick's last word (manual: SPRF=1 blocks
     * the receive buffer from being updated by new transfers) */
    if ((SPSRB & SPSR_SPRF) != 0u) {
        (void)SPDRB;                    /* read to acknowledge */
        SPSRB = 0u;                     /* clear SPRF */
    }
    /* wait TX-empty, send our word, poll for the NEW SPRF */
    uint32_t guard;
    uint8_t status = 0u;
    for (guard = 50000u; guard != 0u && (SPSRB & SPSR_SPTEF) == 0u; guard--) { }
    if ((SPSRB & SPSR_SPTEF) != 0u) {
        SPDRB = tx;                     /* start the transmission */
        status |= 1u;                   /* sent */
    }
    for (guard = 20000u; guard != 0u; guard--) {
        if ((SPSRB & SPSR_SPRF) != 0u) {
            rx = SPDRB;                 /* read the reply */
            status |= 2u;               /* got reply */
            SPSRB = 0u;                 /* clear SPRF */
            break;
        }
        if ((guard & 0xFFFu) == 0u) {
            WDT_WTCNT = 0x5ADBu;        /* WDT service */
        }
    }
    sup_suspend(false);                 /* resume the keep-alive */
    out[0] = (uint8_t)(tx >> 8);
    out[1] = (uint8_t)tx;
    out[2] = (uint8_t)(rx >> 8);
    out[3] = (uint8_t)rx;
    out[4] = fwe_byte();
    out[5] = status;
    return 6;
}

/* Back-to-back burst on the stateless vendor frame (per-frame full re-init — the only
 * CPU-mode discipline proven to deliver more than one frame; see plat_sh7254r.c). The
 * reply to each frame is read in its own slot; we keep the LAST one. Frame-to-frame gap
 * is one setup (11 register writes), effectively back-to-back at the supervisor's scale.
 * Reply: 71 01 FE 36 <last_rx_hi> <last_rx_lo> <fwe> */
static int burst_words(const uint16_t *words, uint16_t count, uint8_t *out, uint16_t max)
{
    uint16_t rx = 0u;
    uint8_t r = 0u;

    if (max < 3u) {
        return -NRC_IMLOIF;
    }
    sup_suspend(true);                  /* stop the 5ms tick: we own the RSPI */
    for (uint16_t i = 0u; i < count; i++) {
        r = plat_sup_exchange(words[i], &rx);
        if ((r & 1u) == 0u) {
            break;                      /* setup refused (cannot happen) */
        }
        WDT_WTCNT = 0x5ADBu;            /* WDT, as the vendor does */
    }
    sup_suspend(false);                 /* resume the keep-alive tick */
    out[0] = (uint8_t)(rx >> 8);
    out[1] = (uint8_t)rx;
    out[2] = fwe_byte();
    return 3;
}

/* Burst flavors (all single-frame UDS requests):
 *   type 1 (n=0): the boot-ROM INIT SEQ  [8E,01] [84,01]x11 [8E,04]
 *   type 2 (n=4): authenticator [88,b3][88,b2][88,b1][88,b0] (32-bit value BE)
 *   type 3 (n=1): demand [84,level]
 * Reply: 71 01 FE 36 <last_rx_hi> <last_rx_lo> <fwe> (as burst_words) */
static int sup_burst(uint8_t type, const uint8_t *in, uint16_t n, uint8_t *out, uint16_t max,
                     bool allowed)
{
    uint16_t words[13];
    uint16_t count = 0u;

    if (type < 1u || type > 4u) {
        return -NRC_SFNS;
    }
    if (!allowed) {
        return -NRC_CNC;
    }
    if (type == 1u) {
        if (n != 0u) {
            return -NRC_IMLOIF;
        }
        words[count++] = 0x8E01u;
        for (uint8_t i = 0u; i < 11u; i++) {
            words[count++] = 0x8401u;
        }
        words[count++] = 0x8E04u;
    } else if (type == 2u) {
        if (n != 4u) {
            return -NRC_IMLOIF;
        }
        for (uint8_t i = 0u; i < 4u; i++) {
            words[count++] = (uint16_t)(0x8800u | in[i]);
        }
    } else if (type == 3u) {
        if (n != 1u) {
            return -NRC_IMLOIF;
        }
        words[count++] = (uint16_t)(0x8400u | in[0]);
    } else {
        if (n != 2u) {
            return -NRC_IMLOIF;
        }
        words[count++] = (uint16_t)((uint16_t)in[0] << 8 | in[1]);
    }
    return burst_words(words, count, out, max);
}

/* Supervisor authenticator attempt (blocking):
 *   31 01 FE 34 -> reads profile + MISR, computes ~(T[profile] ^ MISR), sends as
 *   [88]x4 job, reads back the supervisor state. Returns 8 bytes: profile,
 *   misr[4], result_rx_hi, result_rx_lo, fwe. */
static int sup_authentic(uint8_t type, const uint8_t *in, uint16_t n,
                         uint8_t *out, uint16_t max, bool allowed)
{
    uint32_t misr;
    uint32_t auth;
    uint32_t t_val;
    uint8_t profile;
    uint16_t rx = 0u;

    (void)in;                           /* no request payload (type 1, n = 0) */
    if (type != 1u || n != 0u) {
        return -NRC_SFNS;
    }
    if (max < 8u) {
        return -NRC_IMLOIF;
    }
    if (!allowed) {
        return -NRC_CNC;
    }

    /* 1. INIT SEQUENCE via raw tick pattern (bit 0 NOT cleared for [8E,01]) */
    /* [8E,01] — the init frame that the vendor's boot sends at power-up */
    sup_set_pattern_raw(true, 0x8E01u, 0u);
    lab_delay(20000u);
    /* [84,01] x 11 */
    for (uint8_t i = 0u; i < 11u; i++) {
        sup_set_pattern_raw(true, 0x8401u, 0u);
        for (volatile uint32_t d = 0u; d < 20000u; d++) { }
    }
    /* [8E,04] — the final keep-alive */
    sup_set_pattern_raw(true, 0x8E04u, 0u);
    for (volatile uint32_t d = 0u; d < 20000u; d++) { }
    sup_set_pattern_raw(false, 0u, 0u);

    /* 2. read the supervisor profile via the tick pattern */
    sup_set_pattern(true, 0x1200u, 0u);          /* [12,00] */
    lab_delay(60000u);
    {
        const sup_status_t *st = sup_status();
        rx = st->last_rx;
    }
    profile = (uint8_t)(rx & 0x0Fu);              /* bits 3:0 */
    sup_set_pattern(false, 0u, 0u);

    /* 3. read the MISR (enable first) */
    MISRCR = 1u;                                  /* MISREN */
    lab_delay(100u);
    misr = MISR;                                  /* 32-bit signature */
    MISRCR = 0u;

    /* 4. compute the authenticator */
    t_val = T_CONSTS[profile & 0x0Fu];
    auth = ~(t_val ^ misr);                       /* complement of XOR */

    /* 5. send as the [88]x4 job: MSB first */
    sup_set_pattern(true, (uint16_t)(0x8800u | ((auth >> 24) & 0xFFu)), 0u);
    lab_delay(20000u);
    sup_set_pattern(true, (uint16_t)(0x8800u | ((auth >> 16) & 0xFFu)), 0u);
    for (volatile uint32_t d = 0u; d < 20000u; d++) { }
    sup_set_pattern(true, (uint16_t)(0x8800u | ((auth >> 8) & 0xFFu)), 0u);
    for (volatile uint32_t d = 0u; d < 20000u; d++) { }
    sup_set_pattern(true, (uint16_t)(0x8800u | (auth & 0xFFu)), 0u);
    for (volatile uint32_t d = 0u; d < 20000u; d++) { }

    /* 6. read the demand state [0A,00] to check acceptance */
    sup_set_pattern(true, 0x0A00u, 0u);
    lab_delay(60000u);
    {
        const sup_status_t *st = sup_status();
        rx = st->last_rx;
    }
    sup_set_pattern(false, 0u, 0u);

    /* 7. report */
    out[0] = profile;
    out[1] = (uint8_t)(misr >> 24);
    out[2] = (uint8_t)(misr >> 16);
    out[3] = (uint8_t)(misr >> 8);
    out[4] = (uint8_t)misr;
    out[5] = (uint8_t)(rx >> 8);
    out[6] = (uint8_t)rx;
    out[7] = fwe_byte();
    return 8;
}

int flash_entry_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                        uint16_t max, bool allowed)
{
    if (rid == SUP_PAT_RID) {
        return sup_pattern(type, in, n, out, max, allowed);
    }
    if (rid == FLASH_EN_RID) {
        return enable_test(type, n, out, max, allowed);
    }
    if (rid == SUP_RAW_RID) {
        return sup_raw_exchange(type, in, n, out, max, allowed);
    }
    if (rid == SUP_BURST_RID) {
        return sup_burst(type, in, n, out, max, allowed);
    }
    if (rid == SUP_AUTH_RID) {
        return sup_authentic(type, in, n, out, max, allowed);
    }
    if (rid == SUP_MISR_RID) {
        if (type != 1u || n != 0u || max < 4u) {
            return -NRC_IMLOIF;
        }
        MISRCR = 1u;
        lab_delay(100u);
        {
            const uint32_t misr = MISR;
            MISRCR = 0u;
            out[0] = (uint8_t)(misr >> 24);
            out[1] = (uint8_t)(misr >> 16);
            out[2] = (uint8_t)(misr >> 8);
            out[3] = (uint8_t)misr;
        }
        return 4;
    }
    if (rid == SUP_FOLD_RID) {
        uint32_t got;

        if (type == 2u) {
            /* arbitrary fold: seed BE32 + k words BE32 (n = 4 + 4k, k = 0..8).
             * The GF(2) identification probe: fold(s,[w]) = F(s,w). */
            uint32_t ws[8];
            uint32_t seed;
            uint16_t k;

            if (n < 4u || n > 36u || ((n - 4u) & 3u) != 0u || max < 4u) {
                return -NRC_IMLOIF;
            }
            k = (uint16_t)((n - 4u) >> 2);
            seed = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | in[3];
            for (uint16_t i = 0u; i < k; i++) {
                ws[i] = ((uint32_t)in[4u + 4u * i] << 24) | ((uint32_t)in[5u + 4u * i] << 16) |
                        ((uint32_t)in[6u + 4u * i] << 8) | in[7u + 4u * i];
            }
            MISRCR = 1u;
            (void)MISRCR;
            (void)MISRCR;
            got = misr_fold(seed, ws, k);
            out[0] = (uint8_t)(got >> 24);
            out[1] = (uint8_t)(got >> 16);
            out[2] = (uint8_t)(got >> 8);
            out[3] = (uint8_t)got;
            return 4;
        }
        if (type == 3u) {
            /* the 8 words the app actually reads at 0x40F1C (mapping check) */
            const volatile uint32_t *p = (const volatile uint32_t *)0x00040F1Cu;

            if (n != 0u || max < 32u) {
                return -NRC_IMLOIF;
            }
            for (uint8_t i = 0u; i < 8u; i++) {
                const uint32_t w = p[i];

                out[4u * i] = (uint8_t)(w >> 24);
                out[4u * i + 1u] = (uint8_t)(w >> 16);
                out[4u * i + 2u] = (uint8_t)(w >> 8);
                out[4u * i + 3u] = (uint8_t)w;
            }
            return 32;
        }
        /* stock init self-test (FUN_000969bc): fold(0x7650E321, 8 words @0x40F1C)
         * must equal 0xEDF9FBBE. Validates our MISR driving discipline bit-exactly. */
        if (type != 1u || n != 0u || max < 5u) {
            return -NRC_IMLOIF;
        }
        MISRCR = 1u;                    /* MISREN; stock leaves it on forever */
        (void)MISRCR;
        (void)MISRCR;                   /* manual: read back twice after the enable */
        got = misr_fold(0x7650E321u, (const uint32_t *)(uintptr_t)0x00040F1Cu, 8u);
        out[0] = (uint8_t)(got >> 24);
        out[1] = (uint8_t)(got >> 16);
        out[2] = (uint8_t)(got >> 8);
        out[3] = (uint8_t)got;
        out[4] = (uint8_t)(got == 0xEDF9FBBEu);
        return 5;
    }
    if (rid != FLASH_RID) {
        return -NRC_ROOR;
    }
    if (type != 1u) {
        return -NRC_SFNS;
    }
    if (n != 0u) {
        return -NRC_IMLOIF;
    }
    if (!allowed) {
        return -NRC_CNC;
    }
#if !defined(TCU_RAM_APP)
    enable_lines(true);                     /* as the user boot MAT does; FWE checked at start */
#endif
    s_countdown = FLASH_START_DELAY_MS;     /* time for the positive response to go out */
    return 0;
}

int flash_entry_did(uint8_t *out, uint16_t max)
{
    if (max < 1u) {
        return -1;
    }
    out[0] = plat_flash_fwe() ? 1u : 0u;
    return 1;
}

void flash_entry_tick_1ms(void)
{
    if (s_countdown != 0u && --s_countdown == 0u) {
#if defined(TCU_RAM_APP)
        plat_flasher_start();               /* back to the data-flash loader (spec 18) */
#else
        if (plat_flash_fwe()) {
            plat_flasher_start();
        } else {
            enable_lines(false);            /* FWE did not follow: stay in the application */
        }
#endif
    }
}

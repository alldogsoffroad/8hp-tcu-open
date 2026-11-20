/* test_fr_shadow.c — THE FAULT-REACTION / GEAR-CODE SHADOW (app/fr_shadow.c,
 * app/fr_fm.c; F1 2026-11-19, ~/tcu_sweep/f1_fr_shadow_port.md; decode
 * ~/tcu_sweep/f0_fault_chain_decode.md).
 *
 * A 10 ms timeline of our boot (OS mode 1 from app +650 ms — the r03r bench:
 * the first mode-1 pass at +641 ms), rng_chain's group-109 report (0xA6AB0:
 * every 20 ms from the first mode-1 pass, tested once its counter passes 50,
 * failed without a valid lever) and park_pair's group-98 report (every 100 ms,
 * failed + tested: both contacts inactive on this bench) emulated as the
 * bench measured them (r03s U1267 +1650 ms, r03v P0850 +3150 ms), raw frames
 * as the bench's sims send them (0x2EA / 0x210 / 0x140 / 0x108, CRC + a
 * counter stepping 1).
 *
 *   (no argument)
 *     (1) the RX status 0x6EC80: timeout, the 4th consecutive bad frame,
 *         counter steps 1..3 / 4 / a repeat, the first frame, DLC
 *     (2) the plain boot (no frames): U11E4 pending 400 ms before its confirm
 *         (1600 ms), U1267 1650, P0850 3150, U11E3 2140, P1DCA (group 28,
 *         the TOE32 test) 710 = 'L' (610) + 100; nothing else; the masks =
 *         the dump's 0x00813240 / 0xE8260114 (D-FR-9 retired 2026-11-20);
 *         code 0x20 → 0x89C43 3, AE2D 1, 0x89CF5 1; the fault-fed target 3
 *     (3) the sims from power-on: U11E4 never fails, U0121 1450, U0100 /
 *         U0140 / U0212 / U0102 1790, U0155 6240 (reaction 16), pending 22
 *         from 1300 → code 3; then 0x20 → 3 in P; D: 4 (200 ms) → 0x1C
 *         (50 ms) → 0x20; R: −1
 *     (4) heal: frames from +3 s after a plain boot → U11E4 heals 450 ms
 *         after its first pass and leaves the masks (rule 0x100), U1267 too;
 *         P0850 stays (rule 0x011)
 *     (5) key-off / key-on (E9FE 1 → 0 → 1): the terminal re-cycle 0xAACE4
 *         (the mode-1 rule drops 98 / 109 / 136 / 139 …, the pending masks
 *         zeroed)
 *     (6) the dump: the OR of 28 | 98 | 109 | 136's configs = the dump's
 *         masks 0x00813240 / 0xE8260114 (and the dump file's when present)
 *     (8) groups 27 / 28, the TOE32 test (~/tcu_sweep/group28_port.md): the
 *         0x93658 conversion (the dump's ring 9008 → 3436 mV), the dump's
 *         state (en 18 fl 10 code 84), <= 2400 mV → group 27, >= 4000 mV →
 *         both pass, the heal (100 ms, out of the masks), the gate shut (no
 *         'L', supply 6400 / 6600 / 23600), the 20 ms duty-category window
 *     (7) the cell window 0xFFF80000..0xFFF96000 is untouched by every run
 *         (filled with a pattern first: the shadow writes no live cell,
 *         the DSM image / NVM / record windows included)
 *   --cal  every calibration copy against the stock image (exit 77 absent)
 *   --time host ns per tick (the CPU estimate)
 * test_fr_shadow_neg_CFG (FR_NEG_CFG: group 136's fail time 450 → 452 in
 * app/fr_cal.c) must FAIL (the U11E4 confirm moves one pass);
 * test_fr_shadow_neg_G28 (FR_NEG_G28: the open band's upper threshold 4000 →
 * 3400 mV) must FAIL (group 28 never confirms, the masks lose id 40). */
#include "app/fr_shadow.h"
#include "app/fr_cal.h"
#include "bsw/com/crc8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

static int g_fail;
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); g_fail++; } } while (0)
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define FR (fr_shadow_state())
#define FM (fr_fm_state())

/* ── the window (the shadow must not touch it) ─────────────────────────── */
#define WIN_BASE 0xFFF80000u
#define WIN_LEN  0x16000u
static uint8_t *g_win;
static void win_map(void)
{
    void *p = mmap((void *)(uintptr_t)WIN_BASE, WIN_LEN, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    g_win = (uint8_t *)p;
}
static void win_fill(void)
{
    for (uint32_t i = 0u; i < WIN_LEN; i++) {
        g_win[i] = (uint8_t)(0x5Au ^ (i * 7u));
    }
}
static uint32_t win_changed(void)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < WIN_LEN; i++) {
        if (g_win[i] != (uint8_t)(0x5Au ^ (i * 7u))) {
            n++;
        }
    }
    return n;
}

/* ── the timeline ──────────────────────────────────────────────────────── */
#define M1_MS 650u
typedef struct {
    int      sims;          /* lever / brake / engine 700 frames from power-on */
    unsigned frames_from;   /* ms: frames start (0 = with sims from power-on) */
    unsigned keyoff_at, keyon_at;   /* ms (0 = none) */
    uint8_t  d6d6;          /* the range (8 P, 5 D, 7 R) */
    unsigned lever_at;      /* ms the range changes to `d6d6_late` (0 = none) */
    uint8_t  d6d6_late;
    /* groups 27 / 28 (the TOE32 test) */
    uint16_t pl7_raw;       /* the ring slots 35 / 40 raw (0 = 9008: 3436 mV, the bench / dump) */
    unsigned pl7_at;        /* ms the raw changes to `pl7_raw2` (0 = none) */
    uint16_t pl7_raw2;
    unsigned l_at;          /* ms of 'L' (0x95005 1 → 0; 0 = T3_L_MS) */
    int      no_l;          /* never 'L' */
    uint16_t sup_mv;        /* the supply 0x94F86 (0 = 12950) */
    unsigned duty_at;       /* ms TOE32's applied duty becomes `duty` (0 = off throughout) */
    uint16_t duty;
} scen_t;
#define T3_L_MS 610u

static uint8_t g_ctr[0x800];
static void send(uint8_t bus, uint16_t id, uint8_t dlc, uint8_t step)
{
    uint8_t d[8] = { 0 };
    if (id == 0x108u) {
        d[0] = 0x0Au; d[1] = 0xF0u;             /* 700 rpm (content irrelevant here) */
    }
    g_ctr[id] = (uint8_t)((g_ctr[id] + step) & 0x0Fu);
    d[dlc - 2u] = (uint8_t)((d[dlc - 2u] & 0x0Fu) | (uint8_t)(g_ctr[id] << 4));
    d[dlc - 1u] = crc8_j1850(d, (uint8_t)(dlc - 1u));
    fr_hook_rx(bus, id, dlc, d);
}

static unsigned g_ms;
static uint32_t g_rep109, g_rep98;
static uint8_t g_c20;           /* rng_chain's U1267 counter (D5E4.w16[0x14]) */
static unsigned g_mpass;

static void tick(const scen_t *s)
{
    fr_in_t in;
    memset(&in, 0, sizeof in);
    g_ms += 10u;
    const unsigned ms = g_ms;
    const int key = !(s->keyoff_at != 0u && ms >= s->keyoff_at && (s->keyon_at == 0u || ms < s->keyon_at));
    const int fr_on = (s->sims && ms >= s->frames_from) && key;
    if (fr_on) {
        send(0u, 0x2EAu, 5u, 1u);
        send(1u, 0x210u, 5u, 1u);
        send(0u, 0x140u, 8u, 1u);
        send(0u, 0x108u, 8u, 1u);
    }
    in.os_mode = (ms >= M1_MS) ? (s->sims ? 2u : 1u) : 0u;
    in.e9fe = (uint8_t)((ms >= 20u) && key);
    in.e9ff = (uint8_t)((ms == 20u || (s->keyon_at != 0u && ms == s->keyon_at)) ? 1u : (key ? 2u : 4u));
    uint8_t rng = s->d6d6;
    if (s->lever_at != 0u && ms >= s->lever_at) {
        rng = s->d6d6_late;
    }
    in.d6d6 = rng;
    in.e69b = (rng == 5u) ? 1u : (rng == 7u) ? 2u : 0u;
    in.e6ee = in.e69b;
    in.e6ef = in.e69b;
    in.c3d = (uint8_t)(in.os_mode == 1u);
    /* rng_chain's 0xA6AB0 (20 ms, the first chain pass, modes 1..3) */
    if (in.os_mode >= 1u) {
        if (g_c20 < 255u) {
            g_c20++;                                   /* one per chain pass */
        }
        if (g_mpass % 2u == 0u) {
            in.u_failed = (uint8_t)!fr_on;
            in.u_tested = (uint8_t)(g_c20 > 50u);
            g_rep109++;
        }
        if (g_mpass % 10u == 0u) {
            g_rep98++;                                 /* park_pair 0x141E06 (100 ms) */
        }
        g_mpass++;
    }
    in.g98_failed = 1u;
    in.g98_tested = 1u;
    in.rep109_n = g_rep109;
    in.rep98_n = g_rep98;
    in.eng_st = fr_on ? 0u : 2u;
    in.eng = fr_on ? 700u : 0u;
    in.out_st = 0u;
    in.gf_e69f = (rng == 7u) ? -1 : 1;
    in.gf_c25 = in.gf_e69f;
    in.gf_c28 = in.gf_e69f;
    in.gf_c2b = in.gf_e69f;
    in.gf_cce4 = in.gf_e69f;
    in.gf_e343 = in.gf_e69f;
    in.gf_e6a1 = (rng == 7u) ? -1 : 1;
    in.ce8 = 1u;
    in.cd2 = 0x4Fu;
    in.sup_mv = (s->sup_mv != 0u) ? s->sup_mv : 12950u;
    in.cd0 = in.eng;
    {
        /* groups 27 / 28 (the TOE32 test): PL7 on the AN0 ring slots 35 / 40
         * (the bench's / the dump's 9008 raw = 3436 mV, TOE32 off), 0x94110
         * from the second pass, the sequencer's 'L' (0x95005 1 → 0) at
         * +610 ms (our boot: 'E' ≈ +598, 'U' ≈ +0.62 s) */
        fr_t3_in_t t3;
        const uint16_t raw = (s->pl7_at != 0u && ms >= s->pl7_at) ? s->pl7_raw2
                             : (s->pl7_raw != 0u ? s->pl7_raw : 9008u);
        const unsigned l_at = (s->l_at != 0u) ? s->l_at : T3_L_MS;
        t3.r35[0] = raw;
        t3.r35[1] = raw;
        t3.r40[0] = raw;
        t3.r40[1] = raw;
        t3.duty = (s->duty_at != 0u && ms >= s->duty_at) ? s->duty : 0u;
        t3.x95005 = (uint8_t)(s->no_l != 0 || ms < l_at);
        t3.x94110 = (uint8_t)(ms >= 20u);
        fr_hook_feed_t3(&t3);
    }
    fr_hook_feed(&in);
}

static void run(const scen_t *s, unsigned until_ms)
{
    while (g_ms < until_ms) {
        tick(s);
    }
}

static void reset(void)
{
    fr_hook_init();
    memset(g_ctr, 0, sizeof g_ctr);
    g_ms = 0u;
    g_rep109 = g_rep98 = 0u;
    g_c20 = 0u;
    g_mpass = 0u;
}

/* the first recorded event of a kind for a group (0 = none) */
static uint32_t ev_ms(uint8_t kind, uint8_t g)
{
    for (unsigned i = 0u; i < FM->rec_n; i++) {
        if (FM->rec[i].kind == kind && FM->rec[i].g == g) {
            return FM->rec[i].ms;
        }
    }
    return 0u;
}

static void dump_events(const char *tag)
{
    printf("-- %s: %u events (%u seen)\n", tag, (unsigned)FM->rec_n, (unsigned)FM->rec_all);
    for (unsigned i = 0u; i < FM->rec_n; i++) {
        printf("   %6u ms kind %02X g %3u v %u\n", (unsigned)FM->rec[i].ms, FM->rec[i].kind,
               FM->rec[i].g, FM->rec[i].v);
    }
}

static unsigned confirmed_set(uint8_t *out, unsigned max)
{
    unsigned n = 0u;
    for (uint8_t g = 1u; g <= FR_NGRP; g++) {
        if (fr_fm_confirmed(g) && n < max) {
            out[n++] = g;
        }
    }
    return n;
}

/* ── (1) the RX status ─────────────────────────────────────────────────── */
static void rx_one(uint16_t id, uint8_t dlc, uint8_t nib, int good_crc)
{
    uint8_t d[8] = { 0 };
    d[dlc - 2u] = (uint8_t)(nib << 4);
    d[dlc - 1u] = crc8_j1850(d, (uint8_t)(dlc - 1u));
    if (!good_crc) {
        d[dlc - 1u] ^= 0x01u;
    }
    fr_hook_rx(0u, id, dlc, d);
}

static void test_rx(void)
{
    const scen_t s0 = { 0 };
    reset();
    /* 0x108 = entry 1 (timeout 5 x 10 ms, 0x3B) */
    for (unsigned i = 0u; i < 6u; i++) {
        tick(&s0);
    }
    CHECKV(FR->rx[1].st & 1u, 0u);              /* timer 6 = to + 1: not yet */
    tick(&s0);
    CHECKV(FR->rx[1].st, 1u);                   /* the timeout */
    /* the first frame: any counter accepted */
    rx_one(0x108u, 8u, 9u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0u);
    rx_one(0x108u, 8u, 10u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0u);   /* step 1 */
    rx_one(0x108u, 8u, 13u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0u);   /* step 3 */
    /* step 4: an error, flagged on the 4th consecutive one */
    rx_one(0x108u, 8u, 1u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0u);
    rx_one(0x108u, 8u, 5u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0u);
    rx_one(0x108u, 8u, 9u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0u);
    rx_one(0x108u, 8u, 13u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0x10u);
    rx_one(0x108u, 8u, 14u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0u);   /* a good step clears */
    /* a repeat is an error */
    for (unsigned i = 0u; i < 3u; i++) {
        rx_one(0x108u, 8u, 14u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0u);
    }
    rx_one(0x108u, 8u, 14u, 1); tick(&s0); CHECKV(FR->rx[1].st, 0x10u);
    /* CRC errors: the 4th */
    for (unsigned i = 0u; i < 3u; i++) {
        rx_one(0x108u, 8u, (uint8_t)(15u + i), 0); tick(&s0); CHECKV(FR->rx[1].st & 0x08u, 0u);
    }
    rx_one(0x108u, 8u, 2u, 0); tick(&s0); CHECKV(FR->rx[1].st & 0x08u, 0x08u);
    /* DLC short: 0x108's object DLC 8 */
    for (unsigned i = 0u; i < 4u; i++) {
        rx_one(0x108u, 7u, (uint8_t)(3u + i), 1); tick(&s0);
    }
    CHECKV(FR->rx[1].st & 0x20u, 0x20u);
    /* a coded-out message (entry 20, 0x1D0, en 0) never shows a status */
    CHECKV(FR->rx[20].st, 0u);
    /* 0x112 (entry 15, 0x23): timeout only, no CRC / counter check */
    rx_one(0x112u, 2u, 0u, 0); tick(&s0);
    CHECKV(FR->rx[15].st, 0u);
}

/* ── (2) the plain boot ───────────────────────────────────────────────── */
static void test_plain(void)
{
    const scen_t s = { .sims = 0, .d6d6 = 8u };
    reset();
    run(&s, 3300u);
    dump_events("plain boot");
    CHECKV(ev_ms(FR_EV_CONFIRM, 136u), 1600u);           /* U11E4 */
    CHECKV(ev_ms(FR_EV_PEND_ON, 136u), 1200u);           /* 400 ms before */
    CHECKV(ev_ms(FR_EV_START, 136u), 1150u);
    CHECKV(ev_ms(FR_EV_CONFIRM, 109u), 1650u);           /* U1267 (r03s +1650) */
    CHECKV(ev_ms(FR_EV_CONFIRM, 98u), 3150u);            /* P0850 (r03v +3150) */
    CHECKV(ev_ms(FR_EV_PEND_ON, 98u), 1050u);            /* id 50 pending from 400 ms */
    CHECKV(ev_ms(FR_EV_DIRECT, 143u), 2140u);            /* U11E3 (bus 2, 0x210 lost) */
    CHECKV(ev_ms(FR_EV_START, 28u), T3_L_MS);             /* P1DCA: the TOE32 gate opens at 'L' */
    CHECKV(ev_ms(FR_EV_CONFIRM, 28u), T3_L_MS + 100u);    /* its 100 ms fail time */
    CHECKV(ev_ms(FR_EV_CONFIRM, 27u), 0u);
    uint8_t c[16];
    const unsigned n = confirmed_set(c, 16u);
    CHECKV(n, 5u);
    CHECK(n == 5u && c[0] == 28u && c[1] == 98u && c[2] == 109u && c[3] == 136u && c[4] == 143u);
    CHECKV(FM->m0, 0x00813240u);
    CHECKV(FM->m1, 0xE8260114u);                         /* THE DUMP'S (group 28 adds id 40) */
    CHECKV(FR->cd.c41, 0x20u);
    CHECKV(FR->cd.c42, 0x20u);
    CHECKV(FR->cd.c43, 3);
    CHECKV(FR->cd.ae2d, 1u);
    CHECKV(FR->cd.ced, 0u);
    CHECKV(FR->cd.cf5, 1u);
    CHECKV(FR->cd.ff_e4a4, 3);
    CHECKV(FR->cd.ff_e6a1, 3);                           /* P: block 0 gear 3 (B applied) */
    CHECKV(ev_ms(FR_EV_CF5, 1u), 1050u);                 /* P0850's pending 50 */
    CHECKV(FR->w.unp_n, 0u);
    CHECKV(FR->w.ff_err_n, 0u);
    CHECKV(FM->bad_g_n, 0u);
    /* the bus-1 node groups never tested (U11E4 failing from 1150) */
    CHECKV(FM->s[139u - 1u] & 1u, 0u);
    printf("plain boot: P1DCA %u U11E4 %u (pending %u) U1267 %u P0850 %u U11E3 %u; masks %08X / %08X; code %02X → %d, "
           "0x89CF5 %u, fault-fed target %d\n", (unsigned)ev_ms(FR_EV_CONFIRM, 28u),
           (unsigned)ev_ms(FR_EV_CONFIRM, 136u), (unsigned)ev_ms(FR_EV_PEND_ON, 136u),
           (unsigned)ev_ms(FR_EV_CONFIRM, 109u), (unsigned)ev_ms(FR_EV_CONFIRM, 98u),
           (unsigned)ev_ms(FR_EV_DIRECT, 143u), (unsigned)FM->m0, (unsigned)FM->m1, FR->cd.c41,
           FR->cd.c43, FR->cd.cf5, FR->cd.ff_e6a1);
}

/* the code history over a run (the last value per pass) */
static void test_sims(void)
{
    scen_t s = { .sims = 1, .d6d6 = 8u };
    reset();
    run(&s, 1290u);
    CHECKV(FR->cd.c41, 0u);
    CHECKV(FR->cd.c43, 0);
    run(&s, 1300u);
    CHECKV(FR->cd.c41, 3u);                              /* U0100 failing → pending 22 (0x16) */
    CHECKV(FR->cd.c43, 1);                               /* 0xDB63C(3): the gear code (healthy 1) */
    run(&s, 7000u);
    dump_events("sims");
    CHECKV(FM->s[136u - 1u] & 0x000Eu, 0u);              /* U11E4: never failed */
    CHECK((FM->s[136u - 1u] & 1u) != 0u);
    CHECKV(ev_ms(FR_EV_DIRECT, 140u), 1450u);            /* U0121 */
    CHECKV(ev_ms(FR_EV_DIRECT, 139u), 1790u);            /* U0100 */
    CHECKV(ev_ms(FR_EV_DIRECT, 146u), 1790u);            /* U0140 (0x112) */
    CHECKV(ev_ms(FR_EV_DIRECT, 145u), 1790u);            /* U0212 (0x102) */
    CHECKV(ev_ms(FR_EV_DIRECT, 141u), 1790u);            /* U0102 (0x160) */
    CHECKV(ev_ms(FR_EV_DIRECT, 147u), 6240u);            /* U0155 (0x310) */
    CHECKV(ev_ms(FR_EV_PEND_ON, 139u), 1300u);
    CHECKV(ev_ms(FR_EV_CONFIRM, 98u), 3150u);
    CHECKV(ev_ms(FR_EV_CONFIRM, 109u), 0u);              /* U1267: the lever valid */
    CHECKV(fr_fm_confirmed(142u), 0u);                   /* 0x2EA valid */
    CHECKV(fr_fm_confirmed(143u), 0u);                   /* 0x210 valid */
    CHECKV(fr_fm_react(13u), 1u);
    CHECKV(fr_fm_react(50u), 1u);
    CHECKV(fr_fm_react(16u), 1u);                        /* U0155 → D661 (rng_chain c64d4) */
    CHECKV(fr_fm_react(53u), 1u);                        /* U0121 / P0850 → D660 */
    CHECKV(fr_fm_react(59u), 0u);                        /* D664 / D666: none of these groups */
    CHECKV(fr_fm_react(61u), 0u);
    CHECKV(FM->m0, 0x00812620u);                         /* + group 28 (P1DCA): ids 9 and 40 */
    CHECKV(FM->m1, 0xC0260114u);
    CHECKV(FR->cd.c41, 0x20u);
    CHECKV(FR->cd.c43, 3);
    CHECKV(FR->cd.ff_e6a1, 3);
    printf("sims: U0121 %u U0100 %u U0140 %u U0212 %u U0102 %u U0155 %u P0850 %u; masks %08X / %08X\n",
           (unsigned)ev_ms(FR_EV_DIRECT, 140u), (unsigned)ev_ms(FR_EV_DIRECT, 139u),
           (unsigned)ev_ms(FR_EV_DIRECT, 146u), (unsigned)ev_ms(FR_EV_DIRECT, 145u),
           (unsigned)ev_ms(FR_EV_DIRECT, 141u), (unsigned)ev_ms(FR_EV_DIRECT, 147u),
           (unsigned)ev_ms(FR_EV_CONFIRM, 98u), (unsigned)FM->m0, (unsigned)FM->m1);
    /* D at 8 s: 4 while the stop latch +0x14 runs down (19 passes after the
     * first D pass decremented it), 0x1C for 5 (+0x16), then 0x20 */
    s.lever_at = 8000u;
    s.d6d6_late = 5u;
    unsigned n4 = 0u, n1c = 0u, first20 = 0u;
    int8_t ff_min = 9, ff_max = -2;
    while (g_ms < 9000u) {
        tick(&s);
        if (g_ms >= 8000u) {
            if (FR->cd.c41 == 4u) {
                n4++;
            } else if (FR->cd.c41 == 0x1Cu) {
                n1c++;
            } else if (FR->cd.c41 == 0x20u && first20 == 0u && n1c != 0u) {
                first20 = g_ms;
            }
            if (FR->cd.ff_e6a1 < ff_min) { ff_min = FR->cd.ff_e6a1; }
            if (FR->cd.ff_e6a1 > ff_max) { ff_max = FR->cd.ff_e6a1; }
        }
    }
    CHECKV(n4, 19u);                                     /* +0x14: 20 → 0 from the first D pass */
    CHECKV(n1c, 5u);                                     /* +0x16: 5 */
    CHECKV(first20, 8240u);
    CHECKV(FR->cd.c43, 3);
    printf("D: code 4 x%u, 0x1C x%u, 0x20 from %u ms; 0x89C43 %d; the fault-fed target %d..%d "
           "(0xD1990 from the healthy gear code 1)\n", n4, n1c, first20, FR->cd.c43, ff_min, ff_max);
    /* R */
    s.lever_at = 9000u;
    s.d6d6_late = 7u;
    run(&s, 9500u);
    CHECKV(FR->cd.ff_e6a1, -1);
}

/* ── (4) heal ──────────────────────────────────────────────────────────── */
static void test_heal(void)
{
    const scen_t s = { .sims = 1, .frames_from = 3000u, .d6d6 = 8u };
    reset();
    run(&s, 3460u);
    CHECKV(fr_fm_react(13u), 0u);                        /* healed; U0100 not yet tested */
    CHECKV(fr_fm_react(50u), 0u);                        /* P0850 confirmed (its pending 50
                                                          * dropped); U11E4 healed */
    CHECKV(FR->cd.cf5, 0u);                              /* the healthy Sys law's window */
    run(&s, 6000u);
    dump_events("heal");
    CHECKV(ev_ms(FR_EV_CONFIRM, 136u), 1600u);
    const uint32_t h = ev_ms(FR_EV_HEAL, 136u);
    CHECKV(h, 3450u);                                    /* 450 ms after its first pass (3000) */
    CHECKV(fr_fm_in_masks(136u), 0u);                    /* rule 0x511: bit 8 → dropped */
    CHECKV(fr_fm_in_masks(98u), 1u);                     /* rule 0x011 → kept */
    CHECKV(ev_ms(FR_EV_HEAL, 109u), 3510u);              /* U1267: its first pass at 3010 (20 ms) */
    CHECKV(fr_fm_in_masks(109u), 0u);
    CHECKV(FM->s[136u - 1u] & 0x0040u, 0u);              /* no pending left */
    /* after the heal (3450) the bus-1 timer restarts on the next pass (3460,
     * 0x136506: U11E4 no longer active): testable at +650 (4110), the lost
     * countdown 50 → U0100 at 4600 */
    CHECKV(ev_ms(FR_EV_DIRECT, 139u), 4600u);
    printf("heal: U11E4 healed %u, U1267 %u, U0100 re-confirmed %u; masks %08X / %08X\n", (unsigned)h,
           (unsigned)ev_ms(FR_EV_HEAL, 109u), (unsigned)ev_ms(FR_EV_DIRECT, 139u),
           (unsigned)FM->m0, (unsigned)FM->m1);
}

/* ── (5) key-off / key-on ─────────────────────────────────────────────── */
static void test_keycycle(void)
{
    const scen_t s = { .sims = 1, .keyoff_at = 8000u, .keyon_at = 9000u, .d6d6 = 8u };
    reset();
    run(&s, 7990u);
    const uint32_t m0 = FM->m0;
    CHECK(m0 != 0u);
    run(&s, 9050u);
    CHECKV(FM->recycle_n, 1u);
    CHECKV(ev_ms(FR_EV_RECYCLE, 0u), 9000u);
    CHECKV(FM->p0, 0u);
    CHECKV(FM->p1, 0u);
    CHECKV(fr_fm_in_masks(139u), 0u);                    /* rule 0x599: bit 4 → dropped */
    CHECKV(fr_fm_in_masks(98u), 0u);                     /* 0x011 */
    CHECKV(fr_fm_in_masks(147u), 0u);                    /* 0x111 */
    printf("key cycle: masks before %08X, after the re-cycle %08X / %08X\n", (unsigned)m0,
           (unsigned)FM->m0, (unsigned)FM->m1);
}

/* ── (8) groups 27 / 28: the TOE32 (k = 3, PL7) off / on-state test ─────── */
#define T3S (fr_shadow_t3_state())
static unsigned t3_untested_passes(const scen_t *s, unsigned from, unsigned to)
{
    unsigned n = 0u;
    run(s, from);
    while (g_ms < to) {
        tick(s);
        if (T3S->rep28 == 0u) {
            n++;
        }
    }
    return n;
}

static void test_g28(void)
{
    /* the conversion 0x93658: the bench's / the dump's ring 9008 → 3436 mV
     * (the dump's 0x9089C[3] = 0x908A6[3] = 0x0D6C), the dump's live
     * accumulator 0xF670 / 7 → 3438; 0 → 1; n = 0 → 0 */
    CHECKV(fr_shadow_t3_conv(2u * 9008u, 2u), 3436u);
    CHECKV(fr_shadow_t3_conv(0xF670u, 7u), 3438u);
    CHECKV(fr_shadow_t3_conv(0u, 2u), 1u);
    CHECKV(fr_shadow_t3_conv(5u, 0u), 0u);
    /* (a) the plain boot: the dump's state (0x938F6[3] 0x10, 0x93900[3] 0x18,
     * 0x940D4[3] 0x84), group 28 failed + tested every pass from 'L', 27
     * untested (code bit 7) */
    {
        const scen_t s = { .sims = 0, .d6d6 = 8u };
        reset();
        run(&s, 600u);
        CHECKV(T3S->gate_n, 0u);                        /* before 'L' (0x95005 = 1) */
        CHECKV(T3S->en, 0u);
        CHECKV(FM->s[28u - 1u] & 1u, 0u);              /* untested */
        run(&s, 1500u);
        CHECKV(T3S->v1, 3436u);
        CHECKV(T3S->v2, 3436u);
        CHECKV(T3S->en, 0x18u);
        CHECKV(T3S->fl, 0x10u);
        CHECKV(T3S->m88, 0x18u);
        CHECKV(T3S->code, 0x84u);
        CHECKV(T3S->rep28, 3u);                         /* failed + tested */
        CHECKV(T3S->rep27, 1u);                         /* failed, untested */
        CHECKV(T3S->gate_ms, T3_L_MS);
        CHECKV(T3S->fail_ms, T3_L_MS);
        CHECKV(T3S->first, 1u);
        CHECKV(fr_fm_confirmed(28u), 1u);
        CHECKV(fr_fm_confirmed(27u), 0u);
        CHECKV(FM->s[27u - 1u] & 1u, 0u);               /* 27 never tested */
        CHECKV(fr_fm_react(40u), 1u);                   /* reaction 40: group 28 alone */
        printf("g28 (a) plain: gate %u ms, P1DCA confirm %u ms, v %u / %u mV, en %02X fl %02X code %02X\n",
               (unsigned)T3S->gate_ms, (unsigned)ev_ms(FR_EV_CONFIRM, 28u), T3S->v1, T3S->v2, T3S->en, T3S->fl,
               T3S->code);
    }
    /* (b) PL7 <= 2400 mV (raw 6000 = 2288 mV): short to ground → 0x08 →
     * code 03 → group 27 (P1D8D), not 28 */
    {
        const scen_t s = { .sims = 0, .d6d6 = 8u, .pl7_raw = 6000u };
        reset();
        run(&s, 1500u);
        CHECKV(T3S->v1, 2288u);
        CHECKV(T3S->fl, 0x08u);
        CHECKV(T3S->code, 0x03u);
        CHECKV(T3S->rep27, 3u);
        CHECKV(T3S->rep28, 1u);
        CHECKV(ev_ms(FR_EV_START, 27u), T3_L_MS);
        CHECK(fr_fm_confirmed(27u) == 1u);
        CHECKV(fr_fm_confirmed(28u), 0u);
        /* group 27 carries the same reactions as 28 (ids 9, 40) */
        CHECKV(FRC_R0(FR_CFG(27u)), FRC_R0(FR_CFG(28u)));
        CHECKV(FRC_R1(FR_CFG(27u)), FRC_R1(FR_CFG(28u)));
        CHECKV(fr_fm_react(40u), 1u);
        printf("g28 (b) 2288 mV: P1D8D confirm %u ms, code %02X; masks %08X / %08X\n",
               (unsigned)ev_ms(FR_EV_CONFIRM, 27u), T3S->code, (unsigned)FM->m0, (unsigned)FM->m1);
    }
    /* (c) PL7 >= 4000 mV (raw 12000 = 4577 mV, a load to battery): both pass */
    {
        const scen_t s = { .sims = 0, .d6d6 = 8u, .pl7_raw = 12000u };
        reset();
        run(&s, 3300u);
        CHECKV(T3S->v1, 4577u);
        CHECKV(T3S->en, 0x18u);
        CHECKV(T3S->fl, 0u);
        CHECKV(T3S->code, 0x10u);
        CHECKV(T3S->rep27, 2u);
        CHECKV(T3S->rep28, 2u);
        CHECKV(FM->s[28u - 1u] & 1u, 1u);               /* tested */
        CHECKV(fr_fm_confirmed(28u), 0u);
        CHECKV(fr_fm_confirmed(27u), 0u);
        CHECKV(T3S->fail_n, 0u);
        CHECKV(FM->m1, 0xE8260014u);                    /* the r04x plain-boot masks: no id 40 */
    }
    /* (d) heal: open from 'L', a load from 1500 ms → group 28 heals 100 ms
     * later and leaves the masks (rule 0x0111, bit 8) */
    {
        const scen_t s = { .sims = 0, .d6d6 = 8u, .pl7_at = 1500u, .pl7_raw2 = 12000u };
        reset();
        run(&s, 2000u);
        const uint32_t h = ev_ms(FR_EV_HEAL, 28u);
        CHECK(h >= 1590u && h <= 1610u);
        CHECKV(fr_fm_in_masks(28u), 0u);
        CHECKV(fr_fm_react(40u), 0u);
        CHECKV(fr_fm_react(9u), 1u);                    /* id 9 stays (U1267) */
        printf("g28 (d) heal: confirmed %u, healed %u ms; masks %08X / %08X\n",
               (unsigned)ev_ms(FR_EV_CONFIRM, 28u), (unsigned)h, (unsigned)FM->m0, (unsigned)FM->m1);
    }
    /* (e) the gate closed: never 'L' / the supply below 6500 / above 23500
     * → nothing tested, both untested every pass */
    {
        const scen_t s1 = { .sims = 0, .d6d6 = 8u, .no_l = 1 };
        const scen_t s2 = { .sims = 0, .d6d6 = 8u, .sup_mv = 6400u };
        const scen_t s3 = { .sims = 0, .d6d6 = 8u, .sup_mv = 23600u };
        const scen_t *ss[3] = { &s1, &s2, &s3 };
        for (unsigned i = 0u; i < 3u; i++) {
            reset();
            run(ss[i], 2000u);
            CHECKV(T3S->gate_n, 0u);
            CHECKV(T3S->en, 0u);
            CHECKV(T3S->untest_n, T3S->passes);
            CHECKV(FM->s[28u - 1u] & 1u, 0u);
            CHECKV(FM->s[27u - 1u] & 1u, 0u);
            CHECKV(fr_fm_confirmed(28u), 0u);
            CHECKV(T3S->gate_ms, 0u);
        }
        /* 6700 mV (source FE's threshold) .. 6500: the window opens at 6500 but
         * 0x9482D reloads below 6700 → still closed at 6600 */
        const scen_t s4 = { .sims = 0, .d6d6 = 8u, .sup_mv = 6600u };
        reset();
        run(&s4, 2000u);
        CHECKV(T3S->gate_n, 0u);
        const scen_t s5 = { .sims = 0, .d6d6 = 8u, .sup_mv = 6700u };
        reset();
        run(&s5, 2000u);
        CHECK(T3S->gate_n > 0u);
    }
    /* (f) a duty-category change (0 → 1000 at 1000 ms): 0x94828 = 4 (20 ms)
     * closes the gate for exactly 2 passes; on with the pin at 3436 mV (< 3500:
     * pulled low) → no fault in mask 0x04, the old 0x10 latched outside it →
     * the reporter's "untested" */
    {
        const scen_t s = { .sims = 0, .d6d6 = 8u, .duty_at = 1000u, .duty = 1000u };
        reset();
        run(&s, 990u);
        const uint32_t g0 = T3S->gate_n;
        run(&s, 1010u);
        CHECKV(T3S->gate_n, g0);                        /* 1000, 1010: closed */
        run(&s, 1020u);
        CHECKV(T3S->gate_n, g0 + 1u);
        CHECKV(T3S->en, 0x04u);
        CHECKV(T3S->fl, 0x10u);
        CHECKV(T3S->rep28, 0u);
        CHECKV(t3_untested_passes(&s, 1020u, 1500u), 48u);
    }
}

/* ── (6) the dump ──────────────────────────────────────────────────────── */
static void test_dump_masks(void)
{
    static const uint8_t gs[4] = { 28u, 98u, 109u, 136u };
    uint32_t m0 = 0u, m1 = 0u;
    for (unsigned i = 0u; i < 4u; i++) {
        m0 |= FRC_R0(FR_CFG(gs[i]));
        m1 |= FRC_R1(FR_CFG(gs[i]));
    }
    CHECKV(m0, 0x00813240u);
    CHECKV(m1, 0xE8260114u);
#if defined(TCU_STOCK_DUMP)
    {
        static uint8_t d[0x20000];
        FILE *f = fopen(TCU_STOCK_DUMP, "rb");
        if (f != NULL && fread(d, 1u, sizeof d, f) == sizeof d) {
            const uint32_t dm0 = ((uint32_t)d[0x97D0] << 24) | ((uint32_t)d[0x97D1] << 16) |
                                 ((uint32_t)d[0x97D2] << 8) | d[0x97D3];
            const uint32_t dm1 = ((uint32_t)d[0x97D8] << 24) | ((uint32_t)d[0x97D9] << 16) |
                                 ((uint32_t)d[0x97DA] << 8) | d[0x97DB];
            CHECKV(dm0, m0);
            CHECKV(dm1, m1);
            /* the RAM-built tables taken from the dump */
            for (unsigned n = 0u; n < FR_NE; n++) {
                CHECKV(d[0x5289u + 3u * n], fr_cal_ent[n][0]);
                CHECKV(d[0x5289u + 3u * n + 1u], fr_cal_ent[n][1]);
                CHECKV(d[0x5289u + 3u * n + 2u], fr_cal_ent[n][2]);
            }
            const uint32_t s0 = ((uint32_t)d[0x15858] << 24) | ((uint32_t)d[0x15859] << 16) |
                                ((uint32_t)d[0x1585A] << 8) | d[0x1585B];
            CHECKV(s0, fr_cal_13a.sub0);
            CHECKV(d[0x15863], (uint8_t)fr_cal_13a.sub1);
        }
        if (f != NULL) {
            fclose(f);
        }
    }
#endif
}

/* ── --cal ─────────────────────────────────────────────────────────────── */
static uint8_t g_img[0x200000];
static unsigned U16(unsigned a) { return ((unsigned)g_img[a] << 8) | g_img[a + 1]; }
static unsigned U32(unsigned a) { return ((unsigned)g_img[a] << 24) | ((unsigned)g_img[a + 1] << 16) | ((unsigned)g_img[a + 2] << 8) | g_img[a + 3]; }
static int test_cal(void)
{
#if !defined(TCU_STOCK_IMAGE)
    return 77;
#else
    FILE *f = fopen(TCU_STOCK_IMAGE, "rb");
    if (f == NULL || fread(g_img, 1u, sizeof g_img, f) != sizeof g_img) {
        printf("SKIP no stock image %s\n", TCU_STOCK_IMAGE);
        return 77;
    }
    fclose(f);
    for (unsigned i = 0u; i < FR_NG; i++) {
        CHECKV(U32(0x4F1D0u + 4u * i), 0x1923ECu + 36u * i);
        CHECK(memcmp(FR_CFG(i + 1u), &g_img[0x1923ECu + 36u * i], 36u) == 0);
        for (unsigned k = 0u; k < 3u; k++) {
            CHECKV(FR_DEP(i + 1u, k), U16(0x1955A8u + 6u * i + 2u * k));
        }
    }
    for (unsigned k = 0u; k < 12u; k++) {
        static const unsigned want[12] = { 0x195EBEu, 0x195EC0u, 0x195EC2u, 0x195EC4u, 0x195D76u, 0x195EC6u,
                                           0x195EC8u, 0x195D77u, 0x191B2Cu, 0x191BB0u, 0x191C34u, 0x195D78u };
        CHECKV(U32(0x642BCu + 4u * k), want[k]);
    }
    CHECKV(fr_cal_comm.sup_min, U16(0x195EBEu));
    CHECKV(fr_cal_comm.sup_max, U16(0x195EC0u));
    CHECKV(fr_cal_comm.thr_u0001, U16(0x195EC2u));
    CHECKV(fr_cal_comm.thr_u11e4, U16(0x195EC4u));
    CHECKV(fr_cal_comm.thr_u0074, U16(0x195EC6u));
    CHECKV(fr_cal_comm.thr_137, U16(0x195EC8u));
    CHECKV(fr_cal_comm.mask_u0001, g_img[0x195D76u]);
    CHECKV(fr_cal_comm.mask_u0074, g_img[0x195D77u]);
    CHECKV(fr_cal_comm.b2en, g_img[0x195D78u]);
    for (unsigned n = 0u; n < FR_NE; n++) {
        CHECKV(FR_RELOAD(n), U16(0x191B30u + 2u * n));
        CHECKV(FR_THR(n), U16(0x191BB4u + 2u * n));
        CHECKV(FR_MASK(n), g_img[0x191C38u + n]);
        /* the entry's status getter → the ROM object */
        const unsigned g = U32(0x4BCE0u + 4u * n);
        const unsigned w0 = U16(g), w1 = U16(g + 2u);
        unsigned imm = (((w0 >> 4) & 0xFu) << 16) | w1;
        if (imm & 0x80000u) {
            imm |= 0xFFF00000u;
        }
        CHECKV(imm, fr_cal_rx[n].st);
        CHECKV(U32(fr_cal_rx[n].obj + 0x10u), fr_cal_rx[n].st - 8u);
        CHECKV(U16(fr_cal_rx[n].obj + 4u), fr_cal_rx[n].to);
        CHECKV(g_img[fr_cal_rx[n].obj + 0xCu], fr_cal_rx[n].dlc);
        CHECKV(g_img[fr_cal_rx[n].en_a], fr_cal_rx[n].en);
        CHECKV(U32(fr_cal_rx[n].obj + 0x18u) != 0u, fr_cal_rx[n].crc);
        CHECKV(U32(fr_cal_rx[n].obj + 0x1Cu) != 0u, fr_cal_rx[n].ctr);
    }
    for (unsigned k = 0u; k < 33u; k++) {
        CHECKV(fr_cal_13a_ptr[k], U32(0x64450u + 4u * k));
    }
    for (unsigned k = 0u; k < 26u; k++) {
        CHECKV(fr_cal_13a.w[k], U16(fr_cal_13a_ptr[k]));
    }
    for (unsigned k = 0u; k < 6u; k++) {
        CHECKV(fr_cal_13a.b[k], g_img[fr_cal_13a_ptr[26u + k]]);
    }
    CHECKV(fr_cal_13a.x80, U16(fr_cal_13a_ptr[32]));
    CHECKV(fr_cal_14e_reload, U16(U32(0x64CC8u + 0x28u)));
    CHECKV(fr_cal_ctr_step, g_img[U32(0x664D8u)]);
    CHECK(memcmp(fr_cal_6344e, &g_img[0x6344Eu], 54u) == 0);
    /* group 27 / 28 (the TOE32 test): every field against the image, the
     * cal-bank pointers resolved here again ([0xFFF90AB8 = 0]) */
    {
#define BANK(pool) U32(U32(pool))
        CHECKV(U32(0x914E0u), 0x1918E6u);
        CHECKV(fr_cal_g28.fs, g_img[0x1918E6u]);
        CHECKV(fr_cal_g28.en, g_img[BANK(0x84BB0u)]);
        CHECKV(fr_cal_g28.src, g_img[BANK(0x8B6D8u) + 3u]);
        CHECKV(fr_cal_g28.r2d, g_img[BANK(0x8B6F8u) + 3u]);
        CHECKV(fr_cal_g28.r28, g_img[BANK(0x8B798u) + 3u]);
        CHECKV(fr_cal_g28.win, g_img[BANK(0x8B79Cu)]);
        CHECKV(fr_cal_g28.slo, U16(BANK(0x8B7A0u) + 6u));
        CHECKV(fr_cal_g28.shi, U16(BANK(0x8B7A4u) + 6u));
        CHECKV(fr_cal_g28.fe, U16(BANK(0x8B700u)));
        CHECKV(fr_cal_g28.lo, U16(BANK(0x8B4A8u) + 6u));
        CHECKV(fr_cal_g28.hi, U16(BANK(0x8B4ACu) + 6u));
        CHECKV(fr_cal_g28.on, U16(BANK(0x8B4B0u) + 6u));
        CHECKV(fr_cal_g28.dmin, U16(U32(0x8B4A0u) + 6u));
        CHECKV(fr_cal_g28.dmax, U16(U32(0x8B4A4u) + 6u));
        CHECKV(fr_cal_g28.cut, g_img[BANK(0x8B4B4u)]);
        CHECKV(fr_cal_g28.cvt, g_img[BANK(0x8B6D4u)]);
        CHECKV(fr_cal_g28.andm, U16(BANK(0x8B6E4u) + 6u));
        CHECKV(U32(0x7B4D8u), 0x45C84u);
        CHECKV(fr_cal_g28.g27, g_img[0x45C84u + 48u + 12u]);
        CHECKV(fr_cal_g28.g28, g_img[0x45C84u + 48u + 13u]);
        CHECKV(U32(0x7B4E8u), 0x45CD4u);
        for (unsigned i = 0u; i < 7u; i++) {
            CHECKV(fr_cal_g28.ctab[i][0], U16(0x45CD4u + 4u * i));
            CHECKV(fr_cal_g28.ctab[i][1], U16(0x45CD4u + 4u * i + 2u));
        }
#undef BANK
    }
    for (unsigned i = 0u; i < FR_CAL_NCHK; i++) {
        const fr_cal_chk_t *c = &fr_cal_chk[i];
        const unsigned v = (c->width == 2u) ? U16(c->addr) : g_img[c->addr];
        CHECKV(v, c->val);
    }
    printf("(cal) %d mismatches\n", g_fail);
    return g_fail ? 1 : 0;
#endif
}

/* ── --time ────────────────────────────────────────────────────────────── */
static int test_time(void)
{
    const scen_t s = { .sims = 1, .d6d6 = 8u };
    struct timespec a, b;
    reset();
    run(&s, 2000u);
    timespec_get(&a, TIME_UTC);
    run(&s, 12000u);
    timespec_get(&b, TIME_UTC);
    const double ns = ((double)(b.tv_sec - a.tv_sec) * 1e9 + (double)(b.tv_nsec - a.tv_nsec)) / 1000.0;
    printf("host: %.0f ns per 10 ms tick (sims, steady)\n", ns);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        return test_cal();
    }
    win_map();
    if (argc > 1 && strcmp(argv[1], "--time") == 0) {
        return test_time();
    }
    win_fill();
    test_rx();
    test_plain();
    test_sims();
    test_heal();
    test_keycycle();
    test_g28();
    test_dump_masks();
    CHECKV(win_changed(), 0u);                           /* (7) no live cell written */
    printf(g_fail ? "test_fr_shadow: %d FAILURES\n" : "test_fr_shadow: ok\n", g_fail);
    return g_fail ? 1 : 0;
}

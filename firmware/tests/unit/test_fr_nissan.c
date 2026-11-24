/* test_fr_nissan.c — OPTION C (2026-11-23): the fault-reaction shadow's comm monitor on a
 * NISSAN bus (app/fr_cal_nissan.c; built with TCU_VEHICLE_NISSAN=1, the Nissan configure only).
 *
 * A 10 ms timeline like test_fr_shadow's (OS mode 1 from +650 ms, E9FE from +20 ms), with the
 * Nissan frames at their logged periods (docs/reference/nissan-ecm-tcm-interface.md: ECM 0x1F9
 * / 0x180 / 0x182 / 0x160 10 ms, 0x551 100 ms; ABS 0x280 / 0x284 / 0x285 20 ms, 0x354 40 ms;
 * BCM 0x35D 100 ms; DLC 8, 0x160 7; no CRC). The lever / park reports (groups 109 / 98) are
 * fed healthy so only the comm groups move; the TOE32 test is not fed (it skips).
 *
 *   (1) every Nissan frame present: no comm group (136 / 139 / 140 / 146) ever starts, the
 *       FCA-only groups (141 142 143 144 145 147 166 170 171) are never reported, no frame is
 *       "other", the masks stay 0
 *   (2) 0x1F9 stops at 5 s: U0100 (139) confirms directly at 5540 ms (lost 5050, then the
 *       stock lost time of its entry) — and its reactions as the shadow computes them (the
 *       masks = group 139's words, 13 = the gear code 0x20 -> 3, 50; the substitute target 3);
 *       nothing else
 *   (3) 0x284 stops: U0121 (140) at 5190; reaction 53 (D660) alone; nothing else
 *   (4) 0x35D stops: U0140 (146) at 5900; reactions 36 / 62; nothing else
 *   (5) 0x551 (100 ms ECM) -> 139 at 5900, 0x160 (7 bytes) -> 139 at 5540, 0x354 (40 ms)
 *       -> 140 at 5440
 *   (6) no frame at all (a dead bus): U11E4 (136) confirms (every Nissan key message lost)
 *       and the node groups stay untested (the stock's gate); the FCA-only groups unreported
 *   (7) the FCA frames (0x108 / 0x140 / 0x2EA / 0x210) on the Nissan build match no entry
 *   (8) the inert entries: status 0 and timer never counted, whatever arrives */
#include "app/fr_shadow.h"
#include "app/fr_cal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(TCU_VEHICLE_NISSAN)
#error "test_fr_nissan.c: the Nissan comm table - built with TCU_VEHICLE_NISSAN=1"
#endif

static int g_fail;
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); g_fail++; } } while (0)
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define FR (fr_shadow_state())
#define FM (fr_fm_state())
#define M1_MS 650u

typedef struct { uint16_t id; uint8_t dlc; uint8_t per10; } nmsg_t;   /* period in 10 ms ticks */
static const nmsg_t k_msgs[] = {
    { 0x1F9u, 8u, 1u }, { 0x180u, 8u, 1u }, { 0x182u, 8u, 1u }, { 0x160u, 7u, 1u }, { 0x551u, 8u, 10u },
    { 0x280u, 8u, 2u }, { 0x284u, 8u, 2u }, { 0x285u, 8u, 2u }, { 0x354u, 8u, 4u }, { 0x35Du, 8u, 10u },
};
#define NMSG (sizeof k_msgs / sizeof k_msgs[0])

static const uint8_t k_fca_only[] = { 141u, 142u, 143u, 144u, 145u, 147u, 166u, 170u, 171u };

typedef struct {
    int      frames;        /* the Nissan frames from power-on */
    uint16_t stop_id;       /* this message stops at stop_ms (0 = none) */
    unsigned stop_ms;
    int      fca;           /* send the FCA sims' frames as well */
} scen_t;

static unsigned g_ms;
static uint32_t g_rep109, g_rep98;
static unsigned g_mpass;
static uint8_t g_ctr;

static void tick(const scen_t *s)
{
    fr_in_t in;
    memset(&in, 0, sizeof in);
    g_ms += 10u;
    const unsigned ms = g_ms, t = ms / 10u;
    if (s->frames) {
        for (unsigned i = 0u; i < NMSG; i++) {
            const nmsg_t *m = &k_msgs[i];
            if (m->id == s->stop_id && ms >= s->stop_ms) {
                continue;
            }
            if (t % m->per10 == 0u) {
                uint8_t d[8] = { 0 };
                d[6] = (uint8_t)(g_ctr & 0x0Fu);    /* a low-nibble counter (not checked) */
                fr_hook_rx(0u, m->id, m->dlc, d);
            }
        }
        g_ctr++;
    }
    if (s->fca) {
        static const uint8_t z[8] = { 0 };
        fr_hook_rx(0u, 0x108u, 8u, z);
        fr_hook_rx(0u, 0x140u, 8u, z);
        fr_hook_rx(0u, 0x2EAu, 5u, z);
        fr_hook_rx(1u, 0x210u, 5u, z);
    }
    in.os_mode = (ms >= M1_MS) ? 1u : 0u;
    in.e9fe = (uint8_t)(ms >= 20u);
    in.e9ff = (uint8_t)((ms == 20u) ? 1u : 2u);
    in.d6d6 = 8u;                                   /* P */
    in.c3d = (uint8_t)(in.os_mode == 1u);
    if (in.os_mode >= 1u) {                         /* groups 109 / 98 fed healthy */
        if (g_mpass % 2u == 0u) {
            in.u_failed = 0u;
            in.u_tested = (uint8_t)(g_mpass > 100u);
            g_rep109++;
        }
        if (g_mpass % 10u == 0u) {
            g_rep98++;
        }
        g_mpass++;
    }
    in.g98_failed = 0u;
    in.g98_tested = 1u;
    in.rep109_n = g_rep109;
    in.rep98_n = g_rep98;
    in.eng_st = 0u;
    in.eng = 700u;
    in.gf_e69f = 1; in.gf_c25 = 1; in.gf_c28 = 1; in.gf_c2b = 1; in.gf_cce4 = 1; in.gf_e343 = 1;
    in.gf_e6a1 = 1;
    in.ce8 = 1u;
    in.cd2 = 0x4Fu;
    in.sup_mv = 12950u;
    in.cd0 = in.eng;
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
    g_ms = 0u;
    g_rep109 = g_rep98 = 0u;
    g_mpass = 0u;
    g_ctr = 0u;
}

static uint32_t ev_ms(uint8_t kind, uint8_t g)
{
    for (unsigned i = 0u; i < FM->rec_n; i++) {
        if (FM->rec[i].kind == kind && FM->rec[i].g == g) {
            return FM->rec[i].ms;
        }
    }
    return 0u;
}

static unsigned ev_count(uint8_t g)                 /* every fault-manager event of a group */
{
    unsigned n = 0u;
    for (unsigned i = 0u; i < FM->rec_n; i++) {
        const uint8_t k = FM->rec[i].kind;
        if (k >= FR_EV_START && k <= FR_EV_RECYCLE && FM->rec[i].g == g) {
            n++;
        }
    }
    return n;
}

static void dump_events(const char *tag)
{
    printf("-- %s: %u events (%u seen)\n", tag, (unsigned)FM->rec_n, (unsigned)FM->rec_all);
    for (unsigned i = 0u; i < FM->rec_n; i++) {
        printf("   %6u ms kind %02X g %3u v %u\n", (unsigned)FM->rec[i].ms, FM->rec[i].kind,
               FM->rec[i].g, FM->rec[i].v);
    }
}

static void check_fca_only_silent(void)
{
    for (unsigned i = 0u; i < sizeof k_fca_only; i++) {
        const uint8_t g = k_fca_only[i];
        CHECKV(FM->s[g - 1u], 0u);                  /* never reported: the status untouched */
        CHECKV(FM->r[g - 1u] & 0x0Bu, 0u);          /* no record bit (tested / failed / active) */
        CHECKV(ev_count(g), 0u);
    }
}

static void check_inert(void)
{
    for (unsigned n = 0u; n < FR_NE; n++) {
        if (fr_cal_rx[n].en == 0u) {
            CHECKV(FR->rx[n].st, 0u);
            CHECKV(FR->cm.st[n], 0u);
            CHECKV(fr_cal_ent[n][2], 13u);
        }
    }
}

static unsigned confirmed_list(char *buf, size_t len)
{
    unsigned n = 0u;
    buf[0] = '\0';
    for (uint8_t g = 1u; g <= FR_NGRP; g++) {
        if (fr_fm_confirmed(g)) {
            const size_t l = strlen(buf);
            snprintf(buf + l, len - l, " %u", g);
            n++;
        }
    }
    return n;
}

/* (1) */
static void test_all_present(void)
{
    const scen_t s = { .frames = 1 };
    reset();
    run(&s, 10000u);
    dump_events("all Nissan frames present");
    char c[128];
    CHECKV(confirmed_list(c, sizeof c), 0u);
    CHECKV(ev_count(136u), 0u);
    CHECKV(ev_count(139u), 0u);
    CHECKV(ev_count(140u), 0u);
    CHECKV(ev_count(146u), 0u);
    CHECK((FM->s[139u - 1u] & 1u) != 0u);           /* tested and passed */
    CHECK((FM->s[140u - 1u] & 1u) != 0u);
    CHECK((FM->s[146u - 1u] & 1u) != 0u);
    CHECK((FM->s[136u - 1u] & 1u) != 0u);
    CHECKV(FR->w.rx_other, 0u);
    CHECKV(FM->m0, 0u);
    CHECKV(FM->m1, 0u);
    CHECKV(FR->cd.c41, 0u);                          /* no fault gear code */
    check_fca_only_silent();
    check_inert();
    printf("all present: confirmed {%s }, masks %08X / %08X, code %02X, frames %u\n", c,
           (unsigned)FM->m0, (unsigned)FM->m1, FR->cd.c41, (unsigned)FR->w.rx_frames);
}

/* (2)..(5): one message stops at 5 s */
static void test_stop(uint16_t id, uint8_t grp, unsigned want_ms)
{
    const scen_t s = { .frames = 1, .stop_id = id, .stop_ms = 5000u };
    char tag[64];
    reset();
    run(&s, 9000u);
    snprintf(tag, sizeof tag, "0x%03X stops at 5000 ms", id);
    dump_events(tag);
    CHECKV(ev_ms(FR_EV_DIRECT, grp), want_ms);
    CHECK(fr_fm_confirmed(grp) != 0u);
    char c[128];
    CHECKV(confirmed_list(c, sizeof c), 1u);         /* that group alone */
    static const uint8_t comm[] = { 136u, 139u, 140u, 146u };
    for (unsigned i = 0u; i < sizeof comm; i++) {
        if (comm[i] != grp) {
            CHECKV(ev_count(comm[i]), 0u);
        }
    }
    check_fca_only_silent();
    check_inert();
    printf("0x%03X stops: group %u direct at %u ms (5000 + %u); masks %08X / %08X; code %02X -> %d; "
           "reactions 13 %u 50 %u 53 %u 36 %u 62 %u; the fault-fed target %d\n", id, grp,
           (unsigned)ev_ms(FR_EV_DIRECT, grp), (unsigned)(ev_ms(FR_EV_DIRECT, grp) - 5000u),
           (unsigned)FM->m0, (unsigned)FM->m1, FR->cd.c41, FR->cd.c43, fr_fm_react(13u),
           fr_fm_react(50u), fr_fm_react(53u), fr_fm_react(36u), fr_fm_react(62u), FR->cd.ff_e6a1);
}

/* (6) */
static void test_dead_bus(void)
{
    const scen_t s = { .frames = 0 };
    reset();
    run(&s, 4000u);
    dump_events("no frame (dead bus)");
    CHECK(fr_fm_confirmed(136u) != 0u);              /* U11E4: every Nissan key message lost */
    CHECKV(FM->s[139u - 1u] & 1u, 0u);               /* the node groups untested (gate: 136) */
    CHECKV(FM->s[140u - 1u] & 1u, 0u);
    CHECKV(FM->s[146u - 1u] & 1u, 0u);
    CHECKV(fr_fm_confirmed(139u), 0u);
    CHECKV(fr_fm_react(13u), 1u);                    /* U11E4's reactions (13, 50) */
    CHECKV(fr_fm_react(50u), 1u);
    check_fca_only_silent();
    check_inert();
    printf("dead bus: U11E4 confirm %u ms; masks %08X / %08X; code %02X\n",
           (unsigned)ev_ms(FR_EV_CONFIRM, 136u), (unsigned)FM->m0, (unsigned)FM->m1, FR->cd.c41);
}

/* (7) + (8) */
static void test_fca_frames(void)
{
    const scen_t s = { .frames = 1, .fca = 1 };
    reset();
    run(&s, 3000u);
    CHECKV(FR->w.rx_other, 4u * 300u);               /* every FCA frame: no entry */
    char c[128];
    CHECKV(confirmed_list(c, sizeof c), 0u);
    check_fca_only_silent();
    check_inert();
    printf("FCA frames on the Nissan build: %u unmatched, no group\n", (unsigned)FR->w.rx_other);
}

int main(void)
{
    /* the times: the last frame before 5000 ms; the status goes lost at the pass its timer
     * passes the timeout (timer = 1 at the frame, +1 per silent pass, lost when > to); the
     * entry's countdown starts at the stock reload on that pass (reload - 1) and confirms
     * directly at 0: lost + 10 x (reload - 1) ms */
    test_all_present();
    test_stop(0x1F9u, 139u, 5540u);   /* last 4990, lost 5050 (to 5), + 490 (reload 50) */
    CHECKV(fr_fm_react(13u), 1u);     /* group 139's words 0x00802000 / 0x80060014: 13 (the */
    CHECKV(fr_fm_react(50u), 1u);     /* gear code 0x20), 23, 34, 36, 49, 50, 63 */
    CHECKV(FM->m0, 0x00802000u);
    CHECKV(FM->m1, 0x80060014u);
    CHECKV(FR->cd.c41, 0x20u);
    CHECKV(FR->cd.c43, 3);
    CHECKV(FR->cd.ff_e6a1, 3);        /* the substitute gear (P: block 0 gear 3) */
    test_stop(0x284u, 140u, 5190u);   /* last 4980, lost 5040, + 150 (reload 16) */
    CHECKV(fr_fm_react(53u), 1u);     /* group 140 -> D660 */
    CHECKV(fr_fm_react(13u), 0u);
    CHECKV(FM->m0, 0u);
    CHECKV(FM->m1, 0x00200000u);
    test_stop(0x35Du, 146u, 5900u);   /* last 4900, lost 5110 (to 20), + 790 (reload 80) */
    CHECKV(fr_fm_react(36u), 1u);     /* group 146 -> 36, 62 */
    CHECKV(fr_fm_react(62u), 1u);
    CHECKV(FM->m1, 0x40000010u);
    test_stop(0x551u, 139u, 5900u);   /* the 100 ms ECM frame: to 20, reload 80 */
    test_stop(0x160u, 139u, 5540u);   /* the 7-byte ECM frame */
    test_stop(0x354u, 140u, 5440u);   /* 40 ms: last 4960, lost 5050 (to 8), + 390 (reload 40) */
    test_dead_bus();
    test_fca_frames();
    printf(g_fail ? "test_fr_nissan: %d FAILURES\n" : "test_fr_nissan: ok\n", g_fail);
    return g_fail ? 1 : 0;
}

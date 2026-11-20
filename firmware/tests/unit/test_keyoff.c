/* Unit test: KEY-OFF AUTO-P AND THE FAULT-REACTION PARK REQUEST (2026-11-17,
 * ~/tcu_sweep/keyoff_autop_port.md). The firmware's 10 ms order: vehicle →
 * kl15 (the terminal-15 object 0x140B80) → the range chain → park_tick (the
 * park law with 0x89CF2, park_mon) → the two park demands.
 *
 * 1. kl15 alone: the bench (no 0x112, supply 13.2 V) → E9FF 0 → 1 → 2,
 *    quality 1 (voltage path, the learned agreement), EA03 0 — the dump's
 *    02 / 01 / 00; the supply dip below 4000 mV: 10 passes off-delay, then
 *    3, 4; back: 1, 2; codes 4 / 5 on (quality 0), 3 / 0 / 6 off (code 3
 *    sets EA03), 7 → the voltage path; the disagreement timer (code off,
 *    supply on, 50 passes) learns +0x21 = 2 → a lost 0x112 then gives
 *    quality 2; a torn complement → latched off.
 * 2. Key-off in D (lever D, brake + engine 700 sims, 0x112 code 4 from
 *    power-on): released in D; code 4 → 0: E9FF 3, then 4 → D5F2 0 →
 *    0xC286C D6DE 8 / 0x50 → D6D6 8 → both park outputs off; park_mon
 *    silent; key back on (code 4) with the lever still D: P is held (the
 *    D5FB latch); lever P then D: released again, park_mon silent.
 * 3. 0x112 lost in D → the voltage path keeps the range; supply < 4000 mV
 *    → off → auto-P.
 * 4. 0x89CF2: OS mode 3 → aux 1 → park request + lock → both outputs off
 *    while D6D6 = 5; back to mode 1 → released again; park_mon silent.
 * 5. The DSM operating cycle: U1267 confirmed (lever lost), a terminal
 *    re-cycle drops its reaction (the mode-1 rebuild 0xAACE4).
 */
#include "app/rng_chain.h"
#include "app/park.h"
#include "app/park_cells.h"
#include "app/park_law.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_cells.h"
#include "io/vehicle/vehicle.h"
#include "io/vehicle/vehicle_inject.h"
#include "safety/kl15.h"
#include "safety/park_mon.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

#define C8(a)   (*(volatile uint8_t *)(uintptr_t)(0xFFF80000u | (uint32_t)(a)))
#define SUPPLY  (*(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL)


/* the simulated vehicle: lever (1 P 2 R 3 N 4 D, 0 none), brake, engine,
 * 0x112 code (0xFF = no frame), the OS mode passed to the chain / park */
typedef struct {
    uint8_t  lever;
    bool     brake;
    uint16_t engine;
    uint8_t  ign;
    uint8_t  os_mode;
} sim_t;
static sim_t s;

static void reset(void)
{
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    CELL_TMP_BYTE = 67u;
    vehicle_init();
    park_init();
    rng_chain_init();
    kl15_init();
    SUPPLY = 13206u;
    memset(&s, 0, sizeof s);
    s.ign = 0xFFu;
    s.os_mode = 1u;
}

static void tick(void)
{
    uint8_t d[8];

    if (s.lever != 0u) {
        const uint8_t l[5] = { s.lever, 0, 0, 0, 0 };
        vehicle_inject_msg_lever(LEVER_SRC_PRIMARY, l[0]);
        vehicle_inject_msg_lever(LEVER_SRC_FALLBACK, l[0]);
    }
    if (s.brake) {
        vehicle_inject_msg_brake(BRAKE_ON);
    }
    if (s.engine != 0u) {
        vehicle_inject_msg_engine(s.engine, 0x1FFFu);
    }
    if (s.ign != 0xFFu) {
        vehicle_inject_msg_ign(s.ign);
    }
    vehicle_update_10ms();
    kl15_tick_10ms();
    rng_chain_tick_10ms(s.os_mode, 0u, 0xFFu);
    park_set_os_mode(s.os_mode);
    park_tick_10ms();
}

static void ticks(unsigned n)
{
    for (unsigned k = 0u; k < n; k++) {
        tick();
    }
}

/* both park outputs on (1), both off (0), other (2) */
static int outs(void)
{
    const uint16_t r = park_slot7_release(), h = park_slot11_hold();

    if (r == DRV_PARK_ON_REQ && h == DRV_PARK_ON_REQ) {
        return 1;
    }
    return (r == 0u && h == 0u) ? 0 : 2;
}

/* ── 1. the terminal object alone ──────────────────────────────────────── */
static void test_kl15(void)
{
    reset();
    tick();
    CHECKV(kl15_e9ff(), 1);                 /* 0 → 1 (voltage on) */
    CHECKV(kl15_e9fe(), 1);
    tick();
    CHECKV(kl15_e9ff(), 2);
    CHECKV(kl15_ea01(), 1);                 /* CAN invalid, agreement 1 → 1 */
    CHECKV(kl15_ea03(), 0);
    CHECKV(C8(0xE9F4u), 1);                 /* +0x20 CAN invalid (dump 01) */
    CHECKV(C8(0xE9F5u), 1);                 /* +0x21 (dump 01) */
    CHECKV(C8(0xE9F2u), 1);                 /* +0x1E voltage flag */
    CHECKV(C8(0xE9FCu), 0x01);              /* +0x28 status (dump 01) */
    CHECKV(C8(0xEA00u), 0xFD);              /* +0x2C (dump FD) */
    CHECKV(C8(0xEA02u), 0xFE);              /* +0x2E (dump FE) */
    CHECKV(*(volatile uint16_t *)(uintptr_t)KL15_V_CELL, 13206);
    /* D641 / D642 / D643 on the chain's pass: the dump's 02 / 01 / 00 */
    CHECKV(C8(0xD641u), 2);
    CHECKV(C8(0xD642u), 1);
    CHECKV(C8(0xD643u), 0);
    CHECKV(C8(0xD5F2u), 1);
    CHECKV(C8(0xD5F3u), 1);

    /* the supply below 4000 mV: 10 passes held on, then 3, 4 */
    SUPPLY = 3999u;
    for (unsigned k = 0u; k < 10u; k++) {
        tick();
        CHECKV(kl15_e9ff(), 2);
    }
    tick();
    CHECKV(kl15_e9ff(), 3);
    CHECKV(kl15_e9fe(), 1);
    CHECKV(C8(0xD5F2u), 1);                 /* 3 is still "on" for the chain */
    tick();
    CHECKV(kl15_e9ff(), 4);
    CHECKV(kl15_e9fe(), 0);
    CHECKV(C8(0xD5F2u), 0);
    CHECKV(C8(0xD5F5u), 1);                 /* D5F5: the falling edge of D5F2 */
    SUPPLY = 4000u;                         /* cmp/hs: 4000 is on */
    tick();
    CHECKV(kl15_e9ff(), 1);
    tick();
    CHECKV(kl15_e9ff(), 2);
    CHECKV(kl15_wit()->on_edges, 2);
    CHECKV(kl15_wit()->off_edges, 1);
    /* a short dip (< 10 passes) is bridged by the off-delay */
    SUPPLY = 0u;
    ticks(5u);
    SUPPLY = 13206u;
    ticks(20u);
    CHECKV(kl15_e9ff(), 2);
    CHECKV(kl15_wit()->off_edges, 1);

    /* the CAN path: code 4 on (quality 0), code 5 on, 3 off with EA03 */
    s.ign = 4u;
    tick();
    CHECKV(kl15_e9ff(), 2);
    CHECKV(kl15_ea01(), 0);
    CHECKV(kl15_ea03(), 1);
    CHECKV(C8(0xD642u), 0);
    CHECKV(C8(0xD5F3u), 0);
    s.ign = 5u;
    ticks(3u);
    CHECKV(kl15_e9ff(), 2);
    s.ign = 3u;
    tick();
    CHECKV(kl15_e9ff(), 3);
    CHECKV(kl15_ea03(), 1);
    tick();
    CHECKV(kl15_e9ff(), 4);
    s.ign = 4u;
    tick();
    CHECKV(kl15_e9ff(), 1);
    tick();
    CHECKV(kl15_e9ff(), 2);
    /* code 7 (signal not available): CAN invalid → the voltage path (on) */
    s.ign = 7u;
    ticks(5u);
    CHECKV(kl15_e9ff(), 2);
    CHECKV(kl15_ea01(), 1);
    CHECKV(kl15_ea03(), 0);
    CHECKV(C8(0xE9F4u), 1);
    CHECKV(C8(0xE9FCu), 0);                 /* the frame is fresh: status 0 */

    /* the disagreement timer: code 0 (off) with the supply on — 0x140FCC
     * state 0 (CAN on seen before) → ... → +0x21 = 2 after 50 passes */
    s.ign = 4u;
    ticks(3u);                              /* state 0 → 1 (CAN on, volt on) */
    CHECKV(C8(0xE9F6u), 1);
    s.ign = 0u;
    tick();                                 /* → 2, timer 50 */
    CHECKV(C8(0xE9F6u), 2);
    ticks(49u);
    CHECKV(C8(0xE9F5u), 1);
    tick();                                 /* the 50th decrement: learned "disagree" */
    CHECKV(C8(0xE9F5u), 2);
    CHECKV(C8(0xE9F6u), 0);
    CHECKV(kl15_e9ff(), 4);                 /* (off since the code went 0) */
    CHECKV(kl15_wit()->agree_set_n, 1);
    /* 0x112 lost now: CAN invalid, agreement 2 → quality 2; the voltage
     * path drives the state (on) but D5F3 = 2 → D5F2 = 0 (893E8 = 0) */
    s.ign = 0xFFu;
    ticks(8u);                              /* the 50 ms receive timeout */
    CHECKV(kl15_ea01(), 2);
    CHECKV(kl15_e9ff(), 2);
    CHECKV(C8(0xD5F3u), 2);
    CHECKV(C8(0xD5F2u), 0);

    /* a torn complement: the stock resets; ours latches the terminal off */
    reset();
    ticks(3u);
    C8(0xEA00u) = 0x00u;                    /* +0x2C torn */
    tick();
    CHECKV(kl15_wit()->fatal_n, 1);
    CHECKV(kl15_e9ff(), 4);
    CHECKV(kl15_ea01(), 2);
    ticks(5u);
    CHECKV(kl15_e9ff(), 4);
    CHECKV(C8(0xD5F2u), 0);
}

/* release in D from power-on with the sims (the r03t (b) path) */
static void go_d(uint8_t ign)
{
    reset();
    s.brake = true;
    s.engine = 700u;
    s.ign = ign;
    s.lever = 1u;
    ticks(100u);
    s.lever = 4u;
    ticks(100u);
}

/* ── 2. key-off in D ────────────────────────────────────────────────────── */
static void test_keyoff_d(void)
{
    unsigned t_off = 0u, t_p = 0u;

    go_d(4u);
    CHECKV(C8(0xD6D6u), 5);
    CHECKV(outs(), 1);
    CHECKV(park_mon_faults(), 0);
    CHECKV(C8(0xD642u), 0);                 /* the CAN path */
    /* key off: code 0 */
    s.ign = 0u;
    for (unsigned k = 1u; k <= 60u; k++) {
        tick();
        if (t_off == 0u && kl15_e9ff() == 4u) {
            t_off = k;
        }
        if (t_p == 0u && C8(0xD6D6u) == 8u) {
            t_p = k;
            CHECKV(C8(0xD6DEu), 8);
            CHECKV(C8(0xD6DFu), 0x50);     /* 0xC286C: key-off auto-P */
        }
        CHECKV(park_mon_faults(), 0);
    }
    printf("  key-off in D: E9FF 4 at pass %u, D6D6 8 at pass %u, outputs %d\n",
           t_off, t_p, outs());
    CHECKV(t_off, 2);                       /* 2 → 3 → 4 */
    CHECKV(t_p, 2);                         /* the same pass: D5F2 0 → 8 / 0x50 */
    CHECKV(C8(0xD6D6u), 8);
    CHECKV(outs(), 0);
    CHECKV(PCELL_AE29, 1);
    CHECKV(park_mon_faults(), 0);
    /* key on again, lever still D: P is held (the lever latch D5FB) */
    s.ign = 4u;
    ticks(100u);
    CHECKV(kl15_e9ff(), 2);
    CHECKV(C8(0xD6D6u), 8);
    CHECKV(outs(), 0);
    /* lever P, then D: released again, no monitor fault */
    s.lever = 1u;
    ticks(50u);
    s.lever = 4u;
    ticks(50u);
    CHECKV(C8(0xD6D6u), 5);
    CHECKV(outs(), 1);
    CHECKV(park_mon_faults(), 0);
    CHECKV(rng_chain_model()->recycle_n, 1);
    /* key-off in N and in R: the same */
    s.lever = 3u;
    ticks(50u);
    CHECKV(C8(0xD6D6u), 6);
    s.ign = 2u;
    tick();
    CHECKV(C8(0xD6D6u), 6);                 /* E9FF 3: still on */
    tick();
    CHECKV(C8(0xD6D6u), 8);
    CHECKV(C8(0xD6DFu), 0x50);              /* on the transition pass */
    CHECKV(C8(0xD6D9u), 0);                 /* the D6D6 reason: 0 (0xC70CE takes D6DE) */
    ticks(5u);
    CHECKV(C8(0xD6D6u), 8);
    CHECKV(C8(0xD6DFu), 0);                 /* E545 = 8 since: no 0xC286C any more */
    CHECKV(outs(), 0);
    CHECKV(park_mon_faults(), 0);
}

/* ── 3. 0x112 lost → the voltage path; supply < 4000 mV → auto-P ─────── */
static void test_voltage_path(void)
{
    unsigned t_p = 0u;

    go_d(4u);
    s.ign = 0xFFu;                          /* 0x112 lost */
    ticks(50u);
    CHECKV(kl15_e9ff(), 2);
    CHECKV(kl15_ea01(), 1);                 /* agreement still 1 → quality 1 */
    CHECKV(C8(0xD6D6u), 5);
    CHECKV(outs(), 1);
    SUPPLY = 3500u;
    for (unsigned k = 1u; k <= 30u; k++) {
        tick();
        if (t_p == 0u && C8(0xD6D6u) == 8u) {
            t_p = k;
            CHECKV(C8(0xD6DEu), 8);
            CHECKV(C8(0xD6DFu), 0x50);
        }
    }
    printf("  supply < 4000 mV in D (0x112 absent): D6D6 8 at pass %u\n", t_p);
    CHECKV(t_p, 12);                        /* 10 passes off-delay, 3, 4 */
    CHECKV(outs(), 0);
    CHECKV(park_mon_faults(), 0);

    /* the bench itself (no 0x112, voltage on) never leaves on */
    go_d(0xFFu);
    CHECKV(C8(0xD6D6u), 5);
    CHECKV(C8(0xD642u), 1);
    CHECKV(outs(), 1);
}

/* ── 4. 0x89CF2, the fault-reaction park request ───────────────────────── */
static void test_aux(void)
{
    park_aux_in_t ai = { 0u, 0x20u, 0u, 0u };

    /* the law itself (0xB9B52..0xB9BCE) */
    CHECKV(park_law_aux(&ai), 0);
    ai.c3c = 1u;
    CHECKV(park_law_aux(&ai), 1);
    CHECK(PCELL_AUX[0] == 1u && PCELL_AUX[2] == 1u && PCELL_AUX[3] == 1u && PCELL_AUX[1] == 0u);
    ai.c3c = 0u;
    ai.c42 = 6u;
    CHECKV(park_law_aux(&ai), 1);
    ai.c42 = 8u;
    CHECKV(park_law_aux(&ai), 1);
    ai.c42 = 0x1Au;                         /* 0x1A feeds +0xB3, not +0xB4 */
    CHECKV(park_law_aux(&ai), 0);
    ai.c42 = 0x20u;
    ai.ced = 1u;
    CHECKV(park_law_aux(&ai), 1);
    ai.ced = 7u;
    CHECKV(park_law_aux(&ai), 1);
    ai.ced = 2u;
    CHECKV(park_law_aux(&ai), 0);
    ai.ced = 0u;
    ai.b7be = 1u;
    CHECKV(park_law_aux(&ai), 1);

    /* in the loop: D, then OS mode 3 (the only live input) */
    go_d(0xFFu);
    CHECKV(PCELL_AUX[2], 0);                /* the bench: 0 */
    CHECKV(outs(), 1);
    s.os_mode = 3u;
    tick();
    CHECKV(PCELL_AUX[2], 1);
    CHECKV(PCELL_AE29, 1);                  /* aux && no power flow (E69B 0) */
    CHECKV(park_law_lock(), 1);
    CHECKV(outs(), 0);
    CHECKV(C8(0xD6D6u), 5);                 /* the range itself stays D */
    CHECKV(park_mon_faults(), 0);
    ticks(20u);
    CHECKV(outs(), 0);
    s.os_mode = 1u;
    tick();
    CHECKV(PCELL_AUX[2], 0);
    CHECKV(outs(), 1);
    CHECKV(park_mon_faults(), 0);
}

/* ── 5. the DSM operating cycle: a terminal re-cycle drops U1267 ────────── */
static void test_recycle(void)
{
    const rng_chain_model_t *m = rng_chain_model();

    reset();
    s.ign = 4u;
    ticks(400u);                            /* no lever: U1267 confirms */
    CHECKV(m->g109, 1);
    CHECKV(C8(0xD664u), 1);
    CHECKV(m->dsm_cyc, 2);
    s.ign = 0u;                             /* off */
    ticks(10u);
    CHECKV(m->dsm_cyc, 3);
    CHECKV(m->g109, 1);                     /* nothing dropped yet */
    s.ign = 4u;                             /* on again: the re-cycle (E9FF 4 → 1) */
    tick();
    CHECKV(m->recycle_n, 1);
    CHECKV(m->dsm_cyc, 2);
    CHECKV(m->g109, 0);
    CHECKV(C8(0xD664u), 0);
    CHECK(m->fm109.d714 <= 1u);             /* zeroed; a failed verdict may restart it at 1 */
    CHECKV(m->fm109.st & 0x2000u, 0);       /* status &= 0xDBBB */
    ticks(200u);                            /* still no lever: re-confirms */
    CHECKV(m->g109, 1);
    CHECKV(m->recycle_n, 1);

    /* the bench: never a re-cycle */
    reset();
    ticks(400u);
    CHECKV(m->recycle_n, 0);
    CHECKV(m->dsm_cyc, 2);
}

/* ── 6. the bench tool's keyoff-check sequence (tools/bench/tcu_bench.py
 *     cmd_keyoff_check, --settle 1.0, with the brake + engine 700 sims;
 *     frames from power-on): E9FF, quality and range per step ─────────── */
static void test_tool_sequence(void)
{
    static const struct { uint8_t lever, ign, ff, q, rng; } st[8] = {
        { 1u, 4u,    2u, 0u, 8u }, { 4u, 4u,    2u, 0u, 5u }, { 4u, 0u,    4u, 0u, 8u },
        { 4u, 4u,    2u, 0u, 8u }, { 1u, 4u,    2u, 0u, 8u }, { 4u, 4u,    2u, 0u, 5u },
        /* 0x112 lost after the learned disagreement: quality 2 → D5F3 2,
         * but the engine runs (893E8 >= 1) → D5F2 = E9FF in 1..3 = 1 (0xA8F5C):
         * D is kept */
        { 4u, 0xFFu, 2u, 2u, 5u }, { 1u, 0xFFu, 2u, 2u, 8u },
    };

    reset();
    s.brake = true;
    s.engine = 700u;
    s.ign = 4u;
    s.lever = 1u;
    ticks(300u);                            /* boot + the tool's start-up */
    printf("  keyoff-check (sims):");
    for (unsigned k = 0u; k < 8u; k++) {
        s.lever = st[k].lever;
        s.ign = st[k].ign;
        ticks(100u);
        printf(" [%u] E9FF %u q %u agree %u rng %u out %d", k, kl15_e9ff(), kl15_ea01(),
               C8(0xE9F5u), C8(0xD6D6u), outs());
        CHECKV(kl15_e9ff(), st[k].ff);
        CHECKV(kl15_ea01(), st[k].q);
        CHECKV(C8(0xD6D6u), st[k].rng);
        CHECKV(outs(), st[k].rng != 8u ? 1 : 0);
        CHECKV(park_mon_faults(), 0);
    }
    printf("\n");
}

int main(void)
{
    test_kl15();
    test_keyoff_d();
    test_voltage_path();
    test_aux();
    test_recycle();
    test_tool_sequence();
    if (fails == 0) {
        printf("keyoff: all checks passed\n");
    }
    return fails == 0 ? 0 : 1;
}

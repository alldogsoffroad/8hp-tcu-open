/* Unit test: G5b — THE OS-MODE REQUEST PATH (2026-11-19,
 * ~/tcu_sweep/g5b_port.md; safety/os_sel.c). The stock chain on our cells:
 * the selector 0x1418E2 (before kl15 0x140B80), the echo 0x68EA8 (10 ms
 * head), slot 26's flags, the hand-off 0x68EFE (10 ms tail) → 0x93B5E →
 * bsw/sup/sup_mode.c's 0x6F0B4 / 0x66E06 in the 5 ms batch (1 ms after the
 * 10 ms task, as app/main.c's phases), with the real CAN receive (0x108,
 * 0x112) and the real terminal-15 machine.
 *
 *  1. the law of 0x1418E2 exhaustively against a reference written from the
 *     decode's table (os_mode_selection_decode §1.4 / §2.2), incl. the
 *     engine's cmp/hs on the sign-extended word and the supply's strict
 *     6000 / 17000;
 *  2. the echo (mode >= 4 → 1; only on a change) and the hand-off (only
 *     with 0x848CF != 0 and a request <= 3);
 *  3. plain boot (no 0x108): mode 1 for 60 s; the slot-26 mode-1 flags;
 *  4. sims from power-on: mode 2 at the first mode-1 pass's batch, LATCHED
 *     after the 0x108 frames stop (and with 0x108 valid at 0 rpm);
 *  5. the sim started after boot: 140 rpm never; 150 rpm → mode 2 within
 *     15 ms of the first valid frame;
 *  6. one stray 0x108 frame latches mode 2;
 *  7. key-off: 0x112 code 0 → E9FF 3 → 4 → mode 1 (and stays 1 while
 *     E9FF 4); key-on → E9FF 1 → 2 → mode 2 again (the engine still valid);
 *  8. the turbine term: >= 100 rpm with the sensor ok → 2; 99 → no; a
 *     faulted sensor → no;
 *  9. the supply: 6000 / 17000 refuse, 6001 / 16999 accept. */
#include "safety/os_sel.h"
#include "safety/kl15.h"
#include "bsw/sup/sup_mode.h"
#include "bsw/sup/sup_cells.h"
#include "io/vehicle/vehicle.h"
#include "io/vehicle/vehicle_inject.h"
#include "tests/unit/stub_plat.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

#define U8(a)   (*(volatile uint8_t *)(uintptr_t)(a))
#define SUPPLY_KL15 (*(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL)


/* the bench: 0x108 engine (0xFFFF = no frame), 0x112 code (0xFF = none),
 * the turbine by value, the supply 0x94F86 */
typedef struct {
    uint16_t engine;
    uint8_t  ign;
    uint16_t tur;
    uint8_t  tur_ok;
    uint16_t sup;
    unsigned frame;         /* 10 ms frames since power-on */
    unsigned m1_at;         /* the frame the ladder's 'U' path sets module 2 (mode 1 next) */
    unsigned m1, m2;        /* the frames mode 1 / mode 2 were applied */
} bench_t;
static bench_t b;

static void reset(void)
{
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    vehicle_init();
    kl15_init();
    sup_mode_init();
    os_sel_init();
    SUPPLY_KL15 = 13206u;
    memset(&b, 0, sizeof b);
    b.engine = 0xFFFFu;
    b.ign = 0xFFu;
    b.tur_ok = 1u;
    b.sup = 13360u;
    b.m1_at = 60u;          /* 'U' + module 7 → module 2 0x0F (≈ +0.6 s) */
}

/* the 5 ms batch's mode part (bsw/sup/sup_batch.c): 0x6F0B4, then the apply
 * at the 5 ms tail in modes != 0 (0x67A34) / at the 10 ms tail in mode 0
 * (0x67A42) */
static void batch(bool due10)
{
    const uint8_t mode5 = sup_mode();

    sup_mode_request();
    if (mode5 != 0u) {
        sup_mode_apply();
    }
    if (due10 && sup_mode() == 0u) {
        sup_mode_apply();
    }
}

/* one 10 ms frame: app/main.c's task_10ms at ms 1, the batch at ms 2 and 7 */
static void frame(void)
{
    const uint8_t os0 = sup_mode();
    uint8_t d[8];

    if (b.frame == b.m1_at) {
        CELL_MOD2 = 0x0Fu;  /* module 2 done (bsw/sup/sup_mode.c 0x7DB70): mode 1 requested */
    }
    os_sel_echo_10ms(os0);
    if (b.engine != 0xFFFFu) {
        vehicle_inject_msg_engine(b.engine, 0x1FFFu);
    }
    if (b.ign != 0xFFu) {
        vehicle_inject_msg_ign(b.ign);
    }
    vehicle_update_10ms();
    {
        const vehicle_in_t *v = vehicle_in();
        const os_sel_in_t si = {
            .eng = (uint16_t)v->engine_speed,
            .eng_valid = (v->q_engine_speed == Q_OK) ? 1u : 0u,
            .tur_ok = b.tur_ok,
            .tur = b.tur,
            .sup = b.sup,
            .e9ff = kl15_e9ff(),
            .pad = 0u,
        };
        os_sel_tick_10ms(os0, &si);
    }
    kl15_tick_10ms();
    os_sel_flags_10ms(os0);
    os_sel_handoff_10ms(os0);
    batch(true);
    batch(false);
    if (sup_mode() == 1u && b.m1 == 0u) {
        b.m1 = b.frame;
    }
    if (sup_mode() == 2u && b.m2 == 0u) {
        b.m2 = b.frame;
    }
    b.frame++;
}

static void frames(unsigned n)
{
    for (unsigned k = 0u; k < n; k++) {
        frame();
    }
}

static void flags_are(uint8_t mode)
{
    CHECKV(U8(OS_FLG_C3D), mode == 1u);
    CHECKV(U8(OS_FLG_C55), mode == 1u);
    CHECKV(U8(OS_FLG_C3E), mode == 2u);
    CHECKV(U8(OS_FLG_C53), mode == 2u);
    CHECKV(U8(OS_FLG_C3C), mode == 3u);
    CHECKV(U8(OS_FLG_C54), mode == 3u);
}

/* the decode's table (os_mode_selection_decode §2.2), written independently */
static uint8_t ref_law(uint8_t cur, uint8_t e9ff, uint8_t f, uint16_t eng, uint8_t est, uint16_t tur,
                       uint8_t tst, uint16_t sup)
{
    const bool spd = ((eng >= 150u || eng >= 0x8000u) && est == 0u) || (tur >= 100u && tst == 0u);
    switch (cur) {
    case 1:
        if (e9ff != 2u) {
            return 1u;
        }
        if (f == 0u) {
            return 3u;
        }
        return (spd && sup > 6000u && sup < 17000u) ? 2u : 1u;
    case 2:
        if (f == 0u && e9ff == 2u) {
            return 3u;
        }
        return (e9ff == 4u) ? 1u : 2u;
    case 3:
        return (e9ff == 4u || f == 1u) ? 1u : 3u;
    default:
        return 1u;
    }
}

static void t_law(void)
{
    static const uint16_t E[] = { 0u, 149u, 150u, 151u, 700u, 0x7FFFu, 0x8000u, 0xFFFEu };
    static const uint16_t T[] = { 0u, 99u, 100u, 2000u, 0xFFFFu };
    static const uint16_t S[] = { 0u, 6000u, 6001u, 13360u, 16999u, 17000u, 17001u };
    unsigned n = 0u, bad = 0u;
    for (uint8_t cur = 0u; cur < 6u; cur++)
    for (uint8_t e9 = 0u; e9 < 6u; e9++)
    for (uint8_t f = 0u; f < 3u; f++)
    for (unsigned ie = 0u; ie < sizeof E / sizeof E[0]; ie++)
    for (uint8_t est = 0u; est < 3u; est += 2u)
    for (unsigned it = 0u; it < sizeof T / sizeof T[0]; it++)
    for (uint8_t tst = 0u; tst < 2u; tst++)
    for (unsigned is = 0u; is < sizeof S / sizeof S[0]; is++) {
        uint8_t spd;
        const uint8_t a = os_sel_law(cur, e9, f, E[ie], est, T[it], tst, S[is], &spd);
        const uint8_t r = ref_law(cur, e9, f, E[ie], est, T[it], tst, S[is]);
        n++;
        if (a != r) {
            if (bad < 5u) {
                printf("  law: cur %u e9ff %u f %u eng %u/%u tur %u/%u sup %u: %u (ref %u)\n", cur, e9, f, E[ie],
                       est, T[it], tst, S[is], a, r);
            }
            bad++;
        }
    }
    printf("1. the law of 0x1418E2: %u combinations, %u differ from the decode's table\n", n, bad);
    CHECKV(bad, 0);
}

static void t_echo_handoff(void)
{
    reset();
    CELL_OS_MODE_BOOT = 1u;
    os_sel_echo_10ms(1u);
    CHECKV(os_sel_state()->app, 1);
    CHECKV(U8(OS_SEL_ECHO_LAST), 1);
    CELL_OS_MODE_BOOT = 5u;             /* >= 4 → 1: no change against the last 1 */
    *(volatile uint8_t *)(uintptr_t)(OS_SEL_OBJ + 8u) = 9u;
    os_sel_echo_10ms(1u);
    CHECKV(os_sel_state()->app, 9);     /* not rewritten (1 == last) */
    CELL_OS_MODE_BOOT = 2u;
    os_sel_echo_10ms(2u);
    CHECKV(os_sel_state()->app, 2);
    /* +0x18 t2 = TCNTA at the first echo of 2 (the host's stub clock may read 0) */
    os_sel_echo_10ms(0u);               /* mode 0: not the modes 1..3 objects */
    CHECKV(U8(OS_SEL_ECHO_LAST), 2);
    /* the hand-off */
    *(volatile uint8_t *)(uintptr_t)(OS_SEL_OBJ + 7u) = 2u;
    CELL_OS_MODE_BOOT = 0u;
    CELL_OS_REQ_APP = 0u;
    os_sel_handoff_10ms(1u);
    CHECKV(CELL_OS_REQ_APP, 0);         /* 0x848CF == 0 */
    CELL_OS_MODE_BOOT = 1u;
    os_sel_handoff_10ms(1u);
    CHECKV(CELL_OS_REQ_APP, 2);
    *(volatile uint8_t *)(uintptr_t)(OS_SEL_OBJ + 7u) = 4u;
    os_sel_handoff_10ms(1u);
    CHECKV(CELL_OS_REQ_APP, 2);         /* > 3: not handed over */
    printf("2. the echo 0x68EA8 and the hand-off 0x68EFE as the bytes\n");
}

static void t_plain(void)
{
    reset();
    frames(6000u);                      /* 60 s, no 0x108 */
    printf("3. plain boot: mode 1 at F%u, mode 2 %s; request %u, 0x93B5E %u, selector passes %u (2: %u)\n", b.m1,
           b.m2 ? "ENTERED" : "never", os_sel_state()->req, CELL_OS_REQ_APP, (unsigned)os_sel_state()->n,
           (unsigned)os_sel_state()->n2);
    CHECKV(b.m1, b.m1_at);
    CHECKV(b.m2, 0);
    CHECKV(sup_mode(), 1);
    CHECKV(os_sel_state()->req, 1);
    CHECKV(CELL_OS_REQ_APP, 1);
    CHECKV(os_sel_state()->n2, 0);
    CHECKV(kl15_e9ff(), 2);
    flags_are(1u);
}

static void t_sims(void)
{
    reset();
    b.engine = 700u;
    frames(400u);
    printf("4. sims from power-on: mode 1 at F%u, mode 2 at F%u", b.m1, b.m2);
    CHECKV(b.m2, b.m1 + 1u);            /* the first mode-1 task requests, its batch applies */
    CHECKV(sup_mode(), 2);
    flags_are(2u);
    b.engine = 0xFFFFu;                 /* the tool exits: 0x108 stops */
    frames(3000u);
    CHECKV(sup_mode(), 2);              /* latched */
    CHECKV(os_sel_state()->eng_st, 2);
    b.engine = 0u;                      /* 0x108 valid at 0 rpm (an engine stall) */
    frames(300u);
    CHECKV(sup_mode(), 2);
    printf(", still 2 after the frames stopped (30 s) and at 0 rpm; request changes %u\n",
           (unsigned)os_sel_state()->chg);
    flags_are(2u);
}

static void t_after_boot(void)
{
    unsigned first;
    reset();
    frames(200u);
    CHECKV(sup_mode(), 1);
    b.engine = 140u;
    frames(300u);
    CHECKV(sup_mode(), 1);              /* below 150 */
    b.engine = 150u;
    first = b.frame;
    frames(5u);
    printf("5. sim after boot: 140 rpm for 3 s → mode 1; 150 rpm from F%u → mode 2 at F%u (the batch 1 ms after "
           "the requesting 10 ms pass)\n", first, b.m2);
    CHECK(b.m2 != 0u && b.m2 >= first && (b.m2 - first) * 10u + 1u <= 15u);
    CHECKV(sup_mode(), 2);
}

static void t_stray(void)
{
    unsigned at;
    reset();
    frames(200u);
    b.engine = 700u;
    at = b.frame;
    frame();
    b.engine = 0xFFFFu;
    frames(2000u);
    printf("6. one stray 0x108 frame at F%u: mode 2 at F%u, mode %u 20 s later\n", at, b.m2, sup_mode());
    CHECKV(b.m2, at);
    CHECKV(sup_mode(), 2);
}

static void t_keyoff(void)
{
    unsigned off, on, back1 = 0u, back2 = 0u;
    reset();
    b.engine = 700u;
    b.ign = 4u;                         /* 0x112 ignition on */
    frames(200u);
    CHECKV(sup_mode(), 2);
    CHECKV(kl15_e9ff(), 2);
    b.ign = 0u;                         /* key off (the keyoff-check path) */
    off = b.frame;
    for (unsigned k = 0u; k < 300u; k++) {
        frame();
        if (back1 == 0u && sup_mode() == 1u) {
            back1 = b.frame - 1u;
        }
    }
    CHECKV(kl15_e9ff(), 4);
    CHECKV(sup_mode(), 1);              /* and it stays 1 while E9FF 4 (3 s) */
    b.ign = 4u;                         /* key on, the engine still valid */
    on = b.frame;
    for (unsigned k = 0u; k < 100u; k++) {
        frame();
        if (back2 == 0u && sup_mode() == 2u) {
            back2 = b.frame - 1u;
        }
    }
    printf("7. key-off: code 0 at F%u → mode 1 at F%u (%u passes); key-on at F%u → mode 2 at F%u\n", off, back1,
           back1 - off, on, back2);
    CHECK(back1 != 0u && back1 - off <= 3u);
    CHECK(back2 != 0u && back2 - on <= 3u);
    CHECKV(sup_mode(), 2);
}

static void t_turbine(void)
{
    reset();
    b.tur = 99u;
    frames(300u);
    CHECKV(sup_mode(), 1);
    b.tur = 150u;
    b.tur_ok = 0u;                      /* a faulted turbine sensor (E7EC != 0) */
    frames(100u);
    CHECKV(sup_mode(), 1);
    b.tur_ok = 1u;
    b.tur = 100u;
    frames(3u);
    printf("8. the turbine term: 99 rpm / a faulted sensor → mode 1; 100 rpm → mode 2 at F%u\n", b.m2);
    CHECKV(sup_mode(), 2);
}

static void t_supply(void)
{
    static const struct { uint16_t sup; uint8_t want; } C[] = {
        { 6000u, 1u }, { 6001u, 2u }, { 16999u, 2u }, { 17000u, 1u }, { 5000u, 1u }, { 18000u, 1u },
    };
    for (unsigned k = 0u; k < sizeof C / sizeof C[0]; k++) {
        reset();
        b.engine = 700u;
        b.sup = C[k].sup;
        frames(200u);
        printf("9. supply %5u mV: mode %u (want %u)\n", C[k].sup, sup_mode(), C[k].want);
        CHECKV(sup_mode(), C[k].want);
    }
}

int main(void)
{
    t_law();
    t_echo_handoff();
    t_plain();
    t_sims();
    t_after_boot();
    t_stray();
    t_keyoff();
    t_turbine();
    t_supply();
    if (fails == 0) {
        printf("test_os_sel: ok\n");
    }
    return fails != 0;
}

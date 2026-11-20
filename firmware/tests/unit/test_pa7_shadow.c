/* test_pa7_shadow.c — THE READ-ONLY PA7 SHADOW (app/pa7_shadow.c, the
 * gathers app/pa7_hook.c; ~/tcu_sweep/pa7_decode.md, the port
 * ~/tcu_sweep/r04d_recorder_pa7_shadow.md).
 *
 * The firmware's order: main.c's 10 ms task (phase 1: vehicle → kl15 → the
 * range chain → pa7_hook_range → park) and the supervisor batch (phase 2:
 * the pin-cut entry → app_pin_cut_hook; 5 ms in OS mode 0, every second
 * pass in mode 1), the live cells at their stock addresses in the host
 * window, the range chain the real app/rng_chain.c.
 *
 *   (1) plain P boot (no lever frames, no sims; AN7 0 until our Timer G
 *       starts at 500 ms, then the bench's 19 981 mV; supply 0x94F86 /
 *       0x94FEC plausible from 600 ms; the mode-1 switch at 670 ms): the
 *       stock driving dump — E567 02, D712 01, PA7 1, 0x9095A 0, 0x94122 01,
 *       0x93B2A ≈ 19 981, 0x93B2C = 0x93B2E = 0x0004 (fault 4: the line
 *       reads high while expected low), 0x9417B 00, 0x9417C 0x18, 0x94766
 *       1400 (the dump's 1398 = the same reload two 5 ms countdowns later,
 *       read mid-tick), 0x94768 0; the law's first pass is the 2nd pin-cut
 *       pass (0x9095B);
 *   (2) park-check WITHOUT sims (13 steps, TCU up 3 s before): R / D (the
 *       range held at 8, the lever asking R / D) → PA7 0, C9b7 1; P, N, no
 *       lever, invalid lever → PA7 1, C9b7 0 — the decode's table rows
 *       "P held but lever R / D" and "P, lever P / N / none";
 *   (3) park-check WITH the brake + engine 700 sims from power-on: R 0/1,
 *       N 1/0, D 0/1, P 1/0; the lever-lost steps (the range held at D)
 *       0/1;
 *   (4) the range part and the law on direct inputs: an invalid range
 *       (D6DA 2, or D6D6 outside 5..8) → E567 3 → state 3 → PA7 0, C9b7 0;
 *       the park sensor confirming P (D5F8 1) → state 2 → PA7 1 with the
 *       supervisor's bit 1 clear, 0 / C9b7 1 with it set; D713 (D60A 1 and a
 *       safety reaction / no sensor P / supply < 5000) → state 0;
 *   (5) the AN7 checks: expected HIGH (nibble 4 = the Park contact, C9b7 0)
 *       → 50 ms blanking, then fault 8 below 2000 mV; expected LOW again →
 *       750 ms blanking, then fault 4 above 9000 mV; the ±1000 mV step
 *       limit holds one pass; the input-1 check: PA7 low ≥ 7 s with bit 3
 *       clear → fault 0x200;
 *   --cal    the PA7_CAL_* values against the stock image (exit 77: absent).
 * Built -DPA7_NEG_LAW (the law broken: state 0 / 2 read as 1) it must FAIL
 * (ctest WILL_FAIL). */
#include "app/pa7_shadow.h"
#include "app/rng_chain.h"
#include "safety/kl15.h"
#include "app/park.h"
#include "bsw/drv/drv_cells.h"
#include "io/vehicle/vehicle.h"
#include "io/vehicle/vehicle_inject.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

#define C8(a)   (*(volatile uint8_t  *)(uintptr_t)(0xFFF80000u | (uint32_t)(a)))
#define W16(a)  (*(volatile int16_t  *)(uintptr_t)(0xFFF80000u | (uint32_t)(a)))

static const volatile pa7s_t *P;
static uint32_t s_ms;           /* host ms since boot */

/* the bench around the TCU: the supervisor nibble, AN7, the supply */
static uint8_t s_nib;
static int16_t s_an7_on = 19981;    /* the bench's AN7 (dump 0x93A94 = 0x4E0F ≈ 19 983) */

static void live_env(void)
{
    const uint8_t mode = (uint8_t)((s_ms < 670u) ? 0u : 1u);
    C8(0x93B33u) = mode;
    C8(0x90958u) = s_nib;
    CELL_AN_MV(7) = (uint16_t)((s_ms >= 500u) ? s_an7_on : 0);
    if (s_ms >= 600u) {                     /* the ladder's publisher: supply plausible */
        W16(0x94F86u) = 12945;
        C8(0x94FECu) = 8u;
    } else {
        W16(0x94F86u) = 0;
        C8(0x94FECu) = 0u;
    }
}

static void boot(void)
{
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    s_ms = 0u;
    s_nib = 0u;
    CELL_TMP_BYTE = 67u;
    vehicle_init();
    park_init();
    rng_chain_init();
    kl15_init();
    *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL = 13206u;
    pa7_shadow_init();
    pa7_hook_range_init();                  /* main.c: after rng_chain_init */
    P = pa7_shadow_state();
}

void app_pin_cut_hook(void);

/* (6) the live-identity switch: the same run with the hooks left out */
static bool s_no_hooks;
static uint64_t s_hash;

static void hash_window(void)
{
    const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)0xFFF80000u;
    for (uint32_t a = 0u; a < 0x16000u; a++) {
        s_hash = (s_hash ^ p[a]) * 0x100000001B3ull;
    }
}

/* one ms of the firmware: phase 1 the 10 ms task, phase 2 the batch */
typedef void (*frames_fn)(void);
static void step_ms(frames_fn frames)
{
    const uint8_t mode = (uint8_t)((s_ms < 670u) ? 0u : 1u);
    s_ms++;
    live_env();
    if (s_ms % 10u == 1u) {                 /* task_10ms */
        if (frames != NULL) {
            frames();
        }
        vehicle_update_10ms();
        kl15_tick_10ms();
        rng_chain_tick_10ms(mode, 1u, 1u);
        if (!s_no_hooks) {
            pa7_hook_range(mode);
        }
        park_tick_10ms();
    }
    if (s_ms % 5u == 2u) {                  /* task_sup_batch: the pin-cut entry */
        if ((mode == 0u || (s_ms % 10u) == 7u) && !s_no_hooks) {
            app_pin_cut_hook();
        }
    }
    if (s_ms % 10u == 9u) {
        hash_window();                      /* (6) the whole live cell window */
    }
}

/* ── (1) ─────────────────────────────────────────────────────────────── */
static void test_plain_boot(void)
{
    uint32_t first_pin = 0u;
    boot();
    while (s_ms < 5000u) {
        step_ms(NULL);
        if (first_pin == 0u && P->pin_n == 1u) {
            first_pin = s_ms;
            CHECKV(P->law_n, 0u);           /* 0x9095B still 0 at the first pass */
            CHECKV(P->pa7, 0u);             /* 0x90D12's init value */
        }
        if (P->pin_n == 2u) {
            CHECKV(P->law_n, 1u);           /* the 2nd pin-cut pass: 0x9095B = 0x0F */
        }
        if (s_ms == 300u) {                 /* mode 0: D712 frozen at the init one-shot's */
            CHECKV(P->d712, 1u);
            CHECKV(P->pa7, 1u);
            CHECKV(P->c9b7, 0u);
            CHECKV(P->rng_n, 1u);
        }
    }
    printf("(1) plain P boot, 5 s: E567 %02X D712 %02X D713 %02X | 94122 %02X PA7 %u 9095A %u 9095B %02X | "
           "93A94 %04X 93B2A %04X 93B2C %04X 93B2E %04X 9417B %02X 9417C %02X | 94766 %04X 94768 %04X "
           "94784 %04X 94834 %02X | law %u from pin pass 2 (first pin %u ms) rng %u\n",
           P->e567, P->d712, P->d713, P->s94122, P->pa7, P->c9b7, P->s9095b, P->an7_raw, P->an7_f,
           P->fail, P->tested, P->s9417b, P->s9417c, P->t_hi, P->t_lo, P->blank_w, P->blank_b,
           (unsigned)P->law_n, (unsigned)first_pin, (unsigned)P->rng_n);
    /* the dump (stockram_driving_20261104.bin) */
    CHECKV(P->e567, 2u);
    CHECKV(P->d712, 1u);
    CHECKV(P->d713, 0u);
    CHECKV(P->s94122, 1u);
    CHECKV(P->pa7, 1u);
    CHECKV(P->c9b7, 0u);
    CHECKV(P->s9095b, 0x0Fu);
    CHECKV(P->an7_f, 19981);
    CHECKV(P->fail, 0x0004u);
    CHECKV(P->tested, 0x0004u);
    CHECKV(P->s9417b, 0x00u);
    CHECKV(P->s9417c, 0x18u);
    /* the dump's 0x94766 = 0x0576 (1398) is a live read taken after the 5 ms
     * countdown and before the 10 ms task's pin-cut pass of the same tick
     * (two countdowns since the last 1400 reload); the shadow steps its copy
     * at the pass itself (the check reads it there, as the stock's does), so
     * between passes it rests at the reload value */
    CHECKV(P->t_hi, 0x0578u);
    CHECKV(P->t_lo, 0u);
    CHECK(P->f4_n > 0u && P->f8_n == 0u && P->f100_n == 0u && P->f200_n == 0u);
    CHECKV(P->tr_n, 1u);                    /* one change: 0 → PA7 1 at the first law pass */
}

/* ── (2) / (3) the park-check sequence (tools/bench park-check) ──────── */
static bool s_sims;
static int s_step;
static const struct { const char *d; uint8_t code; bool a, c, bad; uint8_t pa7_nosim, pa7_sim; } ST[13] = {
    { "no shifter frames",        1u, false, false, false, 1u, 1u },
    { "P",                        1u, true,  true,  false, 1u, 1u },
    { "R",                        2u, true,  true,  false, 0u, 0u },
    { "N",                        3u, true,  true,  false, 1u, 1u },
    { "D",                        4u, true,  true,  false, 0u, 0u },
    { "code 0 after D",           0u, true,  true,  false, 9u, 9u },   /* 9: printed, law cross-checked */
    { "P",                        1u, true,  true,  false, 1u, 1u },
    { "0x2EA bad CRC, 0x210 R",   2u, true,  true,  true,  0u, 0u },
    { "0x2EA missing, 0x210 N",   3u, false, true,  false, 1u, 1u },
    { "0x2EA D, 0x210 missing",   4u, true,  false, false, 0u, 0u },
    { "invalid code 7",           7u, true,  true,  false, 9u, 9u },
    { "both missing again",       4u, false, false, false, 9u, 9u },
    { "P",                        1u, true,  true,  false, 1u, 1u },
};

static void pc_frames(void)
{
    if (s_step >= 0 && (ST[s_step].a || ST[s_step].c)) {
        if (ST[s_step].a && !ST[s_step].bad) {   /* a bad-CRC frame is not delivered */
                    vehicle_inject_msg_lever(LEVER_SRC_PRIMARY, ST[s_step].code);
                }
        if (ST[s_step].c) {
            const uint8_t m[5] = { ST[s_step].code, 0, 0, 0, 0 };
            vehicle_inject_msg_lever(LEVER_SRC_FALLBACK, m[0]);
        }
    }
    if (s_sims) {
        vehicle_inject_msg_brake(BRAKE_ON);
        vehicle_inject_msg_engine(0x02BCu, 0x1FFFu);   /* 700 rpm */
    }
}

/* the law, recomputed from the cells the shadow read (a cross-check of the
 * shadow against the decode's table, not a second port) */
static uint8_t expect_pa7(uint8_t d6d6, uint8_t d672, uint8_t d673)
{
    if (d6d6 == 8u || d6d6 == 6u) {
        return (d673 == 2u || d672 == 8u || d672 == 6u) ? 1u : 0u;
    }
    return 0u;      /* 7, 5 → state 0; invalid → state 3 */
}

static void park_check(bool sims)
{
    boot();
    s_sims = sims;
    if (!s_no_hooks) printf("(%d) park-check %s (TCU %s):\n", sims ? 3 : 2, sims ? "WITH brake + engine 700 sims" : "no sims",
           sims ? "with the sims from power-on" : "up 3 s before");
    for (s_step = -1; s_step < 13; s_step++) {
        const uint32_t n = (s_step < 0) ? 3000u : 2000u;
        for (uint32_t t = 0u; t < n; t++) {
            step_ms((s_step >= 0 || sims) ? pc_frames : NULL);
        }
        if (s_step < 0 || s_no_hooks) {
            continue;
        }
        {
            const uint8_t want = sims ? ST[s_step].pa7_sim : ST[s_step].pa7_nosim;
            const uint8_t cross = expect_pa7(P->rin.d6d6, P->rin.d672, P->rin.d673);
            printf("    %-24s D6D6 %u lever %u src %u | A6D6E %3u E567 %u D712 %u | PA7 %u C9b7 %u%s\n",
                   ST[s_step].d, P->rin.d6d6, P->rin.d672, P->rin.d673, P->a6d6e, P->e567, P->d712,
                   P->pa7, P->c9b7, want == 9u ? "  (law cross-check only)" : "");
            if (want != 9u) {
                CHECKV(P->pa7, want);
            }
            CHECKV(P->pa7, cross);
            /* C9b7 is the complement of PA7 in the valid states 0..2 */
            CHECKV(P->c9b7, (uint8_t)(P->pa7 == 0u ? 1u : 0u));
        }
    }
    s_sims = false;
}

/* ── (4) ─────────────────────────────────────────────────────────────── */
static void law_pass(uint8_t nib, uint32_t passes)
{
    pa7_pin_in_t in = { 0u, 1u, nib, 8u, 0u, 19981, 12945 };
    for (uint32_t k = 0u; k < passes; k++) {
        in.t_us += 5000u;
        pa7_shadow_pin(&in);
    }
}

static void test_direct(void)
{
    pa7_rng_in_t r;
    memset(&r, 0, sizeof r);
    pa7_shadow_init();
    P = pa7_shadow_state();
    r.d6d6 = 8u;
    r.d673 = 2u;
    r.d65a = 12939;
    pa7_shadow_range(&r);
    law_pass(0u, 4u);
    CHECKV(P->d712, 1u);
    CHECKV(P->pa7, 1u);
    /* the invalid range */
    r.d6da = 2u;
    pa7_shadow_range(&r);
    law_pass(0u, 1u);
    CHECKV(P->e567, 3u);
    CHECKV(P->d712, 3u);
    CHECKV(P->pa7, 0u);
    CHECKV(P->c9b7, 0u);
    r.d6da = 0u;
    r.d6d6 = 0u;
    pa7_shadow_range(&r);
    law_pass(0u, 1u);
    CHECKV(P->a6d6e, 127u);
    CHECKV(P->d712, 3u);
    CHECKV(P->pa7, 0u);
    CHECKV(P->c9b7, 0u);
    /* the sensor confirms P → state 2: the supervisor's bit 1 decides */
    r.d6d6 = 8u;
    r.d5f8 = 1u;
    pa7_shadow_range(&r);
    law_pass(0x00u, 1u);
    CHECKV(P->d712, 2u);
    CHECKV(P->pa7, 1u);
    CHECKV(P->c9b7, 0u);
    law_pass(0x02u, 1u);
    CHECKV(P->pa7, 0u);
    CHECKV(P->c9b7, 1u);
    /* a safety reaction in P without the sensor → 0 */
    r.d5f8 = 0u;
    r.d5f9 = 1u;
    pa7_shadow_range(&r);
    law_pass(0u, 1u);
    CHECKV(P->d712, 0u);
    CHECKV(P->pa7, 0u);
    CHECKV(P->c9b7, 1u);
    /* D713: D60A 1 and the sensor not P → the override, state 0 */
    r.d5f9 = 0u;
    r.d60a = 1u;
    pa7_shadow_range(&r);
    CHECKV(P->d713, 1u);
    CHECKV(P->d712, 0u);
    /* ... sensor P, supply ≥ 5000 → no override → state 2 */
    r.d5f8 = 1u;
    pa7_shadow_range(&r);
    CHECKV(P->d713, 0u);
    CHECKV(P->d712, 2u);
    /* ... supply 4999 → override */
    r.d65a = 4999;
    pa7_shadow_range(&r);
    CHECKV(P->d713, 1u);
    printf("(4) direct: invalid range → state 3 → PA7 0 / C9b7 0; sensor P → state 2 (bit 1 decides); "
           "D5F9 → 0; D713 (D60A 1: sensor / supply) → 0 — checked\n");
}

/* ── (5) the AN7 checks ──────────────────────────────────────────────── */
static void test_line(void)
{
    pa7_rng_in_t r;
    pa7_pin_in_t in = { 0u, 1u, 0u, 8u, 0u, 19981, 12945 };
    memset(&r, 0, sizeof r);
    pa7_shadow_init();
    P = pa7_shadow_state();
    r.d6d6 = 8u;
    r.d673 = 2u;
    r.d65a = 12939;
    pa7_shadow_range(&r);                   /* state 1: PA7 1, C9b7 0 */
    for (int k = 0; k < 10; k++) {
        pa7_shadow_pin(&in);
    }
    CHECKV(P->fail, 4u);                    /* expected low, reads 19 981 */
    in.nib = 4u;                            /* the Park contact alone: expected HIGH */
    pa7_shadow_pin(&in);
    CHECKV(P->s9417c & 0x03u, 0x03u);
    CHECKV(P->blank_b, 10u);                /* 50 ms */
    for (int k = 0; k < 9; k++) {
        pa7_shadow_pin(&in);
        CHECKV(P->tested, 0u);              /* blanked */
    }
    pa7_shadow_pin(&in);
    pa7_shadow_pin(&in);
    CHECKV(P->tested, 8u);
    CHECKV(P->fail, 0u);
    in.an7 = 500;                           /* the line drops: one-pass hold, then fault 8 */
    pa7_shadow_pin(&in);
    CHECKV(P->an7_f, 19981);
    CHECK((P->s9417c & 0x04u) != 0u);
    pa7_shadow_pin(&in);
    CHECKV(P->an7_f, 500);
    pa7_shadow_pin(&in);
    CHECKV(P->fail, 8u);
    in.nib = 0u;                            /* expected LOW again: 750 ms blanking */
    pa7_shadow_pin(&in);
    CHECKV(P->blank_w, 150u);
    for (int k = 0; k < 150; k++) {
        pa7_shadow_pin(&in);
    }
    pa7_shadow_pin(&in);
    CHECKV(P->tested, 4u);
    CHECKV(P->fail, 0u);                    /* 500 mV < 9000 */
    /* the input-1 check: bit 1 set (bit 3 clear), the range R (state 0): PA7
     * low → after 1400 countdown steps (7 s) fault 0x200 */
    r.d672 = 7u;
    r.d673 = 0u;
    pa7_shadow_range(&r);
    in.nib = 0x02u;
    {
        uint32_t k = 0u;
        while (k < 2000u && (P->fail & 0x200u) == 0u) {
            pa7_shadow_pin(&in);
            k++;
        }
        printf("(5) line checks: fault 4 (high, expected low), blank 50 ms → fault 8 (low, expected high), "
               "blank 750 ms; input-1: fault 0x200 after %u passes (PA7 %u)\n", (unsigned)k, P->pa7);
        CHECK(k >= 1400u && k <= 1402u);
        CHECKV(P->pa7, 0u);
    }
}

/* ── --cal ───────────────────────────────────────────────────────────── */
static int test_cal(void)
{
    static unsigned char img[0x200000];
    FILE *f = fopen(TCU_STOCK_IMAGE, "rb");
    int bad = 0;
    if (f == NULL) {
        printf("SKIP no stock image %s\n", TCU_STOCK_IMAGE);
        return 77;
    }
    if (fread(img, 1u, sizeof img, f) != sizeof img) {
        fclose(f);
        printf("FAIL short image\n");
        return 1;
    }
    fclose(f);
#define U8(a)  ((unsigned)img[a])
#define U16(a) (((unsigned)img[a] << 8) | img[(a) + 1u])
#define U32(a) (((unsigned long)img[a] << 24) | ((unsigned long)img[(a) + 1u] << 16) | \
                ((unsigned long)img[(a) + 2u] << 8) | img[(a) + 3u])
    {
        static const struct { unsigned long pool, cal; unsigned val; int w; } C[] = {
            { 0x668B8u, 0x1919DFu, PA7_CAL_ENABLE, 1 }, { 0x668BCu, 0x1915BEu, PA7_CAL_THI, 2 },
            { 0x668C0u, 0x1915C0u, PA7_CAL_TLO, 2 },    { 0x66C70u, 0x191A18u, PA7_CAL_SUPSEL, 1 },
            { 0x66C74u, 0x191898u, PA7_CAL_BLANK_LO, 2 }, { 0x66C78u, 0x191A19u, PA7_CAL_BLANK_HI, 1 },
            { 0x66C7Cu, 0x19189Au, PA7_CAL_STEP, 2 },   { 0x66C80u, 0x191A1Au, PA7_CAL_SENSE, 1 },
            { 0x66C84u, 0x19189Cu, PA7_CAL_MASK, 2 },   { 0x66C88u, 0x19189Eu, PA7_CAL_SUPMIN, 2 },
            { 0x66C8Cu, 0x1918A0u, PA7_CAL_HIMV, 2 },   { 0x66C90u, 0x1918A2u, PA7_CAL_LOMV, 2 },
        };
        for (unsigned k = 0u; k < sizeof C / sizeof C[0]; k++) {
            const unsigned long p = U32(C[k].pool);
            const unsigned v = (C[k].w == 1) ? U8(C[k].cal) : U16(C[k].cal);
            if (p != C[k].cal || v != C[k].val) {
                printf("FAIL cal pool 0x%05lX -> 0x%06lX (expected 0x%06lX) value 0x%X (expected 0x%X)\n",
                       C[k].pool, p, C[k].cal, v, C[k].val);
                bad++;
            }
        }
    }
    if (U8(0x1918D3u) != PA7_CAL_AN) { printf("FAIL AN index\n"); bad++; }
    if (U32(0x4F024u) != 0x196072ul || U16(0x196072u) != PA7_CAL_D65A_MIN) { printf("FAIL D65A min\n"); bad++; }
    /* *0xFFF84AEC = 0x4F028 (the dump), its +0x10 → the cal byte */
    if (U32(0x4F028u + 0x10u) != 0x1960B8ul) { printf("FAIL E567 override pointer\n"); bad++; }
    if (U8(0x1960B8u) != PA7_CAL_E567_OVR) { printf("FAIL E567 override cal\n"); bad++; }
    printf("(cal) %d mismatches\n", bad);
    return bad == 0 ? 0 : 1;
}

/* ── (6) the live identity: the park-check with the sims (every range
 * 8 / 7 / 6 / 5, the shadow's PA7 / C9b7 toggling) run with and without the
 * PA7 hooks — the whole stock cell window 0xFFF80000..0xFFF96000 hashed
 * every 10 ms must be identical (the shadow's RAM is ours, outside it) ── */
static void test_identity(void)
{
    uint64_t with, without;
    s_hash = 0xCBF29CE484222325ull;
    s_no_hooks = false;
    park_check(true);
    with = s_hash;
    s_hash = 0xCBF29CE484222325ull;
    s_no_hooks = true;
    park_check(true);
    without = s_hash;
    s_no_hooks = false;
    printf("(6) live identity over the park-check with sims (26 s, 2600 window hashes): %016llx / %016llx %s\n",
           (unsigned long long)with, (unsigned long long)without, with == without ? "identical" : "DIFFERENT");
    CHECK(with == without);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        return test_cal();
    }
    test_plain_boot();
    park_check(false);
    park_check(true);
    test_direct();
    test_line();
    test_identity();
    printf("%s (%d failures)\n", fails == 0 ? "PASS" : "FAIL", fails);
    return fails == 0 ? 0 : 1;
}

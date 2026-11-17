/* Unit test: the park position sensor, READ-ONLY (2026-11-16,
 * ~/tcu_sweep/park_sensor_port.md) — the evaluator 0x8DF2E, monitor
 * 0x88EF0, copy 0x7B150 (bsw/drv/drv_pksens.c) and the safety layer's pair
 * object 0x141BA2 + group 98 0x141E06 (safety/park_pair.c), driven with the
 * bench's own millivolts (2026-11-16 zero-flash reads):
 *   mux 15 (sensor supply) 8980 mV; AN5 = AN6 = 6266 mV at rest (69.8 %,
 *   band 3, both inactive); a contact with a magnet 634 mV (7.1 %, band 1).
 *   AN5 = input 2 = contact A, AN6 = input 3 = contact B; position 1 = A
 *   alone, 0 = B alone.
 * Then the read-only property: the park demands, the arbiter target and
 * every park cell are bit-identical with the sensor in every state (and
 * changing every tick) to a run with no sensor at all.
 */
#include "bsw/drv/drv_pksens.h"
#include "safety/park_pair.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/sup/sup_cells.h"
#include "app/park.h"
#include "app/park_cells.h"
#include "app/range.h"
#include "app/rng_chain.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_park.h"
#include "io/vehicle/vehicle.h"
#include "io/sensors/sensors.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

/* ── the bench values (mV) ─────────────────────────────────────────────── */
#define MV_SUPPLY   8980u
#define MV_IDLE     6266u       /* 69.8 %: band 3, inactive */
#define MV_MAGNET    634u       /*  7.1 %: band 1, active */
#define MV_BAND2    3500u       /* 39.0 %: band 2 — electrical fault (0x20) */
#define MV_BAND4    7500u       /* 83.5 %: band 4 — electrical fault (0x10) */
#define MV_BAND0      50u       /*  0.6 %: band 0 — electrical fault (0x08) */

typedef struct {
    uint16_t an5, an6;
    uint8_t  nib;               /* the supervisor chip's digital view 0x90958 */
} sens_in_t;

static const sens_in_t S_IDLE   = { MV_IDLE,   MV_IDLE,   0x00u };
static const sens_in_t S_A      = { MV_MAGNET, MV_IDLE,   0x04u };
static const sens_in_t S_B      = { MV_IDLE,   MV_MAGNET, 0x08u };
static const sens_in_t S_BOTH   = { MV_MAGNET, MV_MAGNET, 0x0Cu };
static const sens_in_t S_AFLT   = { MV_BAND2,  MV_IDLE,   0x04u };
static const sens_in_t S_BFLT   = { MV_IDLE,   MV_BAND4,  0x00u };
static const sens_in_t S_BFLT0  = { MV_IDLE,   MV_BAND0,  0x08u };
static const sens_in_t S_AMISM  = { MV_MAGNET, MV_IDLE,   0x00u };   /* chip disagrees */

static void sens_set(const sens_in_t *s)
{
    CELL_AN_MV(5) = s->an5;
    CELL_AN_MV(6) = s->an6;
    CELL_PKS_MUX15 = MV_SUPPLY;
    CELL_SSLB0_NIB = s->nib;
}

/* one 5 ms mode-0 pass, the stock order: hub c (0x89320, [0x887D8], 0x88EF0),
 * the publishers, then 0x7AB48 (0x7B150) */
static void sens_pass(void)
{
    drv_pksens_ref_tick();
    drv_pksens_tick();
    drv_pksens_copy_tick();
}

/* 10 ms: two sensor passes, then the safety slice */
static void tick10(void)
{
    sens_pass();
    sens_pass();
    park_pair_tick_10ms();
}

static void sens_reset(void)
{
    for (uint32_t k = 0u; k < 6u; k++) {
        CELL_PKS_PAIR[k] = 0x55u;           /* garbage the init must fix */
        CELL_PKS_ST[k] = 0x55u;
    }
    drv_pksens_init();
    park_pair_init();
}

static park_pair_wit_t wit(void)
{
    park_pair_wit_t w;
    park_pair_witness(&w);
    return w;
}

/* ── 1. both inactive: the stock driving dump, cell for cell ─────────────── */
static void test_idle(void)
{
    static const uint8_t obj[] = {          /* +0x0C .. +0x20 (park_sensor_decode §5.2) */
        0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x0F, 0x0F,
        0x03, 0xFC, 0x02, 0xFD, 0x04, 0xFB, 0x00, 0x00, 0x00,
    };
    static const uint8_t ev[10] = { 0x00, 0x03, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
    park_pair_wit_t w;

    sens_reset();
    sens_set(&S_IDLE);
    /* start-up: two passes of mode 0, then normal */
    sens_pass();
    CHECKV(CELL_PKS_MODE, 0);
    CHECKV(CELL_PKS_START, 1);
    sens_pass();
    CHECKV(CELL_PKS_MODE, 0);
    sens_pass();
    CHECKV(CELL_PKS_MODE, 8);
    for (int i = 0; i < 20; i++) {
        tick10();
    }
    CHECKV(CELL_PKS_REF, 8980);
    CHECKV(CELL_PKS_RATIO[2], 6977);        /* 6266 x 10000 / 8980 */
    CHECKV(CELL_PKS_RATIO[3], 6977);
    {
        static const uint8_t band[6] = { 2, 2, 3, 3, 2, 2 };
        for (uint32_t k = 0u; k < 6u; k++) {
            CHECKV(CELL_PKS_BAND[k], band[k]);
            CHECKV(CELL_PKS_STATE[k], 0);
            CHECKV(CELL_PKS_STATE_C[k], 0xFF);
            CHECKV(CELL_PKS_PAIR[k], 0);
            CHECKV(CELL_PKS_PAIR_C[k], 0xFF);
            CHECKV(CELL_PKS_ST[k], 8);
            CHECKV(CELL_PKS_ST_C[k], 0xF7);
            CHECKV(CELL_PKS_ERR[k], 0);
            CHECKV(CELL_PKS_TESTED[k], (k == 2u || k == 3u) ? 0x3E : 0);
        }
    }
    for (uint32_t i = 0u; i < 10u; i++) {
        CHECKV(*(volatile uint8_t *)(uintptr_t)(0xFFF94160u + i), ev[i]);
    }
    CHECKV(CELL_PKS_STABLE, 1);
    CHECKV(CELL_PKS_CHGSEEN, 0);
    CHECK(CELL_PKS_MTESTED[0] == 0 && CELL_PKS_MTESTED[1] == 0 &&
          CELL_PKS_MTESTED[2] == 0x0F && CELL_PKS_MTESTED[3] == 0x0F);
    for (uint32_t i = 0u; i < 8u; i++) {
        CHECKV(CELL_PKS_CP_ST[i], 0x00);    /* 0x847F4..0x847FB */
        CHECKV(CELL_PKS_CP_FLT_C[i], 0xFF); /* 0x847FC..0x84803 */
    }
    for (uint32_t i = 0u; i < sizeof obj; i++) {
        CHECKV(CELL_PKP_U8(0x0Cu + i), obj[i]);
    }
    CHECKV(CELL_PKP_U32(0x00u), 0x00064A1C);
    CHECKV(CELL_PKP_U32(0x04u), 100);
    CHECKV(CELL_PKP_U32(0x08u), 0xFFFFFF9B);
    w = wit();
    CHECKV(w.source, PKP_SRC_P0850_M0);     /* P0850 m0: both inactive */
    CHECKV(w.failed, 1);
    CHECKV(w.tested, 1);
    CHECKV(w.calls, 20);
    CHECKV(w.fail_n, 20);
    CHECKV(w.fatal_n, 0);
    CHECKV(drv_pksens_f221_n(), 0);
}

/* ── 2. A alone → position 1; B alone → position 0 ──────────────────────── */
static void test_single(const sens_in_t *s, uint8_t pos, uint8_t act)
{
    park_pair_wit_t w0, w;

    sens_reset();
    sens_set(&S_IDLE);
    for (int i = 0; i < 10; i++) {
        tick10();
    }
    w0 = wit();
    sens_set(s);
    for (int i = 0; i < 10; i++) {
        tick10();
    }
    CHECKV(CELL_PKS_RATIO[act == 0x04u ? 2 : 3], 706);   /* 634 x 10000 / 8980 */
    CHECKV(CELL_PKS_BAND[act == 0x04u ? 2 : 3], 1);
    CHECKV(CELL_PKS_ACT, act);
    CHECKV(CELL_PKS_DIGM, act);
    CHECKV(CELL_PKS_MISM, 0);
    CHECKV(CELL_PKS_INVAL, 0);
    CHECKV(CELL_PKP_POS, pos);
    CHECKV(CELL_PKP_QUAL, 0);
    CHECKV(CELL_PKP_STATUS, 0);
    CHECKV(CELL_PKP_U8(0x19u), (uint8_t)~pos);
    w = wit();
    CHECKV(w.source, 0);
    CHECKV(w.failed, 0);
    CHECKV(w.tested, 1);
    CHECK(w.pass_n > w0.pass_n);
    CHECKV(w.fatal_n, 0);
    CHECKV(drv_pksens_f221_n(), 0);
}

/* ── 3. both active: position 2 for 99 ticks, then P0850 m1 ─────────────── */
static void test_both(void)
{
    park_pair_wit_t w;
    unsigned n2 = 0u;

    sens_reset();
    sens_set(&S_IDLE);
    for (int i = 0; i < 10; i++) {
        tick10();
    }
    sens_set(&S_BOTH);
    /* until the pair object sees A = B = 1 */
    for (int i = 0; i < 5 && !(CELL_PKP_U8(0x0Cu) == 1u && CELL_PKP_U8(0x10u) == 1u); i++) {
        tick10();
    }
    CHECK(CELL_PKP_U8(0x0Cu) == 1u && CELL_PKP_U8(0x10u) == 1u);
    CHECKV(CELL_PKP_POS, 2);                /* the first both-active pass: timer 99 */
    CHECKV(CELL_PKP_U32(0x04u), 99);
    n2 = 1u;
    while (CELL_PKP_POS == 2u && n2 < 200u) {
        tick10();
        if (CELL_PKP_POS == 2u) {
            n2++;
        }
    }
    CHECKV(n2, 99);                         /* reload 100: the 100th pass sets bit 4 */
    CHECKV(CELL_PKP_POS, 3);
    CHECKV(CELL_PKP_QUAL, 2);
    CHECKV(CELL_PKP_STATUS, 0x10);
    CHECKV(CELL_PKP_U32(0x04u), 0);
    CHECKV(CELL_PKP_U32(0x08u), 0xFFFFFFFF);
    w = wit();
    CHECKV(w.source, PKP_SRC_P0850_M1);
    CHECKV(w.failed, 1);
    CHECKV(w.tested, 1);
    /* stays latched while both stay active */
    for (int i = 0; i < 20; i++) {
        tick10();
    }
    CHECKV(CELL_PKP_POS, 3);
    CHECKV(CELL_PKP_STATUS, 0x10);
    /* back to A alone: the timer reloads, bit 4 clears, position 1 */
    sens_set(&S_A);
    for (int i = 0; i < 5; i++) {
        tick10();
    }
    CHECKV(CELL_PKP_POS, 1);
    CHECKV(CELL_PKP_STATUS, 0);
    CHECKV(CELL_PKP_U32(0x04u), 100);
    CHECKV(wit().fatal_n, 0);
}

/* ── 4. electrical band faults: P07B2 (A), P07B8 (B) ─────────────────────── */
static void test_fault(const sens_in_t *s, uint32_t k, uint8_t err, uint8_t code, uint8_t src)
{
    park_pair_wit_t w;

    sens_reset();
    sens_set(&S_IDLE);
    for (int i = 0; i < 10; i++) {
        tick10();
    }
    sens_set(s);
    for (int i = 0; i < 10; i++) {
        tick10();
    }
    CHECKV(CELL_PKS_ERR[k], err);
    CHECKV(CELL_PKS_TESTED[k], 0x3E);
    CHECKV(CELL_PKS_LATCH[k], 1);
    CHECKV(CELL_PKS_ST[k], 2);              /* latched faulty */
    CHECKV(CELL_PKS_ST_C[k], 0xFD);
    CHECKV(CELL_PKS_MERR[k], err);
    CHECKV(CELL_PKS_CP_FLT[k], code);       /* 0x847F8[k] */
    CHECKV(CELL_PKS_CP_FLT_C[k], (uint8_t)(0xFFu & ~code));
    CHECKV(CELL_PKP_POS, 3);
    CHECKV(CELL_PKP_QUAL, 2);
    CHECK((CELL_PKP_STATUS & (k == 2u ? 0x01u : 0x02u)) != 0u);
    w = wit();
    CHECKV(w.source, src);
    CHECKV(w.failed, 1);
    CHECKV(w.tested, 1);
    CHECKV(w.fatal_n, 0);
    /* healing: back to rest — the fault byte clears, the latch drops on the
     * next stable pass, the object returns to "both inactive" (P0850 m0) */
    sens_set(&S_IDLE);
    for (int i = 0; i < 10; i++) {
        tick10();
    }
    CHECKV(CELL_PKS_CP_FLT[k], 0);
    CHECKV(CELL_PKS_CP_FLT_C[k], 0xFF);
    CHECKV(CELL_PKS_LATCH[k], 0);
    CHECKV(CELL_PKS_ST[k], 8);
    CHECKV(CELL_PKP_STATUS, 0x04);
    CHECKV(wit().source, PKP_SRC_P0850_M0);
    CHECKV(wit().fatal_n, 0);
}

/* ── 5. the reference below 6000 mV (before the deferred mux chain starts):
 *       mode 2, ratio 0xFFFF / band 5, nothing tested, status 2 ─────────── */
static void test_ref_low(void)
{
    sens_reset();
    sens_set(&S_IDLE);
    CELL_PKS_MUX15 = 0u;
    for (int i = 0; i < 10; i++) {
        tick10();
    }
    CHECKV(CELL_PKS_MODE, 2);
    CHECKV(CELL_PKS_FLAGS, 3);
    CHECKV(CELL_PKS_RATIO[2], 0xFFFF);
    CHECKV(CELL_PKS_BAND[2], 5);
    CHECKV(CELL_PKS_INVAL, 0x0C);
    CHECKV(CELL_PKS_STABLE, 0);
    CHECKV(CELL_PKS_TESTED[2], 0);
    CHECKV(CELL_PKS_ST[2], 2);
    CHECKV(CELL_PKS_CP_FLT[2], 0);
    CHECKV(CELL_PKP_POS, 3);
    CHECKV(CELL_PKP_STATUS, 0x04);
    CHECKV(wit().source, PKP_SRC_P0850_M0);
    CHECKV(wit().fatal_n, 0);
}

/* ── 6. a torn complement: the stock's fatal handlers, counted here ─────── */
static void test_torn(void)
{
    sens_reset();
    sens_set(&S_IDLE);
    for (int i = 0; i < 5; i++) {
        tick10();
    }
    CELL_PKP_U8(0x19u) = 0x00u;             /* +0x18 = 3 vs ~0x00 */
    park_pair_tick_10ms();
    CHECKV(wit().fatal_n, 1);
    CHECKV(wit().fatal_site, 0x141DE8);
    CELL_PKS_PAIR_C[2] = 0x00u;             /* 0x94F88[2] = 0 vs ~0x00 */
    sens_pass();
    CHECKV(drv_pksens_f221_n(), 1);
    CHECKV(drv_pksens_f221_site(), 0xFFF94F8A);
}

/* ── 7. the read-only property: park outputs bit-identical ──────────────── */
static sensors_in_t s_z;
const sensors_in_t *sensors_in(void) { return &s_z; }

static uint8_t s_ctr[0x800];

static void send(can_bus_t bus, uint16_t id, uint8_t dlc, bool prot, const uint8_t *data)
{
    can_frame_t f = { .id = id, .dlc = dlc };
    memcpy(f.data, data, dlc);
    if (prot) {
        s_ctr[id] = (uint8_t)((s_ctr[id] + 1u) & 0x0Fu);
        f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(s_ctr[id] << 4));
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
    }
    vehicle_rx(bus, &f);
}

typedef struct {
    uint16_t rel, hold;
    uint8_t  tgt, tgt_c, arb[3], ae28, ae29, obj[4], out[5], out_c[5], rng, rngd;
    uint8_t  target, reason, state, request, lock, mon;
} park_snap_t;

#define N_STEPS 600u

static void park_snap(park_snap_t *p)
{
    const park_status_t *st = park_status();
    memset(p, 0, sizeof *p);
    p->rel = park_slot7_release();
    p->hold = park_slot11_hold();
    p->tgt = PCELL_TGT;
    p->tgt_c = PCELL_TGT_C;
    for (int i = 0; i < 3; i++) p->arb[i] = PCELL_ARB[i];
    p->ae28 = PCELL_AE28;
    p->ae29 = PCELL_AE29;
    for (int i = 0; i < 4; i++) p->obj[i] = PCELL_OBJ[i];
    for (int i = 0; i < 5; i++) { p->out[i] = PCELL_OUT[i]; p->out_c[i] = PCELL_OUT_C[i]; }
    p->rng = PCELL_RNG;
    p->rngd = PCELL_RNGD;
    p->target = (uint8_t)st->target;
    p->reason = st->reason;
    p->state = st->state;
    p->request = st->request;
    p->lock = st->lock;
    p->mon = st->mon_faults;
}

/* the lever / speed schedule: P, none, P, D, R, N, P, invalid, P, D (with
 * the brake + engine sims, the live chain releases on D / R / N); mode < 0 = no sensor at all, else the scenario index (6 =
 * cycling through every state each tick) */
static void park_run(int mode, park_snap_t *out)
{
    static const sens_in_t *const sc[7] = { &S_IDLE, &S_A, &S_B, &S_BOTH, &S_AFLT, &S_BFLT, &S_AMISM };

    memset(&s_z, 0, sizeof s_z);
    memset(s_ctr, 0, sizeof s_ctr);
    CELL_TMP_BYTE = 67u;
    vehicle_init();
    park_init();
    rng_chain_init();
    if (mode >= 0) {
        sens_reset();
    }
    for (uint32_t t = 0u; t < N_STEPS; t++) {
        const uint32_t seg = t / 60u;
        /* 2026-11-17: P before each release (the stock chain's D5FB latch
         * 0xA9144 needs a P after any loss of the lever source) */
        static const uint8_t lever[10] = { 1, 0, 1, 4, 2, 3, 1, 7, 1, 4 };
        const uint8_t code = lever[seg];
        uint8_t l[5] = { code, 0, 0, 0, 0 };

        s_z.output_speed = (seg == 7u) ? 1200 : 0;
        s_z.q_output_speed = Q_OK;
        if (code != 0u) {
            send(CAN_BUS1, 0x2EAu, 5u, true, l);
            send(CAN_BUS2, 0x210u, 5u, false, l);
        }
        if (mode >= 0) {
            sens_set(sc[mode == 6 ? (int)(t % 7u) : mode]);
            sens_pass();
            sens_pass();
        }
        {   /* the bench sims: 0x140 brake pressed, 0x108 700 rpm (the chain
             * releases P only with them) */
            uint8_t d[8] = { 0x04u, 0, 0, 0, 0, 0, 0, 0 };
            send(CAN_BUS1, 0x140u, 8u, true, d);
            d[0] = 0x02u;
            d[1] = 0xBCu;
            d[2] = 0x1Fu;
            d[3] = 0xFFu;
            send(CAN_BUS1, 0x108u, 8u, true, d);
        }
        vehicle_update_10ms();
        {   /* the live range chain, with park_pair's group-98 verdict as
             * main.c passes it (none without the sensor) */
            park_pair_wit_t pw;
            memset(&pw, 0, sizeof pw);
            if (mode >= 0) {
                park_pair_witness(&pw);
            }
            rng_chain_tick_10ms(1u, pw.failed, pw.tested);
        }
        park_tick_10ms();
        if (mode >= 0) {
            park_pair_tick_10ms();
        }
        park_snap(&out[t]);
    }
}

static void test_readonly(void)
{
    static park_snap_t ref[N_STEPS], run[N_STEPS];
    unsigned seen_rel = 0u, seen_tgt6 = 0u, seen_tgt8 = 0u;

    /* 2026-11-17: the range is the live stock chain, which takes the
     * group-98 (P0850) verdict as an input — in its stock role only (D660
     * → 0xC77A0 can force P, never release it); the sims run so the
     * schedule releases park; the outputs must still be identical in every
     * sensor state */
    park_run(-1, ref);
    for (uint32_t t = 0u; t < N_STEPS; t++) {
        seen_rel += ref[t].rel != 0u;
        seen_tgt6 += ref[t].tgt != 8u;
        seen_tgt8 += ref[t].tgt == 8u;
    }
    if (getenv("PKS_DBG")) { for (uint32_t t = 0u; t < N_STEPS; t += 10u) printf("%u:%u/%02X%c ", (unsigned)t, ref[t].tgt, ref[t].reason, ref[t].rel ? 'R' : '.'); printf("mon %02X\n", ref[N_STEPS-1u].mon); }
    CHECK(seen_rel > 0u && seen_tgt6 > 0u && seen_tgt8 > 0u);   /* the schedule exercises both */
    for (int m = 0; m <= 6; m++) {
        park_run(m, run);
        for (uint32_t t = 0u; t < N_STEPS; t++) {
            if (memcmp(&ref[t], &run[t], sizeof ref[t]) != 0) {
                printf("FAIL read-only: sensor scenario %d differs at tick %u\n", m, (unsigned)t);
                fails++;
                break;
            }
        }
    }
}

int main(void)
{
    test_idle();
    test_single(&S_A, 1u, 0x04u);
    test_single(&S_B, 0u, 0x08u);
    test_both();
    test_fault(&S_AFLT, 2u, 0x20u, 0x08u, PKP_SRC_P07B2);     /* band 2 → 0x20 → bit 3 */
    test_fault(&S_BFLT, 3u, 0x10u, 0x04u, PKP_SRC_P07B8);     /* band 4 → 0x10 → bit 2 */
    test_fault(&S_BFLT0, 3u, 0x08u, 0x01u, PKP_SRC_P07B8);    /* band 0 → 0x08 → bit 0 */
    test_fault(&S_AMISM, 2u, 0x02u, 0x08u, PKP_SRC_P07B2);    /* chip view disagrees → 0x02 */
    test_ref_low();
    test_torn();
    test_readonly();
    if (fails != 0) {
        printf("%d failure(s)\n", fails);
        return 1;
    }
    printf("pksens: all passed\n");
    return 0;
}

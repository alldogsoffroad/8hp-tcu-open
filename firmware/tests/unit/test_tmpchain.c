/* Unit test: THE TEMPERATURE BYTE 0xFFF89C13, LIVE (2026-11-17,
 * ~/tcu_sweep/temp_byte_live_port.md) — the stock chain 0x887D8 (fault
 * tail → 0x7BF8C) → 0x7C100 → the safety layer (0x1517E0 / 0x14A6E8 /
 * 0x1405FA, boot 0x151C9C / 0x140416, mode 0 0x132DCE / 0x14A4F0) →
 * 0xB9646 → 0xB986C, and the request producer (M5-1b, A applied at boot)
 * reading it:
 *   1. 0x151DD8, the limiter's step lookup, against the Ghidra decompile's
 *      C (an independent rendering) for every input byte;
 *   2. the fault tail and the publish (0x940E6 = 0xFF on a fault);
 *   3. (--stock) the driving dump reproduced: from the dump's own gathered
 *      inputs and limiter state, one 100 ms slice + 0xB9646 + 0xB986C give
 *      the dump's 0x43 at 0xFFF8E9C8 / 0x89CD2 / 0x89C13; with the dump's
 *      later-read 0x940E6 = 0x42 the same slice gives 0x42 (the one-count
 *      mismatch is the live read); the cal bytes / descriptors vs the image;
 *   4. the schedule: the boot value, a falling and a rising step through
 *      the 100 / 30 / 10 ms chain (latency, slew);
 *   5. the invalid sensor: at boot and cyclic → 0x82 (90 °C), recovery;
 *   6. the producer swept over every byte 0..255 (−40..+215 °C, the
 *      substitute 0x82 included): A's fill pass by pass, every value
 *      inside the guard (500 / 8500, or a fill state with [500, 8500]),
 *      the guard never fires; the table printed for the report;
 *   7. at the dump's byte 0x43, through the whole chain, the producer's
 *      wire is the r03p / r03t one (the compile-time 0x43 trace).
 */
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/drv/drv_temp.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_ladder.h"
#include "app/rq_prod.h"
#include "app/rq_fill.h"
#include "app/tmp_chain.h"
#include "safety/tmp_safe.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

#define U8(a)   (*(volatile uint8_t  *)(uintptr_t)(a))
#define U16(a)  (*(volatile uint16_t *)(uintptr_t)(a))
#define E8(off) U8(TS_E998 + (off))
#define D8(off) U8(TS_ED20 + (off))

/* ── 1. the Ghidra decompile of 0x151DD8, transcribed (reference) ─────── */
static unsigned ref_151dd8(const uint8_t *p, uint8_t param_2)
{
    const uint8_t *pbVar9 = p + 4;
    const int iVar5 = (int)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                            ((uint32_t)p[2] << 8) | p[3]);
    const uint8_t *pbVar2;
    uint8_t bVar4, bVar1;
    short sVar7;
    unsigned uVar8, uVar6, uVar3;
    if (*pbVar9 < param_2) {
        pbVar2 = pbVar9;
        if (param_2 < *(p + iVar5 + 3)) {
            for (; *pbVar2 < param_2; pbVar2 = pbVar2 + 1) {
            }
            bVar1 = pbVar2[-1];
            sVar7 = (short)((pbVar2 - 1) - pbVar9);
            bVar4 = (uint8_t)(*pbVar2 - bVar1);
            param_2 = (uint8_t)(param_2 - bVar1);
        } else {
            bVar4 = 1;
            param_2 = 1;
            sVar7 = (short)(iVar5 - 2);
        }
    } else {
        bVar4 = 0;
        param_2 = 0;
        sVar7 = 0;
    }
    uVar8 = param_2;
    uVar6 = pbVar9[sVar7 + iVar5];
    if (uVar8 != 0) {
        uVar3 = (pbVar9 + sVar7 + iVar5)[1];
        if (uVar3 < uVar6) {
            uVar6 = (uVar6 - ((uVar8 * ((uVar6 - uVar3) & 0xffffu)) / (unsigned)bVar4 & 0xffu)) & 0xffu;
        } else {
            uVar6 = ((uVar8 * ((uVar3 - uVar6) & 0xffffu)) / (unsigned)bVar4 + uVar6) & 0xffu;
        }
    }
    return uVar6;
}

/* ── the chain environment ────────────────────────────────────────────── */
static void env_reset(void)
{
    sup_cells_init();
    drv_jobs_init();                    /* → drv_rq_init → rq_prod_init */
    drv_temp_init();
    tmp_chain_init();
    drv_rq_prod_host_select(true);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, 0u);
    }
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9020u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 12940u;
}

/* the published fluid byte as 0x887D8 would leave it (0x94102), then
 * 0x7C100 */
static void set_byte(uint8_t b)
{
    CELL_TMP_BYTE_SEL = b;
    drv_temp_pub_tick();
}

/* one 10 ms tick of our task_10ms order in OS mode m (the 5 ms publish
 * twice before it, as the supervisor batch runs) */
static void tick10(uint8_t m, int prod)
{
    drv_temp_pub_tick();
    drv_temp_pub_tick();
    tmp_chain_task_10ms(m);
    if (prod) {
        rq_prod_task_10ms(m);
    }
    tmp_chain_task_slow(m);
}

/* ── 2. the fault tail ────────────────────────────────────────────────── */
static void temp_pass(void) { sup_ladder_tick_head(); drv_temp_tick(); drv_temp_pub_tick(); }

static void test_fault_tail(void)
{
    env_reset();
    CHECKV(CELL_TMP_PUB_E2, TMP_STANDIN_SRC_FAULTED);
    CHECKV(CELL_TMP_PUB_E5, TMP_STANDIN_SRC_FAULTED);
    CELL_AN_MV(1) = 164u;
    CELL_AN_MV(8) = 3031u;
    for (unsigned k = 0u; k < 3u; k++) {
        temp_pass();
    }
    CHECKV(CELL_TMP_FLAGS, 0x0880u);
    CHECKV(CELL_TMP_BYTE_SEL, 0x42u);
    CHECKV(U16(0xFFF93AE8u), 0u);            /* the code */
    CHECKV(U16(0xFFF93AEAu), 0x01FCu);       /* the mask */
    CHECKV(CELL_TMP_PUB_E7, 0x87u);          /* dump 0x940E7 = 87 */
    CHECKV(CELL_TMP_PUB_E8, 0u);             /* dump 0x940E8 = 00 */
    CHECKV(CELL_TMP_PUB_E6, 0x42u);
    CHECKV(CELL_TMP_PUB_E3, 0xFFu);          /* the absent second source */
    CHECKV(CELL_TMP_PUB_E0, 0xFFu);          /* the absent third source */
    /* AN8 open (0 mV): code 0x80 → 0x93914 bit 7 → 0x940E8 bit 7 */
    CELL_AN_MV(8) = 0u;
    temp_pass();
    CHECKV(CELL_TMP_FLAGS & 0x087Fu, 0x0854u);
    CHECKV(U16(0xFFF93AE8u), 0x80u);
    CHECKV(CELL_TMP_FWORD, 0x80u);
    CHECKV(CELL_TMP_PUB_E8, 0x80u);
    CHECKV(CELL_TMP_PUB_E6, 0xFFu);
    /* AN8 shorted high (5000 mV): 0x808 → code 4 → 0x940E8 = 0x02 */
    CELL_AN_MV(8) = 5000u;
    temp_pass();
    CHECKV(CELL_TMP_FLAGS & 0x0809u, 0x0808u);
    CHECKV(U16(0xFFF93AE8u), 0x04u);
    CHECKV(CELL_TMP_PUB_E8, 0x02u);
    CHECKV(CELL_TMP_PUB_E6, 0xFFu);
    /* back: the jump pass, then plausible → code 0, flags clear */
    CELL_AN_MV(8) = 3031u;
    temp_pass();
    temp_pass();
    CHECKV(U16(0xFFF93AE8u), 0u);
    CHECKV(CELL_TMP_PUB_E8, 0u);
    CHECK(CELL_TMP_PUB_E6 != 0xFFu);
}

/* ── 4. the schedule ──────────────────────────────────────────────────── */
static void boot_seq(uint8_t b, unsigned mode0_ticks)
{
    env_reset();
    set_byte(b);
    for (unsigned k = 0u; k < mode0_ticks; k++) {
        tick10(0u, 0);
    }
    tmp_chain_boot_pass();
    rq_prod_boot_pass();
}

static void test_schedule(void)
{
    unsigned k;
    boot_seq(0x4Eu, 30u);
    /* the boot pass: E998 conditioned directly, copied, latched */
    CHECKV(E8(TS_E_OUT), 0x4Eu);
    CHECKV(E8(TS_E_ST), 0u);
    CHECKV(E8(TS_E_OUT2), 0x82u);            /* the second source "faulted": 0 + 0x82 */
    CHECKV(E8(TS_E_ST2), 2u);
    CHECKV(TMP_CELL_89CD2, 0x4Eu);
    CHECKV(TMP_CELL_89C13, 0x4Eu);
    /* ED20 re-initialised in mode 0: state = byte × 100 */
    CHECKV(U16(TS_ED20 + TS_D_STATE), 7800u);
    CHECKV(D8(TS_D_C6), 0x4Eu);
    /* mode 1, steady: unchanged */
    for (k = 0u; k < 30u; k++) {
        tick10(1u, 1);
        CHECKV(TMP_CELL_89C13, 0x4Eu);
    }
    CHECKV(D8(TS_D_LIM), 0x4Eu);
    CHECKV(D8(TS_D_LIM + 1u), 0xB1u);
    CHECKV(E8(TS_E_OUT_C), 0xB1u);
    CHECKV(E8(TS_E_ST2), 1u);                /* D3 = 1 (stand-in): +0x35 = 1 */
    /* a falling step (one count): the truncation passes it in one slice;
     * 0x89C13 follows within 100 + 30 + 10 ms */
    set_byte(0x4Du);
    for (k = 0u; k < 20u && TMP_CELL_89C13 != 0x4Du; k++) {
        tick10(1u, 1);
    }
    printf("falling step 0x4E → 0x4D: 0x89C13 followed after %u ticks of 10 ms\n", k);
    CHECK(TMP_CELL_89C13 == 0x4Du && k <= 14u);
    for (k = 0u; k < 300u; k++) {           /* the state settles at 7700 */
        tick10(1u, 1);
    }
    CHECKV(U16(TS_ED20 + TS_D_STATE), 7700u);
    /* a rising step (one count) at 0x4D/0x4E (37..38 °C): slew 3 + (37·3/20)
     * = 0x151DD8(0x4E) per 100 ms slice from 7799 → 7800 */
    {
        const unsigned st = tmp_safe_151dd8(TS_STEP_TBL, 0x4Eu);
        set_byte(0x4Eu);
        for (k = 0u; k < 400u && TMP_CELL_89C13 != 0x4Eu; k++) {
            tick10(1u, 1);
        }
        printf("rising step 0x4D → 0x4E: step %u per slice, 0x89C13 followed after %u ticks of 10 ms\n",
               st, k);
        CHECK(TMP_CELL_89C13 == 0x4Eu);
        CHECK(k > 100u);                     /* slewed: ≥ 100 / step slices */
    }
}

/* ── 5. the invalid sensor ────────────────────────────────────────────── */
static void test_invalid(void)
{
    unsigned k;
    /* at boot (the fault flag already set) */
    env_reset();
    CELL_TMP_PUB_E8 = 0x80u;
    set_byte(0x4Eu);
    CHECKV(CELL_TMP_PUB_E6, 0xFFu);
    for (k = 0u; k < 10u; k++) {
        tick10(0u, 0);
    }
    tmp_chain_boot_pass();
    CHECKV(E8(TS_E_OUT), TS_CAL_DEFAULT);
    CHECKV(E8(TS_E_ST), 2u);
    CHECKV(TMP_CELL_89C13, 0x82u);
    for (k = 0u; k < 30u; k++) {
        tick10(1u, 0);
        CHECKV(TMP_CELL_89C13, 0x82u);
    }
    CHECKV(D8(TS_D_D5), 1u);
    CHECKV(D8(TS_D_D3), 1u);
    CHECKV(E8(TS_E_ST), 2u);
    /* cyclic: steady at 0x4E, then the fault */
    boot_seq(0x4Eu, 10u);
    for (k = 0u; k < 20u; k++) {
        tick10(1u, 0);
    }
    CELL_TMP_PUB_E8 = 0x80u;
    set_byte(0x4Eu);
    for (k = 0u; k < 20u && TMP_CELL_89C13 != 0x82u; k++) {
        tick10(1u, 0);
    }
    printf("fault: 0x89C13 → 0x82 after %u ticks of 10 ms\n", k);
    CHECK(TMP_CELL_89C13 == 0x82u && k <= 14u);
    CHECKV(D8(TS_D_JUMP), 1u);               /* 0xFF − 0x4E > 20 */
    CHECKV(D8(TS_D_C6), 0x4Eu);              /* frozen */
    /* recovery at the same temperature: the flag clears, the jump clears
     * (|0x4E − 0x4E| <= 5), the limiter resets → 0x4E */
    CELL_TMP_PUB_E8 = 0u;
    set_byte(0x4Eu);
    for (k = 0u; k < 20u && TMP_CELL_89C13 != 0x4Eu; k++) {
        tick10(1u, 0);
    }
    printf("recovery (same temperature): 0x89C13 → 0x4E after %u ticks\n", k);
    CHECK(TMP_CELL_89C13 == 0x4Eu && E8(TS_E_ST) == 0u);
    /* recovery 12 °C warmer than before the fault: |0x5A − 0x4E| > 5 keeps
     * the jump flag (+0xC6 frozen) — the substitute stays (until power-off) */
    CELL_TMP_PUB_E8 = 0x80u;
    set_byte(0x4Eu);
    for (k = 0u; k < 20u; k++) {
        tick10(1u, 0);
    }
    CELL_TMP_PUB_E8 = 0u;
    set_byte(0x5Au);
    for (k = 0u; k < 100u; k++) {
        tick10(1u, 0);
    }
    printf("recovery (+12 °C): 0x89C13 = 0x%02X, jump %u\n", TMP_CELL_89C13, D8(TS_D_JUMP));
    CHECK(TMP_CELL_89C13 == 0x82u && D8(TS_D_JUMP) == 1u);
}

/* ── 6. the producer over every byte ──────────────────────────────────── */
#define NP 60u
typedef struct {
    uint16_t a[NP];             /* A's current per pass */
    uint8_t st[NP];
    uint16_t tfill;             /* A +0x36 at the boot pass */
    unsigned fill_end;          /* first applied pass */
    unsigned guard;             /* drv_rq_m5_guard_n at the end */
    unsigned bad;               /* values outside the guard's rule */
} sw_t;

static void prod_run(uint8_t b, sw_t *o)
{
    memset(o, 0, sizeof *o);
    env_reset();
    TMP_CELL_89C13 = b;
    TMP_CELL_89CD2 = b;
    o->fill_end = NP;
    for (unsigned k = 0u; k < NP; k++) {
        if (k == 0u) {
            rq_prod_boot_pass();
            o->tfill = U16(RQ_OBJ_A + RO_TFILL);
        } else {
            rq_prod_task_10ms(1u);
        }
        drv_cyclic_build(0, 8u);
        o->a[k] = U16(0xFFF84B78u);
        o->st[k] = U8(RQ_OBJ_A + RO_ST);
        if (o->st[k] == RQF_ST_APPLIED && o->fill_end == NP) {
            o->fill_end = k;
        }
        for (uint8_t ch = 0u; ch < 7u; ch++) {
            const uint16_t v = rq_prod_ch(ch);
            const int ok = (v == 500u || v == 8500u) ||
                           drv_rq_fill_ok(ch, rq_prod_fill_state(ch), v);
            if (!ok || CELL_RQ_I[ch] != v) {
                o->bad++;
            }
        }
    }
    o->guard = drv_rq_m5_guard_n();
}

static void test_sweep(void)
{
    static sw_t s;
    static const uint8_t SHOW[] = { 0x00, 0x14, 0x28, 0x32, 0x3C, 0x42, 0x43, 0x46, 0x4A,
                                    0x4D, 0x4E, 0x50, 0x55, 0x5A, 0x64, 0x78, 0x82, 0xA0,
                                    0xBE, 0xFF };
    unsigned total_bad = 0u, total_guard = 0u, total_unp = 0u;
    printf("M5-1b sweep: byte (°C) | A +0x36 fill ms | applied at pass | A current per pass 0..13\n");
    for (unsigned b = 0u; b <= 0xFFu; b++) {
        prod_run((uint8_t)b, &s);
        total_bad += s.bad;
        total_guard += s.guard;
        total_unp += rq_prod_unp_n() + rq_prod_f19f_n();
        for (unsigned i = 0u; i < sizeof SHOW; i++) {
            if (SHOW[i] == b) {
                printf("  0x%02X (%4d) | %4u | %2u |", b, (int)b - 40, s.tfill, s.fill_end);
                for (unsigned k = 0u; k < 14u; k++) {
                    printf(" %4u", s.a[k]);
                }
                printf("\n");
            }
        }
        /* the steady state: A 8500 once applied, B 500 (block 0 gear 1) */
        CHECK(s.fill_end < NP);
        CHECK(s.a[NP - 1u] == 8500u && rq_prod_ch(2u) == 500u);
    }
    printf("sweep: %u values outside the guard rule, guard fired %u times, unported/fault %u\n",
           total_bad, total_guard, total_unp);
    CHECKV(total_bad, 0);
    CHECKV(total_guard, 0);
    CHECKV(total_unp, 0);
}

/* ── 7. the dump's byte through the whole chain = the r03p / r03t wire ─── */
static void test_dump_wire(void)
{
    static uint16_t w_const[40][85], w_chain[40][85];
    /* (a) the producer with the cells set to 0x43 (what r03t's compile-time
     * constant gave) */
    env_reset();
    TMP_CELL_89C13 = RQ_M5_TEMP_DUMP;
    TMP_CELL_89CD2 = RQ_M5_TEMP_DUMP;
    for (unsigned k = 0u; k < 40u; k++) {
        if (k == 0u) {
            rq_prod_boot_pass();
        } else {
            rq_prod_task_10ms(1u);
        }
        drv_cyclic_build(0, 8u);
        memcpy(w_const[k], drv_job_tx(DRV_CYC), 45u * sizeof(uint16_t));
        memcpy(&w_const[k][45], drv_job_tx(DRV_CFG), 40u * sizeof(uint16_t));
    }
    /* (b) the whole chain from 0x94102 = 0x43 */
    boot_seq(0x43u, 20u);
    CHECKV(TMP_CELL_89C13, 0x43u);
    drv_cyclic_build(0, 8u);
    memcpy(w_chain[0], drv_job_tx(DRV_CYC), 45u * sizeof(uint16_t));
    memcpy(&w_chain[0][45], drv_job_tx(DRV_CFG), 40u * sizeof(uint16_t));
    for (unsigned k = 1u; k < 40u; k++) {
        tick10(1u, 1);
        drv_cyclic_build(0, 8u);
        memcpy(w_chain[k], drv_job_tx(DRV_CYC), 45u * sizeof(uint16_t));
        memcpy(&w_chain[k][45], drv_job_tx(DRV_CFG), 40u * sizeof(uint16_t));
        CHECKV(TMP_CELL_89C13, 0x43u);
    }
    CHECK(memcmp(w_const, w_chain, sizeof w_const) == 0);
    CHECKV(drv_rq_m5_guard_n(), 0);
}

/* ── 3. the image and the dump ────────────────────────────────────────── */
#ifndef TCU_STOCK_IMAGE
#define TCU_STOCK_IMAGE ""
#endif
#ifndef TCU_STOCK_DUMP
#define TCU_STOCK_DUMP ""
#endif

static size_t load(const char *path, uint8_t *buf, size_t max)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (f == NULL) {
        return 0u;
    }
    n = fread(buf, 1u, max, f);
    fclose(f);
    return n;
}

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
#define R(a)    (ram[(a) - 0xFFF80000u])

static int stock_checks(void)
{
    static uint8_t img[0x200000], ram[0x20000];
    const size_t ni = load(TCU_STOCK_IMAGE, img, sizeof img);
    const size_t nr = load(TCU_STOCK_DUMP, ram, sizeof ram);
    if (ni != sizeof img || nr != sizeof ram) {
        printf("tmpchain --stock: image (%zu) or dump (%zu) not available — skipped\n", ni, nr);
        return 77;
    }
    /* the cal through the descriptors [O image + dump] */
    CHECKV(be32(&R(TS_E998 + 0x20u)), 0x000648E0u);         /* E998 +0x20 */
    CHECKV(be32(&R(TS_ED20 + 0x94u)), 0x00064BC8u);         /* ED20 +0x94 */
    CHECKV(be32(&img[0x648E4]), 0x00195DB7u);
    CHECKV(be32(&img[0x648E8]), 0x00195DB8u);
    CHECKV(img[0x195DB7], TS_CAL_SUB2);
    CHECKV(img[0x195DB8], TS_CAL_DEFAULT);
    CHECKV(be32(&img[0x64BC8]), 0x00191E44u);
    CHECKV(be32(&img[0x64BCC]), 0x00195DE7u);
    CHECKV(be32(&img[0x64BD0]), 0x00195DE8u);
    CHECKV(img[0x195DE7], TS_CAL_JUMP_SET);
    CHECKV(img[0x195DE8], TS_CAL_JUMP_CLR);
    CHECK(memcmp(&img[0x191E44], TS_STEP_TBL, sizeof TS_STEP_TBL) == 0);
    /* code constants: 0x140050 reads 0xFFF8E9C8; 0xB9646 GBR 0xFFF89C3C,
     * +27 / +150; 0xB986C @0xB9D6A; 0x7C100's movi20s */
    CHECKV(be32(&img[0x1402AC]), 0xFFF8E9C8u);
    {
        static const struct { uint32_t a; uint8_t n; uint8_t b[4]; } K[] = {
            { 0xB964A, 4, { 0x00, 0x80, 0x9C, 0x3C } },   /* movi20 #-484292,r0 (GBR) */
            { 0xB965A, 2, { 0xC0, 0x1B } },               /* mov.b r0,@(27,gbr) */
            { 0xB965E, 2, { 0xC0, 0x96 } },               /* mov.b r0,@(150,gbr) */
            { 0xB9D6A, 2, { 0x80, 0x5F } },               /* mov.b r0,@(15,r5) (0x89C13) */
            { 0x7C100, 4, { 0x06, 0x90, 0x40, 0xE8 } },   /* movi20 0x940E8 */
            { 0x7C10C, 4, { 0x05, 0x90, 0x40, 0xE6 } },   /* movi20 0x940E6 */
            { 0x7C114, 4, { 0x06, 0x90, 0x41, 0x02 } },   /* movi20 0x94102 */
            { 0x88938, 4, { 0x0B, 0x90, 0x41, 0x02 } },   /* 0x887D8: movi20 0x94102 */
            { 0x14AD20, 2, { 0xE0, 0x64 } },              /* 0x14ACF0: × 100 */
            { 0x14AD9C, 2, { 0xE0, 0x64 } },              /* / 100 */
        };
        for (size_t k = 0u; k < sizeof K / sizeof K[0]; k++) {
            if (memcmp(&img[K[k].a], K[k].b, K[k].n) != 0) {
                printf("FAIL code constant at 0x%X\n", K[k].a);
                fails++;
            }
        }
    }
    /* the task lists: 0x13204A first in 0x4D9E4 (row 2 = 100 ms, mode 1,
     * via 0x42E58), 0xA318A at 0x4D98C (30 ms 0x4D940 entry 19), 0xA2EFC
     * at 0x4D8A8 (10 ms 0x4D834 entry 29), 0x14F32C first in the row-1
     * list 0x4D7B0, 0x1320B0 first in 0x4D7A0 (10 ms mode 0) */
    CHECKV(be32(&img[0x43430u + (6u * 2u + 1u) * 4u]), 0x42E58u);
    CHECKV(be32(&img[0x42E64]), 0x4D9E4u);
    CHECKV(be32(&img[0x4D9E4]), 0x13204Au);
    CHECKV(be32(&img[0x4D98C]), 0xA318Au);
    CHECKV(be32(&img[0x4D8A8]), 0xA2EFCu);
    CHECKV(be32(&img[0x4D7B0]), 0x14F32Cu);
    CHECKV(be32(&img[0x42D78]), 0x4D7A0u);
    CHECKV(be32(&img[0x4D7A0]), 0x1320B0u);
    /* the dump: the 100 ms slice from the dump's ED20 state with the inputs
     * the gather saw (ED20 +0xB8..+0xBD) */
    env_reset();
    for (uint32_t o = TS_D_B8; o <= TS_D_D7 + 1u; o++) {
        D8(o) = R(TS_ED20 + o);
    }
    U16(TS_ED20 + TS_D_STATE) = be16(&R(TS_ED20 + TS_D_STATE));
    for (uint32_t o = TS_E_IN2; o <= TS_E_ST2; o++) {
        E8(o) = R(TS_E998 + o);
    }
    CELL_TMP_PUB_E0 = R(TS_ED20 + TS_D_B8);
    CELL_TMP_PUB_E2 = R(TS_ED20 + TS_D_B9);
    CELL_TMP_PUB_E3 = R(TS_ED20 + TS_D_BA);
    CELL_TMP_PUB_E5 = R(TS_ED20 + TS_D_BB);
    CELL_TMP_PUB_E6 = R(TS_ED20 + TS_D_BC);
    CELL_TMP_PUB_E8 = R(TS_ED20 + TS_D_BD);
    printf("dump: gathered 0x940E6 = 0x%02X (live cell 0x%02X), ED20 state %u, +0xC2 0x%02X, E9C8 0x%02X, "
           "0x89CD2 0x%02X, 0x89C13 0x%02X, 0x94102 0x%02X, 0x94F98 0x%04X\n",
           R(TS_ED20 + TS_D_BC), R(0xFFF940E6u), be16(&R(TS_ED20 + TS_D_STATE)), R(TS_ED20 + TS_D_LIM),
           R(0xFFF8E9C8u), R(0xFFF89CD2u), R(0xFFF89C13u), R(0xFFF94102u), be16(&R(0xFFF94F98u)));
    tmp_safe_100ms();
    tmp_chain_b9646();
    tmp_chain_b986c();
    CHECKV(U16(TS_ED20 + TS_D_STATE), be16(&R(TS_ED20 + TS_D_STATE)));
    CHECKV(D8(TS_D_LIM), R(TS_ED20 + TS_D_LIM));
    CHECKV(D8(TS_D_LIM + 1u), R(TS_ED20 + TS_D_LIM + 1u));
    CHECKV(D8(TS_D_D5), R(TS_ED20 + TS_D_D5));
    CHECKV(D8(TS_D_D3), R(TS_ED20 + TS_D_D3));
    CHECKV(D8(TS_D_JUMP), R(TS_ED20 + TS_D_JUMP));
    CHECKV(D8(TS_D_C6), R(TS_ED20 + TS_D_C6));
    for (uint32_t o = TS_E_OUT; o <= TS_E_ST2; o++) {
        CHECKV(E8(o), R(TS_E998 + o));                  /* E9C8..E9CD */
    }
    CHECKV(TMP_CELL_89C57, R(0xFFF89C57u));
    CHECKV(TMP_CELL_89CD2, R(0xFFF89CD2u));
    CHECKV(TMP_CELL_89C13, R(0xFFF89C13u));
    CHECKV(TMP_CELL_89C13, 0x43u);
    /* the same slice with the dump's LATER-READ published byte 0x42: the
     * truncation passes it at once */
    CELL_TMP_PUB_E6 = R(0xFFF940E6u);
    tmp_safe_100ms();
    tmp_chain_b9646();
    tmp_chain_b986c();
    printf("dump, with the later-read 0x940E6 = 0x%02X: state %u, +0xC2 0x%02X → 0x89C13 0x%02X\n",
           R(0xFFF940E6u), U16(TS_ED20 + TS_D_STATE), D8(TS_D_LIM), TMP_CELL_89C13);
    CHECKV(TMP_CELL_89C13, 0x42u);
    /* the published cells of the dump: 0x940E6 = 0x94102 (flag 0), 0x940E3
     * = 0x94101 (flag 0), 0x940E0 = 0x940FE, 0x940E7 = 0x87 */
    CHECKV(R(0xFFF940E6u), R(0xFFF94102u));
    CHECKV(R(0xFFF940E3u), R(0xFFF94101u));
    CHECKV(R(0xFFF940E0u), R(0xFFF940FEu));
    CHECKV(R(0xFFF940E7u), 0x87u);
    CHECKV(R(0xFFF940E8u), 0u);
    if (fails == 0) {
        printf("tmpchain --stock: all checks passed\n");
    }
    return fails == 0 ? 0 : 1;
}

/* 2026-11-17: the rest of the stock's 100 ms safety slice 0x14F7EA (main.c:
 * the park sensor's group-98 verdict 0x141E06) hangs off the same slot —
 * every 10th tick of modes 1..3, never in mode 0 */
static unsigned s_hook100_n;
void tmp_chain_hook_100ms(void) { s_hook100_n++; }

static void test_hook_100ms(void)
{
    unsigned n0;
    s_hook100_n = 0u;
    for (unsigned t = 0u; t < 50u; t++) {
        tmp_chain_task_slow(0u);              /* mode 0: never */
    }
    CHECK(s_hook100_n == 0u);
    n0 = s_hook100_n;
    for (unsigned t = 0u; t < 100u; t++) {
        tmp_chain_task_slow(1u);
    }
    CHECK(s_hook100_n - n0 == 10u);           /* 100 ticks of mode 1: 10 calls */
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--stock") == 0) {
        return stock_checks();
    }
    /* 1 */
    for (unsigned x = 0u; x <= 0xFFu; x++) {
        CHECKV(tmp_safe_151dd8(TS_STEP_TBL, (uint8_t)x), ref_151dd8(TS_STEP_TBL, (uint8_t)x));
    }
    CHECKV(tmp_safe_151dd8(TS_STEP_TBL, 0x28u), 3u);
    CHECKV(tmp_safe_151dd8(TS_STEP_TBL, 0x42u), 5u);
    CHECKV(tmp_safe_151dd8(TS_STEP_TBL, 0x4Eu), 6u);
    CHECKV(tmp_safe_151dd8(TS_STEP_TBL, 0xFFu), 3u);
    test_fault_tail();      /* 2 */
    test_schedule();        /* 4 */
    test_invalid();         /* 5 */
    test_sweep();           /* 6 */
    test_dump_wire();       /* 7 */
    test_hook_100ms();      /* 8 */
    if (fails == 0) {
        printf("tmpchain: all checks passed\n");
    }
    return fails == 0 ? 0 : 1;
}

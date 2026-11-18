/* test_tsel_shadow.c — the READ-ONLY ECU-temperature shadow (app/tsel_shadow.c;
 * ~/tcu_sweep/diag_temp_inputs_decode.md, the port
 * ~/tcu_sweep/diag_temp_shadow_port.md), driven directly by value.
 *
 *   (1) the stock driving dump reproduced: the dump's own inputs (J3 average
 *       381, J4 word 0x10AB, comparators 00, variant 0, fluid 411 / 8) give
 *       0x93ABA 0x0219, flags 0x0961, 0x9393E 0x0215, flags 0x0001,
 *       0x9393A 0x0215, 0x940FC 08, source 04, bytes 0x940FF / 0x94101
 *       0x49, the masks 0x62 / 0x160, codes 0, 0x940E1 0x68, 0x940E4 0xE0,
 *       0x940E2 = 0x940E5 = 0; the dump's torn pair (0x9393C 0x0212 and
 *       0x940FE 0x49) from N = 379; with TCU_STOCK_DUMP present those cells
 *       are compared with the dump file too;
 *   (2) a cold boot at the bench's values (r04f: J3 433, J4 0x164B, fluid
 *       35.9 °C): the board's 0xFF03 not-ready word → no board status, the
 *       die (6) then the board (8) win;
 *   (3) a lost board after valid: 0x8001 (our re-arm read twice), the low
 *       "1" bits missing, 0xFFFF, out of range → status 7 for the 120-step
 *       hold (600 ms in both cadences), then 4 with 0x1000 and the failed
 *       flag; the selection falls back to the die (6, source 2); recovery;
 *   (4) the die: N < 5 / > 1022, the comparator mismatch (0x30 → 7; a
 *       consistent pass clears it; sustained → 4 after 8 steps; kept only
 *       in the 128..136 °C band where no consistency test fires);
 *   (5) the selection's fallbacks: die only, fluid only (status 8 → 6,
 *       status 4 kept), none → 3 at 120 °C (byte 0xA0);
 *   --cal  the TSEL_CAL_* values against the stock image (exit 77: absent).
 */
#include "app/tsel_shadow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld (0x%lX), expected %ld (0x%lX)\n", __FILE__, __LINE__, #a, _a, \
           (unsigned long)_a, _b, (unsigned long)_b); fails++; } } while (0)

static tsel_in_t in;
static uint32_t s_t;

static void reset(uint8_t mode)
{
    memset(&in, 0, sizeof in);
    in.n5 = (uint8_t)((mode == 0u) ? 1u : 2u);
    in.os_mode = mode;
    in.fm_ready = 1u;
    s_t = 1000u;
    tsel_shadow_init();
}

static void pass(void)
{
    s_t += (in.n5 == 1u) ? 5000u : 10000u;
    in.t_us = s_t;
    tsel_shadow_pass(&in);
}

static void passes(unsigned n)
{
    for (unsigned k = 0u; k < n; k++) {
        pass();
    }
}

#define S (tsel_shadow_state())

/* ── (1) the dump ─────────────────────────────────────────────────────── */
static void test_dump(void)
{
    reset(1u);
    in.n908ca = 381u;
    in.r908cc = 0x10AB;
    in.f9095e = 0u;
    in.v90966 = 0u;
    in.t94f98 = 411;
    in.st94fee = 8u;
    passes(20u);
    printf("(1) dump: die %04X fl %04X st %u | board %04X fl %04X st %u | sel %04X st %u src %u bytes %02X/%02X/%02X"
           " | rep %02X/%02X %03X/%02X E1 %02X E2 %02X E4 %02X E5 %02X\n",
           (unsigned)(uint16_t)S->aba, S->ab6, S->s15e, (unsigned)(uint16_t)S->s93e, S->ac0, S->s100,
           (unsigned)(uint16_t)S->s93a, S->s0fc, S->s170, S->s0ff, S->s0fe, S->s101,
           S->af2, S->af0, S->af6, S->af4, S->e1, S->e2, S->e4, S->e5);
    CHECKV(S->aba, 0x0219);         /* 537 = 33.6 °C [D] (−738 + 3.347·381 = 537.2) */
    CHECKV(S->ab6, 0x0961);         /* valid, 0x20, 0x40, 0x100 + 0x800 consistent */
    CHECKV(S->s15e, 8);
    CHECKV(S->w477a, 8);            /* the dump's 0x9477A (reloaded on every valid pass) */
    CHECKV(S->b823, 0);             /* the dump's 0x94823 */
    CHECKV(S->s93e, 0x0215);        /* 0x10AB >> 3 = 533 = 33.3 °C */
    CHECKV(S->ac0, 0x0001);
    CHECKV(S->s100, 8);
    CHECKV(S->w77c, 120);
    CHECKV(S->w730, 0);
    CHECKV(S->s93a, 0x0215);
    CHECKV(S->s0fc, 8);
    CHECKV(S->s170, 4);
    CHECKV(S->s0ff, 0x49);
    CHECKV(S->s101, 0x49);
    CHECKV(S->s0fd, 8);
    CHECKV(S->af2, 0x62);
    CHECKV(S->af0, 0);
    CHECKV(S->af6, 0x160);
    CHECKV(S->af4, 0);
    CHECKV(S->w916, 0);
    CHECKV(S->e1, 0x68);
    CHECKV(S->e2, 0);
    CHECKV(S->e4, 0xE0);
    CHECKV(S->e5, 0);
    CHECKV(S->initrep_n, 0);
    /* the dump's torn pair: 0x9393C 0x0212 and 0x940FE 0x49 come from the
     * 0x88BD0 pass that saw N = 379 (−738 + 3.347·379 = 530.5) [D] */
    in.n908ca = 379u;
    pass();
    CHECKV(S->s93c, 0x0212);
    CHECKV(S->s0fe, 0x49);
#if defined(TCU_STOCK_DUMP)
    {
        static unsigned char d[0x20000];
        FILE *f = fopen(TCU_STOCK_DUMP, "rb");
        if (f == NULL || fread(d, 1u, sizeof d, f) != sizeof d) {
            printf("(1) dump file absent — cell comparison skipped\n");
        } else {
#define DW(a) ((unsigned)((d[(a) - 0xFFF80000u] << 8) | d[(a) - 0xFFF80000u + 1u]))
#define DB(a) ((unsigned)d[(a) - 0xFFF80000u])
            CHECKV(DW(0xFFF93ABAu), 0x0219u);
            CHECKV(DW(0xFFF93AB6u), 0x0961u);
            CHECKV(DW(0xFFF9393Eu), 0x0215u);
            CHECKV(DW(0xFFF93AC0u), 0x0001u);
            CHECKV(DW(0xFFF9393Au), 0x0215u);
            CHECKV(DW(0xFFF9393Cu), 0x0212u);
            CHECKV(DB(0xFFF940FCu), 8u);
            CHECKV(DB(0xFFF94170u), 4u);
            CHECKV(DB(0xFFF940FFu), 0x49u);
            CHECKV(DB(0xFFF940FEu), 0x49u);
            CHECKV(DB(0xFFF94101u), 0x49u);
            CHECKV(DB(0xFFF940E1u), 0x68u);
            CHECKV(DB(0xFFF940E4u), 0xE0u);
            CHECKV(DB(0xFFF940E2u), 0u);
            CHECKV(DB(0xFFF940E5u), 0u);
            CHECKV(DW(0xFFF93AF2u), 0x62u);
            CHECKV(DW(0xFFF93AF6u), 0x160u);
            CHECKV(DW(0xFFF9477Au), 8u);
            CHECKV(DW(0xFFF9477Cu), 120u);
            printf("(1) dump file cells compared\n");
        }
        if (f != NULL) {
            fclose(f);
        }
    }
#endif
}

/* ── (2) a cold boot at the bench's values ───────────────────────────── */
static void test_cold_boot(void)
{
    reset(0u);
    in.n908ca = 0u;                 /* no J3 reply yet */
    in.r908cc = 0;                  /* no J4 reply yet */
    pass();
    CHECKV(S->s15e, 0);             /* never valid, hold running: no status write */
    CHECKV(S->s100, 0);             /* never valid, flags 0x7C: no status write */
    CHECKV(S->s0fc, 3);
    CHECKV(S->s170, 0);
    CHECKV(S->s93a, 0x0780);
    CHECKV(S->s0ff, 0xA0);
    in.n908ca = 433u;               /* r04f: 433–434 */
    in.r908cc = (int16_t)0xFF03;    /* the board sensor's not-ready word */
    pass();
    CHECKV(S->s15e, 8);
    CHECKV(S->aba, 711);            /* 44.4 °C */
    CHECK((S->ac0 & 0x08u) != 0u);
    CHECKV(S->s100, 0);
    CHECKV(S->s0fc, 6);             /* the die, capped */
    CHECKV(S->s170, 2);
    in.r908cc = 0x164B;             /* r04f */
    pass();
    CHECKV(S->s100, 8);
    CHECKV(S->s93e, 713);           /* 44.6 °C */
    CHECKV(S->s0fc, 8);
    CHECKV(S->s170, 4);
    CHECKV(S->s93a, 713);
    CHECKV(S->s0ff, 0x55);          /* (713 + 8) >> 4 = 45, + 40 = 85 */
    /* the fluid arrives (drv_temp, ~0.5 s): the board keeps the selection */
    in.t94f98 = 574;
    in.st94fee = 8u;
    passes(10u);
    CHECKV(S->s0fc, 8);
    CHECKV(S->s170, 4);
    CHECKV(S->e5, 0);
    CHECKV(S->e2, 0);
    CHECK(S->first_sel8_us != 0u && S->ff03_n == 1u);
    printf("(2) cold boot: sel %d st %u src %u, die %d, board %d, ff03 %u, first sel 8 at %u us\n",
           S->s93a, S->s0fc, S->s170, S->aba, S->s93e, (unsigned)S->ff03_n, (unsigned)S->first_sel8_us);
}

/* ── (3) the board lost after valid ──────────────────────────────────── */
static unsigned hold_passes(uint8_t mode, int16_t bad, uint16_t want_e5)
{
    unsigned k = 0u;
    reset(mode);
    in.n908ca = 433u;
    in.r908cc = 0x164B;
    in.t94f98 = 574;
    in.st94fee = 8u;
    passes(5u);
    CHECKV(S->s100, 8);
    in.r908cc = bad;
    for (k = 1u; k <= 400u; k++) {
        pass();
        if (k == 1u) {
            CHECKV(S->s100, 7);
            CHECKV(S->s0fc, 7);     /* the board still wins (7 ≥ the die's 6) */
            CHECKV(S->s170, 4);
            CHECKV(S->s93a, 713);   /* held */
        }
        if (S->s100 == 4u) {
            break;
        }
    }
    CHECKV(S->s100, 4);
    CHECK((S->ac0 & 0x1000u) != 0u);
    CHECKV(S->s0fc, 6);             /* the die */
    CHECKV(S->s170, 2);
    CHECKV(S->s93a, 711);
    CHECKV(S->e5, want_e5);
    in.r908cc = 0x164B;             /* recovery */
    pass();
    CHECKV(S->s100, 8);
    CHECKV(S->s0fc, 8);
    CHECKV(S->s170, 4);
    CHECKV(S->e5, 0);
    return k;
}

static void test_board_lost(void)
{
    const unsigned m0 = hold_passes(0u, (int16_t)0x8001, 0x20u);    /* r < −5760 → 0x20 */
    const unsigned m1 = hold_passes(1u, (int16_t)0x8001, 0x20u);
    const unsigned lb = hold_passes(1u, 0x1648, 0x80u);              /* low bits 0 → 0x10 → code 0x100 */
    const unsigned ff = hold_passes(1u, (int16_t)0xFFFF, 0x80u);     /* 0x04 + 0x100 → code 0x100 */
    const unsigned hi = hold_passes(1u, 0x5401, 0x40u);              /* > 21504 → 0x40 */
    printf("(3) board lost: 0x8001 status 4 after %u passes (mode 0) / %u (mode 1); low bits %u, 0xFFFF %u, "
           "high %u (mode 1)\n", m0, m1, lb, ff, hi);
    CHECKV(m0, 120u);               /* 600 ms at 5 ms */
    CHECKV(m1, 60u);                /* 600 ms at 10 ms */
    CHECKV(lb, 60u);
    CHECKV(ff, 60u);
    CHECKV(hi, 60u);
}

/* ── (4) the die ──────────────────────────────────────────────────────── */
static void test_die(void)
{
    unsigned k;
    /* never valid: N below 5 → no status while the 8-step hold runs, then 2 */
    reset(0u);
    in.n908ca = 3u;
    for (k = 1u; k <= 20u && S->s15e != 2u; k++) {
        pass();
    }
    CHECKV(S->s15e, 2);
    CHECKV(k - 1u, 8u);             /* 8 steps = 40 ms */
    CHECK((S->ab6 & 0x82u) == 0x82u);
    /* valid, then N > 1022 → 7 for 8 steps → 4 */
    reset(1u);
    in.n908ca = 433u;
    passes(3u);
    CHECKV(S->s15e, 8);
    in.n908ca = 1023u;
    for (k = 1u; k <= 20u && S->s15e != 4u; k++) {
        pass();
    }
    CHECKV(S->s15e, 4);
    CHECKV(k - 1u, 4u);             /* 8 steps at 2 per pass */
    /* the comparator mismatch: bit 0 set while T < 128 °C → 0x200 → 0x30
     * (0x10 survives the per-pass clear) → 7; the next CONSISTENT pass
     * clears 0x10 (bytes 0x8DC96..0x8DC9A: bset #5, bclr #4) → valid again.
     * So the "latch" holds only while the comparators disagree, or while T
     * sits in a band where neither consistency test fires. */
    reset(0u);
    in.n908ca = 433u;
    passes(4u);
    CHECKV(S->s15e, 8);
    in.f9095e = 1u;
    pass();
    CHECK((S->ab6 & 0x230u) == 0x230u);
    CHECKV(S->s15e, 7);
    in.f9095e = 0u;
    pass();
    CHECK((S->ab6 & 0x10u) == 0u);
    CHECKV(S->s15e, 8);
    /* a SUSTAINED mismatch → 4 after the 8-step hold, report code 2 → E2 0x08 */
    in.f9095e = 1u;
    for (k = 1u; k <= 20u && S->s15e != 4u; k++) {
        pass();
    }
    CHECKV(S->s15e, 4);
    CHECKV(k - 1u, 8u);             /* the 8-step hold (40 ms), counted from the last valid pass */
    CHECK((S->ab6 & 0x10u) != 0u);
    CHECKV(S->af0, 2);              /* (flags & 0xD0) == 0xD0 with 0x20 → code 2 */
    CHECKV(S->e2, 0x08);
    /* in the 128..136 °C band (neither consistency test fires) the 0x10 of
     * an earlier mismatch stays: N 848 → T 2100 */
    in.f9095e = 0u;
    in.n908ca = 848u;
    passes(3u);
    CHECK((S->ab6 & 0x10u) != 0u);
    CHECKV(S->s15e, 4);
    in.n908ca = 433u;               /* back below 128 °C: consistent → valid */
    pass();
    CHECKV(S->s15e, 8);
    printf("(4) die: N<5 -> 2 after 8 steps; N>1022 -> 4; comparator mismatch -> 7, cleared by a consistent pass;"
           " sustained -> 4 (E2 0x08); kept in the 128..136 C band\n");
}

/* ── (5) the selection's fallbacks ───────────────────────────────────── */
static void test_fallbacks(void)
{
    /* die only (no J4 reply ever) */
    reset(1u);
    in.n908ca = 433u;
    passes(10u);
    CHECKV(S->s0fc, 6);
    CHECKV(S->s170, 2);
    CHECKV(S->s93a, 711);
    /* fluid only (J3 invalid, no J4) */
    reset(1u);
    in.n908ca = 0u;
    in.t94f98 = 574;
    in.st94fee = 8u;
    passes(10u);
    CHECKV(S->s0fc, 6);
    CHECKV(S->s170, 1);
    CHECKV(S->s93a, 574);
    in.st94fee = 4u;                /* below 6: kept as is */
    pass();
    CHECKV(S->s0fc, 4);
    CHECKV(S->s170, 1);
    in.st94fee = 2u;                /* below 3: not used */
    pass();
    CHECKV(S->s0fc, 3);
    CHECKV(S->s170, 0);
    CHECKV(S->s93a, 0x0780);
    CHECKV(S->s0ff, 0xA0);          /* 120 °C */
    printf("(5) fallbacks: die only 6/2, fluid only 6/1 (status 4 kept), none 3/0 at 0x0780\n");
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
            { 0x66AECu, 0x191706u, TSEL_CAL_TINIT, 2 },   { 0x66AF4u, 0x1919FFu, TSEL_CAL_DSTART, 1 },
            { 0x66AF0u, 0x191708u, TSEL_CAL_DHOLD, 2 },   { 0x66AF8u, 0x19170Au, TSEL_CAL_NHI, 2 },
            { 0x66AFCu, 0x19170Cu, TSEL_CAL_NLO, 2 },     { 0x66AD8u, 0x191394u, (uint16_t)TSEL_CAL_OFF0, 2 },
            { 0x66AE4u, 0x1913A0u, TSEL_CAL_C1LO0, 2 },   { 0x66AE8u, 0x1913A4u, TSEL_CAL_C1HI0, 2 },
            { 0x66ADCu, 0x191398u, TSEL_CAL_C2LO0, 2 },   { 0x66AE0u, 0x19139Cu, TSEL_CAL_C2HI0, 2 },
            { 0x66B04u, 0x191710u, TSEL_CAL_BINIT, 2 },   { 0x66B1Cu, 0x19171Cu, TSEL_CAL_BNEVER, 2 },
            { 0x66B18u, 0x19171Au, TSEL_CAL_BHOLD, 2 },   { 0x66B08u, 0x191712u, (uint16_t)TSEL_CAL_BNEAR_LO, 2 },
            { 0x66B0Cu, 0x191714u, TSEL_CAL_BNEAR_HI, 2 }, { 0x66B20u, 0x19171Eu, TSEL_CAL_BNOTRDY, 2 },
            { 0x66B00u, 0x19170Eu, TSEL_CAL_BONES, 2 },   { 0x66B10u, 0x191716u, TSEL_CAL_BMAX, 2 },
            { 0x66B14u, 0x191718u, (uint16_t)TSEL_CAL_BMIN, 2 }, { 0x66B64u, 0x191A02u, TSEL_CAL_SEL, 1 },
            { 0x66B68u, 0x19173Au, TSEL_CAL_FOFF, 2 },    { 0x66B6Cu, 0x19173Cu, TSEL_CAL_BOFF, 2 },
        };
        for (unsigned k = 0u; k < sizeof C / sizeof C[0]; k++) {
            const unsigned long p = U32(C[k].pool);
            const unsigned v = (C[k].w == 1) ? U8(C[k].cal) : U16(C[k].cal);
            if (p != C[k].cal || v != (C[k].val & 0xFFFFu)) {
                printf("FAIL cal pool 0x%05lX -> 0x%06lX (expected 0x%06lX) value 0x%X (expected 0x%X)\n",
                       C[k].pool, p, C[k].cal, v, C[k].val);
                bad++;
            }
        }
    }
    /* the second variant's entries, the scale table (a direct literal) and
     * the divisor */
    if (U16(0x191396u) != (uint16_t)TSEL_CAL_OFF1) { printf("FAIL offset 1\n"); bad++; }
    if (U32(0x8DD20u) != 0x191390ul || U16(0x191390u) != TSEL_CAL_SCALE0 || U16(0x191392u) != TSEL_CAL_SCALE1) {
        printf("FAIL scale table\n");
        bad++;
    }
    if (U32(0x8DD24u) != 0x461C4000ul) { printf("FAIL divisor\n"); bad++; }
    if (U16(0x1913A2u) != TSEL_CAL_C1LO1 || U16(0x1913A6u) != TSEL_CAL_C1HI1 ||
        U16(0x19139Au) != TSEL_CAL_C2LO1 || U16(0x19139Eu) != TSEL_CAL_C2HI1) {
        printf("FAIL comparator variant 1\n");
        bad++;
    }
    /* the variant offset word *0xFFF90AB8 is added to every pool address:
     * the shadow assumes 0 (the bench) — the movi20 #-456008 loads at
     * 0x8DB28 / 0x8DB8C / 0x88BEE (the bytes 0x07/0x05 0x90 0x0A 0xB8) */
    if (U32(0x8DB28u) != 0x07900AB8ul || U32(0x88BEEu) != 0x07900AB8ul) {
        printf("FAIL the 0x90AB8 variant-offset loads moved\n");
        bad++;
    }
    printf("(cal) %d mismatches\n", bad);
    return bad == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        return test_cal();
    }
    test_dump();
    test_cold_boot();
    test_board_lost();
    test_die();
    test_fallbacks();
    printf("%s (%d failures)\n", fails == 0 ? "PASS" : "FAIL", fails);
    return fails == 0 ? 0 : 1;
}

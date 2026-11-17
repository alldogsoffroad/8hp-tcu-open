/* park_pair.c — the safety layer's park position pair object and DTC group
 * 98, READ-ONLY (park_pair.h). Ported from the bytes of 0x13418A..0x13420E,
 * 0x141AF4..0x141B1A, 0x141B1C..0x141B32, 0x141B42..0x141BA0,
 * 0x141BA2..0x141E04, 0x141E06..0x141FC0, 0x1327B2..0x1327B8 and the slot
 * table 0x4C724..0x4C750 (sh-elf-objdump -b binary -m sh2a -EB of the
 * pristine image). ~/tcu_sweep/park_sensor_port.md §2 has the
 * write-for-write table.
 *
 * Notes from the bytes (vs the decodes):
 *   - the change flags +0x1E are bits 2 / 3 (bset.b #2 / #3 at 0x141B68 /
 *     0x141B80), i.e. values 4 / 8;
 *   - 0x141E06 reports source 0 whenever no status bit is set — the stored
 *     source from 0xA0FE8 only gates the tested flag (and keeps +0x1E);
 *     with 0xA0FE8 "nothing stored" +0x1E is cleared on every call;
 *   - the previous copies +0x1F / +0x20 are both refreshed only while A's
 *     fault byte +0x0E is 0 (0x141FA8..0x141FB8).
 */
#include "safety/park_pair.h"
#include "bsw/drv/drv_pksens.h"
#include "platform.h"

#include <stdbool.h>

#if defined(TCU_RAM_APP)
void park_pair_init(void) {}
void park_pair_tick_10ms(void) {}
void park_pair_g98_100ms(void) {}
uint8_t park_pair_take_report(void) { return 0u; }
void park_pair_witness(park_pair_wit_t *w) { (void)w; }
#else

/* ── calibration / identity [O bytes] ───────────────────────────────────── */
#define PKP_CALPTR          0x00064A1Cu  /* +0: *(0x64A28) = descriptor 0x64A24 [0] */
#define CAL_PKP_MODE        3u           /* *(u8 *)0x195DC6: heal on both contacts */
#define CAL_PKP_RELOAD      100u         /* *(u8 *)0x195DC7: both-active timer */
/* the group-33 sensor-supply fault 0x84A1F / its inverse 0x84A20 (0x7C418 /
 * 0x7C422): group 33 (0x7C618) is not ported — the stock's initial values
 * (ROM 0x65B2F / 0x65B30: 00 / FF, DTBL 0x415B0) and the driving dump's */
#define STANDIN_SUPPLY_FLT    0x00u
#define STANDIN_SUPPLY_FLT_C  0xFFu

/* the stock check sites (fatal breadcrumbs) */
#define SITE_TIMER          0x141AF4u    /* 0x141AF4: timer +4 / +8 */
#define SITE_ENTRY          0x141DE8u    /* 0x141BA2 entry: +0x18/19, +0x1A/1B */
#define SITE_INPUTS         0x141DD0u    /* 0x141BA2 tail: +0x0C..+0x15 pairs */

/* ── witnesses (ours, .pk_wit) ──────────────────────────────────────────── */
#if defined(__sh__)
#define PK_WIT __attribute__((section(".pk_wit")))
#else
#define PK_WIT
#endif
static volatile uint8_t  s_g98[4] PK_WIT;       /* source, failed, tested, - */
static volatile uint32_t s_g98_calls PK_WIT;
static volatile uint32_t s_g98_fail_n PK_WIT;
static volatile uint32_t s_g98_pass_n PK_WIT;
static volatile uint32_t s_fatal_n PK_WIT;
static volatile uint32_t s_fatal_site PK_WIT;
static volatile uint32_t s_fatal_t PK_WIT;

void park_pair_witness(park_pair_wit_t *w)
{
    w->source = s_g98[0];
    w->failed = s_g98[1];
    w->tested = s_g98[2];
    w->calls = s_g98_calls;
    w->fail_n = s_g98_fail_n;
    w->pass_n = s_g98_pass_n;
    w->fatal_n = s_fatal_n;
    w->fatal_site = s_fatal_site;
    w->fatal_t = s_fatal_t;
}

/* 0x794C8 (+0x7A914, then bra .): the fatal handler, as a counter */
static void fatal_794c8(uint32_t site)
{
    s_fatal_n = s_fatal_n + 1u;
    s_fatal_site = site;
    s_fatal_t = plat_frt_us();
}

static bool pair_ok(uint32_t v, uint32_t c)        /* movu.b v, mov.b c, not, extu.b */
{
    return CELL_PKP_U8(v) == (uint8_t)~CELL_PKP_U8(c);
}

/* 0x141AF4(obj + 4): +4 == ~+8 */
static bool timer_ok_141af4(void)
{
    if (CELL_PKP_U32(0x04u) != ~CELL_PKP_U32(0x08u)) {
        fatal_794c8(SITE_TIMER);
        return false;
    }
    return true;
}

/* ── 0x141BA2: status, position, qualifier ──────────────────────────────── */
static bool eval_141ba2(void)
{
    uint8_t st;
    const uint8_t a = CELL_PKP_U8(0x0Cu);
    const uint8_t b = CELL_PKP_U8(0x10u);

    if (!pair_ok(0x18u, 0x19u) || !pair_ok(0x1Au, 0x1Bu)) {     /* 0x141BAA..0x141BCE */
        fatal_794c8(SITE_ENTRY);
        return false;
    }
    st = CELL_PKP_STATUS;
    st = (uint8_t)((st & 0xFEu) | (CELL_PKP_U8(0x0Eu) != 0u ? 0x01u : 0u));   /* bst #0 */
    CELL_PKP_STATUS = st;
    st = (uint8_t)((st & 0xFDu) | (CELL_PKP_U8(0x12u) != 0u ? 0x02u : 0u));   /* bst #1 */
    CELL_PKP_STATUS = st;
    if (a == 0u && b == 0u) {                                     /* 0x141BF2..0x141C06 */
        st = (uint8_t)(st | 0x04u);
    } else {
        st = (uint8_t)(st & 0xFBu);
    }
    CELL_PKP_STATUS = st;
    st = (uint8_t)((st & 0xF7u) | (CELL_PKP_U8(0x14u) != 0u ? 0x08u : 0u));   /* bst #3 */
    CELL_PKP_STATUS = st;
    if (a != 0u && b != 0u) {                                     /* 0x141C1A..0x141C58 */
        if (!timer_ok_141af4()) {
            return false;
        }
        if (CELL_PKP_U32(0x04u) > 0u) {                           /* cmp/hi */
            CELL_PKP_U32(0x04u) = CELL_PKP_U32(0x04u) - 1u;
        }
        if (CELL_PKP_U32(0x08u) < 0xFFFFFFFFu) {                  /* cmp/hs #-1 */
            CELL_PKP_U32(0x08u) = CELL_PKP_U32(0x08u) + 1u;
        }
        if (!timer_ok_141af4()) {
            return false;
        }
        if (CELL_PKP_U32(0x04u) == 0u) {
            CELL_PKP_STATUS = (uint8_t)(CELL_PKP_STATUS | 0x10u); /* bset.b #4 */
        }
    } else {                                                      /* 0x141C60 */
        CELL_PKP_U32(0x04u) = CAL_PKP_RELOAD;
        CELL_PKP_U32(0x08u) = ~(uint32_t)CAL_PKP_RELOAD;
        CELL_PKP_STATUS = (uint8_t)(CELL_PKP_STATUS & 0xEFu);     /* bclr.b #4 */
    }
    if (CELL_PKP_STATUS != 0u) {                                  /* 0x141CE0 */
        CELL_PKP_QUAL = 2u;
        CELL_PKP_POS = 3u;
    } else {
        CELL_PKP_QUAL = 0u;
        if (a == 0u && b == 1u) {
            CELL_PKP_POS = 0u;
        } else if (a == 1u && b == 0u) {
            CELL_PKP_POS = 1u;
        } else {
            CELL_PKP_POS = 2u;
        }
    }
    CELL_PKP_U8(0x19u) = (uint8_t)~CELL_PKP_POS;                  /* 0x141CEC.. */
    CELL_PKP_U8(0x1Bu) = (uint8_t)~CELL_PKP_QUAL;
    CELL_PKP_U8(0x1Du) = (uint8_t)~CELL_PKP_STATUS;
    /* 0x148ACC on +0x0C..+0x15, +0x18..+0x1D (RAM byte test): dropped */
    if (!pair_ok(0x0Cu, 0x0Du) || !pair_ok(0x10u, 0x11u) ||        /* 0x141D82..0x141DCE */
        !pair_ok(0x0Eu, 0x0Fu) || !pair_ok(0x12u, 0x13u) ||
        !pair_ok(0x14u, 0x15u)) {
        fatal_794c8(SITE_INPUTS);
        return false;
    }
    return true;
}

/* ── 0x141B42 ───────────────────────────────────────────────────────────── */
static bool step_141b42(void)
{
    /* 0x148E92(0xFFF95FE8, 0x89): PFM checkpoint — dropped */
    if (!eval_141ba2()) {
        return false;
    }
    if (CELL_PKP_U8(0x0Cu) != CELL_PKP_U8(0x1Fu) && CELL_PKP_U8(0x0Eu) == 0u) {
        CELL_PKP_CHG = (uint8_t)(CELL_PKP_CHG | 0x04u);           /* bset.b #2 */
    }
    if (CELL_PKP_U8(0x10u) != CELL_PKP_U8(0x20u) && CELL_PKP_U8(0x12u) == 0u) {
        CELL_PKP_CHG = (uint8_t)(CELL_PKP_CHG | 0x08u);           /* bset.b #3 */
    }
    /* 0xFFF96050 != 0 → 0x148EB6 PFM checkpoint, clear: dropped */
    return true;
}

/* 0xA0FE8(group, &src): "a stored entry exists" (+ its source) — stub:
 * nothing stored (no fault memory is ported) */
static uint8_t stored_a0fe8(uint8_t group, uint16_t *src)
{
    (void)group;
    (void)src;
    return 0u;
}

/* 0xA0E70(group, source, failed, tested) → 0x9BE50: the witness cells */
static void report_a0e70(uint8_t group, uint8_t source, uint8_t failed, uint8_t tested)
{
    (void)group;
    s_g98[0] = source;
    s_g98[1] = failed;
    s_g98[2] = tested;
    s_g98_calls = s_g98_calls + 1u;
    if (failed != 0u) {
        s_g98_fail_n = s_g98_fail_n + 1u;
    } else if (tested != 0u) {
        s_g98_pass_n = s_g98_pass_n + 1u;
    }
}

static uint8_t and_flag(uint8_t tested, uint8_t f)              /* the r5 pattern */
{
    return (uint8_t)((tested != 0u && f != 0u) ? 1u : 0u);
}

/* ── 0x141E06: DTC group 98 ─────────────────────────────────────────────── */
static void g98_141e06(void)
{
    uint16_t src = 0u;                                            /* @r15 */
    uint8_t rsrc = 0u;                                            /* @(4) */
    uint8_t tested = (uint8_t)(CELL_PKP_U8(0x14u) == 0u ? 1u : 0u);   /* @(8) */
    const uint8_t mode = CAL_PKP_MODE;                            /* @(12) */
    const uint8_t stored = stored_a0fe8(PKP_G98, &src);           /* r4 */
    uint8_t failed = 0u;                                          /* r12 */
    const uint8_t st = CELL_PKP_STATUS;

    if ((st & 0x01u) != 0u) {                                     /* 0x141E3A */
        failed = 1u;
        rsrc = PKP_SRC_P07B2;
        tested = and_flag(tested, CELL_PKP_U8(0x16u));
    } else if ((st & 0x02u) != 0u) {                              /* 0x141E5E */
        failed = 1u;
        rsrc = PKP_SRC_P07B8;
        tested = and_flag(tested, CELL_PKP_U8(0x17u));
    } else if ((st & 0x04u) != 0u) {                              /* 0x141E84 */
        rsrc = PKP_SRC_P0850_M0;
        failed = 1u;
    } else if ((st & 0x10u) != 0u) {                              /* 0x141E90 */
        rsrc = PKP_SRC_P0850_M1;
        failed = 1u;
    }
    if (failed == 0u && stored == 1u && src == PKP_SRC_P0850_M0) {   /* 0x141E9E.. */
        const uint8_t chg = CELL_PKP_CHG;
        switch (mode) {
        case 1u: tested = and_flag(tested, (uint8_t)(chg & 0x04u)); break;
        case 2u: tested = and_flag(tested, (uint8_t)(chg & 0x08u)); break;
        case 3u: tested = and_flag(tested, (uint8_t)((chg & 0x0Cu) == 0x0Cu)); break;
        default: break;
        }
    } else {
        CELL_PKP_CHG = 0u;                                        /* 0x141F0C */
    }
    if (failed == 0u && stored == 1u && src == PKP_SRC_P07B2) {   /* 0x141F12 */
        tested = and_flag(tested, CELL_PKP_U8(0x16u));
    }
    if (failed == 0u && stored == 1u && src == PKP_SRC_P07B8) {   /* 0x141F64 */
        tested = and_flag(tested, CELL_PKP_U8(0x17u));
    }
    report_a0e70(PKP_G98, rsrc, failed, tested);                  /* 0x141F92 */
    if (CELL_PKP_U8(0x0Eu) == 0u) {                               /* 0x141FA8 */
        CELL_PKP_U8(0x1Fu) = CELL_PKP_U8(0x0Cu);
        CELL_PKP_U8(0x20u) = CELL_PKP_U8(0x10u);
    }
}

/* ── the gather 0x13418A..0x13420E, then 0x141B42; then 0x141E06 ──────── */
void park_pair_tick_10ms(void)
{
    CELL_PKP_U8(0x0Cu) = CELL_PKS_CP_ST[2];        /* 0x4C724 0x7AFF8 → 0x847F6 */
    CELL_PKP_U8(0x0Du) = CELL_PKS_CP_ST_C[2];      /* 0x4C728 0x7B00C → 0x84802 */
    CELL_PKP_U8(0x0Eu) = CELL_PKS_CP_FLT[2];       /* 0x4C72C 0x7B020 → 0x847FA */
    CELL_PKP_U8(0x0Fu) = CELL_PKS_CP_FLT_C[2];     /* 0x4C730 0x7B034 → 0x847FE */
    CELL_PKP_U8(0x10u) = CELL_PKS_CP_ST[3];        /* 0x4C734 0x7B002 → 0x847F7 */
    CELL_PKP_U8(0x11u) = CELL_PKS_CP_ST_C[3];      /* 0x4C738 0x7B016 → 0x84803 */
    CELL_PKP_U8(0x12u) = CELL_PKS_CP_FLT[3];       /* 0x4C73C 0x7B02A → 0x847FB */
    CELL_PKP_U8(0x13u) = CELL_PKS_CP_FLT_C[3];     /* 0x4C740 0x7B03E → 0x847FF */
    CELL_PKP_U8(0x14u) = STANDIN_SUPPLY_FLT;       /* 0x4C744 0x7C418 → 0x84A1F */
    CELL_PKP_U8(0x15u) = STANDIN_SUPPLY_FLT_C;     /* 0x4C748 0x7C422 → 0x84A20 */
    CELL_PKP_U8(0x16u) = CELL_PKS_MTESTED[2];      /* 0x4C74C 0x7B048 → 0x92FE6 */
    CELL_PKP_U8(0x17u) = CELL_PKS_MTESTED[3];      /* 0x4C750 0x7B052 → 0x92FE7 */
    CELL_PKP_U8(0x0Du) = (uint8_t)(CELL_PKP_U8(0x0Du) | 0xFEu);   /* 0x1341FC */
    CELL_PKP_U8(0x11u) = (uint8_t)(CELL_PKP_U8(0x11u) | 0xFEu);   /* 0x134202 */
    if (!step_141b42()) {
        return;                                    /* the stock hangs here */
    }
    /* 0x141E06 is NOT here: the stock runs it in the 100 ms safety slice
     * (0x13204A → 0x14F7EA, modes 1..3, never mode 0) —
     * park_pair_g98_100ms(); ~/tcu_sweep/park_chain_loose_ends_decode.md §10 */
}

/* 0x14F7EA → 0x141E06, from the 100 ms slot of modes 1..3 (app/tmp_chain.c).
 * Returns 1: a report was made this call (the range chain's fault model takes
 * it on its next pass). */
static volatile uint8_t s_g98_new;
void park_pair_g98_100ms(void)
{
    g98_141e06();
    s_g98_new = 1u;
}

uint8_t park_pair_take_report(void)
{
    const uint8_t n = s_g98_new;
    s_g98_new = 0u;
    return n;
}

/* ── init: 0x1327B2 (+0), 0x141B1C, the timer pair ──────────────────────── */
void park_pair_init(void)
{
    for (uint32_t o = 0u; o < 0x24u; o++) {        /* .pkp_obj (stock RAM 0) */
        CELL_PKP_U8(o) = 0u;
    }
    CELL_PKP_U32(0x00u) = PKP_CALPTR;              /* 0x1327B8 (the stock value; unused) */
    CELL_PKP_U32(0x04u) = CAL_PKP_RELOAD;          /* the timer pair (task brief) */
    CELL_PKP_U32(0x08u) = ~(uint32_t)CAL_PKP_RELOAD;
    CELL_PKP_U8(0x0Du) = 0xFFu;                    /* 0x141B1C..0x141B32 */
    CELL_PKP_U8(0x11u) = 0xFFu;
    CELL_PKP_U8(0x0Fu) = 0xFFu;
    CELL_PKP_U8(0x13u) = 0xFFu;
    CELL_PKP_U8(0x15u) = 0xFFu;
    CELL_PKP_U8(0x1Du) = 0xFFu;
    CELL_PKP_U8(0x19u) = 0xFFu;
    CELL_PKP_U8(0x1Bu) = 0xFFu;
    for (uint32_t i = 0u; i < 4u; i++) {
        s_g98[i] = 0u;
    }
    s_g98_calls = 0u;
    s_g98_fail_n = 0u;
    s_g98_pass_n = 0u;
    s_fatal_n = 0u;
    s_fatal_site = 0u;
    s_fatal_t = 0u;
}
#endif /* !TCU_RAM_APP */

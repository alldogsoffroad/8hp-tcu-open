/* sup_ladder.c — L1 (2026-11-13): the stock supervisor-chip self-test
 * ladder, ported 1:1 from the September image's bytes and HELD short of
 * any rail command.
 *
 * Decodes: ~/tcu_sweep/ladder_blockers_decode.md (P1-P4, §5 the L1 recipe),
 * letters_law_decode.md (the ladder / request / seq2 law), gate_bit1_decode.md
 * (0x14 bit 1 = walk level <= 4 with the TIOC clock present). Report:
 * ~/tcu_sweep/l1_port.md. Every function below was ported from the Ghidra
 * text and checked against the bytes (sh-elf-objdump -b binary -m sh2a -EB)
 * wherever the text was ambiguous — notably the per-channel bit (the
 * decompiler's "ch == 0 → bit 0" is a shld artifact: channel 0's bit is 1),
 * seq2's state branches (0x85996..0x85C20) and 0x8D8DE's compares.
 *
 * Stock map:
 *   0x7C7E4 countdown service     0x9181C 0x14 re-derive
 *   0x8D8DE supply plausibility   0x89114 output/supply publisher
 *   0x85678 sequencer init (+0x8556E groups, 0x863D8, 0x85600)
 *   0x8570E / 0x85160 mode sequencer (states 0 / '.' / 'L' / '=' / 0x1F)
 *   0x8B7A8 ladder dispatcher; phases 0x8B974 (1), 0x8BB34 (2), 0x8BB8A (4),
 *     0x8BC54 (8), 0x8BD40 (0x10), 0x8BDD6 (0x20), 0x8BEC0 (0x40),
 *     0x8BFB8 (0x80), 0x8C0F2 (0x100 / fault latch), 0x8C194 (0x200)
 *   0x8C250 / 0x8C2A4 channel checks, 0x85DAC → 0x8C6D4 voltage class
 *   0x85E64 request writer, 0x85600 / 0x85E00 group state writers
 *   0x85828 seq2 (0x8C7A8 = mon2_chan_enable in sup_letters.c)
 *   0x85788 pin-cut driver (0x91E6C / 0x91E4E = plat_tioc_cut/restore,
 *     0x8C34C verdict)
 *
 * THE L1 HOLDS (sup_ladder.h; each a compile-time constant):
 *   H1 SUP_L1_HOLD_PHASE8_REQUEST — phase 8 'U' skips 0x85E64(ch, 1) (the
 *      stock's one-frame 0x9020 shut-off test at level >= 5). The phase's
 *      class check and pass still run; its 0x85E64(ch, 0) still runs.
 *   H2 SUP_L1_HOLD_PHASE40 — the dispatcher returns when the phase word
 *      reads 0x40, at the top of its loop (before the supply window and
 *      the dispatch). This must be INSIDE 0x8B7A8's
 *      loop, not at its call: the loop repeats while the sub-state is 'U',
 *      so phase 0x20's pass and phase 0x40's 'U' entry (0x85E64(ch, 1) —
 *      the first rail-on) run in the SAME call. Holding the call alone
 *      would let that request through on the tick the cut test passes.
 *      Parked there, nothing in the ladder runs again (no 200 ms timeout,
 *      no retry), so the cells hold the at-hold state for the witness.
 *   H3 SUP_L1_HOLD_FAIL_FSM — the sequencer's 0x87766 calls (0x23 after
 *      0xAA; 0x12/0x23 on retry exhaustion) are recorded in s_fsm_req_held
 *      and not made: the walk FSM stays 8 (its 0x23/'E'/0x34 states and the
 *      0x8CEDC 'E' rule are not ported). Every other cell write of the fail
 *      path runs (0x92094(1) strip latch, module 11 = 0x0F, groups → 0x22).
 * Because 0x85E64 is the request cells' only writer and only H1/H2's calls
 * ever pass 1, every request cell stays 0, seq2 (state 0x11) follows it
 * with enable 0, every letter stays 0 and the J2 word stays 0x9000 in
 * every reachable state (tests/unit/test_ladder.c drives them all).
 *
 * STAND-INS (named deviations, each [I] or bench-forced):
 *   S1 0x6F594 (fault memory ready) = 1. Its readers are 0x85740's DTC
 *      report and the '3' diagnosis — neither is ported, so it has no
 *      effect in L1.
 *   S2 0x94FCA[0] (the application's output demand, model block 0x132340)
 *      = 1, 0x94FCA[1] = 0 (dump: 01 00). Read by seq2 only outside 0x11.
 *   S3 0x7BBB4 (output driver fault) = 1 ("no fault": seq2 treats 0 and 1
 *      alike, only 2 is a fault).
 *   S4 0x84A2B (read by 'L' against 0xA5) = 0: an unreserved cell in our
 *      map (never cleared), so a constant instead of a RAM read.
 *   The chip-ID check 0x91A9C is PORTED, not stood in: it returns 0 only
 *   for 0x907CF == 0xBC with 0x907D0 < 0x10; our J5 handler fills both
 *   (bench: 0x907D0 = 0x14) → 1.
 *   S5 THE START GATE. The stock runs 0x85160 only while startup module 11
 *      (0x93B85) != 0; the module manager 0x7DAA4 sets it to 1 once module
 *      3 is up — within the OS's first passes, with the AN0 chain running
 *      since the init batch. Our chain starts at app ~500 ms (the r02l
 *      deferred Timer G start) and its windows fill over the next tens of
 *      ms. Run before then the ladder would abort at the supply window
 *      (0x94F96 = 0 < 6500 → phase 0x200, sub 0x40 → every group 0x16,
 *      module 11 = 0x0F — a run that never retries). So module 11 is set
 *      to 1 here, once, when plat_adcmux_ready() first reports >= 20 DEI5
 *      passes since the start: the same "module 11 ≠ 0" condition, reached
 *      later. Everything before it (init at boot, seq2 / pin cut / publishers
 *      every tick) runs from boot, as in the stock.
 *
 * NOT PORTED (unreachable under the holds, or reporting only):
 *   the '3' switch-on diagnosis 0x85FC4 / 0x86838 / 0x865E0 (seq2 reaches
 *   '3' only from sequencer 'L', which needs FSM 'E' — H3); the per-channel
 *   DTC diagnosis 0x85ED4 and the diverse twins 0x8C3BC / 0x8C670; the DTC
 *   reports 0x85740 → 0x7ADD4, 0x88AE0's 0x8D566, 0x8C34C's 0x7AEAC, the
 *   pair-check faults of 0x6BDCA/0x6BDFE (0x221) and the 0x226 loop checks;
 *   0x7ADBC(0x3FF) in 0x85678; the other members of the same list entries
 *   (0x7C884, 0x7CB6E, 0x7AB48, 0x7CBA8, 0x7CC1E, 0x86A80 …); the profiling
 *   hooks 0x72AE0; the post-ladder periodic cut request 0x94152 (writer not
 *   located; 0x94153 = 0x33 only after 0xAA).
 */
#include "bsw/sup/sup_ladder.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_letters.h"
#include "platform.h"

#if !SUP_L1_HOLD_PHASE40 || !SUP_L1_HOLD_FAIL_FSM
#error "L1 holds released: FSM 0x23/'E'/0x34 (0x873F0/0x87766/0x8CEDC) and the '3' diagnosis 0x85FC4 are not ported"
#endif

/* ── calibration (flash, via the 0x66xxx pointer tables at index 0x90AB8 =
 *    0; every value read from the September image) ────────────────────── */
#define CAL_CHMASK      0x01u      /* 0x1919C8: channel 0 only */
static const uint8_t  CAL_CHTYPE[2] = { 2u, 1u };          /* 0x1915A4: groups */
#define CAL_RETRIES     3u         /* 0x1919F5 → 0x9414D */
#define CAL_SKIP        0x00u      /* 0x1919F6 (0xC3 = skip the ladder) */
#define CAL_TOL_BIT1    3u         /* 0x1919F7 → 0x9481F (15 ms) */
#define CAL_TOL_LADDER  0x28u      /* 0x1919F8 → 0x94820 (200 ms) */
#define CAL_TOL_OUT     6u         /* 0x1919F9 → 0x94821 (30 ms) */
#define CAL_SUP_LO      6500u      /* 0x1916E2: supply window */
#define CAL_SUP_HI      23500u     /* 0x1916E0 */
#define CAL_LOOP_STOP   0x0000u    /* 0x1916DE (0xA5 stops the 'U' loop) */
#define CAL_MIN13_OK    6000u      /* 0x1916DC: 'L' / '=' 0x907FC excuse */
static const uint16_t CAL_C0[2] = { 1000u, 1000u };        /* 0x1912D0 */
static const uint16_t CAL_C1[2] = { 1500u, 1500u };        /* 0x1912CC */
static const uint16_t CAL_C2[2] = { 4000u, 4000u };        /* 0x1912C8 */
#define CAL_C3          1000u      /* 0x1916E4: |v - supply| ON band */
static const uint16_t CAL_C4[2] = { 900u, 900u };          /* 0x1912C4: partial, permil */
/* seq2 */
#define CAL_S2_LOW      6500u      /* 0x1915B2: supply low */
#define CAL_S2_REL      1000u      /* 0x1915B0: release offset */
static const uint8_t  CAL_S2_LOWHOLD[2] = { 0x14u, 0x14u }; /* 0x1915A8 */
static const uint8_t  CAL_S2_SETTLE_U[2] = { 6u, 6u };      /* 0x1915AC */
static const uint8_t  CAL_S2_SETTLE[2]   = { 6u, 6u };      /* 0x1915AA */
/* 0x8D8DE */
#define CAL_P_HI        23500u     /* 0x191704 */
#define CAL_P_LO        4000u      /* 0x1916FC */
#define CAL_P_LOWCNT    4u         /* 0x1916F6 → 0x94774 */
#define CAL_P_RIP       1000u      /* 0x1916FE */
#define CAL_P_RIPCNT    10u        /* 0x1916FA → 0x94776 */
#define CAL_P_RIPCNT2   100u       /* 0x1916F8 → 0x94778 */
#define CAL_P_OFS       200        /* 0x191700 */
#define CAL_P_DIFF      1600u      /* 0x191702 */
#define CAL_P_DIFFCNT   5u         /* 0x1916F4 → 0x94772 */
/* 0x89114 */
#define CAL_O_MASK      0x03u      /* 0x19173E */
#define CAL_O_HI        23750u     /* 0x191740 */
#define CAL_O_SRC       0x00u      /* 0x191A05 (1 = take 0x94F9E) */

/* ── stand-ins (header S1-S4) ───────────────────────────────────────────── */
#define STANDIN_FAULTMEM_READY 1u  /* S1: 0x6F594 */
#define STANDIN_DRV_FAULT      1u  /* S3: 0x7BBB4 → 1 = no fault */
#define STANDIN_84A2B          0u  /* S4 */

static volatile uint8_t s_fsm_req_held;  /* H3 witness (nm: s_fsm_req_held) */

/* ── helpers: the stock's pair writers and the task lock ───────────────── */

static void pair8(volatile uint8_t *v, volatile uint8_t *c, uint8_t x)      /* 0x6BD68 */
{
    *v = x;
    *c = (uint8_t)~x;
}

static void pair16(volatile uint16_t *v, volatile uint16_t *c, uint16_t x) /* 0x6BD6E */
{
    *v = x;
    *c = (uint16_t)~x;
}

static void sub_set(uint8_t x)      { pair8(&CELL_LAD_SUB, &CELL_LAD_SUB_C, x); }
static void phase_set(uint16_t x)   { pair16(&CELL_LAD_PHASE, &CELL_LAD_PHASE_C, x); }
static void seq_set(uint8_t x)      { pair8(&CELL_SEQ_STATE, &CELL_SEQ_STATE_C, x); }
static void grp_set(uint8_t g, uint16_t x) { pair16(&CELL_GRP(g), &CELL_GRP_C(g), x); }

/* 0x9A428 / 0x9A3FC: raise SR.I to at least 1 (cal 0x41174 = 0x10) */
static uint32_t lock(void)
{
    const uint32_t prev = plat_sr_i();

    if (prev < 1u) {
        plat_sr_set(1u);
    }
    return prev;
}

static void unlock(uint32_t prev)
{
    plat_sr_set(prev);
}

static uint8_t chbit(uint8_t ch)
{
    return (uint8_t)(1u << ch);
}

/* the 0x87766 call site, HELD (H3) */
static void fsm_request(uint8_t t)
{
#if SUP_L1_HOLD_FAIL_FSM
    s_fsm_req_held = t;
#endif
}

uint8_t sup_ladder_fsm_req_held(void)
{
    return s_fsm_req_held;
}

/* ── the 5 ms countdown service (0x7C7E4, bytes 0x7C7E4..0x7C83C) ──────── */

static void countdown_5ms(void)
{
    const uint32_t lk = lock();
    uintptr_t a;

    for (a = CNT_W_LO; a < CNT_W_HI; a += 2u) {
        volatile uint16_t *w = (volatile uint16_t *)a;
        if (*w != 0u) {
            *w = (uint16_t)(*w - 1u);
        }
    }
    for (a = CNT_B_LO; a < CNT_B_HI; a++) {
        volatile uint8_t *b = (volatile uint8_t *)a;
        if (*b != 0u) {
            *b = (uint8_t)(*b - 1u);
        }
    }
    unlock(lk);
}

/* ── 0x9181C: re-derive the 0x14 bools (slot table 0x1918E8 = 00 01 02) ── */

static void rederive_14(void)
{
    const uint8_t r = CELL_REG_14;

    CELL_OUT_FAULTS[0] = (r & 0x20u) != 0u ? 1u : 0u;
    CELL_OUT_FAULTS[1] = (r & 0x40u) != 0u ? 1u : 0u;
    CELL_OUT_FAULTS[2] = (r & 0x80u) != 0u ? 1u : 0u;
    CELL_SSLB0_BIT2 = (r & 0x02u) != 0u ? 1u : 0u;
}

void sup_ladder_tick_head(void)
{
    countdown_5ms();                    /* list entry 1 (0x44CD0)            */
    rederive_14();                      /* entry 3 (0x44CD8) = 0x8EDEA tail  */
}

/* ── 0x8D8DE: supply plausibility → 0x94F86 / status 0x94FEC ───────────── */

static void supply_plaus(void)
{
    const int16_t s16 = (int16_t)CELL_MUX11_MV;          /* mov.w: r14 */
    const uint32_t su = (uint16_t)s16;                   /* extu.w: r11 */
    uint16_t f;
    int32_t v;

    CELL_SUP_M13 = CELL_MUX13_MV;
    CELL_SUP_MIN13 = CELL_MUX13_MIN;
    /* r13: cmp/hs on sign-extended words (0x8D90E) */
    f = ((uint32_t)(int32_t)(int16_t)CAL_P_HI >= (uint32_t)(int32_t)s16) ? 0u : 0x08u;
    if ((int32_t)CAL_P_LO > (int32_t)su) {               /* 0x8D922 cmp/gt */
        f |= 0x10u;
        if (CELL_CNT_SUP_LOW == 0u) {
            f |= 0x20u;
        }
    } else {
        CELL_CNT_SUP_LOW = CAL_P_LOWCNT;
    }
    if ((f & 0x18u) == 0u) {
        uint32_t env = CELL_MUX13_ENV;
        if (env >= 0x7FFFu) {
            env = 0x7FFFu;
        }
        CELL_SUP_ENV = (uint16_t)env;
        if (!((int32_t)CAL_P_RIP > (int32_t)(uint16_t)env)) {
            CELL_SUP_940FA = 1u;
            CELL_CNT_SUP_RIP = CAL_P_RIPCNT;
            CELL_SUP_9415B = 1u;
            CELL_CNT_SUP_RIP2 = CAL_P_RIPCNT2;
            f |= 0x40u;
        }
    } else {
        CELL_SUP_ENV = 0u;
    }
    if (CELL_CNT_SUP_RIP == 0u) {
        CELL_SUP_940FA = 0u;
    } else {
        f |= 0x80u;
    }
    if (CELL_CNT_SUP_RIP2 == 0u) {
        CELL_SUP_9415B = 0u;
    }
    {
        bool reload = true;
        if ((f & 0x18u) == 0u) {
            const int32_t d = (int32_t)su - (int32_t)CELL_MUX13_MV - CAL_P_OFS;
            const int16_t d16 = (int16_t)d;
            const uint16_t ad = (uint16_t)(d16 >= 0 ? d16 : -d16);
            CELL_SUP_DIFF = ad;
            if ((int32_t)ad >= (int32_t)CAL_P_DIFF) {
                f |= 0x01u;
                if (CELL_CNT_SUP_DIFF == 0u) {
                    f |= 0x02u;
                }
                reload = false;
            }
        }
        if (reload) {
            CELL_CNT_SUP_DIFF = CAL_P_DIFFCNT;
        }
    }
    v = s16;
    if ((f & 0x28u) != 0u) {
        const int32_t alt = (int32_t)CELL_SUP_M13 + CAL_P_OFS;
        if (alt > (int32_t)su) {
            v = alt;
        }
        pair8(&CELL_SUPPLY_ST, &CELL_SUPPLY_ST_C, 7u);
    } else if ((f & 0x02u) != 0u) {
        v = CAL_P_OFS + (int32_t)CELL_SUP_M13;
        pair8(&CELL_SUPPLY_ST, &CELL_SUPPLY_ST_C, 6u);
    } else {
        pair8(&CELL_SUPPLY_ST, &CELL_SUPPLY_ST_C, 8u);
    }
    {
        const uint32_t vu = (uint16_t)v;                 /* extu.w, clip 0x7FFF */
        const uint16_t o = (uint16_t)(vu >= 0x7FFFu ? 0x7FFFu : vu);
        pair16(&CELL_SUPPLY_RAW, &CELL_SUPPLY_RAW_C, o);
    }
    CELL_SUP_FLAGS = f;
}

/* ── 0x89114: output node + supply publisher ───────────────────────────── */

static void publish_out(void)
{
    uint8_t ch;

    for (ch = 0u; ch < 2u; ch++) {
        const uint16_t v = CELL_MUX_MV[ch];              /* 0x90188 / 0x9018A */
        uint32_t b;
        uint8_t st;

        pair16(&CELL_OUT_MV[ch], &CELL_OUT_MV_C[ch], v);
        b = (uint32_t)((float)v * 0.01f);                /* float 0x89304 */
        if ((b & 0xFFFFu) > 0xFFu) {
            b = 0xFFu;
        }
        CELL_OUT_BYTE[ch] = (uint8_t)b;
        if (((CAL_O_MASK >> ch) & 1u) == 0u) {
            st = 1u;
        } else if (v < CAL_O_HI) {
            st = 8u;
        } else {
            st = 7u;
        }
        pair8(&CELL_OUT_ST[ch], &CELL_OUT_ST_C[ch], st);
    }
#if CAL_O_SRC == 1
#error "0x191A05 = 1 (0x94F9E source) not ported"
#endif
    pair16(&CELL_SUPPLY, &CELL_SUPPLY_C, CELL_SUPPLY_RAW);
    pair8(&CELL_SUPPLY_ST2, &CELL_SUPPLY_ST2_C, CELL_SUPPLY_ST);
}

/* ── the voltage class 0x8C6D4 (via 0x85DAC) ───────────────────────────── */

static uint8_t classify(uint8_t ch)
{
    const uint32_t v = CELL_OUT_MV[ch];
    uint8_t c;

    if (!(CAL_C0[ch] < v)) {
        c = 0x11u;
    } else if (v < CAL_C1[ch]) {
        c = 0x44u;
    } else if (v < CAL_C1[ch] || CAL_C2[ch] < v) {
        const float fs = (float)CELL_SUPPLY;
        const uint32_t s = (uint32_t)(int32_t)fs & 0xFFFFu;
        const uint32_t hi = v < s ? s : v;
        const uint32_t lo = v < s ? v : s;
        const uint32_t d = hi - lo;                      /* 0x88750, >= 0 */

        if (CAL_C3 < d) {
            c = 0x33u;
            if (v < ((uint32_t)(int32_t)(((float)CAL_C4[ch] * fs) / 1000.0f) & 0xFFFFu)) {
                c = 0x55u;
            }
        } else {
            c = 0x66u;
        }
    } else {
        c = 0x22u;
    }
    CELL_OUT_CLASS[ch] = c;
    return c;
}

/* ── channel checks 0x8C250 / 0x8C2A4 ───────────────────────────────────── */

static void chan_check(uint8_t x, bool want)                 /* 0x8C250 */
{
    uint8_t ch;

    for (ch = 0u; ch < 2u; ch++) {
        const uint8_t c = classify(ch);                      /* 0x85DAC */
        const bool match = (c == x) || (x == 0x33u && c == 0x66u);

        CELL_LAD_MISMATCH[ch] = (match != want) ? c : 0u;
    }
}

static uint8_t chan_resolve(bool wait)                       /* 0x8C2A4 */
{
    uint16_t m = CELL_GRP(3u);
    uint8_t r = 0u;
    uint8_t ch;

    for (ch = 0u; ch < 2u; ch++) {
        if (CELL_LAD_MISMATCH[ch] != 0u && (m & chbit(ch)) != 0u) {
            if (wait && CELL_CNT_OUTTOL != 0u) {
                r = 10u;
            } else {
                m = (uint16_t)(m & (uint16_t)~chbit(ch));
                grp_set(3u, m);
                CELL_LAD_FAULT = (uint16_t)(CELL_LAD_FAULT | CELL_LAD_PHASE);
                CELL_LAD_FAILCLS[ch] = CELL_LAD_MISMATCH[ch];
                if (m == 0u) {
                    r = 0x0Fu;
                }
            }
        }
    }
    return r;
}

/* ── the state writers 0x85E64 / 0x85E00 / 0x85600 ─────────────────────── */

static void req_write(uint8_t ch, uint8_t v)                 /* 0x85E64 */
{
    /* THE ONLY WRITER OF THE REQUEST CELLS. The stock lands the write only
     * while seq2 holds the channel in 0x11 / 0x16. */
    if (CELL_SEQ2_STATE[ch] == 0x11u || CELL_SEQ2_STATE[ch] == 0x16u) {
        CELL_LAD_REQ[ch] = v;
    }
}

static void seq2_code(uint8_t ch, uint8_t code)              /* 0x85E00 */
{
    if (ch < 2u &&
        (code == 0x11u || code == 0x16u || code == 0x33u || code == 0x22u)) {
        const uint8_t i = ch != 0u ? 1u : 0u;
        pair8(&CELL_SEQ2_STATE[i], &CELL_SEQ2_STATE_C[i], code);
        if (code == 0x22u) {
            CELL_CNT_SETTLE[i] = CAL_S2_SETTLE[i];
        }
    }
}

static void grp_code(uint8_t g, uint8_t code)                /* 0x85600 */
{
    const uint16_t m = CELL_GRP(g);
    uint8_t ch;

    for (ch = 0u; ch < 2u; ch++) {
        if ((CAL_CHMASK & chbit(ch)) != 0u && ((m >> ch) & 1u) != 0u) {
            seq2_code(ch, code);
        }
    }
}

/* the "request every channel of the alive mask" loops of phases 8 / 0x40 /
 * 0x80 (0x94F72 >> ch & 1 → 0x85E64(ch, v)) */
static void req_alive(uint8_t v)
{
    uint8_t ch;

    for (ch = 0u; ch < 2u; ch++) {
        if (((CELL_GRP(3u) >> ch) & 1u) != 0u) {
            req_write(ch, v);
        }
    }
}

/* ── the ladder phases ─────────────────────────────────────────────────── */

static void ph_1(void)                                       /* 0x8B974 */
{
    const bool reached = sup_walk_target_reached(5u);        /* before the set */
    const uint8_t sub = CELL_LAD_SUB;

    if (sub == 0u) {
        uint32_t lk = lock();
        uint16_t m;
        uint8_t ch;

        sup_walk_target_set(5u);
        unlock(lk);
        CELL_LAD_PASS = 0u;
        CELL_CNT_LADDER = CAL_TOL_LADDER;
        for (ch = 0u; ch < 2u; ch++) {
            req_write(ch, 0u);
        }
        CELL_LAD_94114 = 0u;
        if (!reached) {
            sub_set(0x31u);
        } else {
            sub_set(0x55u);
            CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
            CELL_CNT_BIT1 = CAL_TOL_BIT1;
        }
        m = (uint16_t)(CELL_GRP(2u) | CELL_GRP(1u));
        grp_set(3u, m);
        if (m == 0u) {
            grp_code(0u, 0x33u);
            sub_set(0xAAu);
        }
        return;
    }
    if (sub == 0x31u) {
        if (reached) {
            sub_set(0x55u);
            CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
            CELL_CNT_BIT1 = CAL_TOL_BIT1;
            return;
        }
        if (sup_walk_budget() < 2u) {
            sub_set(0xF1u);
            grp_code(0u, 0x33u);
            return;
        }
        sub_set(0x31u);
        return;
    }
    sub_set(0xF0u);
}

static void ph_2(void)                                       /* 0x8BB34 */
{
    if (CELL_SSLB0_BIT2 == 0u) {
        CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
        CELL_LAD_SUB = 0x55u;
        CELL_LAD_SUB_C = 0xAAu;
    } else if (CELL_CNT_BIT1 == 0u) {
        sub_set(0xF4u);
        grp_code(0u, 0x33u);
    } else {
        CELL_LAD_SUB = 0x33u;
        CELL_LAD_SUB_C = 0xCCu;
    }
}

static void ph_4(void)                                       /* 0x8BB8A */
{
    if (CELL_LAD_SUB == 0x55u) {
        uint16_t m = CELL_GRP(0u);
        uint8_t ch;

        chan_check(0x22u, true);
        for (ch = 0u; ch < 2u; ch++) {
            if (CELL_LAD_MISMATCH[ch] != 0u) {
                m = (uint16_t)(m & (uint16_t)~chbit(ch));
            }
        }
        CELL_LAD_94114 = (uint8_t)m;
        sub_set(0x30u);
        return;
    }
    chan_check(0x22u, true);
    {
        const uint8_t r = chan_resolve(false);
        if (r == 0u) {
            sub_set(0x55u);
            CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
        } else if (r != 0x0Fu) {
            sub_set(0x34u);
            return;
        } else {
            sub_set(0xF2u);
        }
    }
    CELL_LAD_94114 = 0u;
    grp_code(0u, 0x33u);
}

static void ph_8(void)                                       /* 0x8BC54 */
{
    if (CELL_LAD_SUB == 0x55u) {
#if !SUP_L1_HOLD_PHASE8_REQUEST
        req_alive(1u);                  /* 0x8BC6E..0x8BCA0: mov #1,r5 */
#endif
        sub_set(0x34u);
        return;
    }
    chan_check(0x22u, true);
    if (chan_resolve(false) == 0u) {
        sub_set(0x55u);
        CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
        req_alive(0u);                  /* 0x8BD1C: mov #0,r5 */
        return;
    }
    sub_set(0xF2u);
}

static void ph_10(void)                                      /* 0x8BD40 */
{
    if (CELL_LAD_SUB == 0x55u) {
        CELL_LAD_94114 = (uint8_t)CELL_GRP(3u);
        sub_set(0x34u);
        return;
    }
    chan_check(0x22u, true);
    if (chan_resolve(false) == 0u) {
        uint32_t lk;

        CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
        sub_set(0x55u);
        CELL_LAD_94114 = 0u;
        lk = lock();
        sup_walk_target_set(3u);        /* 0x8BDB8: mov #3,r4 */
        unlock(lk);
        return;
    }
    sub_set(0xF3u);
}

/* 0x91A9C: the chip-ID check (0x907CF / 0x907D0 from the J5 handler) */
static bool chip_id_ok(void)
{
    return !(CELL_ID_B1_00 == 0xBCu && CELL_ID_B1_06 < 0x10u);
}

static void ph_20(void)                                      /* 0x8BDD6 */
{
    const uint8_t sub = CELL_LAD_SUB;

    if (sub == 0x55u || sub == 0x31u) {
        if (!sup_walk_target_reached(3u)) {
            if (sup_walk_budget() < 2u) {
                sub_set(0xF1u);
                return;
            }
        } else if (CELL_SSLB0_BIT2 != 0u) {
            if (chip_id_ok()) {
                sub_set(0x33u);
                CELL_CUT_REQ = 0xAAu;   /* arm the one-tick TIOC cut */
                CELL_CNT_BIT1 = CAL_TOL_BIT1;
                return;
            }
            sub_set(0x55u);             /* LAB_0008BE8E: no cut on this chip */
            CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
            return;
        }
        sub_set(0x31u);
        return;
    }
    if (sub == 0x33u) {
        if (CELL_SSLB0_BIT2 == 0u) {    /* bit 1 dropped under the cut */
            sub_set(0x55u);
            CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
            return;
        }
        if (CELL_CNT_BIT1 != 0u) {
            return;
        }
        sub_set(0xF4u);
        return;
    }
    sub_set(0xF0u);
}

/* Phases 0x40 and 0x80 are ported for completeness; under H2 the dispatcher
 * never reaches them (0x80 is reachable only from 0x40). */
static void ph_40(void)                                      /* 0x8BEC0 */
{
    const uint8_t sub = CELL_LAD_SUB;

    if (sub == 0x55u) {
#if !SUP_L1_HOLD_PHASE40
        req_alive(1u);                  /* 0x8BEFA: mov #1,r5 — THE RAIL-ON */
#endif
        CELL_CNT_OUTTOL = CAL_TOL_OUT;
        sub_set(0x34u);
        return;
    }
    if (sub != 0x34u) {
        sub_set(0xF0u);
        return;
    }
    chan_check(0x33u, true);
    {
        const uint8_t r = chan_resolve(true);
        if (r == 0u) {
            CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
            sub_set(0x55u);
        } else if (r == 0x0Fu) {
            sub_set(0xF2u);
        } else {
            sub_set(0x34u);
        }
    }
}

static void ph_80(void)                                      /* 0x8BFB8 */
{
    const uint8_t sub = CELL_LAD_SUB;
    uint32_t lk;

    if (sub == 0x55u) {
        lk = lock();
        sup_walk_target_set(5u);
        unlock(lk);
        sub_set(0x31u);
        return;
    }
    if (sub == 0x31u) {
        if (!sup_walk_target_reached(5u)) {
            if (sup_walk_budget() > 1u) {
                return;
            }
            sub_set(0xF1u);
            return;
        }
        CELL_CNT_OUTTOL = CAL_TOL_OUT;
        sub_set(0x34u);
        return;
    }
    if (sub != 0x34u) {
        sub_set(0xF0u);
        return;
    }
    chan_check(0x66u, false);
    {
        const uint8_t r = chan_resolve(true);
        if (r == 0u) {
            sub_set(0x55u);
            CELL_LAD_PASS = (uint16_t)(CELL_LAD_PASS | CELL_LAD_PHASE);
            CELL_LAD_94114 = 0u;
            req_alive(0u);              /* 0x8C088 */
            CELL_CNT_OUTTOL = CAL_TOL_OUT;
            lk = lock();
            sup_walk_target_set(0u);    /* 0x8C0B8 */
            unlock(lk);
        } else if (r == 0x0Fu) {
            sub_set(0xF2u);
        } else {
            sub_set(0x34u);
        }
    }
}

static void ph_100(void)                                     /* 0x8C0F2 */
{
    const uint8_t sub = CELL_LAD_SUB;

    if (sub < 0xF0u) {
        uint8_t r;
        chan_check(0x22u, true);
        r = chan_resolve(true);
        if (r == 0u) {
            if (CELL_LAD_PASS == 0xFFu) {
                CELL_LAD_OKFLAGS = CELL_LAD_PASS;
                sub_set(0xAAu);
            } else {
                sub_set(0xF0u);
            }
        } else if (r == 0x0Fu) {
            sub_set(0xF2u);
        } else {
            sub_set(0x30u);
        }
    } else {
        /* the fault latch (also reached from the dispatcher on sub >= 0xF0) */
        CELL_LAD_FAULT = (uint16_t)(CELL_LAD_FAULT | (uint16_t)(CELL_LAD_PASS + 1u));
        CELL_LAD_OKFLAGS = (uint16_t)(CELL_LAD_PASS * 2u + 1u);
    }
}

static void ph_200(void)                                     /* 0x8C194 */
{
    const uint16_t s = CELL_SUPPLY;

    CELL_CNT_LADDER = CELL_LAD_SAVED_820;
    CELL_CNT_OUTTOL = CELL_LAD_SAVED_821;
    if (CAL_SUP_LO < s && s < CAL_SUP_HI) {
        phase_set(CELL_LAD_SAVED_PH);
        sub_set(CELL_LAD_SAVED_SUB);
    } else {
        sub_set(0x40u);
    }
}

/* ── the dispatcher 0x8B7A8 ────────────────────────────────────────────── */

static void ladder(void)
{
    do {
        const uint16_t s = CELL_SUPPLY;
        uint16_t ph;
        uint8_t sub;

#if SUP_L1_HOLD_PHASE40
        /* H2: park BEFORE phase 0x40 runs — checked at the top of every
         * pass of the 'U' loop, so phase 0x20's pass cannot fall through
         * into 0x40's request write in the same call; and ahead of the
         * supply window, so a parked ladder is frozen whole (no 0x200 save,
         * no 0x40 abort, no 0xF6 timeout) */
        if (CELL_LAD_PHASE == 0x40u) {
            return;
        }
#endif
        if (s < CAL_SUP_LO || CAL_SUP_HI < s) {      /* the supply window */
            ph = CELL_LAD_PHASE;
            sub = CELL_LAD_SUB;
            if (ph != 0x200u) {
                CELL_LAD_SAVED_821 = CELL_CNT_OUTTOL;
                CELL_LAD_SAVED_820 = CELL_CNT_LADDER;
                CELL_LAD_SAVED_PH = ph;
                CELL_LAD_SAVED_SUB = sub;
                phase_set(0x200u);
            }
        }
        ph = CELL_LAD_PHASE;
        switch (ph) {
        case 0x001u: ph_1();   break;
        case 0x002u: ph_2();   break;
        case 0x004u: ph_4();   break;
        case 0x008u: ph_8();   break;
        case 0x010u: ph_10();  break;
        case 0x020u: ph_20();  break;
        case 0x040u: ph_40();  break;
        case 0x080u: ph_80();  break;
        case 0x100u: ph_100(); break;
        case 0x200u: ph_200(); break;
        default:     sub_set(0xF0u); break;
        }
        ph = CELL_LAD_PHASE;
        if (CELL_CNT_LADDER == 0u && ph != 0x200u && CELL_LAD_SAVED_PH != 1u) {
            sub_set(0xF6u);                          /* 200 ms timeout */
        }
        sub = CELL_LAD_SUB;
        if (sub < 0xF0u) {
            if (sub == 0x55u) {
                phase_set((uint16_t)(CELL_LAD_PHASE << 1));
            }
        } else {
            ph_100();                                /* the fault latch */
        }
    } while (CELL_LAD_SUB == 0x55u && CAL_LOOP_STOP != 0xA5u);
}

/* ── the mode sequencer 0x85160 (via 0x8570E) ─────────────────────────── */

static void retry(void)                                      /* LAB_00085352 */
{
    CELL_LAD_RETRIES = (uint8_t)(CELL_LAD_RETRIES - 1u);
    phase_set(1u);
    sub_set(0u);
    CELL_LAD_OKFLAGS = 0u;
    CELL_LAD_FAULT = 0u;
}

static void sequencer(void)
{
    const uint8_t st = CELL_SEQ_STATE;

    if (st == (uint8_t)'L') {
        const bool hw = plat_hwcond();                       /* 0x68BE0 */
        const uint8_t b = CELL_LAD_9500B;                    /* 0x6BD7A */

        CELL_LAD_OKFLAGS = (uint16_t)(CELL_LAD_OKFLAGS | 0x200u);
        if (CELL_MUX13_MIN < CAL_MIN13_OK || CELL_SSLB0_BIT2 != 0u) {
            if (b != 0xA5u && STANDIN_84A2B != 0xA5u) {
                if (CELL_OUTER_FSM == (uint8_t)'E') {
                    CELL_LAD_FAULT = (uint16_t)(CELL_LAD_FAULT & (uint16_t)~0x200u);
                    return;
                }
                seq_set((uint8_t)'.');
                grp_code(2u, 0x44u);
                grp_code(1u, 0x44u);
                return;
            }
            seq_set(0x1Fu);
            CELL_CLEAR_LATCH = 1u;                           /* 0x92094(1) */
        } else {
            CELL_LAD_FAULT = (uint16_t)(CELL_LAD_FAULT | 0x200u);
            if (hw || (CELL_CUT_FLAGS & 0x14u) != 0u) {
                CELL_LAD_OKFLAGS = (uint16_t)(CELL_LAD_OKFLAGS & (uint16_t)~0x200u);
                return;
            }
            pair8(&CELL_LAD_95005, &CELL_LAD_95005_C, 1u);
            CELL_CLEAR_LATCH = 1u;                           /* 0x92094(1) */
            seq_set((uint8_t)'=');
        }
        grp_code(2u, 0x22u);
        grp_code(1u, 0x22u);
        return;
    }
    if (st == 0u) {
        uint8_t sub;

        if (CAL_SKIP == 0xC3u) {
            sub_set(0xAAu);
        } else {
            ladder();                                        /* 0x8B7A8 */
        }
        sub = CELL_LAD_SUB;
        if (sub < 0xF0u) {
            if (sub == 0xAAu) {
                if (CELL_LAD_FAULT != 0u && CELL_LAD_RETRIES != 0u) {
                    retry();
                    return;
                }
                seq_set((uint8_t)'.');
                fsm_request(0x23u);                          /* H3: 0x87766(0x23) */
                CELL_CUT_MODE = 0x33u;
            } else {
                if (sub != 0x40u) {
                    return;
                }
                grp_code(2u, 0x16u);                         /* supply abort */
                grp_code(1u, 0x16u);
                grp_code(0u, 0x16u);
                if (CELL_MOD11 == 0x0Fu) {
                    return;
                }
            }
            CELL_MOD11 = 0x0Fu;
            return;
        }
        if (CELL_LAD_RETRIES == 0u) {
            seq_set(0x1Fu);
            grp_code(2u, 0x22u);
            grp_code(1u, 0x22u);
            CELL_CUT_MODE = 0x22u;
            CELL_CLEAR_LATCH = 1u;                           /* 0x92094(1) */
            CELL_MOD11 = 0x0Fu;
            fsm_request(CELL_LAD_SUB == 0xF1u ? 0x12u : 0x23u);   /* H3 */
            return;
        }
        retry();
        return;
    }
    if (st == (uint8_t)'.') {
        if (CELL_OUTER_FSM != (uint8_t)'E' || CELL_SSLB0_BIT2 == 0u) {
            return;
        }
        seq_set((uint8_t)'L');
        pair8(&CELL_LAD_95005, &CELL_LAD_95005_C, 0u);
        if (CAL_SKIP != 0xC3u) {
            const uint16_t dead = (uint16_t)((CELL_GRP(2u) | CELL_GRP(1u)) &
                                             (uint16_t)~CELL_GRP(3u));
            uint8_t ch;

            grp_code(3u, 0x33u);
            for (ch = 0u; ch < 2u; ch++) {
                if (((dead >> ch) & 1u) != 0u) {
                    seq2_code(ch, 0x22u);
                }
            }
            return;
        }
        grp_code(0u, 0x33u);
        grp_code(2u, 0x33u);
        grp_code(1u, 0x33u);
        return;
    }
    if (st == (uint8_t)'=') {
        const bool hw = plat_hwcond();

        if (CELL_MUX13_MIN < CAL_MIN13_OK || CELL_SSLB0_BIT2 != 0u) {
            CELL_LAD_FAULT = (uint16_t)(CELL_LAD_FAULT & (uint16_t)~0x200u);
        } else {
            CELL_LAD_FAULT = (uint16_t)(CELL_LAD_FAULT | 0x200u);
        }
        if (!hw && (CELL_CUT_FLAGS & 0x14u) == 0u) {
            CELL_LAD_OKFLAGS = (uint16_t)(CELL_LAD_OKFLAGS | 0x200u);
        } else {
            CELL_LAD_OKFLAGS = (uint16_t)(CELL_LAD_OKFLAGS & (uint16_t)~0x200u);
        }
        return;
    }
    if (st != 0x1Fu) {
        seq_set(0x1Fu);
    }
    /* 0x85740: the DTC report when 0x6F594() — not ported (S1) */
    (void)STANDIN_FAULTMEM_READY;
}

/* ── seq2 0x85828 (bytes 0x85828..0x85D42) ─────────────────────────────── */

static void seq2(void)
{
    uint8_t ch;

    for (ch = 0u; ch < 2u; ch++) {
        const uint8_t bit = chbit(ch);
        uint8_t suplow = CELL_SEQ2_SUPLOW;
        uint8_t demand, st, cls, en, e24, status;
        uint16_t supply;
        bool bad, resolved = false;

        if ((CAL_CHMASK & bit) == 0u) {                      /* 0x85C98 */
            mon2_chan_enable(ch, false);
            CELL_SEQ2_94FCE[ch] = 0u;
            CELL_SEQ2_STATE[ch] = 0x22u;
            CELL_SEQ2_STATE_C[ch] = 0xDDu;                   /* twin 0x8C3BC */
            continue;
        }
        demand = CELL_OUT_DEMAND[ch];
        st = CELL_SEQ2_STATE[ch];
        supply = CELL_SUPPLY;
        (void)classify(ch);
        cls = CELL_OUT_CLASS[ch];
        if (st == 0x33u) {
            if (ch == 0u) { CELL_ADC_ST_MUX8 = 0u; } else { CELL_ADC_ST_MUX10 = 0u; }  /* 0x8F50C */
            /* 0x85FC4 '3' diagnosis: NOT PORTED (unreachable under H3) */
        }
        /* else 0x85ED4(ch, 0) DTC diagnosis: not ported */
        {
            const uint8_t f = STANDIN_DRV_FAULT;             /* 0x7BBB4 (S3) */
            bad = !(f == 1u || f == 0u);
        }
        CELL_SEQ2_94FCC[ch] = (demand != 0u && !bad) ? 1u : 0u;
        if (supply < CAL_S2_LOW && (suplow & bit) == 0u &&
            CELL_CNT_SUPLOW[ch] < CAL_S2_LOWHOLD[ch]) {
            CELL_CNT_SUPLOW[ch] = CAL_S2_LOWHOLD[ch];
        }
        if (supply < CAL_S2_LOW || CELL_CNT_SUPLOW[ch] != 0u) {
            suplow = (uint8_t)(suplow | bit);
        }
        if ((uint32_t)supply >= (uint32_t)CAL_S2_LOW + CAL_S2_REL && CELL_CNT_SUPLOW[ch] == 0u) {
            suplow = (uint8_t)(suplow & (uint8_t)~bit);
        }

        en = 0u;
        e24 = 0u;
        if (st == 0u) {
            CELL_SEQ2_DIAG[ch] = 0u;
        } else if (st == 0x11u || st == 0x16u) {
            if (ch == 0u) { CELL_ADC_ST_MUX8 = 0u; } else { CELL_ADC_ST_MUX10 = 0u; }  /* 0x8F50C */
            e24 = CELL_LAD_REQ[ch];
            en = e24;                    /* the enable follows the request ONLY here */
            CELL_SEQ2_DIAG[ch] = 1u;
            CELL_CNT_HOLDOFF[ch] = 0u;
            resolved = (st == 0x16u);
        } else if (st == 0x22u) {
            if (ch == 0u) { CELL_ADC_ST_MUX8 = 0u; } else { CELL_ADC_ST_MUX10 = 0u; }  /* 0x8F50C */
            CELL_SEQ2_DIAG[ch] = 0xFFu;
            resolved = true;
        } else if (st == 0x33u) {
            if (CELL_SEQ2_DIAG[ch] == 0xFFu) {
                if ((suplow & bit) != 0u) {
                    st = (uint8_t)'8';
                } else if (bad) {
                    st = (uint8_t)'?';
                } else if (demand == 0u) {
                    st = (uint8_t)'D';
                } else {
                    st = (uint8_t)'U';
                    en = 1u;
                }
                if (en != 0u) {
                    CELL_CNT_SETTLE[ch] = CAL_S2_SETTLE_U[ch];
                } else {
                    CELL_CNT_SETTLE[ch] = CAL_S2_SETTLE[ch];
                }
                e24 = en;
            } else if (CELL_SEQ2_DIAG[ch] == 0xFEu) {
                st = (uint8_t)'4';
                CELL_CNT_SETTLE[ch] = CAL_S2_SETTLE[ch];
            }
        } else if (st == (uint8_t)'4' || st == (uint8_t)'8' || st == (uint8_t)'?' ||
                   st == (uint8_t)'D') {
            if (demand == 0u) {
                CELL_CNT_HOLDOFF[ch] = 0u;
            }
            if (CELL_CNT_HOLDOFF[ch] != 0u) {
                st = (uint8_t)'4';
            } else if ((suplow & bit) != 0u) {
                st = (uint8_t)'8';
            } else if (bad) {
                st = (uint8_t)'?';
            } else if (demand == 0u) {
                st = (uint8_t)'D';
            } else if (cls == 0x11u || cls == 0x44u || CELL_OUT_FAULTS[ch] != 0u) {
                st = (uint8_t)'4';
            } else {
                st = (uint8_t)'U';
                en = 1u;
            }
            if (en != 0u) {
                CELL_CNT_SETTLE[ch] = CAL_S2_SETTLE_U[ch];
            }
            e24 = en;
            if ((cls == 0x11u || cls == 0x44u) && demand != 0u) {
                CELL_SEQ2_939C0[ch] = 1u;
            }
            resolved = true;
        } else if (st == (uint8_t)'U') {
            if (CELL_CNT_HOLDOFF[ch] != 0u) {
                st = (uint8_t)'4';
            } else if ((suplow & bit) != 0u) {
                st = (uint8_t)'8';
            } else if (bad) {
                st = (uint8_t)'?';
            } else if (demand == 0u) {
                st = (uint8_t)'D';
            } else if (CELL_OUT_FAULTS[ch] != 0u) {
                st = (uint8_t)'4';
            } else {
                en = 1u;
            }
            if (en == 0u) {
                CELL_CNT_SETTLE[ch] = (CELL_OUT_FAULTS[ch] != 0u) ? 0u : CAL_S2_SETTLE[ch];
            }
            e24 = en;
            resolved = true;
        } else {
            st = 0x22u;                                      /* 0x85C14 */
        }
        if (resolved) {
            CELL_SEQ2_RESOLVED = (uint8_t)(CELL_SEQ2_RESOLVED | bit);   /* 0x85C02 */
        }
        /* 0x85C20: the output status 0x939B8 */
        if (st == (uint8_t)'U') {
            status = CELL_CNT_SETTLE[ch] != 0u ? 3u : 4u;
        } else {
            status = cls == 0x22u ? 1u : 0u;
            if (st == 0x11u && (cls == 0x33u || cls == 0x66u)) {
                status = 2u;
            }
        }
        CELL_SEQ2_STATUS[ch] = status;
        CELL_SEQ2_STATE[ch] = st;
        CELL_SEQ2_STATE_C[ch] = (uint8_t)~st;                /* twin 0x8C3BC (not ported) */
        CELL_SEQ2_SUPLOW = suplow;
        mon2_chan_enable(ch, en != 0u);                      /* 0x8C7A8 */
        CELL_SEQ2_94FCE[ch] = e24;
    }
    if (CELL_MOD7 != 0x0Fu && (CAL_CHMASK & CELL_SEQ2_RESOLVED) == CAL_CHMASK) {
        CELL_MOD7 = 0x0Fu;
    }
}

/* ── the pin-cut driver 0x85788 and its verdict 0x8C34C ────────────────── */

static void cut_verdict(void)                                /* 0x8C34C */
{
    uint8_t f = CELL_CUT_FLAGS;

    if ((f & 0x04u) == 0u) {
        if ((CELL_SSLB0_BIT4 & 3u) == 0u) {
            f &= 0xF4u;
        } else if ((f & 0x10u) == 0u) {
            f = (uint8_t)((f & 0xFEu) | 0x02u);
        } else {
            f &= 0xEFu;
        }
    } else {
        if ((CELL_SSLB0_BIT4 & 3u) == 0u) {
            f = (uint8_t)((f & 0xFDu) | 0x08u);
        } else {
            f = (uint8_t)((f & 0xF7u) | 0x02u);
        }
    }
    CELL_CUT_FLAGS = f;
    /* 0x93B22/0x93B24 + the 0x7AEAC report: not ported */
}

static void pin_cut(void)
{
    const uint8_t mode = CELL_CUT_MODE;

    if (mode == 0x33u) {
        if ((CELL_CUT_FLAGS & 0x20u) == 0u) {
            CELL_CUT_FLAGS = (uint8_t)(CELL_CUT_FLAGS | 0x20u);
        } else {
            cut_verdict();
        }
        if (CELL_CUT_PERIODIC == 0xAAu) {
            plat_tioc_cut();                                 /* 0x91E6C */
            CELL_CUT_FLAGS = (uint8_t)(CELL_CUT_FLAGS | 0x04u);
        } else if (CELL_CUT_PERIODIC == 0x55u) {
            plat_tioc_restore();                             /* 0x91E4E */
            if ((CELL_CUT_FLAGS & 0x04u) != 0u) {
                CELL_CUT_FLAGS = (uint8_t)(CELL_CUT_FLAGS | 0x10u);
            }
            CELL_CUT_FLAGS = (uint8_t)(CELL_CUT_FLAGS & 0xFBu);
            CELL_CUT_PERIODIC = 0u;
        }
    } else if (mode == 0x11u) {
        if (CELL_CUT_REQ == 0xAAu) {
            plat_tioc_cut();                                 /* 0x91E6C */
            CELL_CUT_REQ = 0x55u;
        } else if (CELL_CUT_REQ == 0x55u) {
            plat_tioc_restore();                             /* 0x91E4E */
            CELL_CUT_REQ = 0u;
        }
    }
}

/* ── init and the tick ─────────────────────────────────────────────────── */

void sup_ladder_init(void)                                   /* 0x85678 */
{
    uint16_t g[3] = { 0u, 0u, 0u };
    uint8_t ch;

    seq_set(0u);
    for (ch = 0u; ch < 2u; ch++) {                           /* 0x8556E */
        if ((CAL_CHMASK & chbit(ch)) != 0u) {
            const uint8_t t = CAL_CHTYPE[ch];
            const uint8_t gi = t == 2u ? 2u : (t == 1u ? 1u : 0u);
            g[gi] = (uint16_t)(g[gi] | chbit(ch));
        }
    }
    grp_set(2u, g[2]);
    grp_set(1u, g[1]);
    grp_set(0u, g[0]);
    CELL_CUT_MODE = 0x11u;
    grp_code(0u, 0x11u);
    grp_code(1u, 0x11u);
    grp_code(2u, 0x11u);
    /* 0x7ADBC(0x3FF): not ported */
    CELL_LAD_RETRIES = CAL_RETRIES;
    phase_set(1u);
    sub_set(0u);
    for (ch = 0u; ch < 2u; ch++) {                           /* 0x863D8 */
        CELL_DIAG_93120[ch] = 0u;
        CELL_LAD_FAILCLS[ch] = 0u;
    }
    pair8(&CELL_LAD_95005, &CELL_LAD_95005_C, 1u);
    CELL_LAD_FAULT = 0u;
    CELL_LAD_OKFLAGS = 0u;
    /* S2: the application's output demand (0x7AB9C ← model 0x132340) */
    pair8(&CELL_OUT_DEMAND[0], &CELL_OUT_DEMAND_C[0], 1u);
    pair8(&CELL_OUT_DEMAND[1], &CELL_OUT_DEMAND_C[1], 0u);
    s_fsm_req_held = 0u;
}

void sup_ladder_tick(void)
{
    uint32_t lk;

    /* 0x7CB74 → 0x8332C: lock; 0x88AE0 (→ 0x8D8DE); 0x89518; 0x89114 */
    lk = lock();
    supply_plaus();
    publish_out();
    unlock(lk);

    /* S5: module 11's start, reached when the AN0 inputs are valid */
    if (CELL_MOD11 == 0u && plat_adcmux_ready()) {
        CELL_MOD11 = 1u;
    }
    /* 0x7CC76 → 0x8570E: the sequencer only while module 11 != 0 */
    lk = lock();
    if (CELL_MOD11 != 0u) {
        sequencer();
    }
    unlock(lk);
    /* 0x7CC56 → 0x85828 */
    lk = lock();
    seq2();
    unlock(lk);
    /* 0x7CC32 → 0x85788 (then 0x86A80, 0x86AF8, 0x849D4, 0x86B0C, 0x8646C:
     * not ported) */
    lk = lock();
    pin_cut();
    unlock(lk);
}

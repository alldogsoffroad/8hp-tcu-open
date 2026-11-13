/* sup_ladder.c — L1..L3 (2026-11-13): the stock supervisor-chip self-test
 * ladder, its sequencer, seq2 and the '3' switch-on diagnosis, ported 1:1
 * from the September image's bytes, staged by compile-time holds.
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
 *   0x85FC4 '3' diagnosis (+0x86838 class wait, 0x865E0 latch, 0x86586
 *     status; 0x6F594 = S1); 0x85ED4 DTC diagnosis (+0x86628 output check,
 *     0x85E82 'U' hold-off)
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
 *      and not made: the walk FSM stays 8. Every other cell write of the
 *      fail path runs (0x92094(1) strip latch, module 11 = 0x0F, groups →
 *      0x22).
 * Because 0x85E64 is the request cells' only writer and only H1/H2's calls
 * ever pass 1, with H1 and H2 held every request cell stays 0, seq2 (state
 * 0x11) follows it with enable 0, every letter stays 0 and the J2 word
 * stays 0x9000 in every reachable state (tests/unit/test_ladder.c).
 *
 * THE STAGES (2026-11-13, ~/tcu_sweep/l3_port.md):
 *   L1  H1 H2 H3 held (r02r).  L2  H1 released (r02s).
 *   L3a H1 H2 released, H3 held — THE TARGET DEFAULT: phases 0x40 (request
 *       1, output ON within 30 ms), 0x80 (target 5, output must DROP with
 *       the request still 1; request 0, target 0), 0x100 (class 0x22, mask
 *       0xFF → 0xAA). The sequencer then waits in '.' (it needs FSM 'E',
 *       which H3 withholds): no permanent letter, J2 0x9000 for good.
 *   L3a-E  H3 released, H4 held (sup_ladder.h): the FSM runs the stock's
 *       0x23 → 'E' walk after 0xAA, the sequencer stays '.', J2 0x9000.
 *   L3b H3 and H4 released (host test; SH only with SUP_L3B_ON_TARGET): FSM
 *       0x23 → level 0 + era 0x1838 → 'E' (the 0x8CEDC rule); sequencer
 *       '.' → 'L' (FSM 'E' and 0x14 bit 1); seq2 ch0 0x33 → the '3'
 *       diagnosis (output OFF, class 0x22, ~20 ms) → 'U': letter 'c',
 *       0x9020 for good.
 *
 * STAND-INS (named deviations, each [I] or bench-forced):
 *   S1 0x6F594 (fault memory ready) = 1. Its readers are 0x85740's DTC
 *      report (not ported) and the '3' diagnosis state 8 (L3): equivalent
 *      to the stock's own calibration bypass 0x19199B != 0 (the stock
 *      reaches it through its NVM manager, 0x8E57D = 0xA5, writer not
 *      located). With it, state 8 lasts one tick.
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
 * NOT PORTED (off the stages' paths, or reporting only):
 *   the diverse twins 0x8C3BC / 0x8C670; the 0x7BB4E / 0x87120 reports of
 *   the '3' and DTC diagnoses; the 5 ms entry 0x8646C (the 0x9411A /
 *   0x94119 tested/failed markers, read by 0x86628 into the 0x80 bit); the DTC
 *   reports 0x85740 → 0x7ADD4, 0x88AE0's 0x8D566, 0x8C34C's 0x7AEAC, the
 *   pair-check faults of 0x6BDCA/0x6BDFE (0x221) and the 0x226 loop checks;
 *   0x7ADBC(0x3FF) in 0x85678; the other members of the same list entries
 *   (0x7C884, 0x7CB6E, 0x7AB48, 0x7CBA8, 0x7CC1E, 0x86A80 …); the profiling
 *   hooks 0x72AE0; the post-ladder periodic cut request 0x94152 (writer not
 *   located; 0x94153 = 0x33 only after 0xAA; without it the post-ladder
 *   cut driver only runs its verdict); the post-engagement mode switch
 *   (module 7 → modules 1/2 → mode 1, the 10 ms batch split).
 */
#include "bsw/sup/sup_ladder.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_letters.h"
#include "platform.h"

/* ── THE STAGE GUARDS (L3, 2026-11-13) ─────────────────────────────────────
 * Every piece on the path a stage can reach is now ported (FSM 0x23 / 'E' /
 * 0x12 / 0x34 + 0x87766 / 0x8799C / 0x8CEDC in sup_walk.c; the '3'
 * diagnosis 0x85FC4 / 0x86838 / 0x865E0 / 0x86586 here). What the guards
 * still refuse:
 *   - a stage out of order: H2 released only after H1 (phase 0x40 without
 *     phase 8's shut-off frame is not a stock sequence), H3 only after H2;
 *   - L3b on SH without SUP_L3B_ON_TARGET: past seq2 'U' the stock is in
 *     mode 1 (module 7 = 0x0F → module 1 → module 2 → 0x66E06) and moves the
 *     batch / reply proc / F stage to 10 ms — NOT PORTED, so an L3b image
 *     keeps the 5 ms batch after engagement (a wire difference, the J2 word
 *     itself unaffected: the J2 stage stays at 5 ms in the stock too).
 *     Defining SUP_L3B_ON_TARGET acknowledges it. */
#if !SUP_L1_HOLD_PHASE40 && SUP_L1_HOLD_PHASE8_REQUEST
#error "H2 released with H1 held: phase 0x40 without phase 8's shut-off frame is not a stock sequence"
#endif
#if !SUP_L1_HOLD_FAIL_FSM && SUP_L1_HOLD_PHASE40
#error "H3 released with H2 held: release the stages in order (L2 → L3a → L3b)"
#endif
#if !SUP_L3_HOLD_SEQ_L && SUP_L1_HOLD_FAIL_FSM
#error "H4 released with H3 held: the sequencer can reach 'L' only from FSM 'E' — release H3 first"
#endif
#if defined(__sh__) && !SUP_L1_HOLD_FAIL_FSM && !SUP_L3_HOLD_SEQ_L && !defined(SUP_L3B_ON_TARGET)
#error "L3b on the target: the post-engagement mode-1 10 ms batch split is not ported — define SUP_L3B_ON_TARGET to acknowledge"
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

/* the 0x87766 call site: HELD under H3 (recorded only), MADE on L3b */
static void fsm_request(uint8_t t)
{
    s_fsm_req_held = t;
#if !SUP_L1_HOLD_FAIL_FSM
    sup_monitor_request(t);                                  /* 0x87766 */
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

/* Phases 0x40 and 0x80 (bytes 0x8BEC0..0x8BFB6 / 0x8BFB8..0x8C0F0, re-read
 * for L3): under H2 the dispatcher never reaches them; L3a releases H2. */
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
#if SUP_L3_HOLD_SEQ_L
        return;                                              /* H4: no 'L' */
#endif
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

/* ── the '3' switch-on diagnosis (L3; 0x85FC4 / 0x86838 / 0x865E0 /
 *    0x86586, every branch from the bytes 0x85FC4..0x863D6) ───────────── */

/* calibration (0x66xxx tables at index 0x90AB8 = 0), [ch * 3 + step] */
static const uint8_t CAL_D_WAIT[6] = { 2u, 2u, 2u, 2u, 2u, 2u };      /* 0x1919CA */
static const uint8_t CAL_D_TMO[6]  = { 10u, 2u, 10u, 10u, 2u, 10u };  /* 0x1919D3 */
static const uint8_t CAL_D_EN[2]   = { 2u, 1u };       /* 0x1915A4 (≠ 0 → 0x84A25 bit) */
static const uint8_t CAL_D_STEP11[2] = { 2u, 2u };     /* 0x1915B8 bit 0: the class-0x11 step */
#define CAL_D_STEP3     0x00u      /* 0x1919C9: the 0x84A25/26 step-3 interlock */
static const uint16_t CAL_D_MASK[2] = { 0xFFFFu, 0xFFFFu };  /* 0x190564 */
static const uint16_t CAL_D_REP[2]  = { 0xFFFFu, 0x0000u };  /* 0x190560 (0x86586) */
#define CAL_D_LATCH     0x01E0u    /* 0x1915BA (0x865E0) */

/* 0x6F594 "fault memory ready" (S1): the stock returns 1/2 once its NVM
 * manager is up (0x8E57D = 0xA5 / 0x55 / 0xAA; dump A5) or when cal
 * 0x19199B != 0. We have no stock fault memory: the stand-in is the stock's
 * own calibration bypass (0x19199B != 0 → 1). Its 0x94038/39 copies are
 * not ported. */
static uint8_t faultmem_ready(void)
{
    return STANDIN_FAULTMEM_READY;
}

/* 0x86838: a settle-then-watch for class `want` on the 5 ms countdown
 * 0x947B4[ch] — sub-FSM 0x939C2[ch]: 0 → load `wait`; 1 → wait for the
 * countdown, then load `tmo`; 2 → class == want → 0xFF (sub 3), the
 * countdown expired → the class seen (sub 4), else 0xF0; a finished
 * sub-FSM (>= 3) restarts at 0 on the next call. Returns 0xF0 while busy. */
static uint8_t cls_wait(uint8_t ch, uint8_t want, uint8_t wait, uint8_t tmo)
{
    uint8_t cnt = CELL_CNT_DIAG[ch];
    uint8_t st = CELL_DIAG_SUB[ch];
    uint8_t cls;
    uint8_t r = 0u;

    (void)classify(ch);                                      /* 0x8C6D4 */
    cls = CELL_OUT_CLASS[ch];
    if (st >= 3u) {
        st = 0u;
    }
    if (st == 0u) {
        st = 1u;
        r = 0xF0u;
        cnt = wait;
    }
    if (st == 1u) {
        if (cnt == 0u) {
            st = 2u;
            cnt = tmo;
        }
        r = 0xF0u;
    }
    if (st == 2u) {
        if (want == cls) {
            cnt = 0u;
            st = 3u;
            r = 0xFFu;
        } else if (cnt == 0u) {
            st = 4u;
            r = cls;
        } else {
            r = 0xF0u;
        }
    }
    CELL_CNT_DIAG[ch] = cnt;
    CELL_DIAG_SUB[ch] = st;
    return r;
}

/* 0x865E0: the permanent-fault latch 0x93120[ch] (bits 0x1E0 seen in both
 * words) folds into both words of the pair */
static void diag_latch(uint8_t ch)
{
    const uint16_t v = (uint16_t)((CELL_DIAG_93120[ch] |
                                   (uint16_t)(CELL_DIAG_TEST(ch) & CELL_DIAG_SEEN(ch) &
                                              (CAL_D_LATCH & 0x1E0u))) & 0x1E0u);

    CELL_DIAG_93120[ch] = v;
    CELL_DIAG_TEST(ch) = (uint16_t)(CELL_DIAG_TEST(ch) | v);
    CELL_DIAG_SEEN(ch) = (uint16_t)(CELL_DIAG_SEEN(ch) | CELL_DIAG_93120[ch]);
}

/* 0x86586: the per-channel status 0x939C4[ch] (0 / 2 / 0x82); its 0x87120
 * report is not ported */
static void diag_status(uint8_t ch, uint16_t tested, uint16_t failed)
{
    const uint16_t p = (uint16_t)(tested & failed);
    uint8_t v = 0u;

    if (p != 0u && (CAL_D_REP[ch] & p) != 0u) {
        v = (CAL_D_REP[ch] & p & 0x1E0u) != 0u ? 0x82u : 0x02u;
    }
    CELL_DIAG_CLS[ch] = v;
}

/* 0x85E82: the 'U' hold-off — 0x947B2[ch] = cal 0x1915AE[ch] (0x48 = 72
 * ticks = 360 ms); seq2 leaves 'U' for '4' (enable 0) while it runs */
static const uint8_t CAL_D_HOLD[2] = { 0x48u, 0x48u };          /* 0x1915AE */
static const uint8_t CAL_D_DTC8[2] = { 0u, 0u };                /* 0x1915BC */
static const uint16_t CAL_D_SUPMIN[2] = { 6500u, 6500u };       /* 0x190574 */
static const uint16_t CAL_D_SUPMAX[2] = { 40000u, 40000u };     /* 0x190570 */
static const uint16_t CAL_D_ONBAND[2] = { 1500u, 1500u };       /* 0x190568 */
static const uint16_t CAL_D_ONMIN[2]  = { 4000u, 4000u };       /* 0x19056C */

/* 0x86628 (bytes 0x86628..0x86836): the per-state output plausibility,
 * into the local pair p[0] (tested) / p[1] (failed). In 'U': fault when
 * 0x14 bit 5 (0x90947[ch]) is set, or — supply status 0x94FED >= 8 and no
 * 0x940FA ripple flag — the output is more than 1500 mV from the supply,
 * else below 4000 mV; a fault marks 0x40 and loads the hold-off (0x85E82). */
static void dtc_check(uint8_t ch, uint8_t st, uint16_t p[2])
{
    const uint16_t v = CELL_OUT_MV[ch];
    const uint16_t sup = CELL_SUPPLY;
    const uint8_t sst = CELL_SUPPLY_ST2;                     /* 0x94FED */
    const uint8_t cls = CELL_OUT_CLASS[ch];
    const uint8_t bit = chbit(ch);
    uint16_t r1 = 0u;

    p[0] = 0u;
    p[1] = 0u;
    if (st == (uint8_t)'U') {
        bool fault = false;

        p[0] = (uint16_t)(p[0] | (CAL_D_HOLD[ch] == 0u ? 0x08u : 0x40u));
        if (CELL_OUT_FAULTS[ch] != 0u) {
            fault = true;
        } else if (sst >= 8u && CELL_SUP_940FA == 0u) {
            const uint16_t d = (uint16_t)(sup >= v ? sup - v : v - sup);
            fault = d > CAL_D_ONBAND[ch];
        } else {
            fault = CAL_D_ONMIN[ch] > v;
        }
        if (fault) {
            p[1] = CAL_D_HOLD[ch] != 0u ? 0x40u : 0x08u;
            CELL_CNT_HOLDOFF[ch] = CAL_D_HOLD[ch];           /* 0x85E82 */
        } else {
            p[0] = (uint16_t)(p[0] | ((p[0] & 0x08u) != 0u ? 0x40u : 0x08u));
        }
    } else if (st == 0x22u) {
        const uint8_t f = CELL_LAD_FAILCLS[ch];              /* 0x939B4 */

        p[0] = (uint16_t)(p[0] | 0x0Eu);
        if (f == 0x11u) {
            r1 = 8u;
        } else if (f == 0x33u || f == 0x66u) {
            r1 = 4u;
        } else if (f == 0x22u) {
            r1 = 2u;
        } else if (cls == 0x11u || cls == 0x44u) {
            r1 = 8u;
        } else if (cls == 0x33u || cls == 0x66u || cls == 0x55u) {
            r1 = 4u;
        }
        p[1] = r1;
    } else if (st == (uint8_t)'4' || st == (uint8_t)'8' || st == (uint8_t)'?' ||
               st == (uint8_t)'D') {
        p[0] = (uint16_t)(p[0] | 0x04u);
        if (CAL_D_DTC8[ch] != 0u || CELL_SEQ2_939C0[ch] != 0u) {
            p[0] = (uint16_t)(p[0] | 0x08u);
            CELL_SEQ2_939C0[ch] = 0u;
        }
        if ((cls == 0x11u || cls == 0x44u) && (p[0] & 0x08u) != 0u) {
            r1 = 8u;
        } else {
            if ((p[0] & 0x08u) != 0u) {
                p[0] = (uint16_t)(p[0] | 0x40u);
            }
            if (cls == 0x33u || cls == 0x66u || cls == 0x55u) {
                r1 = 4u;
            } else {
                p[0] = (uint16_t)(p[0] | 0x20u);
            }
        }
        p[1] = r1;
    }
    /* 0x9411A / 0x94119 ("tested" / "failed" markers of the 5 ms entry
     * 0x8646C — not ported, so 0 here; the stock dump has 0x9411A = 01,
     * which is why its 0x9495C[0] reads 0xC8 where ours reads 0x48) */
    if ((CELL_L1_U8(0xFFF9411Au) & bit) != 0u) {
        p[0] = (uint16_t)(p[0] | 0x80u);
    }
    if ((CELL_L1_U8(0xFFF94119u) & bit) != 0u) {
        p[1] = (uint16_t)(p[1] | 0x80u);
    }
}

/* 0x865E0 on a local pair (as 0x85ED4 calls it) */
static void diag_latch_p(uint8_t ch, uint16_t p[2])
{
    const uint16_t v = (uint16_t)((CELL_DIAG_93120[ch] |
                                   (uint16_t)(p[0] & p[1] & (CAL_D_LATCH & 0x1E0u))) & 0x1E0u);

    CELL_DIAG_93120[ch] = v;
    p[0] = (uint16_t)(p[0] | v);
    p[1] = (uint16_t)(p[1] | CELL_DIAG_93120[ch]);
}

/* 0x85ED4 (bytes 0x85ED4..0x85FC2): the DTC diagnosis seq2 runs every tick
 * in every state but '3' (state read at seq2 entry, 0x85DC4). The output
 * check 0x86628 runs only in 0x22 / '4' '8' '?' 'D' 'U' once the settle
 * timer 0x947B0 has expired and the supply is in [6500, 40000] mV. Its one
 * effect on the flow is the 'U' hold-off; the rest is the DTC masks
 * (0x9495C / 0x94958 / 0x939C4, the 0x93120 latch). The 0x7BB4E report is
 * not ported. */
static void dtc_diag(uint8_t ch, uint8_t st)
{
    uint16_t p[2] = { 0u, 0u };
    const uint16_t sup = CELL_SUPPLY;

    if ((st == 0x22u || st >= 0x34u) && CELL_CNT_SETTLE[ch] == 0u &&
        sup >= CAL_D_SUPMIN[ch] && !(sup > CAL_D_SUPMAX[ch])) {
        dtc_check(ch, st, p);
    }
    p[0] = (uint16_t)(p[0] & CAL_D_MASK[ch]);
    p[1] = (uint16_t)(p[1] & CAL_D_MASK[ch]);
    diag_latch_p(ch, p);
    CELL_DIAG_9495C[ch] = p[0];
    CELL_DIAG_94958[ch] = (uint16_t)((p[1] & p[0]) | (CELL_DIAG_94958[ch] & (uint16_t)~p[0]));
    diag_status(ch, p[0], p[1]);                             /* 0x86586 */
}

/* 0x85FC4: the diagnosis seq2 runs every tick in state 0x33 ('3'), with the
 * enable OFF. Sub-state 0x939BE[ch] (seq2 leaves it at 1 in 0x11/0x16):
 *   1  bits 0x6C under test; class 0x22 (OFF with bias) within 10 ms
 *      settle + 50 ms (0x86838, cal 2 / 10); the class seen on a timeout
 *      marks 0x20 (0x66 / 0x33 / 0x55) or 0x40. → 8 (cal: no class-0x11
 *      step, no step-3 interlock on this unit), or → 2 / 3 per calibration
 *   2  (cal-dead) class 0x11 within 10 + 10 ms, bit 0x80
 *   3  (cal-dead) waits for the other channels (0x84A25..28)
 *   4  (cal-dead) class 0x22 again, bit 0x100; 5 waits for the others
 *   8  waits for 0x6F594 (fault memory ready); masks (cal 0xFFFF), folds
 *      the 0x93120 latch (0x865E0), publishes 0x9495C (tested) / 0x94958
 *      (failed bits within the tested mask)
 *   9  0xFF (pass) when nothing tested failed, else 0xFE
 * seq2 reads 0xFF in the same tick: 'U' (enable 1) when supply is not low,
 * 0x7BBB4 reports no fault and the demand 0x94FCA[ch] != 0; 0xFE → '4'.
 * The 0x7BB4E DTC report at the tail is not ported. A 0x1E0 fault latched
 * in 0x93120 (by 0x85ED4's 0x865E0, e.g. an engaged-output mismatch) makes
 * every later diagnosis fail (0xFE), as in the stock. */
static void diag_3(uint8_t ch)
{
    const uint8_t bit = chbit(ch);
    uint8_t d94115 = CELL_DIAG_94115;
    uint8_t st = CELL_SEQ2_DIAG[ch];
    uint16_t tested = 0u;                                    /* uStack_2c */
    uint8_t r;

    switch (st) {
    case 0u:
        break;                                               /* stays 0 */
    case 1u:
        CELL_DIAG_SEEN(ch) = 0u;
        CELL_DIAG_TEST(ch) = 0x6Cu;
        CELL_DIAG_84A26 = (uint8_t)(CELL_DIAG_84A26 & (uint8_t)~bit);
        CELL_DIAG_84A25 = (uint8_t)(CELL_DIAG_84A25 & (uint8_t)~bit);
        CELL_DIAG_84A27 = (uint8_t)(CELL_DIAG_84A27 & (uint8_t)~bit);
        if (CAL_D_EN[ch] != 0u) {
            CELL_DIAG_84A25 = (uint8_t)(CELL_DIAG_84A25 | bit);
        }
        CELL_DIAG_9411C = ch;
        CELL_DIAG_9411B = 0u;
        r = cls_wait(ch, 0x22u, CAL_D_WAIT[ch * 3u], CAL_D_TMO[ch * 3u]);
        if (r == 0xF0u) {
            break;                                           /* stays 1 */
        }
        if (r == 0xFFu) {
            if ((CAL_D_STEP11[ch] & 1u) != 0u) {             /* 0x860A8 */
                d94115 = (uint8_t)(d94115 | bit);
                st = 2u;
                break;
            }
            if (CAL_D_STEP3 != 0u && (CELL_DIAG_84A25 & bit) != 0u) {
                CELL_DIAG_84A26 = (uint8_t)(CELL_DIAG_84A26 | bit);   /* 0x861CA */
                st = 3u;
                break;
            }
        } else {
            CELL_DIAG_SEEN(ch) = (r == 0x66u || r == 0x33u || r == 0x55u) ? 0x20u : 0x40u;
            if (CAL_D_STEP3 != 0u && (CELL_DIAG_84A25 & bit) != 0u) {
                CELL_DIAG_84A25 = (uint8_t)(CELL_DIAG_84A25 & (uint8_t)~bit);
            }
        }
        st = 8u;                                             /* 0x862EA */
        break;
    case 2u:
        CELL_DIAG_TEST(ch) = (uint16_t)(CELL_DIAG_TEST(ch) | 0x80u);
        CELL_DIAG_9411B = 1u;
        CELL_DIAG_9411C = ch;
        r = cls_wait(ch, 0x11u, CAL_D_WAIT[ch * 3u + 1u], CAL_D_TMO[ch * 3u + 1u]);
        if (r == 0xF0u) {
            break;                                           /* stays 2 */
        }
        if (r == 0xFFu || r == 0x44u) {
            if (CAL_D_STEP3 != 0u && (CELL_DIAG_84A25 & bit) != 0u) {
                CELL_DIAG_84A26 = (uint8_t)(CELL_DIAG_84A26 | bit);
                st = 3u;
                break;
            }
        } else {
            CELL_DIAG_SEEN(ch) = 0x80u;
            if (CAL_D_STEP3 != 0u && (CELL_DIAG_84A25 & bit) != 0u) {
                CELL_DIAG_84A25 = (uint8_t)(CELL_DIAG_84A25 & (uint8_t)~bit);
            }
        }
        d94115 = (uint8_t)(d94115 & (uint8_t)~bit);          /* 0x862E8 */
        st = 8u;
        break;
    case 3u:
        if (CELL_DIAG_84A28 != 0u ||
            CELL_DIAG_84A25 != (uint8_t)(CELL_DIAG_84A25 & CELL_DIAG_84A26)) {
            break;                                           /* stays 3 */
        }
        CELL_DIAG_84A28 = 1u;
        d94115 = (uint8_t)(CELL_DIAG_84A25 & (uint8_t)~bit);
        st = 4u;
        break;
    case 4u:
        CELL_DIAG_9411C = ch;
        CELL_DIAG_TEST(ch) = (uint16_t)(CELL_DIAG_TEST(ch) | 0x100u);
        CELL_DIAG_9411B = 2u;
        r = cls_wait(ch, 0x22u, CAL_D_WAIT[ch * 3u + 2u], CAL_D_TMO[ch * 3u + 2u]);
        if (r == 0xF0u) {
            break;                                           /* stays 4 */
        }
        if (r == 0x11u) {
            CELL_DIAG_SEEN(ch) = 0x100u;
        }
        CELL_DIAG_84A27 = (uint8_t)(CELL_DIAG_84A27 | bit);
        CELL_DIAG_84A28 = 0u;
        st = 5u;
        break;
    case 5u:
        if (CELL_DIAG_84A25 != (uint8_t)(CELL_DIAG_84A27 & CELL_DIAG_84A25)) {
            break;                                           /* stays 5 */
        }
        d94115 = (uint8_t)(d94115 & (uint8_t)~CELL_DIAG_84A25);
        st = 8u;
        break;
    case 8u:
        if (faultmem_ready() == 0u) {                        /* 0x6F594 (S1) */
            break;                                           /* stays 8 */
        }
        CELL_DIAG_TEST(ch) = (uint16_t)(CELL_DIAG_TEST(ch) & CAL_D_MASK[ch]);
        CELL_DIAG_SEEN(ch) = (uint16_t)(CELL_DIAG_SEEN(ch) & CAL_D_MASK[ch]);
        diag_latch(ch);                                      /* 0x865E0 */
        tested = CELL_DIAG_TEST(ch);
        CELL_DIAG_9495C[ch] = tested;
        CELL_DIAG_94958[ch] = (uint16_t)((CELL_DIAG_SEEN(ch) & tested) |
                                         (CELL_DIAG_94958[ch] & (uint16_t)~tested));
        st = 9u;
        break;
    case 9u:
        st = ((CELL_DIAG_9495C[ch] & CELL_DIAG_94958[ch]) != 0u) ? 0xFEu : 0xFFu;
        break;
    default:
        if (st != 0xFEu && st != 0xFFu) {
            st = 0u;
        }
        break;
    }
    /* tail (0x8639C): 0x7BB4E report (not ported), 0x86586, then the state */
    (void)tested;
    diag_status(ch, CELL_DIAG_9495C[ch], CELL_DIAG_94958[ch]);
    CELL_SEQ2_DIAG[ch] = st;
    CELL_DIAG_94115 = d94115;
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
            diag_3(ch);                                      /* 0x85FC4 (L3) */
        }
        else {
            dtc_diag(ch, st);                                /* 0x85ED4(ch, 0) (L3) */
        }
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

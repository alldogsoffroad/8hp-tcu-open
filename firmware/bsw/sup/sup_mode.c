/* sup_mode.c — the stock OS operating mode: the mode-0 → mode-1 switch
 * after engagement (sup_mode.h; ~/tcu_sweep/mode1_port.md). Every function
 * below was read from the bytes (sh-elf-objdump -b binary -m sh2a -EB of
 * the September image); [O] = seen in the bytes or the driving dump.
 *
 * WHAT IS PORTED (stock → here):
 *   0x66DA4 (mode part)  sup_mode_init     0x848CF = 0x93B34 = 0x93B33 = 0
 *   0x67A7C / 0x67AD4    sup_mode_background  the boot-state machine 0..3
 *   0x6F144 (part)       state 0: 0x93B8B == 1 → 0x848E6 = 0
 *   0x7DAA4 (part)       the startup-module manager, modules 1 and 2:
 *                        start records 0 ([1]) and 2 ([2, 1]), done record
 *                        0 ([1, 7, 11]) of tables 0x467D4 / 0x467EC
 *                        (counts 0x46937 = 5 / 0x46938 = 1, module pointer
 *                        table 0x46008); 0x93B8B = 1 once module 1 = 0x0F
 *   0x7DB70              row-1 list entry 32: module 2 = 0x0F, 0x93B8A =
 *                        (modules 1..12 all 0x0F)
 *   0x6F0B4 / 0x784B4 / 0x6F0FC / 0x6BC44 / 0x6BC60  sup_mode_request
 *   0x66E06              sup_mode_apply (0x9A428 / 0x9A3FC = the lock)
 *
 * NOT PORTED (no equivalent in our firmware; none touches bus B or the
 * supervisor path):
 *   - module manager start records 1 / 3 / 4 (modules 3, 7, 11) and module
 *     3's done (0x7DB9C, mode-0 10 ms list): module 11's start is stand-in
 *     S5 (sup_ladder.c); seq2 writes module 7 = 0x0F whatever its start
 *     state (0x85CF8 tests only != 0x0F), so module 7 reads 0 (stock 1)
 *     until 'U'.
 *   - the OS tick start in 0x67A7C / 0x6F144 (0x93B40, 0x91A30): ours runs
 *     from boot.
 *   - row-1 entries 1..31 (0x67140 stamp, the model / application inits
 *     0x14F32C, 0x9B2E8, 0xA2D5C .. 0xA0E3C).
 *   - 0x66E06's mode-0 exit work 0x67A08 (aligns the 20/30/50/100 ms
 *     countdowns 0x93548/54/60/42 to the 10 ms one 0x9353C — tasks that
 *     have no list we port) and 0x67BC2 (copies 0x90628 to 0x9357C/8A/98/
 *     B4); 0x6856C(1) and the 0x90A50[mode] duration (instrumentation).
 *   - the mode-1 row-9 init list 0x42FFC: 0x67140 twice (the 0x80C64
 *     stamp buffer) and 0x7DC26 (0xFFF80D1C = 1, the "last mode reached"
 *     marker 0x7DBEC reads at the next boot — below our RAM image).
 *   - state 3's background list (mode 1 row 0: 0x6AB8E) and its
 *     instrumentation (0x6856C(2), 0x6839C, 0x683C8).
 *   - the writers of 0x93B5E (the model's request, 0x68EFE from 0x1320D4,
 *     modes 1..3 only once mode != 0) and of 0x93B8F (the power-down
 *     manager 0x729C8 → mode 5): both stay 0, so mode 1 is final here.
 */
#include "bsw/sup/sup_mode.h"
#include "bsw/sup/sup_cells.h"
#include "platform.h"

/* witnesses (linker .sup_wit, free low RAM; set by sup_mode_init) */
static uint32_t s_sw_frt __attribute__((section(".sup_wit")));  /* TCNTA at the switch */
static uint8_t s_held __attribute__((section(".sup_wit")));     /* H5: mode refused */

/* 0x9A428 / 0x9A3FC: the OS lock around 0x66E06's body — raise SR.I to
 * at least 1 (cal 0x41174 = 0x10), nestable (sup_ladder.c's lock) */
static uint32_t mode_lock(void)
{
    const uint32_t prev = plat_sr_i();

    if (prev < 1u) {
        plat_sr_set(1u);
    }
    return prev;
}

static void mode_unlock(uint32_t prev)
{
    plat_sr_set(prev);
}

void sup_mode_init(void)                                     /* 0x66DA4 (part) */
{
    CELL_OS_MODE_BOOT = 0u;
    CELL_OS_MODE_PEND = 0u;
    CELL_OS_MODE = 0u;
    CELL_OS_STATE = 0u;
    /* [I strong] 0x848E6 starts nonzero: its only writers are 0x6F144
     * (= 0) and nothing else in the image (movi20 scan), it lies in the
     * 0xFFF84000 data-init copy, and the stock's own timestamps show state
     * 1 only after certification (0x80C64[4] = 170.633 ms vs the first
     * 10 ms task at 70.602 ms — driving dump). Zero would run the row-1
     * list on the first background pass. */
    CELL_OS_INIT_GATE = 1u;
    CELL_MOD1 = 0u;
    CELL_MOD2 = 0u;
    CELL_MOD_ALL = 0u;
    CELL_MOD1_UP = 0u;
    s_held = 0u;
    s_sw_frt = 0u;
}

/* 0x7DBB4: every dependency byte of a record reads 0x0F */
static bool deps_done(const uint8_t *dep, uint8_t n)
{
    bool ok = true;
    for (uint8_t i = 0u; i < n; i++) {
        if (*(volatile uint8_t *)(uintptr_t)(0xFFF93B00u + dep[i]) != 0x0Fu) {
            ok = false;
        }
    }
    return ok;
}

/* 0x7DAA4, modules 1 and 2 (the records on the mode-1 path) */
static void module_manager(void)
{
    /* the start loop (table 0x467D4): a module at 0 whose dependencies all
     * read 0x0F goes to 1. Rec 0: module 1, no dependencies; rec 2:
     * module 2 ← module 1. (Recs 1/3/4 — modules 3, 7, 11 — not ported.) */
    if (CELL_MOD1 == 0u) {
        CELL_MOD1 = 1u;
    }
    if (CELL_MOD2 == 0u) {
        static const uint8_t dep[] = { 0x7Eu };              /* module 1 */
        if (deps_done(dep, 1u)) {
            CELL_MOD2 = 1u;
        }
    }
    /* the done loop (table 0x467EC, one record): module 1 → 0x0F once
     * modules 7 and 11 read 0x0F, if it was started (not 0, not 0x0F) */
    if (CELL_MOD1 != 0u && CELL_MOD1 != 0x0Fu) {
        static const uint8_t dep[] = { 0x86u, 0x85u };       /* modules 7, 11 */
        if (deps_done(dep, 2u)) {
            CELL_MOD1 = 0x0Fu;
        }
    }
    /* 0x7DB3A: module 1 up → 0x93B8B */
    if (CELL_MOD1 == 0x0Fu) {
        CELL_MOD1_UP = 1u;
    }
    /* 0x7DB4C (module 3 → 0x93B8C, the OS tick start) and the pass counter
     * 0x92E10: not ported */
}

/* 0x7DB70: row-1 list entry 32 — module 2 done, and 0x93B8A = modules
 * 1..12 (pointer table 0x46008 entries 1..12) all 0x0F */
static void row1_7db70(void)
{
    static const uint8_t mods[12] = { 0x7Eu, 0x81u, 0x87u, 0x84u, 0x7Fu, 0x83u,
                                      0x86u, 0x88u, 0x80u, 0x89u, 0x85u, 0x82u };
    uint8_t all = 1u;

    CELL_MOD2 = 0x0Fu;
    for (uint8_t i = 0u; i < 12u; i++) {
        if (*(volatile uint8_t *)(uintptr_t)(0xFFF93B00u + mods[i]) != 0x0Fu) {
            all = 0u;
        }
    }
    CELL_MOD_ALL = all;
}

void sup_mode_background(void)
{
    /* 0x67A7C: state 0 runs the mode-0 background list 0x42C0C
     * {0x6F144, 0x7DAA4}, then leaves state 0 once 0x848E6 reads 0 */
    if (CELL_OS_STATE == 0u) {
        if (CELL_MOD1_UP == 1u) {                            /* 0x6F144 tail */
            CELL_OS_INIT_GATE = 0u;
        }
        module_manager();                                    /* 0x7DAA4 */
        if (CELL_OS_INIT_GATE == 0u) {
            CELL_OS_STATE = 1u;
        }
    }
    /* 0x67AD4: state 1 runs the row-1 list 0x42C38 once → state 2; state 2
     * → 3 once 0x848CF != 0 */
    if (CELL_OS_STATE == 1u) {
        row1_7db70();
        CELL_OS_STATE = 2u;
    } else if (CELL_OS_STATE == 2u) {
        if (CELL_OS_MODE_BOOT != 0u) {
            CELL_OS_STATE = 3u;
        }
    }
}

/* 0x6F0FC: the supervisor-side request */
static void req_6f0fc(void)
{
    uint8_t r = 0xFFu;
    const uint8_t cur = CELL_OS_REQ_CUR;

    if (cur == 0u) {
        r = (CELL_MOD2 == 0x0Fu) ? 1u : 0u;                  /* 0x6BC44 */
    } else if (cur != 1u && cur != 2u && cur != 3u && cur != 4u) {
        r = 3u;
    }
    if (CELL_OS_SHUTDOWN == 1u) {                            /* 0x6BC60 */
        r = 5u;
    }
    CELL_OS_REQ_SUP = r;
}

/* 0x784B4 */
static uint8_t req_784b4(void)
{
    CELL_OS_REQ_RES = CELL_OS_REQ_CUR;
    req_6f0fc();
    if (CELL_OS_REQ_CUR != 5u && CELL_OS_REQ_RES != 5u) {
        const uint8_t sup = CELL_OS_REQ_SUP;
        const uint8_t app = CELL_OS_REQ_APP;
        if (sup != 0xFFu) {
            CELL_OS_REQ_RES = sup;
        } else if (app != 0u && app <= 5u) {
            CELL_OS_REQ_RES = app;
        }
    }
    return CELL_OS_REQ_RES;
}

void sup_mode_request(void)                                  /* 0x6F0B4 */
{
    uint8_t r;

    CELL_OS_REQ_CUR = CELL_OS_MODE;
    r = req_784b4();
    if (CELL_OS_REQ_CUR != r && r != 0u && r < 6u) {
        CELL_OS_MODE_PEND = r;
    }
}

void sup_mode_apply(void)                                    /* 0x66E06 */
{
    const uint8_t p = CELL_OS_MODE_PEND;
    const uint8_t cur = CELL_OS_MODE_BOOT;

    if (p == cur || cur == 5u || p == 0u) {
        return;
    }
    if (SUP_MODE_HOLD) {
        s_held = p;                 /* H5: recorded, not applied (sup_mode.h) */
        return;
    }
    {
        const uint32_t lk = mode_lock();
        /* cur == 0: 0x67A08 / 0x67BC2 — not ported (header) */
        CELL_OS_MODE_BOOT = p;
        CELL_OS_MODE = p;
        /* the new mode's row-9 list (mode 1: 0x67140 ×2, 0x7DC26) — not
         * ported (header) */
        s_sw_frt = plat_frt_us();
        mode_unlock(lk);
    }
}

uint8_t sup_mode(void)
{
    return CELL_OS_MODE;
}

uint8_t sup_mode_held(void)
{
    return s_held;
}

uint32_t sup_mode_switch_frt(void)
{
    return s_sw_frt;
}

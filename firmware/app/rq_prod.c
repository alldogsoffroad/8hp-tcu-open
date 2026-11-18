/* rq_prod.c — M5-0 / M5-1: the stock's solenoid request PRODUCER for channels 0–6,
 * ported from the bytes of the pristine image (sh-elf-objdump -b binary
 * -m sh2a -EB; flash address = file offset) and checked against the stock
 * driving RAM dump stockram_driving_20261104.bin. app/rq_prod.h for the
 * input frame, bsw/drv/drv_rqprod.h for the interface, ~/tcu_sweep/
 * m5_0_port.md for the write-for-write table.
 *
 * PORTED (every comparison, constant and write from the bytes):
 *   0xDC4C4  head (Sys, WK) and the source-0 holder loop 0xDCBFA..0xDCC5C
 *   0xDCE40  head (lockup permitted?) — the lockup branch 0xDCD38.. NOT
 *   0xDCE9A, 0xDC1E2, 0xDC1C4, 0xD0C94, 0xD11EC, 0xD0C90 (B6BA8)
 *   0xE53DA  setter (checks, stores; fault 0x19A counted)
 *   0xE4E5C  element method — M5-1: the WHOLE method incl. the fill FSM
 *            0xE3836 and 0xE40DA (mode +7 = 0 until +0x46 = 10000) lives in
 *            app/rq_fill.c (~/tcu_sweep/m5_1_fill_port.md); 0xE54A4 Sys / WK
 *   0xE5548  the +6 clear (elements 1..5, 9, 10)
 *   0x107BD2 element actuator (0x107D90 state-0 entry, 0x107CFC, 0x107D8E;
 *            M5-1: mode 0 → 0x107C3E in rq_fill.c)
 *   0x107FB8 Sys / WK actuator (0x10809C, 0x10812E)
 *   0xD9CBC / 0xD9CEE i_min / i_max, 0xD9F70, 0xD9CF8 (at init AND at the
 *            start of every actuator pass, 0xDA00E)
 *   M5-1 cadence: the boot pass at boot state 1 (row-1 list entries 12 / 17)
 *            and the 10 ms task in OS modes 1..3 (tick 0xE5624, update
 *            0xE566C, then the producer) — no longer from the request law
 *
 * NOT PORTED (stand-ins; m5_0_port.md §3) — each unreachable with the
 * M5-0 inputs; where reachable on the host the branch counts in
 * s_m5_unp_n and yields current 0, which the request law's guard replaces:
 *   0xBABEE  line-pressure update (input: 0xBAA58 = RQ_M5_LINEP)
 *   0xDC51C..0xDCBFA  shift-phase sources ids 1..0x11 (input: none)
 *   0xDCD38.. the lockup (WK) branch of 0xDCE40; the 0xFFF8E6BF[0] store
 *   0xE56F0 / 0xE5706  the source log 0xFFF8E374 (no reader here)
 *   0x107D90 actuator fill FSM beyond its state-0 entry; 0x107FE0 (Sys / WK curve)
 *   0xD9C7C's diagnostic override 0xBB13E / 0xBB100 (none on our firmware)
 */
/* READ-ONLY park position sensor: this translation unit is an output path */
#define TCU_OUTPUT_PATH 1
#include "app/rq_prod.h"
#include "bsw/drv/drv_rqprod.h"

#if defined(TCU_RAM_APP)
/* the RAM application keeps the r03f constants (its 64 KB limit) */
#else

/* ── cells at the stock's RAM addresses (linker.ld .m5_*) ─────────────── */
#define U8(a, o)    (*(volatile uint8_t  *)(uintptr_t)((uint32_t)(a) + (o)))
#define U16(a, o)   (*(volatile uint16_t *)(uintptr_t)((uint32_t)(a) + (o)))
#define U32(a, o)   (*(volatile uint32_t *)(uintptr_t)((uint32_t)(a) + (o)))

/* request objects and their fields: app/rq_fill.h (RQ_OBJ_*, RO_*) */

/* actuator objects (constructed by 0x15BF2C: 0x107B80 elements 0x28 bytes,
 * 0x107F6E Sys / WK 0x10 bytes) */
#define AO_ID       0x00u           /* u8 channel = index into 0xFFF84B78 */
#define AO_TYPE     0x01u           /* u8 0 normally closed, 1 normally open */
#define AO_I        0x02u           /* u16 current (0.1 mA) */
#define AO_NI       0x04u           /* u16 ~current */
#define AO_REQ      0x0Cu           /* u32 request object */
#define AO_FILL     0x10u           /* u8 fill state (elements, 0x107D90) */
#define CUR_ARR     0xFFF84B78u     /* u16 [12] 0xD9F70 */
#define CUR_INV     0xFFF84B90u     /* u16 [12] its complement */

/* input mirrors (WRITE-ONLY: the stock cells of the compile-time inputs) */
#define MIR_GEAR    (*(volatile int8_t   *)0xFFF89C2Bu)
#define MIR_PFLOW   (*(volatile uint8_t  *)0xFFF89C30u)
#define MIR_SYSMODE (*(volatile uint8_t  *)0xFFF89CF5u)
#define MIR_LINEP   (*(volatile uint16_t *)0xFFF8BCB0u)
/* the temperature byte, LIVE (2026-11-17, app/tmp_chain.c — 0xB986C owns
 * 0x89C13, 0xB9646 owns 0x89CD2): read here, never written */
#define LIVE_TEMP   (*(const volatile uint8_t *)0xFFF89C13u)
#define LIVE_CD2    (*(const volatile uint8_t *)0xFFF89CD2u)
#define MIR_ENGSTOP (*(volatile uint8_t  *)0xFFF89D10u)     /* M5-1 */

typedef struct {
    uint32_t addr;          /* the stock's object address */
    uint8_t  id;            /* 0x15BF2C r5 */
    uint8_t  type;          /* 0x15BF2C r6 */
    uint32_t req;           /* 0x15BF2C r7 */
    bool     supply;        /* 0x107F6E (Sys / WK) rather than 0x107B80 */
} act_t;

/* 0x15BF2C, in its call order [O bytes 0x15BF2E..0x15BF88] */
static const act_t ACT[RQ_PROD_NCH] = {
    { 0xFFF8B154u, 0u, 0u, RQ_OBJ_A,   false },  /* A   mov #0,r5 / mov r5,r6     */
    { 0xFFF8B17Cu, 2u, 0u, RQ_OBJ_B,   false },  /* B   mov #2,r5 / mov #0,r6     */
    { 0xFFF8B1A4u, 4u, 1u, RQ_OBJ_C,   false },  /* C   mov #4,r5 / mov r13(1),r6 */
    { 0xFFF8B1CCu, 1u, 1u, RQ_OBJ_D,   false },  /* D   mov r13,r5 / mov r13,r6   */
    { 0xFFF8B1F4u, 3u, 1u, RQ_OBJ_E,   false },  /* E   mov #3,r5 / mov r13,r6    */
    { 0xFFF8B21Cu, 5u, 0u, RQ_OBJ_WK,  true  },  /* WK  mov #5,r5 / mov #0,r6     */
    { 0xFFF8B22Cu, 6u, 1u, RQ_OBJ_SYS, true  },  /* Sys mov #6,r5 / mov r13,r6    */
};

/* the element → request object table 0x507D8 (elements 1..5 used here) */
static const uint32_t ELEM_OBJ[6] = { 0u, RQ_OBJ_A, RQ_OBJ_B, RQ_OBJ_C, RQ_OBJ_D, RQ_OBJ_E };

/* ── const data copied from the stock image [O; asserted byte for byte by
 * the host test m5_stock_bytes against the image] ───────────────────────── */
/* 0x505A4: 2 blocks × 10 rows (gear code −1 R, 0 N, 1..8) × 6 bytes
 * (col 0 unused, cols 1..5 = A B C D E) */
const uint8_t RQ_T505A4[2u * 10u * 6u] = {
    /* block 0 (power flow open) */
    0,1,0,0,0,0,  0,0,0,0,0,0,  0,1,0,0,0,0,  0,1,0,0,0,0,  0,0,1,0,0,0,
    0,0,1,0,0,0,  0,0,1,0,0,0,  0,0,0,0,0,0,  0,1,0,0,0,0,  0,1,0,0,0,0,
    /* block 1 (power flow engaged) */
    0,1,1,0,1,0,  0,0,0,0,0,0,  0,1,1,1,0,0,  0,1,1,0,0,1,  0,0,1,1,0,1,
    0,0,1,0,1,1,  0,0,1,1,1,0,  0,0,0,1,1,1,  0,1,0,1,1,0,  0,1,0,0,1,1,
};
const uint8_t RQ_T63122[6] = { 0, 1, 0, 0, 0, 0 };    /* m == 5: A only */
const uint8_t RQ_T63128[6] = { 0, 0, 0, 0, 0, 0 };    /* park request + E6C3: all off */
const uint8_t RQ_T6314C[6] = { 0, 0, 0, 0, 0, 0 };    /* m == 2: all off */
/* 0x6312F[g], g = −1..8 (bytes 0x6312E..0x63137): the neutral-idle slip element */
const uint8_t RQ_T6312E[10] = { 2, 0, 2, 2, 5, 5, 4, 4, 4, 4 };

/* ── witnesses (.m5_wit, 0xFFF84D40: low RAM, not .bss) ────────────────── */
#if defined(__sh__)
#define WIT __attribute__((section(".m5_wit")))
#else
#define WIT
#endif
static volatile uint32_t s_m5_f19a_n WIT;
static volatile uint32_t s_m5_f19f_n WIT;
static volatile uint32_t s_m5_f1c4_n WIT;
static volatile uint32_t s_m5_unp_n WIT;

uint32_t rq_prod_f19a_n(void) { return s_m5_f19a_n; }
uint32_t rq_prod_f19f_n(void) { return s_m5_f19f_n; }
uint32_t rq_prod_f1c4_n(void) { return s_m5_f1c4_n; }
uint32_t rq_prod_unp_n(void)  { return s_m5_unp_n; }

/* the pass's inputs (a pointer to a const set: SH only ever passes M5_IN) */
static const rq_prod_in_t *s_in;

static const rq_prod_in_t M5_IN = {
    .gear = RQ_M5_GEAR, .pflow = RQ_M5_PFLOW, .gear_eng = RQ_M5_GEAR_ENG,
    .c2f = RQ_M5_C2F, .e28e = RQ_M5_E28E, .e298 = RQ_M5_E298,
    .park_req = RQ_M5_PARKREQ, .e6c3 = RQ_M5_E6C3, .b52f = RQ_M5_B52F,
    .sys_mode = RQ_M5_SYSMODE, .line_p = RQ_M5_LINEP,
    .shift_src = RQ_M5_SHIFTSRC, .r37 = RQ_M5_R37,
};

/* 0xBAA58: (s16)*0xFFF8BCB0 — the line-pressure result (input) */
static inline uint16_t lp(void) { return s_in->line_p; }

/* ── 0xD0C94 / 0xD11EC / 0xD0C90 ──────────────────────────────────────── */
static uint8_t d0c94(uint8_t st)
{
    /* jump table 0xD0CBC, braf base 0xD0CAA: 0x6C → 0xD0D16 (0), 0x36 →
     * 0xD0CE0 (2), 0x3A → 0xD0CE4 (5), 0x3E → 0xD0CE8 (4); > 0x11: fault
     * report 0xD0CEC, then 0 */
    if (st > 0x11u) {
        return 0u;
    }
    if (st >= 1u && st <= 6u) {
        return 2u;
    }
    if ((st >= 8u && st <= 10u) || st >= 15u) {
        return 5u;
    }
    if (st >= 12u && st <= 14u) {
        return 4u;
    }
    return 0u;                                  /* 0, 7, 11 */
}

static uint8_t d11ec(uint8_t st)
{
    return (st == 0u || st == 1u || st == 2u || st == 11u || st == 12u) ? (uint8_t)0u : (uint8_t)1u;
}

/* ── 0xDC1E2: is element e applied in this gear / block? ─────────────── */
static uint8_t dc1e2(uint8_t block, int8_t gear, uint8_t e)
{
    const uint8_t m = d0c94(s_in->e28e);                    /* 0xB6B9A */

    if (block == 0u || d11ec(s_in->e298) != 1u) {          /* 0xDC1F8..0xDC208 */
        if (s_in->park_req != 0u && s_in->e6c3 != 0u) {    /* 0xB6A90, @(4,param) */
            return RQ_T63128[e];
        }
        if (m == 5u) {
            return RQ_T63122[e];
        }
        if (m == 2u) {
            return RQ_T6314C[e];
        }
    } else {
        /* 0xDC1C4(param, (s8)*0x89C25) == e → 0 (the stock indexes
         * unchecked; an index outside −1..8 is ours to refuse) */
        const int gi = (int)s_in->gear_eng + 1;
        if (gi < 0 || gi > 9) {
            s_m5_unp_n++;
            return 0u;
        }
        if (RQ_T6312E[gi] == e) {
            return 0u;
        }
    }
    if ((int)gear < -1 || (int)gear > 8) {
        s_m5_unp_n++;                               /* outside the table: ours */
        return 0u;
    }
    /* 0xDC262: 0x505A4[e + (gear + 1)·6 + block·0x3C] */
    return RQ_T505A4[(unsigned)e + (unsigned)((int)gear + 1) * 6u + (unsigned)block * 0x3Cu];
}

/* ── 0xE54A4: the Sys / WK request method ─────────────────────────────── */
static void e54a4(uint32_t o)
{
    uint16_t r;

    if (U8(o, RO_MODE) != 0u) {
        const uint16_t p = U16(o, RO_P);
        const uint16_t line = lp();
        if (p >= line || p == 0u) {                 /* 0xE54BC / 0xE54C4 */
            r = p;
        } else {
            if (p < 12750u) {                       /* 0xE54C8: fault 0x19A (0x36) */
                s_m5_f19a_n++;
            }
            r = (o == RQ_OBJ_WK) ? 0u : line;       /* 0xE54FA */
        }
    } else {
        r = U16(o, RO_P);                           /* 0xE550E */
    }
    U16(o, RO_RES) = r;                             /* 0xE5510 */
    U16(o, RO_LIM) = r;
    /* 0xE5514..0xE5522: cmp/hi on the sign-extended words */
    if ((uint32_t)(int32_t)(int16_t)r > (uint32_t)(int32_t)(int16_t)SYS_RES) {
        U16(o, RO_LIM) = SYS_RES;
    }
}

/* ── 0xE4E5C: the element request method — app/rq_fill.c rqf_e4e5c (M5-1:
 * the whole method incl. the fill FSM) ──────────────────────────────── */

/* ── 0xE53DA: the request setter ──────────────────────────────────────── */
static void e53da(uint32_t o, uint16_t p, uint8_t mode, uint8_t prio, uint8_t src)
{
    if (src > 0x29u) {                              /* 0xE53E4: fault 0x19A (0xE1) */
        s_m5_f19a_n++;
    }
    if (prio > 4u) {                                /* 0xE541C: fault 0x19A (0xE5) */
        s_m5_f19a_n++;
    } else if (prio == 2u && mode != 0u) {          /* 0xE5420: fault 0x19A (0xE9) */
        s_m5_f19a_n++;
    }
    U8(o, RO_SET) = 1u;                             /* 0xE5476 */
    U16(o, RO_P) = p;                               /* 0xE547A */
    U8(o, RO_MODE) = mode;                          /* 0xE547C */
    U8(o, RO_SRC) = src;                            /* 0xE5480 */
    U8(o, RO_PRIO) = prio;                          /* 0xE5484 */
    if (o == RQ_OBJ_SYS || o == RQ_OBJ_WK) {        /* vtable 0x4A0A4 +0x14 */
        e54a4(o);
    } else {                                        /* vtable 0x4A008 +0x14 */
        rqf_e4e5c(o);
    }
    /* 0xE56F0(0xFFF8E374, src, +2): the source log, NOT ported */
}

/* ── 0xDCE40 (head): lockup permitted? ────────────────────────────────── */
static bool dce40(void)
{
    const bool head = (s_in->pflow == 1u && s_in->c2f == s_in->pflow) ||   /* 0xDCE4A..0xDCE5C */
                      s_in->b52f == 1u;                                     /* 0xBA2A4 */
    if (head) {
        s_m5_unp_n++;                               /* 0xDCD38.. the lockup branch: NOT ported */
    }
    return false;
}

/* ── 0xDC4C4: the producer ────────────────────────────────────────────── */
static void dc4c4(void)
{
    const uint8_t block = (s_in->pflow != 0u) ? 1u : 0u;   /* 0xDCC08 movrt */

    /* 0xBABEE(0xFFF8E61A) (line-pressure update): NOT ported — input */
    e53da(RQ_OBJ_SYS, lp(), s_in->sys_mode, 4u, 0x15u);    /* 0xDC4D8..0xDC4EE */

    if (!dce40()) {                                         /* 0xDC4F4..0xDC53C */
        e53da(RQ_OBJ_WK, 0u, 1u, 4u, 0x16u);
    }

    if (s_in->shift_src != 0u) {
        s_m5_unp_n++;                               /* 0xDC540..0xDCBFA: NOT ported */
    }

    /* the holder 0xDCBFA..0xDCC5C: elements 1..5 */
    {
        const uint8_t m = d0c94(s_in->e28e);        /* 0xDCBFE: 0xB6B9A */
        for (uint8_t e = 1u; e <= 5u; e++) {
            const uint32_t o = ELEM_OBJ[e];         /* 0xE55DC */
            bool hold = U8(o, RO_SET) == 0u;        /* 0xDCC0E */
            if (!hold && m != 0u) {
                /* 0xDCE9A: m == 5 && e == 1 && 0xB6BA8 (= 0xD0C90 → 0) == 0 */
                hold = !(m == 5u && e == 1u);
            }
            if (hold) {
                const uint16_t v = dc1e2(block, s_in->gear, e) != 0u ? 20000u : 0u;  /* 0xDCC3A */
                e53da(o, v, 1u, 4u, 0u);            /* 0xDCC42..0xDCC4E */
            }
        }
    }
    /* 0xBA6F4(0xFFF8E617, 0x33): NOT ported (no reader here) */

    /* 0xE5548: clear +6 of elements 1..5, 9, 10 (0xE5706 log NOT ported) */
    for (uint8_t e = 1u; e <= 5u; e++) {
        U8(ELEM_OBJ[e], RO_SET) = 0u;
    }
    U8(RQ_OBJ_SYS, RO_SET) = 0u;
    U8(RQ_OBJ_WK, RO_SET) = 0u;
}

/* ── the actuators ─────────────────────────────────────────────────────── */
static uint16_t d9cbc(const act_t *a)              /* i_min (reaction 37 → 0) */
{
    if (s_in->r37 != 0u && a->id != 6u && a->id != 5u) {    /* 0xD9CCC..0xD9CE0 */
        return 0u;
    }
    return RQ_CAL_IMIN;                             /* 0xD9CE2: **0xFFF84C94 */
}

static uint16_t d9cee(const act_t *a)              /* i_max: *(*0xFFF84C94 + 4) */
{
    (void)a;
    return RQ_CAL_IMAX;
}

/* 0x107CFC (elements) / 0x10809C (Sys / WK): the mode-1 binary law */
static uint16_t law_mode1(const act_t *a, uint16_t p)
{
    if (p == 0u) {
        return a->type != 0u ? d9cee(a) : d9cbc(a);         /* 0x107D06 / 0x1080A6 */
    }
    if (p < lp() && p < 12750u) {                           /* 0x107D18 / 0x107D20 */
        s_m5_f1c4_n++;                                      /* fault 0x1C4 (0xA7) / 0x1C2 (0xB3) */
        return d9cbc(a);
    }
    return a->type == 0u ? d9cee(a) : d9cbc(a);             /* 0x107D50 */
}

static uint16_t act_current(const act_t *a)
{
    const uint32_t r = a->req;
    const uint16_t p = U16(r, RO_RES);              /* 0x107BDA / 0x107FBE: result +2 */

    if (a->supply) {                                /* 0x107FB8 */
        if (U8(r, RO_MODE) == 0u) {
            s_m5_unp_n++;                           /* 0x107FE0 curve: NOT ported */
            return 0u;
        }
        return law_mode1(a, p);                     /* 0x10809C; 0x10812E identity */
    }
    if (U8(r, RO_MODE) == 1u) {                     /* 0x107BE2 */
        /* 0x107D90(act, p == 0), state-0 entry only (0x107E10..0x107E1E) */
        if (U8(a->addr, AO_FILL) != 0u ||
            (p == 0u && U8(r, RO_5A) == 1u)) {
            s_m5_unp_n++;                           /* the actuator fill FSM: NOT ported */
            return 0u;
        }
        return law_mode1(a, p);                     /* 0x107CFC; 0x107D8E identity */
    }
    /* M5-1: mode 0 — the curve law 0x107C3E (rq_fill.c), clamp 0x107D6A */
    return rqf_107c3e(a->id, a->type, p, d9cbc(a), d9cee(a));
}

/* 0xD9F70 */
static void d9f70(const act_t *a, uint16_t i)
{
    U16(a->addr, AO_I) = i;
    U16(a->addr, AO_NI) = (uint16_t)~i;
    U16(CUR_ARR, 2u * a->id) = U16(a->addr, AO_I);
    U16(CUR_INV, 2u * a->id) = U16(a->addr, AO_NI);
}

/* ── interface ─────────────────────────────────────────────────────────── */
/* ── M5-1: the fill inputs (compile-time on SH; rq_prod.h RQ_M5_*) ─────── */
static const rqf_in_t M5_FILL_IN = {
    .temp = 0u /* LIVE: 0x89C13, fill_in() */, .c1c = RQ_M5_C1C, .c0a = RQ_M5_C0A,
    .eng_stop = RQ_M5_ENGSTOP,
    .c3d = RQ_M5_C3D, .cd2 = 0u /* LIVE: 0x89CD2, fill_in() */, .e5c2 = RQ_M5_B119,
    .b0fe = RQ_M5_B0FE,
    .e3ed = RQ_M5_E3ED, .ba79c = RQ_M5_BA79C, .e6a3 = RQ_M5_E6A3, .rec1fd = RQ_M5_REC1FD,
    .lp = RQ_M5_LINEP, .lp2 = RQ_M5_LP2,
    .adapt38 = { RQ_M5_AD38_A, RQ_M5_AD38_B, 0, 0, 0 },          /* dump C/D/E +0x38 = 0 */
    .adapt42 = { RQ_M5_AD42_A, RQ_M5_AD42_B, RQ_M5_AD42_C, RQ_M5_AD42_D, RQ_M5_AD42_E },
    .ba7dc = { RQ_M5_BA7DC_AB, RQ_M5_BA7DC_AB, 0, 0, 0 },        /* C/D/E: open (never filled here) */
    .v10cadc = { 0u, 0u, 0u, 0u, 0u },                          /* dump +0x5C = 0 (A..E) */
    .k4b = { 3u, 4u, 6u, 8u, 9u },                              /* dump +0x4B (A..E) */
    .rec_frac = { RQ_M5_REC_A, RQ_M5_REC_B, RQ_M5_REC_C, RQ_M5_REC_D, RQ_M5_REC_E }, /* 0xFFF80EE8.. */
};
static const rqf_in_t *s_fill_in = &M5_FILL_IN;

/* the fill inputs of this pass: the compile-time set with the two LIVE
 * temperature cells (0x89C13, 0x89CD2) read in; the host's override set
 * (rq_prod_host_fill_in) as given */
#if defined(__sh__)
static rqf_in_t s_fill_live __attribute__((section(".m5_rec")));    /* low RAM: the stack guard */
#else
static rqf_in_t s_fill_live;
#endif
static const rqf_in_t *fill_in(void)
{
    if (s_fill_in != &M5_FILL_IN) {
        return s_fill_in;
    }
    s_fill_live = M5_FILL_IN;
    s_fill_live.temp = LIVE_TEMP;
    s_fill_live.cd2 = LIVE_CD2;
    return &s_fill_live;
}

/* ── the fill recorder (ours, .m5_rec at 0xFFF86000: low RAM, not .bss —
 * the stack guard leaves .noinit 0x30 bytes): the first RQ_REC_N producer
 * passes after boot, and again from the first pass whose element request
 * set differs from the previous pass (one re-arm) ───────────────────────── */
#if defined(__sh__)
#define REC __attribute__((section(".m5_rec")))
#else
#define REC
#endif
static volatile rq_rec_t s_m5_rec[RQ_REC_N] REC;
static volatile uint32_t s_m5_rec_n REC;          /* entries written */
static volatile uint32_t s_m5_pass REC;           /* producer passes since boot */
static volatile uint32_t s_m5_rec_rearm REC;      /* 0 armed, 1 re-armed once */
static volatile uint8_t  s_m5_rec_set REC;        /* last pass's applied set (bit e-1) */
static bool s_started;                            /* the producer has run (boot pass) */

static void rec_pass(void)
{
    uint8_t set = 0u;
    for (uint8_t e = 1u; e <= 5u; e++) {
        if (U16(ELEM_OBJ[e], RO_P) != 0u) {
            set = (uint8_t)(set | (1u << (e - 1u)));
        }
    }
    if (s_m5_pass > 0u && set != s_m5_rec_set && s_m5_rec_n >= RQ_REC_N && s_m5_rec_rearm == 0u) {
        s_m5_rec_rearm = 1u;            /* a later change: record it from here */
        s_m5_rec_n = 0u;
    }
    s_m5_rec_set = set;
    if (s_m5_rec_n < RQ_REC_N) {
        volatile rq_rec_t *r = &s_m5_rec[s_m5_rec_n];
        r->pass = (uint16_t)s_m5_pass;
        for (uint8_t e = 1u; e <= 5u; e++) {
            r->st[e - 1u] = U8(ELEM_OBJ[e], RO_ST);
            r->frac[e - 1u] = U16(ELEM_OBJ[e], RO_FRAC);
        }
        r->set = set;
        for (uint8_t ch = 0u; ch < RQ_PROD_NCH; ch++) {
            r->cur[ch] = U16(CUR_ARR, 2u * ch);
        }
        s_m5_rec_n++;
    }
    s_m5_pass++;
}

void rq_prod_init(void)
{
    /* 0xD9CF8: currents 0, complements 0xFFFF (12 each) */
    for (uint32_t i = 0u; i < 12u; i++) {
        U16(CUR_ARR, 2u * i) = 0u;
        U16(CUR_INV, 2u * i) = 0xFFFFu;
    }
    /* the request objects: cleared (.m5_req 0xFFF8B7C8..0xFFF8B9C7) */
    for (uint32_t a = 0xFFF8B7C8u; a < 0xFFF8B9C8u; a++) {
        U8(a, 0u) = 0u;
    }
    /* the actuator objects (0x15BF2C → 0xD9C54: +0 id, +1 type; +0xC req) */
    for (uint32_t a = 0xFFF8B154u; a < 0xFFF8B23Cu; a++) {
        U8(a, 0u) = 0u;
    }
    for (uint8_t k = 0u; k < RQ_PROD_NCH; k++) {
        U8(ACT[k].addr, AO_ID) = ACT[k].id;
        U8(ACT[k].addr, AO_TYPE) = ACT[k].type;
        U32(ACT[k].addr, AO_REQ) = ACT[k].req;
    }
    s_m5_f19a_n = 0u;
    s_m5_f19f_n = 0u;
    s_m5_f1c4_n = 0u;
    s_m5_unp_n = 0u;
    /* M5-1: the request objects A..E as 0x15BF2C constructs them (0xE3710
     * with the cal cells 0xFFF84D20 + 4·(e − 1)); the fill cells */
    RQF_CELL_851A2 = 0u;
    RQF_CELL_851A3 = 0u;
    RQF_CELL_851A5 = 0u;                /* [I] the stock's .bss start value */
    RQF_CELL_E384 = 0u;
    rqf_bind(fill_in(), &s_m5_unp_n, &s_m5_f19f_n);
    for (uint8_t e = 1u; e <= 5u; e++) {
        rqf_construct(ELEM_OBJ[e], 0xFFF84D20u + 4u * (uint32_t)(e - 1u), e);
    }
    /* the recorder */
    for (uint32_t i = 0u; i < RQ_REC_N; i++) {
        volatile uint8_t *b = (volatile uint8_t *)&s_m5_rec[i];
        for (uint32_t j = 0u; j < sizeof(rq_rec_t); j++) {
            b[j] = 0u;
        }
    }
    s_m5_rec_n = 0u;
    s_m5_pass = 0u;
    s_m5_rec_rearm = 0u;
    s_m5_rec_set = 0u;
    s_started = false;
}

/* one producer pass: 0xDC4C4, then the actuator pass 0xDA00C (0xD9CF8
 * clear, then 0xD9C7C per actuator: method → 0xD9F70) */
static void prod_pass(const rq_prod_in_t *in)
{
    s_in = in;
    rqf_bind(fill_in(), &s_m5_unp_n, &s_m5_f19f_n);
    dc4c4();
    for (uint32_t i = 0u; i < 12u; i++) {           /* 0xDA00E → 0xD9CF8 */
        U16(CUR_ARR, 2u * i) = 0u;
        U16(CUR_INV, 2u * i) = 0xFFFFu;
    }
    for (uint8_t k = 0u; k < RQ_PROD_NCH; k++) {
        d9f70(&ACT[k], act_current(&ACT[k]));
    }
    s_started = true;
    rec_pass();
}

#if !defined(__sh__)
static bool s_host_frozen;
void rq_prod_host_freeze(bool frozen) { s_host_frozen = frozen; }
static rqf_in_t s_fill_host;
void rq_prod_host_fill_in(const rqf_in_t *in)
{
    if (in == 0) {
        s_fill_in = &M5_FILL_IN;
    } else {
        s_fill_host = *in;
        s_fill_in = &s_fill_host;
    }
    rqf_bind(fill_in(), &s_m5_unp_n, &s_m5_f19f_n);
}
#endif

static void mirrors(void)
{
    /* the stock cells of the compile-time inputs: WRITE-ONLY mirrors */
    MIR_GEAR = (int8_t)RQ_M5_GEAR;
    MIR_PFLOW = (uint8_t)RQ_M5_PFLOW;
    MIR_SYSMODE = (uint8_t)RQ_M5_SYSMODE;
    MIR_LINEP = (uint16_t)RQ_M5_LINEP;
    MIR_ENGSTOP = (uint8_t)RQ_M5_ENGSTOP;
}

void rq_prod_step(void)
{
#if !defined(__sh__)
    if (s_host_frozen) {
        return;                         /* host: lets a test corrupt 0x84B78 */
    }
#endif
    mirrors();
    prod_pass(&M5_IN);
}

/* boot state 1 (0x67AD4, row-1 list 0x4D7B0): entry 12 0xA2DEE → 0xE5602
 * (0xE5106 → 0xE4A4E ×5), entry 17 0xA2E74 → the producer + actuators */
void rq_prod_boot_pass(void)
{
    rqf_bind(fill_in(), &s_m5_unp_n, &s_m5_f19f_n);
    rqf_e5602();
    rq_prod_step();
}

/* the 10 ms task in OS modes 1..3 (lists 0x4D834 / 0x4DA2C / 0x4DC24):
 * entry 29 0xA2EE0 → 0xE5624 (tick) then 0xE566C (update), entry 43
 * 0xA30E2 → the producer + actuators. Not in mode 0 (no mode-0 list holds
 * them) nor before the boot pass. */
void rq_prod_task_10ms(uint8_t os_mode)
{
#if !defined(__sh__)
    if (s_host_frozen) {
        return;
    }
#endif
    if (os_mode < 1u || os_mode > 3u || !s_started) {
        return;
    }
    rqf_bind(fill_in(), &s_m5_unp_n, &s_m5_f19f_n);
    rqf_e5624();
    rqf_e566c();
    rq_prod_step();
}

#if !defined(__sh__)
/* host only: one full 10 ms pass (tick, update, producer) with an input set */
void rq_prod_step_in(const rq_prod_in_t *in)
{
    static rqf_in_t f;
    f = *fill_in();
    f.lp = in->line_p;
    rqf_bind(&f, &s_m5_unp_n, &s_m5_f19f_n);
    rqf_e5624();
    rqf_e566c();
    {
        const rqf_in_t *keep = s_fill_in;
        s_fill_in = &f;
        prod_pass(in);
        s_fill_in = keep;
    }
}
#endif

bool rq_prod_started(void)
{
    return s_started;
}

uint8_t rq_prod_fill_state(uint8_t ch)
{
    if (ch >= 5u) {
        return 0xFFu;                   /* WK / Sys: no element FSM */
    }
    for (uint8_t k = 0u; k < RQ_PROD_NCH; k++) {
        if (ACT[k].id == ch) {          /* ACT[] is in construction order */
            return U8(ACT[k].req, RO_ST);
        }
    }
    return 0xFFu;
}

uint32_t rq_prod_rec_n(void) { return s_m5_rec_n; }
uint32_t rq_prod_pass_n(void) { return s_m5_pass; }
const volatile rq_rec_t *rq_prod_rec(uint32_t i)
{
    return (i < RQ_REC_N) ? &s_m5_rec[i] : 0;
}

uint16_t rq_prod_ch(uint8_t ch)
{
    if (ch >= RQ_PROD_NCH) {
        return RQ_PROD_INVALID;
    }
    {
        const uint16_t c = U16(CUR_ARR, 2u * ch);
        const uint16_t n = U16(CUR_INV, 2u * ch);
        return (uint16_t)~n == c ? c : RQ_PROD_INVALID;
    }
}

#endif /* !TCU_RAM_APP */

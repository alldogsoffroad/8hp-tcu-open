/* rq_prod.c — M5-0: the stock's solenoid request PRODUCER for channels 0–6,
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
 *   0xE4E5C  element method up to 0xE4ED2 (+0x59 clear, mode-1 check,
 *            +2 / +4) and 0xE4490 (+0x53); 0xE54A4 Sys / WK method
 *   0xE5548  the +6 clear (elements 1..5, 9, 10)
 *   0x107BD2 element actuator (0x107D90 state-0 entry, 0x107CFC, 0x107D8E)
 *   0x107FB8 Sys / WK actuator (0x10809C, 0x10812E)
 *   0xD9CBC / 0xD9CEE i_min / i_max, 0xD9F70, 0xD9CF8 (array init)
 *
 * NOT PORTED (stand-ins; m5_0_port.md §3) — each unreachable with the
 * M5-0 inputs; where reachable on the host the branch counts in
 * s_m5_unp_n and yields current 0, which the request law's guard replaces:
 *   0xBABEE  line-pressure update (input: 0xBAA58 = RQ_M5_LINEP)
 *   0xDC51C..0xDCBFA  shift-phase sources ids 1..0x11 (input: none)
 *   0xDCD38.. the lockup (WK) branch of 0xDCE40; the 0xFFF8E6BF[0] store
 *   0xE4ED6.. the rest of 0xE4E5C (0xE4424, the priority / application
 *            state machine 0xE4EF6..0xE5030 incl. 0xE40DA, which writes
 *            +7 = 0) and 0xE4522 (called by 0xE4490 on a +0x53 change):
 *            the only +2 / +4 rewrite in that tail (0xE4FDA / 0xE4FDE)
 *            needs +0x59 == 1, set only at 0xE4F36 under mode == 0 — never
 *            in mode 1; 0xE40DA needs 0xE4DBA() == 0 (+0x46 != 10000) with
 *            +0x53 set — the dump holds B's +0x46 = 0x2710 (see §3: the
 *            stock's transient after an element first activates is open)
 *   0xE56F0 / 0xE5706  the source log 0xFFF8E374 (no reader here)
 *   0x107D90 fill FSM beyond its state-0 entry; 0x107C3E / 0x107FE0 curves
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

/* request objects (0x507D8 table; 0x60 bytes for A..E, 0x10 for Sys / WK) */
#define RQ_OBJ_A    0xFFF8B7C8u     /* element 1 */
#define RQ_OBJ_B    0xFFF8B828u     /* element 2 */
#define RQ_OBJ_C    0xFFF8B888u     /* element 3 */
#define RQ_OBJ_D    0xFFF8B8E8u     /* element 4 */
#define RQ_OBJ_E    0xFFF8B948u     /* element 5 */
#define RQ_OBJ_SYS  0xFFF8B9A8u     /* element 9 */
#define RQ_OBJ_WK   0xFFF8B9B8u     /* element 10 */
#define RO_P        0x00u           /* u16 requested pressure (mbar) */
#define RO_RES      0x02u           /* u16 result */
#define RO_LIM      0x04u           /* u16 min(result, Sys result) */
#define RO_SET      0x06u           /* u8 set this cycle */
#define RO_MODE     0x07u           /* u8 */
#define RO_SRC      0x08u           /* u8 source id */
#define RO_PRIO     0x09u           /* u8 */
#define RO_ACTIVE   0x53u           /* u8 0xE4490 (elements) */
#define RO_59       0x59u           /* u8 0xE4E5C clears at entry (elements) */
#define RO_5A       0x5Au           /* u8 fill request (0xE4424; never set by M5-0) */
#define SYS_RES     U16(RQ_OBJ_SYS, RO_RES)     /* 0xFFF8B9AA */

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
    .shift_src = RQ_M5_SHIFTSRC, .coding = RQ_M5_CODING,
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

/* ── 0xE4E5C: the element request method (head, to 0xE4ED2) ──────────── */
static void e4e5c(uint32_t o)
{
    U8(o, RO_59) = 0u;                              /* 0xE4E6A */
    if (U8(o, RO_MODE) != 0u) {
        const uint16_t p = U16(o, RO_P);
        uint16_t r;
        if (p >= 20000u || p == 0u) {               /* 0xE4E82 / 0xE4E88: mode 1 is binary */
            r = p;
        } else {
            s_m5_f19f_n++;                          /* 0xE4EA8: fault 0x19F */
            r = lp();                               /* 0xE4EB6: replaced by the line pressure */
        }
        U16(o, RO_RES) = r;                         /* 0xE4EC0 */
        U16(o, RO_LIM) = r;                         /* 0xE4EC4 */
        if (r > SYS_RES) {                          /* 0xE4ECC: zero-extended, signed cmp */
            U16(o, RO_LIM) = SYS_RES;
        }
    }
    /* 0xE4490: +0x53 = !(p == 0 && mode != 0); 0xE4522 on a change NOT ported */
    U8(o, RO_ACTIVE) = (U16(o, RO_P) == 0u && U8(o, RO_MODE) != 0u) ? (uint8_t)0u : (uint8_t)1u;
    /* 0xE4424 and the tail 0xE4EF6.. NOT ported (file header) */
}

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
        e4e5c(o);
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
static uint16_t d9cbc(const act_t *a)              /* i_min */
{
    if (s_in->coding != 0u && a->id != 6u && a->id != 5u) {    /* 0xD9CCC..0xD9CE0 */
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
            s_m5_unp_n++;                           /* the fill FSM: NOT ported */
            return 0u;
        }
        return law_mode1(a, p);                     /* 0x107CFC; 0x107D8E identity */
    }
    s_m5_unp_n++;                                   /* 0x107C3E curve: NOT ported */
    return 0u;
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
}

static void prod_pass(const rq_prod_in_t *in)
{
    s_in = in;
    dc4c4();
    /* 0xD9C7C per actuator (override 0xBB13E: none) → method → 0xD9F70 */
    for (uint8_t k = 0u; k < RQ_PROD_NCH; k++) {
        d9f70(&ACT[k], act_current(&ACT[k]));
    }
}

#if !defined(__sh__)
static bool s_host_frozen;
void rq_prod_host_freeze(bool frozen) { s_host_frozen = frozen; }
#endif

void rq_prod_step(void)
{
#if !defined(__sh__)
    if (s_host_frozen) {
        return;                         /* host: lets a test corrupt 0x84B78 */
    }
#endif
    /* the stock cells of the compile-time inputs: WRITE-ONLY mirrors */
    MIR_GEAR = (int8_t)RQ_M5_GEAR;
    MIR_PFLOW = (uint8_t)RQ_M5_PFLOW;
    MIR_SYSMODE = (uint8_t)RQ_M5_SYSMODE;
    MIR_LINEP = (uint16_t)RQ_M5_LINEP;
    prod_pass(&M5_IN);
}

#if !defined(__sh__)
void rq_prod_step_in(const rq_prod_in_t *in)
{
    prod_pass(in);
}
#endif

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

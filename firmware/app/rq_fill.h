/* rq_fill.h — M5-1: the stock ELEMENT FILL (switch-on transient) of the
 * request objects A..E, and the actuator mode-0 current law, ported from
 * the bytes (app/rq_fill.c; ~/tcu_sweep/m5_1_fill_port.md, decode
 * ~/tcu_sweep/m5_transient_decode.md). Internal interface between
 * app/rq_prod.c (the producer, M5-0) and app/rq_fill.c.
 *
 * Every request object A..E (0x60 bytes at 0xFFF8B7C8 + 0x60·n) carries a
 * fill state machine at +0x34 (0 off, 1 quick fill, 2 fill ramp, 3 applied,
 * 4 release wait, 5 release ramp, 6 constructed) whose ramp +0x46 runs
 * 0 → 10000. While +0x46 != 10000 with the element requested, 0xE40DA
 * writes the request mode +7 = 0, so the actuator runs the curve law
 * 0x107C3E on the fill pressure 0xE4214 writes into +2.
 */
#ifndef TCU_APP_RQ_FILL_H
#define TCU_APP_RQ_FILL_H

#include <stdint.h>
#include <stdbool.h>

/* ── cell access at the stock's RAM addresses ──────────────────────────── */
#define RQF_U8(a, o)    (*(volatile uint8_t  *)(uintptr_t)((uint32_t)(a) + (o)))
#define RQF_S8(a, o)    (*(volatile int8_t   *)(uintptr_t)((uint32_t)(a) + (o)))
#define RQF_U16(a, o)   (*(volatile uint16_t *)(uintptr_t)((uint32_t)(a) + (o)))
#define RQF_S16(a, o)   (*(volatile int16_t  *)(uintptr_t)((uint32_t)(a) + (o)))
#define RQF_U32(a, o)   (*(volatile uint32_t *)(uintptr_t)((uint32_t)(a) + (o)))
#define RQF_F32(a, o)   (*(volatile float    *)(uintptr_t)((uint32_t)(a) + (o)))

/* request objects (0x507D8 table) */
#define RQ_OBJ_A    0xFFF8B7C8u     /* element 1 */
#define RQ_OBJ_B    0xFFF8B828u     /* element 2 */
#define RQ_OBJ_C    0xFFF8B888u     /* element 3 */
#define RQ_OBJ_D    0xFFF8B8E8u     /* element 4 */
#define RQ_OBJ_E    0xFFF8B948u     /* element 5 */
#define RQ_OBJ_SYS  0xFFF8B9A8u     /* element 9 */
#define RQ_OBJ_WK   0xFFF8B9B8u     /* element 10 */

/* request object fields (elements; 0xE3710 constructor, 0xE4E5C ...) */
#define RO_P        0x00u   /* u16 requested pressure (mbar) */
#define RO_RES      0x02u   /* u16 result (the actuator reads it) */
#define RO_LIM      0x04u   /* u16 min(result, Sys result) */
#define RO_SET      0x06u   /* u8  set this cycle */
#define RO_MODE     0x07u   /* u8  0 curve, 1 on/off */
#define RO_SRC      0x08u   /* u8  source id */
#define RO_PRIO     0x09u   /* u8 */
#define RO_VPTR     0x0Cu   /* u32 vtable 0x4A008 (stored, not dereferenced) */
#define RO_CAL      0x10u   /* u32 the RAM pointer cell of the cal block */
#define RO_T14      0x14u   /* u32 timers (0xE39DC, 0xE49B8, 0xE3C1E) */
#define RO_T18      0x18u
#define RO_T1C      0x1Cu
#define RO_T20      0x20u
#define RO_FLT      0x24u   /* filter: u16 time stamp; float state at +0x28 */
#define RO_FLTV     0x28u
#define RO_T        0x2Cu   /* u16 time in state (ms; tick += 10) */
#define RO_QF       0x2Eu   /* u16 quick-fill credit */
#define RO_C30      0x30u   /* u8  applied counter */
#define RO_C31      0x31u   /* u8 */
#define RO_ELEM     0x32u   /* u8  element 1..5 */
#define RO_IDX      0x33u   /* u8  element - 1 */
#define RO_ST       0x34u   /* u8  fill state */
#define RO_TFILL    0x36u   /* u16 fill time (ms) */
#define RO_ADAPT    0x38u   /* u16 */
#define RO_TQF      0x3Au   /* u16 quick-fill time */
#define RO_TRAMP    0x3Cu   /* u16 ramp time = fill - quick fill */
#define RO_TREL     0x3Eu   /* u16 release ramp time */
#define RO_PMOD     0x40u   /* u16 modelled pressure */
#define RO_PKISS    0x42u   /* u16 kiss pressure (0x10CBFC) */
#define RO_44       0x44u
#define RO_FRAC     0x46u   /* u16 applied fraction 0..10000 */
#define RO_FRAC0    0x48u   /* u16 ramp start */
#define RO_4A       0x4Au
#define RO_4B       0x4Bu
#define RO_4C       0x4Cu
#define RO_4D       0x4Du
#define RO_4E       0x4Eu
#define RO_4F       0x4Fu
#define RO_50       0x50u
#define RO_51       0x51u
#define RO_52       0x52u
#define RO_ACTIVE   0x53u
#define RO_54       0x54u
#define RO_55       0x55u
#define RO_56       0x56u
#define RO_57       0x57u
#define RO_BOOST    0x58u
#define RO_59       0x59u
#define RO_5A       0x5Au
#define RO_5B       0x5Bu
#define RO_5C       0x5Cu   /* u16 0x10CADC value */
#define SYS_RES     RQF_U16(RQ_OBJ_SYS, RO_RES)     /* 0xFFF8B9AA */

/* fill states (0xE3836) */
#define RQF_ST_OFF      0u
#define RQF_ST_QFILL    1u
#define RQF_ST_RAMP     2u
#define RQF_ST_APPLIED  3u
#define RQF_ST_RELWAIT  4u
#define RQF_ST_RELRAMP  5u
#define RQF_ST_CTOR     6u

/* the stock cells this machinery owns (linker.ld .m5_fl / .m5_e384) */
#define RQF_CELL_851A2  (*(volatile uint8_t *)0xFFF851A2u)  /* fill bits, byte 0 */
#define RQF_CELL_851A3  (*(volatile uint8_t *)0xFFF851A3u)  /* fill bits, byte 1 */
#define RQF_CELL_851A5  (*(volatile uint8_t *)0xFFF851A5u)  /* = 0x89C3D at the update end */
#define RQF_CELL_E384   (*(volatile uint8_t *)0xFFF8E384u)  /* last element filled (0xE4214) */

/* ── the inputs the fill reads that our firmware does not produce ──────
 * (rq_prod.h RQ_M5_* — compile-time on SH, asserted against the dump) */
typedef struct {
    uint8_t  temp;          /* 0xFFF89C13 (°C + 40 [I]) */
    uint8_t  c1c;           /* 0xFFF89C1C (fill-pressure map y) */
    uint8_t  c0a;           /* 0xFFF89C0A (fill-pressure offset map y) */
    uint8_t  eng_stop;      /* 0xFFF89D10 engine stopped (freezes B) */
    uint8_t  c3d;           /* 0xFFF89C3D */
    uint8_t  cd2;           /* 0xFFF89CD2 (0xE4424) */
    uint8_t  e5c2;          /* 0xFFF8B119 via 0xB803C */
    uint8_t  b0fe;          /* 0xFFF8B0FE via 0xB8054 */
    uint8_t  e3ed;          /* 0xFFF8E3ED via 0xBA78C (0xE4522) */
    uint8_t  ba79c;         /* 0xBA79C → 0xEAC5E(0xFFF8E3EC) (element E only; not decoded) */
    uint8_t  e6a3;          /* 0xFFF8E6A3 via 0xB7B46 */
    uint8_t  rec1fd;        /* 0xBC82C: parameter record 0x1FD ready (B's +0x46 restore) */
    uint16_t lp;            /* 0xFFF8BCB0 via 0xBAA58 (= the M5-0 line-pressure input) */
    uint16_t lp2;           /* 0xFFF8BCC0 via 0xBAA5E (0xD9F62) */
    int16_t  adapt38[5];    /* 0xAEAE2(0xFFF8D5DC, e) A..E */
    uint16_t adapt42[5];    /* 0x10CBFC(0xFFF8E6F3, ...) A..E */
    int16_t  ba7dc[5];      /* 0xBA7DC → 0x10CFB6(0xFFF8D1B4, e) A..E */
    uint16_t v10cadc[5];    /* 0x10CADC(e) → +0x5C A..E */
    uint8_t  k4b[5];        /* +0x4B (vtable +0x90 setter 0xE4CEE) A..E */
    uint16_t rec_frac[5];   /* parameter record 0x1FD +0xC0 (only with rec1fd) */
} rqf_in_t;

/* ── const flash copies (app/rq_fill_cal.c, generated) ─────────────────── */
typedef struct {
    uint32_t addr;
    uint32_t len;
    const uint8_t *data;
} rqf_blob_t;
#define RQF_NBLOBS  7u
extern const rqf_blob_t RQF_BLOBS[RQF_NBLOBS];

/* flash reads by stock address (big-endian, as the SH reads them); an
 * address outside the copies counts in rq_prod_unp_n and reads 0 */
uint8_t  rqf_fl8(uint32_t a);
uint16_t rqf_fl16(uint32_t a);
uint32_t rqf_fl32(uint32_t a);

/* ── the per-object machinery (rq_fill.c) ──────────────────────────────── */
void rqf_bind(const rqf_in_t *in, volatile uint32_t *unp, volatile uint32_t *f19f);
void rqf_construct(uint32_t o, uint32_t cal_cell, uint8_t elem);    /* 0xE3710 */
void rqf_e4e5c(uint32_t o);                                         /* 0xE4E5C */
void rqf_e5602(void);                                               /* boot: 0xE5106 ×5 */
void rqf_e5624(void);                                               /* tick: 0xE49B8 ×5 */
void rqf_e566c(void);                                               /* update: 0xE3816 ×5 */
/* 0x107C3E: the element mode-0 current law (curve + corrections + clamp) */
uint16_t rqf_107c3e(uint8_t id, uint8_t type, uint16_t p, uint16_t imin, uint16_t imax);

/* the stock interpolators (exported for the host tests) */
uint16_t rqf_b6eae(uint32_t map, uint16_t x);
int32_t  rqf_b6e10(uint32_t map, int16_t x);
uint8_t  rqf_b6d6c(uint32_t map, uint8_t x);
int32_t  rqf_b7164(uint32_t map, int16_t x, int16_t y);

/* the RAM cal pointer cells the stock dereferences (0xFFF84D14..0x84D30,
 * dump values; not materialised at their RAM addresses: .pk_wit lives
 * there) */
uint32_t rqf_cell_ptr(uint32_t cell);

/* the record 0x1FD shadow word of element idx (0..4): 0xBCBD4 / 0xBCC00 */
uint16_t rqf_rec_frac(uint8_t idx);

#endif /* TCU_APP_RQ_FILL_H */

/* rq_prod.h — M5-0: the stock's solenoid REQUEST PRODUCER for channels 0–6
 * (A D B E C WK Sys), ported from the bytes (app/rq_prod.c;
 * ~/tcu_sweep/m5_0_port.md, m5_request_producer_decode.md):
 *   0xDC4C4  the producer (Sys 0x15, WK 0x16, the source-0 holder loop)
 *   0xDC1E2  the element-table lookup (table 0x505A4, overrides 0x63122 /
 *            0x63128 / 0x6314C, neutral idle 0x6312F)
 *   0xE53DA  the request setter; 0xE4E5C / 0xE54A4 the request methods
 *            (mode-1 range check, result +2, +4 = min(+2, Sys result))
 *   0xE5548  the end-of-pass clear of the "set this cycle" bytes
 *   0x107BD2 / 0x107FB8  the actuator methods → mode-1 law 0x107CFC /
 *            0x10809C (500 / 8500 by actuator type), 0xD9CBC / 0xD9CEE
 *   0xD9F70  the per-actuator current → 0xFFF84B78[id] (+ ~ at +0x18)
 *
 * ── THE INPUTS (M5-0: the stock driving dump, compile-time) ──────────────
 * On SH every producer input is a compile-time constant (RQ_M5_*, below),
 * asserted against the dump; there is no runtime input source, no setter,
 * no UDS / FE path and no read of a RAM cell into them. The stock cells of
 * the ones the stock reads directly (0x89C2B, 0x89C30, 0x89CF5, 0x8BCB0)
 * are written as WRITE-ONLY mirrors for the bench read (never read back).
 * The host has rq_prod_step_in() to exercise other rows of the table.
 */
#ifndef TCU_APP_RQ_PROD_H
#define TCU_APP_RQ_PROD_H

#include <stdint.h>
#include <stdbool.h>

/* ── the M5-0 input set: the stock driving dump stockram_driving_20261104 ──
 * (P, engine off, this bench). Each value [O dump]; the address is where
 * the stock reads it. Overridable only to make the SH compile checks fail. */
#ifndef RQ_M5_GEAR
#  define RQ_M5_GEAR      3     /* 0xFFF89C2B gear code (−1 R, 0 N, 1..8) */
#endif
#ifndef RQ_M5_PFLOW
#  define RQ_M5_PFLOW     0     /* 0xFFF89C30 power-flow state → block (≠ 0) */
#endif
#ifndef RQ_M5_GEAR_ENG
#  define RQ_M5_GEAR_ENG  3     /* 0xFFF89C25 engaged gear (neutral-idle slip lookup) */
#endif
#ifndef RQ_M5_C2F
#  define RQ_M5_C2F       0     /* 0xFFF89C2F (0xDCE40 lockup test) */
#endif
#ifndef RQ_M5_E28E
#  define RQ_M5_E28E      0     /* 0xFFF8E28E engagement machine → 0xD0C94 */
#endif
#ifndef RQ_M5_E298
#  define RQ_M5_E298      0     /* 0xFFF8E298 neutral-idle machine → 0xD11EC */
#endif
#ifndef RQ_M5_PARKREQ
#  define RQ_M5_PARKREQ   1     /* 0xFFF8AE29 park request (0xB6A90) */
#endif
#ifndef RQ_M5_E6C3
#  define RQ_M5_E6C3      0     /* 0xFFF8E6C3 (= 0xDC4C4's param 0xFFF8E6BF + 4) */
#endif
#ifndef RQ_M5_B52F
#  define RQ_M5_B52F      0     /* 0xFFF8B52F (0xBA2A4, 0xDCE40 lockup test) */
#endif
#ifndef RQ_M5_SYSMODE
#  define RQ_M5_SYSMODE   1     /* 0xFFF89CF5 Sys request mode */
#endif
#ifndef RQ_M5_LINEP
#  define RQ_M5_LINEP     20000 /* 0xFFF8BCB0 line-pressure result (0xBAA58; reason
                                 * byte +0x18 = 0x01, forced maximum) */
#endif
#ifndef RQ_M5_SHIFTSRC
#  define RQ_M5_SHIFTSRC  0     /* shift-phase sources ids 1..0x11 active (dump: none) */
#endif
#ifndef RQ_M5_CODING
#  define RQ_M5_CODING    0     /* 0xA031C(*0x632CB = 0x37): the i_min coding flag
                                 * (dump: A reads 500 → 0) */
#endif

/* calibration [O bytes]: group 0xFFF84C94 → ROM 0x537F8 → 0x1FEF6C / 0x1FEF6E */
#define RQ_CAL_IMIN     500u
#define RQ_CAL_IMAX     8500u

/* a producer input set (host: rq_prod_step_in; SH: the constant M5-0 set) */
typedef struct {
    int8_t   gear;      /* 0xFFF89C2B */
    uint8_t  pflow;     /* 0xFFF89C30 */
    int8_t   gear_eng;  /* 0xFFF89C25 */
    uint8_t  c2f;       /* 0xFFF89C2F */
    uint8_t  e28e;      /* 0xFFF8E28E */
    uint8_t  e298;      /* 0xFFF8E298 */
    uint8_t  park_req;  /* 0xFFF8AE29 */
    uint8_t  e6c3;      /* 0xFFF8E6C3 */
    uint8_t  b52f;      /* 0xFFF8B52F */
    uint8_t  sys_mode;  /* 0xFFF89CF5 */
    uint16_t line_p;    /* 0xFFF8BCB0 */
    uint8_t  shift_src; /* 0: no shift-phase source */
    uint8_t  coding;    /* 0xA031C(0x37) */
} rq_prod_in_t;

#if defined(__sh__)
#  if defined(RQ_PROD_INPUT_SOURCE) && (RQ_PROD_INPUT_SOURCE != 0)
#    error "rq_prod: a runtime producer input source is refused on SH builds (M5-0: compile-time inputs only)"
#  endif
   /* no runtime input path on the target: any use is a compile error */
#  define rq_prod_step_in(in) \
     _Static_assert(0, "rq_prod: no runtime producer input on SH builds (M5-0 safety frame)")
#else
/* host only: one producer pass with an arbitrary input set */
void rq_prod_step_in(const rq_prod_in_t *in);
/* host only: rq_prod_step() does nothing while frozen (guard tests) */
void rq_prod_host_freeze(bool frozen);
#endif

/* the stock tables copied from the image (rq_prod.c; the host test
 * m5_stock_bytes compares them with the image byte for byte) */
extern const uint8_t RQ_T505A4[120];    /* 0x505A4 */
extern const uint8_t RQ_T63122[6];      /* 0x63122 */
extern const uint8_t RQ_T63128[6];      /* 0x63128 */
extern const uint8_t RQ_T6314C[6];      /* 0x6314C */
extern const uint8_t RQ_T6312E[10];     /* 0x6312E (0x6312F[g], g = −1..8) */

/* the M5-0 input set, asserted against the dump (SH and host alike) */
_Static_assert(RQ_M5_GEAR == 3, "rq_prod: the M5-0 gear code is 3 (dump 0xFFF89C2B), nothing else");
_Static_assert(RQ_M5_PFLOW == 0, "rq_prod: the M5-0 power-flow state is 0 (dump 0xFFF89C30), nothing else");
_Static_assert(RQ_M5_GEAR_ENG == 3 && RQ_M5_C2F == 0 && RQ_M5_E28E == 0 && RQ_M5_E298 == 0 &&
               RQ_M5_PARKREQ == 1 && RQ_M5_E6C3 == 0 && RQ_M5_B52F == 0 &&
               RQ_M5_SYSMODE == 1 && RQ_M5_LINEP == 20000 && RQ_M5_SHIFTSRC == 0 &&
               RQ_M5_CODING == 0,
               "rq_prod: every M5-0 input equals the stock driving dump");
_Static_assert(RQ_CAL_IMIN == 0x01F4u && RQ_CAL_IMAX == 0x2134u,
               "rq_prod: i_min / i_max = cal 0x1FEF6C / 0x1FEF6E (500 / 8500)");

#endif /* TCU_APP_RQ_PROD_H */

/* drv_rqprod.h — the request law's view of the M5-0 request PRODUCER
 * (app/rq_prod.c; ~/tcu_sweep/m5_0_port.md). Like drv_park.h for ch 7:
 * the only way the producer's channel 0–6 currents reach the request law.
 *
 * The producer ends at the stock's actuator current array 0xFFF84B78[id]
 * (+ inverted copy 0xFFF84B90, 0xD9F70); the copy stages 0xA3646 →
 * 0xFFF850D6, 0xBC0F4 → 0xFFF89EAC and 0x7B6D8 → 0xFFF84890 → 0xFFF94EE8
 * are NOT ported (identity in the stock driving dump: all four arrays read
 * 01F4 2134 2134 2134 2134 01F4 01F4). drv_reqlaw.c's rq_request() takes
 * rq_prod_ch(ch) for ch 0..6 THROUGH ITS GUARD: on any value other than
 * 500 / 8500 the r03f value of that channel is used instead and counted.
 */
#ifndef TCU_DRV_RQPROD_H
#define TCU_DRV_RQPROD_H

/* THE RANGE CHAIN IS ISOLATED (app/rng_chain.c, ours): its translation
 * unit defines TCU_RNG_CHAIN_TU and may not include an output-side header —
 * its only way to the outputs is the range D6D6, read by app/park.c */
#if defined(TCU_RNG_CHAIN_TU)
#error "the range chain is isolated: it may not include an output-side header (drv_rqprod.h)"
#endif

#include <stdint.h>
#include <stdbool.h>

#define RQ_PROD_NCH         7u          /* ch 0..6: A D B E C WK Sys */
#define RQ_PROD_INVALID     0xFFFFu     /* complement mismatch (never a current) */

/* 0xD9CF8 (array init) + our request / actuator object init; from
 * drv_rq_init (after the cells are cleared) */
void rq_prod_init(void);

/* one producer pass (0xDC4C4, then every actuator method 0xD9C7C) with the
 * M5-0 compile-time inputs; from the top of drv_rq_law() */
void rq_prod_step(void);

/* 0xFFF84B78[ch] when it equals ~0xFFF84B90[ch], else RQ_PROD_INVALID */
uint16_t rq_prod_ch(uint8_t ch);

/* M5-1: the producer runs at the stock cadence, not from the law:
 * rq_prod_boot_pass() at boot state 1 (sup_mode_row1_app), then
 * rq_prod_task_10ms(mode) from the 10 ms task (OS modes 1..3 only).
 * rq_prod_step() is one producer + actuator pass (no tick / update). */
void rq_prod_boot_pass(void);
void rq_prod_task_10ms(uint8_t os_mode);
bool rq_prod_started(void);
/* the element FSM state (+0x34) behind channel ch (0..4); 0xFF for 5/6 */
uint8_t rq_prod_fill_state(uint8_t ch);

/* the fill recorder (ours, .m5_rec 0xFFF86000): one entry per producer
 * pass, the first RQ_REC_N passes after boot (re-armed once by a change of
 * the applied element set) */
#define RQ_REC_N    64u
typedef struct {
    uint16_t pass;          /* producer pass since boot (0 = the boot pass) */
    uint8_t  st[5];         /* FSM +0x34, A..E */
    uint8_t  set;           /* elements requested (bit e-1) */
    uint16_t frac[5];       /* +0x46, A..E */
    uint16_t cur[7];        /* 0xFFF84B78[ch], ch 0..6 (A D B E C WK Sys) */
} rq_rec_t;
_Static_assert(sizeof(rq_rec_t) == 32u, "rq_rec_t is 32 bytes");
uint32_t rq_prod_rec_n(void);
uint32_t rq_prod_pass_n(void);
const volatile rq_rec_t *rq_prod_rec(uint32_t i);

/* our witnesses (.m5_wit, 0xFFF84D40; not stock cells) */
uint32_t rq_prod_f19a_n(void);     /* 0xE53DA / 0xE54A4 internal fault 0x19A */
uint32_t rq_prod_f19f_n(void);     /* 0xE4E5C mode-1 range fault 0x19F */
uint32_t rq_prod_f1c4_n(void);     /* 0x107CFC / 0x10809C faults 0x1C4 / 0x1C2 */
uint32_t rq_prod_unp_n(void);      /* an unported branch was reached (fail-safe 0) */

#endif /* TCU_DRV_RQPROD_H */

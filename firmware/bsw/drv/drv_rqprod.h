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

#include <stdint.h>

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

/* our witnesses (.m5_wit, 0xFFF84D40; not stock cells) */
uint32_t rq_prod_f19a_n(void);     /* 0xE53DA / 0xE54A4 internal fault 0x19A */
uint32_t rq_prod_f19f_n(void);     /* 0xE4E5C mode-1 range fault 0x19F */
uint32_t rq_prod_f1c4_n(void);     /* 0x107CFC / 0x10809C faults 0x1C4 / 0x1C2 */
uint32_t rq_prod_unp_n(void);      /* an unported branch was reached (fail-safe 0) */

#endif /* TCU_DRV_RQPROD_H */

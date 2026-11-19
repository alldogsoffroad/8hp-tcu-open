/* gs_dual.h — G6-0 two-instance identity (2026-11-19, ~/tcu_sweep/
 * g6_0_port.md): instance 2 of the garage engine template (the candidate,
 * app/gs_cand.c) driven beside instance 1 (the shadow, app/gs_shadow.c)
 * with the same gs_in_t every pass; after each pass every state block of
 * the two instances (gs_*_host_state: the engine statics, the request
 * objects relative to their base, the fill cells, the per-pass latches),
 * the fill instances' counters and rec_frac, and the API's outputs must be
 * byte-identical. A harness calls gs_dual_init() after gs_shadow_init(),
 * gs_dual_tick(&gi) after gs_shadow_tick_10ms(&gi) and gs_dual_standins()
 * wherever it sets the shadow's host stand-ins. */
#ifndef TCU_TESTS_GS_DUAL_H
#define TCU_TESTS_GS_DUAL_H

#include "app/gs_shadow.h"

#include <stdint.h>

void     gs_dual_init(void);
void     gs_dual_tick(const gs_in_t *gi);
void     gs_dual_standins(uint8_t mask, int16_t bd76, uint8_t bd30);
uint32_t gs_dual_passes(void);      /* passes compared */
uint32_t gs_dual_bad(void);         /* passes with a difference */
void     gs_dual_report(const char *where);

#endif /* TCU_TESTS_GS_DUAL_H */

/* gs_cand.c — INSTANCE 2 of the garage engine template (app/gs_engine.c):
 * the future G6a CANDIDATE, HOST-ONLY in G6-0 (app/gs_cand_bind.h; 2026-11-19,
 * ~/tcu_sweep/g6_0_port.md). The same engine text as app/gs_shadow.c, bound
 * to the candidate's own request objects and its own fill instance
 * (app/gs_cand_fill.c, gcf_*), every export renamed gs_cand_*. Driven with
 * the same gs_in_t as instance 1 it must hold the same state, byte for
 * byte, every pass (tests/unit/gs_dual.c). */
#include "app/gs_cand_bind.h"
#include "app/gs_engine.c"
#include "app/gs_cand.h"    /* the gs_cand_* prototypes, checked against the renamed definitions */

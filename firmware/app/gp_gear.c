/* gp_gear.c — THE COUPLED GEAR FEEDBACK CHAIN of the READ-ONLY gear-pair
 * shadow (app/gp_shadow.h; 2026-11-20, ~/tcu_sweep/gp_shadow_port.md).
 *
 * app/sm_gear.c (the stock 10 ms slots 30..33, ported from the bytes in
 * r04w) compiled a SECOND time with SMG_COUPLED = 1: its own state (gf_t) on
 * its own RAM .gp_gear, its own function names, and the shift object
 * 0xFFF8BD14's stand-ins bound to app/gp_shadow.c (see sm_gear.c's GF_*
 * hooks). The r04w instance (app/sm_gear.c itself, .sm_gear) is unchanged —
 * its object code is byte-identical to before. Nothing live is written. */
#define TCU_GP_SHADOW_TU 1
#define SMG_COUPLED 1
#define SMG_SECTION ".gp_gear"
#define sm_gear_init     gp_gear_init
#define sm_gear_pass     gp_gear_pass
#define sm_gear_state    gp_gear_state
#define sm_gear_host_si  gp_gear_host_si
#include "app/sm_gear.c"

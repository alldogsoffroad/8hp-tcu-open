/* SH-only compile check of the M4 group-A safety frame (CMakeLists:
 * ga_sh_guard_*): built with __sh__ defined, -fsyntax-only, against
 * bsw/drv/drv_groupa.h and platform/platform.h. The control (stage 0, no
 * SH_GUARD_* macro) and stages 1 / 2 must compile; a runtime demand source,
 * the host setters, a Timer E group-A write from any TU but drv_groupa.c,
 * a demand on any slot but 11, and any park-hold value but the stage's are
 * refused. */
#include "bsw/drv/drv_groupa.h"
#include "platform/platform.h"

void sh_guard_ga_probe(void);
void sh_guard_ga_probe(void)
{
#if defined(SH_GUARD_GA_SETTER)
    drv_ga_demand_set(1u, 10000u);          /* the host demand setter */
#endif
#if defined(SH_GUARD_GA_TESTS)
    drv_ga_tests_set(true);                 /* the host test release */
#endif
#if defined(SH_GUARD_GA_WRITER)
    plat_te_ga_drld(0x0Du, 1036u);          /* TOE31 DRLD outside drv_groupa.c */
#endif
#if defined(SH_GUARD_GA_CYCLE)
    plat_te_ga_cycle(0x0Du, 1036u, 2000u);
#endif
    (void)plat_te_ga_dtr(0x0Du);            /* reading is allowed (witness) */
}

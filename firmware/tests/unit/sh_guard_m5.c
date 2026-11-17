/* SH-only compile check (CMakeLists: m5_sh_guard_*), built with __sh__
 * defined, -fsyntax-only: the M5-0 producer's input frame. The control
 * (no SH_GUARD_* macro, no RQ_* override) must compile; a runtime input
 * path, a runtime input source, or any M5-0 input other than the dump's
 * value must not. */
#include "app/rq_prod.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/drv/drv_reqlaw.h"

uint16_t sh_guard_m5_probe(void);
uint16_t sh_guard_m5_probe(void)
{
#if defined(SH_GUARD_M5_RUNTIME)
    static const rq_prod_in_t in = { .gear = 1 };
    rq_prod_step_in(&in);                   /* the host-only input path */
#endif
#if defined(SH_GUARD_M5_FILLIN)
    static const rqf_in_t fin = { .temp = 0x50 };
    rq_prod_host_fill_in(&fin);             /* the host-only fill-input path */
#endif
    rq_prod_step();
    return rq_prod_ch(2u);
}

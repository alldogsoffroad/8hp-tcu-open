/* SH-only compile check (CMakeLists: rq_sh_guard_*): built with __sh__
 * defined, -fsyntax-only. The control (no SH_GUARD_* macro) must compile;
 * each SH_GUARD_* variant is a request source the safety frame refuses. */
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_cells.h"

uint16_t sh_guard_probe(void);
uint16_t sh_guard_probe(void)
{
#if defined(SH_GUARD_SETTER)
    drv_rq_request_set(1u, 8500u);          /* the host setter */
#endif
#if defined(SH_GUARD_CELL)
    CELL_RQ_REQ[1] = 8500u;                 /* a direct write to 0xFFF94EE8 */
#endif
#if defined(SH_GUARD_INJECT)
    drv_rq_corr_inject(1u, 4);
#endif
    return CELL_RQ_REQ[1];                  /* reading is allowed (witness) */
}

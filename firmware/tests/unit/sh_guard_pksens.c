/* SH-only compile check of the park position sensor's read-only frame
 * (CMakeLists: pks_sh_guard_*): -fsyntax-only with __sh__. The control
 * (no macro) includes both sensor headers and reads a cell; with
 * TCU_OUTPUT_PATH defined (as every output translation unit does) the
 * headers must refuse to compile. Also a fixture for the source scan: with
 * PKSENS_SCAN_FIXTURE the file carries the forbidden names, and the scan
 * must fail on it. */
#if defined(SH_GUARD_PKS_OUTPUT)
#define TCU_OUTPUT_PATH 1
#endif
#include "bsw/drv/drv_pksens.h"
#include "safety/park_pair.h"

void sh_guard_pksens_probe(void);
void sh_guard_pksens_probe(void)
{
    (void)CELL_PKP_POS;                 /* 0xFFF8EA40 — reading is the point */
    (void)CELL_PKS_BAND[2];
}

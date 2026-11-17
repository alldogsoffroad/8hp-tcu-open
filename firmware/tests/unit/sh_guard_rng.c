/* SH-only compile check of the range shadow's read-only frame (CMakeLists:
 * rng_sh_guard_*): -fsyntax-only with __sh__. The control includes the
 * shadow header and reads its cells; with TCU_OUTPUT_PATH (as every output
 * translation unit defines) the shadow header must refuse; with
 * TCU_RNG_SHADOW_TU (as the shadow translation unit defines) a live cell
 * header must refuse. Also the fixture of both source scans: it names the
 * shadow API (output side) and a live cell header and macro (shadow side),
 * so each negative scan must fail on it. */
#if defined(SH_GUARD_RNG_OUTPUT)
#define TCU_OUTPUT_PATH 1
#endif
#if defined(SH_GUARD_RNG_LIVE)
#define TCU_RNG_SHADOW_TU 1
#include "app/park_cells.h"
#endif
#include "app/rng_shadow.h"

void sh_guard_rng_probe(void);
void sh_guard_rng_probe(void)
{
    (void)rng_shadow_cells();           /* reading the shadow is the point */
#if defined(RNG_SCAN_FIXTURE_NEVER_DEFINED)
    PCELL_TGT = 8u;                     /* scan fixture only: never compiled */
#endif
}

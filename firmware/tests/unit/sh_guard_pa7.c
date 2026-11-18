/* sh_guard_pa7.c — the PA7 shadow's SH compile checks (CMakeLists "THE PA7
 * SHADOW"): the control compiles; each -D below must FAIL with the
 * read-only frame's message.
 *   SH_GUARD_PA7_OUTPUT       an output translation unit includes pa7_shadow.h
 *   SH_GUARD_PA7_CTRL         the C9 composer (bsw/sup/sup_ctrl.c, an output
 *                             path) includes pa7_shadow.h
 *   SH_GUARD_PA7_LIVE_<hdr>   the shadow translation unit includes a live
 *                             header (sup_cells.h, sup_ctrl.h, rng_chain.h,
 *                             park_cells.h, platform.h) */
#if defined(SH_GUARD_PA7_OUTPUT)
#define TCU_OUTPUT_PATH 1
#include "app/pa7_shadow.h"
#elif defined(SH_GUARD_PA7_CTRL)
#include "bsw/sup/sup_ctrl.c"
#include "app/pa7_shadow.h"
#elif defined(SH_GUARD_PA7_LIVE_CELLS)
#define TCU_PA7_SHADOW_TU 1
#include "app/pa7_shadow.h"
#include "bsw/sup/sup_cells.h"
#elif defined(SH_GUARD_PA7_LIVE_CTRL)
#define TCU_PA7_SHADOW_TU 1
#include "app/pa7_shadow.h"
#include "bsw/sup/sup_ctrl.h"
#elif defined(SH_GUARD_PA7_LIVE_RNG)
#define TCU_PA7_SHADOW_TU 1
#include "app/pa7_shadow.h"
#include "app/rng_chain.h"
#elif defined(SH_GUARD_PA7_LIVE_PARK)
#define TCU_PA7_SHADOW_TU 1
#include "app/pa7_shadow.h"
#include "app/park_cells.h"
#elif defined(SH_GUARD_PA7_LIVE_PLAT)
#define TCU_PA7_SHADOW_TU 1
#include "app/pa7_shadow.h"
#include "platform/platform.h"
#else
#include "app/pa7_shadow.h"
#endif
int sh_guard_pa7(void) { return (int)sizeof(pa7s_t); }

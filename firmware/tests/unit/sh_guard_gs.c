/* sh_guard_gs.c — the garage shadow's SH compile checks (CMakeLists
 * "garage shadow"): the control compiles; each -D below must FAIL with the
 * read-only frame's message.
 *   SH_GUARD_GS_OUTPUT   an output translation unit includes gs_shadow.h
 *   SH_GUARD_GS_LIVE     the shadow translation unit includes a live header
 *   GS_SHADOW_HOST_HOOKS the host stand-in overrides on SH
 *   GS_FORCED=2          the forced-commit switch is 0 or 1 */
#if defined(SH_GUARD_GS_OUTPUT)
#define TCU_OUTPUT_PATH 1
#include "app/gs_shadow.h"
#elif defined(SH_GUARD_GS_LIVE)
#define TCU_GS_SHADOW_TU 1
#include "app/gs_shadow.h"
#include "app/rq_fill.h"
#else
#include "app/gs_shadow.h"
#endif
int sh_guard_gs(void) { return GS_FORCED; }

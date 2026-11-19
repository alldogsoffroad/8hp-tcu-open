/* sh_guard_tsel.c — the ECU-temperature shadow's SH compile checks
 * (CMakeLists "THE ECU-TEMPERATURE SHADOW"): the control compiles; each -D
 * below must FAIL with the read-only frame's message.
 *   SH_GUARD_TSEL_OUTPUT       an output translation unit includes tsel_shadow.h
 *   SH_GUARD_TSEL_OUTTU        a real output translation unit (bsw/drv/drv_pa7.c,
 *                              the PA7 pin driver — it needs no libc header, so
 *                              it compiles freestanding; drv_odiag.c's
 *                              <string.h> does not) includes tsel_shadow.h
 *   SH_GUARD_TSEL_DRVTSEL      stage 2a's output TU (bsw/drv/drv_tsel.c, which
 *                              writes the live temperature cells) includes
 *                              tsel_shadow.h
 *   SH_GUARD_TSEL_RAMLIVE      TSEL_LIVE=1 in a RAM-app (or loader) build
 *                              (drv_tsel.h: "for the code-flash application
 *                              only")
 *   SH_GUARD_TSEL_RAMFLAGS     TSEL_LIVE_FLAGS=1 (stage 2b) in a RAM-app build
 *   SH_GUARD_TSEL_FLAGSNOLIVE  TSEL_LIVE_FLAGS=1 with TSEL_LIVE=0 (2b needs 2a)
 *   SH_GUARD_TSEL_LIVE_<hdr>   the shadow translation unit includes a live
 *                              header (sup_cells.h, drv_cells.h, drv_temp.h,
 *                              drv_odiag.h, platform.h) */
#if defined(SH_GUARD_TSEL_OUTPUT)
#define TCU_OUTPUT_PATH 1
#include "app/tsel_shadow.h"
#elif defined(SH_GUARD_TSEL_OUTTU)
#include "bsw/drv/drv_pa7.c"
#include "app/tsel_shadow.h"
#elif defined(SH_GUARD_TSEL_DRVTSEL)
#include "bsw/drv/drv_tsel.c"
#include "app/tsel_shadow.h"
#elif defined(SH_GUARD_TSEL_RAMLIVE)
#define TCU_RAM_APP 1
#define TSEL_LIVE 1
#include "bsw/drv/drv_tsel.h"
#include "app/tsel_shadow.h"
#elif defined(SH_GUARD_TSEL_RAMFLAGS)
#define TCU_RAM_APP 1
#define TSEL_LIVE_FLAGS 1
#include "bsw/drv/drv_tsel.h"
#include "app/tsel_shadow.h"
#elif defined(SH_GUARD_TSEL_FLAGSNOLIVE)
#define TSEL_LIVE 0
#define TSEL_LIVE_FLAGS 1
#include "bsw/drv/drv_tsel.h"
#include "app/tsel_shadow.h"
#elif defined(SH_GUARD_TSEL_LIVE_CELLS)
#define TCU_TSEL_SHADOW_TU 1
#include "app/tsel_shadow.h"
#include "bsw/sup/sup_cells.h"
#elif defined(SH_GUARD_TSEL_LIVE_DCELLS)
#define TCU_TSEL_SHADOW_TU 1
#include "app/tsel_shadow.h"
#include "bsw/drv/drv_cells.h"
#elif defined(SH_GUARD_TSEL_LIVE_TEMP)
#define TCU_TSEL_SHADOW_TU 1
#include "app/tsel_shadow.h"
#include "bsw/drv/drv_temp.h"
#elif defined(SH_GUARD_TSEL_LIVE_ODIAG)
#define TCU_TSEL_SHADOW_TU 1
#include "app/tsel_shadow.h"
#include "bsw/drv/drv_odiag.h"
#elif defined(SH_GUARD_TSEL_LIVE_PLAT)
#define TCU_TSEL_SHADOW_TU 1
#include "app/tsel_shadow.h"
#include "platform/platform.h"
#else
#include "app/tsel_shadow.h"
#endif
int sh_guard_tsel(void) { return (int)sizeof(tsel_t); }

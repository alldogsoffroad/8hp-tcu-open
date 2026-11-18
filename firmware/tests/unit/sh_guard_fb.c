/* sh_guard_fb.c — the feedback shadow's SH compile checks (CMakeLists
 * "feedback shadow"): the control compiles; each -D below must FAIL with the
 * read-only frame's message.
 *   SH_GUARD_FB_OUTPUT   an output translation unit includes fb_shadow.h
 *   SH_GUARD_FB_LIVE     the shadow translation unit includes a live header
 *   SH_GUARD_FB_CAL      an output translation unit includes fb_cal.h */
#if defined(SH_GUARD_FB_OUTPUT)
#define TCU_OUTPUT_PATH 1
#include "app/fb_shadow.h"
#elif defined(SH_GUARD_FB_LIVE)
#define TCU_FB_SHADOW_TU 1
#include "app/fb_shadow.h"
#include "bsw/drv/drv_odiag.h"
#elif defined(SH_GUARD_FB_CAL)
#define TCU_OUTPUT_PATH 1
#include "app/fb_cal.h"
#else
#include "app/fb_shadow.h"
#endif
int sh_guard_fb(void) { return (int)sizeof(fbs_t); }

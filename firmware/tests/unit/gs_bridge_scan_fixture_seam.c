/* NEGATIVE FIXTURE (ctest gs_bridge_scan_seam_negative), scanned AS
 * bsw/drv/drv_reqlaw.c: the seam call outside `#if GS_LIVE_DIR != 0` —
 * the scan must FAIL */
#define TCU_OUTPUT_PATH 1
#include "bsw/drv/drv_gsbr.h"
static unsigned short rq_prod_guarded(unsigned char ch)
{
    unsigned short v = 500u;
#if DRV_RQ_USE_PRODUCER
    if (ch < 5u && drv_gsbr_take(ch, &v)) {
        return v;
    }
#endif
    return v;
}

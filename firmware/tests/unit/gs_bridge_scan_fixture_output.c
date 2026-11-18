/* NEGATIVE FIXTURE (ctest gs_bridge_scan_outputs_negative): an output path
 * reading the garage bridge outside the one seam — the scan must FAIL */
#define TCU_OUTPUT_PATH 1
#include "bsw/drv/drv_gsbr.h"
static unsigned fixture_read(void)
{
    unsigned short v = 0u;
    (void)drv_gsbr_take(2u, &v);        /* not inside #if GS_LIVE_DIR != 0 */
    return v;
}

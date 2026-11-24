/* lp_scan_fixture_nlp.c - NEGATIVE fixture of lp_ti_shadow_scan.py outputs (2026-11-23): an
 * output path that names the Nissan line-pressure live seam - must be refused (only the
 * monitor app/gs_bridge.c may, inside an #if DRV_RQ_SYS_LIVE block). Never compiled. */
#define TCU_OUTPUT_PATH 1
#include "app/nissan_lp.h"
#include <stdint.h>

uint16_t fixture_sys(void)
{
    return nissan_lp_sys_request();
}

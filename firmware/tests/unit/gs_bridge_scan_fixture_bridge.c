/* NEGATIVE FIXTURE (ctest gs_bridge_scan_bridge_negative): a bridge that
 * writes a live cell and calls a live writer — the scan must FAIL */
#include "app/gs_bridge.h"
#include "bsw/drv/drv_reqlaw.h"
static void fixture_write(void)
{
    *(volatile unsigned short *)0xFFF84B78u = 9000u;
    drv_rq_request_set(2u, 9000u);
}

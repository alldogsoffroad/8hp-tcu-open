/* NEGATIVE FIXTURE (fb_shadow_scan.py shadow): a shadow that includes a live
 * header and writes a live cell — the scan must FAIL on it */
#include "bsw/drv/drv_odiag.h"
void fixture_shadow(void) { *(volatile unsigned char *)0xFFF93A2Eu = 0x10u; drv_od_tick_head(); }

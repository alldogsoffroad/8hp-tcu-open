/* negative fixture (gp_shadow_scan.py shadow): the shadow writing the live
 * shift object and calling a live module */
#include "bsw/sup/sup_cells.h"
void fixture_shadow(void) { *(volatile unsigned char *)0xFFF8BD15u = 0u; drv_rq_request_set(0u, 0u); }

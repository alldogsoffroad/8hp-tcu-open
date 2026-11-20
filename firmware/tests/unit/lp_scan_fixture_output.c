/* negative fixture (lp_ti_shadow_scan.py outputs): an output path naming the line-pressure shadow */
#include "app/lp_shadow.h"
int fixture_output(void) { return lp_shadow_state()->sys_i; }

/* negative fixture (gp_shadow_scan.py outputs): an output path naming the
 * gear-pair shadow's shift object */
#include "app/gp_shadow.h"
int fixture_output(void) { return gp_shadow_state()->bd15; }

/* negative fixture (fr_shadow_scan.py outputs): an output path naming the
 * fault-reaction shadow's verdict */
#include "app/fr_shadow.h"
int fixture_output(void) { return fr_shadow_state()->cd.c43; }

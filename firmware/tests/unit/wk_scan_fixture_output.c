/* negative fixture (wk_shadow_scan.py outputs): an output translation unit naming the lock-up shadow */
#include "app/wk_shadow.h"
int fixture_output(void) { return wk_shadow_state()->wk_i; }

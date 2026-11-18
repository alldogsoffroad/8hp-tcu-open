/* NEGATIVE FIXTURE (pa7_shadow_scan.py outputs): an output path (the C9
 * composer) that reads the PA7 shadow — the scan must FAIL on it */
#include "app/pa7_shadow.h"
int fixture_out(void) { return pa7_shadow_state()->c9b7 != 0 ? 0x80 : 0; }

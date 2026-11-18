/* NEGATIVE FIXTURE (fb_shadow_scan.py outputs): an output path that reads
 * the feedback shadow — the scan must FAIL on it */
#include "app/fb_shadow.h"
int fixture_out(void) { return fb_shadow_state()->cls[0]; }

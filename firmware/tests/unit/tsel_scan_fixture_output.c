/* NEGATIVE FIXTURE (tsel_shadow_scan.py outputs): an output path (the
 * diagnosis) that reads the temperature shadow — the scan must FAIL on it */
#include "app/tsel_shadow.h"
int fixture_out(void) { return tsel_shadow_state()->s0fc >= 6u; }

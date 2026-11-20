/* negative fixture (2026-11-19): an output path naming the gear feedback
 * chain (must FAIL) */
#include "app/sm_shadow.h"
int fixture(void) { return sm_gear_state()->e4a7; }

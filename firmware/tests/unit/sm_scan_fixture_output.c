/* negative fixture: an output path naming the shift-map shadow (must FAIL) */
#include "app/sm_shadow.h"
unsigned fixture(void) { return sm_shadow_state()->a697; }

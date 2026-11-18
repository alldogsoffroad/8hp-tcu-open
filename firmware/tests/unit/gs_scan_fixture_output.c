/* negative fixture: an output path that names the garage shadow — the
 * outputs scan must FAIL on this file */
#define TCU_OUTPUT_PATH 1
#include "app/gs_shadow.h"
uint16_t leak(void) { return gs_shadow_cur(2); }

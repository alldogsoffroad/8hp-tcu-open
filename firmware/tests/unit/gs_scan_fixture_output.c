/* negative fixture: an output path that names the garage shadow — the
 * outputs scan must FAIL on this file */
#define TCU_OUTPUT_PATH 1
#include "app/gs_shadow.h"
uint16_t leak(void) { return gs_shadow_cur(2); }
/* G4a: naming the release object must also fail the outputs scan */
static const volatile void *const g4a_bad = (const volatile void *)0xFFF8872Cu;

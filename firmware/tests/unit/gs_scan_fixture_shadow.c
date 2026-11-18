/* negative fixture: a shadow that writes a live cell and calls a live
 * module — the shadow scan must FAIL on this file */
#define TCU_GS_SHADOW_TU 1
#include "app/rq_prod.h"
void leak(void)
{
    *(volatile unsigned short *)0xFFF84B78u = 8500u;
    rq_prod_step();
}

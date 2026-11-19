/* negative fixture: the shadow reading a live cell and calling a live module (must FAIL) */
#include "bsw/drv/drv_cells.h"
unsigned fixture(void) { drv_rq_law(); return *(volatile unsigned char *)0xFFF89C2Bu; }

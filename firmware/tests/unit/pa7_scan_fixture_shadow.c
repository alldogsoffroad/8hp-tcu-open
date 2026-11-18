/* NEGATIVE FIXTURE (pa7_shadow_scan.py shadow): a shadow that includes a
 * live header and drives PA7 / the C9 cell — the scan must FAIL on it */
#include "bsw/sup/sup_cells.h"
void fixture_shadow(void)
{
    *(volatile unsigned char *)0xFFFE3803u |= 0x80u;
    CELL_C9_VARIANT = 1u;
}

/* NEGATIVE FIXTURE (tsel_shadow_scan.py shadow): a shadow that includes a
 * live header and writes the live selection cells — the scan must FAIL */
#include "bsw/drv/drv_cells.h"
void fixture_shadow(void)
{
    *(volatile short *)0xFFF9393Au = 0x0215;
    CELL_TMP_940FC = 8u;
}

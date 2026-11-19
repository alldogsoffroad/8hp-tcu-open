/* g6a_scan_fixture_cmp.c — NEGATIVE fixture for g6a_scan.py cmp: a
 * comparator that writes a live request object and calls the law (must be
 * refused). */
#include "app/g6a.h"
#include "bsw/sup/sup_cells.h"
void fixture_cmp(void)
{
    *(volatile uint16_t *)0xFFF8B7C8u = 0u;
    drv_rq_law();
}

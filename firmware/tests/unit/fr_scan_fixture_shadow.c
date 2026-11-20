/* negative fixture (fr_shadow_scan.py shadow): the shadow writing a live cell
 * and the fault manager's RAM from outside app/fr_fm.c */
#include "bsw/sup/sup_cells.h"
extern fr_fm_t s_fm;
void fixture_shadow(void) { *(volatile unsigned char *)0xFFF89C43u = 3u; s_fm.m0 = 1u; }

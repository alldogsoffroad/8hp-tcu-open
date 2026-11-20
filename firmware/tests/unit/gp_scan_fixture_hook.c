/* negative fixture (gp_shadow_scan.py hook): the gather reading a modelled
 * cell (block A) and writing through a non-const volatile */
void fixture_hook(void) { *(volatile unsigned char *)0xFFF8BD6Cu = 1u; }

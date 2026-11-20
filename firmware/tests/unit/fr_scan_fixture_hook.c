/* negative fixture (fr_shadow_scan.py hook): the gather reading a modelled
 * cell and writing through a non-const volatile */
void fixture_hook(void) { *(volatile unsigned char *)0xFFF89CF5u = 1u; }

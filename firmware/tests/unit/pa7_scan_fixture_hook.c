/* NEGATIVE FIXTURE (pa7_shadow_scan.py hook): a gather that writes the live
 * PA7 mirror — the scan must FAIL on it */
void fixture_hook(void) { *(volatile unsigned char *)0xFFF90920u = 1u; }

/* NEGATIVE FIXTURE (fb_shadow_scan.py hook): a gather that writes a live
 * cell — the scan must FAIL on it */
void fixture_hook(void) { *(volatile unsigned short *)0xFFF9340Cu = 0u; }

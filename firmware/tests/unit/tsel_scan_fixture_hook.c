/* NEGATIVE FIXTURE (tsel_shadow_scan.py hook): a gather that writes the live
 * selection status — the scan must FAIL on it */
void fixture_hook(void) { *(volatile unsigned char *)0xFFF940FCu = 8u; }

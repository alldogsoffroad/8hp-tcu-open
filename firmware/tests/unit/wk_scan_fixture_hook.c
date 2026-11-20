/* negative fixture (wk_shadow_scan.py hook): the gather naming a cell the shadow models (the lock-up
 * request 0xFFF8B2AA), writing through a non-const volatile and naming a transmit path */
void fixture_hook(void) { *(volatile unsigned short *)0xFFF8B2AAu = 1u; fca_tx_enable(1); }

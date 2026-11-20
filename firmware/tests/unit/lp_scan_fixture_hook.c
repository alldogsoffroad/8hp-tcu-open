/* negative fixture (lp_ti_shadow_scan.py hook): the gather naming a modelled cell (the line-pressure
 * object's result), writing through a non-const volatile and naming a transmit path */
void fixture_hook(void) { *(volatile unsigned short *)0xFFF8BCB0u = 1u; fca_tx_enable(1); }

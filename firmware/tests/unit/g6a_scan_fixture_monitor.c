/* g6a_scan_fixture_monitor.c — NEGATIVE fixture for g6a_scan.py monitor
 * (G6b): the monitor ticking the candidate (a write), and a read accessor
 * outside an `#if GS_G6B` block — both must be reported. */
#if GS_G6B
#include "app/gs_cand.h"
static void fx_bad_tick(const gs_in_t *in) { gs_cand_tick_10ms(in); }
#endif
static unsigned fx_bad_read(void) { return gs_cand_cur(0u); }

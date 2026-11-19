/* g6a_scan_fixture_output.c — NEGATIVE fixture for g6a_scan.py outputs: an
 * output path that names the G6a candidate (must be refused). */
#include "app/gs_cand.h"
void fixture_output(void) { (void)gs_cand_cur(0u); }

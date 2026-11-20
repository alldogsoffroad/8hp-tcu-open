/* negative fixture (shadow_sim_scan.py outputs): a live-path file reading the simulated speeds */
#include "io/vehicle/shadow_sim.h"
unsigned fixture_output(void) { shadow_sim_in_t s; shadow_sim_get(&s); return s.output_rpm; }

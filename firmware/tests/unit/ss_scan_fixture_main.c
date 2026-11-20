/* negative fixture (shadow_sim_scan.py main): main.c reading the values instead of only feeding them */
#include "io/vehicle/shadow_sim.h"
void fixture_main(void) { shadow_sim_in_t s; shadow_sim_get(&s); shadow_sim_init(); }

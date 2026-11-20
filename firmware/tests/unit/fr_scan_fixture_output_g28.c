/* negative fixture (fr_shadow_scan.py outputs, groups 27 / 28): an output
 * path naming the TOE32 test's state (its type / its RAM) */
extern const volatile struct fr_t3_s *s_frt;
unsigned fixture_output_g28(void) { return s_frt != 0; }

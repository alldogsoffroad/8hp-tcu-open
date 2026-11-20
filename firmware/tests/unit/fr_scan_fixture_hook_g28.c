/* negative fixture (fr_shadow_scan.py hook, groups 27 / 28): the gather
 * reading a stock cell the TOE32 test models (0x938F6[3], the fault bits)
 * through a const volatile — the shadow keeps its own copy, the hook must
 * never name the stock cell */
unsigned fixture_hook_g28(void) { return *(const volatile unsigned short *)0xFFF938FCu; }

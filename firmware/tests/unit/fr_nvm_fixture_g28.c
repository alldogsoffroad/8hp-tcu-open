/* negative fixture (g6a_scan.py nvm, groups 27 / 28): a symbol naming the
 * per-output fault record store 0x87120 (+ fatal), and the stock fault-bit
 * cell 0x938F6 as a data word (the shadow keeps its own copies) */
#include <stdint.h>
const uint32_t fr_fix_g28_words[1] = { 0xFFF938F6u };
void fr_fix_87120_record(void) { }

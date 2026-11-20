/* negative fixture (g6a_scan.py nvm): a symbol naming the stock NV shift
 * counter 0xBA52C (0xEBC60's) and the end's 0xE8512 */
#include <stdint.h>
const uint32_t gp_fix_word = 0xFFF8E621u;
void gp_fix_ba52c_count(void) { }
void gp_fix_e8512_write(void) { }

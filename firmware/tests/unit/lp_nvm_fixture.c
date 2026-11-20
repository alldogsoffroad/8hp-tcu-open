/* negative fixture (g6a_scan.py nvm): symbols naming the stock routines the line-pressure shadow keeps
 * out (the fault monitor 0xF31B2, the adaptation reader 0xAEA24) and an EEPROM dirty-flag literal */
#include <stdint.h>
const uint32_t lp_fix_word = 0xFFF85175u;
void lp_fix_f31b2_monitor(void) { }
void lp_fix_aea24_reader(void) { }

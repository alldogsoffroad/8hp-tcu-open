/* negative fixture (g6a_scan.py nvm): symbols naming the stock routines the lock-up shadow keeps out (the
 * engagement counter's EEPROM writer 0xBF8E8, the fill-adaptation learner 0xAEE26) and an EEPROM dirty-flag literal */
#include <stdint.h>
const uint32_t wk_fix_word = 0xFFF85175u;
void wk_fix_bf8e8_counter(void) { }
void wk_fix_aee26_learner(void) { }

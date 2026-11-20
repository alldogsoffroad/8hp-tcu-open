/* negative fixture (g6a_scan.py nvm): a symbol naming the stock record-create
 * routine 0xAC324 and the event-log NV object 0xFFF8E621 / a DSM-image
 * address as data words */
#include <stdint.h>
const uint32_t fr_fix_words[2] = { 0xFFF8E621u, 0xFFF823F7u };
void fr_fix_ac324_create(void) { }

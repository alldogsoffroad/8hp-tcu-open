/* Entry into the project flasher (spec 16), BENCH USE.
 *
 * UDS RoutineControl FE30, extended session only:
 *   start   31 01 FE 30 -> 71 01 FE 30, then about 20 ms later the application hands the CPU to
 *           the RAM flasher (plat_flasher_start) and does not resume. Refused with NRC 0x22 while
 *           the engine runs, a bench output test is active, or the FWE pin is low.
 * Identifier 0xFD03 (ReadDataByIdentifier): 1 byte, bit 0 = FPMON.FWE (flash programming possible).
 */
#ifndef TCU_FLASH_ENTRY_H
#define TCU_FLASH_ENTRY_H

#include "tcu/units.h"

#define FLASH_RID 0xFE30u
#define FLASH_DID 0xFD03u
#define FLASH_EN_RID 0xFE31u    /* flash-enable line test (no flash access):
                                 *   31 01 FE 31 -> 71 01 FE 31 fpmon_before fpmon_after pc14
                                 *   31 02 FE 31 -> lines back to inputs; 71 02 FE 31 fpmon pc14 */
#define FLASH_START_DELAY_MS 20u

/* allowed: the caller's interlock result (engine stopped, no bench output test active). */
int flash_entry_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                        uint16_t max, bool allowed);
int flash_entry_did(uint8_t *out, uint16_t max);
void flash_entry_tick_1ms(void);

#endif

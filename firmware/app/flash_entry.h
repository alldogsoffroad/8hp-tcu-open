/* Entry into the project flasher (spec 16), BENCH USE.
 *
 * UDS RoutineControl FE30, extended session only:
 *   start   31 01 FE 30 -> 71 01 FE 30; the flash enable lines (port C + supervisor programming
 *           mode) are raised, and about 20 ms later the application hands the CPU to the RAM
 *           flasher if FWE reads high (else the lines are dropped and the application continues).
 *           Refused with NRC 0x22 while the engine runs or a bench output test is active.
 *           RAM application (spec 18, TCU_RAM_APP): FE30 returns to the data-flash loader instead,
 *           with no FWE dependency.
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
#define SUP_PAT_RID 0xFE32u     /* supervisor word experiment (bench, no flash access):
                                 *   31 01 FE 32 hh ll tm -> word hhll every 5 ms, bits tm toggled on
                                 *        alternate frames (bit 0 output enable always cleared)
                                 *   31 03 FE 32 -> 71 03 FE 32 tx_hi tx_lo rx_hi rx_lo fpmon
                                 *   31 02 FE 32 -> normal supervisor word again */
#define SUP_RAW_RID 0xFE33u     /* raw supervisor exchange (lab): send one word ONCE,
                                 * no bit-0 clearing, returns tx/rx/fpmon.
                                 *   31 01 FE 33 hh ll -> 71 01 FE 33 tx_hi tx_lo rx_hi rx_lo fpmon */
#define SUP_AUTH_RID 0xFE34u    /* supervisor authenticator (blocking, may WDT-reset).
                                 * DANGER: sends the INIT SEQUENCE starting with [8E,01],
                                 * which RESETS the TCU — never use mid-run (bench 2026-10-26) */
#define SUP_MISR_RID 0xFE35u    /* read the MISR (instant, non-blocking):
                                 *   31 01 FE 35 -> 71 01 FE 35 <MISR BE32> */
#define SUP_BURST_RID 0xFE36u   /* vendor-discipline burst (lab, per-frame RSPI re-init):
                                 *   31 01 FE 36              -> INIT SEQ [8E,01][84,01]x11[8E,04]
                                 *   31 01 FE 36 b3 b2 b1 b0  -> [88]x4 authenticator burst
                                 *   31 01 FE 36 <level>      -> [84,level] burst
                                 * Reply: 71 01 FE 36 <last_rx_hi> <last_rx_lo> <fwe>
                                 * DANGER: type 1 starts with [8E,01], which RESETS the
                                 * TCU — never use mid-run (bench 2026-10-26) */
#define SUP_FOLD_RID 0xFE37u    /* MISR fold self-test (lab): the stock init check
                                 * fold(0x7650E321, 8 words @0x40F1C) == 0xEDF9FBBE.
                                 *   31 01 FE 37 -> 71 01 FE 37 <fold BE32> <match 01/00> */
#define FLASH_START_DELAY_MS 20u

/* allowed: the caller's interlock result (engine stopped, no bench output test active). */
int flash_entry_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                        uint16_t max, bool allowed);
int flash_entry_did(uint8_t *out, uint16_t max);
void flash_entry_tick_1ms(void);

#endif

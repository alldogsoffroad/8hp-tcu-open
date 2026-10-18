/* Flasher protocol (spec 16 section 4): minimal ISO-TP and a UDS subset on 0x7E1 -> 0x7E9.
 *   3E 00 / 3E 80                     tester present
 *   22 F1 95                          "FL" version, then FPMON FSTATR0 FSTATR1 FASTAT FPESTAT last-error
 *   31 01 FF 01 addr(4) len(4)        CRC-32 (zlib) of ROM 0x000000-0x1FFFFF -> 71 01 FF 01 crc(4)
 *   31 01 FF 00 addr(4) len(4)        erase whole blocks inside the window, blank-checked
 *   34 00 44 addr(4) len(4)           open a download (256-aligned, inside the window) -> 74 20 01 02
 *   36 ctr data(256)                  program + read back one page -> 76 ctr (a repeat of the last
 *                                     counter is acknowledged again without programming)
 *   37                                close the download -> 77
 *   11 01                             reset -> 51 01
 * Write window: 0x040000-0x1FFFFF. Nothing outside it is ever erased or programmed. */
#ifndef TCU_FL_CORE_H
#define TCU_FL_CORE_H

#include <stdint.h>

#define FL_WIN_LO   0x00040000u
#define FL_WIN_HI   0x00200000u     /* exclusive */
#define FL_ROM_END  0x00200000u     /* readable for CRC: 0x000000-0x1FFFFF */
#define FL_PAGE     256u
#define FL_VERSION  1u

void fl_core_init(void);
void fl_core_rx(const uint8_t d[8], uint8_t dlc);   /* one frame received on 0x7E1 */
void fl_main_loop(void);                            /* never returns on the target */

/* Erase-block geometry of the user MAT (manual table 25.x): size of the block holding a. */
uint32_t fl_block_size(uint32_t a);

#endif

/* Flasher protocol (spec 16 section 4, spec 18 section 5): minimal ISO-TP and a UDS subset on
 * 0x7E1 -> 0x7E9, over one flash target (code flash for the RAM flasher, data flash for the loader).
 *   3E 00 / 3E 80                     tester present
 *   22 F1 95                          target ident (2 chars: "FL" code flash, "LD" loader / data
 *                                     flash), version, then FPMON FSTATR0 FSTATR1 FASTAT FPESTAT
 *                                     last-error
 *   31 01 FF 01 addr(4) len(4)        CRC-32 (zlib) of a readable range -> 71 01 FF 01 crc(4)
 *   31 01 FF 00 addr(4) len(4)        erase whole blocks inside the write window
 *   34 00 44 addr(4) len(4)           open a download (page-aligned, inside the window)
 *                                     -> 74 20 <max block length = 2 + page size>
 *   36 ctr data(page)                 program + read back one page -> 76 ctr (a repeat of the last
 *                                     counter is acknowledged again without programming)
 *   37                                close the download -> 77
 *   11 01                             reset -> 51 01
 * Nothing outside the target's write window is ever erased or programmed. */
#ifndef TCU_FL_CORE_H
#define TCU_FL_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "flasher/fl_fcu.h"

#define FL_PAGE_MAX 256u
#define FL_VERSION  2u

/* Code-flash target (spec 16): application and calibration blocks. */
#define FL_WIN_LO   0x00040000u
#define FL_WIN_HI   0x00200000u     /* exclusive */
#define FL_ROM_END  0x00200000u     /* readable for CRC: 0x000000-0x1FFFFF */
#define FL_PAGE     256u

/* Data-flash target (spec 18): application image in DB00-DB07; DB08-DB15 keep the stock EEPROM
 * sections and are never written. */
#define FL_DF_WIN_LO 0x80100000u
#define FL_DF_WIN_HI 0x80110000u    /* exclusive: DB00-DB07 */
#define FL_DF_PAGE   128u

typedef struct {
    char ident[2];
    uint32_t win_lo, win_hi;        /* erase / program window, hi exclusive */
    uint32_t rd_lo, rd_hi;          /* readable for CRC, hi exclusive */
    uint16_t page;                  /* programming unit, power of two, <= FL_PAGE_MAX */
    bool erased_ff;                 /* erased flash reads 0xFF (code flash); data flash: undefined */
    uint32_t (*block_size)(uint32_t a);
    fl_err_t (*erase)(uint32_t a);
    fl_err_t (*program)(uint32_t a, const uint8_t *p);
} fl_target_t;

extern const fl_target_t fl_target_rom;    /* spec 16 */
extern const fl_target_t fl_target_df;     /* spec 18 */

/* Optional 13th ident byte: when fl_ident_aux_set is true, the ident reply (22 F1 95) appends
 * fl_ident_aux as byte 12. The loader uses it to report the FPMON latched at boot hand-over. */
extern uint8_t fl_ident_aux;
extern bool fl_ident_aux_set;

void fl_core_init(const fl_target_t *t);
void fl_core_rx(const uint8_t d[8], uint8_t dlc);   /* one frame received on 0x7E1 */
void fl_main_loop(void);                            /* never returns on the target */

/* Erase-block geometry of the user MAT (manual table 25.x): size of the block holding a. */
uint32_t fl_block_size(uint32_t a);

#endif

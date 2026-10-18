/* Host simulation of the SH7254R flash controller and the flasher's hardware services (spec 16
 * section 7). Implements fl_regs.h accessors and fl_hal.h with the manual's rules:
 *   - ROM read addresses may be read only with FENTRYR = 0 (else: violation, ROMAE, ILGLERR);
 *   - P/E addresses only inside the area selected by FENTRYR;
 *   - FENTRYR / FCURAME / FRESETR key codes; FENTRYR changes only from 0;
 *   - commands need the FCU firmware copied to FCU RAM; FWE pin gate;
 *   - program only onto a fully erased page (stricter than the "no double programming" rule);
 *   - FRDY low for a simulated busy time; injectable time-out, PRGERR, ERSERR;
 *   - optional power cut after N program/erase commands (contents of the hit area undefined). */
#ifndef TCU_FL_SIM_H
#define TCU_FL_SIM_H

#include <stdbool.h>
#include <stdint.h>

#define SIM_ROM_SIZE 0x200000u
#define SIM_QLEN     4096u

typedef struct {
    uint8_t rom[SIM_ROM_SIZE];
    bool fwe;
    /* registers */
    uint16_t fentryr;
    uint8_t fstatr0, fstatr1, fastat;
    uint16_t fpestat;
    bool fcurame, fw_loaded, freset;
    uint32_t fw_words_ok;
    /* command state */
    int cmd;                    /* 0 idle, 1 got E8, 2 got 80 (data), 3 got 20 */
    uint32_t prog_base;
    unsigned prog_n;
    uint16_t prog_buf[128];
    uint32_t busy_ms;           /* FRDY = 0 while > 0 */
    /* statistics and checks */
    unsigned violations;        /* rule breaches by the driver (tests expect 0) */
    unsigned erases, programs, resets_fcu, polls_busy;
    uint32_t ms;
    uint32_t last_poll_ms, max_gap_ms;
    /* fault injection */
    uint32_t fail_prog_addr;    /* PRGERR when programming this page (0 = off) */
    uint32_t fail_erase_addr;   /* ERSERR when erasing this block (0 = off) */
    bool hang;                  /* FRDY never returns */
    long cut_after;             /* power cut when this many P/E commands have started (<0 off) */
    bool cut;                   /* set when the cut happened */
    /* CAN: frames to the flasher and from it */
    uint8_t rxq[SIM_QLEN][9];
    unsigned rx_head, rx_tail;
    uint8_t txq[SIM_QLEN][8];
    unsigned tx_n;
    bool stop;
    unsigned hal_resets;
    /* bridge mode (tcu_flasher_sim): frames over stdin/stdout */
    bool bridge;
} fl_sim_t;

extern fl_sim_t sim;

void fl_sim_init(void);
void fl_sim_push(const uint8_t *d, uint8_t dlc);   /* queue a frame for the flasher */
uint32_t fl_sim_fw_word(uint32_t i);               /* simulated FCU firmware content */

#endif

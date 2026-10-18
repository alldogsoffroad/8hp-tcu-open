/* Flash controller (FCU) driver for the project flasher (spec 16, manual section 25.6.3).
 * ROM addresses are user MAT read addresses (0x000000-0x1FFFFF). The driver itself does not
 * restrict addresses; the write window is enforced in fl_core.c before any call. */
#ifndef TCU_FL_FCU_H
#define TCU_FL_FCU_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FL_OK = 0,
    FL_E_FWE = 1,       /* FWE pin low or P/E mode entry refused */
    FL_E_TIMEOUT = 2,   /* FRDY stayed 0; FCU was reset */
    FL_E_ILGL = 3,      /* illegal command / access (ILGLERR) */
    FL_E_ERS = 4,       /* ERSERR (FPESTAT tells why) */
    FL_E_PRG = 5,       /* PRGERR (FPESTAT tells why) */
    FL_E_FCU = 6,       /* FCUERR / FCU RAM ECC error; firmware reloaded next time */
} fl_err_t;

#define FL_PROG_TIMEOUT_MS   20u     /* tP256 max 14.4 ms (>100 cycles) x 1.1 */
#define FL_ERASE_TIMEOUT_MS  2400u   /* tE128K max 2.1 s (>100 cycles) x 1.1 */

/* Called from every wait loop (in addition to fl_hal_poll), e.g. to send response-pending. */
extern void (*fl_fcu_busy_hook)(void);

fl_err_t fl_fcu_erase(uint32_t rom);                       /* block containing rom */
fl_err_t fl_fcu_program(uint32_t rom, const uint8_t *p);   /* 256 bytes, 256-aligned */
void fl_fcu_read(uint32_t rom, uint8_t *dst, uint32_t n);  /* ROM read mode only */

/* Last status snapshot: FPMON, FSTATR0, FSTATR1, FASTAT, FPESTAT low byte, last error. */
void fl_fcu_diag(uint8_t out[6]);
bool fl_fcu_fwe(void);

#endif

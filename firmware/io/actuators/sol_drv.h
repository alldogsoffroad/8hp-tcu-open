/* Solenoid driver chips on bus 0 (spec 12 sections 1 and 2b; spec 03 section 4).
 *
 * Five chips with two current-controlled channels each; driver channel k = chip k / 2,
 * channel k % 2. Channel use (spec 12 section 1): 0 A, 1 D, 2 B, 3 E, 4 C, 5 TCC, 6 line
 * pressure, 7 park release, 8-9 unused.
 *
 * The layer owns the SPI jobs: identification and configuration at start-up, the cyclic setpoint
 * job every 10 ms and the status job every 100 ms. Every reply is checked (header, read-back of
 * the written values); any failure latches a fault and forces all channels off.
 * It does not decide WHEN a channel may be driven; that is the caller's job (bench_out.c).
 */
#ifndef TCU_SOL_DRV_H
#define TCU_SOL_DRV_H

#include "platform/platform.h"

#define SOL_CHIPS      5u
#define SOL_CHANNELS   10u
#define SOL_MAX_MA10   12282u   /* driver maximum, 0.1 mA [I] (spec 12 section 2b) */

typedef enum {
    SOL_MODE_DISABLED = 0,
    SOL_MODE_OFF      = 1,
    SOL_MODE_CURRENT  = 2,
    SOL_MODE_FULL_ON  = 3       /* not used by the project firmware */
} sol_mode_t;

/* fault bits */
enum {
    SOL_F_BUS        = 0x01u,   /* SPI transfer timed out                  */
    SOL_F_ID         = 0x02u,   /* identification reply wrong              */
    SOL_F_CONFIG     = 0x04u,   /* configuration read-back mismatch        */
    SOL_F_HEADER     = 0x08u,   /* cyclic / status reply header wrong      */
    SOL_F_READBACK   = 0x10u,   /* setpoint / dither read-back mismatch    */
    SOL_F_NOT_READY  = 0x20u    /* start-up jobs not completed             */
};

typedef struct {
    uint8_t  id[2];             /* identification bytes (replies to 0x06, 0x00) */
    uint8_t  status_ch[2];      /* status per channel (replies to 0x18, 0x1E)   */
    uint8_t  status_chip;       /* chip status (reply to 0x28)                  */
} sol_chip_t;

typedef struct {
    uint8_t    faults;          /* SOL_F_*, latched until sol_init()  */
    bool       ready;           /* identification and configuration done */
    uint32_t   cycles;
    sol_chip_t chip[SOL_CHIPS];
    ma10_t     current[SOL_CHANNELS];
    uint8_t    mode[SOL_CHANNELS];
} sol_status_t;

/* Run identification and configuration; all channels off. */
void sol_init(void);
/* Request a channel's mode and current (0.1 mA). Takes effect with the next cyclic job. */
void sol_set(uint8_t ch, sol_mode_t mode, ma10_t current);
/* All channels off. */
void sol_all_off(void);
void sol_tick_10ms(void);
const sol_status_t *sol_status(void);

/* exposed for tests */
uint16_t sol_setpoint(ma10_t current);

#endif

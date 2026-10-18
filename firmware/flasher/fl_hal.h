/* Flasher hardware services (spec 16 section 3). Implemented by fl_sh7254r.c on the target and by
 * tests/unit/fl_sim.c on the host. Everything here runs from RAM and polls; no interrupts. */
#ifndef TCU_FL_HAL_H
#define TCU_FL_HAL_H

#include <stdbool.h>
#include <stdint.h>

void fl_hal_init(void);

/* Advance the millisecond clock and service the internal watchdog (every ms) and the external
 * supervisor (every 5 ms, output enable off). Must be called at least every millisecond or so;
 * every wait loop in the flasher calls it. */
void fl_hal_poll(void);
uint32_t fl_hal_ms(void);

/* Bus 1: requests on 0x7E1, responses on 0x7E9 (always DLC 8). */
bool fl_hal_can_recv(uint8_t d[8], uint8_t *dlc);
bool fl_hal_can_send(const uint8_t d[8]);
bool fl_hal_can_idle(void);         /* all transmissions complete */

void fl_hal_rom_cache_flush(void);
void fl_hal_reset(void);            /* watchdog reset; does not return on the target */

#ifdef FL_HOST
bool fl_sim_stop(void);             /* simulator: leave fl_main_loop() */
#endif

#endif

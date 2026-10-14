/* Platform interface: everything above this layer is hardware independent.
 * Implemented by platform/sh7254r (target) and platform/host (simulator). */
#ifndef TCU_PLATFORM_H
#define TCU_PLATFORM_H

#include "tcu/units.h"

typedef enum { CAN_BUS1 = 0, CAN_BUS2 = 1, CAN_BUS_COUNT } can_bus_t;

typedef struct {
    uint16_t id;        /* 11-bit standard identifier */
    uint8_t  dlc;       /* 0..8                       */
    uint8_t  data[8];
} can_frame_t;

/* Start-up: safe pin state, clocks, timers, CAN. Called once, with interrupts masked. */
void plat_init(void);

/* Returns true once per elapsed 1 ms tick (polled). */
bool plat_tick_poll(void);

/* Watchdog: must be called at least every 30 ms (spec 05 section 2). */
void plat_wdt_service(void);

/* Queue a frame for transmission; returns false if all transmit buffers are busy. */
bool plat_can_send(can_bus_t bus, const can_frame_t *f);

/* Accept frames with this identifier on the bus (one hardware mailbox per identifier).
 * Returns false when the bus has no free receive mailbox. */
bool plat_can_rx_register(can_bus_t bus, uint16_t id);

/* Fetch one received frame, if any. Polled from task context. */
bool plat_can_recv(can_bus_t bus, can_frame_t *f);

/* Software reset through the watchdog (same method as the stock reset routine 0x966B8).
 * Does not return on the target. */
void plat_reset(void);

/* Write the boot request record (spec 06 section 5a) to 0xFFF80080, then reset, so the Bosch
 * boot software takes over the pending diagnostic request. Does not return on the target. */
void plat_boot_handover(const uint8_t *rec, uint8_t len);

/* Busy-loop counter used to estimate the CPU clock during bring-up (spec 05 section 1.2). */
void plat_idle_count(void);
uint32_t plat_idle_take(void);   /* returns and clears the count */

#endif

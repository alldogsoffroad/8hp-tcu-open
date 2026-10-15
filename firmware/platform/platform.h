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

/* ---- Analog inputs (spec 04) ----
 * Latest conversion result of channel AN0..AN27 or AN40..AN46, left-aligned 16-bit
 * (65536 = 5.000 V at the pin, before the external divider). Returns false for channels
 * that are not converted. Called from the 1 ms task. */
bool plat_adc_raw(uint8_t an, uint16_t *raw);

/* ---- Speed inputs (spec 02 section 4.1) ----
 * Consistent snapshot of speed channel ch (0 = TIF12, 1 = TIF13, 2 = TIF14):
 *   edges   rising edges counted since start (wraps at 2^16)
 *   t_edge  time of the last edge, 4 MHz ticks (wraps at 2^24)
 *   age     ticks since the last edge (saturates at 2^24 - 1)
 * Returns false if the channel does not exist or no stable read was possible. */
typedef struct {
    uint16_t edges;
    uint32_t t_edge;
    uint32_t age;
} speed_raw_t;

#define SPEED_CHANNELS   3u
#define SPEED_TICK_HZ    4000000u
#define SPEED_TIME_MASK  0x00FFFFFFu

bool plat_speed_read(uint8_t ch, speed_raw_t *r);

/* Read n bytes at a target address for the measurement protocol. Returns false (nothing
 * copied) if any byte lies outside the readable areas: project application and calibration
 * flash 0x040000-0x1FFFFF, RAM 0xFFF80000-0xFFF9FFFF. Boot flash and I/O registers are not
 * readable (register reads can have side effects). */
bool plat_mem_read(uint32_t addr, uint8_t *out, uint16_t n);

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

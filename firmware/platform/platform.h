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
 * copied) if any byte lies outside the readable areas: user MAT flash 0x000000-0x1FFFFF (boot
 * blocks, application, calibration; for cross-checking read-outs) and RAM 0xFFF80000-0xFFF9FFFF.
 * I/O registers are not readable (register reads can have side effects). */
bool plat_mem_read(uint32_t addr, uint8_t *out, uint16_t n);

/* ---- External supervisor on RSPI-B / SSLB1 (spec 03 section 4.4) ----
 * CPU-driven 16-bit frames. plat_sup_exchange() collects the reply of the previous frame (if one
 * has arrived) into *rx and starts sending tx if the transmitter is free.
 * Returns bit 0 = tx started, bit 1 = *rx valid. */
void plat_sup_init(void);
uint8_t plat_sup_exchange(uint16_t tx, uint16_t *rx);
/* Stock-job transport (spec 03 section 4.4, app cyclic jobs 0x40BE8/0x40C0C/0x40C54): send n
 * 16-bit frames as one exchange - full register re-setup per exchange plus the explicit start
 * trigger, two-command sequence SPCMD0 = SSLB1, SPCMD1 = SSLB0, so even-indexed frames go to
 * the supervisor (SSLB1) and odd-indexed frames to the second system chip (SSLB0), exactly as
 * the stock DMA jobs alternate. Replies land in rx[i]. Blocking; false on a bus timeout. */
bool plat_sup_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n);
/* Burst variant: all n frames to SSLB1 alone (single-command sequence), back to back in
 * one exchange - the shape of the stock boot routine 0x1D478: 0x8E01 followed by eleven
 * 0x8401 (spec 03 section 4.4). */
bool plat_sup_burst(const uint16_t *tx, uint16_t *rx, uint8_t n);

/* ---- Solenoid driver chips on RSPI-A (spec 03, spec 12 section 2b) ----
 * Send n 16-bit frames; frame i goes to chip i mod 5 (SSLA3, SSLA2, SSLA1, SSLA0, SSLA5) and the
 * reply received during frame i is stored in rx[i]. Blocking (about 13 us per frame); returns
 * false on a bus timeout. */
void plat_drv_init(void);
bool plat_drv_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n);

/* ---- Park hold PWM output: TOE31 on PL6 (spec 02 section 3.1, spec 12 section 1.1) ----
 * A 1 kHz PWM switch output for the park hold magnet (low-side driver, terminal C7).
 * plat_toe_out_init() configures the timer channel with duty 0 and switches the pin to its
 * timer function last, so the pin stays low (output off) throughout; call it once after
 * plat_init(). plat_toe_out_set() sets the energisation in per mille (1000 = fully on,
 * 0 = off); the duty takes effect through the reload buffer at the next cycle match. */
void plat_toe_out_init(void);
void plat_toe_out_set(uint16_t permil);

/* ---- User boot MAT read-out (SH7254R manual 25.9.1) ----
 * Copy the 32 KB user boot MAT into dst (4-byte aligned, at least len bytes) by switching the
 * flash MAT from RAM-resident code with interrupts masked. Returns false if not available. */
#define USERBOOT_SIZE 0x8000u
bool plat_userboot_copy(uint8_t *dst, uint32_t len);

/* Software reset through the watchdog (same method as the stock reset routine 0x966B8).
 * Does not return on the target. */
void plat_reset(void);

/* Write the boot request record (spec 06 section 5a) to 0xFFF80080, then reset, so the Bosch
 * boot software takes over the pending diagnostic request. Does not return on the target. */
void plat_boot_handover(const uint8_t *rec, uint8_t len);

/* FWE pin level as seen by the flash controller (FPMON bit 7, manual 25.3.1): programming and
 * erasure of the flash are possible only while it is high. */
bool plat_flash_fwe(void);

/* Flash enable lines as the user boot MAT drives them at every reset (bench finding 2026-10-17,
 * spec 16 section 10): on = PC12, PC13 outputs high and PC15 output low; off = PC12/13/15 back to
 * inputs (the application's normal state, in which FWE reads low). Returns the PC14 level. */
bool plat_flash_enable(bool on);

/* State left by the boot software, captured as the first action of plat_init (before the
 * watchdog and port set-up): FPMON, then big-endian 16-bit PAIOR PADR PBIOR PBDR PCIOR PCDR PCPR,
 * then the port D-L registers written by the stock initialisation (offsets from 0xFFFFC800, see
 * plat_sh7254r.c). Returns the number of bytes copied (0 on the host). Bench identifier FD04. */
uint16_t plat_entry_snapshot(uint8_t *out, uint16_t max);

/* CPU clock probe (spec 05 section 1.2), bench identifier FD05. Fills 16 bytes, big-endian:
 *   0-1   word at 0xFFFF1000 (undocumented; the boot code branches on bits 2..0)
 *   2-3   word at 0xFFFF0812 (on-chip RAM access-cycle setting, 0x7810 / 0x7821 in the boot code)
 *   4-5   word at 0xFFFF0802 (RAM write enable)    6-7  word at 0xFFFF0842
 *   8-11  additions executed in a chain of dependent adds (one per CPU clock)
 *   12-15 CMT0 counts (Pphi/8 = 2.5 MHz) the chain took
 * Blocks for about 10-20 ms (watchdog serviced). Returns 16, or 0 where not available. */
uint16_t plat_clock_probe(uint8_t *out, uint16_t max);

/* Copy the project flasher (spec 16) to RAM and run it. Interrupts are masked first; the
 * application does not resume (the flasher ends with a reset). Does not return on the target. */
void plat_flasher_start(void);

/* Busy-loop counter used to estimate the CPU clock during bring-up (spec 05 section 1.2). */
void plat_idle_count(void);
uint32_t plat_idle_take(void);   /* returns and clears the count */

#endif

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

/* Watchdog: replaced by plat_wdt_kick (rebuild §5.6). The old name is kept for compatibility during the transition. */
void plat_wdt_service(void); /* compat — calls plat_wdt_kick */

/* Queue a frame for transmission; returns false if all transmit buffers are busy. */
bool plat_can_send(can_bus_t bus, const can_frame_t *f);

/* Accept frames with this identifier on the bus (one hardware mailbox per identifier).
 * Returns false when the bus has no free receive mailbox. */
bool plat_can_rx_register(can_bus_t bus, uint16_t id);
/* Program the collected RX registrations in ascending identifier
 * order (the RCAN stays in ID-reorder mode; see plat_sh7254r.c). */
void plat_can_rx_commit(void);

/* Fetch one received frame, if any. Polled from task context. */
bool plat_can_recv(can_bus_t bus, can_frame_t *f);

/* ---- RSPI job transport: interrupt-per-frame engine (rebuild design §5.5) ----
 * Replaces the polled plat_sup_xfer/burst and plat_drv_xfer family.
 * The mainline stages a job; the RX ISR collects replies and feeds the next
 * frame; the completion handler runs in ISR context. SPCR discipline:
 * idle = 0x08, armed = 0xC8, ISR disarms to 0x08 at burst end. */
typedef void (*rspi_done_fn)(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t frames);

/* Queue a transaction (task context; caller holds SR if windowed).
 * Returns false if the bus is busy. After launch, call plat_rspi_pump(). */
bool plat_rspi_launch(uint8_t bus, uint8_t job_id, uint16_t *tx, uint16_t *rx,
                      uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                      rspi_done_fn done);
/* Start the staged transaction if the bus is idle (task or init context). */
void plat_rspi_pump(uint8_t bus);
/* Returns true while a transaction is armed or feeding on this bus (the
 * launch gate: false inside a completion handler, so the handler's engine
 * pump chains the next job — stock 0x93A08 launches via 0x93902 BEFORE the
 * release function runs). */
bool plat_rspi_busy(uint8_t bus);
/* busy OR inside the burst-end completion window (the stock's '@' state):
 * the finishing job is still owned by its handler. Used ONLY by the stale-
 * burst recovery in sup_job_check_free, never as a launch gate. */
bool plat_rspi_owned(uint8_t bus);
/* ISR-entry count per bus (P8 diagnostic: verifies the IPR assignment). */
uint32_t plat_rspi_isr_count(uint8_t bus);
bool plat_rspi_poll(uint8_t bus);      /* r01e superloop fallback: harvest SPRF (lab only) */
/* S3: error-complete a channel ARMED/FEEDING longer than max_us (call with
 * SR.I >= the bus's IPR). A completion with frames == 0 is an ERROR
 * completion (S2: OVRF/MODF/PERF twice, a dead SPTEF, or a reap). */
bool plat_rspi_reap(uint8_t bus, uint32_t max_us);
uint32_t plat_rspi_err_count(uint8_t bus);
/* Blocking single transaction for lab/diagnostic use (FE13). */
bool plat_rspi_lab(uint8_t bus, const uint16_t *tx, uint16_t *rx,
                   uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                   uint32_t timeout_ms);
/* Engine init (both buses to idle SPCR = 0x08). */
void plat_rspi_init(void);
/* Bus-A bit-rate accessor (the engine uses this). */
uint8_t plat_drv_spbr_get(void);

/* ---- INTC and SR (rebuild design §5.2) ---- */
typedef struct { uint8_t ipr; uint8_t shift; uint8_t prio; } plat_ipr_ent_t;
typedef struct { const plat_ipr_ent_t *ent; uint8_t n; } plat_ipr_map_t;
void plat_intc_setup(const plat_ipr_map_t *m);
void plat_sr_open(void);               /* SR.I = 0xE (stock order 21) */
void plat_sr_close(void);              /* SR.I = 0xF (critical sections) */
uint32_t plat_sr_i(void);              /* current SR.I level */
void plat_sr_set(uint32_t level);      /* SR.I = level exactly (nestable windows) */

/* ---- 1 ms tick (rebuild design §5.3, ATU-D1 CMID10) ---- */
void plat_tick_start(void);
uint32_t plat_tick_count(void);
uint32_t plat_tick_isr_count(void);   /* TRUE vector deliveries (r01e diagnostic) */
bool plat_tick_poll_hw(void);         /* r01e superloop fallback: harvest the compare flag */

/* ---- Walk timer (rebuild design §5.4, ATU-D0 compare) ---- */
void plat_walktmr_start(void);
/* one-shot walk timer (stock 0x6C0BA/0x6BF9C tail): arm a shot of about
 * `us` microseconds — deliberately SHORT (the DCNTD00 clock select is not
 * decoded; ~28 µs/tick measured, armed at 32 µs/tick). The service closes
 * the loop on TCNTA and re-arms for any remainder. The ISR never re-arms. */
void plat_walktmr_calibrate(void);     /* B2: time DCNTD00 against TCNTA at boot */
void plat_walktmr_arm_us(uint32_t us);
void plat_walktmr_wait_until(uint32_t t);   /* spin until TCNTA reaches t (< 40 µs) */
void plat_walktmr_stop(void);
bool plat_walktmr_running(void);
uint32_t plat_walktmr_count(void);     /* P8 step-0 probe: must advance */
bool plat_walktmr_poll(void);          /* r01e superloop fallback */

/* ---- WDT with reset-cause capture (rebuild design §5.6) ---- */
void plat_wdt_kick(void);
void plat_wdt_mode(bool interval);
void plat_wdt_quick_reset(void) __attribute__((noreturn));
bool plat_wdt_is_interval(void);
void plat_wdt_capture_reset_cause(void); /* once at init */
uint8_t plat_wdt_wovf_count(void);
/* S11 reset-cause witness (.noinit, magic-guarded): raw WDT registers and
 * TCNTA captured at entry BEFORE wdt_init, a boot counter since power-on,
 * and the previous boot's last-alive breadcrumb. */
typedef struct {
    uint32_t magic;
    uint32_t boots;             /* 1 = first boot since power-on */
    uint16_t wtcr, wtcnt, wtsr, wrcr;   /* raw at entry */
    uint32_t tcnta;             /* TCNTA at entry */
    uint32_t prev_alive_t;      /* the previous boot's last breadcrumb */
    uint32_t prev_alive_code;
} plat_rst_t;
void plat_wdt_early_capture(void);       /* app: BEFORE plat_init (wdt_init) */
void plat_app_init(void);                /* app: after plat_init (not the loader) */
/* L0-sense: the stock's AN0 external-multiplexer acquisition (plat_adcmux.c,
 * ~/tcu_sweep/adc_chain_port.md) — publishes 0x90188 (mux 8, output node),
 * 0x907E2 (mux 11, supply), 0x907E4/0x907FC (mux 13 mean/min) */
void plat_adcmux_dma_global_init(void);   /* 0x96010 */
void plat_adcmux_timer_g_init(void);      /* 0x97744 */
void plat_adcmux_init(void);              /* 0x8F9EC */
void plat_adcmux_start(void);             /* r02l: Timer G enable (TGE), deferred */
void plat_adcmux_finish_5ms(void);        /* 0x8F62C */
void plat_adcmux_status_5ms(void);        /* 0x8F5A0 (status ramp part) */
void plat_dei5_isr(void);                 /* 0x95FC4 -> 0x8F058, vector 128 */
void plat_dei4_isr(void);                 /* 0x95FEA, vector 124 */
uint32_t plat_boot_index(void);
uint32_t plat_boot_tcnta(void);
void plat_alive(uint32_t code);
/* r02n: .noinit rolling event ring (two rings; the previous boot's is kept).
 * kind: 'L' launch, 'C' completion, 'M' monitor tick, 'B' batch tick */
void plat_trace(uint8_t kind, uint8_t job, uint8_t n, uint16_t tx0, const uint16_t *rx, uint8_t nrx);          /* breadcrumb: TCNTA + code */

/* ---- FRT microsecond timestamp (design §3) ---- */
uint32_t plat_frt_us(void);

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
/* SSLB2 (third RSPI-B device, SPI mode 0) burst - the stock's cyclic [0x00FF] job. */
bool plat_sup_burst2(const uint16_t *tx, uint16_t *rx, uint8_t n);
/* SSLB0 single-select burst (SPCMD 0xAF01, mode 1 - the odd-frame select of the
 * two-command jobs). E15 (spec 03 section 6.2m): the stock's cyclic job 0x40BC0
 * sends its one-word status-nibble poll to SSLB0 alone (select cfg 0x40B76). */
bool plat_sup_burst0(const uint16_t *tx, uint16_t *rx, uint8_t n);
/* Five-select rotation on bus B (SSLB3/2/1/0/5 in bus-B pin terms; select cfg 0x40CE4 =
 * AF31/AF21/AF11/AF01/AF51 — SPCMD values are PERIPHERAL-RELATIVE: the same table on bus A
 * walks SSLA3/2/1/0/5 = the four driver chips + the dead fifth select; on bus B index 2
 * (AF11 = SSLB1) is the supervisor. E60i decode, spec 03 6.2aw.)
 * CORRECTED 2026-11-04 (spec 03 section 6.2e): 0x40CE4 is the stock's BUS-0 five-select —
 * the ID/write/setpoint commissioning jobs run on RSPI-A (SSLA3/2/1/0/5, driver chips;
 * their live RX staging carries the bus-0 signature A200 x4 + 0000). The supervisor does
 * NOT witness those jobs; earlier bench conclusions drawn from the bus-B rotation (E4/E5
 * "supervisor ignores write/ID jobs") were the wrong bus, not the stock's design. */
bool plat_sup_xfer5(const uint16_t *tx, uint16_t *rx, uint8_t n);
/* Four-select (driver chips only, supervisor excluded) - for WRITES. */
bool plat_sup_xfer4(const uint16_t *tx, uint16_t *rx, uint8_t n);
/* Watchdog mode: interval-timer (enable-edge behavior, 0x978A4) vs reset mode (0x97850). */
void plat_wdt_mode(bool interval);

/* ---- Solenoid driver chips on RSPI-A (spec 03, spec 12 section 2b) ----
 * Send n 16-bit frames; frame i goes to chip i mod 5 (SSLA3, SSLA2, SSLA1, SSLA0, SSLA5) and the
 * reply received during frame i is stored in rx[i]. Blocking (about 13 us per frame); returns
 * false on a bus timeout. */
void plat_drv_init(void);
bool plat_drv_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n);
/* E54: live-set the driver-bus (RSPI-A) bit rate; takes effect at the next exchange. */
void plat_drv_spbr_set(uint8_t spbr);
/* E59 (6.2aq): the interrupt-driven frame feed — fill buffer, arm, ISR feeds. */
bool plat_drv_xfer_irq(const uint16_t *tx, uint16_t *rx, uint8_t n);
/* E60b: staged arm — the SR window stays closed until the scheduler has proven
 * life; arming live separates an init death from a delivery death on the bus. */
void plat_drv_irq_arm(bool on);

/* ---- Park hold PWM output: TOE31 on PL6 (spec 02 section 3.1, spec 12 section 1.1) ----
 * A 1 kHz PWM switch output for the park hold magnet (low-side driver, terminal C7).
 * plat_toe_out_init() configures the timer channel with duty 0 and switches the pin to its
 * timer function last, so the pin stays low (output off) throughout; call it once after
 * plat_init(). plat_toe_out_set() sets the energisation in per mille (1000 = fully on,
 * 0 = off); the duty takes effect through the reload buffer at the next cycle match. */
void plat_toe_out_init(void);
void plat_toe_out_set(uint16_t permil);

/* ---- Mechatronic counterpart boot mailbox (spec 03 section 6.2) ----
 * The stock reads the input word at 0xFFFC0D00 during start-up, before its scheduler:
 * 9/10 select letter 85, 13/14 letter 86, any other value aborts without a handshake
 * (stock 0x67200). The handshake writes 0x1234 to 0xFFFC0D04; the counterpart
 * acknowledges by clearing it; the reply magic (0x5555 letter 85, 0x3737 letter 86)
 * completes the exchange that admits the engaged system mode (stock 0x6728E).
 * The input word may be a power-up transient, so plat_mb_capture() must run at the
 * earliest instruction of main(); the clear-wait machine ticks from a task (~100 ms).
 * Host build: capture/tick are no-ops and the status reads all zero. */
enum {
    PLAT_MB_IDLE = 0u,         /* capture not taken yet */
    PLAT_MB_NO_HANDSHAKE = 1u, /* captured input not in {9,10,13,14}: the stock stops here */
    PLAT_MB_WAIT_CLEAR = 2u,   /* 0x1234 written, waiting for the counterpart's clear */
    PLAT_MB_OK = 3u,           /* cleared; reply magic written */
    PLAT_MB_TIMEOUT = 4u,      /* counterpart did not clear within the bound */
};
typedef struct {
    uint16_t input_capture;    /* 0xFFFC0D00 as latched at start-up */
    uint16_t mailbox_capture;  /* 0xFFFC0D04 as latched at start-up */
    uint8_t  letter;           /* 0x55 / 0x56 / 0 */
    uint8_t  phase;            /* PLAT_MB_* */
    uint16_t mailbox;          /* latest 0xFFFC0D04 value */
} plat_mb_status_t;
void plat_mb_capture(void);
void plat_stock_init_mirror(void);            /* the stock start-up's system writes (spec 03
                                                * section 6.2, disassembled 2026-11-04): module
                                                * stop key/value, RAM cycle, ROM cache, port
                                                * block - the mailbox input may depend on them */
void plat_mb_tick(void);                       /* ~100 ms: the clear-wait machine */
bool plat_mb_poll_init(void);                  /* re-read the input word; a valid value that
                                                * appears later than start-up starts the
                                                * handshake (returns true when it fires) */
void plat_mb_tick_1ms(void);                   /* 1 ms: history sample + init re-check */
const plat_mb_status_t *plat_mb_status(void);
const uint16_t *plat_mb_hist(uint16_t *n);     /* 1 ms input-word history from start-up */

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

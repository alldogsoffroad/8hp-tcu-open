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
#define SOL_CH_PARK_RELEASE 7u  /* MV Pos: park release valve, on/off (spec 12 section 1) */

typedef enum {
    SOL_MODE_DISABLED = 0,
    SOL_MODE_OFF      = 1,
    SOL_MODE_CURRENT  = 2,
    SOL_MODE_FULL_ON  = 3       /* on/off channels (ch 7); first driven in M4b — the FE11
                                 * commissioning routine verifies the chip mode field (spec 12
                                 * section 2b) before the park release valve is trusted with it */
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
    uint16_t   cfg_mismatch;    /* config/cyclic read-back words that did not echo; the reply
                                 * format is open (bench 2026-10-30) — logged, not faulted */
    uint32_t   cycles;
    uint32_t   id_poll_ok;      /* E19: 5 ms ID polls with clean headers */
    uint32_t   id_poll_bad;     /* transport loss or bad headers (never faulted) */
    sol_chip_t chip[SOL_CHIPS];
    ma10_t     current[SOL_CHANNELS];
    uint8_t    mode[SOL_CHANNELS];
    uint16_t   duty_rx[8];      /* slot-2 (SSLA1) replies of the last sol_duty_job()  */
    uint8_t    duty_runs;       /* duty jobs completed (bus timeout does not count)   */
} sol_status_t;

/* Run identification and configuration; all channels off. */
void sol_init(void);
/* E29 (spec 03 section 6.2s): bus-0 traffic gate. OFF at boot = the pure supervisor
 * stream (no cyclic / status / ID polls interleaved with the 5 ms pass); opening the
 * gate lazily runs the start-up commissioning. FE13 sub 15 toggles it live. */
void sol_stream_gate(bool on);
/* Request a channel's mode and current (0.1 mA). Takes effect with the next cyclic job. */
void sol_set(uint8_t ch, sol_mode_t mode, ma10_t current);
/* All channels off. */
void sol_all_off(void);
void sol_tick_10ms(void);
/* E19 (spec 03 section 6.2o): the stock queues the bus-0 ID job EVERY 5 ms pass
 * (engine kick 0x921A0). One 10-frame poll: no retries, no fault latch. */
bool sol_id_poll(void);
const sol_status_t *sol_status(void);

/* Engaged re-configuration (spec 03 section 6.2e): the stock's commissioning write job is a
 * bus-0 job that repeats while engaged, its 0x8E row carrying 0x80 | 0x1F live at HOLD
 * (driving dump 2026-11-04). 0x1F is degenerate there: ladder level or the 1 kHz PWM
 * divider of spec 12 section 2b (32000/f - 1). We send 0x80 | level per ladder step — under
 * the divider reading the ramp values are benign higher PWM frequencies while all setpoints
 * stay zero. Stages one re-send of the 40-word configuration job; runs from the 10 ms task.
 * The rows 0x90 = 0x21, 0x96 = 0x55 and the mode bytes ride the same job / the cyclic job. */
void sol_engage_config(uint8_t level);

/* One-shot 40-word duty job (spec 03 section 6.2e): eight five-select rotations; the slot-2
 * select (SSLA1) receives 0xD0 | duty[r] in rotation r while the other four selects carry the
 * stock's fixed maintenance words (template flash 0x40ECC, driving dump 2026-11-04). The
 * slot-2 replies land in status->duty_rx. Bench-gated; never part of the cyclic stream. */
bool sol_duty_job(const uint16_t duty[8]);

/* exposed for tests */
uint16_t sol_setpoint(ma10_t current);

#endif

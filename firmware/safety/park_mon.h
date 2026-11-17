/* Independent plausibility check of the range the park outputs follow
 * (D-M4-6: kept; rewritten 2026-11-17 for the live stock chain,
 * ~/tcu_sweep/range_chain_live_port.md §4). OURS, NOT STOCK.
 *
 * The range D6D6 is owned by the stock's cyclic chain (app/rng_chain.c).
 * This module does NOT recompute that chain — it is not a second copy of
 * the same code. It checks the chain's OUTPUT against a handful of rules
 * written here from the raw inputs (io/vehicle), with its own copies of
 * the limits:
 *
 *   MON_F_RANGE     D6D6 ∈ {5, 6, 7, 8} with D6D7 == ~D6D6, every pass.
 *                   [O shape: the chain writes the pair (0xC7390 / 0xA548C);
 *                   the stock's park path reads D6D6 raw (0x9B588 →
 *                   0xA7B78) — the check is ours]
 *   MON_F_LEVER     D6D6 leaves 8 only on a pass where the lever source is
 *                   valid and the lever requests exactly the new range.
 *                   [I strong: on this build every chain path out of 8 goes
 *                   through the one-pass lever pulse E53C → 0x8941A → filter
 *                   → D6CE → E545 (0xC7A56) → D6DE (guard 0xC290E: E545 8 →
 *                   only 8) → D6D6; the D6CE rewrites to 6 (0xC6578) need
 *                   U1267, whose D664 then forces 8 / 0x3C (0xC7756)]
 *   MON_F_BRAKE     at that transition the brake interlock holds: (0x140
 *                   valid && pressed) || (0x140 invalid && 0x14B valid &&
 *                   raw / 5 > 10) — the law of D607 (0xA90A0, cal 0x196130
 *                   = 10) re-written from the raw signals. [O law; the
 *                   chain needs D607 = 1 to switch off the 0x89434 filter
 *                   sub-object +0 that maps R/N/D from P to 0]
 *   MON_F_ENGINE    at that transition the engine runs, by our own
 *                   hysteresis on the raw 0x108 speed: on above 430 rpm
 *                   (valid), off after 25 consecutive passes at or below
 *                   380 rpm or invalid (cal 0x191FD2 = 430, 0x191FCC = 380,
 *                   0x191FD4 = 25). [O thresholds (0xC3716 / 0xC3900); the
 *                   chain needs 893E8 ∈ {1, 2} for 0xC7A56 to copy the
 *                   request. Stricter than the stock in two corners, both
 *                   failing safe: an invalid 0x108 counts as 0 rpm (the
 *                   stock's P/N word is EF0A, a dump constant 0 here), and
 *                   893E8 state 2 (auto-stop, needs D61D = 1 — a dump
 *                   constant 0 here) is not accepted]
 *   MON_F_INTEGRITY the park law's output pairs 0xFFF851A8 / AD broken, or
 *                   the monitor's own fault byte broken.
 *
 * Reaction (ours, as before): any violation LATCHES a fault until
 * power-off and removes the output enable — app/park.c then forces both
 * park demands to 0. The stock's own reaction to an implausible range is
 * not decoded.
 */
#ifndef TCU_PARK_MON_H
#define TCU_PARK_MON_H

#include <stdint.h>
#include <stdbool.h>

enum {
    MON_F_RANGE     = 0x01u,        /* D6D6 not 5..8, or its complement broken      */
    MON_F_LEVER     = 0x02u,        /* left 8 without a valid lever asking for it    */
    MON_F_BRAKE     = 0x04u,        /* left 8 without the brake interlock           */
    MON_F_ENGINE    = 0x08u,        /* left 8 without the engine running            */
    MON_F_INTEGRITY = 0x10u         /* corrupt copy in the park law or the monitor   */
};

/* the monitor's own limits (copies; not taken from the chain on purpose) */
#define MON_ENG_ON_RPM      430u    /* 0x191FD2 */
#define MON_ENG_OFF_RPM     380u    /* 0x191FCC / 0x191FD0 */
#define MON_ENG_OFF_PASSES  25u     /* 0x191FD4 */
#define MON_BRAKE_SPEED     10u     /* 0x196130: 0x14B raw / 5 must exceed it */

typedef struct {
    /* raw inputs (io/vehicle), the monitor's own view */
    uint8_t  lever;         /* lever request 5 D, 6 N, 7 R, 8 P; 0 none */
    bool     lever_ok;      /* a valid lever source */
    bool     brake_ok;      /* 0x140 valid */
    bool     brake_on;      /* 0x140 pressed */
    bool     speed14b_ok;   /* 0x14B valid */
    uint16_t speed14b_raw;  /* 0x14B raw (11 bit) */
    bool     engine_ok;     /* 0x108 valid */
    uint16_t engine_rpm;    /* 0x108 engine speed */
    /* the chain's output, as published */
    uint8_t  target;        /* 0xFFF8D6D6 */
    uint8_t  target_c;      /* 0xFFF8D6D7 */
    bool     law_integrity_ok;
} park_mon_in_t;

void park_mon_init(void);
void park_mon_step(const park_mon_in_t *in);
bool park_mon_enable(void);
uint8_t park_mon_faults(void);
bool park_mon_engine_running(void);     /* the monitor's own engine state */

#endif

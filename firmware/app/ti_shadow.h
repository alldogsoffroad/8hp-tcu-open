/* ti_shadow.h - a READ-ONLY shadow of the stock's SHIFT / ENGAGEMENT TORQUE-
 * INTERVENTION request (2026-11-20; decodes ~/tcu_sweep/line_pressure_torque_decode.md §2 and
 * ~/tcu_sweep/ti_amounts_wk_decode.md Task A, ports ~/tcu_sweep/lp_ti_shadow_port.md and
 * ~/tcu_sweep/ti_amounts_port.md - the addresses below re-read in the stock image).
 *
 * WHAT IT IS. The stock arms an intervention object (0xFFF8B9D8, 0xE5A20) at every
 * gear-pair shift and every garage engagement (BD16 = 1 up / 2 down / 3 garage), runs a
 * per-class phase sequence (table 0x51C6C: down 2->3->4->5->6->0xC, up 8->9->0xA->0xB->0xC,
 * garage 8->0xA->0xB->0xC; 0xE5EAA / 0xE630C), and turns its signed output TI+0 (0xDDF84
 * with the second source E3DE) into the torque request 0xFFF8AEC8 = T3 + delta
 * (0xD31D0 / 0xD38B4 / 0xD385C / 0xD3208, type 1 = increase, 2 = reduction, 0 = none;
 * an increase is cancelled while fault reaction 23 is active), which the safety layer
 * packs into the 13-bit fields (Nm x 4 + 2000, 0x1FFE = no request) of 0x2F9 / 0x037
 * with the maximum-torque limit beside it. This shadow computes that chain and holds
 * the would-be field values in a witness. IT NEVER TRANSMITS: nothing here (nor its hook)
 * calls a CAN transmit path, and the scan refuses any such name.
 *
 * PORTED (bytes read; the amounts of the 2026-11-20 port are the shaded part):
 *   the class table 0x634D6 and the phase table 0x51C6C (app/lp_cal.c); the arm 0xE5A20 (the
 *   gear / up / down block indexes: app/ti_cal.c) and start 0xE5E48; the run 0xE5EAA with the
 *   phase loop, the phase entry 0xE6210 (blend inits D2038, the phase-start values), the phase
 *   exits 0xE630C / 0xE74E2 / 0xE727A (the stock timeouts, 200 passes; the sync-progress exits
 *   BD76 >= curve - 128 of phases 0xA / 3 / 4 / 5; phases 8 / 6 / 0xB / 0xC), the amounts
 *   0xE65F4 for every class:
 *     UP    8: 0; 9: the flare limiter 0xE7D5E (BD80 against the turbine flare map of the up
 *           block); 0xA: base 0xE69F4 = 0xE6F5E (map8 of the E584E set, x = BD78 the turbine
 *           peak / 32, y = the torque byte, x6 - 870) x the temperature factor 0xE7782 x the
 *           final scale map, through the accelerating blend 0xD1FB4 (tau / end of the E5814
 *           set), clipped to <= 0 and by the flare limit; 0xB: TI+0x2A x the BD76 fade map
 *           (0xE6CA0) / 100, held >= the previous amount, <= 0; 0xC: 0;
 *     DOWN  2: 0; 3: the coast blip 0xE795A (blend of the phase-start value toward 0xE76DE by
 *           a percent map over the phase timer) behind the gate 0xE73AC, positive only when
 *           the increase mode (0xE7814 / 0xE7844) is on; 4: 0xE76DE or 0xE78EE (increase);
 *           5: 0xE7126 (the reduction under power) or 0xE78EE; 6: TI+6 x curve%/100;
 *     GARAGE 8: 0; 0xA: the gear-block base map (+0x18, flat 145 = 0 Nm on this unit) through
 *           the blend and the gate map; 0xB: TI+0x2A x the BD76 fade map; 0xC: 0;
 *   the end 0xE5FF6 (TI+0x20 = the last negative value, TI+0x1F = the ramp-out length 10 from
 *   the up block +0x24 / the gear block +4) and the ramp-out 0xE6B6E / 0xD1B94 (a linear 10-pass
 *   fade to 0 after the shift end); the increase mode 0xE7814 / 0xE7844 (down: calibration
 *   enable 0x1FE840 = 1 AND the blip base > 0 AND BD22 > 0x1FE83E; up: 0x1FE841 = 0 = off); the
 *   high-torque flag 0xE7C12; E3DE from its calibration enable byte 0x1FE8CB (0 = disabled on
 *   this unit: the value is 0 and counted if the enable were ever set); the combine 0xDDF84 /
 *   0xDDF98 (CORRECTION to the decode of the first port: with TI >= 0 and below E3DE the result
 *   is E3DE - a maximum); the request 0xD31D0 with 0xD38B4 (E330 and the lock-up raise B5E1 /
 *   B5E2 BY VALUE; the hook ti_in_t.b5e1 / b5e2 is where a lock-up shadow plugs in) / 0xD385C
 *   (pass-through) / 0xD3208 and the reaction-23 cancel, the limit's minimum (0xA32E4) and the
 *   field packing.
 *
 * STOOD IN (counted in ti_w_t; every one named in the port report): the phase-9 exit 0xE72E6 and
 * the hold target TI+0x26 (they read the on-coming phase object 0xBE1C, counters 0xDE158 /
 * 0xDE174 / 0xDE13A: the stand-in is "the gear-pair shadow's on-coming object has left its fill
 * phase" plus a 3-pass floor), 0xDDBA8(E33C) in the phase-3 exit (false), the engine-speed byte
 * 0x89C12 (= engine rpm / 32), 0x89C1A = 0x89CA0 = the torque in Nm, the up-shift limit byte TI+0x19
 * (0xE7C8E's gate BD72 / 0xEEAB4: its nominal 0), the variant bytes 0x89C04 / 0x89C06 (= 0), the
 * garage phase-0xA gate 0xE728A (true), 0xE7B2C (TI+2, no consumer here), the 0x10FB52 predicate
 * of the BD18 branch of 0xE7126 (false), the safety monitor's re-encoding (0x1549CA, undecoded),
 * the converter-limit terms other than 0xFFF8B5D0 = 1500, fault reaction 23 (HELD 0). */
#ifndef TCU_APP_TI_SHADOW_H
#define TCU_APP_TI_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the torque-intervention shadow is read-only: an output path may not include ti_shadow.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TI_R23_HELD   0u      /* fault reaction 23 (0x632BD): HELD 0 */
#define TI_SI_MIN9    3u      /* phase 9 lasts at least this many passes (the stand-in of 0xE72E6) */
#define TI_SI_LIM_CONV 1500   /* 0xFFF8B5D0: the converter's limit, 0.1 Nm (dump) */
#define TI_TIMEOUT    200u    /* 0x1FF126 / 0x1FF128: the phase timeout, passes (cal, re-read by the test) */

/* the inputs, BY VALUE (app/lp_hook.c): the stock cell each one stands for */
typedef struct {
    uint8_t  os_mode;   /* +0  0x93B33 */
    uint8_t  bd16;      /* +1  the shift kind 1 up / 2 down / 3 garage (0xFFF8BD16) */
    uint8_t  bd17;      /* +2  0xFFF8BD17 (4: the second down block) */
    uint8_t  bd18;      /* +3  0xFFF8BD18 (the pre-selected context was used) */
    uint8_t  bd1a;      /* +4  0xFFF8BD1A (== 0x0A: the phase logic is skipped) */
    uint8_t  bd1b;      /* +5  0xFFF8BD1B */
    uint8_t  bd22;      /* +6  0xFFF8BD22 the temperature byte of the shift start */
    uint8_t  pf;        /* +7  0x89C30 the committed power flow */
    int16_t  t3;        /* +8  0x89CA8 T3, Nm */
    int16_t  tq;        /* +10 0x89CA0 the input torque, Nm (also 0x89C1A) */
    int16_t  turb;      /* +12 0x89CBA the turbine speed */
    int16_t  eng;       /* +14 0x89C10 the engine speed */
    int16_t  sync_to;   /* +16 ctx+0x2A: output speed x ratio(new gear) / 1000 */
    int16_t  bd80;      /* +18 0xFFF8BD80: turbine - output x ratio(old gear) / 1000 (0x118082) */
    int16_t  bd76;      /* +20 0xFFF8BD76 the sync progress of block A (ctx+0xA) */
    uint16_t now_ms;    /* +22 0x89CF8 */
    uint8_t  c2d;       /* +24 0x89C2D the old gear */
    uint8_t  c27;       /* +25 0x89C27 the new gear */
    uint8_t  c26;       /* +26 0x89C26 the engaged gear + 1 */
    uint8_t  k7;        /* +27 ctx+7 the kind (0x1180BE) */
    uint8_t  ktype;     /* +28 ctx+5 the pair type */
    uint8_t  temp;      /* +29 0x89C13 the conditioned fluid temperature byte */
    uint8_t  on_left_fill; /* +30 the gear-pair shadow's on-coming object (block A) left its fill (phase >= 4): the phase-9 exit stand-in */
    uint8_t  b5e1;      /* +31 0xFFF8B5E1 the lock-up torque raise (BY VALUE hook: a lock-up shadow plugs in here; 0) */
    int16_t  e330;      /* +32 0xFFF8E330 the E32A+6 torque term (BY VALUE: 0xFFF8AFA0's term, 0) */
    uint8_t  b5e2;      /* +34 0xFFF8B5E2 */
    uint8_t  sync_a;    /* +35 block A's sync flag (ctx+0, 0x117F48): the exit of phase 0xA when BD1B != 0 */
} ti_in_t;
_Static_assert(sizeof(ti_in_t) == 36u, "ti_in_t is 36 bytes");

typedef struct {
    uint32_t pass_n;        /* +0x00 passes run (modes 1..3) */
    uint32_t skip_mode_n;   /* +0x04 */
    uint32_t arm_n[4];      /* +0x08 arms by BD16 (index 1 up, 2 down, 3 garage) */
    uint32_t end_n;         /* +0x18 shift ends */
    uint32_t phase_n;       /* +0x1C phase entries */
    uint32_t si_dwell_n;    /* +0x20 phase exits decided by a stand-in (the phase-9 exit, the 3-pass floor) */
    uint32_t si_amount_n;   /* +0x24 amounts taken from a stand-in term (0x89C12 / 0x89C1A / the final-map x / the garage gate) */
    uint32_t si_misc_n;     /* +0x28 E3DE enabled-but-unported / 0xE73AC / limit terms taken at their stand-in */
    uint32_t req_n;         /* +0x2C passes the would-be request type was 1 or 2 */
    uint32_t r23_n;         /* +0x30 increases cancelled by reaction 23 */
    uint32_t err_n;         /* +0x34 an index out of range (0xB75B4, counted) */
    uint32_t red_n;         /* +0x38 passes with a negative amount (a reduction) */
    uint32_t inc_n;         /* +0x3C passes with a positive amount (a blip / increase) */
    uint32_t ramp_n;        /* +0x40 ramp-out passes (the 0xE6B6E / 0xD1B94 fade) */
    uint32_t rsvd[2];       /* +0x44 */
} ti_w_t;
_Static_assert(sizeof(ti_w_t) == 0x4Cu, "ti_w_t is 0x4C bytes");

typedef struct {
    uint8_t  armed;     /* +0x00 TI+0x17 */
    uint8_t  cls;       /* +0x01 TI+4 : 0 down, 1 up, 2 garage */
    uint8_t  step;      /* +0x02 TI+0x24 / the phase table row */
    uint8_t  phase;     /* +0x03 TI+0xB */
    uint16_t tmr;       /* +0x04 TI+0x1C passes in the phase (reset in phases 2 / 9) */
    int16_t  out;       /* +0x06 TI+0 (Nm, signed) */
    uint8_t  bd16_last; /* +0x08 */
    uint8_t  mode;      /* +0x09 E33C+0xC : 0 none / 1 increase / 2 reduction (0xDDF98) */
    uint8_t  type;      /* +0x0A 0xD3208: 0 none / 1 increase / 2 reduction */
    uint8_t  inc;       /* +0x0B TI+0x28 the increase mode */
    int16_t  delta;     /* +0x0C the combined delta, Nm (0xDDF84) */
    int16_t  req_q;     /* +0x0E the request, Nm x 4 (0xFFF8AEC8+0) */
    int16_t  dreq_q;    /* +0x10 request - T3, Nm x 4 (0xFFF8AEC8+2) */
    int16_t  lim_nm;    /* +0x12 the maximum-torque limit, Nm (0xFFF850D0) */
    uint16_t fld_req;   /* +0x14 the would-be 13-bit request field of 0x2F9 / 0x037 */
    uint16_t fld_lim;   /* +0x16 the would-be 13-bit limit field */
    ti_in_t  in;        /* +0x18 the last inputs (36 B) */
    ti_w_t   w;         /* +0x3C */
    /* the stock object's own fields (offsets in the comments) */
    int16_t  a22;       /* +0x88 TI+0x22 the last raw amount */
    int16_t  v6;        /* +0x8A TI+6 */
    int16_t  v8;        /* +0x8C TI+8 */
    int16_t  v2a;       /* +0x8E TI+0x2A the amount at the phase-0xB entry */
    int16_t  v12;       /* +0x90 TI+0x12 the flare limit */
    int16_t  v20;       /* +0x92 TI+0x20 the last negative output at the end (the ramp-out start) */
    int16_t  v30;       /* +0x94 TI+0x30 */
    int16_t  v32;       /* +0x96 TI+0x32 */
    int16_t  bl_start;  /* +0x98 TI+0x38 blend: start */
    int16_t  bl_ts;     /* +0x9A TI+0x38 blend: stamp (ms) */
    int32_t  bl_frac;   /* +0x9C TI+0x38 blend: fraction, per mille */
    uint16_t pc;        /* +0xA0 TI+0xC passes since the phase entry */
    uint16_t t1a;       /* +0xA2 TI+0x1A passes since the shift start */
    uint8_t  rc;        /* +0xA4 TI+0x1E the ramp-out counter */
    uint8_t  rlen;      /* +0xA5 TI+0x1F the ramp-out length */
    uint8_t  hd;        /* +0xA6 TI+0xD the high-torque variant */
    uint8_t  hl;        /* +0xA7 TI+0xE its latch */
    uint8_t  inc_prev;  /* +0xA8 TI+0x29 */
    uint8_t  p5seen;    /* +0xA9 TI+0x16 */
    uint8_t  gi;        /* +0xAA the arm's gear block index */
    uint8_t  ui;        /* +0xAB the arm's up block index */
    uint8_t  di;        /* +0xAC the arm's down block index */
    uint8_t  d4i;       /* +0xAD the arm's BD17 == 4 block index */
    uint8_t  ci;        /* +0xAE the arm's E5814 block index (TI+0x54, class flag 1) */
    uint8_t  bd78;      /* +0xAF ctx+0xC the turbine peak / 32 (upshift) or the new gear's sync speed / 32 */
    uint8_t  pa;        /* +0xB0 TI+0xA passes since the down phase-4 entry */
    uint8_t  pad_b1;
    int16_t  v26;       /* +0xB2 TI+0x26 */
    uint8_t  c18;       /* +0xB4 0x89C18 = fluid byte + 8 */
    uint8_t  c1c;       /* +0xB5 0x89C1C = clamp_u8(torque / 6 + 34) */
    uint8_t  c12;       /* +0xB6 0x89C12 = engine / 32 [I] */
    uint8_t  c0e;       /* +0xB7 0x89C0E = clamp_u8(turbine / 32) */
    int16_t  last_amt;  /* +0xB8 the raw amount of the last pass (witness) */
    int16_t  flare;     /* +0xBA the last flare limit (witness) */
    uint8_t  e3de_en;   /* +0xBC the calibration enable of E3DE (0x1FE8CB) */
    uint8_t  pad_bd[3];
} ti_t;
_Static_assert(sizeof(ti_t) == 0xC0u, "ti_t is 0xC0 bytes (.ti_shadow)");

void ti_shadow_init(void);
void ti_shadow_pass(const ti_in_t *in);       /* the 10 ms pass (modes 1..3) */
const volatile ti_t *ti_shadow_state(void);
void ti_hook_feed(const ti_in_t *in);          /* app/lp_hook.c: the part after the gather (the host harness's entry) */

/* the pure field packer (host tests): Nm x 4 + 2000, 0x1FFF invalid */
uint16_t ti_field13(int32_t q);

#endif /* TCU_APP_TI_SHADOW_H */

/* ti_shadow.h - a READ-ONLY shadow of the stock's SHIFT / ENGAGEMENT TORQUE-
 * INTERVENTION request (2026-11-20; decode ~/tcu_sweep/line_pressure_torque_decode.md
 * §2, port ~/tcu_sweep/lp_ti_shadow_port.md - the addresses below re-read in the stock
 * image).
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
 * PORTED (bytes read): the class table 0x634D6 and the phase table 0x51C6C (app/lp_cal.c),
 * the arm 0xE5A20 (class; the per-type calibration indexes are counted), the start 0xE5E48,
 * the run 0xE5EAA with the phase loop (the immediate exits of the garage / up phase 8 and the
 * down phase 2, the terminal hold of phase 0xC), the end 0xE5FF6, the combine 0xDDF84 /
 * 0xDDF98 (CORRECTION to the decode: with TI >= 0 and below E3DE the result is E3DE - a
 * maximum, not a minimum), the request 0xD31D0 with 0xD38B4 / 0xD385C (pass-through) /
 * 0xD3208 and the reaction-23 cancel, the limit's minimum (0xA32E4) and the field packing.
 *
 * STOOD IN (counted in ti_w_t): the phase DWELLS (the exit tests 0xE727A / 0xE72E6 / 0xE74E2
 * read per-type calibration blocks behind the cache objects of 0xE5A20 - TI_SI_DWELL
 * passes), every phase AMOUNT (0xE65F4 / 0xE69F4 / 0xE7126 / 0xE6F5E / 0xE6848 / 0xE73AC -
 * zero; the decoded zeros of phases 2, 8, 0xC are exact) and the output shaping 0xE6B6E /
 * 0xE618C, the second torque source E3DE (0), the converter-limit flags (B5E1 / B5E2) and
 * E330's term in 0xD38B4 (0), the limit terms other than the converter limit (the dump's
 * 0xFFF8B5D0 = 1500, i.e. 150 Nm), fault reaction 23 (HELD 0), the safety monitor's
 * re-encoding (0x1549CA, undecoded). */
#ifndef TCU_APP_TI_SHADOW_H
#define TCU_APP_TI_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the torque-intervention shadow is read-only: an output path may not include ti_shadow.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TI_R23_HELD   0u      /* fault reaction 23 (0x632BD): HELD 0 */
#define TI_SI_DWELL   3u      /* passes a phase lasts when its exit test is a stand-in */
#define TI_SI_LIM_CONV 1500   /* 0xFFF8B5D0: the converter's limit, 0.1 Nm (dump) */

typedef struct {
    uint8_t  os_mode;   /* +0 0x93B33 */
    uint8_t  bd16;      /* +1 the shift kind (live 0xFFF8BD16, else the gear-pair shadow's) */
    uint8_t  bd1a;      /* +2 0xFFF8BD1A (== 0x0A: the phase logic is skipped) */
    uint8_t  pad3;
    int16_t  t3;        /* +4 0x89CA8 T3, Nm */
    int16_t  e3de;      /* +6 0xFFF8E3DE the second torque source (stand-in 0) */
    uint16_t now_ms;    /* +8 */
    uint16_t pad10;
} ti_in_t;
_Static_assert(sizeof(ti_in_t) == 12u, "ti_in_t is 12 bytes");

typedef struct {
    uint32_t pass_n;        /* +0x00 passes run (modes 1..3) */
    uint32_t skip_mode_n;   /* +0x04 */
    uint32_t arm_n[4];      /* +0x08 arms by BD16 (index 1 up, 2 down, 3 garage) */
    uint32_t end_n;         /* +0x18 shift ends */
    uint32_t phase_n;       /* +0x1C phase entries */
    uint32_t si_dwell_n;    /* +0x20 phase exits decided by the dwell stand-in */
    uint32_t si_amount_n;   /* +0x24 passes whose amount is the stand-in (not a decoded zero) */
    uint32_t si_misc_n;     /* +0x28 E3DE / converter-flag / E330 / limit terms taken at their stand-in */
    uint32_t req_n;         /* +0x2C passes the would-be request type was 1 or 2 */
    uint32_t r23_n;         /* +0x30 increases cancelled by reaction 23 */
    uint32_t err_n;         /* +0x34 an index out of range (0xB75B4, counted) */
} ti_w_t;
_Static_assert(sizeof(ti_w_t) == 0x38u, "ti_w_t is 0x38 bytes");

typedef struct {
    uint8_t  armed;     /* +0x00 TI+0x17 */
    uint8_t  cls;       /* +0x01 TI+4 : 0 down, 1 up, 2 garage */
    uint8_t  step;      /* +0x02 TI+0x24 / the phase table row */
    uint8_t  phase;     /* +0x03 TI+0xB */
    uint16_t tmr;       /* +0x04 TI+0x1C passes in the phase */
    int16_t  out;       /* +0x06 TI+0 (Nm, signed) */
    uint8_t  bd16_last; /* +0x08 */
    uint8_t  mode;      /* +0x09 E33C+0xC : 0 none / 1 increase / 2 reduction (0xDDF98) */
    uint8_t  type;      /* +0x0A 0xD3208: 0 none / 1 increase / 2 reduction */
    uint8_t  pad0b;
    int16_t  delta;     /* +0x0C the combined delta, Nm (0xDDF84) */
    int16_t  req_q;     /* +0x0E the request, Nm x 4 (0xFFF8AEC8+0) */
    int16_t  dreq_q;    /* +0x10 request - T3, Nm x 4 (0xFFF8AEC8+2) */
    int16_t  lim_nm;    /* +0x12 the maximum-torque limit, Nm (0xFFF850D0) */
    uint16_t fld_req;   /* +0x14 the would-be 13-bit request field of 0x2F9 / 0x037 */
    uint16_t fld_lim;   /* +0x16 the would-be 13-bit limit field */
    ti_in_t  in;        /* +0x18 the last inputs */
    ti_w_t   w;         /* +0x24 */
} ti_t;
_Static_assert(sizeof(ti_t) == 0x5Cu, "ti_t is 0x5C bytes (.ti_shadow)");

void ti_shadow_init(void);
void ti_shadow_pass(const ti_in_t *in);       /* the 10 ms pass (modes 1..3) */
const volatile ti_t *ti_shadow_state(void);
void ti_hook_feed(const ti_in_t *in);          /* app/lp_hook.c: the part after the gather (the host harness's entry) */

/* the pure field packer (host tests): Nm x 4 + 2000, 0x1FFF invalid */
uint16_t ti_field13(int32_t q);

#if !defined(__sh__)
void ti_shadow_host_amount(int16_t nm);       /* inject the phase amount stand-in (tests only) */
#endif

#endif /* TCU_APP_TI_SHADOW_H */

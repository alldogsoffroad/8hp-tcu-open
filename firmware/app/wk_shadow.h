/* wk_shadow.h - a READ-ONLY shadow of the stock's TORQUE-CONVERTER LOCK-UP (WK) strategy
 * (2026-11-20; decode ~/tcu_sweep/ti_amounts_wk_decode.md Task B, port ~/tcu_sweep/wk_shadow_port.md;
 * the addresses below re-read in the stock image).
 *
 * WHAT IT IS. The stock's lock-up object 0xFFF8B294 runs a 12-state machine (0xDEA5E: 1 open,
 * 2 prefill, 3 fill ramp, 4 engaged ramp, 5/6/8 closed, 7 re-engage, 9/10 controlled slip,
 * 0xB release) in the 10 ms task's slot 39 (0xE106E -> 0xDE688) when OS mode is not 1; in OS mode 1
 * the reset list (0xE0FE4 -> 0xDE614: state 1, mode byte 0xFFF8B4AC = 1, pressure 0xFFF8B2AA = 0)
 * runs instead. The pressure it computes becomes the WK request 0xFFF8B2AA (mbar) and the mode
 * 0xFFF8B4AC (0xDF004), which the producer gate 0xDCE40 offers to the actuator law
 * (0xE54A4 -> 0x107FB8: the binary law 0x10809C while the mode byte is 1, else the curve 0xE13C +
 * the normally-closed temperature map 0xE214). Today our LIVE engine compiles WK to the constant
 * 500 (D-G6-10). This shadow computes what the stock would command - the gate, the state, the
 * pressure, the mode, the would-be WK current - and compares the current with the live WK every
 * pass in a witness. It NEVER writes a live cell, never offers anything to the producer, never
 * reaches a bus. Nothing on the live path reads it.
 *
 * PORTED (the bytes read; every address re-checked in the stock image):
 *   0xDCE40 / 0xDCD38 the producer gate (1:1); 0xDE614 the OS-mode-1 reset; 0xDE688's frame
 *   (the +0x2B / +0x2C state flags, the engagement edge), 0xDEA5E the full 12-state transition
 *   structure with every transition predicate (0xDF3E4 entry, 0xDF738 / 0xDF300 / 0xDF1FE timers,
 *   0xDF61C / 0xDF63A / 0xDF6B4 / 0xDF6EC / 0xDF6CA / 0xDF594 / 0xDF58C aborts and releases,
 *   0xDF2B8 / 0xDF3A4 / 0xDF30E / 0xDF742 / 0xDF7A6 / 0xDF21C / 0xDF248 the slip tests), the
 *   thresholds from the stock calibration in place (500 / 750 rpm, 0 degC + 5 hysteresis, 135 /
 *   150 degC, slip limits 10 / 10 / 10 / 5, the release 0.5 bar), the traction flag and the
 *   request-level table (0xDFD2C: cal 0x19F410 = [0, 1, 2, 3]), the output stage 0xDF004 (the map
 *   D10+0x48, the ceiling curve D10+0x34 = 12.5 bar, B2AC = B2AA - 1000), the slip controller
 *   (feed-forward map D10+0x140 against the turbine torque, the ramp-in curve D10+0x130, the
 *   I-only PID: Ki = 0.05 x 0.01 bar per rpm per pass toward the 40 rpm target, the ceiling) and
 *   the WK current law (0xE54A4 -> 0x107FB8: the binary law with the NC roles, else the curve
 *   0xE13C + the converter offset + the NC temperature map 0xE214 + the 0xD9F10 correction map -
 *   verified all zero -, clamp 500 .. 8500).
 *
 * STOOD IN (each counted in wk_w_t, named wk_in_t field or WK_SI_* constant):
 *   - the strategy's request index 0xFFF8A0A1 (the stock writer is unlocated: a counted by-value input,
 *     the hook feeds 0); the abort / inhibit flags E352 (0xDFE68, 314 instructions), the converter-object
 *     flags 0xBA2A4 / 0xBA2AC(E612), the busy test 0xBB13E(E61C, 5), the range-change test 0xDFB7C and the
 *     B6AC / B6AD / B6DA flags: BY-VALUE inputs (the hook feeds 0 / the dump's values);
 *   - the speed conditioning 0xE1AF6 (PT1 filters, slip gradients): the filtered values = the raw ones;
 *   - the state ACTIONS' ramp generators (0x10B172 / 0x10BB8A / 0x10BD7E / 0x10B5F8 / 0x10C7E0 / 0x10C8A6 /
 *     0x10B2EA / 0x10B534 / 0x10B4AE, the prefill map 0x10BC24, the fill adaptation reader 0xAEE0E - held
 *     CONSTANT 0): prefill / fill / engage / release pressures and durations are the WK_SI_* constants;
 *   - the inhibit's other terms (+0x33), the converter limit B5E1's hold input, the output mode 0xDF0A8.
 *
 * KEPT OUT: 0xBF8E8 (the engagement counter written to EEPROM block 0x0304: the shadow only COUNTS the
 * 0 -> 1 edge of its closed flag, eng_cnt_n), the fill-adaptation learner 0xAEE26 / 0xAEE1C / 0xAEE2E
 * (-> 0xCEE94 / 0xCE48C / 0xCE00C), the event log 0xB79E4 (the 1099 / 0x1C2 reports are counted).
 *
 * OUTPUT FOR THE TORQUE SHADOW: wk_b5e1() / wk_b5e2() (0xE14EC): the lock-up torque raise the stock's
 * 0xD38B4 adds to T3 while B5E2 == 1 (another agent's torque shadow consumes it). */
#ifndef TCU_APP_WK_SHADOW_H
#define TCU_APP_WK_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the lock-up shadow is read-only: an output path may not include wk_shadow.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* the stand-ins' constants (labelled; the stock's values sit behind the unported ramp generators) */
#define WK_SI_PREFILL_N     20u      /* passes of the prefill state */
#define WK_SI_FILL_N        30u      /* passes of the fill ramp */
#define WK_SI_ENGAGE_N      20u      /* passes of the engaged ramp */
#define WK_SI_S10_N         50u      /* the controlled-slip ramp-out timer (0x27A) */
#define WK_SI_PREFILL_MBAR  1000     /* prefill pressure, mbar */
#define WK_SI_ENGAGE_MBAR   3000     /* the fill ramp's end, mbar */
#define WK_SI_REL_MBAR      250      /* the release ramp's step per pass, mbar */
#define WK_SI_BAR_STEP      0.20f    /* bar per pass: engaged ramp up to the closed pressure, ramp-out */

/* the inputs, BY VALUE (app/wk_hook.c): the stock cell each one stands for */
typedef struct {
    uint8_t  os_mode;     /* +0  0x93B33 */
    uint8_t  c3d;         /* +1  0x89C3D the mode-1 flag (the reset list runs while set) */
    uint8_t  c2f;         /* +2  0x89C2F the completed power flow */
    uint8_t  c30;         /* +3  0x89C30 the committed power flow (1 = forward) */
    uint8_t  cf6;         /* +4  0x89CF6 (fault reaction 49 / 42: the gate is shut) */
    uint8_t  e612_a;      /* +5  0xBA2A4(E612) == 1 (stand-in) */
    uint8_t  e612_b;      /* +6  0xBA2AC(E612) == 1 (stand-in) */
    uint8_t  e61c_busy;   /* +7  0xBB13E(E61C, 5) (stand-in 0) */
    uint8_t  req_idx;     /* +8  0xFFF8A0A1 the strategy's lock-up request index (producer unlocated: 0) */
    uint8_t  req_force0;  /* +9  E352 forces the request 0 (stand-in) */
    uint8_t  e353;        /* +10 E353: the abort flag (stand-in) */
    uint8_t  shift_busy;  /* +11 the shift / tip-in terms: 0xB6AC, 0xBAB2C(E61A), the range test 0xDFB7C */
    uint8_t  range_ok;    /* +12 0xBA196(E60F) in {0, 6} */
    uint8_t  inhibit_x;   /* +13 the inhibit's other terms (0x89D2C, B618) */
    uint8_t  temp_byte;   /* +14 0x89C13 (the NC temperature map's y) */
    uint8_t  pad15;
    int16_t  temp_c;      /* +16 fluid temperature, C (0x89C13 - 40) */
    uint16_t eng;         /* +18 engine speed, rpm (the filtered value B5FC+0 = raw) */
    uint16_t turb;        /* +20 turbine speed, rpm (B5FC+0x10 = raw) */
    int16_t  tq;          /* +22 turbine torque, Nm (B542) */
    uint16_t line;        /* +24 the line pressure target E61A, mbar (the line-pressure shadow's) */
    uint16_t now_ms;      /* +26 */
    uint16_t live_wk;     /* +28 the LIVE law's WK current (0xFFF94ED4 + 10), witness only */
    uint16_t pad30;
} wk_in_t;
_Static_assert(sizeof(wk_in_t) == 32u, "wk_in_t is 32 bytes");

/* the witnesses */
typedef struct {
    uint32_t pass_n;        /* +0x00 10 ms passes run (modes 1..3) */
    uint32_t reset_n;       /* +0x04 passes the reset list ran (OS mode 1) */
    uint32_t gate_n;        /* +0x08 passes the producer gate 0xDCE40 was true */
    uint32_t cmp_n;         /* +0x0C passes compared with the live WK */
    uint32_t diff_n;        /* +0x10 passes where the would-be WK current differs from the live one */
    uint32_t perm_n;        /* +0x14 passes the machine left state 1 (== 0 on every dry-bench input) */
    uint32_t eng_cnt_n;     /* +0x18 would-be 0xBF8E8 calls (the 0 -> 1 edge of the closed flag; NEVER executed) */
    uint32_t abort_n;       /* +0x1C passes with the abort flag E353 */
    uint32_t err_n;         /* +0x20 a state outside 1..0xC (the stock logs event 1099 and resets) */
    uint32_t fault_n;       /* +0x24 the law's 0x1C2 reports (counted, not raised) */
    uint32_t si_req_n;      /* +0x28 passes the request index (0xFFF8A0A1) was a stand-in input */
    uint32_t si_ramp_n;     /* +0x2C passes a state action's pressure was a WK_SI_* stand-in */
    uint32_t si_filt_n;     /* +0x30 passes the speed conditioning was the raw value */
    uint32_t si_flag_n;     /* +0x34 passes an E352 / B6xx flag was a by-value input */
    uint32_t trans_n;       /* +0x38 state transitions */
    uint32_t us_last;       /* +0x3C the last pass's duration, us (wk_hook's note) */
    uint32_t us_max;        /* +0x40 the longest */
    uint16_t seen;          /* +0x44 bit n set: state n was visited */
    uint16_t pad46;
} wk_w_t;
_Static_assert(sizeof(wk_w_t) == 0x48u, "wk_w_t is 0x48 bytes");

/* the shadow's state: the object 0xFFF8B294 (stock offsets in the comments) and the result */
typedef struct {
    uint8_t  state;       /* +0x00 B294+4 */
    uint8_t  prev;        /* +0x01 B294+0x1A the previous state */
    uint8_t  closed;      /* +0x02 B294+0x14 0 open / 1 closed / 2 closed level 2 */
    uint8_t  b4ac;        /* +0x03 B4AC = B294+0x218: 1 = open (binary law) */
    uint8_t  inhibit;     /* +0x04 B294+0x33 */
    uint8_t  fl_ok;       /* +0x05 B294+0x2D the fluid-warm hysteresis flag */
    uint8_t  req;         /* +0x06 B526 the request level 0..3 */
    uint8_t  trac;        /* +0x07 B524+1 the traction flag */
    uint8_t  trac_last;   /* +0x08 B524+0xC */
    uint8_t  gate;        /* +0x09 the producer gate 0xDCE40 */
    uint8_t  slip_run;    /* +0x0A the slip controller's sub-phase (0 ramp-in, 1 run) */
    uint8_t  b5e2;        /* +0x0B 0xFFF8B5E2 */
    uint8_t  f2b;         /* +0x0C B294+0x2B state == 3 */
    uint8_t  f2c;         /* +0x0D B294+0x2C state == 4 */
    uint8_t  b5e1;        /* +0x0E 0xFFF8B5E1, Nm / 10 */
    uint8_t  mode_out;    /* +0x0F the mode the producer offers (B4AC, or 1 when the gate is shut) */
    uint16_t t_pre;       /* +0x10 B294+0x258 the prefill / engage timer */
    uint16_t t_fill;      /* +0x12 B294+0x24E the fill counter */
    uint16_t t_s10;       /* +0x14 B294+0x27A */
    uint16_t b2aa;        /* +0x16 B2AA = B294+0x16, mbar */
    int16_t  b2ac;        /* +0x18 B2AC */
    uint16_t p_mbar;      /* +0x1A B294+0x34 */
    float    p_bar;       /* +0x1C B294+0 the pressure, bar */
    float    integ;       /* +0x20 the slip controller's integrator (ramp-in value) */
    float    ff;          /* +0x24 the torque feed-forward, bar */
    float    ceil;        /* +0x28 the pressure ceiling, bar */
    int16_t  slip;        /* +0x2C B5FC+8 engine - turbine */
    int16_t  tq_hold;     /* +0x2E the torque max-hold (B5E0+0xC's source) */
    uint16_t req_mbar;    /* +0x30 the request offered: max(0, B2AA) when the gate is open, else 0 */
    uint16_t wk_i;        /* +0x32 the would-be WK current, 0.1 mA */
    wk_in_t  in;          /* +0x34 the last inputs */
    wk_w_t   w;           /* +0x54 */
} wk_t;
_Static_assert(sizeof(wk_t) == 0x9Cu, "wk_t is 0x9C bytes (.wk_shadow)");

void wk_shadow_init(void);
void wk_shadow_pass(const wk_in_t *in);        /* the 10 ms pass: slots 37..39 (modes 1..3) */
const volatile wk_t *wk_shadow_state(void);
uint8_t wk_b5e1(void);                         /* 0xFFF8B5E1, Nm / 10: the lock-up torque raise (0 unless engaging) */
uint8_t wk_b5e2(void);                         /* 0xFFF8B5E2 == 1: the raise applies */
void wk_shadow_note_us(uint32_t us);           /* the hook's pass duration (witness only) */
void wk_hook_init(void);                       /* app/wk_hook.c */
void wk_hook_10ms(void);                       /* main.c's task_10ms, after lp_hook_10ms */
void wk_hook_feed(const wk_in_t *in);          /* the part after the gather (also the host harness's entry) */

/* the host's pure entry points (tests) */
uint16_t wk_law(uint16_t p, uint8_t mode, uint8_t temp_byte, uint16_t line, int16_t offset);  /* 0xE54A4 -> 0x107FB8 */
bool wk_gate(const wk_in_t *in);               /* 0xDCE40 */

#if !defined(__sh__)
void wk_shadow_host_req(uint8_t v);            /* the request-index seam (tests): the hook feeds it, the SH image has none */
uint8_t wk_shadow_host_req_get(void);
#endif

#endif /* TCU_APP_WK_SHADOW_H */

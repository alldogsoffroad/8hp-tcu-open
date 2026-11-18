/* fb_shadow.h — a READ-ONLY shadow of the stock's PWM-FEEDBACK CHAIN
 * (app/fb_shadow.c; ~/tcu_sweep/feedback_chain_decode.md, the port
 * ~/tcu_sweep/feedback_shadow_port.md).
 *
 * What the stock does [O bytes]: each bus-A channel's driver chip returns a
 * 1 kHz active-low PWM whose low time is the chip's switch-on time; Timer F
 * measures it (PWM-measurement mode, F1 = plat_sh7254r.c fb_capture_init);
 *   - the Timer-D0 CMID00 ISR 0x90A72 (vector 216) runs the 4-state window
 *     sampler 0x90844 and twice per 10 240 µs cycle snapshots every channel
 *     (0x9076E → the capture service 0x97648) into windows 0 / 1;
 *   - the task part 0x904FA (0x8EE12) re-runs 0x97648 under SR.I = 6 (the
 *     timeout path, pin level read from PHPR / PFPR) and copies the windows
 *     into the stable snapshot;
 *   - 0x7CBA8 → 0x83A1C (the coil-resistance thermal model 0x83ABC / its
 *     complement twin 0x83CA8, one channel per pass every 2 s) and 0x838B0
 *     (per channel the classifier + duty filter 0x89A8C / 0x89EDA and the
 *     current estimate 0x8390C), before the law, 5 ms in mode 0 / 10 ms in
 *     modes 1..3.
 * The SHADOW runs all of that into ITS OWN cells (struct fbs_t, linker
 * section .fb_shadow at 0xFFF86A00, ours): it writes no stock cell, no
 * output and no bus-A job; every consumer (0x89F5C, 0x84644, the law) keeps
 * the stand-in od_838b0_standin (bsw/drv/drv_odiag.c). Per pass it compares
 * its class / duty / estimate / resistance with the stand-in's.
 *
 * NOT PORTED (stated): state 1's bus-A readback job 0x92812 (an ISR-context
 * bus-A launch into our CPU-fed engine) and its record bytes 0x91119 /
 * 0x91121 / the 0x90830 handshake — counted (st1_n); the dither 0x9334C held
 * 0 (the 0x18 / 0x19 branches are ported but unreachable); 0x9393A (the
 * second temperature, board) held at the dump's 0x0215 (33.3 °C); 0x9090B
 * held at the dump's 0x20; the group table 0x901DC.. held at the dump's
 * (5 chips, channels 0..9, chip ids 0..4); the 0x6BDFE complement check of
 * 0x83CA8 counts (pair_bad_n) instead of the stock's fatal 0x221.
 *
 * READ-ONLY, ENFORCED: every output TU defines TCU_OUTPUT_PATH — this header
 * #errors under it; app/fb_shadow.c defines TCU_FB_SHADOW_TU and may include
 * no live header (fb_shadow_scan.py, sh-elf-gcc checks); its live inputs
 * arrive BY VALUE (fbs_in_t, app/fb_hook.c). */
#ifndef TCU_APP_FB_SHADOW_H
#define TCU_APP_FB_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the feedback shadow is read-only: an output path may not include fb_shadow.h"
#endif

#include <stdint.h>
#include <stddef.h>

/* the live values one task pass reads (app/fb_hook.c fills them) */
typedef struct {
    uint8_t  n5;            /* 5 ms countdown steps since the last pass (mode 0: 1, else 2) */
    uint16_t rail_mv;       /* 0x94ED0[0] (0x8390C's V) */
    int16_t  t_fluid;       /* 0x94F9C (°C · 16) */
    int16_t  i_req[10];     /* 0x94ED4 (the previous law pass's current, 0.1 mA) */
    /* the stand-in's outputs this pass (drv_odiag.c od_838b0_standin) */
    uint8_t  si_cls[10];    /* 0x93A2E */
    uint16_t si_duty[10];   /* 0x9340C */
    uint16_t si_est[10];    /* 0x93394 */
    uint16_t si_res[10];    /* 0x933A8 */
} fbs_in_t;

/* the capture state 0x90334 + 12·ch */
typedef struct {
    uint32_t p;             /* +0 period (counts) / 0xFFFFFFFF timed out */
    uint32_t on;            /* +4 P − GRAF (sub 0) / GRAF (sub 1); 0 / −1 = pin level */
    uint8_t  fl;            /* +8 0 start, 0x10 restarted, 0x21 valid, 0x31 long, 0x40 timeout */
    uint8_t  pad[3];
} fbs_st_t;
_Static_assert(sizeof(fbs_st_t) == 12u, "fbs_st_t is the stock's 12-byte state");

/* per channel: the shadow vs the stand-in (ours) */
typedef struct {
    uint32_t agree_n;       /* passes with real class == stand-in class */
    uint32_t dis_n;         /* passes with real class != stand-in class */
    uint16_t si_duty;       /* the stand-in's duty / estimate / R this pass */
    uint16_t si_est;
    uint16_t si_res;
    uint16_t dduty_max;     /* max |real duty − stand-in duty| (classes agreeing) */
    uint8_t  si_cls;
    uint8_t  dis_real;      /* the last disagreeing pair (real, stand-in) */
    uint8_t  dis_si;
    uint8_t  pad;
} fbs_cmp_t;
_Static_assert(sizeof(fbs_cmp_t) == 20u, "fbs_cmp_t is 20 bytes (tools/bench witness_fbshadow.py)");

typedef struct {
    /* 0x000 the window sampler, group 0 (0x9020C..0x90217) */
    uint16_t iv[3];         /* 0x9020C t1, 0x9020E t2, 0x90210 t3 − 500 */
    uint8_t  reload;        /* 0x90212 */
    uint8_t  state;         /* 0x90213 */
    uint32_t time;          /* 0x90214 (24 bit) */
    uint8_t  m94434;        /* 0x94434 */
    uint8_t  m9091a;        /* 0x9091A (+3 per pass, −1 per sampler call) */
    uint8_t  m9090b;        /* 0x9090B [S] the dump's 0x20 */
    uint8_t  pad0;
    fbs_st_t st[10];        /* 0x010: 0x90334 */
    uint16_t win_p[20];     /* 0x088: 0x902BC [2ch + w] (µs) */
    uint16_t win_a[20];     /* 0x0B0: 0x902E4 (low time µs) */
    uint16_t snap_p[20];    /* 0x0D8: 0x9026C */
    uint16_t snap_a[20];    /* 0x100: 0x90294 */
    uint16_t duty[10];      /* 0x128: 0x9340C */
    uint16_t dutyf[10];     /* 0x13C: 0x93420 */
    int32_t  fstate[10];    /* 0x150: 0x93434 */
    uint16_t d0[10];        /* 0x178: 0x9345C */
    uint16_t d1[10];        /* 0x18C: 0x93470 */
    uint16_t est[10];       /* 0x1A0: 0x930E4 */
    uint16_t estm[10];      /* 0x1B4: 0x93394 */
    uint16_t res[10];       /* 0x1C8: 0x933A8 */
    uint16_t rsum[10];      /* 0x1DC: 0x933BC */
    uint16_t rfast[10];     /* 0x1F0: 0x933D0 */
    uint16_t rslow[10];     /* 0x204: 0x933E4 */
    uint8_t  cls[10];       /* 0x218: 0x93A2E */
    uint8_t  hold[10];      /* 0x222: 0x93A24 */
    uint8_t  rfirst[10];    /* 0x22C: 0x93964 */
    uint8_t  rfirst2[10];   /* 0x236: 0x9396E */
    uint16_t finit;         /* 0x240: 0x93A22 */
    int16_t  tdrv;          /* 0x242: 0x93A10 */
    int16_t  tfl;           /* 0x244: 0x94FD6 */
    int16_t  amb;           /* 0x246: 0x94FD4 */
    uint16_t ambc;          /* 0x248: 0x94E4C */
    uint16_t tflc;          /* 0x24A: 0x94E4E */
    uint16_t cd476e;        /* 0x24C: 0x9476E (our copy of the 5 ms countdown) */
    uint8_t  c9410c;        /* 0x24E: 0x9410C (0x838B0 first call) */
    uint8_t  c9410d;        /* 0x24F: 0x9410D (0x83A1C channel) */
    uint8_t  c9410e;        /* 0x250: 0x9410E (0x83A1C call count) */
    uint8_t  c84a21;        /* 0x251: 0x84A21 (ambient-update flag) */
    uint16_t pad1;
    /* 0x254 our witnesses */
    uint32_t isr_n;         /* sampler ISR entries */
    uint32_t isr_loops;     /* catch-up iterations (beyond the first per entry) */
    uint32_t isr_max_us;    /* longest ISR, TCNTA µs */
    uint32_t lock_max_us;   /* longest SR.I = 6 window of the task part */
    uint32_t win_n[2];      /* window snapshots 0 / 1 */
    uint32_t st1_n;         /* state-1 passes (the readback job 0x92812 NOT launched) */
    uint32_t tick_n;        /* task passes */
    uint32_t pair_bad_n;    /* 0x6BDFE value / complement mismatches (stock: fatal 0x221) */
    uint32_t r_upd_n;       /* 0x83ABC calls */
    uint32_t armed;         /* 1: the sampler was armed (plat_fbsamp_start) */
    uint32_t isr_last_us;   /* the last ISR's duration */
    fbs_cmp_t cmp[10];      /* 0x284 */
} fbs_t;
_Static_assert(offsetof(fbs_t, st) == 0x010u, "fbs_t layout (witness_fbshadow.py)");
_Static_assert(offsetof(fbs_t, win_p) == 0x088u, "fbs_t layout");
_Static_assert(offsetof(fbs_t, snap_p) == 0x0D8u, "fbs_t layout");
_Static_assert(offsetof(fbs_t, duty) == 0x128u, "fbs_t layout");
_Static_assert(offsetof(fbs_t, est) == 0x1A0u, "fbs_t layout");
_Static_assert(offsetof(fbs_t, res) == 0x1C8u, "fbs_t layout");
_Static_assert(offsetof(fbs_t, cls) == 0x218u, "fbs_t layout");
_Static_assert(offsetof(fbs_t, finit) == 0x240u, "fbs_t layout");
_Static_assert(offsetof(fbs_t, isr_n) == 0x254u, "fbs_t layout");
_Static_assert(offsetof(fbs_t, cmp) == 0x284u, "fbs_t layout");
_Static_assert(sizeof(fbs_t) == 0x34Cu, "fbs_t is 0x34C bytes (.fb_shadow, witness_fbshadow.py)");

void fb_shadow_init(void);          /* clear every shadow cell (before the start) */
void fb_shadow_start(void);         /* arm the Timer-D0 sampler (0x90010's arming) */
void fb_shadow_isr(void);           /* vector 216 body (0x90A72) */
void fb_shadow_tick(const fbs_in_t *in);   /* 0x904FA capture part, 0x83A1C, 0x838B0 */

const volatile fbs_t *fb_shadow_state(void);

#if !defined(__sh__)
/* host only (tests/unit/test_fb_shadow.c) */
volatile fbs_t *fb_shadow_host_state(void);
void fb_shadow_host_classify(uint8_t ch);   /* 0x89A8C on the current snapshot */
void fb_shadow_host_est(uint8_t ch, uint16_t v, int16_t i, uint16_t d);   /* 0x8390C */
void fb_shadow_host_rupd(uint8_t ch, int16_t i, uint8_t flag, int16_t t_fluid);  /* 0x83ABC */
#endif

#endif /* TCU_APP_FB_SHADOW_H */

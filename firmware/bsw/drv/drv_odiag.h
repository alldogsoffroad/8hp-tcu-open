/* drv_odiag.h — the stock's solenoid OUTPUT DIAGNOSIS (2026-11-17,
 * ~/tcu_sweep/output_diag_port.md). Ported from the bytes:
 *
 *   bus-A reply harvest  0x921E0 (ID) · 0x92298 (diag) · 0x923BC (config) ·
 *                        0x92530 (cyclic) · 0x9218C · the diag job 0x9225C
 *                        (template 0x40CEE) queued 3.8 ms after each cyclic
 *                        (0x92060 Timer D one-shot → ISR 0x92078)
 *   chip status          0x92A48 / 0x92A04 / 0x931F8 summaries 0x9096C and
 *                        the 0x9030C words of 0x904FA (10 ms entry 0x8EE12)
 *   per-channel diag     0x89F5C (+ 0x8E8E4, 0x8A6D8, 0x8E7B4, 0x8ECF0,
 *                        0x86414) → 0x7BA2A → 0x93004 → 0x7BA76 (the law's
 *                        fault class) · 0x897AC (request/state 0x93B0C) ·
 *                        0x8A788 + 0x8ACFE (current bounds) · 0x8422E (rail
 *                        filter) · 0x84392 + 0x89718 (complement mirror)
 *   DSM groups 1–20      0x7B222 (+ 0x7B17C, 0x82D4C, table 0x45C54) from
 *                        0x7B9B8 (list entry 0x7ABC8, right after the law)
 *   WITNESS ONLY         0x84644 (unintended output → fatal 0x226) ·
 *                        0x8493A (complement checks → fatal 0x221) · the
 *                        0x83DB0 inline 0x226 checks — counted and recorded,
 *                        NEVER a reset (user decision pending)
 *
 * Reporters: 0xA0E70 / 0x9B918 (DSM), 0x87120 (module fault record), 0x7CA24
 * (event log) and 0x6865C (fatal) are stand-ins that write .od_wit cells.
 *
 * STAND-INS (the stock's PWM-feedback chain 0x838B0 → 0x89A8C / 0x8390C
 * needs Timer F capture of the chips' feedback pins, 0x904FA / 0x97648 —
 * not ported): 0x93A2E feedback class, 0x9340C / 0x93420 duty, 0x93394
 * current estimate, 0x933A8 coil resistance (drv_odiag.c §stand-ins).
 */
#ifndef TCU_DRV_ODIAG_H
#define TCU_DRV_ODIAG_H

/* THE TEMPERATURE-SELECTION SHADOW IS READ-ONLY (app/tsel_shadow.c defines
 * TCU_TSEL_SHADOW_TU): it may not include a live header */
#if defined(TCU_TSEL_SHADOW_TU)
#error "the temperature-selection shadow is read-only: it may not include a live header (drv_odiag.h)"
#endif
#if defined(TCU_SM_SHADOW_TU)
#error "the shift-map shadow is read-only: it may not include a live header (drv_odiag.h)"
#endif
#if defined(TCU_FR_SHADOW_TU)
#error "the fault-reaction shadow is read-only: it may not include a live header (drv_odiag.h)"
#endif
#if defined(TCU_GP_SHADOW_TU)
#error "the gear-pair shadow is read-only: it may not include a live header (drv_odiag.h)"
#endif

/* THE FEEDBACK SHADOW IS READ-ONLY (app/fb_shadow.c defines TCU_FB_SHADOW_TU):
 * it may not include a live output header */
#if defined(TCU_FB_SHADOW_TU)
#error "the feedback shadow is read-only: it may not include a live output header (drv_odiag.h)"
#endif

#include <stdbool.h>
#include <stdint.h>

/* r04f (2026-11-18, ~/tcu_sweep/diag_real_feedback_dryrun.md): the REAL
 * feedback chain feeds the diagnosis. DRV_OD_FB_REAL = 1 (the SH application
 * only): after the stand-in (kept as the fallback, drv_od_tick_head), the
 * feedback hook hands the real chain's per-channel class / duty / filtered
 * duty / estimate / resistance in BY VALUE and they overwrite the five cells
 * 0x93A2E / 0x9340C / 0x93420 / 0x93394 / 0x933A8 — the cells the stock's
 * 0x838B0 / 0x89A8C / 0x8390C / 0x83ABC write. 0 = the stand-in only (host
 * tests, RAM app). */
#if !defined(DRV_OD_FB_REAL)
#if defined(__sh__) && !defined(TCU_RAM_APP) && !defined(TCU_LOADER)
#define DRV_OD_FB_REAL 1
#else
#define DRV_OD_FB_REAL 0
#endif
#endif
typedef struct {
    uint8_t  cls[10];       /* → 0x93A2E */
    uint16_t duty[10];      /* → 0x9340C (‰) */
    uint16_t dutyf[10];     /* → 0x93420 */
    uint16_t estm[10];      /* → 0x93394 (0.1 mA) */
    uint16_t res[10];       /* → 0x933A8 (mΩ) */
} drv_od_fb_t;
void drv_od_fb_real(const drv_od_fb_t *fb);

/* the diag job's delay after a cyclic completion (0x92530 → 0x92060(3800):
 * DCNTD02 loaded with 3800 << 8, 1 µs/tick) */
#define DRV_OD_DIAG_DELAY_US 3800u

void drv_od_init(void);

/* reply harvest — called by the bus-A completion handlers (drv_handlers.c)
 * with the burst as sent (tx) and received (rx); ok = frames delivered (the
 * stock's '@' result) */
void drv_od_rx_id(const uint16_t *rx, bool ok);
void drv_od_rx_diag(const uint16_t *rx, bool ok);
void drv_od_rx_cfg(const uint16_t *tx, const uint16_t *rx, bool ok);
void drv_od_rx_cyc(const uint16_t *tx, const uint16_t *rx, bool ok);

/* the diag job's timer (ours: polled from the 1 ms task, see .c) */
void drv_od_diag_poll(uint32_t now_us);

/* the head of the law tick: 0x904FA's status part (0x92A48 + 0x9030C) and
 * the 0x838B0 PWM-feedback stand-ins */
void drv_od_tick_head(void);

/* inside 0x83DB0 (drv_reqlaw.c), in the stock order */
void    drv_od_8422e(void);                 /* pass head: rail filter */
void    drv_od_requests(const uint16_t *req);   /* 0x7B6D8's copies 0x84890 / 0x94D60 */
bool    drv_od_enabled(void);               /* 0x84A22 (0x89F5C from pass 2) */
void    drv_od_89f5c(uint8_t ch);
uint8_t drv_od_7ba76(uint8_t ch);           /* the fault class 1 / 2 */
uint8_t drv_od_chan_class(uint8_t ch);      /* 0x93A42[ch] */
void    drv_od_897ac(uint8_t ch);
void    drv_od_8a788(uint8_t ch);
void    drv_od_ch_tail(uint8_t ch, bool ilock); /* 0x84644 (witness), 0x84392, 0x8493A (witness) */
void    drv_od_pass_end(uint8_t loops_done);    /* 0x84A22 = 1, 0x84A23 = 0xFE; loop-count check */

/* the 0x83DB0 inline 0x226 checks (witness): site 0..5 */
void drv_od_fatal_226(uint8_t site, uint8_t ch);

/* 0x7B9B8's 0x7B222 × 10: the DSM report of groups 1–20 */
void drv_od_report(void);

/* ── witnesses (.od_wit, ours) ─────────────────────────────────────────── */
typedef struct {
    uint8_t  monitor;       /* last 0xA0E70 monitor (fault index) */
    uint8_t  failed;
    uint8_t  tested;
    uint8_t  pend_mon;      /* last 0x9B918 monitor */
    uint32_t calls;
    uint32_t fail_n;        /* reports with failed != 0 */
    uint32_t pend_n;        /* 0x9B918 calls */
} drv_od_grp_t;

typedef struct {
    uint32_t n;             /* would-have-reset count */
    uint16_t code;          /* 0x226 / 0x221 */
    uint8_t  site;          /* see drv_odiag.c OD_SITE_* */
    uint8_t  ch;
    uint32_t t_us;          /* plat_frt_us() at the event */
    uint16_t a;             /* site-specific: value / setpoint */
    uint16_t b;             /* site-specific: complement / current */
    uint32_t site_n[12];    /* per-site counts */
} drv_od_fatal_t;

const drv_od_grp_t *drv_od_group(uint8_t g);    /* g = 1..20 */
const drv_od_fatal_t *drv_od_fatal(void);
uint32_t drv_od_event_n(uint8_t ch);            /* 0x7CA24(0x9E3, ch) events */
uint32_t drv_od_rec87120_n(uint8_t ch);         /* 0x87120 calls with a class */
uint32_t drv_od_diag_sent_n(void);              /* diag jobs queued */
uint32_t drv_od_diag_drop_n(void);              /* diag timer fired, its own previous job still pending */

#if !defined(__sh__)
/* host only: the healthy harvested state of the r03v bench (a test
 * convenience for executables that never drain bus A; the target starts
 * from zero like the stock and harvests within ~20 ms of boot) */
void drv_od_host_seed_healthy(void);
void drv_od_host_seed_select(bool on);          /* default on */
/* the pre-r03y feedback stand-in (live law cells) — test_odiag part 4b */
void drv_od_host_fb_live(bool on);
#endif

#endif /* TCU_DRV_ODIAG_H */

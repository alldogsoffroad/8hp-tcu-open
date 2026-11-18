/* drv_odiag.c — the stock's solenoid OUTPUT DIAGNOSIS (drv_odiag.h for the
 * map). Every function below is ported from the bytes of the pristine image
 * (sh-elf-objdump; the r08_app / r04_boot decompiles checked against them);
 * calibration read through the 0x66xxx pointer tables at index 0x90AB8 = 0.
 * ~/tcu_sweep/output_diag_port.md.
 *
 * Graded in the report: [O] bytes / dump, [I] inference, [S] stand-in.
 */
/* this unit computes values the request law consumes: an output path — the
 * park-sensor headers refuse to compile here */
#define TCU_OUTPUT_PATH 1
#include "bsw/drv/drv_odiag.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "platform.h"
#include <string.h>

#if defined(TCU_RAM_APP)
/* THE RAM APPLICATION does not carry the output diagnosis (its 64 KB
 * limit, as for the group-A law): the r03b stand-ins — fault class 1,
 * channel class 0, no monitor, no diag job */
void drv_od_init(void) {}
void drv_od_rx_id(const uint16_t *rx, bool ok) { (void)rx; (void)ok; }
void drv_od_rx_diag(const uint16_t *rx, bool ok) { (void)rx; (void)ok; }
void drv_od_rx_cfg(const uint16_t *tx, const uint16_t *rx, bool ok) { (void)tx; (void)rx; (void)ok; }
void drv_od_rx_cyc(const uint16_t *tx, const uint16_t *rx, bool ok) { (void)tx; (void)rx; (void)ok; }
void drv_od_diag_poll(uint32_t now_us) { (void)now_us; }
void drv_od_tick_head(void) {}
void drv_od_8422e(void) {}
void drv_od_requests(const uint16_t *req) { (void)req; }
bool drv_od_enabled(void) { return false; }
void drv_od_89f5c(uint8_t ch) { (void)ch; }
uint8_t drv_od_7ba76(uint8_t ch) { (void)ch; return 1u; }
uint8_t drv_od_chan_class(uint8_t ch) { (void)ch; return 0u; }
void drv_od_897ac(uint8_t ch) { (void)ch; }
void drv_od_8a788(uint8_t ch) { (void)ch; }
void drv_od_ch_tail(uint8_t ch, bool ilock) { (void)ch; (void)ilock; }
void drv_od_pass_end(uint8_t loops_done) { (void)loops_done; }
void drv_od_fatal_226(uint8_t site, uint8_t ch) { (void)site; (void)ch; }
void drv_od_report(void) {}
const drv_od_grp_t *drv_od_group(uint8_t g) { (void)g; return 0; }
const drv_od_fatal_t *drv_od_fatal(void) { return 0; }
uint32_t drv_od_event_n(uint8_t ch) { (void)ch; return 0u; }
uint32_t drv_od_rec87120_n(uint8_t ch) { (void)ch; return 0u; }
uint32_t drv_od_diag_sent_n(void) { return 0u; }
uint32_t drv_od_diag_drop_n(void) { return 0u; }
#else

/* ── cells (stock addresses; linker.ld .od_* / .sup_* reservations) ───── */
#define U8A(a)   ((volatile uint8_t  *)(uintptr_t)(a))
#define U16A(a)  ((volatile uint16_t *)(uintptr_t)(a))
#define S16A(a)  ((volatile int16_t  *)(uintptr_t)(a))
#define S32A(a)  ((volatile int32_t  *)(uintptr_t)(a))
#define U8C(a)   (*(volatile uint8_t  *)(uintptr_t)(a))
#define U16C(a)  (*(volatile uint16_t *)(uintptr_t)(a))
#define U32C(a)  (*(volatile uint32_t *)(uintptr_t)(a))

/* harvest / status */
#define C_STAT      U16A(0xFFF9030Cu)   /* [10] chip status word (0x904FA) */
#define C_DIAG_T    U32C(0xFFF9062Cu)   /* TCNTA at the last diag reply (0x92298) */
#define C_DIAGCNT   U8C(0xFFF9096Bu)    /* cyclics since the last diag reply */
#define C_CHIPSUM   U8A(0xFFF9096Cu)    /* [5] 0x931F8 summary | 0x80 ID 0xBD */
#define REC_OK      0x00u               /* record +0 OK bits, +1 class errors, +2 mismatches */
#define REC_ERR     0x01u
#define REC_MIS     0x02u
#define REC_DRV     0x06u               /* bit 1/3 setpoint > 0 confirmed (cyc), bit 0/2 copied (diag) */
#define REC_MRB     0x08u               /* 0x28 read: the mode register as read back */
#define REC_DG0     0x12u               /* 0x18 read: channel 2c diagnosis byte */
#define REC_DG1     0x1Au               /* 0x1E read: channel 2c+1 diagnosis byte */

/* fault words */
#define C_FW        U16A(0xFFF93004u)   /* [10] fault word (value) */
#define C_FM        U16A(0xFFF93018u)   /* [10] fault word (tested mask) */
#define C_RFST      S32A(0xFFF930F8u)   /* [2] rail filter state ×10000 */
#define C_RF        S16A(0xFFF93100u)   /* [2] filtered rail mV */
#define C_DSMW      U16A(0xFFF93104u)   /* [10] estimate copy for the DSM table */

/* the PWM-feedback chain's outputs ([S] here) and the bounds */
#define C_EST       U16A(0xFFF93394u)   /* [10] current estimate (0x8390C) */
#define C_RES       U16A(0xFFF933A8u)   /* [10] coil resistance mΩ */
#define C_PREQ      S16A(0xFFF933F8u)   /* [10] 0x897AC: request last pass */
#define C_DUTY      U16A(0xFFF9340Cu)   /* [10] measured duty ‰ (0x89A8C) */
#define C_DUTYF     U16A(0xFFF93420u)   /* [10] its filter (0x89EDA) */
#define C_PI        U16A(0xFFF93484u)   /* [10] 0x8A788: current last pass */
#define C_LO        U16A(0xFFF93498u)   /* [10] lower integrator */
#define C_HI        U16A(0xFFF934ACu)   /* [10] upper integrator */
#define C_E7B4      U16A(0xFFF934E8u)   /* [10] 0x8E7B4 duty estimate */
#define C_PDI       S16A(0xFFF934FCu)   /* [10] current step last pass */
#define C_UB        U16A(0xFFF93510u)   /* [10] upper bound */
#define C_LB        U16A(0xFFF93524u)   /* [10] lower bound */
#define C_93374     U16C(0xFFF93374u)   /* [S] 0 (writer not ported) */
#define C_93378     U16C(0xFFF93378u)   /* [S] 0 (writer not ported) */

#define C_UVF2      U16C(0xFFF9397Eu)   /* 0x84392's undervoltage flags */
#define C_LATCH     U8A(0xFFF93980u)    /* [10] 0x84644 two-pass latch */
#define C_PSEQ      U8A(0xFFF9398Au)    /* [10] 0x84644 seq2 last pass */
#define C_CLS       U8A(0xFFF93994u)    /* [10] 0x89F5C report class */
#define C_SUPST     U8A(0xFFF9399Eu)    /* [10] 0x8A6D8 supply state 1..4 */
#define C_939A8     U16C(0xFFF939A8u)
#define C_939BA     U8A(0xFFF939BAu)    /* [..] 0x86414 counters */

#define C_93A04     U16C(0xFFF93A04u)   /* [S] 0 (writer not ported) */
#define C_CHG       U16C(0xFFF93A14u)   /* 0x897AC: request changed */
#define C_TPEND     U16C(0xFFF93A16u)   /* 0x897AC: test pending */
#define C_PMODE     U8A(0xFFF93A18u)    /* [10] 0x897AC: mode last pass */
#define C_FB        U8A(0xFFF93A2Eu)    /* [10] feedback class 0x10 / 0x20 / 0x21 / 0x31 ([S]) */
#define C_OST       U8A(0xFFF93A38u)    /* [10] 0x8E8E4 state */
#define C_CCLS      U8A(0xFFF93A42u)    /* [10] channel class → the law */
#define C_OLET      U8A(0xFFF93A4Cu)    /* [10] 0x8E8E4 letter */
#define C_OLET2     U8A(0xFFF93A56u)    /* [10] 0x8E8E4 result letter */
#define C_93A60     U16C(0xFFF93A60u)
#define C_INCU      U16C(0xFFF93A62u)   /* 0x8A788 lower-integrator hold bits */
#define C_INCO      U16C(0xFFF93A64u)   /* 0x8A788 upper-integrator hold bits */
#define C_STATE     U8A(0xFFF93B0Cu)    /* [10] 0x897AC channel state */
#define C_SAT       U8A(0xFFF93B16u)    /* [10] 0x8E7B4 saturation state */
#define C_93B20     (*(volatile int16_t *)(uintptr_t)0xFFF93B20u)

#define C_9410F     U8C(0xFFF9410Fu)    /* 0x8422E seeded */
#define C_94141     U8C(0xFFF94141u)    /* 0x897AC: all requests 0 */

/* the 5 ms countdown service's range (0x7C7E4: words [0x94734, 0x94786),
 * bytes [0x94788, 0x94835)) — these count down by themselves */
#define T_HI        U16A(0xFFF9473Cu)   /* [10] upper hold (words) */
#define T_LO        U16A(0xFFF94750u)   /* [10] lower hold (words) */
#define T_GRACE     U8A(0xFFF9478Eu)    /* [10] 0x84644 grace */
#define T_94798     U8A(0xFFF94798u)
#define T_947A2     U8A(0xFFF947A2u)
#define T_947CC     U8A(0xFFF947CCu)
#define T_947D6     U8A(0xFFF947D6u)
#define T_947E0     U8A(0xFFF947E0u)
#define T_947EA     U8A(0xFFF947EAu)
#define T_947F4     U8A(0xFFF947F4u)
#define T_947FE     U8A(0xFFF947FEu)
#define T_94826     U8C(0xFFF94826u)
#define T_94827     U8C(0xFFF94827u)

#define C_94930     U16A(0xFFF94930u)   /* [10] latched fault bits */
#define C_94944     U16A(0xFFF94944u)   /* [10] tested bits */

/* complement mirror (0x84392) and the 0x7B6D8 copies */
#define C_IC        U16A(0xFFF94D4Cu)   /* [10] ~0x94ED4 */
#define C_REQC      U16A(0xFFF94D60u)   /* [10] ~0x94EE8 (0x7B6D8) */
#define C_I2C       U16A(0xFFF94DD0u)   /* [10] ~0x94F58 (0x89718) */
#define C_ACTC      U8A(0xFFF94E18u)    /* [10] ~0x94FA0 */
#define C_MODEC     U8A(0xFFF94E50u)    /* [10] ~0x94FD8 */
#define C_RQSLOT    U16A(0xFFF84890u)   /* [10] 0x7B6D8 slot array (the request) */
#define C_848CC     U16C(0xFFF848CCu)
#define C_EN        U8C(0xFFF84A22u)    /* 0x89F5C enable (from pass 2) */
#define C_EN_C      U8C(0xFFF84A23u)
#define C_LASTCH    U8C(0xFFF84A2Du)    /* 0x897AC: last channel (init 0xFF) */
#define C_90826     U8A(0xFFF90826u)    /* [S] 0 (writer not ported) */

/* ── calibration (flash 0x19xxxx, [O] read through the 0x66xxx tables) ── */
static const uint8_t  CAL_DIAGCFG[10] = { 2, 1, 2, 1, 1, 2, 2, 1, 1, 1 }; /* 0x1916C6 (0x66A18) */
#define CAL_TYPE        1u          /* 0x1916BA[10], all 01 (0x66A08) */
#define CAL_RELOAD_798  0u          /* 0x1919B2 (0x667F0) */
#define CAL_RELOAD_7A2  10u         /* 0x1919B3 (0x667F4) */
#define CAL_CNT_MASK    0x001Fu     /* 0x191556 (0x667EC) */
#define CAL_ALLOW       0xCFFFu     /* 0x19054C[10], all CFFF (0x667E8) */
static const uint16_t CAL_M1[10] = { 0xCF5Bu, 0xCFFFu, 0xCF5Bu, 0xCFFFu, 0xCFFFu,
                                     0u, 0u, 0xCFFFu, 0xCFFFu, 0xCFFFu };  /* 0x190524 (0x667E0) */
static const uint16_t CAL_M2[10] = { 0xCF5Bu, 0xCF5Bu, 0xCF5Bu, 0xCF5Bu, 0xCF5Bu,
                                     0u, 0u, 0xCC5Bu, 0xCC5Bu, 0xCC5Bu };  /* 0x190538 (0x667E4) */
#define CAL_CH_MASK     0x00FFu     /* 0x19154C (0x667C0) */
#define CAL_CHIP_EN     0x1Fu       /* 0x1918C1 (0x8FD94) */
/* 0x8A6D8 */
#define CAL_SUP3_LO     6500u       /* 0x19155E (0x66804) */
#define CAL_SUP3_HI     19200u      /* 0x19155C (0x66800) */
#define CAL_SUP4_LO     8400u       /* 0x19155A (0x667FC) */
#define CAL_SUP4_HI     19200u      /* 0x191558 (0x667F8) */
/* 0x8E8E4 / 0x8ECF0 */
#define CAL_OD_RAILMIN  8000u       /* 0x1916DA (0x66A2C) */
#define CAL_OD_7E0      20u         /* 0x1919F4 (0x66A28) */
#define CAL_OD_7D6      4u          /* 0x1916D0[10], all 4 (0x66A24) */
static const uint16_t CAL_OD_CURVE0[7] = { 3u, 8000u, 10000u, 12000u, 6u, 4u, 2u }; /* 0x1912A8 */
static const uint16_t CAL_OD_CURVE1[7] = { 3u, 8000u, 10000u, 12000u, 4u, 4u, 2u }; /* 0x1912B6 */
/* 0x8E7B4 */
#define CAL_SAT_DMAX    7000u       /* 0x1913E4[10], all 7000 (0x66BA4) */
#define CAL_SAT_DFILT   800u        /* 0x19175A (0x66B9C) */
static const uint16_t CAL_SAT_RAIL[10] = { 9000u, 9000u, 9000u, 9000u, 9000u,
                                           9000u, 9000u, 11500u, 9000u, 9000u }; /* 0x1913F8 (0x66BA8) */
#define CAL_SAT_HYST    200u        /* 0x19175C (0x66BA0) */
#define CAL_SAT_RWIN    1000u       /* 0x19175E (0x66BAC) */
/* 0x897AC */
#define CAL_CHG_TOL     100u        /* 0x191742 (0x66B8C) */
#define CAL_CHG_MASK    0x001Fu     /* 0x191744 (0x66B90) */
static const uint8_t CAL_HOLD_826[10] = { 6, 6, 6, 6, 6, 6, 6, 12, 6, 6 };       /* 0x191750 (0x66B98) */
static const uint8_t CAL_HOLD_7EA[10] = { 10, 10, 10, 10, 10, 10, 10, 16, 10, 10 }; /* 0x191746 (0x66B94) */
/* 0x8A788 / 0x8ACFE */
static const uint8_t CAL_BND_B2[10] = { 3, 3, 3, 3, 3, 0, 0, 0, 0, 0 };   /* 0x1917B8 (0x66BE0) */
static const uint8_t CAL_BND_B3[10] = { 1, 2, 1, 2, 2, 0, 0, 0, 0, 0 };   /* 0x1917C2 (0x66BE4) */
#define CAL_BND_LOOFS   1200        /* 0x1917B4 (0x66BD8) */
#define CAL_BND_HIOFS   1700        /* 0x1917B6 (0x66BDC) */
static const int16_t CAL_BND_HOLD_LO[19] = { 9, -1500, -1000, -500, -200, 0, 200, 500, 1000, 1500,
                                             20, 16, 10, 0, 0, 0, 16, 20, 20 };             /* 0x191806 */
static const int16_t CAL_BND_K_LO[19]    = { 9, -1500, -1000, -500, -200, 0, 200, 500, 1000, 1500,
                                             2500, 2500, 2500, 2500, 1500, 1500, 1500, 1500, 1500 }; /* 0x191786 */
static const int16_t CAL_BND_HOLD_HI[19] = { 9, -1500, -1000, -500, -200, 0, 200, 500, 1000, 1500,
                                             20, 20, 16, 0, 0, 0, 10, 16, 20 };             /* 0x1917E0 */
static const int16_t CAL_BND_K_HI[19]    = { 9, -1500, -1000, -500, -200, 0, 200, 500, 1000, 1500,
                                             3000, 3000, 3000, 3000, 2000, 2000, 2000, 2000, 2000 }; /* 0x191760 */
#define CAL_BND_DIMAX   1500u       /* 0x191448[10], all 1500 (0x66BC8) */
#define CAL_BND_LOMIN   500u        /* 0x191434[10], all 500 (0x66BC4) */
#define CAL_BND_HIMAX   8500u       /* 0x191420[10], all 8500 (0x66BC0) */
#define CAL_BND_ILIM    4000u       /* 0x1917B0 (0x66BD0) */
#define CAL_BND_7FE     6u          /* 0x1917D6[10], all 6 (0x66BEC) */
#define CAL_ACF_827     10u         /* 0x191A08 (0x66BF8) */
#define CAL_ACF_A       1000        /* 0x1917AE (0x66BCC) */
#define CAL_ACF_TEMP    0           /* 0x1917B2 (0x66BD4) */
#define CAL_ACF_ONE     500         /* 0x1917AC (0x66BB8) */
#define CAL_ACF_7F4     6u          /* 0x1917CC[10], all 6 (0x66BE8) */
#define CAL_ACF_CH      100         /* 0x19140C[10], all 100 (0x66BBC) */
/* 0x84392 / 0x89718 (the law's own cal) */
#define CAL_UV_MV       7400u       /* 0x191554 (0x667DC) */
#define CAL_UV_THR      4000u       /* 0x191548 (0x667B8) */
#define CAL_FULL15      0x03FFu     /* 0x191550 (0x667CC) */
#define CAL_FULL14      0x03FFu     /* 0x19154E (0x667C4) */
#define CAL_UV_REL      500u        /* 0x191552 (0x667D8) */
#define CAL_UV_FLAGS    0x0001u     /* 0x190510[10], all 0x0001 (0x667C8) */
#define CAL_IMIN        500u        /* 0x19154A (0x667BC) */
/* 0x84644 */
#define CAL_GRACE_SEQ   6u          /* 0x1919B1 (0x667D4) */
#define CAL_GRACE_FLT   10u         /* 0x1919B0 (0x667D0) */
/* 0x8422E */
static const int16_t CAL_RFK[2] = { 500, 500 };                           /* 0x19050C (0x667B4) */
/* 0x7B222: (mask, code) pairs, table 0x45C54 (12 entries, the last is the default) */
static const uint16_t TBL_45C54[12][2] = {
    { 0x0040u, 0x43u }, { 0x0010u, 0x06u }, { 0x0008u, 0x03u }, { 0x0080u, 0xC4u },
    { 0x0020u, 0xC2u }, { 0x0004u, 0x82u }, { 0x0400u, 0x0Cu }, { 0x0800u, 0x0Du },
    { 0x0100u, 0x08u }, { 0x0200u, 0x07u }, { 0x1000u, 0x06u }, { 0x0000u, 0x01u },
};

/* ── STAND-INS [S] for the PWM-feedback chain (0x838B0, list entry 0x7CBA8,
 * before the law): coil resistance = the driving dump's 0x933A8 (mΩ) */
static const uint16_t SI_RES[10] = { 0x1653u, 0x17FAu, 0x17FAu, 0x17FAu, 0x17FAu,
                                     0x1653u, 0x1653u, 0x2E01u, 0x16B7u, 0x16B7u };

/* ── witnesses (.od_wit, ours: free low RAM, cleared by drv_od_init) ──── */
#if defined(__sh__)
#define OD_WIT __attribute__((section(".od_wit")))
#else
#define OD_WIT
#endif
/* the output the last DIAGNOSTIC pass of the law commanded (mode 0x94FD8,
 * current 0x94ED4), for the feedback stand-in (od_838b0_standin; 2026-11-18,
 * ~/tcu_sweep/ch7_fault_bit4_decode.md). Defined FIRST: GCC lays the
 * .od_wit statics out in reverse order, so these go to the END of .od_wit
 * and no existing witness address moves */
static uint8_t  s_fb_mode[10] OD_WIT;
static uint16_t s_fb_i[10] OD_WIT;
static drv_od_grp_t    s_grp[21] OD_WIT;          /* [1..20] */
static drv_od_fatal_t  s_fatal OD_WIT;
static uint32_t        s_event_n[10] OD_WIT;
static uint32_t        s_rec87_n[10] OD_WIT;
static uint8_t         s_rec87_last[10][2] OD_WIT; /* flags, mask low byte */
static uint32_t        s_diag_sent_n OD_WIT;
static uint32_t        s_diag_drop_n OD_WIT;
static volatile uint32_t s_cyc_t_us OD_WIT;        /* the last cyclic completion */
static volatile uint8_t  s_diag_armed OD_WIT;      /* the 0x92060 one-shot is running */

/* site numbers of the would-be resets */
enum {
    OD_SITE_LAW_I = 0,      /* 0x83DB0: I vs its complement shadow (0x226) */
    OD_SITE_LAW_MODE,       /* 0x83DB0: mode vs ~mode shadow (0x226) */
    OD_SITE_LAW_SEQ2,       /* 0x83DB0: two 0x85DC4 reads differ (0x226) */
    OD_SITE_LAW_LOOP,       /* 0x83DB0: loop counter vs complement (0x226) */
    OD_SITE_LAW_END,        /* 0x83DB0: 10 loops / 0x84A22 pair (0x226) */
    OD_SITE_LAW_RSV,
    OD_SITE_84644,          /* unintended output, two passes (0x226) */
    OD_SITE_8493A_REQ,      /* 0x94EE8 / 0x94D60 (0x221) — not checkable, see .c */
    OD_SITE_8493A_I,        /* 0x94ED4 / 0x94D4C (0x221) */
    OD_SITE_8493A_ACT,      /* 0x94FA0 / 0x94E18 (0x221) */
    OD_SITE_8493A_I2,       /* 0x94F58 / 0x94DD0 (0x221) */
    OD_SITE_8493A_MODE,     /* 0x94FD8 / 0x94E50 (0x221) */
};

static void od_fatal(uint16_t code, uint8_t site, uint8_t ch, uint16_t a, uint16_t b)
{
    s_fatal.n++;
    s_fatal.code = code;
    s_fatal.site = site;
    s_fatal.ch = ch;
    s_fatal.t_us = plat_frt_us();
    s_fatal.a = a;
    s_fatal.b = b;
    if (site < 12u) {
        s_fatal.site_n[site]++;
    }
}

void drv_od_fatal_226(uint8_t site, uint8_t ch)
{
    od_fatal(0x226u, site, ch, 0u, 0u);
}

const drv_od_grp_t *drv_od_group(uint8_t g) { return (g >= 1u && g <= 20u) ? &s_grp[g] : 0; }
const drv_od_fatal_t *drv_od_fatal(void) { return &s_fatal; }
uint32_t drv_od_event_n(uint8_t ch) { return ch < 10u ? s_event_n[ch] : 0u; }
uint32_t drv_od_rec87120_n(uint8_t ch) { return ch < 10u ? s_rec87_n[ch] : 0u; }
uint32_t drv_od_diag_sent_n(void) { return s_diag_sent_n; }
uint32_t drv_od_diag_drop_n(void) { return s_diag_drop_n; }

/* ── the reporters (stand-ins) ─────────────────────────────────────────── */
/* 0xA0E70(group, monitor, failed, tested) → the DSM (not ported) */
static void rep_a0e70(uint8_t g, uint8_t mon, uint8_t failed, uint8_t tested)
{
    if (g >= 1u && g <= 20u) {
        s_grp[g].monitor = mon;
        s_grp[g].failed = failed;
        s_grp[g].tested = tested;
        s_grp[g].calls++;
        if (failed != 0u) {
            s_grp[g].fail_n++;
        }
    }
}

/* 0x9B918(group, monitor) → the DSM's second entry (not ported) */
static void rep_9b918(uint8_t g, uint8_t mon)
{
    if (g >= 1u && g <= 20u) {
        s_grp[g].pend_mon = mon;
        s_grp[g].pend_n++;
    }
}

/* 0x87120(module 0, ch, class 0, flags, mask): the module fault record (its
 * priority/latch internals and their 0x226 complement checks not ported) */
static void rep_87120(uint8_t ch, uint8_t flags, uint16_t mask)
{
    if (ch < 10u) {
        s_rec87_last[ch][0] = flags;
        s_rec87_last[ch][1] = (uint8_t)mask;
        if ((flags & 7u) != 0u) {
            s_rec87_n[ch]++;
        }
    }
}

/* 0x7CA24(0x9E3, ch, mask, value): the event log (stand-in: a count) */
static void rep_7ca24(uint8_t ch, uint16_t mask, uint16_t value)
{
    if ((mask & value) != 0u && ch < 10u) {
        s_event_n[ch]++;
    }
}

/* ── curves (local copies of 0x8ED60 / 0x8D42C, as drv_temp.c / drv_groupa.c) */
static uint16_t od_curve_u16(const uint16_t *tbl, uint16_t x)   /* 0x8ED20 → 0x8ED60 */
{
    const uint16_t n = tbl[0];
    const uint16_t *xs = &tbl[1];
    const uint16_t *ys = &tbl[1u + n];
    float f;
    if ((int32_t)x <= (int32_t)xs[0]) {
        f = (float)ys[0];
    } else if ((int32_t)x >= (int32_t)xs[n - 1u]) {
        f = (float)ys[n - 1u];
    } else {
        uint32_t j = 1u;
        while ((int32_t)x >= (int32_t)xs[j]) {
            j++;
        }
        {
            const int32_t dx = (int32_t)x - (int32_t)xs[j - 1u];
            const int32_t range = (int32_t)xs[j] - (int32_t)xs[j - 1u];
            if (dx <= 0) {
                f = (float)ys[j - 1u];
            } else {
                const float y0 = (float)ys[j - 1u];
                f = (((float)ys[j] - y0) * (float)dx) / (float)range + y0;
            }
        }
    }
    return (uint16_t)(int32_t)(f + 0.5f);
}

static int16_t od_curve_s16(const int16_t *tbl, int16_t x)      /* 0x886AC → 0x8D42C */
{
    const int16_t n = tbl[0];
    const int16_t *xs = &tbl[1];
    const int16_t *ys = &tbl[1 + n];
    float f;
    if ((int32_t)x <= (int32_t)xs[0]) {
        f = (float)ys[0];
    } else if ((int32_t)x >= (int32_t)xs[n - 1]) {
        f = (float)ys[n - 1];
    } else {
        int32_t j = 1;
        while ((int32_t)x >= (int32_t)xs[j]) {
            j++;
        }
        {
            const int32_t dx = (int32_t)x - (int32_t)xs[j - 1];
            const int32_t range = (int32_t)xs[j] - (int32_t)xs[j - 1];
            if (dx <= 0) {
                f = (float)ys[j - 1];
            } else {
                const float y0 = (float)ys[j - 1];
                f = (((float)ys[j] - y0) * (float)dx) / (float)range + y0;
            }
        }
    }
    return (int16_t)(int32_t)(f < 0.0f ? f - 0.5f : f + 0.5f);
}

/* 0x88750: saturating a − b (u16) */
static uint16_t od_satsub(uint16_t a, uint16_t b)
{
    const int32_t d = (int32_t)a - (int32_t)b;
    return (uint16_t)(d < 0 ? 0 : d);
}

static uint16_t chbit(uint8_t ch) { return (uint16_t)(1u << ch); }   /* shld: 1 << 0 = 1 */

static bool od_chip_enabled(uint8_t ch)                              /* 0x8FD94 */
{
    const uint8_t chip = (uint8_t)(ch >> 1);
    return chip < 5u && ((CAL_CHIP_EN >> chip) & 1u) != 0u;
}

/* ════════════════════════════════════════════════════════════════════════
 * 1. THE REPLY HARVEST (completion handlers; ISR context like the stock's)
 * ════════════════════════════════════════════════════════════════════════ */
/* 0x9218C: an error bit on all five records */
static void od_9218c(uint8_t m)
{
    for (uint8_t c = 0u; c < 5u; c++) {
        CELL_REC_U8(c, REC_ERR) = (uint8_t)(CELL_REC_U8(c, REC_ERR) | m);
    }
}

/* 0x921E0: the ID job (0600 ×5, 0000 ×5) */
void drv_od_rx_id(const uint16_t *rx, bool ok)
{
    if (!ok) {
        od_9218c(0x01u);
        return;
    }
    for (uint8_t c = 0u; c < 5u; c++) {
        if ((rx[c] & 0xFF00u) == 0xA200u && (rx[5u + c] & 0xFF00u) == 0xA200u) {
            CELL_REC_U8(c, REC_ID_HI) = (uint8_t)rx[c];          /* 0xFFF9110D */
            CELL_REC_U8(c, REC_ID_LO) = (uint8_t)rx[5u + c];     /* 0xFFF9110C */
            CELL_REC_U8(c, REC_OK) = (uint8_t)(CELL_REC_U8(c, REC_OK) | 0x01u);
        } else {
            CELL_REC_U8(c, REC_ERR) = (uint8_t)(CELL_REC_U8(c, REC_ERR) | 0x01u);
        }
    }
}

/* 0x92298: the diag job (1800 ×5, 1E00 ×5, 2800 ×5) */
void drv_od_rx_diag(const uint16_t *rx, bool ok)
{
    C_DIAG_T = plat_frt_us();                   /* 0x9062C = TCNTA (1 µs) */
    C_DIAGCNT = 0u;
    if (!ok) {
        od_9218c(0x02u);
        return;
    }
    for (uint8_t c = 0u; c < 5u; c++) {
        bool cls = true;
        for (uint8_t r = 0u; r < 3u; r++) {
            if ((rx[c + 5u * r] & 0xFF00u) != 0xA200u) {
                cls = false;
            }
        }
        if (cls) {
            uint8_t d = CELL_REC_U8(c, REC_DRV);
            CELL_REC_U8(c, REC_DG0) = (uint8_t)rx[c];
            CELL_REC_U8(c, REC_DG1) = (uint8_t)rx[5u + c];
            CELL_REC_U8(c, REC_MRB) = (uint8_t)rx[10u + c];
            d = (uint8_t)((d & 0xFEu) | ((d >> 1) & 1u));       /* bld #1 → bst #0 */
            d = (uint8_t)((d & 0xFBu) | (((d >> 3) & 1u) << 2)); /* bld #3 → bst #2 */
            CELL_REC_U8(c, REC_DRV) = d;
            CELL_REC_U8(c, REC_OK) = (uint8_t)(CELL_REC_U8(c, REC_OK) | 0x42u);
        } else {
            CELL_REC_U8(c, REC_ERR) = (uint8_t)(CELL_REC_U8(c, REC_ERR) | 0x42u);
        }
    }
}

/* 0x923BC: the config job's readbacks */
void drv_od_rx_cfg(const uint16_t *tx, const uint16_t *rx, bool ok)
{
    if (!ok) {
        od_9218c(0x04u);
        return;
    }
    for (uint8_t c = 0u; c < 5u; c++) {
        bool bad = false;
        for (uint8_t r = 0u; r < 8u; r++) {
            if ((rx[c + 5u * r] & 0xFF00u) != 0xA200u) {
                bad = true;
            }
        }
        if (bad) {
            CELL_REC_U8(c, REC_ERR) = (uint8_t)(CELL_REC_U8(c, REC_ERR) | 0x04u);
        } else {
            bad = (tx[15u + c] & 0x3Fu) != (rx[20u + c] & 0x3Fu) ||       /* 0x88 → 0x28 */
                  (uint8_t)rx[25u + c] != (uint8_t)tx[5u + c] ||           /* 0x90 → 0x30 */
                  (uint8_t)rx[30u + c] != (uint8_t)tx[0u + c] ||           /* 0x8E → 0x2E */
                  (uint8_t)rx[35u + c] != (uint8_t)tx[10u + c];            /* 0x96 → 0x36 */
        }
        if (bad) {
            CELL_REC_U8(c, REC_MIS) = (uint8_t)(CELL_REC_U8(c, REC_MIS) | 0x04u);
        } else {
            CELL_REC_U8(c, REC_OK) = (uint8_t)(CELL_REC_U8(c, REC_OK) | 0x04u);
        }
    }
}

/* 0x92530: the cyclic's readbacks (and the 3.8 ms diag one-shot) */
void drv_od_rx_cyc(const uint16_t *tx, const uint16_t *rx, bool ok)
{
    /* 0x92060(3800): DCNTD02 one-shot → 0x92078 → 0x9225C (ours: polled) */
    s_cyc_t_us = plat_frt_us();
    s_diag_armed = 1u;
    if (C_DIAGCNT > 1u) {                       /* cmp/hi #1 */
        od_9218c(0x40u);
    } else {
        C_DIAGCNT = (uint8_t)(C_DIAGCNT + 1u);
    }
    if (!ok) {
        od_9218c(0x10u);
        return;
    }
    for (uint8_t c = 0u; c < 5u; c++) {
        bool bad = false;
        for (uint8_t r = 0u; r < 9u; r++) {
            if ((rx[c + 5u * r] & 0xF800u) != 0xA000u) {
                bad = true;
            }
        }
        if (bad) {
            CELL_REC_U8(c, REC_ERR) = (uint8_t)(CELL_REC_U8(c, REC_ERR) | 0x10u);
        } else {
            const uint16_t sp0 = (uint16_t)(tx[10u + c] & 0x7FFu);
            const uint16_t sp1 = (uint16_t)(tx[20u + c] & 0x7FFu);
            uint8_t d = CELL_REC_U8(c, REC_DRV);
            bad = (uint8_t)rx[25u + c] != (uint8_t)tx[5u + c] ||           /* 0xA0 → 0x60 */
                  (rx[30u + c] & 0x7FFu) != sp0;                           /* 0xE8 → 0xC0 */
            if (!bad) {
                d = (uint8_t)((d & 0xFDu) | (sp0 > 0u ? 0x02u : 0u));      /* bst #1 */
            }
            bad = bad || (uint8_t)rx[35u + c] != (uint8_t)tx[15u + c] ||   /* 0xA6 → 0x66 */
                  (rx[40u + c] & 0x7FFu) != sp1;                           /* 0xF0 → 0xD8 */
            if (!bad) {
                d = (uint8_t)((d & 0xF7u) | (sp1 > 0u ? 0x08u : 0u));      /* bst #3 */
            }
            CELL_REC_U8(c, REC_DRV) = d;
        }
        if (bad) {
            CELL_REC_U8(c, REC_MIS) = (uint8_t)(CELL_REC_U8(c, REC_MIS) | 0x10u);
        } else {
            CELL_REC_U8(c, REC_OK) = (uint8_t)(CELL_REC_U8(c, REC_OK) | 0x10u);
        }
    }
}

/* the 0x92078 ISR's job (0x9225C: check-free, stage 0x40CEE, queue; no
 * retry — the next cyclic re-arms). DEVIATION (ours): the stock's Timer D
 * one-shot DCNTD02 is not ported; the 1 ms task polls the delay, so the
 * diag job is QUEUED 3.8–4.8 ms after the cyclic, not 3.8 ms. 2026-11-18:
 * it waits in the bus-A ring behind any job on the wire (never dropped for
 * a busy channel); "drop" now counts only a diag whose own previous
 * instance was still pending (the 0x93B8E busy branch). */
void drv_od_diag_poll(uint32_t now_us)
{
    if (s_diag_armed == 0u || (uint32_t)(now_us - s_cyc_t_us) < DRV_OD_DIAG_DELAY_US) {
        return;
    }
    s_diag_armed = 0u;
    if (drv_diag_kick()) {
        s_diag_sent_n++;
    } else {
        s_diag_drop_n++;
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * 2. CHIP STATUS (0x904FA's part, list entry 0x8EE12 — before the law)
 * ════════════════════════════════════════════════════════════════════════ */
/* 0x931F8(rec, &sum, mask) */
static void od_931f8(uint8_t c, uint8_t mask)
{
    const uint8_t ok = CELL_REC_U8(c, REC_OK);
    const uint8_t er = CELL_REC_U8(c, REC_ERR);
    const uint8_t mi = CELL_REC_U8(c, REC_MIS);
    uint8_t s = 0u;
    if (er != 0u) {
        s = 2u;
    }
    if ((mi & (uint8_t)~er) != 0u) {
        s = (uint8_t)(s | 4u);
    }
    if ((ok & (uint8_t)~er & (uint8_t)~mi) != 0u) {
        s = (uint8_t)(s | 1u);
    }
    if (mask == ((er | mi | ok) & mask)) {
        CELL_REC_U8(c, REC_ERR) = 0u;
        CELL_REC_U8(c, REC_MIS) = 0u;
        CELL_REC_U8(c, REC_OK) = 0u;
    } else {
        s = (uint8_t)(s | (C_CHIPSUM[c] & 7u));
    }
    C_CHIPSUM[c] = s;       /* 0x92A04 then ORs 0x80 (below) */
}

/* the summary mask: stock 0x77 (0x92A04). DEVIATION [S]: bit 5 (0x20) comes
 * from the readback job handler in 0x92900..0x929A4 (records +0x0E / +0x16,
 * the Timer-E-synchronised reads of 0x90844) — not ported, so the round
 * would never complete; 0x57 keeps the stock's per-round recomputation. */
#define OD_SUM_MASK 0x57u

/* the stock's lock 0x9665C(level << 4): SR.I raised to at least level,
 * restored by 0x96674 — it masks the bus-A completion ISR (IPR 4) that ORs
 * into the same record bytes */
static uint32_t od_lock(uint32_t level)
{
    const uint32_t lk = plat_sr_i();
    if (lk < level) {
        plat_sr_set(level);
    }
    return lk;
}

static void od_904fa_status(void)
{
    uint32_t lk = od_lock(6u);                          /* 0x9665C(0x60) */
    for (uint8_t c = 0u; c < 5u; c++) {                 /* 0x92A48 → 0x92A04 */
        od_931f8(c, OD_SUM_MASK);
        if (CELL_REC_U8(c, REC_ID_LO) == 0xBDu) {
            C_CHIPSUM[c] = (uint8_t)(C_CHIPSUM[c] | 0x80u);
        }
    }
    plat_sr_set(lk);                                    /* 0x96674 */
    lk = od_lock(4u);                                   /* 0x9665C(0x40) */
    for (uint8_t ch = 0u; ch < 10u; ch++) {             /* 0x904FA's channel loop */
        const uint8_t c = (uint8_t)(ch >> 1);
        const uint8_t mrb = CELL_REC_U8(c, REC_MRB);
        const uint8_t drv = CELL_REC_U8(c, REC_DRV);
        uint16_t w = (uint16_t)((uint16_t)C_CHIPSUM[c] << 8);
        if ((mrb & 0x80u) != 0u) {
            w = (uint16_t)(w | 0x4000u);
        }
        if ((mrb & 0x40u) != 0u) {
            w = (uint16_t)(w | 0x2000u);
        }
        if ((ch & 1u) == 0u) {
            w = (uint16_t)(w | CELL_REC_U8(c, REC_DG0));
            if ((mrb & 0x01u) == 0u && (drv & 0x01u) != 0u) {
                w = (uint16_t)((w | 0x1000u) & (0xFFu | 0xFF00u));   /* cal 0x1918C2 = 0xFF */
            }
        } else {
            w = (uint16_t)(w | CELL_REC_U8(c, REC_DG1));
            if ((mrb & 0x04u) == 0u && (drv & 0x04u) != 0u) {
                w = (uint16_t)((w | 0x1000u) & (0xFFu | 0xFF00u));
            }
        }
        C_STAT[ch] = w;
        /* 0x90258 (the readback-job current × gain 0x84A30) not ported */
    }
    plat_sr_set(lk);                                    /* 0x96674 */
}

/* 0x838B0 → 0x89A8C / 0x8390C STAND-INS [S] (the feedback chain is not
 * ported): from the law's previous DIAGNOSTIC pass, as 0x838B0 sees it.
 * 2026-11-18 (ch7_fault_bit4_decode.md): NOT from the live law cells. In the
 * stock 0x838B0 (5 ms / 10 ms list, right before 0x83DB0: 0x7CBA8 → 0x7CC1E)
 * measures the physical output, which the previous law pass commanded — the
 * same pass whose 0x897AC set the state 0x93B0C that 0x89F5C reads. r03x's 10 ms
 * rebuild (drv_rq_law_between, task_10ms; removed 2026-11-18) ran the law
 * WITHOUT 0x897AC one ms before the tick; reading its mode here paired a switched-off feedback
 * ('!') with a still-driven state (0x30) → 0x89F5C bit 2 (P1D79 on the park
 * release), latched for good once the state left the driven range (r03x).
 * The snapshot keeps the stock's pairing: state and feedback from one pass —
 *   0x93A2E = 0x10 (a PWM measured) while the channel is driven (mode 3 /
 *             4 with a current), else 0x21 (static low): the driving dump's
 *             10 ×7 (driven) / 21 ×3 (off) pattern;
 *   0x9340C = 0x93420 = I·R / V in ‰ (≤ 1000) while driven, else 0;
 *   0x93394 = the commanded current 0x94ED4 (0x8390C's estimate);
 *   0x933A8 = the dump's coil resistances. */
#if !defined(__sh__)
/* host only (test_odiag part 4b): the PRE-r03y stand-in, reading the LIVE
 * law cells instead of the end-of-pass snapshot — with one law pass per
 * output tick (2026-11-18) the two must agree, which proves the r03x ch 7
 * artefact's root cause (the 10 ms rebuild) gone, not masked */
static bool s_fb_live;
void drv_od_host_fb_live(bool on) { s_fb_live = on; }
#define FB_MODE(ch) (s_fb_live ? CELL_RQ_MODE[ch] : s_fb_mode[ch])
#define FB_I(ch)    (s_fb_live ? CELL_RQ_I[ch] : s_fb_i[ch])
#else
#define FB_MODE(ch) s_fb_mode[ch]
#define FB_I(ch)    s_fb_i[ch]
#endif

static void od_838b0_standin(void)
{
    const uint16_t v = CELL_OUT_MV[0];
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        const uint8_t m = FB_MODE(ch);          /* the last diagnostic pass's 0x94FD8 */
        const uint16_t i = FB_I(ch);            /* ... and 0x94ED4 */
        const bool on = (m == 3u || m == 4u) && i != 0u;
        uint32_t d = 0u;
        C_RES[ch] = SI_RES[ch];
        C_FB[ch] = on ? 0x10u : 0x21u;
        if (on && v != 0u) {
            d = ((uint32_t)i * SI_RES[ch]) / ((uint32_t)v * 10u);
            if (d > 1000u) {
                d = 1000u;
            }
        }
        C_DUTY[ch] = (uint16_t)d;
        C_DUTYF[ch] = (uint16_t)d;
        C_EST[ch] = i;
    }
}

void drv_od_tick_head(void)
{
    od_904fa_status();
    od_838b0_standin();
}

/* r04f: the real feedback chain over the stand-in (see drv_odiag.h). Runs
 * after drv_od_tick_head in the same output tick (sup_batch: stand-in → hook
 * → law), so nothing reads the five cells in between. */
void drv_od_fb_real(const drv_od_fb_t *fb)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        C_FB[ch] = fb->cls[ch];
        C_DUTY[ch] = fb->duty[ch];
        C_DUTYF[ch] = fb->dutyf[ch];
        C_EST[ch] = fb->estm[ch];
        C_RES[ch] = fb->res[ch];
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * 3. INSIDE THE LAW (0x83DB0's callees)
 * ════════════════════════════════════════════════════════════════════════ */
/* 0x8422E (pass head): the rail-node filter, channels 0..1 (→ 0x93100) */
void drv_od_8422e(void)
{
    for (uint8_t ch = 0u; ch < 2u; ch++) {
        int16_t v = (int16_t)CELL_OUT_MV[ch];
        const int16_t k = CAL_RFK[ch];
        if (k > 0) {
            if (C_9410F == 0u) {
                C_RFST[ch] = (int32_t)v * 10000;                /* 0x88748 */
            } else {                                            /* 0x842BA */
                int32_t st = C_RFST[ch];
                float y = (float)st * 9.999999747378752e-05f;
                int32_t e;
                y += (st < 0) ? -0.5f : 0.5f;
                e = (int32_t)v - (int32_t)(int16_t)(int32_t)y;
                if (e < 0) {
                    st -= (int32_t)((uint32_t)(uint16_t)k * (uint32_t)(uint16_t)(-e));
                } else {
                    st += (int32_t)((uint32_t)(uint16_t)k * (uint32_t)(uint16_t)e);
                }
                C_RFST[ch] = st;
                y = (float)st * 9.999999747378752e-05f;
                y += (st < 0) ? -0.5f : 0.5f;
                v = (int16_t)(int32_t)y;
            }
            C_RF[ch] = v;
        }
    }
    C_9410F = 1u;
}

/* 0x7B6D8's two copies of the request (the stock writes them before the
 * law): the slot array 0x84890 (read by 0x7BA76 / 0x897AC here) and the
 * complement 0x94D60 (read by 0x84392). The stock's 0x94EE8 itself stays
 * unwritten on SH (the request law's safety frame). */
void drv_od_requests(const uint16_t *req)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        C_RQSLOT[ch] = req[ch];
        C_REQC[ch] = (uint16_t)~req[ch];
    }
}

bool drv_od_enabled(void) { return C_EN != 0u; }

/* 0x7BA76 */
uint8_t drv_od_7ba76(uint8_t ch)
{
    if (C_FW[ch] == 0u || C_RQSLOT[ch] != 0u) {
        return 1u;
    }
    return 2u;
}

uint8_t drv_od_chan_class(uint8_t ch) { return C_CCLS[ch]; }

/* 0x8ECF0 */
static void od_8ecf0(uint8_t ch, uint8_t v)
{
    C_OLET[ch] = v;
    if (v == 0x10u) {
        T_947D6[ch] = CAL_OD_7D6;
    }
}

/* 0x86414 */
static void od_86414(uint8_t n)
{
    if (C_939BA[n] != 0xFFu) {
        C_939BA[n] = (uint8_t)(C_939BA[n] + 1u);
    }
}

/* 0x7BA2A */
static void od_7ba2a(uint16_t mask, uint16_t value, uint8_t ch)
{
    rep_7ca24(ch, mask, value);                 /* 0x7CA24(0x9E3, ch, mask, value) */
    mask = (uint16_t)(mask & 0xDFFFu);
    C_FM[ch] = mask;
    C_FW[ch] = (uint16_t)((C_FW[ch] & (uint16_t)~mask) | (value & mask));
}

/* 0x8A6D8: the supply state */
static void od_8a6d8(uint8_t ch)
{
    const uint16_t v = CELL_OUT_MV[0];
    uint8_t s = 1u;
    if (CELL_SEQ2_STATE[0] == (uint8_t)'U') {
        if (CELL_SEQ2_STATUS[0] == 4u) {
            s = 1u;
            if ((int32_t)v >= (int32_t)CAL_SUP3_LO && (int32_t)v < (int32_t)CAL_SUP3_HI) {
                s = 3u;
            }
            if ((int32_t)v >= (int32_t)CAL_SUP4_LO && (int32_t)v < (int32_t)CAL_SUP4_HI) {
                s = 4u;
            }
        } else {
            s = 2u;
        }
    }
    C_SUPST[ch] = s;
}

/* status-word predicates of 0x89F5C / 0x8E8E4 / 0x84644 */
#define ST_F10(w)       (((w) & 0x1000u) != 0u)                 /* hi bit 4 */
#define ST_CODE(w, c)   (!ST_F10(w) && ((w) & 7u) == (c))
#define ST_LO(w, b)     (ST_F10(w) && ((w) & (b)) != 0u)

/* 0x8E8E4: the off-state test machine (diag-config 2 channels) */
static void od_8e8e4(uint8_t ch)
{
    const uint8_t cfg = CAL_DIAGCFG[ch];
    uint8_t st = C_OST[ch];             /* local_28 */
    uint8_t let = C_OLET[ch];           /* cStack_24 */
    const uint8_t c2 = C_STATE[ch];     /* 0x93B0C */
    uint8_t cls = C_CCLS[ch];           /* uVar7 */
    const uint8_t s = CELL_SEQ2_STATE[0];
    const uint16_t v = CELL_OUT_MV[0];
    const uint16_t w = C_STAT[ch];
    uint8_t nst;                        /* bVar6 */

    C_93A60 = v;
    if (cfg != 2u) {
        let = 0u;
        cls = 0u;
        nst = 0u;
        goto store;
    }
    if (s != (uint8_t)'U') {
        cls = 0u;
        C_OLET2[ch] = 0u;
        let = 1u;
        T_947CC[ch] = 0u;
        T_947D6[ch] = 0u;
        T_947E0[ch] = 0u;
        nst = 0u;
        goto store;
    }
    if (v < CAL_OD_RAILMIN && st > 1u && st < 5u) {
        st = 5u;
    }
    nst = st;
    if (st == 0u) {
        if (let != 0x10u) {
            let = 1u;
            if (T_947E0[ch] == 0u) {
                C_OLET2[ch] = 0u;
            }
            goto store;
        }
        cls = 1u;
        let = 0x11u;
        if (T_947E0[ch] != 0u) {
            T_947E0[ch] = CAL_OD_7E0;
        }
        goto inc;                       /* falls to LAB_8ec70 */
    }
    if (st == 1u) {
        bool okq;
        if (c2 == 0x40u) {              /* '@' */
            okq = !ST_F10(w) ? ((w & 7u) == 0u) : true;
            if (okq) {
                let = 1u;
                nst = 0u;
                goto store;
            }
        }
        /* LAB_8e9e8 */
        if (c2 != 0x41u || C_FB[ch] != 0x10u || (w & 0xFFu) != 0u) {
            if (T_947D6[ch] != 0u) {
                goto store;
            }
            T_947CC[ch] = (uint8_t)od_curve_u16(CAL_OD_CURVE0, v);
            cls = 2u;
            nst = 3u;
            if (c2 == 0x41u) {
                goto store;
            }
            goto inc;
        }
        let = 1u;
        nst = 0u;
        goto store;
    }
    if (st == 2u) {
        goto inc;
    }
    if (st == 3u) {
        if (T_947CC[ch] != 0u) {
            goto store;
        }
        if (ST_CODE(w, 1u) || ST_CODE(w, 2u) || ST_CODE(w, 3u)) {
            let = 0x21u;                /* '!' */
            goto st4done;
        }
        if (ST_CODE(w, 4u)) {
            let = 0x21u;
        } else if (ST_CODE(w, 6u)) {
            let = 0x25u;                /* '%' */
            goto st4done;
        } else if (ST_F10(w) || (w & 7u) == 0u) {
            let = 0x20u;                /* ' ' */
        } else {
            let = 0x21u;
            goto st4done;
        }
        T_947CC[ch] = (uint8_t)od_curve_u16(CAL_OD_CURVE1, v);
        cls = 3u;
        goto inc;
    }
    if (st == 4u) {
        if (T_947CC[ch] != 0u) {
            goto store;
        }
        if (ST_CODE(w, 4u)) {
            let = 0x25u;
        } else if (ST_CODE(w, 2u) || ST_CODE(w, 6u)) {
            let = 0x22u;                /* '"' */
        } else if (ST_F10(w) || (w & 7u) == 0u) {
            T_947E0[ch] = CAL_OD_7E0;
            let = 0x20u;
            if (C_FB[ch] != 0x21u && C_DUTY[ch] != 0u) {
                let = 0x24u;            /* '$' */
            }
            C_OLET2[ch] = let;
        } else {
            let = 0x21u;
        }
        goto st4done;
    }
    if (st == 5u) {
        cls = 4u;
        let = 0x24u;
        goto inc;
    }
    if (st == 6u) {
        if (!(ST_F10(w) || (w & 7u) == 0u) || v < CAL_OD_RAILMIN) {
            if (ST_CODE(w, 1u)) {
                let = 0x21u;
            }
            cls = 4u;
            goto store;
        }
        cls = 0u;
        let = 0x30u;
        goto inc;
    }
    if (st == 7u) {
        let = 1u;
        cls = 0u;
        nst = 0u;
        goto store;
    }
    cls = 4u;
    let = 0x21u;
    nst = 0u;
    goto store;
st4done:                                /* LAB_8ec32 */
    cls = 4u;
    nst = 6u;
    goto store;
inc:                                    /* LAB_8ec70 */
    nst = (uint8_t)(st + 1u);
store:                                  /* LAB_8ecd2 */
    C_OLET[ch] = let;
    C_OST[ch] = nst;
    C_CCLS[ch] = cls;
}

/* 0x8E7B4: the duty-saturation state (0x93B16) */
static void od_8e7b4(uint8_t ch)
{
    uint8_t st = C_SAT[ch];
    const uint32_t v = CELL_OUT_MV[0];
    const uint16_t i = CELL_RQ_I[ch];
    uint16_t est;
    if (CAL_SAT_DMAX == 0u) {
        est = 0u;
        st = 0u;
    } else if (CAL_SAT_RAIL[ch] < v || v == 0u || i == 0u) {
        est = 0u;
        st = 1u;
    } else {
        const uint32_t e = (uint32_t)(((float)i * (float)C_RES[ch]) / (float)v);
        est = (uint16_t)e;
        if ((int32_t)v < (int32_t)C_RF[0] - (int32_t)CAL_SAT_RWIN ||
            (uint32_t)((int32_t)C_RF[0] + (int32_t)CAL_SAT_RWIN) < v ||
            CAL_SAT_DFILT < C_DUTYF[ch] ||
            (C_STATE[ch] != 0x31u && C_STATE[ch] != 0x30u) || st == 4u) {
            if (st < 3u) {
                st = 3u;
            }
            if ((int32_t)CAL_SAT_DMAX - (int32_t)CAL_SAT_HYST < (int32_t)(e & 0xFFFFu)) {
                if (CAL_SAT_DMAX <= (e & 0xFFFFu)) {
                    st = 4u;
                }
            } else {
                st = 3u;
            }
        } else {
            st = 2u;
        }
    }
    C_E7B4[ch] = est;
    C_SAT[ch] = st;
}

/* ── 0x89F5C: the per-channel output diagnosis ─────────────────────────── */
void drv_od_89f5c(uint8_t ch)
{
    const uint8_t b1 = C_STATE[ch];             /* bVar1: 0x93B0C */
    const uint8_t c3 = CAL_DIAGCFG[ch];         /* cVar3 */
    const uint16_t bit = chbit(ch);
    uint16_t est, w, r19 = 0u, s30 = 0u, r11 = 0u;
    uint8_t c2, b13;

    c2 = C_FB[ch];                              /* cVar2: 0x93A2E */
    od_8e8e4(ch);
    od_8a6d8(ch);
    est = C_EST[ch];                            /* uVar5 */
    C_DSMW[ch] = est;
    b13 = C_SUPST[ch];
    C_939A8 = (uint16_t)(C_939A8 & (uint16_t)~bit);
    w = C_STAT[ch];

    if (b13 < 3u || CAL_TYPE == 0u) {
        r19 = 0u;
        r11 = 0u;
        s30 = 0u;
        goto a5f4;
    }
    {
        const uint8_t b4 = C_OLET[ch];
        if (b4 < 0x10u) {
            if (b1 < 0x12u || b1 == 0x60u) {
                if (ST_CODE(w, 1u)) {
                    r19 = 8u;
                } else if (ST_CODE(w, 2u) || c2 == 0x20u) {
                    if (c3 == 2u) {
                        od_8ecf0(ch, 0x10u);
                        r19 = 0x1000u;
                    } else {
                        r19 = 0x10u;
                    }
                } else {
                    r19 = ST_CODE(w, 4u) ? 0x400u : 0u;
                }
                goto chk_e100;
            }
            if (b1 >= 0x30u && b1 <= 0x36u) {
                uint8_t c16 = T_94798[ch];
                if (c2 == 0x20u) {
                    if (c3 != 2u) {
                        r19 = 0x10u;
                        goto a21a;
                    }
                    c16 = 0u;
                    goto a1a6;
                }
                if (c2 == 0x21u || (w & 0x10u) != 0u) {
                    if ((C_94930[ch] & 0x900u) != 0u) {
                        rep_87120(ch, 0u, 0u);          /* 0x87120(0, ch, 0, 0, 0) */
                        C_939A8 = (uint16_t)(C_939A8 | bit);
                        C_94930[ch] = (uint16_t)(C_94930[ch] & 0xF6FFu);
                        b13 = C_SUPST[ch];
                    }
                    r19 = 4u;
                    goto a21a;
                }
                if (C_94930[ch] == 4u) {
                    if (!ST_LO(w, 1u) && (w & 0xC0u) == 0u && !ST_LO(w, 4u)) {
                        goto a0d4;
                    }
                    r19 = 4u;
                    goto a21a;
                }
            a0d4:
                if (c2 == 0x31u) {
                    r19 = 2u;
                    goto a21a;
                }
                if (!ST_LO(w, 4u) && !ST_LO(w, 2u) && !ST_LO(w, 1u) && (bit & C_848CC) == 0u) {
                    c16 = CAL_RELOAD_798;
                    goto a21a;
                }
                if (c16 == 0u) {
                    if (c3 == 1u) {
                        r19 = 0x800u;
                        if (!ST_LO(w, 2u) || (bit & C_848CC) != 0u) {
                            C_848CC = (uint16_t)(C_848CC & (uint16_t)~bit);
                        } else {
                            C_848CC = (uint16_t)(C_848CC | bit);
                        }
                    } else if (c3 == 2u) {
                        goto a1a6;
                    } else {
                        r19 = 0u;
                    }
                }
                goto a21a;
            a1a6:
                od_8ecf0(ch, 0x10u);
                b13 = C_SUPST[ch];
                r19 = 0x1000u;
            a21a:
                T_94798[ch] = c16;
                if ((w & 0xE100u) != 0xC100u) {
                    r19 = (uint16_t)(r19 | 1u);
                }
                if (b13 == 4u && b1 < 0x33u && C_OLET[ch] < 0x10u &&
                    (c16 == 0u || CAL_RELOAD_798 == c16)) {
                    const uint16_t u6 = C_UB[ch], u7 = C_LB[ch];
                    const uint8_t fb = C_FB[ch];
                    if ((u6 != 0u || u7 != 0u) && (fb == 0x21u || fb == 0x20u || fb == 0x10u)) {
                        uint16_t r1 = 0x2000u;
                        s30 = 0x2000u;
                        if (CELL_TMP_940FC < 6u || CELL_TMP_ST < 6u) {
                            r19 = (uint16_t)(r19 | 0x2000u);
                        } else {
                            if (u6 != 0u) {
                                r1 = 0x2200u;
                                s30 = 0x2200u;
                                if ((int16_t)est >= (int16_t)u6) {      /* cmp/ge */
                                    r19 = (uint16_t)(r19 | 0x200u);
                                }
                            }
                            if (r19 != 4u && u7 != 0u) {
                                s30 = (uint16_t)(r1 | 0x100u);
                                if (!((int16_t)est > (int16_t)u7)) {    /* cmp/gt */
                                    r19 = (uint16_t)(r19 | 0x100u);
                                }
                            }
                        }
                    }
                }
                goto a438;
            }
            if (b1 == 0x50u) {
                if (ST_CODE(w, 1u)) {
                    r19 = 8u;
                } else if (!ST_F10(w) && (w & 7u) != 0u) {
                    r19 = 0x10u;
                }
                goto chk_e100;
            }
            if (b1 != 0x61u && b1 != 0x51u) {
                goto a438;
            }
            if ((w & 0x10u) != 0u) {
                r19 = 4u;
            }
        chk_e100:
            if ((w & 0xE100u) != 0xC100u) {
                r19 = (uint16_t)(r19 | 1u);
            }
        } else {
            const uint8_t cc = C_CCLS[ch];
            if (cc == 1u || cc == 2u || cc == 3u) {
                r19 = 0x1000u;
            } else if (cc == 4u) {
                switch (b4) {
                case 0x22u: r19 = 0x80u;  break;
                case 0x20u: r19 = 0x20u;  break;
                case 0x21u: r19 = 0x40u;  break;
                case 0x25u: r19 = 0x840u; break;
                case 0x24u: r19 = 0x8C0u; break;
                default:    r19 = 0xC0u;  break;
                }
            }
        }
    }
a438:
    if (C_OLET2[ch] == 0x20u) {
        r19 = (uint16_t)(r19 | 0x20u);
    } else if (C_OLET2[ch] == 0x24u) {
        r19 = (uint16_t)(r19 | 0x8C0u);
    }
    od_8e7b4(ch);
    if (C_SAT[ch] == 4u) {
        r19 = (uint16_t)(r19 & 0x305u);
        od_8ecf0(ch, 0u);
    }
    if (b1 == 0x11u || b1 == 0x10u || b1 == 0x40u || b1 == 0x60u) {
        r11 = (uint16_t)(s30 | ((c3 == 0u || c3 == 1u) ? 0x419u : 0x14C9u));
        s30 = r11;
    } else if (b1 == 0x50u) {
        r11 = 0x19u;
        s30 = r11;
    } else if (b1 == 0x61u || b1 == 0x51u) {
        r11 = 5u;
        s30 = r11;
    } else if (b1 == 0u || b1 == 0x20u) {
        r11 = 0u;
        s30 = 0u;
    } else {
        if (c3 == 1u) {
            r11 = (uint16_t)(s30 | 0x817u);
        } else if (c3 == 2u) {
            if (C_OLET[ch] != 0x10u && (C_OST[ch] == 0u || C_OST[ch] > 5u)) {
                r11 = (uint16_t)(s30 | 0x18E7u);
            } else {
                r11 = 0u;
            }
        } else {
            r11 = (uint16_t)(s30 | 0x17u);
        }
        s30 = r11;
        if (C_SAT[ch] == 4u) {
            r11 = (uint16_t)(r11 & 0x305u);
            s30 = r11;
        }
        if (C_FB[ch] == 0x20u) {
            T_947A2[ch] = CAL_RELOAD_7A2;
        }
        if (T_947A2[ch] != 0u && T_947A2[ch] != CAL_RELOAD_7A2) {
            r11 = (uint16_t)(r11 & (uint16_t)~4u);
            s30 = r11;
        }
    }
a5f4:
    if ((r11 & r19 & 0x10D8u) != 0u && (CAL_CNT_MASK & bit) != 0u) {
        od_86414(0u);
    }
    s30 = (uint16_t)(s30 & CAL_ALLOW);
    r19 = (uint16_t)(r19 & CAL_ALLOW);
    {
        const uint16_t nv = (uint16_t)((r19 & s30) | (C_94930[ch] & (uint16_t)~s30));
        const uint16_t f = (uint16_t)(s30 & r19);
        uint8_t cls = 0u;
        C_94944[ch] = s30;
        C_94930[ch] = nv;
        od_7ba2a(s30, nv, ch);
        if (f != 0u) {
            if ((CAL_M1[ch] & f) != 0u) {
                cls = ((CAL_M1[ch] & f & 0xE0u) != 0u) ? 0x81u : 1u;
            }
            if ((CAL_M2[ch] & f) != 0u) {
                cls = ((CAL_M2[ch] & f & 0xE0u) != 0u) ? 0x82u : 2u;
            }
        }
        C_CLS[ch] = cls;
        rep_87120(ch, cls, f);                  /* 0x87120(0, ch, 0, cls, f) */
    }
}

/* ── 0x897AC: the channel state 0x93B0C (request / mode bookkeeping) ───── */
void drv_od_897ac(uint8_t ch)
{
    const uint8_t c1 = C_STATE[ch];
    const uint8_t c2 = CELL_RQ_MODE[ch];
    uint8_t c8 = T_947EA[ch];
    int16_t req;
    uint8_t out;

    if (ch <= C_LASTCH) {
        uint16_t chg = C_CHG;
        C_94141 = 1u;
        for (uint8_t k = 0u; k < 10u; k++) {
            const uint16_t kb = chbit(k);
            if ((kb & CAL_CH_MASK) == 0u) {
                continue;
            }
            {
                const uint16_t r = (CAL_TYPE != 0u) ? C_RQSLOT[k] : 0u;
                if (CAL_TYPE != 2u) {
                    const int32_t prev = (uint16_t)C_PREQ[k];
                    bool changed;
                    if (r == 0u) {
                        changed = prev != 0;
                    } else {
                        C_94141 = 0u;
                        changed = (int32_t)r <= prev - (int32_t)CAL_CHG_TOL ||
                                  prev + (int32_t)CAL_CHG_TOL <= (int32_t)r;
                    }
                    chg = changed ? (uint16_t)(chg | kb) : (uint16_t)(chg & (uint16_t)~kb);
                }
                if ((kb & CAL_CHG_MASK) != 0u && (kb & chg) != 0u && T_94826 < CAL_HOLD_826[k]) {
                    T_94826 = CAL_HOLD_826[k];
                }
            }
        }
        C_CHG = chg;
    }
    req = (CAL_TYPE != 0u) ? (int16_t)C_RQSLOT[ch] : 0;
    C_LASTCH = ch;
    C_PREQ[ch] = req;
    if (CAL_TYPE == 2u) {
        out = (req != 0) ? 0x51u : 0x50u;
    } else if (C_OLET[ch] < 0x10u) {
        if (c2 != C_PMODE[ch]) {
            C_PMODE[ch] = c2;
            out = 0x20u;
            c8 = CAL_HOLD_7EA[ch];
        } else if (c2 == 0u) {
            out = 0x60u;
        } else if (c2 == 4u) {
            out = 0x61u;
        } else {
            const uint16_t b = chbit(ch);
            const bool b4 = (b & C_CHG) != 0u;
            if (b4 && c1 != 0x20u) {
                c8 = CAL_HOLD_7EA[ch];
            }
            if (c8 == 0u || c1 != 0x20u) {
                if (req == 0) {
                    out = (C_94141 != 0u) ? 0x10u : 0x11u;
                } else {
                    const bool b5 = (b & C_TPEND) != 0u;
                    if ((c8 != 0u && c1 == 0x32u) || b4 || b5) {
                        out = 0x32u;
                        if (b5) {
                            C_TPEND = (uint16_t)(C_TPEND & (uint16_t)~b);
                            c8 = CAL_HOLD_7EA[ch];
                        }
                    } else if (T_94826 == 0u || (c1 != 0x31u && C_CHG == 0u)) {
                        out = 0x30u;
                    } else {
                        out = 0x31u;
                    }
                }
            } else {
                out = 0x20u;
                if (b4) {
                    C_TPEND = (uint16_t)(C_TPEND | b);
                }
            }
        }
    } else if (C_OLET[ch] != 0x30u) {
        out = (C_RQSLOT[ch] == 0u) ? 0x40u : 0x41u;
    } else {
        out = 0x20u;
        c8 = CAL_HOLD_7EA[ch];
    }
    C_STATE[ch] = out;
    T_947EA[ch] = c8;
}

/* ── 0x8ACFE: the bound offset ─────────────────────────────────────────── */
static int32_t od_8acfe(uint8_t ch)
{
    int32_t o = 0;
    bool b;
    if (CELL_SUP_940FA != 0u) {
        T_94827 = CAL_ACF_827;
    }
    if (T_94827 != 0u) {
        o = CAL_ACF_A;
    }
    if (CELL_TMP_940FC < 6u || CELL_TMP_ST < 6u) {
        o += CAL_ACF_TEMP;
    }
    if (C_STATE[ch] == 0x31u) {
        o += CAL_ACF_ONE;
    }
    b = (chbit(ch) & C_93378) != 0u;
    if (b) {
        T_947F4[ch] = CAL_ACF_7F4;
    }
    if (T_947F4[ch] != 0u || b) {
        o += CAL_ACF_CH;
    }
    return o;
}

/* ── 0x8A788: the current bounds 0x93510 (upper) / 0x93524 (lower) ────── */
void drv_od_8a788(uint8_t ch)
{
    const uint16_t i = CELL_RQ_I[ch];
    uint16_t hold_up = T_HI[ch];        /* uVar13: 0x9473C (upper-bound hold) */
    uint16_t hold_lo = T_LO[ch];        /* uVar9: 0x94750 (lower-bound hold) */
    uint16_t up = 0u, lo = 0u;          /* uStack_5c, uStack_58 */

    if (i == 0u) {
        C_HI[ch] = 0u;
        C_LO[ch] = 0u;
        goto out;
    }
    {
        const uint8_t b2 = CAL_BND_B2[ch], b3 = CAL_BND_B3[ch];
        const uint8_t st = C_STATE[ch];
        const uint16_t bit = chbit(ch);
        const int16_t di = (int16_t)(i - C_PI[ch]);
        const int16_t pdi = C_PDI[ch];
        const int32_t adi = di < 0 ? -(int32_t)di : (int32_t)di;
        C_93B20 = di;
        C_PI[ch] = i;
        C_PDI[ch] = di;
        if (st > 0x1Fu && st < 0x33u) {
            if ((b3 & 2u) != 0u || (b2 & 2u) != 0u) {          /* the lower bound */
                uint16_t v;
                const int16_t ofs = (int16_t)(od_8acfe(ch) + CAL_BND_LOOFS);
                const int16_t k = od_curve_s16(CAL_BND_K_LO, di);
                if (pdi != di) {
                    const uint16_t h = (uint16_t)od_curve_s16(CAL_BND_HOLD_LO, di);
                    if (hold_lo < h) {
                        hold_lo = h;
                        C_INCU = (uint16_t)(C_INCU | bit);
                    }
                }
                v = od_satsub(i, (uint16_t)((int32_t)((float)adi * (float)k * 0.0010000000474974513f) + ofs));
                if (hold_lo == 0u) {
                    if (di < 0) {
                        C_LO[ch] = v;
                        hold_lo = (uint16_t)od_curve_s16(CAL_BND_HOLD_LO, di);
                        C_INCU = (uint16_t)(C_INCU | bit);
                    }
                    if (di == 0) {
                        C_INCU = (uint16_t)(C_INCU & (uint16_t)~bit);
                        C_LO[ch] = v;
                    }
                    if (di > 0) {
                        if ((bit & C_INCU) != 0u) {
                            C_LO[ch] = v;
                        }
                        hold_lo = (uint16_t)od_curve_s16(CAL_BND_HOLD_LO, di);
                        C_INCU = (uint16_t)(C_INCU | bit);
                    }
                } else if (di < 0 && v < C_LO[ch]) {
                    C_LO[ch] = v;
                    {
                        const uint16_t h = (uint16_t)od_curve_s16(CAL_BND_HOLD_LO, di);
                        if (hold_lo < h) {
                            hold_lo = h;
                            C_INCU = (uint16_t)(C_INCU | bit);
                        }
                    }
                }
                lo = C_LO[ch];
            }
            if ((b3 & 1u) != 0u || (b2 & 1u) != 0u) {          /* the upper bound */
                uint16_t v;
                const int16_t ofs = (int16_t)(od_8acfe(ch) + CAL_BND_HIOFS);
                const int16_t k = od_curve_s16(CAL_BND_K_HI, di);
                if (pdi != di) {
                    const uint16_t h = (uint16_t)od_curve_s16(CAL_BND_HOLD_HI, di);
                    if (hold_up < h) {
                        hold_up = h;
                        C_INCO = (uint16_t)(C_INCO | bit);
                    }
                }
                v = (uint16_t)((int32_t)((float)adi * (float)k * 0.0010000000474974513f) +
                               (int32_t)ofs + (int32_t)(int16_t)i);
                if (hold_up == 0u) {
                    if (di > 0) {
                        C_HI[ch] = v;
                        hold_up = (uint16_t)od_curve_s16(CAL_BND_HOLD_HI, di);
                        C_INCO = (uint16_t)(C_INCO | bit);
                    }
                    if (di == 0) {
                        C_HI[ch] = v;
                        C_INCO = (uint16_t)(C_INCO & (uint16_t)~bit);
                    }
                    if (di < 0) {
                        if ((bit & C_INCO) != 0u) {
                            C_HI[ch] = v;
                        }
                        hold_up = (uint16_t)od_curve_s16(CAL_BND_HOLD_HI, di);
                        C_INCO = (uint16_t)(C_INCO | bit);
                    }
                } else if (di > 0 && C_HI[ch] < v) {
                    C_HI[ch] = v;
                    {
                        const uint16_t h = (uint16_t)od_curve_s16(CAL_BND_HOLD_HI, di);
                        if (hold_up < h) {
                            hold_up = h;
                            C_INCO = (uint16_t)(C_INCO | bit);
                        }
                    }
                }
                up = C_HI[ch];
                {
                    const int32_t ap = C_PDI[ch] < 0 ? -(int32_t)C_PDI[ch] : (int32_t)C_PDI[ch];
                    if ((int32_t)CAL_BND_DIMAX < ap) {
                        lo = 0u;
                        up = 0u;
                    }
                }
                if (CAL_BND_LOMIN != 0u && lo < CAL_BND_LOMIN) {
                    lo = 0u;
                }
                if (CAL_BND_HIMAX != 0u && CAL_BND_HIMAX < up) {
                    up = 0u;
                }
            }
        }
        {
            uint8_t bb = b2;
            if (st == 0x31u || st == 0x30u) {
                if (i < CAL_BND_ILIM) {
                    lo = 0u;
                    if ((b3 & 1u) == 0u) {
                        up = 0u;
                    }
                }
                if (CAL_BND_ILIM <= i) {
                    up = 0u;
                    bb = b3;
                    if ((bb & 2u) == 0u) {
                        lo = 0u;
                    }
                }
            } else if (st == 0x32u) {
                if ((b2 & 1u) == 0u) {
                    up = 0u;
                }
                if ((bb & 2u) == 0u) {
                    lo = 0u;
                }
            } else {
                lo = 0u;
                up = 0u;
            }
        }
        {
            const uint8_t idx = C_90826[ch];
            const uint16_t ib = (uint16_t)(1u << (idx & 0x1Fu));
            if ((ib & C_93A04) != 0u || (bit & C_93374) != 0u) {
                T_947FE[ch] = CAL_BND_7FE;
            }
        }
        if (T_947FE[ch] != 0u) {
            lo = 0u;
            up = 0u;
        }
    }
out:
    C_UB[ch] = up;
    C_LB[ch] = lo;
    T_HI[ch] = hold_up;
    T_LO[ch] = hold_lo;
}

/* ════════════════════════════════════════════════════════════════════════
 * 4. THE CHANNEL TAIL (both paths of 0x83DB0's loop): 0x84644, 0x84392,
 *    0x8493A
 * ════════════════════════════════════════════════════════════════════════ */
/* 0x84644: the unintended-output check — WITNESS ONLY (the stock calls the
 * fatal handler 0x6865C(0x226) with tag 0x3DCC0 → MCU reset; here: counted
 * and recorded, never a reset) */
static void od_84644(uint8_t ch)
{
    const uint8_t mc = C_MODEC[ch];             /* cVar1: 0x94E50 (last pass) */
    const uint16_t sent = CELL_RQ_CUR_SENT[ch]; /* uVar3: 0x90244 */
    const uint8_t let = CELL_RQ_LET_APPLIED[ch];/* cVar2: 0x9081C */
    const uint16_t ic = (uint16_t)~C_IC[ch];    /* uVar15: ~0x94D4C (last pass) */
    const uint8_t s = CELL_SEQ2_STATE[0];
    const bool t15 = (CELL_DIAG_94115 & 1u) != 0u;
    const bool t14 = (CELL_LAD_94114 & 1u) != 0u;
    bool drive = false;                         /* bVar5 */

    if (od_chip_enabled(ch) && (chbit(ch) & CAL_CH_MASK) != 0u) {
        if (s != C_PSEQ[ch]) {
            C_PSEQ[ch] = s;
            T_GRACE[ch] = CAL_GRACE_SEQ;
        }
        if (s == (uint8_t)'D' || s == (uint8_t)'4' || s == (uint8_t)'8' ||
            s == (uint8_t)'?' || s == (uint8_t)'"') {
            if (sent != 0u || let == (uint8_t)'C') {
                drive = true;
            }
        } else if (s == (uint8_t)'3' || s == 0x11u || s == 0x16u) {
            if (!t15 && !t14 && sent != 0u) {
                drive = true;
            }
        } else if (s == (uint8_t)'U') {
            if (CAL_TYPE == 2u) {
                if ((ic != 0u && let != (uint8_t)'C') || (ic == 0u && let != (uint8_t)'2')) {
                    drive = true;
                }
            } else {
                bool dev_path = false;
                if (CAL_TYPE == 1u) {
                    if (CAL_DIAGCFG[ch] == 2u) {
                        const uint16_t w = C_STAT[ch];
                        bool quiet = !ST_LO(w, 4u) && !ST_LO(w, 2u) && !ST_LO(w, 1u) &&
                                     (ST_F10(w) || (w & 7u) == 0u);
                        if (quiet) {
                            const uint8_t fb = C_FB[ch];
                            quiet = (fb != 0x20u || mc == 0xFBu || ic > 0x2FF9u) &&
                                    (fb != 0x21u || mc == 0xFFu || mc == 0xFEu || ic == 0u) &&
                                    (ic != 0x2FFAu || C_94930[ch] == 0u);
                        }
                        if (!quiet) {
                            T_GRACE[ch] = CAL_GRACE_FLT;
                        }
                    }
                    dev_path = true;                    /* LAB_84896 */
                }
                if (dev_path) {
                    const uint16_t hi = sent < ic ? ic : sent;
                    const uint16_t lo = ic < sent ? ic : sent;
                    const uint16_t dev = od_satsub(hi, lo);
                    if (ic != 0u) {
                        if (dev > 0x104u) {
                            drive = true;
                        }
                        goto grace;
                    }
                }
                if (sent != 0u || let == (uint8_t)'C') {
                    drive = true;
                }
            }
        }
    grace:
        if (T_GRACE[ch] != 0u) {
            drive = false;
            C_LATCH[ch] = 0u;
        }
    } else if (sent != 0u || let == (uint8_t)'C') {
        drive = true;
    }
    if (drive && C_LATCH[ch] != 0u) {
        od_fatal(0x226u, OD_SITE_84644, ch, sent, ic);
    }
    C_LATCH[ch] = drive ? 1u : 0u;
}

/* 0x89718: the corrected-current complement 0x94DD0 */
static void od_89718(uint8_t ch)
{
    uint16_t u2 = 0xFFFFu, u3 = 0xFFFFu;
    if (CAL_TYPE == 1u) {
        const uint8_t mc = C_MODEC[ch];
        if (mc == 0xFCu) {
            u2 = C_I2C[ch];
        }
    }
    {
        const uint16_t u4 = (uint16_t)((uint16_t)~u2 + (uint16_t)CELL_RQ_CORR_LAST);
        if ((int16_t)u4 > 0 && u2 != 0xFFFFu) {
            u3 = (uint16_t)~u4;
            if (u4 > 0x2FFAu) {
                u3 = 0xD005u;
            }
        }
    }
    C_I2C[ch] = u3;
}

/* 0x84392: the law again, in the complement domain (0x94D4C ~I, 0x94E50
 * ~mode, 0x94E18 ~active, 0x94DD0 ~I2). ilock: OUR full-on interlock
 * refused this channel in the law (mirrored here, ours) */
static void od_84392(uint8_t ch, bool ilock)
{
    const uint16_t bit = chbit(ch);
    const uint16_t v = CELL_OUT_MV[0];
    const uint8_t s = CELL_SEQ2_STATE[0];
    const bool t15 = (CELL_DIAG_94115 & 1u) != 0u;
    const bool t14 = (CELL_LAD_94114 & 1u) != 0u;
    uint8_t m = 0xFEu;                          /* uVar16 */
    bool fault;
    uint16_t p, out;                            /* puVar14, uVar15 */

    if (!od_chip_enabled(ch) || (bit & CAL_CH_MASK) == 0u) {
        C_I2C[ch] = 0xFFFFu;
        C_IC[ch] = 0xFFFFu;
        C_MODEC[ch] = 0xFFu;
        return;
    }
    {
        const uint8_t fc = drv_od_7ba76(ch);
        fault = !(fc == 1u || fc == 0u);
        if (fault) {
            m = 0xFFu;
        }
    }
    C_ACTC[ch] = (C_REQC[ch] == 0xFFFFu || fault) ? 0xFFu : 0xFEu;
    p = 0xFFFFu;
    if (s == (uint8_t)'U' && !fault) {
        p = C_REQC[ch];
    }
    if (p < 0xD005u) {
        p = 0xD005u;
    }
    {
        const uint8_t cc = C_CCLS[ch];
        if (cc == 2u) {
            m = 0xFDu;
        } else if (cc == 1u || cc == 0u) {
            if (p < 0xFFFFu) {
                m = 0xFCu;
            }
        } else {
            m = 0xFFu;
        }
    }
    if (s == 0x11u || s == 0x16u || s == (uint8_t)'3') {
        const bool full = (t15 && (bit & CAL_FULL15) != 0u) ||
                          (t14 && (s == 0x11u || s == 0x16u) && (bit & CAL_FULL14) != 0u);
        if (full) {
            m = 0xFBu;
        } else {
            p = 0xFFFFu;
            m = 0xFFu;
        }
    }
    out = p;
    if ((uint32_t)CAL_UV_REL + CAL_UV_MV <= v) {
        C_UVF2 = 0u;
    }
    if (v < CAL_UV_MV || (C_UVF2 & bit) != 0u) {
        if (p < 0xFFFFu && CAL_TYPE == 1u) {
            if (p < (uint16_t)~CAL_UV_THR) {
                C_UVF2 = (uint16_t)(C_UVF2 & (uint16_t)~bit);
                if ((CAL_UV_FLAGS & 2u) != 0u) {
                    p = 0xD005u;
                    m = 0xFBu;
                    out = 0xD005u;
                }
            } else if ((CAL_UV_FLAGS & 1u) != 0u) {
                C_UVF2 = (uint16_t)(C_UVF2 | bit);
                p = 0xFFFFu;
                m = 0xFFu;
                out = 0xFFFFu;
            }
        }
    }
    if ((uint16_t)~CAL_IMIN < p && p < 0xFFFFu && CAL_TYPE == 1u) {
        m = 0xFFu;
        out = 0xFFFFu;
    }
    /* OUR INTERLOCK (not stock), mirrored: the law turned full-on off */
    if (ilock) {
        m = 0xFFu;
        out = 0xFFFFu;
    }
    C_IC[ch] = out;
    C_MODEC[ch] = m;
    C_I2C[ch] = out;
    od_89718(ch);
}

/* 0x8493A: the complement checks — WITNESS ONLY (stock: 0x6BDCA / 0x6BDFE →
 * 0x6865C(0x221) → reset). The 0x94EE8 / 0x94D60 pair is not checked: on SH
 * the request law never writes 0x94EE8 (its safety frame); 0x94D60 is our
 * copy of ~request (drv_od_requests). */
static void od_8493a(uint8_t ch)
{
    if (CELL_RQ_I[ch] != (uint16_t)~C_IC[ch]) {
        od_fatal(0x221u, OD_SITE_8493A_I, ch, CELL_RQ_I[ch], C_IC[ch]);
    }
    if (CELL_RQ_ACTIVE[ch] != (uint8_t)~C_ACTC[ch]) {
        od_fatal(0x221u, OD_SITE_8493A_ACT, ch, CELL_RQ_ACTIVE[ch], C_ACTC[ch]);
    }
    if (CELL_RQ_I2[ch] != (uint16_t)~C_I2C[ch]) {
        od_fatal(0x221u, OD_SITE_8493A_I2, ch, CELL_RQ_I2[ch], C_I2C[ch]);
    }
    if (CELL_RQ_MODE[ch] != (uint8_t)~C_MODEC[ch]) {
        od_fatal(0x221u, OD_SITE_8493A_MODE, ch, CELL_RQ_MODE[ch], C_MODEC[ch]);
    }
}

void drv_od_ch_tail(uint8_t ch, bool ilock)
{
    od_84644(ch);
    od_84392(ch, ilock);
    od_8493a(ch);
}

/* the 0x83DB0 tail: 0x84A23 = 0xFE, 0x84A22 = 1, then the checks (0x226) */
void drv_od_pass_end(uint8_t loops_done)
{
    C_EN_C = 0xFEu;
    C_EN = 1u;
    /* the commanded output of THIS diagnostic pass, for the next tick's
     * feedback stand-in (ours; od_838b0_standin) */
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        s_fb_mode[ch] = CELL_RQ_MODE[ch];
        s_fb_i[ch] = CELL_RQ_I[ch];
    }
    if (loops_done != 10u) {
        od_fatal(0x226u, OD_SITE_LAW_END, 0xFFu, loops_done, 10u);
    }
    if (C_EN != (uint8_t)~C_EN_C) {
        od_fatal(0x226u, OD_SITE_LAW_END, 0xFEu, C_EN, C_EN_C);
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * 5. THE DSM REPORT (0x7B9B8 → 0x7B222 × 10; list entry 0x7ABC8, right
 *    after the law). Channel ch → groups 2ch+1 / 2ch+2 (table 0x45BB4
 *    +0xC/+0xD). The table's per-channel status / tested / word cells
 *    (0x940B6.., 0x938C4..) and 0x938EC are not written (no DSM).
 * ════════════════════════════════════════════════════════════════════════ */
static uint8_t od_7b17c(uint16_t v)
{
    uint8_t b = (uint8_t)((v & 0x1C58u) != 0u);
    if ((v & 0x1090u) != 0u) b = (uint8_t)(b | 4u);
    if ((v & 0x24u) != 0u)   b = (uint8_t)(b | 2u);
    if ((v & 0x100u) != 0u)  b = (uint8_t)(b | 0x20u);
    if ((v & 0x200u) != 0u)  b = (uint8_t)(b | 0x40u);
    if (v != 0u && b == 0u)  b = 0x10u;
    return b;
}

static uint8_t od_82d4c(uint16_t v)        /* first (mask, code) with v ⊇ mask */
{
    for (uint8_t k = 0u; k < 12u; k++) {
        if ((uint16_t)(v & TBL_45C54[k][0]) == TBL_45C54[k][0]) {
            return (uint8_t)TBL_45C54[k][1];
        }
    }
    return 0u;
}

static uint8_t s_dsm_status[10] OD_WIT;   /* our copy of *(0x45BB4[ch]+0) */
static uint8_t s_dsm_tested[10] OD_WIT;   /* our copy of *(0x45BB4[ch]+4) */

void drv_od_report(void)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        const uint8_t g1 = (uint8_t)(2u * ch + 1u), g2 = (uint8_t)(2u * ch + 2u);
        const uint16_t m = C_FM[ch], v = C_FW[ch];
        const uint8_t b5 = od_7b17c(m), b6 = od_7b17c(v);
        s_dsm_tested[ch] = b5;
        s_dsm_status[ch] = (uint8_t)((b6 & b5) | (s_dsm_status[ch] & (uint8_t)~b5));
        if ((v & m) == 0u) {
            if (m == 0u || v != 0u) {
                rep_a0e70(g1, 0u, 0u, 0u);
                rep_a0e70(g2, 0u, 0u, 0u);
            } else {
                rep_a0e70(g1, 0u, 0u, 1u);
                rep_a0e70(g2, 0u, 0u, 1u);
            }
        } else {
            const uint8_t code = od_82d4c((uint16_t)(v & m));
            const uint8_t mon = (uint8_t)(code & 0xFu);
            if ((code & 0x80u) == 0u) {
                if ((code & 0x40u) == 0u) {
                    rep_a0e70(g1, mon, 1u, 1u);
                } else {
                    rep_9b918(g1, mon);
                }
                rep_a0e70(g2, mon, 1u, 0u);
            } else {
                rep_a0e70(g1, mon, 1u, 0u);
                if ((code & 0x40u) == 0u) {
                    rep_a0e70(g2, mon, 1u, 1u);
                } else {
                    rep_9b918(g2, mon);
                }
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * 6. INIT
 * ════════════════════════════════════════════════════════════════════════ */
#if !defined(__sh__)
static bool s_seed_host = true;
void drv_od_host_seed_select(bool on) { s_seed_host = on; }

void drv_od_host_seed_healthy(void)
{
    for (uint8_t c = 0u; c < 4u; c++) {
        CELL_REC_U8(c, REC_ID_LO) = 0xBDu;      /* the dump: BD 12 */
        CELL_REC_U8(c, REC_ID_HI) = 0x12u;
        CELL_REC_U8(c, REC_MRB) = 0x80u;        /* a mode readback (bit 7 status) */
        C_CHIPSUM[c] = 0x81u;
    }
    C_CHIPSUM[4] = 0x02u;                       /* the empty fifth select */
    od_904fa_status();
}
#endif

void drv_od_init(void)
{
    /* our NOLOAD ranges (linker.ld .od_*); the cells inside .sup_cells /
     * .sup_l1c / .sup_l1f / .sup_l1g / .sup_cnt / .sup_os2 were cleared by
     * sup_cells_init (call after it) */
    memset((void *)(uintptr_t)0xFFF9030Cu, 0, 0x14u);
    memset((void *)(uintptr_t)0xFFF9062Cu, 0, 0x04u);
    memset((void *)(uintptr_t)0xFFF84890u, 0, 0x14u);
    memset((void *)(uintptr_t)0xFFF93004u, 0, 0x28u);
    memset((void *)(uintptr_t)0xFFF930F8u, 0, 0x20u);
    memset((void *)(uintptr_t)0xFFF93374u, 0, 0x08u);
    memset((void *)(uintptr_t)0xFFF93394u, 0, 0xA0u);
    memset((void *)(uintptr_t)0xFFF93484u, 0, 0x3Cu);
    memset((void *)(uintptr_t)0xFFF934E8u, 0, 0x50u);
    memset((void *)(uintptr_t)0xFFF9397Eu, 0, 0x02u);
    memset((void *)(uintptr_t)0xFFF93980u, 0, 0x2Au);
    memset((void *)(uintptr_t)0xFFF93A04u, 0, 0x0Eu);
    memset((void *)(uintptr_t)0xFFF93A14u, 0, 0x52u);
    memset((void *)(uintptr_t)0xFFF93B0Cu, 0, 0x16u);
    memset((void *)(uintptr_t)0xFFF94930u, 0, 0x28u);
    C_DIAGCNT = 0u;
    for (uint8_t c = 0u; c < 5u; c++) {
        C_CHIPSUM[c] = 0u;
    }
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        C_REQC[ch] = 0xFFFFu;
        C_IC[ch] = 0xFFFFu;
        C_I2C[ch] = 0xFFFFu;
        C_ACTC[ch] = 0xFFu;
        C_MODEC[ch] = 0xFFu;
        C_LATCH[ch] = 0u;
        C_PSEQ[ch] = 0u;
    }
    C_848CC = 0u;
    /* the stock's initialised-data block (DTBL 0x415B0: ROM 0x65110 →
     * 0xFFF84000): 0x84A22 = 00, 0x84A23 = FF, 0x84A2C = 01, 0x84A2D = FF */
    C_EN = 0u;
    C_EN_C = 0xFFu;
    U8C(0xFFF84A2Cu) = 0x01u;
    C_LASTCH = 0xFFu;
    C_9410F = 0u;
    C_94141 = 0u;
    memset(s_grp, 0, sizeof s_grp);
    memset(&s_fatal, 0, sizeof s_fatal);
    memset(s_event_n, 0, sizeof s_event_n);
    memset(s_rec87_n, 0, sizeof s_rec87_n);
    memset(s_rec87_last, 0, sizeof s_rec87_last);
    memset(s_dsm_status, 0, sizeof s_dsm_status);
    memset(s_dsm_tested, 0, sizeof s_dsm_tested);
    s_diag_sent_n = 0u;
    s_diag_drop_n = 0u;
    s_cyc_t_us = 0u;
    s_diag_armed = 0u;
    memset(s_fb_mode, 0, sizeof s_fb_mode);
    memset(s_fb_i, 0, sizeof s_fb_i);
#if !defined(__sh__)
    if (s_seed_host) {
        drv_od_host_seed_healthy();
    }
#endif
}
#endif /* !TCU_RAM_APP */

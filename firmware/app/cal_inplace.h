/* cal_inplace.h — THE READ-ONLY SHADOWS READ THE STOCK CALIBRATION IN PLACE
 * (2026-11-20, ~/tcu_sweep/cal_in_place_port.md).
 *
 * Flash above our application's limit (CAL_FLASH_CAL_BASE = 0xA0000) stays
 * on the board byte-identical to the stock image (the install verifies those
 * blocks' signatures every flash); the stock calibration block itself is
 * 0x180000-0x1FFFFF. (2026-11-20 correction: the application block is
 * 0x040000-0x17FFFF; the earlier "0x80000 and above is calibration" was wrong
 * — 0x80000-0x17FFFF is stock code our firmware never runs. Our app may now
 * grow to 0xA0000; every in-place address must stay at or above it.) The shadows (fr_*, sm_*, gp_*) used
 * to carry const COPIES of the tables they read there. They now read the
 * stock bytes where they are: on the target through an absolute address, on
 * the host through the stock image file loaded into a buffer
 * (app/cal_inplace.c; skips loudly, exit 77, when the file is missing).
 *
 * THE ONE ADDRESS LIST of the in-place views is CAL_IP_LIST below (each entry:
 * name, stock address, length in bytes, the module's accessor). The host test
 * `cal_inplace` checks every entry — no address below CAL_FLASH_CAL_BASE, none
 * past the image — and the POST_BUILD scan tests/unit/cal_inplace_scan.py
 * refuses any SH ELF with a section at CAL_FLASH_CAL_BASE or above.
 * Tables whose stock source lies BELOW it (our app overwrites that
 * region: 0x505A4, 0x63122, 0x6315C, 0x4F6A4 ...) stay const copies in the
 * generated *_cal.c.
 *
 * Wide values are big-endian in the image: on SH (big-endian) a direct load,
 * on the host a byte-assembled read. Byte tables may be addressed through
 * cal_p8() on both. Read-only like the shadows: an output path may not
 * include it. */
#ifndef TCU_APP_CAL_INPLACE_H
#define TCU_APP_CAL_INPLACE_H

#if defined(TCU_OUTPUT_PATH)
#error "the shadows' in-place calibration view is read-only: an output path may not include cal_inplace.h"
#endif

#include <stdint.h>

#define CAL_FLASH_CAL_BASE 0xA0000u        /* our app's flash limit: in-place reads lie at or above it, never written */
#define CAL_FLASH_END      0x280000u       /* the stock image is 0x280000 bytes (2.5 MiB) */

/* ── the address list ───────────────────────────────────────────────────── */
/* fault-reaction shadow (fr_*) */
#define CAL_FR_GRP        0x1923ECu   /* 195 DSM group configs, 36 B each (cell pointer 0x4F1D0) */
#define CAL_FR_GRP_ROW    36u
#define CAL_FR_NG         195u
#define CAL_FR_DEP        0x1955A8u   /* the dependency masters, 3 x u16 per group (0x9CDA8) */
#define CAL_FR_RELOAD     0x191B30u   /* comm entries 0..52: reload u16 (table 0x191B2C + 4) */
#define CAL_FR_THR        0x191BB4u   /* threshold u16 (0x191BB0 + 4) */
#define CAL_FR_MASK       0x191C38u   /* mask u8 (0x191C34 + 4) */
#define CAL_FR_NE         53u
/* shift-map shadow (sm_*) */
#define CAL_SM_MAP0       0x1E9910u   /* five maps, contiguous (pointer table 0x62278 -> 0x1E9910 / 9A9C / 9C28 / 9DB4 / 9F40) */
#define CAL_SM_MAP_LEN    0x18Cu      /* each {u32 nx, u32 ny, u16 xa[14], ya[12], z[12 x 14]} */
#define CAL_SM_NMAP       5u
#define CAL_SM_REMAP      0x1F3B80u   /* u8[100] (0x1F3B7C + 4) */
#define CAL_SM_PRIO       0x1F3BECu   /* u8[252] (0x1F3BE4 + 8) */
#define CAL_SM_STEP       0x1E5E4Cu   /* u8[126] (0x1E5E44 + 8) */
#define CAL_SM_MINGEAR    0x1E5E34u   /* u16[8] (0x1E5E30 + 4) */
#define CAL_SM_AB30W      0x1FEC8Cu   /* u8[2] */
#define CAL_SM_GSPD       0x19AB78u   /* six 10 x 10 tables, stride 0xD0, data at +8: s16 x5 ... */
#define CAL_SM_GSPD_STEP  0xD0u
#define CAL_SM_GTEMP      (CAL_SM_GSPD + 5u * CAL_SM_GSPD_STEP) /* ... the sixth: u8[100] */
/* gear-pair shadow (gp_*) */
#define CAL_GP_F8_DN      0x1B607Cu   /* {u32 5, u8[5]} */
#define CAL_GP_F8_UP      0x1B6094u
#define CAL_GP_F8_OV      0x19B03Cu
#define CAL_GP_WIN_OUT    0x1B69A4u   /* u16 10 x 10 at +8; four tables, stride 0xD0 (0xEC40E / 4D8 / 578 / 614) */
#define CAL_GP_WIN_LO     0x1B6664u   /* s16, torque lower */
#define CAL_GP_WIN_HI     0x1B6324u   /* s16, torque upper */
#define CAL_GP_WIN_STEP   0xD0u
#define CAL_GP_K7C        0x1FE8E9u   /* u8 */
#define CAL_GP_TMP        0x1FE8EAu   /* u8[10] */
#define CAL_GP_KA8        0x1FE8F4u   /* u8 */
#define CAL_GP_OV_PED     0x1FE8F6u   /* u8 */
#define CAL_GP_SYNC       0x1FF1F6u   /* u16 */
#define CAL_GP_THR        0x1FF1FAu   /* s16[5] */
#define CAL_GP_OV_MAP     0x1B78A0u   /* {u32 4, s16 x[4], s16 y[4]} */
/* torque-intervention shadow (ti_*): the maps / curves the per-type blocks (app/ti_cal.c) point at, and its scalars */
#define CAL_TI_MAPS       0x1AAB84u   /* maps and curves 0x1AAB84 .. 0x1B52FF (the variant-0 block words, the common block's maps) */
#define CAL_TI_MAPS_LEN   0xA77Cu
#define CAL_TI_SC1        0x1FE83Eu   /* scalars: thresholds, enables, ramp lengths (0x1FE83E .. 0x1FE8CB) */
#define CAL_TI_SC1_LEN    0x8Eu
#define CAL_TI_SC2        0x1FF126u   /* scalars: timeouts, thresholds, gains (0x1FF126 .. 0x1FF1AF) */
#define CAL_TI_SC2_LEN    0x8Au

/* lock-up (WK) shadow (wk_*): every address is a stock address >= 0xA0000, reached through the config
 * struct 0x54680 (*0xFFF84D10; the D10+offset in each comment) or a direct literal */
#define CAL_WK_REQTAB     0x19F410u   /* {u32 4, u8[4]} the request-level table (index 0xFFF8A0A1) */
#define CAL_WK_ENG        0x1FF01Cu   /* u16 500: engine speed to engage */
#define CAL_WK_TURB       0x1FF01Eu   /* u16 750: turbine speed to engage */
#define CAL_WK_TRAC_HI    0x1FF002u   /* s16 100: traction flag set above this turbine torque */
#define CAL_WK_TRAC_LO    0x1FF004u   /* s16 -100: cleared below */
#define CAL_WK_TRAC_SLIP  0x1FE7DFu   /* u8 5: slip hysteresis of the traction flag */
#define CAL_WK_FLUID0     0x19F13Cu   /* f32 0.0: fluid temperature to engage (C) */
#define CAL_WK_FLUID_HYS  0x1A27C8u   /* f32 5.0: its hysteresis (D10+4) */
#define CAL_WK_OVERHEAT   0x1A27C4u   /* f32 135.0: fluid overheat (D10+0) */
#define CAL_WK_OVERHEAT2  0x1A28ECu   /* f32 150.0: converter overheat (D10+0x4C) */
#define CAL_WK_SLIP_A     0x1FE811u   /* u8 10 (D10+0x58): states 5/6/8 -> 9 and 10 -> 9 slip limit */
#define CAL_WK_SLIP_B     0x1FE812u   /* u8 10 (D10+0x5C): 10 -> 6 */
#define CAL_WK_SLIP_C     0x1FE813u   /* u8 10 (D10+0x60): 6/5 -> 8 */
#define CAL_WK_SLIP_D     0x1FE814u   /* u8 5 (D10+0x64): 5/8 -> 6 */
#define CAL_WK_CFG3C      0x1FE80Du   /* u8 1 (D10+0x3C): 7 -> 6 */
#define CAL_WK_CFG40      0x1FE80Eu   /* u8 0 (D10+0x40): the 5/6/10 -> 7 range test */
#define CAL_WK_CFG278     0x1FE823u   /* u8 0 (D10+0x278): the release hold */
#define CAL_WK_REL_BAR    0x1A5D1Cu   /* f32 0.5 (D10+0x228): release complete below (bar) */
#define CAL_WK_B5E_EN     0x1FE827u   /* u8 1: the lock-up torque raise enable */
#define CAL_WK_CEIL       0x1A2800u   /* {u32 4, f32 x[4], f32 y[4]} the pressure ceiling curve (D10+0x34) */
#define CAL_WK_FF_MAP     0x1A3C04u   /* {u32 6, u32 6, f32...} the torque feed-forward map (D10+0x140) */
#define CAL_WK_RAMP       0x1A3B64u   /* {u32 6, f32 x[6], f32 y[6]} the ramp-in curve (D10+0x130) */
#define CAL_WK_TARGET     0x1A3E3Cu   /* f32 40.0: the slip target (D10+0x15C) */
#define CAL_WK_KI         0x1A3E40u   /* f32 0.05: the integral gain, x 0.01 (D10+0x160) */
#define CAL_WK_OUTMAP     0x1A2834u   /* {u32 8, u32 4, f32...} the output map (D10+0x48) */
#define CAL_WK_B2AC       0x1FF072u   /* s16 10: B2AC = B2AA - 100 x this */
#define CAL_WK_ZMAP       0x1A8638u   /* {u32 4, u32 2, f32...} the 0xD9F10 correction map of a type-0 (NC) element: z all 0 */

/* X-macro of every in-place object: name, address, length, accessor note.
 * Used by the host test (range / spot checks) — keep it complete. */
#define CAL_IP_LIST(X) \
    X(fr_grp,     CAL_FR_GRP,     CAL_FR_NG * CAL_FR_GRP_ROW) \
    X(fr_dep,     CAL_FR_DEP,     CAL_FR_NG * 6u) \
    X(fr_reload,  CAL_FR_RELOAD,  CAL_FR_NE * 2u) \
    X(fr_thr,     CAL_FR_THR,     CAL_FR_NE * 2u) \
    X(fr_mask,    CAL_FR_MASK,    CAL_FR_NE) \
    X(sm_maps,    CAL_SM_MAP0,    CAL_SM_NMAP * CAL_SM_MAP_LEN) \
    X(sm_remap,   CAL_SM_REMAP,   100u) \
    X(sm_prio,    CAL_SM_PRIO,    252u) \
    X(sm_step,    CAL_SM_STEP,    126u) \
    X(sm_mingear, CAL_SM_MINGEAR, 16u) \
    X(sm_ab30w,   CAL_SM_AB30W,   2u) \
    X(sm_gspd,    CAL_SM_GSPD,    5u * CAL_SM_GSPD_STEP + 108u) \
    X(gp_f8_dn,   CAL_GP_F8_DN,   9u) \
    X(gp_f8_up,   CAL_GP_F8_UP,   9u) \
    X(gp_f8_ov,   CAL_GP_F8_OV,   9u) \
    X(gp_win_out, CAL_GP_WIN_OUT, 3u * CAL_GP_WIN_STEP + 208u) \
    X(gp_win_lo,  CAL_GP_WIN_LO,  3u * CAL_GP_WIN_STEP + 208u) \
    X(gp_win_hi,  CAL_GP_WIN_HI,  3u * CAL_GP_WIN_STEP + 208u) \
    X(gp_k7c,     CAL_GP_K7C,     1u) \
    X(gp_tmp,     CAL_GP_TMP,     10u) \
    X(gp_ka8,     CAL_GP_KA8,     1u) \
    X(gp_ov_ped,  CAL_GP_OV_PED,  1u) \
    X(gp_sync,    CAL_GP_SYNC,    2u) \
    X(gp_thr,     CAL_GP_THR,     10u) \
    X(gp_ov_map,  CAL_GP_OV_MAP,  20u) \
    X(wk_reqtab,  CAL_WK_REQTAB,  8u) \
    X(wk_eng,     CAL_WK_ENG,     4u) \
    X(wk_trac,    CAL_WK_TRAC_HI, 4u) \
    X(wk_tracslip, CAL_WK_TRAC_SLIP, 1u) \
    X(wk_fluid0,  CAL_WK_FLUID0,  4u) \
    X(wk_fluidhys, CAL_WK_FLUID_HYS, 4u) \
    X(wk_overheat, CAL_WK_OVERHEAT, 4u) \
    X(wk_overheat2, CAL_WK_OVERHEAT2, 4u) \
    X(wk_slips,   CAL_WK_SLIP_A,  4u) \
    X(wk_cfg3c,   CAL_WK_CFG3C,   2u) \
    X(wk_cfg278,  CAL_WK_CFG278,  1u) \
    X(wk_relbar,  CAL_WK_REL_BAR, 4u) \
    X(wk_b5een,   CAL_WK_B5E_EN,  1u) \
    X(wk_ceil,    CAL_WK_CEIL,    36u) \
    X(wk_ffmap,   CAL_WK_FF_MAP,  200u) \
    X(wk_ramp,    CAL_WK_RAMP,    52u) \
    X(wk_target,  CAL_WK_TARGET,  4u) \
    X(wk_ki,      CAL_WK_KI,      4u) \
    X(wk_outmap,  CAL_WK_OUTMAP,  184u) \
    X(wk_b2ac,    CAL_WK_B2AC,    2u) \
    X(wk_zmap,    CAL_WK_ZMAP,    64u) \
    X(ti_maps,    CAL_TI_MAPS,    CAL_TI_MAPS_LEN) \
    X(ti_sc1,     CAL_TI_SC1,     CAL_TI_SC1_LEN) \
    X(ti_sc2,     CAL_TI_SC2,     CAL_TI_SC2_LEN)

/* ── the accessors ──────────────────────────────────────────────────────── */
#if defined(__sh__)
/* the target: the stock bytes are mapped at their flash addresses */
static inline const uint8_t *cal_p8(uint32_t a) { return (const uint8_t *)(uintptr_t)a; }
static inline uint8_t  cal_u8(uint32_t a)  { return *(const uint8_t *)(uintptr_t)a; }
static inline uint16_t cal_u16(uint32_t a) { return *(const uint16_t *)(uintptr_t)a; }
static inline int16_t  cal_s16(uint32_t a) { return *(const int16_t *)(uintptr_t)a; }
static inline uint32_t cal_u32(uint32_t a) { return *(const uint32_t *)(uintptr_t)a; }
#else
/* the host: the stock image file, loaded on first use (app/cal_inplace.c) */
const uint8_t *cal_img(void);
static inline const uint8_t *cal_p8(uint32_t a) { return cal_img() + a; }
static inline uint8_t  cal_u8(uint32_t a)  { return cal_img()[a]; }
static inline uint16_t cal_u16(uint32_t a)
{
    const uint8_t *p = cal_img() + a;
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static inline int16_t cal_s16(uint32_t a) { return (int16_t)cal_u16(a); }
static inline uint32_t cal_u32(uint32_t a)
{
    const uint8_t *p = cal_img() + a;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
#endif

/* a stock float (big-endian image word; IEEE-754) */
static inline float cal_f32(uint32_t a)
{
    union { uint32_t u; float f; } c;
    c.u = cal_u32(a);
    return c.f;
}

#endif /* TCU_APP_CAL_INPLACE_H */

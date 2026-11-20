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
    X(gp_ov_map,  CAL_GP_OV_MAP,  20u)

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

#endif /* TCU_APP_CAL_INPLACE_H */

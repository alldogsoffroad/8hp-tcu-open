/* test_cal_inplace.c — THE SHADOWS READ THE STOCK CALIBRATION IN PLACE (app/cal_inplace.h).
 *
 * (1) the address list: every in-place object lies at 0x80000 or above (never in our app's range
 *     0x40000..0x7FFFF, never in a region our image overwrites) and inside the stock image; the
 *     objects the shadows' wide accessors read are aligned;
 * (2) the digests: each object's FNV-1a over the bytes the view returns (cal_p8) equals the golden
 *     digest of the stock bytes — and the goldens equal the digests of the former generated COPIES
 *     (serialised big-endian; verified when the copies were retired, 2026-11-20);
 * (3) the endianness / typing of the accessors the shadows use, against values the decodes quote
 *     (literals taken from the retired generated copies — an oracle independent of the image).
 * Exit 77 (a loud SKIP from app/cal_inplace.c) when the stock image file is missing. */
#include <stdio.h>
#include <string.h>

#include "app/cal_inplace.h"
#include "app/fr_cal.h"
#include "app/gp_cal.h"
#include "app/sm_cal.h"
#include "cal_inplace_golden.h"

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECKV(a, b) CHECK((long long)(a) == (long long)(b), "%s = %lld, want %lld", #a, (long long)(a), (long long)(b))

static uint32_t fnv(const uint8_t *p, uint32_t n)
{
    uint32_t h = 0x811C9DC5u;
    while (n--) {
        h = (h ^ *p++) * 0x01000193u;
    }
    return h;
}

int main(void)
{
    /* (1) the address list vs the golden table (same order), the ranges, the digests */
    static const struct { const char *name; uint32_t addr, len; } lst[] = {
#define X(n, a, l) { #n, (a), (l) },
        CAL_IP_LIST(X)
#undef X
    };
    const unsigned n = (unsigned)(sizeof lst / sizeof lst[0]);
    CHECKV(n, sizeof cal_golden / sizeof cal_golden[0]);
    for (unsigned i = 0u; i < n && i < sizeof cal_golden / sizeof cal_golden[0]; i++) {
        CHECK(strcmp(lst[i].name, cal_golden[i].name) == 0, "entry %u: %s vs %s", i, lst[i].name, cal_golden[i].name);
        CHECKV(lst[i].addr, cal_golden[i].addr);
        CHECKV(lst[i].len, cal_golden[i].len);
        CHECK(lst[i].addr >= CAL_FLASH_CAL_BASE, "%s at 0x%X is below the calibration region 0x%X (our app overwrites it)",
              lst[i].name, lst[i].addr, CAL_FLASH_CAL_BASE);
        CHECK(lst[i].addr + lst[i].len <= CAL_FLASH_END, "%s runs past the stock image", lst[i].name);
        CHECK(lst[i].len > 0u, "%s empty", lst[i].name);
        CHECKV(fnv(cal_p8(lst[i].addr), lst[i].len), cal_golden[i].fnv);
    }
    /* the accessors' alignment (SH-2A has no unaligned access) */
    CHECK((CAL_FR_DEP & 1u) == 0 && (CAL_FR_RELOAD & 1u) == 0 && (CAL_FR_THR & 1u) == 0, "fr u16 tables");
    CHECK((CAL_SM_MAP0 & 3u) == 0 && (CAL_SM_MAP_LEN & 3u) == 0, "sm maps carry u32 headers");
    CHECK((CAL_SM_MINGEAR & 1u) == 0 && ((CAL_SM_GSPD + 8u) & 1u) == 0 && (CAL_SM_GSPD_STEP & 1u) == 0, "sm u16 tables");
    CHECK(((CAL_GP_WIN_OUT + 8u) & 1u) == 0 && ((CAL_GP_WIN_LO + 8u) & 1u) == 0 && ((CAL_GP_WIN_HI + 8u) & 1u) == 0 &&
              (CAL_GP_WIN_STEP & 1u) == 0 && (CAL_GP_SYNC & 1u) == 0 && (CAL_GP_THR & 1u) == 0 &&
              ((CAL_GP_OV_MAP + 4u) & 1u) == 0, "gp u16 tables");

    /* (3) the accessors against decoded values */
    /* fr: group 136 = U11E4 (fail / heal 450 / 450 ms), 109 = U1267 (500 / 500), 98 = P0850 (2500 / 100) */
    CHECKV(FRC_FAIL(FR_CFG(136u)), 450);
    CHECKV(FRC_HEAL(FR_CFG(136u)), 450);
    CHECKV(FRC_FAIL(FR_CFG(109u)), 500);
    CHECKV(FRC_HEAL(FR_CFG(109u)), 500);
    CHECKV(FRC_FAIL(FR_CFG(98u)), 2500);
    CHECKV(FRC_HEAL(FR_CFG(98u)), 100);
    /* the OR of the reaction words of groups 28 | 98 | 109 | 136 = the dump's masks 00813240 / E8260114 */
    {
        static const unsigned gs[4] = { 28u, 98u, 109u, 136u };
        uint32_t m0 = 0u, m1 = 0u;
        for (unsigned i = 0u; i < 4u; i++) {
            m0 |= FRC_R0(FR_CFG(gs[i]));
            m1 |= FRC_R1(FR_CFG(gs[i]));
        }
        CHECKV(m0, 0x00813240u);
        CHECKV(m1, 0xE8260114u);
    }
    /* the dependency masters: only groups 97 / 149 / 150 carry them (f0 section 4.1) */
    {
        unsigned nz = 0u;
        for (unsigned g = 1u; g <= FR_NG; g++) {
            for (unsigned k = 0u; k < 3u; k++) {
                if (FR_DEP(g, k) != 0u) {
                    CHECK(g == 97u || g == 149u || g == 150u, "group %u has a dependency master", g);
                    nz++;
                }
            }
        }
        CHECKV(nz, 3);
        CHECKV(FR_DEP(97u, 0), 165);    /* first master of group 97 (the retired copy's { 165u, 0u, 0u }) */
        CHECKV(FR_DEP(149u, 0), 150);
        CHECKV(FR_DEP(150u, 0), 149);
    }
    CHECKV(FR_RELOAD(0u), 80);
    CHECKV(FR_RELOAD(17u), 300);
    CHECKV(FR_THR(1u), 65);
    CHECKV(FR_THR(17u), 260);
    CHECKV(FR_MASK(0u), 9);
    CHECKV(FR_MASK(1u), 143);
    CHECKV(FR_MASK(21u), 15);
    /* sm: map 0 header and first cells; map 1's cell the mutation negative moves (line 8, pedal row 0: 125) */
    CHECKV(cal_u32(SM_CAL_MAP_ADDR(0)), 14);
    CHECKV(cal_u32(SM_CAL_MAP_ADDR(0) + 4u), 12);
    CHECKV(cal_u16(SM_CAL_MAP_ADDR(0) + 8u + 2u * 13u), 14);                           /* xa[13] */
    CHECKV(cal_u16(SM_CAL_MAP_ADDR(0) + 8u + 2u * 14u), 13);                           /* ya[0] */
    CHECKV(cal_u16(SM_CAL_MAP_ADDR(0) + 8u + 2u * (14u + 12u)), 504);                  /* z[0] */
    CHECKV(cal_u16(SM_CAL_MAP_ADDR(0) + 8u + 2u * (14u + 12u) + 2u * 6u), 3600);       /* z[6] */
    CHECKV(cal_u16(SM_CAL_MAP_ADDR(1) + 8u + 2u * (14u + 12u) + 2u * 7u), 125);
    {
        static const uint16_t mg[8] = { 0u, 1188u, 1783u, 2667u, 3353u, 4340u, 5600u, 6746u };
        for (unsigned i = 0u; i < 8u; i++) {
            CHECKV(cal_u16(CAL_SM_MINGEAR + 2u * i), mg[i]);
        }
    }
    CHECKV(cal_u8(CAL_SM_AB30W), 1);
    CHECKV(cal_u8(CAL_SM_AB30W + 1u), 1);
    CHECKV(cal_u8(CAL_SM_PRIO + 18u), 0x1B);
    CHECKV(cal_u8(CAL_SM_PRIO + 22u), 0x01);
    CHECKV(SM_CAL_GTEMP(0u), 190);
    CHECKV(SM_CAL_GTEMP(99u), 190);
    /* gp: the f8 arrays, the temperature windows, the thresholds, the override map */
    {
        static const uint8_t dn[5] = { 0u, 1u, 1u, 1u, 1u }, up[5] = { 0u, 1u, 1u, 1u, 1u }, ov[5] = { 1u, 1u, 2u, 2u, 2u };
        static const uint8_t tmp[10] = { 170u, 40u, 40u, 170u, 40u, 40u, 150u, 70u, 185u, 40u };
        static const int16_t ox[4] = { 2000, 3000, 4000, 6000 }, oy[4] = { 300, 200, 150, 100 };
        for (unsigned i = 0u; i < 5u; i++) {
            CHECKV(GP_CAL_F8_DN(i), dn[i]);
            CHECKV(GP_CAL_F8_UP(i), up[i]);
            CHECKV(GP_CAL_F8_OV(i), ov[i]);
            CHECKV(GP_CAL_THR(i), 0);
        }
        for (unsigned i = 0u; i < 10u; i++) {
            CHECKV(GP_CAL_TMP(i), tmp[i]);
        }
        for (unsigned i = 0u; i < 4u; i++) {
            CHECKV(GP_CAL_OV_X(i), ox[i]);
            CHECKV(GP_CAL_OV_Y(i), oy[i]);
        }
    }
    CHECKV(GP_CAL_K7C, 1);
    CHECKV(GP_CAL_KA8, 1);
    CHECKV(GP_CAL_OV_PED, 77);
    CHECKV(GP_CAL_SYNC, 20);
    /* the windows: the retired copy's row 0 cell 23 = -200 / 1300 (to 1, from -1 ... the 10 x 10 index) */
    for (unsigned f = 0u; f < 4u; f++) {
        CHECKV(GP_CAL_WIN_LO(f, 23u), -200);
        CHECKV(GP_CAL_WIN_HI(f, 23u), 1300);
        CHECKV(GP_CAL_WIN_LO(f, 0u), 0);
    }
    if (g_fail == 0) {
        printf("cal_inplace: %u in-place objects, ranges / digests / accessors OK\n", n);
        return 0;
    }
    printf("cal_inplace: %d FAIL\n", g_fail);
    return 1;
}

/* rng_shadow.c — the stock's cyclic 10 ms range chain as a READ-ONLY
 * SHADOW (2026-11-17). Ported from the bytes of the pristine image
 * (sh-elf-objdump -m sh2a -EB; every function cites its address range);
 * ~/tcu_sweep/range_shadow_port.md has the function map, the stand-ins
 * and the corrections to m5_reverse_range_decode.md.
 *
 * The stock 10 ms list 0x4D834 runs, in this order:
 *   0x9B306  0xA7C9C(D61A) gather; 0x155AC0 [not ported: profiling/safety
 *            object 0xFFF971B4]; 0xA5878(E530) = 0xA5884; 0xA8F5C(D5E6)
 *   0x9B322  0x9B2AC [cal-variant select on D61A[0] change: D61A[0] is a
 *            constant 0 here, the variant is the dump's]; D5E5 path [D5E5
 *            dump 0]; 0xA591C [object 0xFFF81C38, no chain cell];
 *            0xA5944: 0xA6A2C(D60C) [0xA6A3E ported; 0xA6B06 not], 0xC36FC(893E0),
 *            0xA8EFE(D5E6), 0xC6E38(E51C) [read only by 0xC7EF8, not
 *            ported]; 0xA525A(E549) [+0x10 only]
 *   0x9B34A  0xA5A4C: 0xC4F90(E531), 0xC5550(893F8) [no chain reader];
 *            0xA58D4: 0xC7D64(E532), 0xC29A0(89418), 0xC8556(89434),
 *            0xC64D4(D6CA); 0xA64B6 / 0xA59C0 [no chain reader];
 *            0xA59E8: 0xC7A14(E543), 0xC3A7E(89420), 0xC2754(D6DE),
 *            0xC360A(E546), 0xC70CE(D6D4); 0xA6CF8 / 0xA8474 / 0xA59A4
 *            [no chain reader]
 * "No chain reader" = no ported function reads a cell the skipped one
 * writes (getter/literal audit, range_shadow_port.md §4).
 *
 * The shadow writes ONLY its own block (.rng_shadow). It cannot name a
 * live cell: TCU_RNG_SHADOW_TU makes every live cell header #error.
 *
 * LIVE INPUTS (2026-11-17, ~/tcu_sweep/range_shadow_live_inputs.md):
 *   - the chain runs only in OS modes 1..3: the 10 ms lists of modes 1..3
 *     (0x4D834 / 0x4DA2C / 0x4DC24) carry 0x9B306..0x9B34A, the mode-0 one
 *     (0x4D7A0: 0x1320B0, 0xA0E2E, 0x1534EE) does not [O]; the 0x9B228
 *     one-shot is in the mode-0 row-1 list 0x4D7B0 (0x9B302 → bra 0x9B228);
 *   - 0xA6A2C (in 0xA5944, 10 ms) → 0xA6A3E: the D60C counters behind the
 *     U1267 monitor; 0xA6AB0 (via 0xA6A3A in 0x9B39E, the 20 ms lists
 *     0x4D910 / 0x4DB08 / 0x4DD00 of modes 1..3) → the report
 *     0xA0E70(group 109, 1, failed = (D673 == 2), tested = (D60C > 50 &&
 *     D65A > 7000)) — the argument order is 0x9BE50's (r6 failed, r7
 *     tested: its pass path 0x9BF20 runs on r6 == 0 && r7 != 0, and the
 *     group-98 call site 0x141F92 passes r6 = failed, r7 = tested) [O bytes,
 *     I names]; the decode had tested/failed the other way round;
 *   - the gather's brake (0x135DC6), engine-speed (0x13BF66) and 0x14B
 *     (0x13608C, filter 0x154918 = identity at cal 01/00, then / 5)
 *     conditioning, mapped from the vehicle evaluation; absent = the dump.
 */
#define TCU_RNG_SHADOW_TU 1
#include "app/rng_shadow.h"
#include "app/range.h"              /* range_arbitrate: pure (writes *res only) */
#include "io/vehicle/vehicle.h"     /* the lever evaluation, read-only */
#include "bsw/sched/sched.h"        /* sched_now_ms, read-only */

#include <string.h>

#if defined(TCU_RAM_APP)
/* the RAM application does not carry the shadow (its 64 KB limit) */
void rng_shadow_init(void) {}
void rng_shadow_tick_10ms(uint8_t os_mode, uint8_t g98_fail) { (void)os_mode; (void)g98_fail; }
#else

#define RS_SECT __attribute__((section(".rng_shadow")))

RS_SECT rng_shadow_cells_t s_rs;
RS_SECT rng_shadow_wit_t   s_rs_wit;
RS_SECT rng_shadow_rec_t   s_rs_rec[RNG_SHADOW_REC_N];

/* ══ stand-ins: the stock driving dump stockram_driving_20261104.bin (P, no
 * lever, engine off, this bench), D61A+off. D672 / D673 come from the lever
 * evaluation instead (rs_gather). ═══════════════════════════════════════ */
#define SI_D61B 0x00u  /* D61A+0x01 cal-override enable (0xC709C, 0xC79B8, ..) */
#define SI_D61C 0x00u  /* D61A+0x02 (0xC78BE) */
#define SI_D61D 0x00u  /* D61A+0x03 (0xC39D4) */
#define SI_D61E 0x00u  /* D61A+0x04 (0xC39D4) */
#define SI_D627 0x00u  /* D61A+0x0D (0xA52A2, 0xC3C46) */
#define SI_D62F 0x00u  /* D61A+0x15 (0xC2812, 0xC286C, 0xC7xxx candidates) */
#define SI_D631 0x02u  /* D61A+0x17 (0xA90A0) */
#define SI_D63A 0x00u  /* D61A+0x20 word hi (0xC3716) */
#define SI_D63B 0x00u  /* D61A+0x21 word lo */
#define SI_D63C 0x02u  /* D61A+0x22 (0xC3716, 0xC74F4) */
#define SI_D63E 0x00u  /* D61A+0x24 word hi (0xC3716, 0xC74F4) */
#define SI_D63F 0x00u  /* D61A+0x25 word lo */
#define SI_D640 0x00u  /* D61A+0x26 (0xC3716, 0xC74F4) */
#define SI_D641 0x02u  /* D61A+0x27 CAN 0x112 → D5F2 (0xA8F5C) */
#define SI_D642 0x01u  /* D61A+0x28 (0xA8F5C) */
#define SI_D643 0x00u  /* D61A+0x29 (0xA9668, 0xC3B88) */
#define SI_D644 0x00u  /* D61A+0x2A (0xA9668, 0xC3BF4) */
#define SI_D648 0x03u  /* D61A+0x2E safety park request (0xA9126, 0xC7258, 0xC733A) */
#define SI_D649 0x00u  /* D61A+0x2F (0xA52A2) */
#define SI_D64A 0x00u  /* D61A+0x30 (0xA52A2) */
#define SI_D64B 0x00u  /* D61A+0x31 (0xA90A0) */
#define SI_D64C 0x00u  /* D61A+0x32 word hi (0xA90A0) */
#define SI_D64D 0x00u  /* D61A+0x33 word lo */
#define SI_D64E 0x02u  /* D61A+0x34 (0xA90A0) */
#define SI_D650 0x00u  /* D61A+0x36 word hi: speed → D5F6 (0xA9086) */
#define SI_D651 0x00u  /* D61A+0x37 word lo */
#define SI_D652 0x02u  /* D61A+0x38 qualifier (0xA9126, 0xC7258, 0xC733A) */
#define SI_D653 0x20u  /* D61A+0x39 (0xC74F4) */
#define SI_D654 0x00u  /* D61A+0x3A → D5F9 (0xA908E) */
#define SI_D656 0x01u  /* D61A+0x3C ← 0xFFF9582E (0x1420B0): clears the U1267 latch at 0 (0xA6A4C) */
#define SI_D65A 0x32u  /* D61A+0x40 word hi (0xC3BE4) */
#define SI_D65B 0x8Bu  /* D61A+0x41 word lo */
#define SI_D65C 0x00u  /* D61A+0x42 (0xA9538, 0xA9594, 0xA9616) */
#define SI_D65F 0x00u  /* D61A+0x45 (0xC7630, 0xC77A0) */
#define SI_D660 0x01u  /* D61A+0x46 fault id 53 (0xC77A0, 0xC3BCA) */
#define SI_D661 0x01u  /* D61A+0x47 (0xC6502) */
#define SI_D664 0x01u  /* D61A+0x4A fault id 59 (0xC7756) */
#define SI_D665 0x00u  /* D61A+0x4B fault id 60 (0xC767C, 0xC76D8) */
#define SI_D666 0x01u  /* D61A+0x4C fault id 61 (0xC767C, 0xC76D8) */
#define SI_D669 0x01u  /* D61A+0x4F (0xC3B94) */
#define SI_D671 0x00u  /* D61A+0x57 (0xC7DA4) */
#define SI_D675 0x00u  /* D61A+0x5B (0xC842E) */
#define SI_D676 0x00u  /* D61A+0x5C → D60A (0xA9748) */
#define SI_D677 0x0Bu  /* D61A+0x5D (0xA5884) */
#define SI_D67A 0x03u  /* D61A+0x60 word hi (0xA5884) */
#define SI_D67B 0x48u  /* D61A+0x61 word lo */
#define SI_D681 0x00u  /* D61A+0x67 → E541 (0xC7DC6) */
#define SI_D682 0x00u  /* D61A+0x68 word hi (0xC3A40) */
#define SI_D683 0x00u  /* D61A+0x69 word lo */
#define SI_D68C 0x00u  /* D61A+0x72 (0xC848E) */
#define SI_D68D 0x02u  /* D61A+0x73 (0xC848E) */
#define SI_D68E 0x00u  /* D61A+0x74 (0xC7110) */
#define SI_D68F 0x00u  /* D61A+0x75 (0xC71BE) */
#define SI_D692 0xFFu  /* D61A+0x78 word hi (0xC3D8A) */
#define SI_D693 0xFFu  /* D61A+0x79 word lo */
/* outside the gathered struct */
#define SI_D703  0x00u  /* 0xFFF8D703 (0xC84E6) */
#define SI_89444 0x00u  /* 0xFFF89444 sub-object +0xC enable (0xC5F14, not ported) */
/* 0x89434 filter enables as the dump holds them, used when an enable
 * reaches the coding-mask virtual call (0xC8650, not ported):
 * +0x58 0x89490, +0x40 0x89478, +0x4C 0x89484, +0x34 0x8946C, +0 0x89438,
 * +0x70 0x894A8, +0x64 0x8949C */
static const uint8_t s_flt_en_dump[7] = { 0u, 0u, 0u, 0u, 1u, 0u, 0u };

/* the whole gathered struct D61A..D699 as the dump holds it (the bytes not
 * named above are read by no ported function) */
static const uint8_t s_d61a_dump[0x80] = {
    0x00u, SI_D61B, SI_D61C, SI_D61D, SI_D61E, 0x00u, 0x04u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, SI_D627, 0x00u, 0x00u,
    0x02u, 0x00u, 0x00u, 0x00u, 0x02u, SI_D62F, 0x01u, SI_D631,
    0x00u, 0x43u, 0x00u, 0x4Cu, 0x00u, 0x43u, 0x00u, 0x00u,
    SI_D63A, SI_D63B, SI_D63C, 0x00u, SI_D63E, SI_D63F, SI_D640, SI_D641,
    SI_D642, SI_D643, SI_D644, 0x00u, 0x00u, 0x00u, SI_D648, SI_D649,
    SI_D64A, SI_D64B, SI_D64C, SI_D64D, SI_D64E, 0x00u, SI_D650, SI_D651,
    SI_D652, SI_D653, SI_D654, 0x00u, SI_D656, 0x00u, 0x32u, 0x66u,
    SI_D65A, SI_D65B, SI_D65C, 0x00u, 0x00u, SI_D65F, SI_D660, SI_D661,
    0x00u, 0x01u, SI_D664, SI_D665, SI_D666, 0x01u, 0x00u, SI_D669,
    0x01u, 0x00u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u, SI_D671,
    0x00u, 0x02u, 0x03u, SI_D675, SI_D676, SI_D677, 0x00u, 0x00u,
    SI_D67A, SI_D67B, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, SI_D681,
    SI_D682, SI_D683, 0x00u, 0x00u, 0x00u, 0x1Du, 0x00u, 0x00u,
    0x00u, 0x00u, SI_D68C, SI_D68D, SI_D68E, SI_D68F, 0x00u, 0x00u,
    SI_D692, SI_D693, 0x00u, 0x00u, 0x03u, 0x03u, 0x01u, 0x03u,
};
/* the stand-ins the decode's prediction rests on (dump bytes, re-read) */
_Static_assert(SI_D641 == 0x02u && SI_D660 == 0x01u && SI_D664 == 0x01u &&
               SI_D665 == 0x00u && SI_D666 == 0x01u && SI_D62F == 0x00u &&
               SI_D65F == 0x00u && SI_D61B == 0x00u && SI_D650 == 0x00u &&
               SI_D651 == 0x00u && SI_D654 == 0x00u && SI_D63C == 0x02u &&
               SI_D640 == 0x00u && SI_D661 == 0x01u && SI_D681 == 0x00u &&
               SI_D656 == 0x01u && SI_D65A == 0x32u && SI_D65B == 0x8Bu,
               "range shadow stand-ins are the stock driving dump's bytes");
/* the live inputs reproduce these bytes exactly when their signal is absent
 * (rs_live_inputs): brake invalid → 02 / 00, 0x108 invalid → E912 = EF0A
 * (D63E, 0000; EF0C = D640 = 00) / 02, 0x14B invalid → 0000 / 02 */
_Static_assert(SI_D631 == 0x02u && SI_D64B == 0x00u && SI_D63A == 0x00u &&
               SI_D63B == 0x00u && SI_D63C == 0x02u && SI_D63E == 0x00u &&
               SI_D63F == 0x00u && SI_D64C == 0x00u && SI_D64D == 0x00u &&
               SI_D64E == 0x02u,
               "absent CAN inputs map to the dump's gathered bytes");
/* the dump's reaction bits are what the U1267 + P0850 model gives on this
 * bench (U1267 failed, P0850 failed): D660 = D661 = D664 = D666 = 1, D665 0 */
_Static_assert(SI_D660 == 1u && SI_D661 == 1u && SI_D664 == 1u && SI_D665 == 0u &&
               SI_D666 == 1u, "reaction bits: the dump = U1267 + P0850 failed");


/* ══ calibration: the normal variant (0x9B2AC → 0xA70AA; dump: every cal
 * pointer cell holds a 0x4Exxx/0x4Fxxx struct), values read from the image
 * at the cal address given ═════════════════════════════════════════════ */
/* *0xFFF84A8C (0x4EC78) +0 → 0x195E05: bytes +9/+0xA/+0xB/+0xE */
#define CAL_D6DE_EN9   0x01u    /* 0x195E0E 0xC2776 */
#define CAL_D6DE_ENA   0x00u    /* 0x195E0F 0xC27C4 */
#define CAL_D6DE_ENB   0x01u    /* 0x195E10 0xC2812 */
#define CAL_D6DE_ENE   0x01u    /* 0x195E13 0xC286C */
/* *0xFFF84A90 (0x4EC7C) */
#define CAL_R_TIMER    0x1Eu    /* +0 → 0x195FCE 0xC2A40 */
#define CAL_8941A_OVR  0xFFu    /* +4 → 0x195FCF 0xC2C04 (D61B-gated) */
/* *0xFFF84A9C */
#define CAL_E546_T     0xBEu    /* +0 → 0x195FD0 0xC365A */
#define CAL_E546_EN    0x01u    /* +4 → 0x195FD1 0xC361A */
/* *0xFFF84AA4 (0x4ECC4) 893E0 FSM */
#define CAL_FSM_0      0x017Cu  /* +0  → 0x191FCC */
#define CAL_FSM_4      0x1F40u  /* +4  → 0x191FCE */
#define CAL_FSM_8      0x017Cu  /* +8  → 0x191FD0 */
#define CAL_FSM_C      0x01AEu  /* +12 → 0x191FD2 */
#define CAL_FSM_10     0x0019u  /* +16 → 0x191FD4 */
#define CAL_FSM_14     0x01u    /* +20 → 0x191FD6 */
/* *0xFFF84AAC 8942C module */
#define CAL_8942C_T0   0x00000000u  /* +0  → 0x191FDC u32 */
#define CAL_8942C_C    0x0BD6u      /* +12 → 0x191FE8 */
#define CAL_8942C_10   0x0B9Au      /* +16 → 0x191FEA */
#define CAL_8942C_14   0x015Eu      /* +20 → 0x191FEC */
#define CAL_8942C_18   0x00C8u      /* +24 → 0x191FEE */
#define CAL_8942C_24   0x0Eu        /* +36 → 0x191FF4 */
_Static_assert(CAL_8942C_T0 == 0u, "0xC3B50: state 0 never leaves with this calibration");
/* *0xFFF84AB0 (0x4ED14) */
#define CAL_AB0_4      0x0050u  /* +4  → 0x195FD6 */
#define CAL_AB0_10     0x00u    /* +16 → 0x195FDC */
#define CAL_AB0_14     0x01u    /* +20 → 0x195FDD */
#define CAL_304C_OVR   0xFFu    /* +24 → 0x195FDE (0xC79B8, D61B-gated) */
/* *0xFFF84AF0 (0x4F03C) D6CA module */
#define CAL_AF0_0      0xFFFFu  /* +0  → 0x1960BA */
#define CAL_AF0_4      0xFFFFu  /* +4  → 0x1960BC */
#define CAL_AF0_8E     0xFFFFu  /* +8 [E530 = 0] → 0x1960BE */
#define CAL_AF0_10     0x0064u  /* +16 → 0x1960C2 */
#define CAL_AF0_14     0x01FFu  /* +20 → 0x1960C4 */
#define CAL_AF0_1C     0xFFu    /* +28 → 0x1960C7 (D61B-gated) */
#define CAL_AF0_20     0x01u    /* +32 → 0x1960C8 */
#define CAL_AF0_24     0x00u    /* +36 → 0x1960C9 */
#define CAL_AF0_28     0x01u    /* +40 → 0x1960CA */
#define CAL_AF0_2C     0x01u    /* +44 → 0x1960CB */
#define CAL_AF0_30     0x01u    /* +48 → 0x1960CC */
#define CAL_AF0_34     0x01u    /* +52 → 0x1960CD */
#define CAL_AF0_38     0x00u    /* +56 → 0x1960CE */
/* *0xFFF84AF8 (0x4F088) D6D4 module */
#define CAL_AF8_0      0x0000u  /* +0  → 0x192090 */
static const uint16_t s_cal_af8_0 = CAL_AF8_0;  /* (a variable: the compare stays as the bytes have it) */
#define CAL_AF8_4      0x00u    /* +4  → 0x192092 */
#define CAL_AF8_8      0x04u    /* +8  → 0x192093 */
#define CAL_AF8_C      0x00u    /* +12 → 0x192094 */
#define CAL_AF8_10     0x04u    /* +16 → 0x192095 */
#define CAL_AF8_14     0x64u    /* +20 → 0x192096 */
#define CAL_AF8_18     0x01u    /* +24 → 0x192097 candidate chain enable */
#define CAL_AF8_1C     0x01u    /* +28 → 0x192098 */
#define CAL_AF8_20     0x00u    /* +32 → 0x192099 */
#define CAL_AF8_24     0x01u    /* +36 → 0x19209A */
#define CAL_AF8_28     0x01u    /* +40 → 0x19209B */
/* +52 → 0x19209E: ten bytes 01 (0xC74F4) */
static const uint8_t s_cal_af8_34[10] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
#define CAL_E545_OVR   0xFFu    /* *0xFFF84AFC +0 → 0x195E8E (0xC7CCA, D61B-gated) */
#define CAL_E53C_OVR   0xFFu    /* *0xFFF84B08 +0 → 0x195E90 (0xC7FDA, D61B-gated) */
/* *0xFFF84B10 (0x4F100) */
#define CAL_B10_20E    0x0010u  /* +0x20 [E530 = 0] → 0x1920C6 (0xC8578) */
#define CAL_B10_14     0x0028u  /* +20 → 0x1920C0 (0xC8578) */
#define CAL_B10_C      0x01AEu  /* +12 → 0x1920BC (0xC842E) */
#define CAL_B10_10     0x0FA0u  /* +16 → 0x1920BE (0xC823E) */
/* *0xFFF84B14 (0x4F134) */
#define CAL_B14_88     0x01u    /* +0x88 → 0x196160 (0xA8F5C) */
#define CAL_B14_10     0x000Au  /* +16 → 0x196130 (0xA90A0) */
#define CAL_B14_1C     0x0140u  /* +28 → 0x196136 (0xC3832) */
#define CAL_B14_20     0x0020u  /* +32 → 0x196138 */
#define CAL_B14_24     0x0050u  /* +36 → 0x19613A */
#define CAL_B14_28     0x0080u  /* +40 → 0x19613C */
#define CAL_B14_2C     0xFFF0u  /* +44 → 0x19613E */
#define CAL_B14_30     0x0008u  /* +48 → 0x196140 */
#define CAL_B14_34     0x0050u  /* +52 → 0x196142 */
#define CAL_B14_38     0x0020u  /* +56 → 0x196144 */
#define CAL_B14_3C     0xFFF0u  /* +60 → 0x196146 */
#define CAL_B14_40     0x0020u  /* +64 → 0x196148 */
#define CAL_B14_44     0x0050u  /* +68 → 0x19614A */
#define CAL_B14_48E    0x0080u  /* +72 [E530 = 0] → 0x19614C */
#define CAL_B14_50E    0x0140u  /* +80 [E530 = 0] → 0x196150 */
#define CAL_AE0_0E     0x00FAu  /* *0xFFF84AE0 +0 [E530 = 0] → 0x19606C (0xC829E) */
/* *0xFFF84AE4 (0x4F010) the U1267 monitor 0xA6AB0 */
#define CAL_U1267_N    0x0032u  /* +0 → 0x192088: D60C+0 must exceed 50 (10 ms passes) */
#define CAL_U1267_MV   0x1B58u  /* +8 → 0x19208C: supply D65A must exceed 7000 mV */
#define U1267_GROUP    109u     /* *0x4B8D4 = 0x6324E → 0x6D */
#define U1267_SOURCE   1u       /* *0x4B924 = 0x632E7 → 0x01 */
/* the 0x14B speed object 0xFFF8E8B0: 0x154918(raw, prev, *0x195D74, *0x195D75)
 * = (raw * w1 + prev * w2 [+ w2]) / (w1 + w2) — the identity at 01 / 00 */
#define CAL_14B_W1     0x01u    /* 0x195D74 (cal pointer 0x642A8 +0) */
#define CAL_14B_W2     0x00u    /* 0x195D75 (cal pointer 0x642A8 +4) */
_Static_assert(CAL_14B_W1 == 1u && CAL_14B_W2 == 0u, "0x154918 is the identity at this calibration");

/* 0x15BB28: the filter tables, written as constants at init (GBR =
 * 0xFFF8E64C; the dump holds the same bytes). [row = D6D6 8/7/6/5]
 * [col = the request 8/7/6/5]; 0xFF = no change */
static const uint8_t s_flt_tabs[4][16] = {
    /* 0xFFF8E64C */ { 0xFFu, 0x00u, 0x00u, 0x00u, 0xFFu, 0xFFu, 0xFFu, 0xFFu,
                       0xFFu, 0x00u, 0xFFu, 0x00u, 0xFFu, 0xFFu, 0xFFu, 0xFFu },
    /* 0xFFF8E65D */ { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu,
                       0xFFu, 0x00u, 0xFFu, 0xFFu, 0xFFu, 0x06u, 0xFFu, 0xFFu },
    /* 0xFFF8E66E */ { 0xFFu, 0x00u, 0x00u, 0x00u, 0xFFu, 0xFFu, 0xFFu, 0xFFu,
                       0xFFu, 0x00u, 0xFFu, 0x00u, 0xFFu, 0x06u, 0xFFu, 0xFFu },
    /* 0xFFF8E67F */ { 0xFFu, 0x00u, 0x00u, 0x00u, 0xFFu, 0x06u, 0xFFu, 0x06u,
                       0xFFu, 0x00u, 0x00u, 0x00u, 0xFFu, 0x06u, 0xFFu, 0x06u },
};
enum { T_E64C = 0, T_E65D = 1, T_E66E = 2, T_E67F = 3 };

/* ══ cell access, named by stock address ═════════════════════════════ */
#define R              s_rs
#define D5(a)          R.d5e6[(a) - 0xD5E6u]
#define D6(a)          R.d61a[(a) - 0xD61Au]
#define E5(a)          R.e530[(a) - 0xE530u]
#define CA(a)          R.d6ca[(a) - 0xD6CAu]
#define D4(a)          R.d6d4[(a) - 0xD6D4u]
#define F3(a)          R.x893e0[(a) - 0x893E0u]
#define X18(a)         R.x89418[(a) - 0x89418u]
#define X20(a)         R.x89420[(a) - 0x89420u]
#define D6D6           D4(0xD6D6u)
#define D6DE           R.d6de[0]
#define D6DF           R.d6de[1]

static uint16_t w61a(uint32_t a)            /* big-endian word of D61A */
{
    return (uint16_t)((uint16_t)D6(a) << 8 | D6(a + 1u));
}

/* the getters used by the chain (0xA7700..0xA7C9C); each a plain byte read */
#define G_D5F2  D5(0xD5F2u)     /* 0xA772A */
#define G_D5F3  D5(0xD5F3u)     /* 0xA7796 */
#define G_D5F5  D5(0xD5F5u)     /* 0xA7782 */
#define G_D5F8  D5(0xD5F8u)     /* 0xA7716 */
#define G_D5F9  D5(0xD5F9u)     /* 0xA7720 */
#define G_D5FA  D5(0xD5FAu)     /* 0xA7746 */
#define G_D5FB  D5(0xD5FBu)     /* 0xA7778 */
#define G_D603  D5(0xD603u)     /* 0xA775A */
#define G_D604  D5(0xD604u)     /* 0xA776E */
#define G_D605  D5(0xD605u)     /* 0xA7764 */
#define G_D606  D5(0xD606u)     /* 0xA770C */
#define G_D607  D5(0xD607u)     /* 0xA773C */
#define G_D608  D5(0xD608u)     /* 0xA7750 */
#define G_D609  D5(0xD609u)     /* 0xA7702 */
#define G_D60A  D5(0xD60Au)     /* 0xA77AA */
#define G_D672  D6(0xD672u)     /* 0xA7820 */
#define G_D673  D6(0xD673u)     /* 0xA7848 */
#define G_E531  E5(0xE531u)     /* 0xA782A */
#define G_E53C  E5(0xE53Cu)     /* 0xA79FE */
#define G_E541  E5(0xE541u)     /* 0xA7998 */
#define G_E545  E5(0xE545u)     /* 0xA7C78 */
#define G_E54A  E5(0xE54Au)     /* 0xA7C6E */
#define G_893E8 F3(0x893E8u)    /* 0xA76CE */
#define G_893E9 F3(0x893E9u)    /* 0xA76D8 */
#define G_893EA F3(0x893EAu)    /* 0xA76C4 */
#define G_893F7 F3(0x893F7u)    /* 0xA76E2 */
#define G_8941A X18(0x8941Au)   /* 0xA798E */
#define G_D6D9  D4(0xD6D9u)     /* 0xA7B96 */
#define G_D6CE  CA(0xD6CEu)     /* 0xA7966 */

/* 0xA5204: 304C+0x10 with its complement, 0 when torn */
static uint8_t g_304c_10(void)
{
    return (uint8_t)(R.o304c[4] == (uint32_t)~R.o304c[5] ? R.o304c[4] : 0u);
}

/* ══ 0x9B306 ═════════════════════════════════════════════════════════ */
/* the lever (D672 ← 0xFFF8E964, D673 ← 0xFFF8E95D): app/park.c's lever
 * request (CAN type-1 evaluation, 5..8, 0 when no valid source) and the
 * source status (0/1 = the source in use, 2 = no valid source) */
static void rs_lever(uint8_t *lev, uint8_t *src)
{
    const vehicle_in_t *v = vehicle_in();

    *lev = 0u;
    *src = 2u;
    if (v->q_lever != Q_OK) {
        return;
    }
    switch (v->lever) {
    case RANGE_D: *lev = 5u; break;
    case RANGE_N: *lev = 6u; break;
    case RANGE_R: *lev = 7u; break;
    case RANGE_P: *lev = 8u; break;
    default:      return;
    }
    *src = v->lever_src == LEVER_SRC_FALLBACK ? 1u : 0u;
}

#if defined(RNG_SHADOW_TEST_HOOKS)
#if defined(__sh__)
#error "RNG_SHADOW_TEST_HOOKS (input overrides) is host-only: the SH shadow runs on the dump stand-ins"
#endif
static uint8_t s_ovr_on[0x80], s_ovr_val[0x80];

void rng_shadow_test_override(uint8_t off, uint8_t val)
{
    if (off < 0x80u) {
        s_ovr_on[off] = 1u;
        s_ovr_val[off] = val;
    }
}

void rng_shadow_test_clear(void)
{
    memset(s_ovr_on, 0, sizeof s_ovr_on);
}
#endif

/* the vehicle evaluation → the gathered CAN-side bytes, with the stock
 * conditioning's status encoding (0 = valid, 2 = invalid); "valid" = our
 * receive layer's Q_OK (CRC, counter, 50 ms timeout, range) standing in for
 * the stock's raw status byte == 0 [I: the stock's raw-status producers are
 * not decoded] */
static void rs_live_inputs(void)
{
    const vehicle_in_t *v = vehicle_in();
    uint16_t w;

    /* 0x135DC6, brake object 0xFFF8E898 (+4 raw, +10 raw status): status 0
     * and raw < 2 → E8A4 = 0, E8A5 = (raw == 1); otherwise E8A4 = 2,
     * E8A5 = 0 (0x135E3A..0x135E92, and 0x135F54 writes the same) [O].
     * Getters 0x135D64 (E8A4 → D631), 0x135D6C (E8A5 → D64B) [O decode]. */
    if (v->q_brake == Q_OK) {           /* received and raw (byte 0 bits 3..2) <= 1 */
        D6(0xD631u) = 0u;
        D6(0xD64Bu) = (uint8_t)(v->brake == BRAKE_ON ? 1u : 0u);
    } else {
        D6(0xD631u) = 2u;
        D6(0xD64Bu) = 0u;
    }
    /* 0x13BF66, engine object 0xFFF8E904 (+4 raw word, +8 raw status):
     * status 0 and raw != 0xFFFF → E910 = 0, E90E = raw; else E910 = 2.
     * E912 = E90E when E910 != 2; when E910 == 2, E912 = EF0A if EF0C == 0,
     * else E912 is held; E916 = E910 (0x13BF72..0x13BFB8) [O]. Getters
     * 0x13BF32 (E912 → D63A), 0x13BF3E (E916 → D63C). Our Q_OK also refuses
     * raw 0x8000..0xFFFE, which the stock would pass [deviation, I: never
     * seen]. EF0A / EF0C are the gathered D63E / D640 (dump stand-ins). */
    if (v->q_engine_speed == Q_OK) {
        R.e912 = (uint16_t)v->engine_speed;
        D6(0xD63Cu) = 0u;
    } else {
        if (D6(0xD640u) == 0u) {
            R.e912 = w61a(0xD63Eu);
        }
        D6(0xD63Cu) = 2u;
    }
    D6(0xD63Au) = (uint8_t)(R.e912 >> 8);
    D6(0xD63Bu) = (uint8_t)R.e912;
    /* 0x13608C, the 0x14B object 0xFFF8E8B0 (+4 raw word, +8 raw status):
     * status 0 and raw != 0x7FF → E8BE = 0, +18 = 0x154918(raw, +18, 01,
     * 00) = raw, E8BA = +18 / 5 (divs); else E8BE = 2, E8BA = 0 [O].
     * Getters 0x13603C (E8BA → D64C), 0x136048 (E8BE → D64E). */
    if (v->q_park_speed == Q_OK) {      /* received and raw != 0x7FF */
        w = (uint16_t)(v->park_speed_raw / 5u);
        D6(0xD64Eu) = 0u;
    } else {
        w = 0u;
        D6(0xD64Eu) = 2u;
    }
    D6(0xD64Cu) = (uint8_t)(w >> 8);
    D6(0xD64Du) = (uint8_t)w;
    /* 0xA031C(id) over the masks rebuilt from the stored groups (0xAAD76):
     * group 109 (U1267) carries ids 6, 9, 12, 16, 36, 53, 59, 61, 63, group
     * 98 (P0850) ids 36, 53 [O config 0x19331C / 0x193190]; the dump's other
     * stored groups (28: 9, 40; 136: 13, 23, 34, 36, 49, 50, 62, 63) carry
     * none of 16 / 53 / 59 / 60 / 61, and 897D4 / DC / E0 / E4 / 89810 / 14
     * are 0 [O dump] — so on this bench these five bytes are exactly: */
    D6(0xD660u) = (uint8_t)(R.g109 != 0u || R.g98 != 0u);   /* id 53 (*0x4B8F0 = 0x35) */
    D6(0xD661u) = R.g109;                                   /* id 16 (*0x4B8CC = 0x16) */
    D6(0xD664u) = R.g109;                                   /* id 59 (*0x4B8FC = 0x3B) */
    D6(0xD665u) = 0u;                                       /* id 60: no group here */
    D6(0xD666u) = R.g109;                                   /* id 61 (*0x4B904 = 0x3D) */
    /* D65F (D61A+0x45) = group 113 (P1B14) status via 0xA0FA0: not ported,
     * the dump's 00 stays */
}

/* 0xA7C9C: the gathered struct = the dump, the lever and the CAN-side
 * inputs live, the reaction bits from the shadow's fault model */
static void rs_gather(void)
{
    uint8_t lev, src;

    memcpy(R.d61a, s_d61a_dump, sizeof R.d61a);
    rs_lever(&lev, &src);
    D6(0xD672u) = lev;
    D6(0xD673u) = src;
    rs_live_inputs();
#if defined(RNG_SHADOW_TEST_HOOKS)
    for (uint8_t k = 0u; k < 0x80u; k++) {
        if (s_ovr_on[k] != 0u) {
            R.d61a[k] = s_ovr_val[k];
        }
    }
#endif
}

/* 0xA5884 (0xA5884..0xA58B0) */
static uint8_t a5884(void)
{
    const uint8_t c = D6(0xD677u);

    return (uint8_t)(w61a(0xD67Au) == 0x9Cu && (c == 11u || c == 6u || c == 7u));
}

/* 0xA8F5C (0xA8F5C..0xA902C): D5F2..D5F5, E572 */
static void a8f5c(void)
{
    uint8_t r5, r0;
    const uint8_t d641 = D6(0xD641u);

    if (R.f8975c == 0u) {
        R.e572 = D5(0xD5F2u);
        R.f8975c = 1u;
    }
    r5 = (uint8_t)(!(d641 >= 1u && d641 <= 4u));
    if (D6(0xD642u) == 0u && r5 == 0u) {
        r0 = 0u;
    } else if (D6(0xD642u) == 1u && r5 == 0u && CAL_B14_88 == 1u) {
        r0 = 1u;
    } else {
        r0 = 2u;
    }
    D5(0xD5F3u) = r0;
    if (r0 <= 1u || G_893E8 >= 1u) {
        r0 = (uint8_t)(d641 == 1u || d641 == 2u || d641 == 3u);
    } else {
        r0 = 0u;
    }
    D5(0xD5F2u) = r0;
    D5(0xD5F4u) = (uint8_t)(r0 == 1u && R.e572 == 0u);
    D5(0xD5F5u) = (uint8_t)(r0 == 0u && R.e572 == 1u);
    R.e572 = r0;
}

/* ══ U1267 (group 109): 0xA6A3E (10 ms, via 0xA6A2C(D60C) in 0xA5944) ═══ */
static void a6a3e(void)                     /* 0xA6A3E..0xA6AAE */
{
    if (G_D5F2 == 1u) {                     /* 0xA6A40: D5E6+12 */
        R.p85160 = 1u;
    }
    if (D6(0xD656u) == 0u) {                /* 0xA6A4C */
        R.p85160 = 0u;
    }
    if (R.p85160 != 0u) {                   /* 0xA6A5A */
        if (R.d60c_0 == 0xFFFFu) {
            R.d60c_2 = 0u;                  /* bt.s with mov #0, stored at 0xA6A7E */
        } else {
            R.d60c_0++;
            R.d60c_2 = (uint8_t)(R.d60c_0 == 0xFFFFu);
        }
    } else {
        R.d60c_0 = 0u;                      /* 0xA6A80 (+2 not written) */
    }
    if (G_D5F2 == 1u) {                     /* 0xA6A84 */
        if (R.d60c_4 == 0xFFFFu) {
            R.d60c_6 = 0u;
        } else {
            R.d60c_4++;
            R.d60c_6 = (uint8_t)(R.d60c_4 == 0xFFFFu);
        }
    } else {
        R.d60c_4 = 0u;                      /* 0xA6AA6 (+6 not written) */
    }
}

/* our stand-in for the fault manager 0xA0E70 → 0x9BE50 → the record's
 * status bit 4 → 0xAAD76: failed AND tested latches the group for the rest
 * of the power cycle (no debounce, no healing — neither decoded) [I] */
static void fm_report_109(uint8_t failed, uint8_t tested)
{
    R.u_failed = failed;
    R.u_tested = tested;
    R.u_rep_n++;
    if (failed != 0u && tested != 0u) {
        R.u_fail_n++;
        if (R.g109 == 0u) {
            R.g109 = 1u;
            R.g109_ms = sched_now_ms();
        }
    } else if (tested != 0u) {
        R.u_pass_n++;                       /* 0x9BF20's pass path: healing undecoded */
    }
}

/* 0xA6AB0 (20 ms, via 0xA6A3A in 0x9B39E): 0xA6AB0..0xA6B04 */
static void a6ab0(void)
{
    /* mov.w (sign-extending) loads, cmp/hs (unsigned) compares [O] */
    const uint32_t n = (uint32_t)(int32_t)(int16_t)CAL_U1267_N;
    const uint32_t c = (uint32_t)(int32_t)(int16_t)R.d60c_0;
    const uint32_t mv = (uint32_t)(int32_t)(int16_t)CAL_U1267_MV;
    const uint32_t sup = (uint32_t)(int32_t)(int16_t)w61a(0xD65Au);
    const uint8_t failed = (uint8_t)(G_D673 == 2u);      /* 0xA6AC2: getter 0xA7848 */
    uint8_t tested;

    tested = (uint8_t)(!(n >= c) && !(mv >= sup));       /* 0xA6ACE / 0xA6ADE */
    (void)U1267_GROUP;
    (void)U1267_SOURCE;
    fm_report_109(failed, tested);          /* jmp *0x4B888 = 0xA0E70(109, 1, r6, r7) */
}

/* ══ 0x893E0 FSM: 0xC36FC (0xC3716, 0xC39D4, 0xC37BE, 0xC3832, 0xC3900) ═ */
static void c3716(void)
{
    uint8_t r0;

    if (D6(0xD63Cu) == 0u) {
        R.x893e0_c = w61a(0xD63Au);
        r0 = 0u;
    } else if (D6D6 == 8u) {
        if (D6(0xD640u) == 0u) {
            R.x893e0_c = w61a(0xD63Eu);
            r0 = 1u;
        } else {
            r0 = 2u;
        }
    } else if (D6D6 == 6u && D6(0xD640u) == 0u && D5(0xD5F9u) == 0u) {
        R.x893e0_c = w61a(0xD63Eu);
        r0 = 1u;
    } else {
        r0 = 2u;
    }
    F3(0x893EAu) = r0;
    if (R.x893e0_t0 != 0u) {
        R.x893e0_t0--;
        F3(0x893E2u) = (uint8_t)(R.x893e0_t0 == 0u);
    } else {
        F3(0x893E2u) = 0u;
    }
    if (R.x893e0_c > CAL_FSM_C) {
        goto latch;
    }
    if ((int32_t)R.x893e0_c > (int32_t)CAL_FSM_8) {
        if (R.x893e0_e == 0u) {
            return;
        }
        goto latch;
    }
    if (R.x893e0_t0 > 0u) {
        return;
    }
    R.x893e0_e = 0u;
    return;
latch:
    R.x893e0_e = R.x893e0_c;
    R.x893e0_t0 = CAL_FSM_10;
    F3(0x893E2u) = 0u;
}

static void c37be(void)
{
    if (R.x893e0_t4 != 0u) {
        R.x893e0_t4--;
        F3(0x893E6u) = (uint8_t)(R.x893e0_t4 == 0u);
    } else {
        F3(0x893E6u) = 0u;
    }
    if (R.x893e0_c > CAL_FSM_8) {
        if ((int32_t)R.x893e0_c > (int32_t)CAL_FSM_4 || R.x893e0_10 != 0u) {
            R.x893e0_10 = R.x893e0_c;
            R.x893e0_t4 = CAL_FSM_10;
            F3(0x893E6u) = 0u;
        } else {
            R.x893e0_t4 = 0u;
        }
    } else if (R.x893e0_t4 == 0u) {
        R.x893e0_10 = 0u;
    }
    F3(0x893F2u) = (uint8_t)(F3(0x893EAu) != 2u && R.x893e0_10 > 0u);
}

static void c3832(void)
{
    const uint8_t a = F3(0x893EAu);
    const uint8_t r1 = (uint8_t)(a != 2u && R.x893e0_e > 0u);
    const uint8_t r7 = (uint8_t)(a == 2u && D5(0xD5F2u) == 1u && D5(0xD5F3u) != 2u);
    uint8_t r4 = 0u, r2;
    const uint16_t d5f6 = (uint16_t)((uint16_t)D5(0xD5F6u) << 8 | D5(0xD5F7u));

    if (a == 2u && (D5(0xD5F2u) == 0u || D5(0xD5F3u) == 2u) && d5f6 > CAL_B14_1C) {
        r4 = 1u;
    }
    F3(0x893F4u) = F3(0x893F3u);
    r2 = (uint8_t)(r1 == 1u || r7 == 1u || r4 == 1u);
    F3(0x893F3u) = r2;
    F3(0x893F5u) = (uint8_t)(r2 == 1u || F3(0x893F4u) == 0u);
    F3(0x893F6u) = (uint8_t)(r1 == 0u && (r7 == 1u || r4 == 1u) && G_E54A == 5u &&
                             CAL_FSM_14 == 1u);
}

static void c39d4(void)
{
    F3(0x893F7u) = (uint8_t)(D6(0xD61Du) == 1u && D6(0xD61Eu) == 0u &&
                             D5(0xD5F2u) == 1u && D5(0xD5F3u) != 2u);
}

static void c3900(void)
{
    uint8_t s = F3(0x893E8u);

    F3(0x893E9u) = s;
    switch (s) {
    case 0u:
        if (R.x893e0_c > CAL_FSM_C && F3(0x893EAu) != 2u) {
            s = 1u;
            break;
        }
        if (F3(0x893F6u) == 1u) {
            s = 1u;
            break;
        }
        if (F3(0x893F2u) != 1u || F3(0x893F3u) != 0u) {
            return;
        }
        s = 3u;
        break;
    case 1u:
        if (F3(0x893F3u) == 0u && F3(0x893F7u) == 0u) {
            s = 0u;
            break;
        }
        if (F3(0x893F7u) != 1u || R.x893e0_c > CAL_FSM_0) {
            return;
        }
        s = 2u;
        break;
    case 2u:
        if (R.x893e0_c > CAL_FSM_C && F3(0x893EAu) != 2u && F3(0x893F7u) == 1u) {
            s = 1u;
            break;
        }
        if (F3(0x893F7u) != 0u) {
            return;
        }
        s = 0u;
        break;
    case 3u:
        if (F3(0x893F3u) == 1u) {
            s = 1u;
            break;
        }
        if (F3(0x893F2u) != 0u) {
            return;
        }
        s = 0u;
        break;
    default:
        return;
    }
    F3(0x893E8u) = s;
}

static void c36fc(void)
{
    c3716();
    c39d4();
    c37be();
    c3832();
    c3900();
}

/* ══ 0xA8EFE: the D5E6 evaluators (0xA8EFE..0xA9758) ═══════════════════ */
static void a9144(void)                     /* D5FB, the lever latch */
{
    const uint8_t eq = (uint8_t)(G_D672 == D6D6);

    if (G_D673 == 2u) {
        D5(0xD5FBu) = 1u;
    }
    if (D5(0xD5FBu) == 1u && G_D673 != 2u && eq == 1u) {
        D5(0xD5FBu) = 0u;
    }
}

static void a9086(void)                     /* D5F6 = D650 */
{
    D5(0xD5F6u) = D6(0xD650u);
    D5(0xD5F7u) = D6(0xD651u);
}

static void a908e(void)                     /* D5F9 */
{
    D5(0xD5F9u) = (uint8_t)(D6(0xD654u) != 0u);
}

static void a90a0(void)                     /* D607 */
{
    uint8_t r2;

    if (D6(0xD64Bu) == 1u && D6(0xD631u) == 0u) {
        r2 = 1u;
    } else if ((int32_t)w61a(0xD64Cu) > (int32_t)CAL_B14_10 && D6(0xD64Eu) == 0u &&
               D6(0xD631u) != 0u) {
        r2 = 1u;
    } else {
        r2 = 0u;
    }
    D5(0xD607u) = r2;
}

static void a9126(void)                     /* D5F8 */
{
    D5(0xD5F8u) = (uint8_t)(D6(0xD648u) == 1u && D6(0xD652u) == 0u);
}

static void a8f38(void)                     /* D5FA */
{
    const uint8_t l = G_D672;

    if (l == 8u || l == 7u || l == 6u || l == 5u) {
        D5(0xD5FAu) = l;
    }
}

static uint16_t d5f6(void)
{
    return (uint16_t)((uint16_t)D5(0xD5F6u) << 8 | D5(0xD5F7u));
}

/* the two-threshold band of 0xA95F4 / 0xA9636 (signed compares on the
 * zero-extended words) */
static uint8_t band(uint16_t lo, uint16_t hi)
{
    const int32_t v = (int32_t)d5f6();

    if (!(v > (int32_t)lo)) {
        return 0u;
    }
    return (uint8_t)(v > (int32_t)hi ? 2u : 1u);
}

static void a9526(void)
{
    const uint8_t sel = D6(0xD65Cu);
    uint8_t r2;

    /* 0xA9538: D603 */
    if (sel == 0u) {
        if (!(d5f6() > CAL_B14_38)) {
            r2 = 0u;
        } else {
            r2 = (uint8_t)((int32_t)d5f6() > (int32_t)CAL_B14_3C ? 2u : 1u);
        }
    } else if (sel == 1u) {
        if (!(d5f6() > CAL_B14_30)) {
            r2 = 0u;
        } else {
            r2 = (uint8_t)((int32_t)d5f6() > (int32_t)CAL_B14_34 ? 2u : 1u);
        }
    } else {
        r2 = 2u;
    }
    D5(0xD603u) = r2;
    /* 0xA9594: D604 */
    if (sel == 0u) {
        D5(0xD604u) = band(CAL_B14_48E, CAL_B14_50E);
    } else if (sel == 1u) {
        D5(0xD604u) = band(CAL_B14_40, CAL_B14_44);
    } else {
        D5(0xD604u) = 2u;
    }
    /* 0xA9616: D605 */
    if (sel == 1u) {
        D5(0xD605u) = band(CAL_B14_20, CAL_B14_24);
    } else {
        D5(0xD605u) = band(CAL_B14_28, CAL_B14_2C);
    }
}

static void a9668(void)                     /* D606 */
{
    if (D6D6 == 6u && D5(0xD5F5u) == 1u && D5(0xD5F3u) != 2u && D6(0xD643u) == 1u) {
        if (G_893E8 == 1u && D6(0xD644u) == 1u) {
            D5(0xD606u) = 1u;
            /* falls through to 0xA96A6 */
        } else {
            goto a96b2;
        }
    }
    if (D6(0xD643u) == 0u) {
        goto zero;
    }
a96b2:
    if (D6D6 == 8u) {
        goto zero;
    }
    if (D5(0xD5F2u) == 1u && D5(0xD5F3u) != 2u) {
        goto zero;
    }
    if (D6(0xD644u) != 0u) {
        return;
    }
zero:
    D5(0xD606u) = 0u;
}

static void a96de(void)                     /* D608 = D6D6 != E545 */
{
    D5(0xD608u) = (uint8_t)(D6D6 != G_E545);
}

static void a902e(void)                     /* D609 */
{
    if (D6D6 == 8u && G_D672 != 8u && (D6DF != 0u || G_D6D9 != 0u) && G_893F7 == 1u) {
        D5(0xD609u) = 1u;
    }
    if (G_D672 == 8u && D6D6 == 8u) {
        D5(0xD609u) = 0u;
    }
}

static void a9748(void)                     /* D60A */
{
    D5(0xD60Au) = (uint8_t)(D6(0xD676u) == 1u);
}

static void a8efe(void)
{
    a9144();
    a9086();
    a908e();
    a90a0();
    a9126();
    a8f38();
    a9526();
    a9668();
    /* 0xA9190 (D5E6+0..+6, +0x16..+0x1C): no chain reader — not ported */
    a96de();
    a902e();
    a9748();
}

/* 0xA525A, the +0x10 part (0xA52A2): the direction flag (A5352 +0x18,
 * A53D4 the E549 report, A5448 +0x20: no chain reader — not ported; the
 * 0x8515E refresh: 0x8515E is 0 in the dump and has no writer here) */
static void a52a2(void)
{
    uint32_t v;

    if (!((int32_t)d5f6() > (int32_t)CAL_AB0_4) || D6(0xD627u) == 2u) {
        v = 0u;
    } else if (g_304c_10() != 0u) {
        return;
    } else if ((D6(0xD649u) != 0u && D6(0xD64Au) == 0u && CAL_AB0_10 == 0u) ||
               (D6(0xD649u) != 0u && D6(0xD64Au) != 2u && CAL_AB0_10 == 1u)) {
        v = D6(0xD649u);
    } else if (CAL_AB0_14 != 1u) {
        return;
    } else if (D6D6 == 5u) {
        v = 1u;
    } else if (D6D6 == 7u) {
        v = 2u;
    } else {
        return;
    }
    R.o304c[4] = v;
    R.o304c[5] = ~v;
}

/* ══ 0x9B34A ═════════════════════════════════════════════════════════ */
/* 0xC7D64 (0xC7D64..0xC7FF8): E53A..E541 (0xC7EF8 not ported: it writes
 * E532..E539 / E53F / E542, none read by the chain) */
static void c7de2_c7e52_c7e30(void)          /* 0xC7E0E */
{
    const uint8_t b = E5(0xE53Bu);

    E5(0xE53Cu) = 0u;
    /* 0xC7DE2 */
    if (G_D5FB == 1u && (b == 6u || b == 7u || b == 5u)) {
        E5(0xE53Cu) = 0u;
        return;
    }
    /* 0xC7E30 */
    if (b == 8u) {
        E5(0xE53Cu) = 8u;
        return;
    }
    if (b == 7u || b == 5u || b == 6u) {
        E5(0xE53Cu) = b;
        return;
    }
    /* 0xC7E52 */
    {
        const uint8_t r6 = G_D5FA, d = D6D6;
        const uint8_t r7 = (uint8_t)(r6 == 8u || d == 8u);
        const uint8_t r14 = (uint8_t)(r6 == 7u || d == 7u);
        const uint8_t r4 = (uint8_t)(r6 == 5u || d == 5u);

        if ((b == 9u && r4 == 1u) || (b == 10u && (r7 == 1u || r4 == 1u)) ||
            (b == 11u && (r7 == 1u || r14 == 1u))) {
            E5(0xE53Cu) = 6u;
        }
    }
}

static void c7d64(void)
{
    uint8_t r6;

    /* 0xC7D82 */
    r6 = G_E531;
    E5(0xE53Bu) = (uint8_t)(E5(0xE53Au) == r6 ? 0u : r6);
    E5(0xE53Au) = r6;
    /* 0xC7DA4 */
    r6 = D6(0xD671u);
    E5(0xE53Eu) = (uint8_t)(E5(0xE53Du) == 0u && r6 == 1u);
    E5(0xE53Du) = r6;
    /* 0xC7DC6 */
    r6 = D6(0xD681u);
    E5(0xE541u) = (uint8_t)(E5(0xE540u) == r6 ? 0u : r6);
    E5(0xE540u) = r6;
    c7de2_c7e52_c7e30();
    /* 0xC7FDA */
    if (D6(0xD61Bu) == 1u && CAL_E53C_OVR != 0xFFu) {
        s_rs_wit.ovr_n++;
        E5(0xE53Cu) = CAL_E53C_OVR;
    }
}

/* 0xC29A0 (0xC29A0..0xC2C22): 0x8941A, the R timer */
static void c2a9e(void)
{
    X18(0x89418u) = 0u;
    X18(0x8941Bu) = 0u;
    X18(0x8941Au) = 7u;
    X18(0x8941Cu) = 0u;
    X18(0x8941Du) = 1u;
}

static void c29a0(void)
{
    /* +2 = E53C */
    X18(0x8941Au) = G_E53C;
    /* 0xC29C0: +7 */
    X18(0x8941Fu) = (uint8_t)(G_D5F9 == 1u || G_D673 == 2u);
    /* 0xC29E4 */
    X18(0x8941Cu) = 1u;
    X18(0x8941Du) = 0u;
    do {
        const uint8_t r = G_E53C;
        /* 0xC2A28 → 0xC2A40 */
        if (r == 7u && CAL_R_TIMER > 0u && (D6D6 == 6u || D6D6 == 5u) &&
            X18(0x8941Fu) == 0u && G_893F7 == 1u) {
            X18(0x89418u) = CAL_R_TIMER;
            X18(0x89419u) = 0u;
            X18(0x8941Bu) = 4u;
            X18(0x8941Au) = 6u;
            X18(0x8941Cu) = 0u;
            X18(0x8941Du) = 1u;
        }
        if (X18(0x8941Du) == 1u) {
            break;
        }
        /* 0xC2AB4 */
        if (r == 8u || r == 7u || r == 6u || r == 5u) {
            X18(0x89418u) = 0u;
            X18(0x8941Bu) = 0u;
            X18(0x8941Cu) = 0u;
            X18(0x8941Du) = 1u;
            break;
        }
        /* 0xC2ADC */
        if (X18(0x8941Bu) != 0u && G_D608 == 1u) {
            X18(0x8941Au) = 0u;
            X18(0x89418u) = 0u;
            X18(0x8941Bu) = 0u;
            X18(0x8941Cu) = 0u;
            X18(0x8941Du) = 1u;
            break;
        }
        /* 0xC2B06 */
        if (G_893F7 == 0u && X18(0x8941Bu) == 4u) {
            c2a9e();
            break;
        }
        /* 0xC2B26 */
        if (X18(0x8941Bu) == 4u && X18(0x8941Fu) == 1u) {
            c2a9e();
        }
    } while (0);
    /* 0xC2B42 */
    if (X18(0x8941Cu) == 1u) {
        uint8_t e = 0u;

        if (X18(0x89418u) != 0u) {
            X18(0x89418u)--;
            e = (uint8_t)(X18(0x89418u) == 0u);
        }
        X18(0x89419u) = e;
        if (e == 1u) {
            if (X18(0x8941Bu) == 4u) {
                c2a9e();
            } else {
                X18(0x8941Au) = 0u;
                X18(0x8941Du) = 1u;
            }
            X18(0x89418u) = 0u;
            X18(0x8941Bu) = 0u;
        }
    }
    /* 0xC2B86: an inconsistent timer is reported (0xA7B36 event 0x1C) and cleared */
    if ((X18(0x8941Bu) == 0u && X18(0x89418u) > 0u) ||
        (X18(0x8941Bu) != 0u && !(X18(0x89418u) > 0u))) {
        X18(0x8941Bu) = 0u;
        X18(0x89418u) = 0u;
    }
    /* 0xC2BE4: +6 = the last nonzero request */
    {
        const uint8_t c = E5(0xE53Cu);

        if (c == 7u || c == 6u || c == 5u || c == 8u) {
            X18(0x8941Eu) = c;
        }
    }
    /* 0xC2C04 */
    if (D6(0xD61Bu) == 1u && CAL_8941A_OVR != 0xFFu) {
        s_rs_wit.ovr_n++;
        X18(0x8941Au) = CAL_8941A_OVR;
    }
}

/* 0xC8556: 0xC8650 (the enables) then 0xC85D6 (the filters) */
static void c8578(void)                     /* 0x894B8 */
{
    const int32_t r13 = (int32_t)CAL_B10_20E * 10;
    const int32_t r1 = (int32_t)d5f6() * 10;
    const int32_t r6 = (int32_t)CAL_B10_14;
    const int32_t r4 = r13 >= r6 ? r13 - r6 : 0;
    uint8_t r5 = R.x894b8;

    if (R.x894b8 == 0u) {
        if (r1 > r13) {
            r5 = 1u;
        }
    } else if (R.x894b8 == 1u) {
        if (!(r1 > r4)) {
            r5 = 0u;
        }
    }
    R.x894b8 = r5;
}

/* an enable whose result the coding-mask virtual call (+8 object, method
 * +0x14) decides: not ported — the dump's enable is used and the reach
 * counted (when another gate already forces 0, the mask cannot matter) */
static uint8_t mask_reach(uint8_t k)
{
    s_rs_wit.mask_n[k]++;
    return s_flt_en_dump[k];
}

static void c8650(void)
{
    /* 0xC8300: sub-object 0's table */
    R.flt_tab[4] = (uint8_t)(E5(0xE530u) == 1u ? T_E66E : T_E64C);
    /* 0xC856A */
    c8578();
    /* 0xC848E (+0x58): mask, then D5F2 == 1 && D68C == 1 && D68D == 0 */
    R.flt_en[0] = (G_D5F2 == 1u && D6(0xD68Cu) == 1u && D6(0xD68Du) == 0u) ?
                  mask_reach(0u) : 0u;
    /* 0xC83E8 (+0x40): mask, then D60A == 1 */
    R.flt_en[1] = G_D60A == 1u ? mask_reach(1u) : 0u;
    /* 0xC842E (+0x4C): D675 == 1, 893EE > cal, 893EA != 2, then mask */
    R.flt_en[2] = (D6(0xD675u) == 1u && (int32_t)R.x893e0_e > (int32_t)CAL_B10_C &&
                   G_893EA != 2u) ? mask_reach(2u) : 0u;
    /* 0xC823E (+0x34): 894B8 == 0, 893EE > cal, 893EA != 2, then mask */
    R.flt_en[3] = (R.x894b8 == 0u && (int32_t)R.x893e0_e > (int32_t)CAL_B10_10 &&
                   G_893EA != 2u) ? mask_reach(3u) : 0u;
    /* 0xC829E (+0): [cal > 0 → 0xC5F14(+0xC)], then 894B8 == 0 && D607 == 0
     * && (+0xC)+4 == 0 */
    if (CAL_AE0_0E > 0u) {
        s_rs_wit.c5f14_n++;
    }
    R.flt_en[4] = (uint8_t)(R.x894b8 == 0u && G_D607 == 0u && SI_89444 == 0u);
    /* 0xC8352 (+0x70): the mask first — not ported */
    R.flt_en[5] = mask_reach(5u);
    /* 0xC84E6 (+0x64): mask, then D703 == 1 */
    R.flt_en[6] = SI_D703 == 1u ? mask_reach(6u) : 0u;
}

/* 0xC81A6: range code → index */
static int8_t c81a6(uint8_t v)
{
    switch (v) {
    case 8u: return 0;
    case 7u: return 1;
    case 6u: return 2;
    case 5u: return 3;
    default: return -1;
    }
}

static void c85d6(void)
{
    R.x894b6 = G_8941A;
    for (uint8_t k = 0u; k < 7u; k++) {     /* +0x58 +0x40 +0x4C +0x34 +0 +0x70 +0x64 */
        /* 0xC820A → 0xC816C(table, 0x8941A, D6D6) */
        if (R.flt_en[k] != 0u) {
            const int8_t i1 = c81a6(D6D6), i2 = c81a6(G_8941A);

            if (i1 >= 0 && i2 >= 0) {
                const uint8_t v = s_flt_tabs[R.flt_tab[k]][(uint8_t)(i1 * 4 + i2)];

                if (v != 0xFFu) {
                    R.x894b6 = v;
                }
            }
        }
    }
}

/* ══ 0xC64D4: D6CE and the candidate arbiter 0xC6512 (0xC64D4..0xC6E0A) ═ */
#define CA_T     R.d6ca_t
#define CA_EXP   CA(0xD6CCu)      /* +2 */
#define CA_OUT   CA(0xD6CEu)      /* +4 */
#define CA_PREV  CA(0xD6CFu)      /* +5 */
#define CA_HIT   CA(0xD6D0u)      /* +6 */
#define CA_RSN   CA(0xD6D1u)      /* +7 */
#define CA_D2    CA(0xD6D2u)      /* +8 */
#define CA_D3    CA(0xD6D3u)      /* +9 */

static void c6b7a(void)
{
    CA_T = 0u;
    CA_OUT = CA_PREV;
    CA_PREV = 0u;
    CA_RSN = 0u;
    CA_HIT = 1u;
}

static void c6b8e(uint8_t v)
{
    CA_T = 0u;
    CA_OUT = v;
    CA_PREV = 0u;
    CA_RSN = 0u;
    CA_HIT = 1u;
}

static uint8_t c6ba8(void)
{
    const uint8_t b6 = R.x894b6, d = D6D6;
    const uint8_t r10 = (uint8_t)(G_D605 == 1u || (G_D605 == 2u && CAL_AF0_28 == 1u));
    const uint8_t r13 = (uint8_t)(G_D604 == 1u || (G_D604 == 2u && CAL_AF0_28 == 1u));
    const uint8_t dir = g_304c_10();
    uint8_t c1, c2, c3, c4, c5, c6;

    c1 = (uint8_t)(d == 7u && b6 == 5u && r10 == 1u);
    c2 = (uint8_t)(d == 5u && b6 == 7u && r13 == 1u);
    c3 = (uint8_t)(d == 6u && CAL_AF0_2C == 1u && b6 == 7u &&
                   (dir == 1u || (dir == 0u && CAL_AF0_34 == 1u)) && r13 == 1u);
    c4 = (uint8_t)(d == 6u && CAL_AF0_2C == 1u && b6 == 5u &&
                   (dir == 2u || (dir == 0u && CAL_AF0_34 == 1u)) && r10 == 1u);
    c5 = (uint8_t)(d == 6u && CA_PREV == b6 && CAL_AF0_30 == 1u &&
                   ((r10 == 1u && b6 == 5u) || (r13 == 1u && b6 == 7u)));
    c6 = (uint8_t)(d == 6u && b6 == 7u && CAL_AF0_2C == 1u && dir == 2u &&
                   G_D604 == 2u && CAL_AF0_28 == 1u);
    return (uint8_t)(c1 || c2 || c3 || c4 || c5 || c6);
}

static uint8_t c6578(void)
{
    const uint8_t b6 = R.x894b6;
    uint16_t t = 0u;

    if (!(b6 == 7u || b6 == 5u) || CA_D3 != 0u || G_D5F2 != 1u || G_D608 != 0u) {
        return CA_HIT;
    }
    if (G_893E8 == 1u || G_893E8 == 2u) {
        if (b6 == 7u) {
            t = CAL_AF0_8E;
        } else if (b6 == 5u) {
            t = CAL_AF0_0;
        }
        if (t > 0u && c6ba8() == 1u) {
            CA_T = t;
            CA_EXP = 0u;
            CA_PREV = b6;
            CA_OUT = 6u;
            CA_RSN = 3u;
            CA_HIT = 1u;
        }
    } else if (G_893E8 == 0u) {
        const uint8_t d86 = (uint8_t)((G_D604 == 0u && b6 == 7u) || (G_D605 == 0u && b6 == 5u));
        const uint8_t dd8 = (uint8_t)((D6D6 == 8u || D6D6 == 6u) && (b6 == 7u || b6 == 5u));

        if (CAL_AF0_10 > 0u && d86 == 1u && dd8 == 1u) {
            CA_T = CAL_AF0_10;
            CA_EXP = 0u;
            CA_PREV = b6;
            CA_OUT = 6u;
            CA_RSN = 4u;
            CA_HIT = 1u;
        }
    }
    return CA_HIT;
}

static uint8_t c6664(void)
{
    if (R.x894b6 == 8u &&
        (G_D603 == 1u || (CAL_AF0_20 == 1u && G_D603 == 2u)) &&
        D6D6 != 8u && CAL_AF0_4 > 0u) {
        CA_T = CAL_AF0_4;
        CA_EXP = 0u;
        CA_OUT = 6u;
        CA_PREV = 8u;
        CA_RSN = 2u;
        CA_HIT = 1u;
    }
    return CA_HIT;
}

static uint8_t c66f8(void)
{
    uint16_t v;

    if (!(CA_T > 0u)) {
        return CA_HIT;
    }
    if (G_E531 == 8u && CA_PREV == 8u && G_D603 != 0u && CAL_AF0_4 > 0u &&
        CAL_AF0_24 == 1u) {
        v = CAL_AF0_4;
        goto write;
    }
    if (G_E531 != CA_PREV || G_D608 != 0u || CA_D3 != 0u || G_D5F2 != 1u ||
        !(G_893E8 == 1u || G_893E8 == 2u) || CAL_AF0_38 != 1u) {
        return CA_HIT;
    }
    if (CA_PREV == 7u) {
        if (!(CAL_AF0_8E > 0u)) {
            return CA_HIT;
        }
        if (G_D604 != 0u) {
            v = CAL_AF0_8E;
            goto write;
        }
    }
    if (CA_PREV != 5u || !(CAL_AF0_0 > 0u) || G_D605 == 0u) {
        return CA_HIT;
    }
    v = CAL_AF0_0;
write:
    CA_T = v;
    CA_EXP = 0u;
    CA_OUT = 0u;
    CA_HIT = 1u;
    return CA_HIT;
}

static uint8_t c67f6(void)
{
    if (G_D608 == 1u && (CA_PREV == 7u || CA_PREV == 6u || CA_PREV == 5u)) {
        c6b8e(0u);
    }
    return CA_HIT;
}

static uint8_t c6822(void)
{
    const uint8_t p = CA_PREV, a = G_8941A;

    if (p == 8u) {
        if (a == 5u || a == 6u || a == 7u) {
            c6b8e(R.x894b6);
        }
    } else if (p == 5u || p == 7u) {
        if (((a == 8u || a == 6u) && a != p) || ((a == 5u || a == 7u) && a != p)) {
            c6b8e(R.x894b6);
        }
    }
    return CA_HIT;
}

static uint8_t c687e(void)
{
    if ((CA_PREV == 5u || CA_PREV == 7u) && CA_D3 == 1u) {
        c6b7a();
    }
    return CA_HIT;
}

static uint8_t c68cc(void)
{
    const uint8_t s8 = G_893E8, s9 = G_893E9, b6 = R.x894b6;

    if (!(CA_PREV == 5u || CA_PREV == 7u)) {
        return CA_HIT;
    }
    if (s9 == 0u && s8 == 1u && b6 == 0u && G_D5F2 == 1u) {
        goto b7a;
    }
    if (s9 == 2u && s8 == 1u && b6 == 0u) {
        goto b7a;
    }
    if (s9 == 2u && s8 == 0u) {
        c6b8e(0u);
        return CA_HIT;
    }
    if (s9 == 1u && s8 == 0u) {
        goto b7a;
    }
    if ((int32_t)s8 > 1) {
        return CA_HIT;
    }
    if (G_D5F2 != 0u) {
        return CA_HIT;
    }
b7a:
    c6b7a();
    return CA_HIT;
}

static uint8_t c695e(void)
{
    const uint8_t p = CA_PREV;

    if (p == 8u) {
        if (G_D603 == 0u || (G_D603 == 2u && CAL_AF0_20 == 0u)) {
            c6b7a();
        }
    } else if (p == 5u || p == 7u) {
        uint8_t r2;

        if (!(G_893E8 == 1u || G_893E8 == 2u)) {
            return CA_HIT;
        }
        r2 = p == 7u ? G_D604 : G_D605;
        if (r2 == 0u) {
            c6b7a();
        } else if (r2 == 1u) {
            if (p == 7u && g_304c_10() == 2u) {
                c6b7a();
            }
        } else if (r2 == 2u) {
            if (CAL_AF0_28 == 0u) {
                c6b7a();
            }
        }
    }
    return CA_HIT;
}

static void c69fc(void)
{
    if (CA_T != 0xFFFFu) {
        if (CA_T != 0u) {
            CA_T--;
            CA_EXP = (uint8_t)(CA_T == 0u);
        } else {
            CA_EXP = 0u;
        }
    }
    if (CA_EXP == 1u) {
        if (CA_PREV == 7u || CA_PREV == 5u) {
            if (R.x894b6 == 0u) {
                c6b7a();
            }
        } else if (CA_PREV == 8u) {
            c6b7a();
        }
    }
}

static void c6a46(void)                     /* D6D2 */
{
    const uint8_t r = CA_RSN;
    const uint16_t c = CAL_AF0_14;

    CA_D2 = 0u;
    switch (CA_PREV) {
    case 6u:
        if ((r == 1u && (c & 1u)) || (r == 5u && ((c >> 7) & 1u))) {
            CA_D2 = 6u;
        }
        break;
    case 8u:
        if (r == 2u && ((c >> 1) & 1u)) {
            CA_D2 = 8u;
        }
        break;
    case 7u:
        if ((r == 3u && ((c >> 2) & 1u)) || (r == 4u && ((c >> 4) & 1u)) ||
            (r == 5u && ((c >> 6) & 1u))) {
            CA_D2 = 7u;
        }
        break;
    case 5u:
        if ((r == 3u && ((c >> 3) & 1u)) || (r == 4u && ((c >> 5) & 1u)) ||
            (r == 5u && ((c >> 8) & 1u))) {
            CA_D2 = 5u;
        }
        break;
    default:
        break;
    }
}

static void c64d4(void)
{
    CA_OUT = R.x894b6;
    /* 0xC6512 */
    CA_HIT = 0u;
    CA_D3 = (uint8_t)(D6(0xD661u) == 1u ? 0u : 1u);        /* 0xC6502 */
    if (c6578() != 1u && c6664() != 1u && c67f6() != 1u && c6822() != 1u &&
        c687e() != 1u && c68cc() != 1u && c695e() != 1u && c66f8() != 1u) {
        c69fc();
    }
    c6a46();
    if (D6(0xD61Bu) == 1u && CAL_AF0_1C != 0xFFu) {
        s_rs_wit.ovr_n++;
        CA_OUT = CAL_AF0_1C;
    }
}

/* ══ 0xC7A14: E543..E545 (0xC7A14..0xC7CE8) ════════════════════════════ */
static void c7a56(void)
{
    const uint8_t c = G_D6CE;

    if (G_893E8 == 1u || G_893E8 == 2u) {
        if (c == 7u && G_D604 != 0u) {
            E5(0xE545u) = 6u;
            E5(0xE544u) = 1u;
        } else {
            E5(0xE545u) = c;
        }
    }
}

static void c7ab8(void)
{
    if (G_D603 != 0u) {
        E5(0xE545u) = 6u;
        E5(0xE543u) = 1u;
    } else {
        E5(0xE545u) = (uint8_t)(D6(0xD62Fu) == 1u ? 6u : 8u);
    }
}

static void c7ae8(void)
{
    if (G_D603 == 1u || G_D603 == 2u) {
        E5(0xE545u) = 6u;
        E5(0xE543u) = 1u;
    } else {
        E5(0xE545u) = D6(0xD62Fu) == 1u ? D6D6 : 8u;
    }
}

static void c7b18(void)
{
    const uint8_t d = D6D6, c = G_D6CE;

    if (!((c == 5u && d == 7u) || (c == 7u && d == 5u))) {
        return;
    }
    if (c == 7u && D6(0xD62Fu) == 1u) {
        E5(0xE545u) = d;
        return;
    }
    if (!(G_893E8 == 1u || G_893E8 == 2u)) {
        E5(0xE545u) = 6u;
        return;
    }
    if (c == 7u && G_D604 == 0u) {
        E5(0xE545u) = c;
        return;
    }
    if (c == 5u && G_D605 == 0u) {
        E5(0xE545u) = c;
        return;
    }
    E5(0xE545u) = 6u;
    E5(0xE544u) = 1u;
}

static void c7c0c(void)
{
    const uint8_t c = G_D6CE;

    if (!(G_893E8 == 1u || G_893E8 == 2u) || D6(0xD62Fu) == 1u) {
        E5(0xE545u) = 6u;
        return;
    }
    if (c == 7u) {
        if (G_D604 == 0u || (G_D604 == 1u && g_304c_10() == 2u)) {
            E5(0xE545u) = 7u;
        } else {
            E5(0xE545u) = 6u;
            E5(0xE544u) = 1u;
        }
        return;
    }
    if (c == 5u) {
        if (G_D605 == 0u || (G_D605 != 0u && g_304c_10() == 1u)) {
            E5(0xE545u) = 5u;
            return;
        }
    }
    E5(0xE545u) = 6u;
}

static void c7a14(void)
{
    const uint8_t d = D6D6;

    E5(0xE543u) = 0u;
    E5(0xE544u) = 0u;
    E5(0xE545u) = d;
    if (d == 8u) {
        /* 0xC7A94 */
        const uint8_t c = G_D6CE;

        if (c == 7u || c == 6u || c == 5u) {
            c7a56();
        } else {
            E5(0xE545u) = 8u;
        }
    } else if (d == 5u || d == 7u) {
        /* 0xC7BAA */
        const uint8_t c = G_D6CE;

        if (c == 6u) {
            E5(0xE545u) = 6u;                       /* 0xC7AE2 */
        } else if (c == 8u) {
            c7ae8();
        } else if (c == 5u || c == 7u) {
            c7b18();
        } else {
            E5(0xE545u) = d;
        }
    } else if (d == 6u) {
        /* 0xC7C9C */
        const uint8_t c = G_D6CE;

        if (c == 8u) {
            c7ab8();
        } else if (c == 5u || c == 7u) {
            c7c0c();
        } else {
            E5(0xE545u) = 6u;
        }
    }
    /* 0xC7CCA */
    if (D6(0xD61Bu) == 1u && CAL_E545_OVR != 0xFFu) {
        s_rs_wit.ovr_n++;
        E5(0xE545u) = CAL_E545_OVR;
    }
}

/* ══ 0xC3A7E: 0x8942C (0xC3A7E..0xC3DB2) ═══════════════════════════════ */
static void c3a7e(void)
{
    /* 0xC3D8A: +0x10 */
    {
        const uint16_t v = w61a(0xD692u);

        X20(0x89430u) = (uint8_t)(!(v > CAL_8942C_C) && v >= CAL_8942C_10);
    }
    /* 0xC3CEA: +0xF */
    {
        const uint8_t a = G_8941A;

        if (a == 6u && G_E545 == 6u) {
            const uint8_t s = G_893E8;

            if ((s == 0u && (CAL_8942C_24 & 1u)) || (s == 1u && (CAL_8942C_24 & 2u)) ||
                (s == 2u && (CAL_8942C_24 & 4u)) || (s == 3u && (CAL_8942C_24 & 8u))) {
                X20(0x8942Fu) = 1u;
            }
        }
        if (a == 8u || a == 7u || a == 5u || G_E545 == 8u || G_E545 == 7u || G_E545 == 5u) {
            X20(0x8942Fu) = 0u;
        }
    }
    /* 0xC3A94 → 0xC3A40 */
    R.x89420_lim = (uint32_t)((int32_t)w61a(0xD682u) * (int32_t)CAL_8942C_18 / 1000) +
                   CAL_8942C_14;
    R.x89420_t = R.x89420_t >= R.x89420_lim ? R.x89420_t - R.x89420_lim : 0u;
    X20(0x89424u) = 0u;
    /* 0xC3AE6 */
    if (X20(0x8942Du) != 0u) {
        s_rs_wit.s8942c_n++;            /* 0xC3A98 + states 1/2: unreachable, see below */
        return;
    }
    X20(0x8942Eu) = 0u;
    X20(0x8942Cu) = 0u;
    R.x89420_t = 0u;
    X20(0x89424u) = 1u;
    if (D6D6 != 6u || G_E545 != 6u) {
        return;
    }
    if (X20(0x8942Fu) != 1u && CAL_8942C_24 != 0u) {
        return;
    }
    /* 0xC3B50: cal +0 (u32) must be > 0 — it is 0, so state 0 never
     * leaves (_Static_assert above); states 1 / 2 are not ported */
    if (!(CAL_8942C_T0 > 0u)) {
        return;
    }
}

/* ══ 0xC2754 → 0xC28DC: D6DE / D6DF (0xC2754..0xC2954) ════════════════ */
static void c290e(uint8_t v, uint8_t rsn)
{
    const uint8_t e = G_E545;

    if (e == 8u) {
        if (v != 8u) {
            s_rs_wit.guard_c290e_n++;
            return;
        }
    } else if (e == 6u) {
        if (!(v == 8u || v == 6u)) {
            s_rs_wit.guard_c290e_n++;
            return;
        }
    }
    D6DE = v;
    D6DF = rsn;
}

static void c28dc(void)
{
    const uint8_t e = G_E545;

    D6DE = e;
    D6DF = 0u;
    /* 0xC2812 → 8 / 0x28 */
    if (CAL_D6DE_ENB != 0u && (e == 7u || e == 6u || e == 5u) && G_E541 == 1u &&
        G_D603 == 0u && D6(0xD62Fu) == 0u) {
        c290e(8u, 0x28u);
        return;
    }
    /* 0xC2758 / 0xC2776 → 6 / 0x1E */
    if (CAL_D6DE_EN9 != 0u && (e == 5u || e == 7u) && G_893E8 == 0u &&
        (G_D5F2 == 1u || G_D603 != 0u)) {
        c290e(6u, 0x1Eu);
        return;
    }
    /* 0xC27C4 → 6 / 0x23 */
    if (CAL_D6DE_ENA != 0u && (e == 5u || e == 7u) && G_D5F2 == 0u && G_D603 > 0u) {
        c290e(6u, 0x23u);
        return;
    }
    /* 0xC286C → 8 / 0x50 */
    if (CAL_D6DE_ENE == 1u && G_D5F2 == 0u && G_D606 == 0u && D6(0xD62Fu) == 0u &&
        (e == 5u || e == 7u || e == 6u) && X20(0x8942Cu) == 0u && G_D603 == 0u) {
        c290e(8u, 0x50u);
    }
}

/* ══ 0xC360A: E546..E548 (0xC360A..0xC36B2) ════════════════════════════ */
static void c360a(void)
{
    const uint8_t s = G_893E8;

    if (CAL_E546_EN != 1u) {
        E5(0xE548u) = 0u;
        E5(0xE546u) = 0u;
        R.p8517e = D6D6;
        return;
    }
    {
        uint8_t e = 0u;

        if (E5(0xE546u) != 0u) {
            E5(0xE546u)--;
            e = (uint8_t)(E5(0xE546u) == 0u);
        }
        E5(0xE547u) = e;
    }
    if (D6D6 == 8u || s == 1u) {
        E5(0xE546u) = 0u;
    } else if (R.p8517e == 8u && D6D6 != 8u && s == 2u) {
        E5(0xE546u) = CAL_E546_T;
        E5(0xE547u) = 0u;
    }
    if (E5(0xE547u) == 1u && (s == 2u || s == 0u)) {
        E5(0xE548u) = 1u;
    } else if (E5(0xE546u) > 0u && s == 0u) {
        E5(0xE548u) = 1u;
        E5(0xE546u) = 0u;
    } else {
        E5(0xE548u) = 0u;
    }
    R.p8517e = D6D6;
}

/* ══ 0xC70CE: D6D6 (0xC70CE..0xC79EC) ══════════════════════════════════ */
static void c792e(uint8_t v, uint8_t rsn)   /* the write guard */
{
    const uint8_t de = D6DE;

    if (de == 8u) {
        if (v != 8u) {
            s_rs_wit.guard_c792e_n++;
            return;
        }
    } else if (de == 6u) {
        if (!(v == 8u || v == 6u)) {
            s_rs_wit.guard_c792e_n++;
            return;
        }
    }
    D6D6 = v;
    D4(0xD6D9u) = rsn;
}

static void c7110(void)                     /* D6DB */
{
    const uint8_t r1 = D6(0xD68Eu);

    if (D4(0xD6D8u) == 6u || G_E53C == 6u) {
        D4(0xD6DBu) = 0u;
    } else if (R.p85182 != r1) {
        if ((CAL_AF8_10 != 0u && r1 == CAL_AF8_10) || (CAL_AF8_C != 0u && r1 == CAL_AF8_C)) {
            D4(0xD6DBu) = 1u;
        }
    }
    R.p85182 = D6(0xD68Eu);
}

static void c71be(void)                     /* D6DC */
{
    const uint8_t r1 = D6(0xD68Fu);

    if (D4(0xD6D8u) == 6u || G_E53C == 6u) {
        D4(0xD6DCu) = 0u;
    } else if (R.p85183 != r1) {
        if ((CAL_AF8_8 != 0u && r1 == CAL_AF8_8) || (CAL_AF8_4 != 0u && r1 == CAL_AF8_4)) {
            D4(0xD6DCu) = 1u;
        }
    }
    R.p85183 = D6(0xD68Fu);
}

static void c7258(void)                     /* D6DD, the D6D4 timer */
{
    const uint8_t s8 = G_893E8;
    const uint8_t r4 = (uint8_t)(D6(0xD648u) == 0u && D6(0xD652u) == 0u);

    if (D4(0xD6DDu) == 0u) {
        D4(0xD6D4u) = 0u;
        D4(0xD6DDu) = 0u;
        if (D4(0xD6D8u) != 8u && G_893E9 == 1u && s8 != 1u) {
            D4(0xD6DDu) = 1u;
            D4(0xD6D4u) = CAL_AF8_14;
            D4(0xD6D5u) = 0u;
        }
        return;
    }
    if (D4(0xD6DDu) == 1u) {
        uint8_t e = 0u;

        if (s8 == 1u && r4 == 1u) {
            if (D4(0xD6D4u) != 0u) {
                D4(0xD6D4u)--;
                e = (uint8_t)(D4(0xD6D4u) == 0u);
            }
        } else {
            D4(0xD6D4u) = CAL_AF8_14;
        }
        D4(0xD6D5u) = e;
        if (D4(0xD6D8u) != 8u && D4(0xD6D4u) > 0u) {
            return;
        }
    }
    D4(0xD6DDu) = 0u;
}

static uint8_t c77fe(void)
{
    const uint8_t c = CAL_AF8_20;

    if (c != 0u && D6(0xD62Fu) == 1u &&
        (((D6DE == 7u || D6DE == 5u) && c == 2u) || (D6DE == 7u && c == 1u))) {
        c792e(6u, 0x28u);
        return 1u;
    }
    return 0u;
}

static uint8_t c7444(void)
{
    if (G_D5F9 == 1u && D4(0xD6D8u) == 8u && D6DE != 8u) {
        c792e(8u, 0x0Au);
        return 1u;
    }
    return 0u;
}

static uint8_t c747c(void)
{
    if (D6DE == 8u || G_D5F9 != 1u) {
        return 0u;
    }
    if (G_D5F8 == 1u && G_D603 == 0u && D6(0xD62Fu) == 0u) {
        c792e(8u, 0x14u);
        return 1u;
    }
    if (D6DE == 6u) {
        return 0u;
    }
    c792e(6u, 0x15u);
    return 1u;
}

static uint8_t c767c(void)
{
    if (D4(0xD6D8u) == 8u && G_D5F9 == 0u && G_893E8 != 0u &&
        ((D6DE == 7u && D6(0xD666u) == 1u) || (D6DE == 5u && D6(0xD665u) == 1u))) {
        c792e(8u, 0x33u);
        return 1u;
    }
    return 0u;
}

static uint8_t c7756(void)
{
    if (D4(0xD6D8u) == 8u && D6DE != 8u && G_D5F9 == 0u && D6(0xD664u) == 1u &&
        G_893E8 != 0u) {
        c792e(8u, 0x3Cu);
        return 1u;
    }
    return 0u;
}

static uint8_t c7178(void)
{
    if (D4(0xD6DBu) == 1u && (D6DE == 7u || D6DE == 5u)) {
        if (D4(0xD6D8u) == 8u) {
            c792e(8u, 0x50u);
        } else {
            c792e(6u, 0x51u);
        }
        return 1u;
    }
    return 0u;
}

static uint8_t c7226(void)
{
    if (D4(0xD6DCu) == 1u && (D6DE == 7u || D6DE == 5u)) {
        if (D4(0xD6D8u) == 8u) {
            c792e(8u, 0x52u);
        } else {
            c792e(6u, 0x53u);
        }
        return 1u;
    }
    return 0u;
}

static uint8_t c733a(void)
{
    if (D4(0xD6DDu) == 1u && D6(0xD648u) != 0u && D6(0xD652u) == 0u && G_D603 == 0u &&
        D6(0xD62Fu) == 0u && CAL_AF8_24 == 1u) {
        c792e(8u, 0x59u);
        return 1u;
    }
    return 0u;
}

static uint8_t c7630(void)
{
    if (D6DE != 8u && D6(0xD65Fu) == 1u && D6(0xD62Fu) == 0u && G_D5F8 == 1u &&
        G_D603 == 0u) {
        c792e(8u, 0x5Au);
        return 1u;
    }
    return 0u;
}

static uint8_t c75fa(void)
{
    if (G_D609 == 1u && D6DE != 8u && CAL_AF8_1C == 1u) {
        c792e(8u, 0x20u);
        return 1u;
    }
    return 0u;
}

static uint8_t c76d8(void)
{
    if (G_893E8 == 0u) {
        return 0u;
    }
    if (D6DE == 5u && D6(0xD665u) == 1u) {
        c792e(6u, 0x6Eu);
        return 1u;
    }
    if (D6DE == 7u && D6(0xD666u) == 1u) {
        c792e(6u, 0x6Fu);
        return 1u;
    }
    return 0u;
}

static uint8_t c77a0(void)
{
    if (D6DE == 6u && D6(0xD65Fu) == 0u && G_D5F9 == 0u && D6(0xD62Fu) == 0u &&
        G_893E8 == 0u && G_D603 == 0u && D6(0xD660u) == 1u) {
        c792e(8u, 0x78u);
        return 1u;
    }
    return 0u;
}

static uint8_t c785c(void)
{
    if (D6DE == 8u || G_D5F9 != 0u || E5(0xE548u) != 1u) {
        return 0u;
    }
    if (G_D603 == 0u && D6(0xD62Fu) == 0u) {
        c792e(8u, 0x82u);
        return 1u;
    }
    if (D6DE == 6u) {
        return 0u;
    }
    c792e(6u, 0x83u);
    return 1u;
}

static uint8_t c78be(void)
{
    uint8_t hit = 0u;

    if (CAL_AF8_28 == 1u &&
        ((D6(0xD61Cu) == 1u && D6DE == 7u) || (D6(0xD61Cu) == 3u && D6DE == 5u))) {
        c792e(6u, 0x8Cu);
        hit = 1u;
    }
    s_rs_wit.report_c78be_n++;              /* *0x4B888(.., hit, 1): not ported */
    return hit;
}

static uint8_t c74f4(void)
{
    const uint8_t c = D6(0xD653u);
    uint8_t ok = 0u;

    if (D6DE != 5u || G_D5F9 != 0u || w61a(0xD63Eu) >= s_cal_af8_0 ||
        D6(0xD63Cu) == 0u || D6(0xD640u) != 0u) {
        return 0u;
    }
    switch (c) {
    case 4u:  ok = s_cal_af8_34[0]; break;
    case 35u: ok = s_cal_af8_34[1]; break;
    case 27u: ok = s_cal_af8_34[2]; break;
    case 28u: ok = s_cal_af8_34[3]; break;
    case 29u: ok = s_cal_af8_34[4]; break;
    case 23u: ok = s_cal_af8_34[5]; break;
    case 30u: ok = s_cal_af8_34[6]; break;
    case 37u: ok = s_cal_af8_34[7]; break;
    case 39u: ok = s_cal_af8_34[8]; break;
    case 41u: ok = s_cal_af8_34[9]; break;
    default:  break;
    }
    if (ok == 1u) {
        c792e(6u, 0x1Eu);
        return 1u;
    }
    return 0u;
}

static void c7390(void)
{
    D6D6 = D6DE;                            /* 0xC7392..0xC7396: the seed */
    D4(0xD6D9u) = 0u;
    if (CAL_AF8_18 == 1u) {
        (void)(c77fe() || c7444() || c747c() || c767c() || c7756() || c7178() ||
               c7226() || c733a() || c7630() || c75fa() || c76d8() || c77a0() ||
               c785c() || c78be() || c74f4());
    }
    D4(0xD6D7u) = (uint8_t)~D6D6;
}

static void c79b8(uint8_t v)                /* 304C+0 (and the E64B mirror) */
{
    uint32_t r1 = v;

    R.o304c[0] = r1;
    R.e64b = v;
    if (D6(0xD61Bu) == 1u && CAL_304C_OVR != 0xFFu) {
        s_rs_wit.ovr_n++;
        r1 = CAL_304C_OVR;
        R.e64b = CAL_304C_OVR;
    }
    R.o304c[0] = r1 & 0xFFu;
    R.o304c[1] = ~(r1 & 0xFFu);
}

static void c70ce(void)
{
    D4(0xD6D8u) = D6D6;
    c7110();
    c71be();
    c7258();
    c7390();
    /* 0xC709C: the cal override (D61B == 1 only) */
    if (D6(0xD61Bu) == 1u) {
        s_rs_wit.ovr_n++;
    }
    c79b8(D6D6);
    if (G_D5F9 == 0u) {
        R.o304c[2] = D6D6;
        R.o304c[3] = (uint32_t)~(uint32_t)D6D6;
    }
    D4(0xD6DAu) = 0u;
}

/* ══ the recorder ═══════════════════════════════════════════════════════ */
static void rs_record(void)
{
    rng_shadow_rec_t e;
    const rng_shadow_rec_t *last;

    memset(&e, 0, sizeof e);
    e.now_ms = sched_now_ms();
    e.d672 = G_D672;
    e.d673 = G_D673;
    e.d5fb = G_D5FB;
    e.e531 = G_E531;
    e.e53c = G_E53C;
    e.x8941a = G_8941A;
    e.x894b6 = R.x894b6;
    e.d6ce = G_D6CE;
    e.e545 = G_E545;
    e.d6de = D6DE;
    e.d6df = D6DF;
    e.d6d6 = D6D6;
    e.d6d9 = G_D6D9;
    e.x893e8 = G_893E8;
    e.d607 = G_D607;
    e.flags = (uint8_t)((D6(0xD660u) != 0u ? 0x01u : 0u) | (D6(0xD661u) != 0u ? 0x02u : 0u) |
                        (D6(0xD664u) != 0u ? 0x04u : 0u) | (D6(0xD665u) != 0u ? 0x08u : 0u) |
                        (D6(0xD666u) != 0u ? 0x10u : 0u) | (D6(0xD631u) == 0u ? 0x20u : 0u) |
                        (D6(0xD64Bu) != 0u ? 0x40u : 0u) | (D6(0xD63Cu) == 0u ? 0x80u : 0u));
    if (s_rs_wit.rec_n != 0u) {
        last = &s_rs_rec[(uint8_t)(s_rs_wit.rec_head + RNG_SHADOW_REC_N - 1u) % RNG_SHADOW_REC_N];
        if (memcmp(&last->d672, &e.d672, sizeof e - sizeof e.now_ms) == 0) {
            return;
        }
    }
    s_rs_rec[s_rs_wit.rec_head] = e;
    s_rs_wit.rec_head = (uint8_t)((s_rs_wit.rec_head + 1u) % RNG_SHADOW_REC_N);
    if (s_rs_wit.rec_n < 255u) {
        s_rs_wit.rec_n++;
    }
}

/* ══ entry points ═══════════════════════════════════════════════════════ */
void rng_shadow_init(void)
{
    range_in_t in;
    range_res_t res;

    memset(&s_rs, 0, sizeof s_rs);
    memset(&s_rs_wit, 0, sizeof s_rs_wit);
    memset(s_rs_rec, 0, sizeof s_rs_rec);
    /* the 0x9B228 one-shot (0xFFF8D5E4 == 0): the list entries that touch
     * a chain cell, in list order */
    rs_gather();                                    /* 0xA7C9C */
    E5(0xE530u) = a5884();                          /* 0xA586C */
    D5(0xD5FAu) = G_D672;                           /* 0xA8EDC */
    a908e();
    a9126();
    D5(0xD5FBu) = 1u;
    /* 0xA548C(E549): the init arbiter (app/range.c's pure 0xA557C), with the
     * lever object 0xFFF8304C still 0 at this pass and the dump's D61A */
    in.lever = (uint8_t)R.o304c[0];
    in.lever2 = 0u;
    in.dir = 0u;
    in.fast = 0u;
    in.status = 0u;
    in.d646 = 0u;                                   /* 0xFFF8EE6E, dump 0 */
    in.d648 = D6(0xD648u);
    in.d650 = w61a(0xD650u);
    in.d652 = D6(0xD652u);
    in.d5f8 = D5(0xD5F8u);
    res.flag = 0u;
    res.target = 0u;
    res.reason = 0u;
    range_arbitrate(&in, &res);
    E5(0xE549u) = res.flag;
    E5(0xE54Au) = res.target;
    E5(0xE54Bu) = res.reason;
    D6D6 = res.target;
    D4(0xD6D7u) = (uint8_t)~res.target;
    /* 0xC8528(89434): the sub-objects' tables (enables follow 0xC8650) */
    R.flt_tab[0] = T_E64C;                          /* 0xC84DE (+0x58) */
    R.flt_tab[1] = T_E64C;                          /* 0xC8426 (+0x40) */
    R.flt_tab[2] = T_E64C;                          /* 0xC8486 (+0x4C) */
    R.flt_tab[3] = T_E64C;                          /* 0xC8296 (+0x34) */
    R.flt_tab[4] = (uint8_t)(E5(0xE530u) == 1u ? T_E66E : T_E64C);  /* 0xC82EE → 0xC8300 */
    R.flt_en[4] = 1u;
    R.flt_tab[5] = T_E65D;                          /* 0xC83B0 (+0x70) */
    R.flt_tab[6] = T_E67F;                          /* 0xC8520 (+0x64) */
    /* 0xC3A76(89420), 0xC3604(E546): cleared above */
    R.d5e4 = 1u;                                    /* 0xA5864 */
}

void rng_shadow_tick_10ms(uint8_t os_mode, uint8_t g98_fail)
{
    if (R.d5e4 == 0u) {
        rng_shadow_init();
    }
    /* group 98 (P0850 / P07B2 / P07B8), reported by safety/park_pair.c's
     * 0x141E06 every 10 ms: latched by our fault model like group 109 */
    if (g98_fail != 0u && R.g98 == 0u) {
        R.g98 = 1u;
        R.g98_ms = sched_now_ms();
    }
    /* the chain is in the 10 ms lists of modes 1..3 only */
    if (os_mode < 1u || os_mode > 3u) {
        R.skip_mode_n++;
        return;
    }
    s_rs_wit.passes++;
    /* 0x9B306 */
    rs_gather();
    E5(0xE530u) = a5884();
    a8f5c();
    /* 0x9B322 → 0xA5944: 0xA6A2C(D60C) → 0xA6A3E, then 0xC36FC */
    a6a3e();
    c36fc();
    a8efe();
    a52a2();
    /* 0x9B34A */
    E5(0xE531u) = G_D672;                           /* 0xC4F90 */
    c7d64();
    c29a0();
    c8650();
    c85d6();
    c64d4();
    c7a14();
    c3a7e();
    c28dc();
    c360a();
    c70ce();
    /* the 20 ms list (0x9B39E → 0xA6A3A → 0xA6AB0), modes 1..3: on the
     * first chain pass and every second one after [I: the 20 ms task's
     * phase and its order against the 10 ms task are not decoded] */
    if (R.ph20 == 0u) {
        a6ab0();
    }
    R.ph20 = (uint8_t)(R.ph20 ^ 1u);
    rs_record();
}

const rng_shadow_cells_t *rng_shadow_cells(void) { return &s_rs; }
const rng_shadow_wit_t   *rng_shadow_wit(void)   { return &s_rs_wit; }
const rng_shadow_rec_t   *rng_shadow_rec(void)   { return s_rs_rec; }

#endif /* !TCU_RAM_APP */

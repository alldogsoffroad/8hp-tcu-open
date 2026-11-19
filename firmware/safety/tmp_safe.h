/* tmp_safe.h — the safety layer's two temperature objects, the integer
 * channel that produces the conditioned fluid-temperature byte 0xFFF8E9C8
 * (the application getter 0x140050), ported from the bytes
 * (~/tcu_sweep/temp_byte_live_port.md):
 *
 * The die / board fault flags 0x940E2 / 0x940E5 gathered here (+0xB9 /
 * +0xBB, +0x2F) are the 0x80 "faulted" stand-in unless TSEL_LIVE_FLAGS
 * (stage 2b: the SH application) makes them the stock's live 0x7C01E /
 * 0x7C0B6 results — then D2 / D3 / D7 read 0 on a healthy unit (the dump's)
 * and a fluid-sensor failure falls back to the BOARD byte (0x1405FA, status
 * 1), not to 0x82.
 *
 *   object 0xFFF8E998 ("E998", cal descriptor 0x648E0: +4 → 0x195DB7 = 0x00,
 *                      +8 → 0x195DB8 = 0x82)
 *     +0x2C..+0x2F  gathered 0x940E3 / 0x940E6 / 0x940E8 / 0x940E5
 *     +0x30 / +0x31 THE byte / complement     +0x32 / +0x33 status / ~
 *     +0x34 / +0x35 the second output / status
 *   object 0xFFF8ED20 ("ED20", cal descriptor 0x64BC8)
 *     +0xB8..+0xBD  gathered 0x940E0 / E2 / E3 / E5 / E6 / E8
 *     +0xC0 (u16)   the slew limiter's state (byte × 100)
 *     +0xC2 / +0xC3 the limited fluid byte / complement
 *     +0xC6 the last accepted fluid byte, +0xC8 / +0xC9 its jump flag / prev.
 *     +0xD3 / +0xD5 (+ complements) the second / fluid "invalid" flags
 *
 * Who runs what [O task table 0x43430]:
 *   mode-0 init list (row 9, 0x4D788 → 0x1320B6 → 0x132980): ED20's
 *     0x14A4F0, E998's 0x1401C2 — after the startup table's 0x15A67C →
 *     0x151C9C (E998 conditioned once, at startup)        tmp_safe_init()
 *   the 10 ms task, MODE 0 only (0x4D7A0 → 0x1320B0 → 0x132DCE): ED20
 *     gathered and re-initialised (0x14A4F0) every pass   tmp_safe_mode0_10ms()
 *   boot state 1, row-1 list entry 0 (0x14F32C → 0x151C9C): E998 gathered
 *     and conditioned directly (0x1401C2, 0x140416)      tmp_safe_boot_pass()
 *   the 100 ms task, modes 1..3 (0x4D9E4 entry 0 0x13204A → 0x1517E0 gather
 *     → 0x14F7EA: 0x14A6E8(ED20) then 0x1402F0(E998) → 0x1405FA)
 *                                                         tmp_safe_100ms()
 *
 * NOT PORTED (stated deviations): the redundant float channel (0x14FB66,
 * 0x149D24, 0x1400A0) and every lockstep / complement / RAM-test check
 * that compares it with the integer channel and halts on a mismatch
 * (0x148ACC, 0x148B6C, the 0x14A6E8 / 0x1402F0 fatal paths); the cross-
 * plausibility monitors 0x14AE38 / 0x14AF70 / 0x14B27A (they compare the
 * fluid byte with the die / board bytes 0x940E0 / 0x940E3 — published since
 * stage 2b, ~/tcu_sweep/temp_stage2b_port.md — and the stock writes their
 * flags to the EEPROM record 0xFFF82EF4 + 0x76.. at 0x14A9BC..0x14AAC8:
 * NOT PORTED; +0xCB..+0xCE / +0xD0 hold the dump's 0); the 0x14A6E8 tail
 * that writes +0xCC/CD/CE/D5/D2/D3 into that record (not ported); the
 * +0xCC..+0xD7 start values 0x14A4F0 copies from 0xFFF82EF4 + 0x76..0x82
 * (stand-in 0, the dump's); 0x1404C8 / 0x1402F0's +0x36..+0x3A bytes.
 */
#ifndef TCU_SAFETY_TMP_SAFE_H
#define TCU_SAFETY_TMP_SAFE_H

#include <stdint.h>

#define TS_E998             0xFFF8E998u
#define TS_ED20             0xFFF8ED20u

#define TS_U8(o, off)       (*(volatile uint8_t  *)(uintptr_t)((o) + (off)))
#define TS_U16(o, off)      (*(volatile uint16_t *)(uintptr_t)((o) + (off)))

/* E998 */
#define TS_E_IN2            0x2Cu   /* 0x940E3 (second byte) */
#define TS_E_IN             0x2Du   /* 0x940E6 (fluid byte) */
#define TS_E_INF            0x2Eu   /* 0x940E8 (fluid flags) */
#define TS_E_IN2F           0x2Fu   /* 0x940E5 (second flags) */
#define TS_E_OUT            0x30u   /* THE conditioned byte 0xFFF8E9C8 */
#define TS_E_OUT_C          0x31u
#define TS_E_ST             0x32u   /* 0 direct / 1 from the second byte / 2 cal default */
#define TS_E_ST_C           0x33u
#define TS_E_OUT2           0x34u
#define TS_E_ST2            0x35u
/* ED20 */
#define TS_D_B8             0xB8u   /* 0x940E0 */
#define TS_D_B9             0xB9u   /* 0x940E2 */
#define TS_D_BA             0xBAu   /* 0x940E3 */
#define TS_D_BB             0xBBu   /* 0x940E5 */
#define TS_D_BC             0xBCu   /* 0x940E6 */
#define TS_D_BD             0xBDu   /* 0x940E8 */
#define TS_D_BE             0xBEu   /* BD of the previous pass */
#define TS_D_STATE          0xC0u   /* u16: byte × 100 */
#define TS_D_LIM            0xC2u   /* the limited byte */
#define TS_D_C4             0xC4u
#define TS_D_C6             0xC6u   /* the last accepted fluid byte */
#define TS_D_JUMP           0xC8u
#define TS_D_JUMP_P         0xC9u
#define TS_D_CB             0xCBu
#define TS_D_CC             0xCCu
#define TS_D_CD             0xCDu
#define TS_D_CE             0xCEu
#define TS_D_D0             0xD0u
#define TS_D_D2             0xD2u
#define TS_D_D3             0xD3u   /* the second byte invalid */
#define TS_D_D5             0xD5u   /* THE fluid byte invalid */
#define TS_D_D7             0xD7u

/* calibration [O image]: descriptor 0x648E0 +4 / +8; descriptor 0x64BC8
 * +0 (the step table) / +4 / +8 */
#define TS_CAL_SUB2         0x00u   /* 0x195DB7: offset on the second byte */
#define TS_CAL_DEFAULT      0x82u   /* 0x195DB8: both invalid → 90 °C */
#define TS_CAL_JUMP_SET     0x14u   /* 0x195DE7: |in − last| > 20 → jump */
#define TS_CAL_JUMP_CLR     0x05u   /* 0x195DE8: ≤ 5 → clear */
extern const uint8_t TS_STEP_TBL[16];   /* 0x191E44: u32 n = 6, x[6], y[6] */

void    tmp_safe_init(void);
void    tmp_safe_mode0_10ms(void);
void    tmp_safe_boot_pass(void);
void    tmp_safe_100ms(void);

/* 0x140050: the application getter */
uint8_t tmp_safe_byte(void);
/* 0x151DD8 (the limiter's step table lookup), exported for the host test */
uint8_t tmp_safe_151dd8(const uint8_t *tbl, uint8_t x);

#endif /* TCU_SAFETY_TMP_SAFE_H */

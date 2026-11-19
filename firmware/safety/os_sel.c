/* os_sel.c — G5b: THE OS-MODE REQUEST PATH, LIVE (os_sel.h has the map,
 * the stand-ins and what is not ported; ~/tcu_sweep/g5b_port.md).
 *
 * Writes ONLY: the selector object (OS_SEL_OBJ, r04r's retired pad), the
 * echo's last-seen cell 0xFFF93B56, the six one-hot flags 0x89C3C..3E /
 * 0x89C53..55 and the model request 0x93B5E (CELL_OS_REQ_APP) — the one
 * cell bsw/sup/sup_mode.c reads to switch the OS mode (0x6F0B4 → 0x66E06).
 * No .bss / .data of its own. */
#include "safety/os_sel.h"
#include <stdbool.h>

#if defined(TCU_RAM_APP)

/* the RAM application has no mode-2 path (it stays in mode 1, unchanged) */

#else

#include "bsw/sup/sup_cells.h"
#include "platform.h"

#define O        (*(volatile os_sel_t *)(uintptr_t)OS_SEL_OBJ)
#define ECHO_LAST (*(volatile uint8_t *)(uintptr_t)OS_SEL_ECHO_LAST)
#define FLG(a)   (*(volatile uint8_t *)(uintptr_t)(a))

void os_sel_init(void)
{
    /* the stock: all bss (DTBL 0x415B0 covers 0xFFF84000..0xFFF85354
     * only) — zero; +0 holds the stock table pointer as a marker */
    O.tbl = OS_SEL_TBL;
    O.eng = 0u;
    O.eng_st = 0u;
    O.req = 0u;
    O.app = 0u;
    O.f584a = 0u;
    O.e9ff = 0u;
    O.tur_st = 0u;
    O.tur = 0u;
    O.sup = 0u;
    O.n = 0u;
    O.n2 = 0u;
    O.t2 = 0u;
    O.chg = 0u;
    O.hand = 0xFFu;
    O.spd = 0u;
    ECHO_LAST = 0u;
    FLG(OS_FLG_C3C) = 0u;
    FLG(OS_FLG_C3D) = 0u;
    FLG(OS_FLG_C3E) = 0u;
    FLG(OS_FLG_C53) = 0u;
    FLG(OS_FLG_C54) = 0u;
    FLG(OS_FLG_C55) = 0u;
}

/* 0x68EA8 [O bytes 0x68EA8..0x68EFC]: r6 = (s8)0x848CF; cmp/hs #4 → 1;
 * if extu.b != 0x93B56: 1 / 2 / 3 → +8 (0x1320BC / C4 / CC); 0x93B56 = it */
void os_sel_echo_10ms(uint8_t mode)
{
    uint8_t v;

    if (mode < 1u || mode > 3u) {
        return;                         /* the pre-hook of the modes 1..3 objects */
    }
    v = CELL_OS_MODE_BOOT;              /* 0x848CF */
    if (v >= 4u) {                      /* cmp/hs on the sign-extended byte: 0x80.. too */
        v = 1u;
    }
    if (v != ECHO_LAST) {
        if (v >= 1u && v <= 3u) {
            O.app = v;                  /* 0x95484 */
            if (v == 2u && O.t2 == 0u) {
                O.t2 = plat_frt_us();   /* ours: the witness of the first mode 2 */
            }
        }
        ECHO_LAST = v;
    }
}

/* the law of 0x1418E2 [O bytes 0x1418E2..0x141996] */
uint8_t os_sel_law(uint8_t cur, uint8_t e9ff, uint8_t f584a, uint16_t eng, uint8_t eng_st,
                   uint16_t tur, uint8_t tur_st, uint16_t sup, uint8_t *spd)
{
    uint8_t req = cur;                  /* 0x1418FE: +7 = +8 */
    /* engine: mov.w (sign-extended) cmp/hs mov.w 150 → u32 compare, then
     * +6 tst; turbine: movu.w cmp/ge movu.w 100 (signed, both >= 0), then
     * E7EC mov.b tst */
    const bool eng_ok = (uint32_t)(int32_t)(int16_t)eng >= OS_SEL_ENG_MIN && eng_st == 0u;
    const bool tur_ok = (int32_t)tur >= (int32_t)OS_SEL_TUR_MIN && tur_st == 0u;
    const bool s = eng_ok || tur_ok;

    if (spd != 0) {
        *spd = s ? 1u : 0u;
    }
    if (cur == 1u) {
        if (e9ff == 2u) {
            if (f584a == 0u) {
                req = 3u;
            } else if (s && (int32_t)OS_SEL_SUP_MIN < (int32_t)sup &&
                       (int32_t)OS_SEL_SUP_MAX > (int32_t)sup) {
                req = 2u;               /* cmp/ge r5,r7 exits on 6000 >= sup; cmp/gt on 17000 > sup */
            }
        }
    } else if (cur == 2u) {
        if (e9ff == 4u) {
            req = 1u;                   /* 0x141970 */
        }
        if (f584a == 0u && e9ff == 2u) {
            req = 3u;
        }
    } else if (cur == 3u) {
        if (e9ff == 4u || f584a == 1u) {
            req = 1u;
        }
    } else {
        req = 1u;                       /* 0x141990 */
    }
    return req;
}

void os_sel_tick_10ms(uint8_t mode, const os_sel_in_t *in)
{
    uint8_t req, spd = 0u;

    if (mode < 1u || mode > 3u) {
        return;                         /* 0x14F34C: the 10 ms lists of modes 1..3 */
    }
    /* 0x1507BE..C4: +4 / +6 ← E90E / E910 (0x13BF66: E90E = the raw word
     * when the status is valid and it is not 0xFFFF, else held; E910 0 / 2) */
    if (in->eng_valid != 0u && in->eng != 0xFFFFu) {
        O.eng = in->eng;
        O.eng_st = 0u;
    } else {
        O.eng_st = 2u;
    }
    O.f584a = 1u;                       /* 0x14DC70 stand-in (D-G5-3) */
    O.e9ff = in->e9ff;
    O.tur = in->tur;
    O.tur_st = (uint8_t)((in->tur_ok != 0u) ? 0u : 1u);
    O.sup = in->sup;
    req = os_sel_law(O.app, O.e9ff, O.f584a, O.eng, O.eng_st, O.tur, O.tur_st, O.sup, &spd);
    if (req != O.req) {
        O.chg = (uint16_t)(O.chg + 1u);
    }
    O.req = req;                        /* 0xFFF95483 */
    O.spd = spd;
    O.n = O.n + 1u;
    if (req == 2u) {
        O.n2 = O.n2 + 1u;
    }
}

/* slot 26 [O bytes 0xB9824..0xB986A], r4 = 0xFFF89C3C */
void os_sel_flags_10ms(uint8_t mode)
{
    if (mode == 1u) {                   /* 0xB983C */
        FLG(OS_FLG_C55) = 1u;
        FLG(OS_FLG_C3D) = 1u;
        FLG(OS_FLG_C54) = 0u;
        FLG(OS_FLG_C3C) = 0u;
        FLG(OS_FLG_C53) = 0u;
        FLG(OS_FLG_C3E) = 0u;
    } else if (mode == 2u) {            /* 0xB9854 */
        FLG(OS_FLG_C53) = 1u;
        FLG(OS_FLG_C3E) = 1u;
        FLG(OS_FLG_C54) = 0u;
        FLG(OS_FLG_C3C) = 0u;
        FLG(OS_FLG_C55) = 0u;
        FLG(OS_FLG_C3D) = 0u;
    } else if (mode == 3u) {            /* 0xB9824 */
        FLG(OS_FLG_C54) = 1u;
        FLG(OS_FLG_C3C) = 1u;
        FLG(OS_FLG_C53) = 0u;
        FLG(OS_FLG_C3E) = 0u;
        FLG(OS_FLG_C55) = 0u;
        FLG(OS_FLG_C3D) = 0u;
    }
}

/* 0x68EFE [O bytes 0x68EFE..0x68F1E] under 0x9A428 / 0x9A3FC (0x6D6EC):
 * r = 0x95483 (0x1320D4 movu.b); if (s8)0x848CF != 0 && r <= 3 → 0x93B5E */
void os_sel_handoff_10ms(uint8_t mode)
{
    uint32_t prev;
    uint8_t r;

    if (mode < 1u || mode > 3u) {
        return;                         /* the post-hook of the modes 1..3 objects */
    }
    prev = plat_sr_i();
    if (prev < 1u) {
        plat_sr_set(1u);                /* 0x9A428: the OS lock (cal 0x41174 = 0x10) */
    }
    r = O.req;
    if (CELL_OS_MODE_BOOT != 0u && r <= 3u) {
        CELL_OS_REQ_APP = r;
        O.hand = r;
    }
    plat_sr_set(prev);                  /* 0x9A3FC */
}

const volatile os_sel_t *os_sel_state(void)
{
    return &O;
}

uint8_t os_sel_req(void)
{
    return O.req;
}

#endif /* TCU_RAM_APP */

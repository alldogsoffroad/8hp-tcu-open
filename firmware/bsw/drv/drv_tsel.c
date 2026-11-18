/* drv_tsel.c — the selected ECU temperature, live (stage 2a; drv_tsel.h).
 * Output path (TCU_OUTPUT_PATH): the values are handed in by value from the
 * temperature hub hook (app/tsel_live.c); this TU knows only the stock
 * cells. Compiled for the code-flash application and the host; empty in the
 * RAM app and the loader. */
#define TCU_OUTPUT_PATH 1
#include "bsw/drv/drv_tsel.h"

#if !defined(TCU_RAM_APP) && !defined(TCU_LOADER)
#define C_S16(a) (*(volatile int16_t *)(uintptr_t)(a))
#define C_U8(a)  (*(volatile uint8_t *)(uintptr_t)(a))

void drv_tsel_apply(drv_tsel_val_t v)
{
    /* 0x88DA8 → 0x8DD7C: the board (0x9393E / 0x94100), its byte 0x94101 */
    C_S16(0xFFF9393Eu) = v.t_brd;
    C_U8(0xFFF94100u) = v.st_brd;
    C_U8(0xFFF94101u) = v.b_brd;
    /* 0x88BD0 → 0x8DB5A: the die (0x93ABA / 0x9415E) */
    C_S16(0xFFF93ABAu) = v.t_die;
    C_U8(0xFFF9415Eu) = v.st_die;
    /* 0x88BD0: the die copies, the die byte, THE SELECTION */
    C_S16(0xFFF9393Cu) = v.t_die_cp;
    C_U8(0xFFF940FDu) = v.st_die_cp;
    C_U8(0xFFF940FEu) = v.b_die;
    C_S16(0xFFF9393Au) = v.t_sel;
    C_U8(0xFFF940FCu) = v.st_sel;
    C_U8(0xFFF94170u) = v.src;
    C_U8(0xFFF940FFu) = v.b_sel;
}

#endif

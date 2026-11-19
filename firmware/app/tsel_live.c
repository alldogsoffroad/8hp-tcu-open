/* tsel_live.c — stage 2a: the temperature shadow's results of this hub pass
 * handed BY VALUE to the output side (bsw/drv/drv_tsel.c), which writes the
 * stock's cells 0x9393A / 0x940FC / 0x94170 / 0x940FF / 0x940FE / 0x94101
 * and the die / board cells, and — stage 2b, TSEL_LIVE_FLAGS — the fault
 * flags 0x940E1 / E2 / E4 / E5 (0x7C01E / 0x7C0B6's results). Called by the hub hook (app/tsel_hook.c) right after
 * tsel_shadow_pass — the stock's 0x88BD0 slot, after the fluid 0x887D8 and
 * before the law in the same tick — and once after the init hub (0x832AC
 * writes 0x93ABA / 0x9393A / 0x9393E = 0x0780), both only under TSEL_LIVE
 * (bsw/drv/drv_tsel.h: the SH application). Host tests built -DTSEL_LIVE=1
 * call it from their own hub hook. Like the hook, it names no live cell. */
#include "app/tsel_shadow.h"
#include "bsw/drv/drv_tsel.h"

#include <stdint.h>

#if !defined(TCU_RAM_APP) && !defined(TCU_LOADER)
void tsel_live_apply(void)
{
    const volatile tsel_t *t = tsel_shadow_state();
    drv_tsel_val_t v;
    v.t_sel = t->s93a;
    v.t_die = t->aba;
    v.t_die_cp = t->s93c;
    v.t_brd = t->s93e;
    v.st_sel = t->s0fc;
    v.src = t->s170;
    v.b_sel = t->s0ff;
    v.b_die = t->s0fe;
    v.b_brd = t->s101;
    v.st_die = t->s15e;
    v.st_die_cp = t->s0fd;
    v.st_brd = t->s100;
    v.e1 = t->e1;               /* stage 2b: written only under TSEL_LIVE_FLAGS */
    v.e2 = t->e2;
    v.e4 = t->e4;
    v.e5 = t->e5;
    drv_tsel_apply(v);
}
#endif

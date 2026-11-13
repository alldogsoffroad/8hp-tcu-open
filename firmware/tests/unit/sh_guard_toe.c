/* SH-only compile check of the Timer E stage-0b safety frame (CMakeLists:
 * toe_sh_guard_*): built with __sh__ defined, -fsyntax-only, against
 * platform/platform.h. The control (no SH_GUARD_* macro) must compile; on
 * the flashed application (no TCU_RAM_APP) the group-A duty setter and the
 * M4b output switch are refused. The RAM application with TCU_M4B_OUTPUTS
 * (the only SH build that may drive TOE31) must still compile. */
#include "platform/platform.h"

void sh_guard_toe_probe(void);
void sh_guard_toe_probe(void)
{
#if defined(SH_GUARD_TOE_SET) || defined(SH_GUARD_TOE_RAM)
    plat_toe_out_set(500u);                 /* a group-A duty source */
#endif
}

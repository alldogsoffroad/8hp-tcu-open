/* SH-only compile check of the range chain's writer guard (CMakeLists:
 * rng_sh_guard_*; 2026-11-17, ~/tcu_sweep/range_chain_live_port.md):
 * -fsyntax-only with __sh__.
 *   control                 reads the range through app/park_cells.h — compiles;
 *   SH_GUARD_RNG_WRITE      writes PCELL_TGT outside the chain — must fail
 *                           (the cell macros are const: "read-only");
 *   SH_GUARD_RNG_ISOLATED   the chain's translation unit (TCU_RNG_CHAIN_TU)
 *                           including an output-side header — must fail.
 * Also the negative fixture of range_chain_writer_scan.py: it names the
 * writer macros and the stock addresses, so the scan must fail on it. */
#if defined(SH_GUARD_RNG_ISOLATED)
#define TCU_RNG_CHAIN_TU 1
#include "bsw/drv/drv_park.h"
#endif
#include "app/park_cells.h"

void sh_guard_rng_probe(void);
void sh_guard_rng_probe(void)
{
    volatile uint8_t sink = PCELL_TGT;              /* reading the range is fine */
    (void)sink;
#if defined(SH_GUARD_RNG_WRITE)
    PCELL_TGT = 7u;                                 /* must not compile */
#endif
#if defined(RNG_SCAN_FIXTURE_NEVER_DEFINED)
    PCELL_TGT_W = 7u;                               /* scan fixture only: never compiled */
    *(volatile uint8_t *)0xFFF8D6D6u = 7u;
    *(volatile uint32_t *)0xFFF8304Cu = 7u;
#endif
}

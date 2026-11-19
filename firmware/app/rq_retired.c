/* rq_retired.c — r04r (2026-11-19, ~/tcu_sweep/r04r_port.md): THE LIVE HOLDER
 * IS RETIRED FROM THE CODE-FLASH APPLICATION.
 *
 * Up to r04q the image linked app/rq_prod.c (the M5-0 holder-only 0xDC4C4
 * with the compile-time M5-0 input frame) and its own fill instance
 * app/rq_fill.c. Since G6b the engine (app/gs_cand.c, instance 2 of the
 * garage engine template, with its fill app/gs_cand_fill.c) produced A..E;
 * r04r gives it WK / Sys (ch 5 / 6) and the start of the producer content
 * too (bsw/drv/drv_reqlaw.c rq_prod_guarded, app/gs_bridge.c
 * drv_gsbr_take), so the holder has no consumer left and is no longer
 * linked. app/rq_prod.c and app/rq_fill.c stay in the tree as the HOST
 * reference model of the M5 tests (test_m5prod, test_m5fill*, ...) and as
 * the retired holder the G6b harness replays so the r04q goldens' cell
 * hashes stay comparable (tests/unit/test_gs_bridge_chain.c
 * G6R_HOLDER_REPLICA).
 *
 * THIS FILE takes their place in the link (CMakeLists.txt CORE_SOURCES, the
 * same position) so that .bss does not move: the two retired objects held
 * 0x20 bytes of .bss at 0xFFF95F7C..0xFFF95F9B in r04q (rq_prod.c s_started
 * 1 B + pad + s_in 4 B; rq_fill.c s_tb_89cf8, s_rec_frac, s_f19f, s_unp,
 * s_in) — this pad occupies exactly that span, 4-aligned, so every later
 * .bss symbol and all of .noinit keep their r04q addresses (the release
 * window, the monitor's windows / latch, the reset record). It is a GC root
 * (linker.ld EXTERN) and is never read or written.
 *
 * The holder's stock-address reservations (.m5_cur / .m5_act / .m5_req /
 * .m5_in*, .m5_fl, .m5_e384, .m5_rec, .m5_wit) stay in linker.ld as empty
 * reservations: the cells are stock RAM nothing of ours writes any more
 * (drv_reqlaw.c's guard witness s_m5_guard_n keeps its .m5_wit address). */
#include <stdint.h>

#if defined(__sh__) && !defined(TCU_RAM_APP)
uint8_t tcu_rq_retired_pad[0x20] __attribute__((aligned(4)));
#endif

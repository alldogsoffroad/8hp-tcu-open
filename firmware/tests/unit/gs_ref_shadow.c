/* gs_ref_shadow.c — INSTANCE 1 of the garage engine template, HOST-ONLY
 * since G6c (2026-11-19, ~/tcu_sweep/g6c_port.md; was app/gs_shadow.c).
 * D-G6-7 deleted the shadow from the application (its flash and RAM
 * returned). The host tests keep this binding as the engine's INSTRUMENTED
 * REFERENCE: its recorders (GSE_REC 1) drive test_gs_shadow's engine-logic
 * checks, and tests/unit/gs_dual.c holds the image's engine (instance 2,
 * app/gs_cand.c) byte for byte to it. Never in an SH build.
 * (r04l..r04o: the READ-ONLY garage-engagement shadow, as r04l built it.)
 *
 * G6-0 (2026-11-19, ~/tcu_sweep/g6_0_port.md; plan g6_migration_plan.md
 * §3 G6-0): the engine's code moved to app/gs_engine.c so that a second
 * binding can compile it against other RAM. This binding renames nothing
 * and binds nothing new: the engine's statics land in this TU (the linker's
 * *gs_shadow.c.obj globs and the .gs_* sections are unchanged), its
 * request objects are gs_fill.h GS_REQ_BASE (GSE_REQ_BASE left at its
 * default), its fill instance is app/gs_fill.c (gsf_*). The SH object code
 * of this TU is the r04l object code (the G6-0 identity gates).
 *
 * The read-only frame (gs_shadow.h) is unchanged: TCU_GS_SHADOW_TU is set
 * by the engine, every live input arrives by value in gs_in_t. */
#if defined(__sh__)
#  error "gs_ref_shadow.c: instance 1 (the shadow) is a host-only test reference since G6c"
#endif
#define GSE_BINDING 1
#include "app/gs_engine.c"

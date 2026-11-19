/* gs_cand_fill.c — the fill instance of the G6a CANDIDATE (instance 2 of
 * the garage engine template): app/gs_fill.c (itself a compile of
 * app/rq_fill.c) compiled a THIRD time against the candidate's request
 * objects and cells, every export renamed gcf_* (app/gs_cand_bind.h).
 * G6-0 (2026-11-19, ~/tcu_sweep/g6_0_port.md) made it host-only; G6a
 * (~/tcu_sweep/g6a_port.md) links it into the code-flash application: its
 * statics land in .gsc_obj (linker.ld glob), its request objects are
 * .gsc_req. */
#define GSC_FILL_TU 1
#include "app/gs_cand_bind.h"
#include "app/gs_fill.c"

#if !GSC_SH_FORM
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

/* G6-0 host form: the candidate's request objects in their own host page,
 * outside the stock cell window the host tests map and hash
 * (tests/unit/stub_plat.c). The G6a form (SH, GSC_G6A) puts them at
 * 0xFFF925C0, inside that window — the identity tests hash it and show
 * that only the candidate's own block changes. */
static void gcf_map(void) __attribute__((constructor));
static void gcf_map(void)
{
    void *p = mmap((void *)(uintptr_t)GSC_REQ_BASE, GSC_REQ_PAGE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS
#ifdef MAP_FIXED_NOREPLACE
                   | MAP_FIXED_NOREPLACE
#else
                   | MAP_FIXED
#endif
                   , -1, 0);
    if (p == MAP_FAILED || (uintptr_t)p != (uintptr_t)GSC_REQ_BASE) {
        perror("gs_cand: candidate object page mmap failed");
        exit(1);
    }
}
#endif

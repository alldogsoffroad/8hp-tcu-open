/* gs_dual.c — G6-0 two-instance identity, see gs_dual.h. */
#include "tests/unit/gs_dual.h"
#include "app/gs_cand.h"
#include "app/gs_fill.h"

#include <stdio.h>
#include <string.h>

/* rq_fill.c's export in the shadow's fill instance (app/gs_fill.c) */
uint16_t gsf_rec_frac(uint8_t idx);

static uint32_t s_pass, s_bad, s_bytes, s_cur_bad;
static const char *const k_blk[] = {
    "gs_t", "gs_wit_t", "gs_prog_t", "recorder slot 0", "recorder slot 1", "gs_rel_t",
    "request objects", "fill cells", "park model latch", "park request latch", "boot flag",
};

void gs_dual_init(void)
{
    gs_cand_init();
}

void gs_dual_standins(uint8_t mask, int16_t bd76, uint8_t bd30)
{
    gs_shadow_host_standins(mask, bd76, bd30);
    gs_cand_host_standins(mask, bd76, bd30);
}

static void diff_note(const char *what, unsigned k, unsigned off, unsigned a, unsigned b)
{
    if (s_bad < 6u) {
        printf("FAIL gs_dual pass %u: %s", s_pass, what);
        if (k != ~0u) {
            printf(" [%s +0x%X]", k < sizeof k_blk / sizeof k_blk[0] ? k_blk[k] : "?", off);
        }
        printf(": shadow 0x%X, candidate 0x%X\n", a, b);
    }
}

void gs_dual_tick(const gs_in_t *gi)
{
    bool bad = false;
    gs_cand_tick_10ms(gi);
    s_pass++;
    for (uint8_t k = 0u;; k++) {
        uint32_t na = 0u, nb = 0u;
        const volatile uint8_t *a = (const volatile uint8_t *)gs_shadow_host_state(k, &na);
        const volatile uint8_t *b = (const volatile uint8_t *)gs_cand_host_state(k, &nb);
        if (a == NULL || b == NULL) {
            if (a != b) {
                diff_note("block lists differ", k, 0u, 0u, 0u);
                bad = true;
            }
            break;
        }
        if (na != nb) {
            diff_note("block size", k, 0u, na, nb);
            bad = true;
            continue;
        }
        for (uint32_t i = 0u; i < na; i++) {
            s_bytes++;
            if (a[i] != b[i]) {
                if (!bad) {
                    diff_note("state byte", k, i, a[i], b[i]);
                }
                bad = true;
                break;
            }
        }
    }
#define CMP(fa, fb, name) do { const unsigned x_ = (unsigned)(fa), y_ = (unsigned)(fb); \
        if (x_ != y_) { if (!bad) { diff_note(name, ~0u, 0u, x_, y_); } bad = true; } } while (0)
    {
        bool cb = false;
        for (uint8_t c = 0u; c < 7u; c++) {
            cb = cb || gs_shadow_cur(c) != gs_cand_cur(c);
            CMP(gs_shadow_cur(c), gs_cand_cur(c), "current");
        }
        if (cb) {
            s_cur_bad++;            /* the engine's OUTPUT (ch 0..6 currents) differs */
        }
    }
    for (uint8_t c = 0u; c < 5u; c++) {
        CMP(gs_shadow_reqmode(c), gs_cand_reqmode(c), "request mode");
        CMP(gsf_rec_frac(c), gcf_rec_frac(c), "fill rec_frac");
    }
    for (uint8_t k = 0u; k < 3u; k++) {
        CMP(gs_shadow_phase(k), gs_cand_phase(k), "phase");
    }
    CMP(gs_shadow_pf(), gs_cand_pf(), "power flow");
    CMP((uint8_t)gs_shadow_gear(), (uint8_t)gs_cand_gear(), "gear code");
    CMP(gs_shadow_pending(), gs_cand_pending(), "pending");
    CMP(gs_shadow_e6ef(), gs_cand_e6ef(), "E6EF");
    CMP(gs_shadow_ae28(), gs_cand_ae28(), "AE28");
    CMP(gs_shadow_d0f(), gs_cand_d0f(), "0x89D0F");
    CMP(gs_shadow_releasing(), gs_cand_releasing(), "releasing");
    CMP((uint16_t)gs_shadow_bd76(), (uint16_t)gs_cand_bd76(), "BD76");
    CMP(gs_shadow_bd30(), gs_cand_bd30(), "BD30");
    CMP(gs_shadow_sync(), gs_cand_sync(), "sync");
    CMP((uint16_t)gs_shadow_slip(), (uint16_t)gs_cand_slip(), "slip");
    CMP(gsf_unp_n(), gcf_unp_n(), "fill unported count");
    CMP(gsf_f19f_n(), gcf_f19f_n(), "fill 0x19F count");
    CMP(gsf_shift_n(), gcf_shift_n(), "fill shift-source count");
    CMP(gsf_e469c_n(), gcf_e469c_n(), "fill 0xE469C count");
#undef CMP
    if (bad) {
        s_bad++;
    }
}

uint32_t gs_dual_passes(void) { return s_pass; }
uint32_t gs_dual_bad(void)    { return s_bad; }

void gs_dual_report(const char *where)
{
    printf("gs_dual (%s): %u passes compared (%u state bytes), %u with a difference, "
           "%u with a current (output) difference\n", where, s_pass, s_bytes, s_bad, s_cur_bad);
}

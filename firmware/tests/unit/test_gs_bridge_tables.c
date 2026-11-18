/* test_gs_bridge_tables.c — the garage bridge's compile-time element bounds
 * (app/gs_bridge.c gs_bridge_table) derived independently from the stock
 * image's bytes (~/tcu_sweep/garage_g1_port.md; plan §2.2):
 *   role tables 0x6312F / 0x63139 / 0x63143 [g] (first / second / third
 *   element of target gear g; 1 A 2 B 3 C 4 D 5 E), D = gear 1, R = gear −1;
 *   table 0x505A4[e + (g + 1)·6 + block·0x3C] (1 = applied);
 *   the live P content = block 0, gear 1 (RQ_M5_STAGE 1B);
 *   the mode-1 current law 0x107CFC: 8500 for an applied normally-closed
 *   (A, B) or a released normally-open (C, D, E) element, else 500 — the
 *   types as the r03f P set shows them (A 8500 applied, B 500 released,
 *   C / D / E 8500 released).
 * Moving = the first and third elements (the second is dropped while it is
 * already applied in the live content — 0xDC282); every other channel is
 * fixed and must have ONE value in both the live row and the target row.
 * Exit 77 without the image. The GSB_HOST_MUT_TABLE build must FAIL. */
#include "app/gs_bridge.h"
#include <stdio.h>

int main(void)
{
    static unsigned char img[0x200000];
    static const uint8_t CH_OF_E[6] = { 0xFFu, 0u, 2u, 4u, 1u, 3u };    /* A B C D E → ch */
    static const uint8_t NC[6] = { 0u, 1u, 1u, 0u, 0u, 0u };            /* A, B normally closed */
    FILE *f = fopen(TCU_STOCK_IMAGE, "rb");
    unsigned bad = 0u;
    if (f == NULL) {
        printf("SKIP no stock image %s\n", TCU_STOCK_IMAGE);
        return 77;
    }
    if (fread(img, 1u, sizeof img, f) != sizeof img) {
        fclose(f);
        printf("FAIL short image\n");
        return 1;
    }
    fclose(f);
    for (uint8_t pf = 1u; pf <= 2u; pf++) {
        const int g = (pf == 1u) ? 1 : -1;
        const uint8_t e1 = img[0x6312Fu + (unsigned)g], e2 = img[0x63139u + (unsigned)g];
        const uint8_t e3 = img[0x63143u + (unsigned)g];
        uint8_t move = 0u, tmove;
        uint16_t fixed[5] = { 0u, 0u, 0u, 0u, 0u }, tfix[5];
        bool e2_live_applied;
        if (e1 < 1u || e1 > 5u || e2 < 1u || e2 > 5u || e3 < 1u || e3 > 5u) {
            printf("FAIL pf %u: role bytes %u %u %u\n", pf, e1, e2, e3);
            return 1;
        }
        /* 0xDC282: the second element dropped while applied in the live content */
        e2_live_applied = img[0x505A4u + e2 + (1u + 1u) * 6u] != 0u;
        move = (uint8_t)((1u << CH_OF_E[e1]) | (1u << CH_OF_E[e3]) |
                         (e2_live_applied ? 0u : (1u << CH_OF_E[e2])));
        for (uint8_t e = 1u; e <= 5u; e++) {
            const uint8_t ch = CH_OF_E[e];
            const bool live = img[0x505A4u + e + (1u + 1u) * 6u] != 0u;            /* block 0 gear 1 */
            const bool tgt = img[0x505A4u + e + (unsigned)(g + 1) * 6u + 0x3Cu] != 0u;  /* block 1 gear g */
            const uint16_t vl = (live == (NC[e] != 0u)) ? 8500u : 500u;
            const uint16_t vt = (tgt == (NC[e] != 0u)) ? 8500u : 500u;
            if (((move >> ch) & 1u) != 0u) {
                continue;
            }
            if (vl != vt) {
                printf("FAIL pf %u: fixed channel %u holds %u live but %u in the target row\n", pf, ch, vl, vt);
                bad++;
            }
            fixed[ch] = vl;
        }
        if (!gs_bridge_table(pf, &tmove, tfix)) {
            printf("FAIL pf %u: no table\n", pf);
            return 1;
        }
        printf("pf %u (%s): roles %u %u %u (second %s), moving mask %02X (bridge %02X), fixed",
               pf, pf == 1u ? "D, gear 1" : "R, gear -1", e1, e2, e3,
               e2_live_applied ? "dropped: applied in the live content" : "kept", move, tmove);
        for (uint8_t ch = 0u; ch < 5u; ch++) {
            printf(" %u/%u", fixed[ch], tfix[ch]);
            if (fixed[ch] != tfix[ch]) {
                bad++;
            }
        }
        printf("\n");
        if (move != tmove) {
            bad++;
        }
    }
    printf("gs_bridge tables vs the image: %u mismatches\n", bad);
    return bad == 0u ? 0 : 1;
}

/* test_g6c_windows.c — G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): THE
 * MONITOR'S .noinit WINDOWS OVER A RESET. The r04n bridge-logic tests that
 * carried this check (test_gs_bridge_live: the windows over a warm reset)
 * went with the r04n bridge form; gs_bridge_init's window rules are
 * unchanged code and stay tested here:
 *   power-on (cold)             → a fresh budget: acc 0, commits 0, latch 0;
 *   warm reset, guard intact    → carried: acc, commits, latch as they were
 *                                 (a 30 s budget already spent stays latched);
 *   warm reset, guard broken    → FAIL SAFE: acc = 30 s, commits = 17, latch
 *                                 WIN | ENGT | COMMITS | GUARD, broken = 1.
 * The window object is written through a cast here to emulate the RAM a
 * reset leaves behind (the firmware itself never does). */
#include "app/gs_bridge.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
    printf("\n"); g_fails++; } } while (0)

static gsb_win_t *w(void) { return (gsb_win_t *)(uintptr_t)gs_bridge_win(); }
static void seal(gsb_win_t *x)
{
    x->magic = GSB_WIN_MAGIC;
    x->acc_inv = ~x->acc_ms;
    x->commits_inv = (uint16_t)~x->commits;
    x->latch_inv = (uint8_t)~x->latch;
}

int main(void)
{
    gs_bridge_init(true);
    CHECK(w()->magic == GSB_WIN_MAGIC && w()->acc_ms == 0u && w()->commits == 0u && w()->latch == 0u &&
          w()->warm == 0u && w()->broken == 0u, "cold: a fresh, sealed window");
    /* a power cycle's use: 12 s armed, 3 commits, no latch — then a warm reset */
    w()->acc_ms = 12000u;
    w()->commits = 3u;
    seal(w());
    gs_bridge_init(false);
    CHECK(w()->acc_ms == 12000u && w()->commits == 3u && w()->latch == 0u && w()->warm == 1u && w()->broken == 0u,
          "warm: carried (acc %u commits %u latch %02X warm %u broken %u)", (unsigned)w()->acc_ms, w()->commits,
          w()->latch, w()->warm, w()->broken);
    /* a monitor latch (e.g. PERMIT) survives a warm reset */
    w()->latch = GSB_L_PERMIT;
    seal(w());
    gs_bridge_init(false);
    CHECK(w()->latch == GSB_L_PERMIT && w()->warm == 2u, "warm: the PERMIT latch carried (%02X)", w()->latch);
    /* the 30 s budget spent: latched WIN at the reset */
    w()->latch = 0u;
    w()->acc_ms = GSB_WIN_MS;
    seal(w());
    gs_bridge_init(false);
    CHECK((w()->latch & GSB_L_WIN) != 0u, "warm: a spent budget latches WIN (%02X)", w()->latch);
    /* a torn guard: fail safe */
    w()->acc_ms = 100u;
    w()->latch = 0u;
    seal(w());
    w()->commits_inv ^= 1u;
    gs_bridge_init(false);
    CHECK(w()->broken == 1u && w()->acc_ms == GSB_WIN_MS && w()->commits == GSB_COMMITS_MAX + 1u &&
          w()->latch == (GSB_L_WIN | GSB_L_ENGT | GSB_L_COMMITS | GSB_L_GUARD),
          "broken guard: spent and latched (acc %u commits %u latch %02X broken %u)", (unsigned)w()->acc_ms,
          w()->commits, w()->latch, w()->broken);
    /* power-on again: fresh */
    gs_bridge_init(true);
    CHECK(w()->acc_ms == 0u && w()->commits == 0u && w()->latch == 0u && w()->broken == 0u, "cold after: fresh");
    if (g_fails == 0) {
        printf("test_g6c_windows: ok\n");
    }
    return g_fails != 0;
}

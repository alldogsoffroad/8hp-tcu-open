/* g6c_gold.h — G6c: THE r04o GOLDENS OF test_g6b (2026-11-19,
 * ~/tcu_sweep/g6c_port.md). Included by tests/unit/test_g6b.c after its
 * res_t / gr_t / tick_t are defined.
 *
 * G6c deletes the shadow (instance 1) and the r04n bridge form, so the
 * one-binary "r04n form vs G6b form" comparison of G6b is gone. Instead the
 * G6b form of r04o (HEAD fff32bd) wrote, for every run of test_g6b (every
 * scenario, the base run, every monitor negative, both 10 ms parities):
 *   F  every frame:  the wire hash (every bus-A word in wire order + the
 *      park demands) and the cell-window hash (0xFFF80000..0xFFF96000; the
 *      shadow's own RAM and the candidate's request objects excluded, as
 *      the harness always hashed);
 *   T  every 10 ms tick: frame, monitor pass, the seam source, the latch,
 *      the fence, the trip count, the lever, the five staged A..E words;
 *   B  the monitor state gsb_t and the .noinit windows gsb_win_t, byte for
 *      byte, at the end of the run;
 *   R  the release header fields the monitor keeps;
 *   a binary file with the whole cell window at the end of the run.
 * G6C_GOLD_OUT=<dir> writes them (the HEAD build); G6C_GOLD_IN=<dir>
 * compares the current build against them (g6c_gold_cmp returns the number
 * of differences and prints the first of each kind).
 *
 * Expected differences by construction (G6c): the end-of-run window bytes
 * of the shadow's request objects (0xFFF911A0..0xFFF91440, its only
 * absolute-address RAM on the host — never hashed) and, in the one negative
 * that injected candidate != shadow, the monitor's SHADOW check (deleted
 * with the shadow). Everything else must be byte-identical. */
#ifndef G6C_GOLD_H
#define G6C_GOLD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define G6C_SH_REQ_LO   0xFFF911A0u     /* the shadow's request objects (host absolute RAM) */
#define G6C_SH_REQ_HI   0xFFF91440u

/* r04r (~/tcu_sweep/r04r_port.md): G6R_NOREP — the harness WITHOUT the
 * retired holder's replica (G6R_HOLDER_REPLICA 0: the image's configuration)
 * against goldens written WITH it. The wire hash (every bus-A word + the
 * park demands) must be identical on every frame; the cell-window hash
 * cannot be (the holder's own stock cells are no longer written), so it is
 * not compared per frame — instead the end-of-run window must be identical
 * OUTSIDE the retired holder's cells, listed here (every one a literal
 * address app/rq_prod.c / app/rq_fill.c write: the current array 0x84B78
 * + complements, the actuator objects 0x8B154, the request objects 0x8B7C8,
 * the write-only input mirrors 0x89C2B / 0x89C30 / 0x89CF5 / 0x8BCB0 /
 * 0x89D10, the fill cells 0x851A2..5 and 0x8E384). Ticks, gsb_t, the
 * windows and the release header are compared exactly. */
static const uint32_t G6R_HOLDER_CELLS[][2] = {
    { 0xFFF84B78u, 0xFFF84BA8u }, { 0xFFF8B154u, 0xFFF8B23Cu }, { 0xFFF8B7C8u, 0xFFF8B9C8u },
    { 0xFFF89C2Bu, 0xFFF89C2Cu }, { 0xFFF89C30u, 0xFFF89C31u }, { 0xFFF89CF5u, 0xFFF89CF6u },
    { 0xFFF8BCB0u, 0xFFF8BCB2u }, { 0xFFF89D10u, 0xFFF89D11u }, { 0xFFF851A2u, 0xFFF851A6u },
    { 0xFFF8E384u, 0xFFF8E385u },
};
static int g6r_holder_cell(uint32_t abs)
{
    for (unsigned k = 0u; k < sizeof G6R_HOLDER_CELLS / sizeof G6R_HOLDER_CELLS[0]; k++) {
        if (abs >= G6R_HOLDER_CELLS[k][0] && abs < G6R_HOLDER_CELLS[k][1]) {
            return 1;
        }
    }
    return 0;
}
static unsigned long g6r_holder_bytes_differ;   /* G6R_NOREP: the holder cells that differ (info) */

static void g6c_tag(char *out, size_t n, const char *name)
{
    size_t k = 0u;
    for (const char *p = name; *p != '\0' && k + 1u < n; p++) {
        const char c = *p;
        out[k++] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? c : '_';
    }
    out[k] = '\0';
}

static void g6c_hex(FILE *f, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0u; i < n; i++) {
        fprintf(f, "%02X", b[i]);
    }
    fprintf(f, "\n");
}

/* one tick as the golden holds it */
typedef struct {
    unsigned frame, bpass, src, latch, fence, trip_n, lev;
    unsigned st[5];
} g6c_tk_t;

static void g6c_tk_of(const tick_t *t, g6c_tk_t *o)
{
    o->frame = t->frame;
    o->bpass = (unsigned)t->bpass;
    o->src = t->src;
    o->latch = t->g6c_latch;
    o->fence = t->g6c_fence;
    o->trip_n = t->g6c_trip_n;
    o->lev = t->lev;
    for (unsigned c = 0u; c < 5u; c++) {
        o->st[c] = t->st[c];
    }
}

static void g6c_gold_put(const char *name, const gr_t *g, const res_t *r, const uint8_t *win,
                         const tick_t *tks, unsigned tk_n)
{
    const char *dir = getenv("G6C_GOLD_OUT");
    char tag[128], path[512];
    FILE *f;
    if (dir == NULL) {
        return;
    }
    g6c_tag(tag, sizeof tag, name);
    snprintf(path, sizeof path, "%s/%s.txt", dir, tag);
    f = fopen(path, "w");
    if (f == NULL) {
        perror(path);
        return;
    }
    fprintf(f, "RUN %s\n", name);
    for (unsigned k = 0u; k < GFRAMES; k++) {
        fprintf(f, "F %u %016llX %016llX\n", k, (unsigned long long)g->hw[k], (unsigned long long)g->h[k]);
    }
    for (unsigned i = 0u; i < tk_n; i++) {
        g6c_tk_t t;
        g6c_tk_of(&tks[i], &t);
        fprintf(f, "T %u %u %u %u %u %u %u %u %04X %04X %04X %04X %04X\n", i, t.frame, t.bpass, t.src, t.latch,
                t.fence, t.trip_n, t.lev, t.st[0], t.st[1], t.st[2], t.st[3], t.st[4]);
    }
    fprintf(f, "B ");
    g6c_hex(f, &r->b, sizeof r->b);
    fprintf(f, "W ");
    g6c_hex(f, &r->w, sizeof r->w);
    fprintf(f, "R %u %u %u %u %u %u\n", (unsigned)r->rel_t0[0], (unsigned)r->rel_t0[1], (unsigned)r->rel_trip_n,
            (unsigned)r->rel_trip_code, (unsigned)r->relev, (unsigned)r->relref);
    fprintf(f, "END %u\n", tk_n);
    fclose(f);
    snprintf(path, sizeof path, "%s/%s.win", dir, tag);
    f = fopen(path, "wb");
    if (f != NULL) {
        fwrite(win, 1u, WIN_N, f);
        fclose(f);
    }
}

static unsigned g6c_hexcmp(const char *line, const void *p, size_t n, size_t *first)
{
    const uint8_t *b = (const uint8_t *)p;
    unsigned d = 0u;
    *first = (size_t)-1;
    for (size_t i = 0u; i < n; i++) {
        unsigned v = 0u;
        if (sscanf(line + 2u * i, "%2X", &v) != 1 || v != b[i]) {
            if (d == 0u) {
                *first = i;
            }
            d++;
        }
    }
    return d;
}

/* compare the current run against the golden; bmask: gsb_t byte offsets
 * [lo, hi) not compared (the deleted SHADOW check's own fields), 0/0 none */
static unsigned g6c_gold_cmp(const char *name, const gr_t *g, const res_t *r, const uint8_t *win,
                             const tick_t *tks, unsigned tk_n, size_t bm_lo, size_t bm_hi)
{
    const char *dir = getenv("G6C_GOLD_IN");
    char tag[128], path[512];
    static char line[8192];
    FILE *f;
    unsigned fd = 0u, td = 0u, bd = 0u, wd = 0u, ed = 0u, rd = 0u, gtk = 0u;
    unsigned f_first = 0xFFFFFFFFu, t_first = 0xFFFFFFFFu;
    if (dir == NULL) {
        return 0u;
    }
    g6c_tag(tag, sizeof tag, name);
    snprintf(path, sizeof path, "%s/%s.txt", dir, tag);
    f = fopen(path, "r");
    if (f == NULL) {
        printf("   G6c golden: %s missing\n", path);
        return 1u;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        if (line[0] == 'F') {
            unsigned k;
            unsigned long long hw, h;
            if (sscanf(line, "F %u %llX %llX", &k, &hw, &h) == 3 && k < GFRAMES) {
#if defined(G6R_NOREP)
                h = g->h[k];                /* r04r: the cell hash not compared per frame (above) */
#endif
                if (g->hw[k] != hw || g->h[k] != h) {
                    fd++;
                    if (f_first == 0xFFFFFFFFu) {
                        f_first = k + 1u;
                    }
                }
            }
        } else if (line[0] == 'T') {
            g6c_tk_t e, a;
            unsigned i;
            gtk++;
            if (sscanf(line, "T %u %u %u %u %u %u %u %u %X %X %X %X %X", &i, &e.frame, &e.bpass, &e.src, &e.latch,
                       &e.fence, &e.trip_n, &e.lev, &e.st[0], &e.st[1], &e.st[2], &e.st[3], &e.st[4]) == 13) {
                if (i >= tk_n) {
                    td++;
                    continue;
                }
                g6c_tk_of(&tks[i], &a);
                if (memcmp(&a, &e, sizeof a) != 0) {
                    td++;
                    if (t_first == 0xFFFFFFFFu) {
                        t_first = i;
                        printf("   G6c golden %s: first tick difference #%u: golden F%u pass %u src %u latch %02X "
                               "fence %u trips %u words %04X %04X %04X %04X %04X | now F%u pass %u src %u latch %02X "
                               "fence %u trips %u words %04X %04X %04X %04X %04X\n", name, i, e.frame, e.bpass, e.src,
                               e.latch, e.fence, e.trip_n, e.st[0], e.st[1], e.st[2], e.st[3], e.st[4], a.frame,
                               a.bpass, a.src, a.latch, a.fence, a.trip_n, a.st[0], a.st[1], a.st[2], a.st[3],
                               a.st[4]);
                    }
                }
            }
        } else if (line[0] == 'B') {
            gsb_t b = r->b;
            size_t first;
            /* the masked bytes compared as the golden's */
            if (bm_hi > bm_lo) {
                for (size_t i = bm_lo; i < bm_hi && i < sizeof b; i++) {
                    unsigned v = 0u;
                    (void)sscanf(line + 2u + 2u * i, "%2X", &v);
                    ((uint8_t *)&b)[i] = (uint8_t)v;
                }
            }
            bd = g6c_hexcmp(line + 2, &b, sizeof b, &first);
            if (bd != 0u) {
                printf("   G6c golden %s: gsb_t differs in %u bytes (first +0x%zX)\n", name, bd, first);
            }
        } else if (line[0] == 'W') {
            size_t first;
            wd = g6c_hexcmp(line + 2, &r->w, sizeof r->w, &first);
            if (wd != 0u) {
                printf("   G6c golden %s: gsb_win_t differs in %u bytes (first +0x%zX)\n", name, wd, first);
            }
        } else if (line[0] == 'R') {
            unsigned v[6];
            if (sscanf(line, "R %u %u %u %u %u %u", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
                if (v[0] != r->rel_t0[0] || v[1] != r->rel_t0[1] || v[2] != r->rel_trip_n ||
                    v[3] != r->rel_trip_code || v[4] != r->relev || v[5] != r->relref) {
                    rd++;
                    printf("   G6c golden %s: release header differs (golden %u %u %u %u %u %u)\n", name, v[0], v[1],
                           v[2], v[3], v[4], v[5]);
                }
            }
        } else if (line[0] == 'E') {
            unsigned n = 0u;
            if (sscanf(line, "END %u", &n) == 1 && n != tk_n) {
                td++;
                printf("   G6c golden %s: tick count golden %u now %u\n", name, n, tk_n);
            }
        }
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/%s.win", dir, tag);
    f = fopen(path, "rb");
    if (f != NULL) {
        static uint8_t gw[WIN_N];
        uint32_t lo = 0u, hi = 0u;
        if (fread(gw, 1u, WIN_N, f) == WIN_N) {
            for (uint32_t i = 0u; i < WIN_N; i++) {
                const uint32_t abs = 0xFFF80000u + i;
                if (abs >= G6C_SH_REQ_LO && abs < G6C_SH_REQ_HI) {
                    continue;               /* the deleted shadow's request objects */
                }
#if defined(G6R_NOREP)
                if (g6r_holder_cell(abs)) {
                    if (gw[i] != win[i]) {
                        g6r_holder_bytes_differ++;
                    }
                    continue;               /* r04r: the retired holder's cells (no longer written) */
                }
#endif
                if (gw[i] != win[i]) {
                    if (ed == 0u) {
                        lo = abs;
                    }
                    hi = abs;
                    ed++;
                }
            }
        } else {
            ed = 1u;
        }
        fclose(f);
        if (ed != 0u) {
            printf("   G6c golden %s: end-of-run window bytes differ %u (0x%08X..0x%08X)\n", name, ed,
                   (unsigned)lo, (unsigned)hi);
        }
    } else {
        ed = 1u;
        printf("   G6c golden %s: window file missing\n", name);
    }
#if defined(G6R_NOREP)
    printf("   r04r no-replica [%s]: the retired holder's cells differing from the golden at the end: %lu B "
           "(not compared); per-frame cell hash not compared, wire hash compared\n", name, g6r_holder_bytes_differ);
    g6r_holder_bytes_differ = 0u;
#endif
    printf("   vs the golden [%s]: frames differ %u (first F%u), ticks differ %u of %u, gsb_t %u B, windows %u B, "
           "release %u, end window %u B => %s\n", name, fd, f_first == 0xFFFFFFFFu ? 0u : f_first, td, gtk, bd, wd,
           rd, ed, (fd | td | bd | wd | rd | ed) == 0u ? "IDENTICAL" : "DIFFERENT");
    return fd + td + bd + wd + rd + ed;
}

#endif /* G6C_GOLD_H */

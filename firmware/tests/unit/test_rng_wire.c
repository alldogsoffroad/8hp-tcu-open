/* "The only output effect of the live range chain is the park demands"
 * (2026-11-17, ~/tcu_sweep/range_chain_live_port.md §6).
 *
 * The firmware's 10 ms output path is run over the r03s scenarios — the
 * request producer (app/rq_prod.c + fill, compile-time inputs) → the request
 * law and the bus-A cyclic / config words (bsw/drv), with channel 7 (the park
 * release) taken from park_slot7_release() as the target's law takes it —
 * after vehicle → the range chain → park_tick. Per tick the program hashes
 * every output word and law cell EXCEPT the park-release / park-hold
 * positions (the masks below), and prints "t release hold hash" followed by
 * the chain's own cells (D6D6 D6D7 D6D8 D6D9 D6DE D6DF E545 893E8 D5FB
 * 304C+0 894B6 — at r03s read from the shadow's block, now at the stock
 * addresses).
 *
 * The golden file tests/unit/golden/rng_wire_r03s.txt was produced by THIS
 * program built against HEAD 91cc956 (r03s: range.c cyclic, the chain a
 * read-only shadow; -DRNG_WIRE_HEAD) — so the comparison is the claim: every
 * masked hash equal to r03s, and the live chain's cells equal to the r03s
 * shadow's, tick for tick; the release / hold columns are printed and NOT
 * compared (they are the intended change).
 *
 *   test_rng_wire                 print the trace
 *   test_rng_wire GOLDEN          compare the hash column with GOLDEN
 */
#define _POSIX_C_SOURCE 200809L
#include "app/park.h"
#include "app/park_cells.h"
#include "app/rq_prod.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/sup/sup_cells.h"
#include "io/vehicle/vehicle.h"
#include "io/sensors/sensors.h"
#include "tests/unit/stub_plat.h"
#if defined(RNG_WIRE_HEAD)
#include "app/rng_shadow.h"     /* r03s: the chain as a read-only shadow */
#else
#include "app/rng_chain.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* r03s's park.c read the output speed; the live path no longer does */
static sensors_in_t s_z;
const sensors_in_t *sensors_in(void) { return &s_z; }

static uint8_t s_ctr[0x800];

static void send(can_bus_t bus, uint16_t id, uint8_t dlc, bool prot, const uint8_t *data)
{
    can_frame_t f = { .id = id, .dlc = dlc };

    memcpy(f.data, data, dlc);
    if (prot) {
        s_ctr[id] = (uint8_t)((s_ctr[id] + 1u) & 0x0Fu);
        f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(s_ctr[id] << 4));
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
    }
    vehicle_rx(bus, &f);
}

typedef struct {
    const char *name;
    uint8_t  sched[8];      /* lever code per segment: 1 P 2 R 3 N 4 D, 0 none */
    uint8_t  nseg;
    bool     brake;
    uint16_t engine;
    uint32_t lever_from, gap_from, gap_len;
} scen_t;

#define SEG 100u
static const scen_t k_scen[] = {
    { "rest: no sims",          { 1, 2, 3, 4, 1, 0 },    6u, false, 0u,   0u,   0u,   0u },
    { "brake + engine",         { 1, 2, 3, 4, 1, 2 },    6u, true,  700u, 0u,   0u,   0u },
    { "engine, no brake",       { 1, 2, 3, 4, 1 },       5u, false, 700u, 0u,   0u,   0u },
    { "late lever 1.1 s",       { 1, 1, 2, 3, 4, 1 },    6u, true,  700u, 110u, 0u,   0u },
    { "2 s gap with R held",    { 1, 2, 2, 2, 2, 2, 1 }, 7u, true,  700u, 0u,   140u, 200u },
};

/* the positions that carry the park release (ch 7, chip 3) — masked:
 * cyclic word 3 (chip 3's mode word: 8812 / 881A), cyclic word 23 (the F0
 * row, chip 3's second channel: F000 / F682..), config word 8 (chip 3's
 * 0x90 byte: the high-current rule 0x90232 for one pass) and config word
 * 18 (chip 3's mode row); the law cells of ch 7 */
static bool masked_cyc(unsigned i) { return i == 3u || i == 23u; }
static bool masked_cfg(unsigned i) { return i == 8u || i == 18u; }

static uint64_t fnv(uint64_t h, uint32_t v)
{
    for (int k = 0; k < 4; k++) {
        h = (h ^ (uint8_t)(v >> (8 * k))) * 0x100000001B3ull;
    }
    return h;
}

static void run(const scen_t *sc, FILE *out, unsigned base)
{
    memset(s_ctr, 0, sizeof s_ctr);
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    memset(&s_z, 0, sizeof s_z);
    s_z.q_output_speed = Q_OK;
    CELL_TMP_BYTE = 67u;
    vehicle_init();
    park_init();
#if defined(RNG_WIRE_HEAD)
    rng_shadow_init();
#else
    rng_chain_init();
#endif
    drv_jobs_init();                    /* → drv_rq_init → rq_prod_init */
    /* the temperature byte is LIVE since 2026-11-17 (app/tmp_chain.c, not
     * linked here): the dump's 0x43 — r03s/r03t's compile-time value, so the
     * golden trace stands unchanged */
    *(volatile uint8_t *)0xFFF89C13u = 0x43u;
    *(volatile uint8_t *)0xFFF89CD2u = 0x43u;
    drv_rq_prod_host_select(true);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, 0u);
    }
    CELL_SEQ2_STATE[0] = (uint8_t)'U';  /* the engaged rail, as the bench */
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9020u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 12940u;
    rq_prod_boot_pass();
    for (uint32_t t = 0u; t < sc->nseg * SEG; t++) {
        const uint8_t code = sc->sched[t / SEG];
        uint8_t d[8];
        uint64_t h = 0xCBF29CE484222325ull;

        if (code != 0u && t >= sc->lever_from &&
            !(t >= sc->gap_from && t < sc->gap_from + sc->gap_len)) {
            const uint8_t l[5] = { code, 0, 0, 0, 0 };
            send(CAN_BUS1, 0x2EAu, 5u, true, l);
            send(CAN_BUS2, 0x210u, 5u, true, l);
        }
        if (sc->brake) {
            memset(d, 0, sizeof d);
            d[0] = 0x04u;
            send(CAN_BUS1, 0x140u, 8u, true, d);
        }
        if (sc->engine != 0u) {
            memset(d, 0, sizeof d);
            d[0] = (uint8_t)(sc->engine >> 8);
            d[1] = (uint8_t)sc->engine;
            d[2] = 0x1Fu;
            d[3] = 0xFFu;
            send(CAN_BUS1, 0x108u, 8u, true, d);
        }
        /* main.c's task_10ms order */
        rq_prod_task_10ms(1u);
        vehicle_update_10ms();
#if defined(RNG_WIRE_HEAD)
        rng_shadow_tick_10ms(1u, 1u, 1u);
#else
        rng_chain_tick_10ms(1u, 1u, 1u);
#endif
        park_tick_10ms();
        /* the law takes ch 7 from the park slot getter on the target */
        drv_rq_request_set(7u, park_slot7_release());
        drv_cyclic_build(0, 8u);
        {
            const uint16_t *cyc = drv_job_tx(DRV_CYC);
            const uint16_t *cfg = drv_job_tx(DRV_CFG);
            for (unsigned i = 0u; i < 45u; i++) {
                h = fnv(h, masked_cyc(i) ? 0u : cyc[i]);
            }
            for (unsigned i = 0u; i < 40u; i++) {
                h = fnv(h, masked_cfg(i) ? 0u : cfg[i]);
            }
        }
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            if (ch == 7u) {
                continue;
            }
            h = fnv(h, CELL_RQ_I[ch]);
            h = fnv(h, CELL_RQ_MODE[ch]);
            h = fnv(h, CELL_RQ_LETTER[ch]);
        }
        for (unsigned k = 0u; k < 7u; k++) {    /* the producer's currents 0xFFF84B78 */
            h = fnv(h, *(volatile uint16_t *)(uintptr_t)(0xFFF84B78u + 2u * k));
        }
        fprintf(out, "%u %u %u %016llx", base + t, park_slot7_release(), park_slot11_hold(),
                (unsigned long long)h);
#if defined(RNG_WIRE_HEAD)
        {
            const rng_shadow_cells_t *c = rng_shadow_cells();
            fprintf(out, " %u %u %u %u %u %u %u %u %u %u %u\n", c->d6d4[2], c->d6d4[3], c->d6d4[4],
                    c->d6d4[5], c->d6de[0], c->d6de[1], c->e530[0x15], c->x893e0[8],
                    c->d5e6[0xD5FBu - 0xD5E6u], (unsigned)c->o304c[0], c->x894b6);
        }
#else
#define C8(a) (*(volatile uint8_t *)(uintptr_t)(0xFFF80000u | (uint32_t)(a)))
        fprintf(out, " %u %u %u %u %u %u %u %u %u %u %u\n", C8(0xD6D6u), C8(0xD6D7u), C8(0xD6D8u),
                C8(0xD6D9u), C8(0xD6DEu), C8(0xD6DFu), C8(0xE545u), C8(0x893E8u), C8(0xD5FBu),
                (unsigned)*(volatile uint32_t *)(uintptr_t)0xFFF8304Cu, C8(0x894B6u));
#endif
    }
}

int main(int argc, char **argv)
{
    char *buf = NULL;
    size_t len = 0u;
    FILE *mem = open_memstream(&buf, &len);
    unsigned base = 0u, n_scen = (unsigned)(sizeof k_scen / sizeof k_scen[0]);

    for (unsigned k = 0u; k < n_scen; k++) {
        fprintf(mem, "# %s\n", k_scen[k].name);
        run(&k_scen[k], mem, base);
        base += k_scen[k].nseg * SEG;
    }
    fclose(mem);
    if (argc < 2) {
        fputs(buf, stdout);
        free(buf);
        return 0;
    }
    {
        FILE *g = fopen(argv[1], "r");
        char gl[256], *save = NULL;
        unsigned lines = 0u, bad = 0u, rel_diff = 0u;
        char *ln = strtok_r(buf, "\n", &save);

        if (g == NULL) {
            printf("FAIL cannot open %s\n", argv[1]);
            return 1;
        }
        while (ln != NULL) {
            unsigned t1, r1, h1, t2, r2, h2;
            int o1 = 0, o2 = 0;
            char x1[32], x2[32];

            if (fgets(gl, sizeof gl, g) == NULL) {
                printf("FAIL golden shorter than the trace at: %s\n", ln);
                return 1;
            }
            if (ln[0] == '#') {
                if (strncmp(ln, gl, strlen(ln)) != 0) {
                    printf("FAIL scenario header differs: %s / %s", ln, gl);
                    bad++;
                }
            } else if (sscanf(ln, "%u %u %u %31s%n", &t1, &r1, &h1, x1, &o1) != 4 ||
                       sscanf(gl, "%u %u %u %31s%n", &t2, &r2, &h2, x2, &o2) != 4 || t1 != t2) {
                printf("FAIL unparsable / misaligned line: %s / %s", ln, gl);
                return 1;
            } else {
                lines++;
                rel_diff += (r1 != r2 || h1 != h2);
                gl[strcspn(gl, "\n")] = '\0';
                if (strcmp(x1, x2) != 0 || strcmp(ln + o1, gl + o2) != 0) {
                    if (bad < 10u) {
                        printf("FAIL tick %u: masked output hash / chain cells %s%s, r03s %s%s\n",
                               t1, x1, ln + o1, x2, gl + o2);
                    }
                    bad++;
                }
            }
            ln = strtok_r(NULL, "\n", &save);
        }
        fclose(g);
        free(buf);
        printf("rng_wire: %u ticks, %u differ from r03s (masked output hash or chain cells); the "
               "park demands differ on %u ticks (the intended change)\n", lines, bad, rel_diff);
        return bad == 0u ? 0 : 1;
    }
}

/* transcript.c — THE BYTE-IDENTITY AND TIMING VERIFICATION.
 *
 * Runs our firmware's first 100 frames (5 ms each) on a 10 µs time step with
 * a bench-shaped supervisor model and prints every bus-B word per frame;
 * then judges:
 *   1. the cyclic per-pass order (stock pool 0x8EE82 + engine 0x937F8):
 *      J3, J4, J2, F, J6, J1, J5, F2 every frame (S4: J3 first, 0x91C12) — no J7 (no stock queuer);
 *   2. THE WALK TIMING LAW (r02f; stock 0x6C0BA / 0x6BF9C / 0x6C594 for the
 *      06 era, period 0x4B0): relative to the era base the programmer aimed
 *      from (the last exchange stamp TCNTA - 0x29, or that plus whole 3600
 *      µs cycles once the stepper re-bases), every W1 token launches at
 *      fire - 1 = +2345 or +3206 µs (or the next cycle's +5945); no W2 84
 *      write launches inside the keep-out (3206, 3774) µs;
 *   3. the conversation shape (0x8D020): tokens AND resends both occur —
 *      tokens drag the level down while it is above the target 5, resends
 *      raise it at or below 5 — and no free-running walk stream exists.
 *   4. THE OPENING (S12): walk job #1 is 84|06; walk job #2 is the
 *      SELECTOR-0 TOKEN 88FF 880F 88F0 8800 1200 0C00 0A00 in a token slot
 *      (+2345, or +3206 when the programming pass falls past OPEN — main.c's
 *      phases put it at +3206)
 *      (the fresh-chip F0 reply: reported = expected = 7 > target 5 →
 *      0x8D020 → 0x6C802(token(sel 0), 0x1E)). r02f's "resend first" was
 *      an artifact of the old level-6 model.
 *
 * Schedule = main.c: monitor at frame offsets 0 and 3 ms (the stock's 2.5 ms
 * task as a 2/3 ms alternation), batch at 2 ms; the walk ISR fires when the
 * stub's one-shot elapses (the stub runs 28 µs/tick against the platform's
 * 32 µs/tick arm, so shots land early and the service's closed loop on
 * TCNTA is exercised). RSPI transfers complete instantly at the step.
 *
 * Chip model [I, for the transcript only] — the stock's own expected-level
 * bookkeeping read as a chip model (0x6C0BA bumps expected +1 per 84 write,
 * 0x6BF9C ctx 0x1E lowers it -1 per in-window token). S12 (walkdata review
 * 2026-11-11): the opening is the bench's OWN fresh-chip reply — r02e
 * witness m+1.0: fb F0 / echo 06 / reported = expected = 7 — not the
 * unsourced "first reply = level 6": the first 84 leaves the level at 7; a
 * repeat 84 raises it by one (cap 7); a token lowers it by one (floor 0)
 * and rotates the selector; fb = 0x80 | level<<4 | selector. Under the
 * stock law that opening makes walk job #2 a SELECTOR-0 TOKEN at OPEN.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "platform.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_walk.h"
#include "stub_plat.h"

extern struct { uint8_t bus; uint8_t job; uint16_t word; uint32_t t_us; } stub_log[32768];
extern unsigned stub_log_n;
extern unsigned stub_log_dropped;

static uint8_t s_level = 7u;         /* the chip's level (model) */
static uint8_t s_sel;                /* the chip's selector (model) */
static uint32_t s_base_at[32768];    /* CELL_ERA_BASE when each word was sent:
                                      * the base the walk programmer aimed from
                                      * (frozen while a shot is in flight) */
static uint8_t s_echo;               /* the last 84 value (model) */

static uint16_t bench_reply(uint16_t tx)
{
    switch (tx & 0xFF00u) {
    case 0x8400u:                                  /* W2 frame 0: 84|v */
        if ((uint8_t)tx == s_echo && s_level < 7u) {
            s_level++;                             /* a repeat 84 */
        }
        s_echo = (uint8_t)tx;                      /* a new value: level holds */
        return 0xAA00u;
    case 0x8800u:                                  /* W1 token frames */
        return 0xAA00u;
    case 0x0A00u: return (uint16_t)(0xAA00u | s_echo);
    case 0x0C00u: return (uint16_t)(0xAA80u | (uint16_t)(s_level << 4) | s_sel);
    case 0x1200u: return 0xAA00u;                  /* commit 0 */
    case 0x1400u: return 0xAAD5u;                  /* the gate-closed 0x14 */
    case 0x2200u: return (uint16_t)(0xAA00u | (CELL_REQUEST & 0x01u));
    case 0x0600u: return 0xAA14u;
    default:      return 0xAA00u;
    }
}

static void drain(void)
{
    for (uint8_t b = 0u; b < 2u; b++) {
        unsigned guard = 512u;
        while (stub_rspi_busy(b) && guard-- != 0u) {
            unsigned before = stub_log_n;
            stub_rspi_isr(b);
            for (unsigned k = before; k < stub_log_n && k < 32768u; k++) {
                s_base_at[k] = CELL_ERA_BASE;
            }
            /* a token's first frame lowers the model level once */
            if (stub_log_n != before && stub_log[before].bus == 1u &&
                (stub_log[before].word & 0xFF00u) == 0x8800u &&
                stub_log[before].job == JOB_DUTY &&
                (before == 0u || stub_log[before - 1u].job != JOB_DUTY ||
                 (stub_log[before - 1u].word & 0xFF00u) != 0x8800u)) {
                if (s_level != 0u) {
                    s_level--;
                }
                s_sel = (uint8_t)((s_sel + 1u) & 0x0Fu);
            }
        }
    }
}

extern uint32_t plat_walk_true_q8;   /* stub: the counter's true rate */
extern int plat_walk_cal_err_permil;
extern uint32_t plat_walk_isr_latency_us;

int main(int argc, char **argv)
{
    static const char *JN[] = {"CTRL","EN","POS","SSLB2","ID","STAT","NIB",
                               "DUTY","LEVEL","FLT"};
    enum { FRAMES = 400 };              /* 2 s; P01-P100 printed */
    unsigned pass_mark[FRAMES + 2];
    uint32_t lcg = 0x2026111Bu;         /* seeded scheduler jitter */
    int fail = 0;
    unsigned walk_fires = 0u;

    /* B2: the walk counter's true rate, q8 µs/tick (default 256 = 1 µs;
     * ctest also runs 7168 = 28 µs, the old bench-launch reading) */
    if (argc > 1) {
        plat_walk_true_q8 = (uint32_t)strtoul(argv[1], NULL, 0);
    }
    /* r02g review: the platform's calibration is imperfect and the walk
     * ISR is entered late — model +1.5 % calibration error (the arm uses a
     * rate 1.5 % SLOW, so shots land later than aimed) and 15 µs entry
     * latency, the worst-case the arm margin (20 µs + 1/32) must absorb */
    plat_walk_cal_err_permil = 15;
    plat_walk_isr_latency_us = 15u;
    printf("WALK CLOCK: %u/256 us per tick\n",
           (unsigned)(plat_walk_true_q8 ? plat_walk_true_q8 : 256u));

    sup_jobs_init();
    sup_walk_init();
    sup_batch_init();
    stub.sup_reply_fn = bench_reply;
    sup_job_engine_pump();                      /* main.c:289 — the init J5 */
    drain();
    unsigned init_end = stub_log_n;

    for (unsigned p = 1u; p <= FRAMES; p++) {
        /* r02f review: jitter the task phases (0..900 µs, 10 µs grain) so
         * the walk programmers run at many offsets from the exchange stamp
         * — a fixed schedule never programs a resend near the keep-out */
        unsigned j0, j1, jb;
        lcg = lcg * 1664525u + 1013904223u; j0 = ((lcg >> 16) % 91u) * 10u;
        lcg = lcg * 1664525u + 1013904223u; j1 = ((lcg >> 16) % 91u) * 10u;
        lcg = lcg * 1664525u + 1013904223u; jb = ((lcg >> 16) % 91u) * 10u;
        if (p <= 100u) { j0 = j1 = jb = 0u; }   /* P01-P100: main.c's exact phases */
        pass_mark[p] = stub_log_n;
        for (unsigned step = 0u; step < 500u; step++) {   /* 5 ms in 10 µs */
            unsigned us = step * 10u;
            if (us == j0 || us == 3000u + j1) {
                sup_monitor_poll();
                drain();
            }
            if (us == 2000u + jb) {
                sup_batch_5ms();
                drain();
            }
            if (plat_walktmr_running() &&
                (int32_t)(stub.frt_us - stub.walk_fire_us) >= 0) {
                plat_walktmr_stop();            /* the one-shot underflow */
                walk_fires++;
                sup_walk_isr();                 /* may re-arm (closed loop) */
                drain();
            }
            stub.frt_us += 10u;
        }
    }

    /* ── print ─────────────────────────────────────────────────────────── */
    printf("P00");
    for (unsigned j = 0u; j < init_end; j++) {
        if (stub_log[j].bus == 1u) printf(" %s:%04X", JN[stub_log[j].job % 10u], stub_log[j].word);
    }
    pass_mark[FRAMES + 1u] = stub_log_n;
    for (unsigned p = 1u; p <= 100u; p++) {
        unsigned end = pass_mark[p + 1u];
        printf("\nP%02u", p);
        for (unsigned j = pass_mark[p]; j < end; j++) {
            if (stub_log[j].bus != 1u) continue;
            printf(" %s:%04X", JN[stub_log[j].job % 10u], stub_log[j].word);
        }
    }
    printf("\n");

    /* ── verdict 0: the recorder ───────────────────────────────────────── */
    if (stub_log_dropped != 0u) {
        printf("FAIL: recorder dropped %u words\n", stub_log_dropped);
        fail = 1;
    }
    for (unsigned j = 0u; j < init_end; j++) {
        if (stub_log[j].bus == 1u && stub_log[j].job != JOB_ID) {
            printf("FAIL: P00 carries a non-J5 word\n");
            fail = 1;
        }
    }

    /* ── verdict 1: the cyclic order (walk jobs excluded — they ride the
     *    priority ring from the asynchronous one-shot) ─────────────────── */
    {
        /* S4: J3 (POS) before J4 (SSLB2) — stock 0x91C12/0x91C16 */
        static const uint8_t EXP[] = { JOB_POS, JOB_SSLB2, JOB_EN, JOB_FLT,
                                       JOB_STAT, JOB_CTRL, JOB_ID, JOB_FLT };
        static const uint8_t NFR[JOB_COUNT] = { [JOB_CTRL]=4u, [JOB_EN]=2u,
            [JOB_POS]=2u, [JOB_SSLB2]=1u, [JOB_ID]=4u, [JOB_STAT]=5u,
            [JOB_NIB]=1u, [JOB_DUTY]=7u, [JOB_LEVEL]=4u, [JOB_FLT]=2u };
        for (unsigned p = 1u; p <= FRAMES && !fail; p++) {
            unsigned end = pass_mark[p + 1u];
            unsigned k = 0u, j = pass_mark[p];
            while (j < end) {
                uint8_t id = stub_log[j].job;
                if (stub_log[j].bus != 1u || id == JOB_LEVEL || id == JOB_DUTY) {
                    j++;
                    continue;
                }
                if (id >= JOB_COUNT || k >= sizeof EXP || id != EXP[k]) {
                    printf("FAIL: P%02u job slot %u: got %s, expected %s\n", p, k,
                           JN[id % 10u], k < sizeof EXP ? JN[EXP[k]] : "(end)");
                    fail = 1;
                    break;
                }
                unsigned got = 0u;
                while (j < end && got < NFR[id]) {
                    if (stub_log[j].bus == 1u && stub_log[j].job == id) got++;
                    j++;
                }
                k++;
            }
            if (!fail && k != sizeof EXP) {
                printf("FAIL: P%02u carried %u cyclic jobs, expected %u\n", p, k,
                       (unsigned)sizeof EXP);
                fail = 1;
            }
        }
    }

    /* ── verdicts 2 and 3: the walk timing law and the conversation shape ─
     * Walk job launches = the first frame of each W1/W2 burst. Each one's
     * offset is taken from the PREVIOUS walk job's completion stamp: the
     * launch time of its last frame (instant transfers) minus 0x29. */
    {
        enum { SLACK = 12 };             /* the 10 µs step + rounding */
        bool have_stamp = false;
        unsigned run_w2 = 0u, max_run_w2 = 0u;
        unsigned n_tok = 0u, n_lvl = 0u, n_tok_ok = 0u;
        unsigned printed = 0u;

        /* verdict 4: the opening */
        {
            static const uint16_t TOK0[7] = { 0x88FFu, 0x880Fu, 0x88F0u, 0x8800u,
                                              0x1200u, 0x0C00u, 0x0A00u };
            unsigned w = 0u, j1 = 0u, j2 = 0u;
            /* walk-job boundaries by FRAME COUNT (W1 = 7, W2 = 4), not by id
             * change — two adjacent W2s would merge (r02g review N3a) */
            for (unsigned j = 0u; j < stub_log_n && w < 2u; ) {
                uint8_t id = stub_log[j].job;
                if (id == JOB_DUTY || id == JOB_LEVEL) {
                    if (w == 0u) j1 = j; else j2 = j;
                    w++;
                    j += (id == JOB_DUTY) ? 7u : 4u;
                } else {
                    j++;
                }
            }
            bool ok = (w == 2u && stub_log[j1].word == 0x8406u &&
                       stub_log[j2].job == JOB_DUTY);
            for (unsigned k = 0u; ok && k < 7u; k++) {
                ok = (stub_log[j2 + k].word == TOK0[k]);
            }
            int32_t off2 = w == 2u ? (int32_t)(stub_log[j2].t_us - s_base_at[j2]) : -1;
            /* the slot depends on when the monitor pass programs it (0x6BF9C:
             * OPEN if elapsed < OPEN, else CLOSE) — both are stock slots */
            bool slot = (off2 >= 2345 && off2 <= 2345 + SLACK) ||
                        (off2 >= 3206 && off2 <= 3206 + SLACK) ||
                        (off2 >= 5945 && off2 <= 5945 + SLACK);   /* N3b */
            if (!ok || !slot) {
                printf("FAIL: opening — walk job #2 must be the sel-0 token in a slot "
                       "(got %s %04X at %d)\n", w == 2u ? JN[stub_log[j2].job % 10u] : "-",
                       w == 2u ? stub_log[j2].word : 0u, (int)off2);
                fail = 1;
            } else {
                printf("WALK: opening 84|06 then sel-0 token at +%d\n", (int)off2);
            }
        }

        printf("WALK: job  offset-from-stamp (µs)\n");
        for (unsigned j = init_end; j < stub_log_n; j++) {
            uint8_t id = stub_log[j].job;
            bool first = (id == JOB_DUTY || id == JOB_LEVEL) &&
                         (j == 0u || stub_log[j - 1u].job != id);
            if (!first) continue;
            unsigned nf = (id == JOB_DUTY) ? 7u : 4u;
            int32_t off = (int32_t)(stub_log[j].t_us - s_base_at[j]);

            if (printed < 24u) {
                printf("WALK: %s %d\n", id == JOB_DUTY ? "W1 " : "W2 ", (int)off);
                printed++;
            }
            if (id == JOB_DUTY) {
                n_tok++;
                if (have_stamp) {
                    /* the stock fires at base + fire - 1 (0x6C0BA/0x6BF9C:
                     * delay = fire - 1 - elapsed), so its realized slots are
                     * 2345 / 3206 / 5945 */
                    bool ok = (off >= 2345 && off <= 2345 + SLACK) ||
                              (off >= 3206 && off <= 3206 + SLACK) ||
                              (off >= 5945 && off <= 5945 + SLACK);
                    if (ok) {
                        n_tok_ok++;
                    } else {
                        printf("FAIL: W1 token at +%d µs (slots 2345/3206/5945)\n", (int)off);
                        fail = 1;
                    }
                }
            } else {
                n_lvl++;
                if (have_stamp && off > 3206 && off < 3774) {   /* (0x93840, 0x93842) - 1 */
                    printf("FAIL: W2 84 write at +%d µs inside the keep-out (3206, 3774)\n",
                           (int)off);
                    fail = 1;
                }
            }
            if (id == JOB_LEVEL) {
                if (++run_w2 > max_run_w2) max_run_w2 = run_w2;
            } else {
                run_w2 = 0u;
            }
            have_stamp = true;
        }
        printf("WALK: %u W1 tokens (%u in slot), %u W2 writes over %u frames (jittered after P100)\n",
               n_tok, n_tok_ok, n_lvl, (unsigned)FRAMES);
        if (n_lvl == 0u) {
            printf("FAIL: no W2 resend — the 0x8D020 level <= target path never ran\n");
            fail = 1;
        }
        if (n_tok == 0u) {
            printf("FAIL: no W1 token in 500 ms — the state-8 law never answered the chip\n");
            fail = 1;
        }
        if (max_run_w2 > 4u) {
            printf("FAIL: %u W2 writes in a row — a resend loop, not the 5<->6 "
                   "oscillation of the 0x8D020 law\n", max_run_w2);
            fail = 1;
        }
        /* B2: a timed shot costs at most ~3 walk-ISR entries (arm short,
         * one remainder re-arm, the final spin) — more is the IPR-6 storm
         * that starves RSPI-B */
        printf("WALK: %u ISR entries for %u walk jobs\n", walk_fires, n_tok + n_lvl);
        if (walk_fires > 3u * (n_tok + n_lvl) + 8u) {
            printf("FAIL: walk ISR storm — %u entries for %u jobs\n", walk_fires, n_tok + n_lvl);
            fail = 1;
        }
    }

    printf(fail ? "TRANSCRIPT: DRIFT\n" : "TRANSCRIPT: stock order + walk timing law hold, 400 frames\n");
    return fail;
}

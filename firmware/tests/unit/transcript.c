/* transcript.c — THE BYTE-IDENTITY VERIFICATION (the never-run diff):
 * runs our firmware's first 100 scheduler passes with a bench-shaped reply
 * model and prints every bus-B word per pass. Diff against the stock's
 * documented composition (first500ms_diff.md §1). */
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_walk.h"
#include "stub_plat.h"

#define STUB extern
/* the recorder lives in stub_plat.c */
extern struct { uint8_t bus; uint8_t job; uint16_t word; } stub_log[2048];
extern unsigned stub_log_n;
extern unsigned stub_log_dropped;

/* bench-shaped reply model (r01v/r01w live observations, gate closed):
 *  - walk 84|06/88 words: AA-class echoes; the 0C reply carries fb->level 6
 *  - CTRL 22xx: echo with the request bit (the chip's grant law)
 *  - STAT 1400: the 0x14 word = D5 (our bench chip, gate closed)
 *  - J5 ID: AA14 (the observed ID reply class)                             */
static uint16_t bench_reply(uint16_t tx)
{
    switch (tx & 0xFF00u) {
    case 0x0C00u: return 0xAA60u;              /* fb nibble 6 -> level 6 */
    case 0x1400u: return 0xAAD5u;              /* the gate-closed 0x14   */
    case 0x2200u: return (uint16_t)(0xAA00u | (CELL_REQUEST & 0x01u));
    case 0x0600u: return 0xAA14u;              /* J5 ID                  */
    default:      return 0xAA00u;
    }
}

static void drain(void)
{
    for (uint8_t b = 0u; b < 2u; b++) {
        unsigned guard = 512u;
        while (stub_rspi_busy(b) && guard-- != 0u) {
            stub_rspi_isr(b);
        }
    }
}

int main(void)
{
    unsigned pass_mark[128];
    sup_jobs_init();
    sup_walk_init();
    sup_batch_init();
    stub.sup_reply_fn = bench_reply;
    sup_job_engine_pump();                      /* main.c:289 — the init J5
                                                 * launches before the tick */
    drain();
    unsigned init_end = stub_log_n;

    for (unsigned p = 1u; p <= 100u; p++) {
        pass_mark[p] = stub_log_n;
        /* one 5 ms frame in main.c's order: monitor (phase 0), batch
         * (phase 2), monitor (phase 3) — the monitor is the stock's 2.5 ms
         * task (0x7EE2E -> 0x873F0), no lead-in */
        stub.frt_us += 2500u;
        sup_monitor_poll();
        sup_batch_5ms();
        drain();
        stub.frt_us += 2500u;
        sup_monitor_poll();
        if (plat_walktmr_running()) {            /* ONE walk tick per frame:
                                                  * the down-counter load
                                                  * 0xB000 is ~4.5-4.95 ms,
                                                  * and it only fires once
                                                  * the monitor arms it */
            sup_walk_isr();
        }
        drain();
    }

    /* print: pass | job words */
    static const char *JN[] = {"CTRL","EN","POS","SSLB2","ID","STAT","NIB",
                               "DUTY","LEVEL","FLT"};
    printf("P00");
    for (unsigned j = 0u; j < init_end; j++) {
        if (stub_log[j].bus == 1u) printf(" %s:%04X", JN[stub_log[j].job % 10u], stub_log[j].word);
    }
    for (unsigned p = 1u; p <= 100u; p++) {
        printf("\nP%02u", p);
        unsigned end = (p < 100u) ? pass_mark[p + 1u] : stub_log_n;
        unsigned j = pass_mark[p];
        int line = 0;
        for (; j < end; j++) {
            if (stub_log[j].bus != 1u) continue;           /* bus B only */
            if (line++ == 0) printf(" ");
            printf("%s:%04X ", JN[stub_log[j].job % 10u], stub_log[j].word);
        }
    }
    printf("\n");

    /* ── THE VERDICT: the transcript fails the build on any drift ──────────
     * The stock's per-pass bus-B order in mode 0 (pool 0x8EE82 + the queue
     * engine 0x937F8/0x93902), cyclic jobs only:
     *   J4, J3, J2, F, J6, J1, J5, F2
     * F2 = the J6-completion re-fire (0x92BBE -> 0x92F7C on 0x14 & 0xE0;
     * the model's 0x14 = D5). F's descriptor 0x40C30 has priority byte 0 ->
     * the normal ring, TAIL insert (0x937F8); J1/J5 are queued in the same
     * CPU burst, long before J6's 5 frames complete behind a 9-frame
     * backlog -> F2 lands after J5 [O ring/priority; I timing, strong].
     * J7 never appears (no stock queuer). The walk-queue jobs (LEVEL, DUTY)
     * are excluded: they ride the priority ring (stock descriptors 0x40A14 /
     * 0x409F4 are the ONLY priority-1 bus-B descriptors; ours JOBQ_WALK)
     * from the asynchronous timer tick, so their position is the tick's,
     * not the pool's. */
    static const uint8_t EXP[] = { JOB_SSLB2, JOB_POS, JOB_EN, JOB_FLT,
                                   JOB_STAT, JOB_CTRL, JOB_ID, JOB_FLT };
    static const uint8_t NFR[JOB_COUNT] = { [JOB_CTRL]=4u, [JOB_EN]=2u,
        [JOB_POS]=2u, [JOB_SSLB2]=1u, [JOB_ID]=4u, [JOB_STAT]=5u,
        [JOB_NIB]=1u, [JOB_DUTY]=7u, [JOB_LEVEL]=4u, [JOB_FLT]=2u };
    int fail = 0;
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
    for (unsigned p = 1u; p <= 100u && !fail; p++) {
        unsigned end = (p < 100u) ? pass_mark[p + 1u] : stub_log_n;
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
            /* consume this job's frames (walk frames may interleave) */
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
    printf(fail ? "TRANSCRIPT: DRIFT\n" : "TRANSCRIPT: stock order holds, P01-P100\n");
    return fail;
}

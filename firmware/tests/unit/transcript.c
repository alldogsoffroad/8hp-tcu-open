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
    drain();                                    /* the init J5 */

    for (unsigned p = 1u; p <= 100u; p++) {
        pass_mark[p] = stub_log_n;
        stub.frt_us += 5000u;
        sup_batch_5ms();
        sup_monitor_poll();
        if (plat_walktmr_running()) {            /* the tick only fires once
                                                  * the monitor's first pass
                                                  * arms the walk timer */
            sup_walk_isr();
        }
        drain();
    }

    /* print: pass | job words */
    static const char *JN[] = {"CTRL","EN","POS","SSLB2","ID","STAT","NIB",
                               "DUTY","LEVEL","FLT"};
    printf("P00 J5");
    unsigned i = 0u;
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
    return 0u;
}

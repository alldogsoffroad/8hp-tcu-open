/* sup_ctrl.h — the ctrl+C9 composers and the enable request cell
 * (design §7.6; stock 0x91C86 + 0x93126 + 0x6C612).
 *
 * The 0x8E control byte is composed bit-by-bit every 5 ms into the J1
 * staging frame 0; the SSLB0 C9 byte into J1 frame 1. Dialects are
 * mode-gated (NR-4): the engaged dialect (mode & 0x11 == 0x11, steady
 * words 0x8E91/0x8E81) is entered by the letters/mode chain, never by a
 * style flag; the normal dialect carries the echo-complement life sign
 * (steady words 0x8E04/0x8E00 + bit 0 once requested).
 */
#ifndef TCU_SUP_CTRL_H
#define TCU_SUP_CTRL_H

#include <stdint.h>
#include <stdbool.h>

/* compose the 0x8E ctrl byte (stock 0x91C86 law) into CELL_CTRL_STAGED and
 * the J1 staging frame 0. Also forces the request cell every pass while
 * outputs are not disabled (the builder law's tail). */
void sup_ctrl_compose(void);

/* compose the SSLB0 C9 byte (stock 0x93126 law) into CELL_C9_STAGED and the
 * J1 staging frame 1. */
void sup_c9_compose(void);

/* the request-cell setter (stock 0x6C612 park_store_request_state):
 * on  → request cell 1, outputs-disabled 0
 * off → outputs-disabled 1, request cell 0 (boot-handover disable shape) */
/* FE13 sub-17 bench arm: force the ENGAGED dialect's mode gate
 * (CELL_MODE_BYTE |= 0x11) so the composer emits the 0x91/0x81
 * engaged pair (bit7 held + bit4 toggling, the 0x91C86 law)
 * instead of the normal 0x8E04. The stock certifies the rail
 * INSIDE this dialect (the 0xAA00 ack-but-refused on r01m rode
 * the normal dialect). Reversible: off clears the gate bits. */
void sup_ctrl_bench_dialect(bool on);

void sup_enable_request(bool on);

/* the per-pass echo-derived condense (stock 0x90ACC/0x91ACA law): grant
 * cell = 0x22-echo bit 0, handover readiness = 0x14 bit 0, the WDT mode
 * A-law (interval mode ONLY when the echo confirms bit 0), the SSLB2 copy
 * + sentinel re-arm, and the 4-sample telemetry average. */
void sup_reply_condense(void);

/* the 0x93126 summarizer (sup_handlers.c): fold + per-round clear of the
 * OK/ERR/ECHOFAIL triples (stock 0x931F8) */
void sup_reply_summarize(void);

#endif /* TCU_SUP_CTRL_H */

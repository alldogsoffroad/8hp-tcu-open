/* sup_token.h — the duty-token challenge engine (design §7.9, spec 22 §5).
 *
 * The W1 duty job's payload is a rolling challenge-response token keyed by
 * the 0x0C feedback's low nibble (the challenge selector 0xFFF93BA6):
 *
 *     token = ~(xor_table[sel & 0xF] ^ 0xAAAA ^ 0xB32F8A6A)
 *
 * xor_table: 16 u32 entries extracted from the stock ELF at flash 0x4526C
 * (tools/extract_token_table.py); values verified against the bench-observed
 * rounds in sup_engage.c (the same table, bench-proven).
 *
 * Contexts (design §7.9, artifact2 §5): while the reported level is 0 the
 * duty arms cycle 0 → 7 → 0x19, and context 0x19 sends the COMPLEMENT of the
 * token; once level != 0 the steady context is 0x1E (plain token).
 */
#ifndef TCU_SUP_TOKEN_H
#define TCU_SUP_TOKEN_H

#include <stdint.h>
#include <stdbool.h>

/* compute the wire token for a challenge selector */
uint32_t sup_token_compute(uint8_t sel);

/* compute and stage the token at 0xFFF92414 (the stock's 0x7EE8A law;
 * runs in the monitor pass, the duty builder only reads the cell) */
void sup_token_stage(uint8_t sel);

/* duty-context step: level == 0 cycles 0 → 7 → 0x19 (one step per call);
 * level != 0 snaps to the steady context 0x1E. Writes the context pair
 * cells (0xFFF94FE7 / 0xFFF94E5F) and returns the new context. Called from
 * the walk's duty programmer (task context) at the era cadence. */
uint8_t sup_token_context(void);

#endif /* TCU_SUP_TOKEN_H */

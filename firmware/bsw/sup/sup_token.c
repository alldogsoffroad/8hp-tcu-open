/* sup_token.c — the duty-token challenge engine (design §7.9, spec 22 §5).
 *
 * Token law (stock flash 0x7F424 constant fold): the wire word set of the W1
 * duty job is the four token bytes OR'd onto the four 0x88 template words.
 * The stock computes the token in 0x7EEA-class monitor_integrity_sweep (the
 * same pass that checksums the dual-rail RAM), stages it at 0xFFF92414 and
 * the duty era's timer service (job-mode 0) sends it every tick.
 *
 * Context law (0x6BF9C + 0x8CEDC): while the achieved level is 0 the duty
 * arms cycle contexts 0 → 7 → 0x19 and context 0x19 sends ~token; level != 0
 * → steady context 0x1E (plain token). Contexts live in the diverse pair
 * 0xFFF94FE7 / 0xFFF94E5F.
 */
#include "bsw/sup/sup_token.h"
#include "bsw/sup/sup_cells.h"

/* the xor table: 16 u32 entries extracted from the stock ELF (flash 0x4526C);
 * byte-identical to the bench-proven table carried in sup_engage.c */
static const uint32_t S_TOKEN_TABLE[16] = {
    0xB3DF2F3Fu, 0xFC906070u, 0xA5C93929u, 0xEA867666u,
    0x3955A5B5u, 0x761AEAFAu, 0x2F43B3A3u, 0x600CFCECu,
    0x9EF20212u, 0xD1BD4D5Du, 0x88E41404u, 0xC7AB5B4Bu,
    0x14788898u, 0x5B37C7D7u, 0x026E9E8Eu, 0x4D21D1C1u,
};
#define S_TOKEN_CONST 0xB32F8A6Au   /* the stock's fold constant */

/* duty contexts */
#define TOKEN_CTX_STEADY  0x1Eu     /* plain token (level != 0) */
#define TOKEN_CTX_0       0x00u
#define TOKEN_CTX_7       0x07u
#define TOKEN_CTX_INV     0x19u     /* sends ~token */

uint32_t sup_token_compute(uint8_t sel)
{
    return ~(S_TOKEN_TABLE[sel & 0x0Fu] ^ 0xAAAAu ^ S_TOKEN_CONST);
}

void sup_token_stage(uint8_t sel)
{
    CELL_TOKEN_STAGED = sup_token_compute(sel);
}

uint8_t sup_token_context(void)
{
    uint8_t ctx = CELL_DUTY_CTX;

    if (CELL_LVL_REPORTED != 0u) {
        ctx = TOKEN_CTX_STEADY;                 /* level != 0: plain token */
    } else {
        /* dragging: cycle 0 → 7 → 0x19 → 0 ... (one step per call; the walk's
         * era stepper calls this at the era cadence, so a full cycle spans
         * three era dwells) */
        switch (ctx) {
        case TOKEN_CTX_STEADY:
        case TOKEN_CTX_INV:
        default:
            ctx = TOKEN_CTX_0;
            break;
        case TOKEN_CTX_0:
            ctx = TOKEN_CTX_7;
            break;
        case TOKEN_CTX_7:
            ctx = TOKEN_CTX_INV;
            break;
        }
    }
    /* keep the diverse pair coherent */
    CELL_DUTY_CTX      = ctx;
    CELL_DUTY_CTX_COMPL = (uint8_t)~ctx;
    return ctx;
}
